#pragma once

#include "FFmpegDecoder.h"
#include <mutex>

#include "../AudioIO/AudioBuffer.h"



class AudioPlayer
{

public:

    AudioPlayer();


    bool Load(
        const char* filename
    );


    void Play();


    void Pause();


    void Stop();



    void Process();



    bool IsPlaying();



    void SetBuffer(
        AudioBuffer* buffer
    );

    void SetOutputFormat(int sampleRate, int channels);



private:


    FFmpegDecoder mDecoder;


    AudioBuffer* mBuffer;


    bool mPlaying;

    std::mutex mMutex;


};
