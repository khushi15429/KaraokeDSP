#pragma once
#ifndef AIRSHEENFILTER_H
#define AIRSHEENFILTER_H
#include <cmath>

// Subtle "air/shine" high-shelf: a gentle brightness lift above ~11kHz that adds
// shimmer/openness to a vocal without adding harshness (much gentler than the
// EQ's brightness band, meant as a final polish stage).
class AirSheenFilter {
public:
    // Self-initializing: works correctly even if Init() is never called (defaults
    // to 44100 Hz, which matches this project's typical sample rate). Call Init()
    // explicitly if you know the real sample rate up front, for exact tuning.
    void Init(int sampleRate) {
        if (sampleRate > 0) m_sampleRate = sampleRate;
        m_initialized = false; // force coefficient recompute
        EnsureInitialized();
    }

    float Process(float sample) {
        EnsureInitialized();
        float out = m_b0 * sample + m_b1 * m_x1 + m_b2 * m_x2 - m_a1 * m_y1 - m_a2 * m_y2;
        m_x2 = m_x1; m_x1 = sample;
        m_y2 = m_y1; m_y1 = out;
        return out;
    }

private:
    void EnsureInitialized() {
        if (m_initialized) return;
        m_initialized = true;

        constexpr float freq = 11000.0f;
        constexpr float gainDb = 1.5f; // gentle -- this is "sheen", not a brightness boost
        constexpr float q = 0.707f;

        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = 2.0f * 3.14159265f * freq / static_cast<float>(m_sampleRate);
        float alpha = std::sin(w0) / (2.0f * q);
        float cosw0 = std::cos(w0);
        float sqrtA = std::sqrt(A);

        float b0 = A * ((A + 1) + (A - 1) * cosw0 + 2 * sqrtA * alpha);
        float b1 = -2 * A * ((A - 1) + (A + 1) * cosw0);
        float b2 = A * ((A + 1) + (A - 1) * cosw0 - 2 * sqrtA * alpha);
        float a0 = (A + 1) - (A - 1) * cosw0 + 2 * sqrtA * alpha;
        float a1 = 2 * ((A - 1) - (A + 1) * cosw0);
        float a2 = (A + 1) - (A - 1) * cosw0 - 2 * sqrtA * alpha;

        m_b0 = b0 / a0; m_b1 = b1 / a0; m_b2 = b2 / a0;
        m_a1 = a1 / a0; m_a2 = a2 / a0;
    }

    int m_sampleRate = 44100;
    bool m_initialized = false;
    float m_b0 = 1.0f, m_b1 = 0.0f, m_b2 = 0.0f;
    float m_a1 = 0.0f, m_a2 = 0.0f;
    float m_x1 = 0, m_x2 = 0, m_y1 = 0, m_y2 = 0;
};
#endif