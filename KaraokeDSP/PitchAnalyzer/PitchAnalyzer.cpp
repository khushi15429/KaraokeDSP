#include "PitchAnalyzer.h"

PitchAnalyzer::PitchAnalyzer()
{
    mBuffer.reserve(2048);
}

bool PitchAnalyzer::AddSamples(
    const float* samples,
    int count
)
{
    for (int i = 0; i < count; i++)
    {
        mBuffer.push_back(samples[i]);

        if (mBuffer.size() >= 2048)
            return true;
    }

    return false;
}

const float* PitchAnalyzer::GetBuffer() const
{
    return mBuffer.data();
}

int PitchAnalyzer::GetSampleCount() const
{
    return (int)mBuffer.size();
}

void PitchAnalyzer::Clear()
{
    mBuffer.clear();
}