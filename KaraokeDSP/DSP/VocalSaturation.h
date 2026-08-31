#pragma once
#ifndef VOCALSATURATION_H
#define VOCALSATURATION_H
#include <cmath>

class VocalSaturation {
public:
    void SetDrive(float drive) { m_drive = drive; }  // 1.0 = subtle, 2-3 = noticeable
    void SetEnabled(bool en) { m_enabled = en; }

    float Process(float sample) {
        if (!m_enabled) return sample;
        float driven = sample * m_drive;
        float saturated = tanhf(driven);           // soft-clip
        return saturated / m_drive;                 // gain-compensate, keep level consistent
    }

private:
    float m_drive = 1.3f;
    bool m_enabled = true;
};
#endif