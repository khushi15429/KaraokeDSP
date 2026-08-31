#include "PitchWorker.h"
#include "AudioBuffer.h"
#include <iostream>

PitchWorker::PitchWorker() : mInput(nullptr), mOutput(nullptr), mRunning(false) {}

PitchWorker::~PitchWorker() {
    Stop();
}

bool PitchWorker::Initialize(AudioBuffer* input, AudioBuffer* output) {
    mInput = input;
    mOutput = output;
    return true;
}

void PitchWorker::Start() {
    mRunning = true;
}

void PitchWorker::Stop() {
    mRunning = false;
}
