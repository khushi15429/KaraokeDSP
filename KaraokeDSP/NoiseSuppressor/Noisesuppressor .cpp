#include "NoiseSuppressor.h"
#include <cmath>
#include <algorithm>

NoiseSuppressor::NoiseSuppressor(double sampleRate, int channels)
    : sampleRate_(sampleRate), channels_(channels)
{
    // Envelope follower speeds: fast attack so the gate opens quickly
    // when singing starts, slower release so it doesn't "chatter"
    // (rapidly open/close) between words.
    float attackMs = 3.0f;
    float releaseMs = 80.0f;
    attackCoeff_ = std::exp(-1.0f / (0.001f * attackMs * (float)sampleRate_));
    releaseCoeff_ = std::exp(-1.0f / (0.001f * releaseMs * (float)sampleRate_));

    hpPrevIn_.assign(channels_, 0.0f);
    hpPrevOut_.assign(channels_, 0.0f);
    setHighPassCutoffHz(90.0f); // cut rumble/hum below ~90Hz
}

void NoiseSuppressor::setHighPassCutoffHz(float hz)
{
    float rc = 1.0f / (2.0f * 3.14159265f * hz);
    float dt = 1.0f / (float)sampleRate_;
    hpCoeff_ = rc / (rc + dt);
}

float NoiseSuppressor::rms(const float* samples, size_t n) const
{
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) sum += (double)samples[i] * samples[i];
    return (float)std::sqrt(sum / std::max<size_t>(1, n));
}

void NoiseSuppressor::calibrateNoiseFloor(const float* samples, size_t numSamples)
{
    float measured = rms(samples, numSamples);
    if (measured > 0.0f)
        noiseFloorRms_ = measured;
}

void NoiseSuppressor::process(float* buffer, size_t numSamples)
{
    size_t numFrames = numSamples / channels_;

    for (size_t f = 0; f < numFrames; ++f)
    {
        // --- Step 1: high-pass filter each channel (remove rumble/hum) ---
        for (int ch = 0; ch < channels_; ++ch)
        {
            size_t idx = f * channels_ + ch;
            float in = buffer[idx];

            float hp = hpCoeff_ * (hpPrevOut_[ch] + in - hpPrevIn_[ch]);
            hpPrevIn_[ch] = in;
            hpPrevOut_[ch] = hp;

            buffer[idx] = hp;
        }

        // --- Step 2: adaptive noise gate on the filtered frame ---
        float frameRms = rms(&buffer[f * channels_], channels_);
        envelope_ = (frameRms > envelope_)
            ? attackCoeff_ * envelope_ + (1.0f - attackCoeff_) * frameRms
            : releaseCoeff_ * envelope_ + (1.0f - releaseCoeff_) * frameRms;

        float envelopeDb = 20.0f * std::log10(std::max(envelope_, 1e-9f));
        float noiseFloorDb = 20.0f * std::log10(std::max(noiseFloorRms_, 1e-9f));
        float targetGain = (envelopeDb > noiseFloorDb + gateThresholdDb_) ? 1.0f : 0.0f;

        // smooth the gain itself so gating doesn't cause audible clicks
        float gainSmoothing = (targetGain > gateGain_) ? attackCoeff_ : releaseCoeff_;
        gateGain_ = gainSmoothing * gateGain_ + (1.0f - gainSmoothing) * targetGain;

        for (int ch = 0; ch < channels_; ++ch)
            buffer[f * channels_ + ch] *= gateGain_;
    }
}