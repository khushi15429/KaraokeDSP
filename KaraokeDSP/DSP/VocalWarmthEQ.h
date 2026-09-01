#pragma once
#ifndef VOCALWARMTHEQ_H
#define VOCALWARMTHEQ_H
#include <cmath>

// A single biquad stage (peaking, high-pass, or high-shelf depending on how it's configured).
class BiquadStage {
public:
    enum class Type { Peaking, HighPass, HighShelf };

    void Configure(Type type, int sampleRate, float freq, float gainDb, float q) {
        m_type = type;
        m_sampleRate = sampleRate;
        m_freq = freq;
        m_gainDb = gainDb;
        m_q = q;
        UpdateCoefficients();
    }

    void SetEnabled(bool en) { m_enabled = en; }

    float Process(float sample) {
        if (!m_enabled) return sample;
        float out = m_b0 * sample + m_b1 * m_x1 + m_b2 * m_x2 - m_a1 * m_y1 - m_a2 * m_y2;
        m_x2 = m_x1; m_x1 = sample;
        m_y2 = m_y1; m_y1 = out;
        return out;
    }

private:
    void UpdateCoefficients() {
        float A = std::pow(10.0f, m_gainDb / 40.0f);
        float w0 = 2.0f * 3.14159265f * m_freq / static_cast<float>(m_sampleRate);
        float alpha = std::sin(w0) / (2.0f * m_q);
        float cosw0 = std::cos(w0);

        float b0, b1, b2, a0, a1, a2;

        switch (m_type) {
        case Type::HighPass: {
            b0 = (1 + cosw0) / 2; b1 = -(1 + cosw0); b2 = (1 + cosw0) / 2;
            a0 = 1 + alpha; a1 = -2 * cosw0; a2 = 1 - alpha;
            break;
        }
        case Type::HighShelf: {
            float sqrtA = std::sqrt(A);
            b0 = A * ((A + 1) + (A - 1) * cosw0 + 2 * sqrtA * alpha);
            b1 = -2 * A * ((A - 1) + (A + 1) * cosw0);
            b2 = A * ((A + 1) + (A - 1) * cosw0 - 2 * sqrtA * alpha);
            a0 = (A + 1) - (A - 1) * cosw0 + 2 * sqrtA * alpha;
            a1 = 2 * ((A - 1) - (A + 1) * cosw0);
            a2 = (A + 1) - (A - 1) * cosw0 - 2 * sqrtA * alpha;
            break;
        }
        case Type::Peaking:
        default: {
            b0 = 1 + alpha * A; b1 = -2 * cosw0; b2 = 1 - alpha * A;
            a0 = 1 + alpha / A; a1 = -2 * cosw0; a2 = 1 - alpha / A;
            break;
        }
        }

        m_b0 = b0 / a0; m_b1 = b1 / a0; m_b2 = b2 / a0;
        m_a1 = a1 / a0; m_a2 = a2 / a0;
    }

    Type m_type = Type::Peaking;
    int m_sampleRate = 44100;
    float m_freq = 250.0f;
    float m_gainDb = 0.0f;
    float m_q = 0.707f;
    bool m_enabled = true;

    float m_b0 = 1.0f, m_b1 = 0.0f, m_b2 = 0.0f;
    float m_a1 = 0.0f, m_a2 = 0.0f;
    float m_x1 = 0, m_x2 = 0, m_y1 = 0, m_y2 = 0;
};

// VOICE-ENHANCEMENT: multi-band EQ chain for vocal warmth, formant clarity, and presence.
// Stages run in series: LowCut -> LowMidBoost -> FormantBody -> FormantClarity ->
// Formant3-4kHz -> Presence4-5kHz -> Brightness.
class VocalWarmthEQ {
public:
    void Initialize(int sampleRate) {
        m_sampleRate = sampleRate;
        m_lowCut.Configure(BiquadStage::Type::HighPass, sampleRate, 90.0f, 0.0f, 0.707f);
        m_lowMidBoost.Configure(BiquadStage::Type::Peaking, sampleRate, 200.0f, 2.0f, 1.0f);
        // FORMANT ENHANCEMENT (per latest spec) -- replaces the old flat mud-dip cut in
        // this same region, since a cut there would fight the new body boost.
        m_formantBody.Configure(BiquadStage::Type::Peaking, sampleRate, 350.0f, 2.0f, 1.2f);      // 300-400 Hz: voice body
        m_formantClarity.Configure(BiquadStage::Type::Peaking, sampleRate, 1200.0f, 2.5f, 1.0f);  // 1000-1500 Hz: clarity
        m_formant3to4k.Configure(BiquadStage::Type::Peaking, sampleRate, 3500.0f, 2.0f, 1.2f);    // 3000-4000 Hz: presence/formant
        m_presence.Configure(BiquadStage::Type::Peaking, sampleRate, 4500.0f, 2.5f, 1.0f);        // 4000-5000 Hz: presence boost
        m_brightness.Configure(BiquadStage::Type::HighShelf, sampleRate, 8000.0f, 2.0f, 0.707f);
    }

    void SetEnabled(bool en) { m_enabled = en; }

    float Process(float sample) {
        if (!m_enabled) return sample;
        float s = sample;
        s = m_lowCut.Process(s);
        s = m_lowMidBoost.Process(s);
        s = m_formantBody.Process(s);
        s = m_formantClarity.Process(s);
        s = m_formant3to4k.Process(s);
        s = m_presence.Process(s);
        s = m_brightness.Process(s);
        return s;
    }

private:
    int m_sampleRate = 44100;
    bool m_enabled = true;
    BiquadStage m_lowCut;
    BiquadStage m_lowMidBoost;
    BiquadStage m_formantBody;
    BiquadStage m_formantClarity;
    BiquadStage m_formant3to4k;
    BiquadStage m_presence;
    BiquadStage m_brightness;
};
#endif