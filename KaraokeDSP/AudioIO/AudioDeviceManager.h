#pragma once
#ifndef AUDIODEVICEMANAGER_H
#define AUDIODEVICEMANAGER_H

#include <string>
#include <vector>
#include <QStringList>

struct AudioDeviceInfo {
    int id;
    std::string name;
    int maxInputChannels;
    int maxOutputChannels;
};

class AudioDeviceManager {
public:
    AudioDeviceManager();
    bool Initialize();
    void ListDevices();

    std::vector<AudioDeviceInfo> GetAllDevices();
    std::vector<AudioDeviceInfo> GetInputDevices();
    std::vector<AudioDeviceInfo> GetOutputDevices();

    QStringList GetInputDeviceNames();
    QStringList GetOutputDeviceNames();

private:
    bool mInitialized = false;
};

#endif // AUDIODEVICEMANAGER_H