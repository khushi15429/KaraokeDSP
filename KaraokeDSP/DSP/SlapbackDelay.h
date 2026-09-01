#pragma once
#ifndef SLAPBACKDELAY_H
#define SLAPBACKDELAY_H
#include <vector>
#include <cstddef>

// Short slapback delay for subtle vocal depth/space (distinct from reverb -- a
// single short, quiet repeat rather than a diffuse tail).
class SlapbackDelay {
public:
    void Init(int sampleRate, float delayMs = 120.0f) {
        m_delaySamples = static_cast<size_t>((delayMs / 1000.0f) * static_cast<float>(sampleRate > 0 ? sampleRate : 44100));
        m_buffer.assign(static_cast<size_t>(sampleRate > 0 ? sampleRate : 44100), 0.0f); // 1 sec buffer
        m_writeIndex = 0;
    }

    float Process(float input, float mix = 0.08f) {
        if (m_delaySamples == 0 || m_buffer.empty()) return input;

        size_t readIndex = (m_writeIndex + m_buffer.size() - m_delaySamples) % m_buffer.size();
        float delayedSample = m_buffer[readIndex];

        // Low feedback (10%) keeps the voice clear/up-front rather than echoing repeatedly.
        m_buffer[m_writeIndex] = input + (delayedSample * 0.10f);
        m_writeIndex = (m_writeIndex + 1) % m_buffer.size();

        return input + (delayedSample * mix);
    }

private:
    std::vector<float> m_buffer;
    size_t m_writeIndex = 0;
    size_t m_delaySamples = 0;
};
#endif