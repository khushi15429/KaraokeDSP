#pragma once
#ifndef VOCALDOUBLER_H
#define VOCALDOUBLER_H
#include <cmath>
#include <vector>

// Simulates a second voice doubling the lead vocal (the "unison layering" technique
// for a fuller/thicker sound): a short, slowly-modulated delay line creates a subtle
// pitch/timing variance that reads as "two voices" rather than a single thin voice,
// without needing a full separate pitch-shifted copy.
class VocalDoubler {
public:
    void Init(int sampleRate) {
        m_sampleRate = (sampleRate > 0) ? sampleRate : 44100;
        // ~20ms max delay is enough for a natural doubling effect without becoming
        // an audible slap/echo.
        m_buffer.assign(static_cast<size_t>(m_sampleRate / 20), 0.0f);
        m_writeIndex = 0;
        m_lfoPhase = 0.0f;
        m_initialized = true;
    }

    // NOTE: sampleRate param kept for call-site compatibility; Init() should be
    // called once beforehand with the real sample rate. If Init() was never called,
    // this lazily initializes using the sampleRate passed here.
    float Process(float sample, int sampleRate) {
        if (!m_initialized) Init(sampleRate);
        if (m_buffer.empty()) return sample;

        // Slow LFO (~0.6 Hz) modulates delay time slightly for a natural, non-static
        // doubling character (a static/fixed delay sounds more like a flat echo).
        constexpr float kLfoRateHz = 0.6f;
        m_lfoPhase += 2.0f * 3.14159265f * kLfoRateHz / static_cast<float>(m_sampleRate);
        if (m_lfoPhase > 2.0f * 3.14159265f) m_lfoPhase -= 2.0f * 3.14159265f;

        constexpr float kBaseDelayMs = 14.0f;   // short -- doubling, not a slap delay
        constexpr float kModDepthMs = 3.0f;
        float delayMs = kBaseDelayMs + kModDepthMs * std::sin(m_lfoPhase);
        float delaySamples = (delayMs / 1000.0f) * static_cast<float>(m_sampleRate);

        float readPosF = static_cast<float>(m_writeIndex) - delaySamples;
        while (readPosF < 0.0f) readPosF += static_cast<float>(m_buffer.size());
        size_t readIndex0 = static_cast<size_t>(readPosF) % m_buffer.size();
        size_t readIndex1 = (readIndex0 + 1) % m_buffer.size();
        float frac = readPosF - std::floor(readPosF);
        float delayedSample = m_buffer[readIndex0] * (1.0f - frac) + m_buffer[readIndex1] * frac;

        m_buffer[m_writeIndex] = sample;
        m_writeIndex = (m_writeIndex + 1) % m_buffer.size();

        constexpr float kDoubleMix = 0.22f; // subtle -- fuller, not literally two equal voices
        return sample + delayedSample * kDoubleMix;
    }

private:
    int m_sampleRate = 44100;
    bool m_initialized = false;
    std::vector<float> m_buffer;
    size_t m_writeIndex = 0;
    float m_lfoPhase = 0.0f;
};
#endif
