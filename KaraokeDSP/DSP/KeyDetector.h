#ifndef KEYDETECTOR_H
#define KEYDETECTOR_H

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>

// 1. Hysteresis Filter to prevent frequent key switching (Stabilizer)
class KeyDetectorStabilizer {
public:
    explicit KeyDetectorStabilizer(int holdFramesThreshold = 40)
        : mHoldThreshold(holdFramesThreshold) {
    }

    int ProcessCandidateKey(int candidateKeyIndex) {
        if (candidateKeyIndex == mPendingKey) {
            mFrameCount++;
            if (mFrameCount >= mHoldThreshold) {
                mActiveKey = mPendingKey;
            }
        }
        else {
            mPendingKey = candidateKeyIndex;
            mFrameCount = 0;
        }
        return mActiveKey;
    }

private:
    int mActiveKey{ 2 }; // Default D
    int mPendingKey{ 2 };
    int mFrameCount{ 0 };
    int mHoldThreshold{ 40 };
};

// 2. Automated Chromagram Key Detector
class KeyDetector {
public:
    KeyDetector() {
        mChroma.fill(0.0f);
    }

    void Reset() {
        mChroma.fill(0.0f);
    }

    // Main frequency accumulator
    void AccumulateFrequency(float freq) {
        if (freq < 65.0f || freq > 1000.0f) return;

        double midiNote = 69.0 + 12.0 * std::log2(freq / 440.0);
        int noteInOctave = (static_cast<int>(std::round(midiNote)) % 12 + 12) % 12;

        mChroma[noteInOctave] += 1.0f;
    }

    // Alias wrapper for Dashboard call compatibility
    void AccumulatePitch(float freq) {
        AccumulateFrequency(freq);
    }

    int GetDetectedKeyIndex() {
        auto maxIt = std::max_element(mChroma.begin(), mChroma.end());
        int candidateRoot = static_cast<int>(std::distance(mChroma.begin(), maxIt));

        return mStabilizer.ProcessCandidateKey(candidateRoot);
    }

    std::string GetDetectedKeyName() {
        static const std::string NoteNames[12] = {
            "C Major", "C# Major", "D Major", "D# Major",
            "E Major", "F Major", "F# Major", "G Major",
            "G# Major", "A Major", "A# Major", "B Major"
        };
        return NoteNames[GetDetectedKeyIndex()];
    }

private:
    std::array<float, 12> mChroma;
    KeyDetectorStabilizer mStabilizer;
};

#endif // KEYDETECTOR_H