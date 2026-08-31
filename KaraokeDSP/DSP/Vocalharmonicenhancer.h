#pragma once
#ifndef VOCALHARMONICENHANCER_H
#define VOCALHARMONICENHANCER_H
#include <cmath>

// VOICE-ENHANCEMENT ITEMS 3b (harmonic exciter) + 3c (light saturation).
// Adds subtle even-harmonic content and soft-clip warmth so a thin voice
// sounds fuller/richer, without changing pitch or timing.
class VocalHarmonicEnhancer {
public:
    void SetEnabled(bool en) { m_enabled = en; }
    void SetExciterMix(float mix) { m_exciterMix = mix; }     // recommended 0.10-0.20
    void SetSaturationDrive(float drive) { m_drive = (drive > 0.01f) ? drive : 0.01f; } // recommended 1.1-1.4

    float Process(float sample) {
        if (!m_enabled) return sample;

        // 2nd-harmonic exciter: sample^2 (sign-preserved) adds an octave-up harmonic
        // on top of the fundamental, which reads as "fuller/richer" without being
        // audible as distortion at a low mix amount.
        float sign = (sample >= 0.0f) ? 1.0f : -1.0f;
        float harmonic = sample * sample * sign;
        float excited = sample + m_exciterMix * harmonic;

        // Light soft-clip saturation (tube/tape-style warmth), gain-compensated so
        // overall level doesn't change.
        float driven = excited * m_drive;
        float saturated = std::tanh(driven);
        return saturated / m_drive;
    }

private:
    bool m_enabled = true;
    float m_exciterMix = 0.15f;
    float m_drive = 1.2f;
};
#endif
