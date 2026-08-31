#include "AudioDeviceManager.h"
#include <iostream>
#include <portaudio.h>

AudioDeviceManager::AudioDeviceManager() {
    mInitialized = false;
}

bool AudioDeviceManager::Initialize() {
    PaError error = Pa_Initialize();
    if (error != paNoError) return false;
    mInitialized = true;
    return true;
}

void AudioDeviceManager::ListDevices() {
    int count = Pa_GetDeviceCount();
    for (int i = 0; i < count; i++) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (info) {
            std::cout << i << " : " << info->name
                << " In:" << info->maxInputChannels
                << " Out:" << info->maxOutputChannels << std::endl;
        }
    }
}

std::vector<AudioDeviceInfo> AudioDeviceManager::GetAllDevices() {
    std::vector<AudioDeviceInfo> list;
    int count = Pa_GetDeviceCount();
    for (int i = 0; i < count; i++) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info) continue;
        list.push_back({ i, info->name, info->maxInputChannels, info->maxOutputChannels });
    }
    return list;
}

std::vector<AudioDeviceInfo> AudioDeviceManager::GetInputDevices() {
    std::vector<AudioDeviceInfo> list;
    for (auto& d : GetAllDevices()) {
        if (d.maxInputChannels > 0) list.push_back(d);
    }
    return list;
}

std::vector<AudioDeviceInfo> AudioDeviceManager::GetOutputDevices() {
    std::vector<AudioDeviceInfo> list;
    for (auto& d : GetAllDevices()) {
        if (d.maxOutputChannels > 0) list.push_back(d);
    }
    return list;
}

QStringList AudioDeviceManager::GetInputDeviceNames() {
    QStringList names;
    for (const auto& dev : GetInputDevices()) {
        names.append(QString::fromStdString(dev.name));
    }
    return names;
}

QStringList AudioDeviceManager::GetOutputDeviceNames() {
    QStringList names;
    for (const auto& dev : GetOutputDevices()) {
        names.append(QString::fromStdString(dev.name));
    }
    return names;
}