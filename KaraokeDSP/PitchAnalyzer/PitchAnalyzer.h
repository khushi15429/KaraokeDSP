#pragma once
#pragma once

#include <vector>

class PitchAnalyzer
{
public:

    PitchAnalyzer();

    bool AddSamples(
        const float* samples,
        int count
    );

    const float* GetBuffer() const;

    int GetSampleCount() const;

    void Clear();

private:

    std::vector<float> mBuffer;
};