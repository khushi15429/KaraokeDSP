#pragma once
#pragma once

class Compressor
{
public:

    Compressor();

    void SetThreshold(float threshold);

    void SetRatio(float ratio);

    void SetMakeupGain(float gain);

    void SetAttack(float attackMs);

    void SetRelease(float releaseMs);

    float Process(float sample);

private:

    float mThreshold;
    float mRatio;
    float mMakeupGain;
    float mAttackMs;
    float mReleaseMs;
    float mEnvelope;
    int mSampleRate;
};