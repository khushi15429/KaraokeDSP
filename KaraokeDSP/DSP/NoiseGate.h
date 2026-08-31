#pragma once

class NoiseGate
{
public:

    NoiseGate();

    void SetThreshold(float threshold);

    float Process(float sample);

private:

    float mThreshold;
};