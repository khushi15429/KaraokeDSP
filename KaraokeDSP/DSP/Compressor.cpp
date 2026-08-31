#include "Compressor.h"

#include <cmath>

Compressor::Compressor()
    : mThreshold(-20.0f), // -20 dB threshold
      mRatio(4.0f),       // 4:1 ratio
      mMakeupGain(3.0f),  // +3 dB makeup gain
      mAttackMs(10.0f),   // 10ms attack
      mReleaseMs(100.0f),  // 100ms release
      mEnvelope(0.0f),
      mSampleRate(44100)
{
}

void Compressor::SetThreshold(float thresholdDb)
{
    mThreshold = thresholdDb;
}

void Compressor::SetRatio(float ratio)
{
    mRatio = ratio;
}

void Compressor::SetMakeupGain(float makeupGainDb)
{
    mMakeupGain = makeupGainDb;
}

void Compressor::SetAttack(float attackMs)
{
    mAttackMs = attackMs;
}

void Compressor::SetRelease(float releaseMs)
{
    mReleaseMs = releaseMs;
}

float Compressor::Process(float sample)
{
    // Convert linear to dB
    float inputDb = 20.0f * std::log10(std::fabs(sample) + 1e-6f);
    
    // Calculate gain reduction needed
    float gainReductionDb = 0.0f;
    if (inputDb > mThreshold) {
        gainReductionDb = (inputDb - mThreshold) * (1.0f - 1.0f / mRatio);
    }
    
    // Convert attack/release to coefficients
    float attackCoef = std::exp(-1.0f / (mAttackMs * mSampleRate / 1000.0f));
    float releaseCoef = std::exp(-1.0f / (mReleaseMs * mSampleRate / 1000.0f));
    
    // Envelope follower with attack/release
    float targetEnvelope = std::fabs(sample);
    if (targetEnvelope > mEnvelope) {
        mEnvelope = targetEnvelope * (1.0f - attackCoef) + mEnvelope * attackCoef;
    } else {
        mEnvelope = targetEnvelope * (1.0f - releaseCoef) + mEnvelope * releaseCoef;
    }
    
    // Apply gain reduction to envelope
    float envelopeDb = 20.0f * std::log10(mEnvelope + 1e-6f);
    float compressedEnvelopeDb = envelopeDb;
    if (envelopeDb > mThreshold) {
        compressedEnvelopeDb = mThreshold + (envelopeDb - mThreshold) / mRatio;
    }
    
    // Calculate gain from envelope
    float gainDb = compressedEnvelopeDb - envelopeDb + mMakeupGain;
    float gainLinear = std::pow(10.0f, gainDb / 20.0f);
    
    // Apply gain to sample
    float output = sample * gainLinear;
    
    // Hard limiter to prevent clipping
    if (output > 1.0f) output = 1.0f;
    if (output < -1.0f) output = -1.0f;
    
    return output;
}