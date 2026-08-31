#ifndef TARGETPITCHTIMELINE_H
#define TARGETPITCHTIMELINE_H

#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <cstdint>

struct TargetPitchFrame {
    double timeStartSec = 0.0;
    double timeEndSec = 0.0;
    double timestamp = 0.0;
    float targetHz = 0.0f;
    float targetF0 = 0.0f;
    int16_t midiNote = 0;
    bool isVoiced = false;
    bool isVibrato = false;
    float confidence = 0.0f;
    char noteName[16] = { 0 };
};

typedef TargetPitchFrame TargetNoteFrame;

class TargetPitchTimeline {
public:
    TargetPitchTimeline() = default;

    void Clear();
    size_t GetFrameCount() const;
    void SetFrames(const std::vector<TargetPitchFrame>& frames);
    std::vector<TargetPitchFrame> GetFrames() const;
    const std::vector<TargetPitchFrame>& GetFramesRef() const;

    void AddPitchFrame(const TargetPitchFrame& frame);
    void AddNoteFrame(const TargetPitchFrame& frame);

    TargetPitchFrame GetTargetPitchAt(double timestamp) const;
    TargetPitchFrame GetTargetAtTime(double timestamp) const;
    TargetPitchFrame GetTargetAtTime(double timestamp, float toleranceSec) const;

    bool LoadFromFile(const std::string& filePath);
    bool SaveToFile(const std::string& filePath) const;

private:
    std::vector<TargetPitchFrame> mFrames;
    mutable std::mutex mTimelineMutex;
};

#endif // TARGETPITCHTIMELINE_H