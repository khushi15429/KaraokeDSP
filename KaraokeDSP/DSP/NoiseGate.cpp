#include "NoiseGate.h"

#include <cmath>

NoiseGate::NoiseGate()
{
    mThreshold = 0.02f;
}

void NoiseGate::SetThreshold(float threshold)
{
    mThreshold = threshold;
}

float NoiseGate::Process(float sample)
{
    if (std::fabs(sample) < mThreshold)
    {
        return 0.0f;
    }

    return sample;
}