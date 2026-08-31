#pragma once
#ifndef PITCHSHIFTER_H
#define PITCHSHIFTER_H

#include <vector>
#include <deque>
#include <memory>

namespace RubberBand { class RubberBandStretcher; }

class PitchShifter {
public:
    PitchShifter();
    ~PitchShifter();
    bool Initialize(int sampleRate, int channels);
    void SetPitch(float pitchRatio);
    void SetPitchSemitones(float semitones);
    int Process(const float* in, float* out, int frames);
    float GetPitchRatio() const { return m_pitchRatio; }

private:
    float m_pitchRatio = 1.0f;
    float m_targetPitchRatio = 1.0f;  // Added for smooth transition
    float m_currentPitchRatio = 1.0f; // Added for smooth transition
    int m_sampleRate = 48000;
    int m_channels = 1;
    std::unique_ptr<RubberBand::RubberBandStretcher> m_stretcher;
    std::deque<float> m_outputFifo;
};

#endif