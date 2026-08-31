#ifndef PITCHDETECTOR_H
#define PITCHDETECTOR_H

#include <vector>
#include <cmath>
#include <algorithm>

class PitchDetector {
public:
    PitchDetector();
    ~PitchDetector();

    float Process(const float* buffer, int numFrames, int sampleRate, float& outConfidence);

private:
    static const int HISTORY_SIZE = 2048;
    std::vector<float> mHistoryBuffer;
    std::vector<float> mYinBuffer;
};

#endif // PITCHDETECTOR_H