#ifndef VOCALREVERB_H
#define VOCALREVERB_H

#include <vector>
#include <cmath>
#include <algorithm>

class VocalReverb {
public:
    VocalReverb(int sampleRate = 44100, float roomSize = 0.50f, float damp = 0.60f)
        : mSampleRate(sampleRate), mRoomSize(roomSize), mDamp(damp) {

        // Short Ultra-Smooth Delay Buffers for Natural Vocal Polish (No Chorus Effect)
        mDelayBuffer.resize(static_cast<size_t>(sampleRate * 0.025f), 0.0f); // 25ms Air Diffusion
    }

    void Process(float* buffer, size_t numFrames, float wetMix = 0.08f) {
        if (!buffer || numFrames == 0) return;

        float dryMix = 1.0f - wetMix;

        for (size_t i = 0; i < numFrames; ++i) {
            float input = buffer[i];

            // Smooth Single All-Pass Reverb Filter
            float delayed = mDelayBuffer[mWritePos];
            float output = -input + delayed;
            mDelayBuffer[mWritePos] = input + (delayed * mRoomSize * (1.0f - mDamp));

            mWritePos = (mWritePos + 1) % mDelayBuffer.size();

            // Soft Mix output
            buffer[i] = (input * dryMix) + (output * wetMix);
        }
    }

    void SetWetMix(float mix) {
        mWetMix = std::clamp(mix, 0.0f, 0.25f);
    }

private:
    int mSampleRate;
    float mRoomSize;
    float mDamp;
    float mWetMix{ 0.08f };

    std::vector<float> mDelayBuffer;
    size_t mWritePos{ 0 };
};

#endif // VOCALREVERB_H