#pragma once
#include <string>
#include <thread>
#include <atomic>
#include "../AudioIO/AudioDeviceManager.h"
#include "../AudioIO/AudioStream.h"
#include "../Player/AudioPlayer.h"

class AudioEngine
{
public:
    AudioEngine();

    bool Initialize();
    void Shutdown();
    bool IsRunning() const;

    bool LoadSong(const char* file);
    void PlaySong();
    void StopSong();
    void Process();

    // DSP & Volume Controls
    void SetPitchShift(float semitones);
    void SetMicVolume(float vol);
    void SetSpeakerVolume(float vol);

private:
    std::atomic_bool mRunning{ false };

    AudioDeviceManager mDeviceManager;
    AudioStream mStream;               // Ensure mStream is present here
    AudioPlayer mPlayer;

    std::thread mDecodeThread;
};
