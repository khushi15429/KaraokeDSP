#include "AudioPlayer.h"

#include <iostream>
#include <mutex>



AudioPlayer::AudioPlayer()
{

    mBuffer = nullptr;

    mPlaying = false;

}





bool AudioPlayer::Load(const char* filename)
{
    std::lock_guard<std::mutex> lock(mMutex);

    if (mBuffer)
        mBuffer->Clear();

    return mDecoder.Open(filename);
}

void AudioPlayer::Play()
{
    std::lock_guard<std::mutex> lock(mMutex);

    mPlaying = true;

    std::cout << "Playback Started\n";
}

void AudioPlayer::Pause()
{
    std::lock_guard<std::mutex> lock(mMutex);

    mPlaying = false;
}

void AudioPlayer::Stop()
{
    std::lock_guard<std::mutex> lock(mMutex);

    mPlaying = false;

    mDecoder.Close();
}





void AudioPlayer::SetBuffer(
    AudioBuffer* buffer
)
{

    mBuffer = buffer;

}





bool AudioPlayer::IsPlaying()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mPlaying;

}






void AudioPlayer::Process()
{

    std::lock_guard<std::mutex> lock(mMutex);

    if (!mPlaying)
        return;

    if (!mBuffer)
        return;

    float samples[4096];

    // Buffer ko pehle se bharne ki koshish karo
    while (mBuffer->Available() < 24000)
    {
        int count = mDecoder.ReadFrame(samples, 4096);

        if (count <= 0)
        {
            // Song khatam
            mPlaying = false;
            break;
        }

        mBuffer->Write(samples, count);
    }
}

void AudioPlayer::SetOutputFormat(int sampleRate, int channels)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mDecoder.SetOutputFormat(sampleRate, channels);
}
