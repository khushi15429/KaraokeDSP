#include "AudioEngine.h"
#include <iostream>
#include <thread>
#include <chrono>

AudioEngine::AudioEngine()
{
    mRunning = false;
}

bool AudioEngine::Initialize()
{
    if (!mDeviceManager.Initialize())
    {
        return false;
    }

    if (!mStream.Open())
    {
        return false;
    }

    // Decoder and microphone both feed AudioStream, which performs the final mix.
    mPlayer.SetBuffer(mStream.GetSongBuffer());
    mPlayer.SetOutputFormat(mStream.GetSampleRate(), mStream.GetChannelCount());

    if (!mStream.Start())
    {
        return false;
    }

    mDeviceManager.ListDevices();

    mRunning = true;

    std::cout << "Audio Engine Started" << std::endl;

    return true;
}

void AudioEngine::Shutdown()
{
    mRunning = false;

    mPlayer.Stop();

    if (mDecodeThread.joinable())
    {
        mDecodeThread.join();
    }

    mStream.Stop();
}

bool AudioEngine::LoadSong(const char* file)
{
    if (!mPlayer.Load(file))
    {
        return false;
    }

    return true;
}

void AudioEngine::PlaySong()
{
    if (mDecodeThread.joinable()) {
        if (mPlayer.IsPlaying())
            return;
        mDecodeThread.join();
    }

    mRunning = true;
    mPlayer.Play();

    mDecodeThread = std::thread([this]()
        {
            while (mRunning && mPlayer.IsPlaying())
            {
                mPlayer.Process();

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(2)
                );
            }
        });
}

void AudioEngine::StopSong()
{
    // Stopping a song must not shut down the engine; it can play again.
    mPlayer.Stop();

    if (mDecodeThread.joinable())
    {
        mDecodeThread.join();
    }

}

void AudioEngine::Process()
{
}

bool AudioEngine::IsRunning() const
{
    return mRunning;
}

void AudioEngine::SetPitchShift(float semitones)
{
    mStream.SetPitchShift(semitones);
}

void AudioEngine::SetMicVolume(float vol)
{
    mStream.SetMicVolume(vol);
}

void AudioEngine::SetSpeakerVolume(float vol)
{
    mStream.SetSpeakerVolume(vol);
}
