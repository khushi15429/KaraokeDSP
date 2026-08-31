#include "PitchDetector.h"

PitchDetector::PitchDetector() {
    mHistoryBuffer.resize(HISTORY_SIZE, 0.0f);
    mYinBuffer.resize(HISTORY_SIZE / 2, 0.0f);
}

PitchDetector::~PitchDetector() {}

float PitchDetector::Process(const float* input, int numFrames, int sampleRate, float& outConfidence) {
    outConfidence = 0.0f;
    if (!input || numFrames <= 0 || sampleRate <= 0) return 0.0f;

    // 1. Shift rolling history buffer by numFrames and append new block
    if (numFrames >= HISTORY_SIZE) {
        std::copy(input + (numFrames - HISTORY_SIZE), input + numFrames, mHistoryBuffer.begin());
    }
    else {
        std::move(mHistoryBuffer.begin() + numFrames, mHistoryBuffer.end(), mHistoryBuffer.begin());
        std::copy(input, input + numFrames, mHistoryBuffer.end() - numFrames);
    }

    // 2. Compute RMS over history buffer for Voice Activity Detection
    float sumSq = 0.0f;
    for (int i = 0; i < HISTORY_SIZE; ++i) {
        sumSq += mHistoryBuffer[i] * mHistoryBuffer[i];
    }
    float rms = std::sqrt(sumSq / static_cast<float>(HISTORY_SIZE));

    // Silence gate: Return 0.0 Hz for unvoiced or low signal
    if (rms < 0.005f) {
        return 0.0f;
    }

    // 3. Set lag bounds based on human vocal range (75 Hz to 1100 Hz)
    int maxLag = static_cast<int>(static_cast<float>(sampleRate) / 75.0f);
    int minLag = static_cast<int>(static_cast<float>(sampleRate) / 1100.0f);

    int halfWindow = HISTORY_SIZE / 2; // 1024
    maxLag = std::min(maxLag, halfWindow - 1);

    if (minLag >= maxLag) return 0.0f;

    // Step 1: Difference Function
    mYinBuffer[0] = 1.0f;
    for (int tau = 1; tau <= maxLag; ++tau) {
        float deltaSum = 0.0f;
        for (int i = 0; i < halfWindow; ++i) {
            float diff = mHistoryBuffer[i] - mHistoryBuffer[i + tau];
            deltaSum += diff * diff;
        }
        mYinBuffer[tau] = deltaSum;
    }

    // Step 2: Cumulative Mean Normalized Difference
    float runningSum = 0.0f;
    mYinBuffer[0] = 1.0f;
    for (int tau = 1; tau <= maxLag; ++tau) {
        runningSum += mYinBuffer[tau];
        if (runningSum > 0.0f) {
            mYinBuffer[tau] *= (static_cast<float>(tau) / runningSum);
        }
        else {
            mYinBuffer[tau] = 1.0f;
        }
    }

    // Step 3: Absolute Threshold Search with harmonic preference
    const float YIN_THRESHOLD = 0.15f;
    int tauFound = -1;
    float bestVal = 1.0f;

    for (int tau = minLag; tau <= maxLag; ++tau) {
        if (mYinBuffer[tau] < YIN_THRESHOLD) {
            while (tau + 1 <= maxLag && mYinBuffer[tau + 1] < mYinBuffer[tau]) {
                tau++;
            }
            tauFound = tau;
            bestVal = mYinBuffer[tau];
            break;
        }
    }

    // Global minimum fallback if threshold not met
    if (tauFound == -1) {
        float minVal = 1.0f;
        for (int tau = minLag; tau <= maxLag; ++tau) {
            if (mYinBuffer[tau] < minVal) {
                minVal = mYinBuffer[tau];
                tauFound = tau;
            }
        }
        bestVal = minVal;
        if (minVal > 0.40f) {
            tauFound = -1;
        }
    }

    if (tauFound <= 0) {
        return 0.0f;
    }

    outConfidence = 1.0f - std::clamp(bestVal, 0.0f, 1.0f);

    if (outConfidence < 0.45f) {
        return 0.0f;
    }

    // Step 4: Parabolic Interpolation for accurate pitch peak determination
    float betterTau = static_cast<float>(tauFound);
    if (tauFound > minLag && tauFound < maxLag) {
        float s0 = mYinBuffer[tauFound - 1];
        float s1 = mYinBuffer[tauFound];
        float s2 = mYinBuffer[tauFound + 1];
        float denom = 2.0f * (2.0f * s1 - s0 - s2);
        if (std::abs(denom) > 1e-5f) {
            betterTau += (s2 - s0) / denom;
        }
    }

    return static_cast<float>(sampleRate) / betterTau;
}