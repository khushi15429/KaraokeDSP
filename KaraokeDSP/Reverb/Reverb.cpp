#include "Reverb.h"
Reverb::Reverb()
{
    // Buffer size ko thoda bada karein taaki bada room/hall feel aaye (~1 second)
    mDelayBuffer.resize(44100, 0.0f);

    mWriteIndex = 0;

    // Melodious depth ke liye wet aur feedback badhayein
    mWet = 0.28f;         // 28% reverb mix (taaki aawaz me ghumti hui richness aaye)
    mFeedback = 0.40f;    // Thoda lamba aur pyara tail decay
}

void Reverb::SetWet(float wet)
{
    mWet = wet;
}

void Reverb::SetFeedback(float feedback)
{
    mFeedback = feedback;
}

float Reverb::Process(float input)
{
    float delayed =
        mDelayBuffer[mWriteIndex];

    float output =
        input * (1.0f - mWet) +
        delayed * mWet;

    mDelayBuffer[mWriteIndex] =
        input +
        delayed * mFeedback;

    mWriteIndex++;

    if (mWriteIndex >= (int)mDelayBuffer.size())
        mWriteIndex = 0;

    return output;
}