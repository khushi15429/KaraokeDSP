#include "WavWriter.h"
#include <iostream>

WavWriter::WavWriter()
    : mSampleRate(48000), mChannels(2), mDataSize(0) {
}

WavWriter::~WavWriter() {
    Close();
}

bool WavWriter::Open(const std::string& filename, int sampleRate, int channels) {
    mSampleRate = sampleRate;
    mChannels = channels;
    mDataSize = 0;

    mFile.open(filename, std::ios::binary);
    if (!mFile.is_open())
        return false;

    // Placeholder for WAV header (44 bytes)
    char dummyHeader[44] = { 0 };
    mFile.write(dummyHeader, 44);

    return true;
}

void WavWriter::Write(const float* data, int numFrames) {
    if (!mFile.is_open()) return;

    for (int i = 0; i < numFrames * mChannels; i++) {
        float sample = data[i];
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;

        int16_t pcmSample = static_cast<int16_t>(sample * 32767.0f);
        mFile.write(reinterpret_cast<const char*>(&pcmSample), sizeof(int16_t));
        mDataSize += static_cast<int>(sizeof(int16_t));
    }
}

void WavWriter::Close() {
    if (!mFile.is_open()) return;

    mFile.seekp(0, std::ios::beg);

    int32_t fileSize = static_cast<int32_t>(mDataSize + 36);
    int32_t byteRate = static_cast<int32_t>(mSampleRate * mChannels * 2);
    int16_t blockAlign16 = static_cast<int16_t>(mChannels * 2);

    mFile.write("RIFF", 4);
    mFile.write(reinterpret_cast<const char*>(&fileSize), 4);
    mFile.write("WAVEfmt ", 8);

    int32_t subChunk1Size = 16;
    mFile.write(reinterpret_cast<const char*>(&subChunk1Size), 4);

    int16_t audioFormat16 = static_cast<int16_t>(1); // PCM
    int16_t channels16 = static_cast<int16_t>(mChannels);
    int32_t sampleRate32 = static_cast<int32_t>(mSampleRate);
    int16_t bitsPerSample16 = static_cast<int16_t>(16);

    mFile.write(reinterpret_cast<const char*>(&audioFormat16), 2);
    mFile.write(reinterpret_cast<const char*>(&channels16), 2);
    mFile.write(reinterpret_cast<const char*>(&sampleRate32), 4);
    mFile.write(reinterpret_cast<const char*>(&byteRate), 4);
    mFile.write(reinterpret_cast<const char*>(&blockAlign16), 2);
    mFile.write(reinterpret_cast<const char*>(&bitsPerSample16), 2);

    mFile.write("data", 4);
    int32_t dataSize32 = static_cast<int32_t>(mDataSize);
    mFile.write(reinterpret_cast<const char*>(&dataSize32), 4);

    mFile.close();
}