#pragma once
#pragma once

#include <vector>

class Reverb
{
public:

    Reverb();

    void SetWet(float wet);

    void SetFeedback(float feedback);

    float Process(float input);

private:

    std::vector<float> mDelayBuffer;

    int mWriteIndex;

    float mWet;

    float mFeedback;
};