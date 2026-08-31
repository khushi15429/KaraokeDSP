#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

class YinPitchDetector {
public:
    YinPitchDetector(float sampleRate = 44100.0f, int bufferSize = 512, float threshold = 0.15f);
    ~YinPitchDetector() = default;

    // Core pitch detection call for audio buffer
    float GetPitch(const float* buffer, int bufferSize);

    // Helper: Convert frequency (Hz) to musical note name (e.g. 440Hz -> "A4")
    static std::string FrequencyToNote(float frequency);

private:
    void Difference(const float* buffer, int bufferSize);
    void CumulativeMeanNormalizedDifference();
    int AbsoluteThreshold();
    float ParabolicInterpolation(int tau);

    float mSampleRate;
    int mBufferSize;
    float mThreshold;
    int mYinBufferSize;
    std::vector<float> mYinBuffer;
};