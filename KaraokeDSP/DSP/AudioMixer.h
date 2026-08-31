#pragma once

class AudioMixer
{
public:

    AudioMixer();

    void SetSongVolume(float volume);

    void SetMicVolume(float volume);

    float Process(
        float song,
        float mic
    );

private:

    float mSongVolume;

    float mMicVolume;
};