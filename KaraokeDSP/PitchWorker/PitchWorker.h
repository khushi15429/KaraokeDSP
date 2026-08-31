#pragma once
#ifndef PITCHWORKER_H
#define PITCHWORKER_H

class AudioBuffer;

class PitchWorker
{
public:
    PitchWorker();
    ~PitchWorker();
    bool Initialize(AudioBuffer* input, AudioBuffer* output);
    void Start();
    void Stop();

private:
    AudioBuffer* mInput = nullptr;
    AudioBuffer* mOutput = nullptr;
    bool mRunning = false;
};

#endif // PITCHWORKER_H