#pragma once
#ifndef PITCHSHIFTER_H
#define PITCHSHIFTER_H

#include <vector>
#include <deque>
#include <memory>
#include <iostream>

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
    float m_targetPitchRatio = 1.0f;
    float m_currentPitchRatio = 1.0f;
    int m_sampleRate = 44100;
    int m_channels = 1;

    std::unique_ptr<RubberBand::RubberBandStretcher> m_stretcher;
    std::deque<float> m_outputFifo;

    // Real-time Audio Thread Safety: Pre-allocated buffers to prevent dynamic memory allocation
    std::vector<float> m_scratchBuffer;
    std::vector<const float*> m_inChannelPointers;
    std::vector<float*> m_outChannelPointers;

    int m_debugCounter = 0;
};

#endif