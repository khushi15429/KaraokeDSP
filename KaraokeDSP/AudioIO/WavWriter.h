#pragma once
#pragma once
#include <string>
#include <fstream>
#include <vector>
#include <cstdint>

class WavWriter
{
public:
    WavWriter();
    ~WavWriter();

    bool Open(const std::string& filename, int sampleRate, int channels);
    void Write(const float* data, int numFrames);
    void Close();

private:
    std::ofstream mFile;
    int mSampleRate;
    int mChannels;
    int mDataSize;

    void WriteHeader();
};