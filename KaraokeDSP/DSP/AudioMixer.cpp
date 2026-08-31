#include "AudioMixer.h"


AudioMixer::AudioMixer()
{
    mSongVolume = 0.75f; // Music thoda peeche rahe taaki aawaz dab na jaye
    mMicVolume = 1.3f;   // User ki aawaz aage, clear aur prominent rahe
}

void AudioMixer::SetSongVolume(float volume)
{
    mSongVolume = volume;
}

void AudioMixer::SetMicVolume(float volume)
{
    mMicVolume = volume;
}

float AudioMixer::Process(
    float song,
    float mic
)
{
    float output =
        song * mSongVolume +
        mic * mMicVolume;

    // Simple Limiter
    if (output > 1.0f)
        output = 1.0f;

    if (output < -1.0f)
        output = -1.0f;

    return output;
}