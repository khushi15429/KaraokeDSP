#include "Limiter.h"
#include <cmath>


Limiter::Limiter()
{
    mThreshold = 0.90f;

    mRelease = 0.01f;

    mGain = 1.0f;
}



void Limiter::SetThreshold(float threshold)
{
    mThreshold = threshold;
}



void Limiter::SetRelease(float release)
{
    mRelease = release;
}



float Limiter::Process(float sample)
{

    float level = std::fabs(sample);


    float targetGain = 1.0f;


    // Peak limit

    if (level > mThreshold)
    {
        targetGain =
            mThreshold / level;
    }



    // Smooth gain recovery

    mGain +=
        (targetGain - mGain) * mRelease;



    float output =
        sample * mGain;



    // Safety clipping

    if (output > 1.0f)
        output = 1.0f;


    if (output < -1.0f)
        output = -1.0f;



    return output;
}