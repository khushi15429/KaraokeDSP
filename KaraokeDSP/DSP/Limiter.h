#pragma once
#pragma once


class Limiter
{

public:

    Limiter();


    void SetThreshold(float threshold);


    void SetRelease(float release);


    float Process(float sample);



private:

    float mThreshold;

    float mRelease;

    float mGain;

};