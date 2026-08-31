#include "YinPitchDetector.h"

YinPitchDetector::YinPitchDetector(float sampleRate, int bufferSize, float threshold)
    : mSampleRate(sampleRate), mBufferSize(bufferSize), mThreshold(threshold) {
    mYinBufferSize = mBufferSize / 2;
    mYinBuffer.resize(mYinBufferSize, 0.0f);
}

void YinPitchDetector::Difference(const float* buffer, int bufferSize) {
    int maxTau = bufferSize / 2;
    for (int tau = 0; tau < maxTau; tau++) {
        mYinBuffer[tau] = 0.0f;
    }
    for (int tau = 1; tau < maxTau; tau++) {
        for (int i = 0; i < maxTau; i++) {
            float delta = buffer[i] - buffer[i + tau];
            mYinBuffer[tau] += delta * delta;
        }
    }
}

void YinPitchDetector::CumulativeMeanNormalizedDifference() {
    mYinBuffer[0] = 1.0f;
    float runningSum = 0.0f;

    for (int tau = 1; tau < mYinBufferSize; tau++) {
        runningSum += mYinBuffer[tau];
        if (runningSum > 0.0f) {
            mYinBuffer[tau] *= static_cast<float>(tau) / runningSum;
        }
        else {
            mYinBuffer[tau] = 1.0f;
        }
    }
}

int YinPitchDetector::AbsoluteThreshold() {
    int tau;
    for (tau = 2; tau < mYinBufferSize; tau++) {
        if (mYinBuffer[tau] < mThreshold) {
            while (tau + 1 < mYinBufferSize && mYinBuffer[tau + 1] < mYinBuffer[tau]) {
                tau++;
            }
            return tau;
        }
    }

    // Fallback: Find global minimum if no value below threshold
    int globalMinTau = 2;
    float minVal = mYinBuffer[2];
    for (tau = 3; tau < mYinBufferSize; tau++) {
        if (mYinBuffer[tau] < minVal) {
            minVal = mYinBuffer[tau];
            globalMinTau = tau;
        }
    }
    return (minVal < 0.85f) ? globalMinTau : -1;
}

float YinPitchDetector::ParabolicInterpolation(int tau) {
    if (tau < 1 || tau >= mYinBufferSize - 1) return static_cast<float>(tau);

    float s0 = mYinBuffer[tau - 1];
    float s1 = mYinBuffer[tau];
    float s2 = mYinBuffer[tau + 1];

    float denominator = 2.0f * (s0 - 2.0f * s1 + s2);
    if (std::abs(denominator) < 1e-6f) return static_cast<float>(tau);

    return static_cast<float>(tau) + (s0 - s2) / denominator;
}

float YinPitchDetector::GetPitch(const float* buffer, int bufferSize) {
    if (!buffer || bufferSize < 128) return 0.0f;

    Difference(buffer, bufferSize);
    CumulativeMeanNormalizedDifference();

    int tauEstimate = AbsoluteThreshold();
    if (tauEstimate == -1) return 0.0f; // Unvoiced / Silence

    float betterTau = ParabolicInterpolation(tauEstimate);
    if (betterTau <= 0.0f) return 0.0f;

    float pitch = mSampleRate / betterTau;

    // Vocal range check (Human Singing Range: 50 Hz to 1500 Hz)
    if (pitch < 50.0f || pitch > 1500.0f) return 0.0f;

    return pitch;
}

std::string YinPitchDetector::FrequencyToNote(float frequency) {
    if (frequency < 30.0f) return "---";

    const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    // MIDI Note formula: 69 + 12 * log2(freq / 440Hz)
    int midiNote = static_cast<int>(std::round(69.0f + 12.0f * std::log2(frequency / 440.0f)));

    int noteIndex = (midiNote % 12 + 12) % 12;
    int octave = (midiNote / 12) - 1;

    return std::string(noteNames[noteIndex]) + std::to_string(octave);
}