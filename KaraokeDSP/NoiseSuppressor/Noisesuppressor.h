#pragma once
#include <vector>
#include <cstddef>

// Lightweight real-time noise suppressor for karaoke mic input.
// Combines a one-pole high-pass filter (removes rumble/hum) with
// an adaptive RMS-based noise gate (mutes signal below the learned
// noise floor). No external dependencies.
class NoiseSuppressor {
public:
    NoiseSuppressor(double sampleRate, int channels = 1);

    // Call once at startup with a short chunk (e.g. 0.5-1s) of
    // "silence" (mic input with no one singing) to learn the room's
    // background noise level. Re-call any time to recalibrate.
    void calibrateNoiseFloor(const float* samples, size_t numSamples);

    // Process a block of interleaved audio in-place.
    void process(float* buffer, size_t numSamples);

    // How many dB above the noise floor the signal must rise before
    // the gate opens. Lower = more sensitive (may let noise through).
    // Higher = safer but may clip quiet vocal starts. Default 6 dB.
    void setGateThresholdDb(float db) { gateThresholdDb_ = db; }

    void setHighPassCutoffHz(float hz);

private:
    double sampleRate_;
    int channels_;

    // Noise gate state
    float noiseFloorRms_ = 0.001f;
    float gateThresholdDb_ = 6.0f;
    float envelope_ = 0.0f;
    float attackCoeff_;
    float releaseCoeff_;
    float gateGain_ = 0.0f;

    // High-pass filter state (one-pole, per channel)
    std::vector<float> hpPrevIn_;
    std::vector<float> hpPrevOut_;
    float hpCoeff_ = 0.0f;

    float rms(const float* samples, size_t n) const;
};