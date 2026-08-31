#pragma once

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libavutil/error.h>
}

#include <mutex>

class FFmpegDecoder
{
public:

    FFmpegDecoder();
    ~FFmpegDecoder();

    bool Open(const char* filename);

    void Close();

    // Returns number of float samples written
    int ReadFrame(float* outputBuffer, int maxSamples);

    bool IsOpen() const;

    int GetSampleRate() const;
    int GetChannels() const;
    void SetOutputFormat(int sampleRate, int channels);

private:

    AVFormatContext* mFormatContext;
    AVCodecContext* mCodecContext;
    SwrContext* mSwrContext;

    AVPacket* mPacket;
    AVFrame* mFrame;

    int mAudioStreamIndex;

    int mOutputSampleRate;
    int mOutputChannels;

    bool mEndOfFile;

    void CloseUnlocked();

    // Prevent simultaneous access
    std::mutex mMutex;
};
