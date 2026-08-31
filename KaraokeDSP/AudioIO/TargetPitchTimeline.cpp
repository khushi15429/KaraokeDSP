#include "TargetPitchTimeline.h"

void TargetPitchTimeline::Clear() {
    std::lock_guard<std::mutex> lock(mTimelineMutex);
    mFrames.clear();
}

size_t TargetPitchTimeline::GetFrameCount() const {
    std::lock_guard<std::mutex> lock(mTimelineMutex);
    return mFrames.size();
}

void TargetPitchTimeline::SetFrames(const std::vector<TargetPitchFrame>& frames) {
    std::lock_guard<std::mutex> lock(mTimelineMutex);
    mFrames = frames;
}

std::vector<TargetPitchFrame> TargetPitchTimeline::GetFrames() const {
    std::lock_guard<std::mutex> lock(mTimelineMutex);
    return mFrames;
}

const std::vector<TargetPitchFrame>& TargetPitchTimeline::GetFramesRef() const {
    return mFrames;
}

void TargetPitchTimeline::AddPitchFrame(const TargetPitchFrame& frame) {
    std::lock_guard<std::mutex> lock(mTimelineMutex);
    mFrames.push_back(frame);
}

void TargetPitchTimeline::AddNoteFrame(const TargetPitchFrame& frame) {
    AddPitchFrame(frame);
}

TargetPitchFrame TargetPitchTimeline::GetTargetPitchAt(double timestamp) const {
    std::lock_guard<std::mutex> lock(mTimelineMutex);
    if (mFrames.empty()) return TargetPitchFrame();

    // FIX: the old range-based scan (timeStartSec <= t <= timeEndSec) returned the
    // FIRST matching frame it found, but frame windows are ~23ms wide while frames
    // are spaced ~10ms apart -- so consecutive windows overlap, and the scan would
    // often return the wrong (earlier, stale) frame instead of the closest one.
    // This is fixed by dropping the range scan entirely and doing a nearest-by-timestamp
    // binary search instead (frames are already stored in time order).
    auto it = std::lower_bound(mFrames.begin(), mFrames.end(), timestamp,
        [](const TargetPitchFrame& frame, double t) {
            return frame.timestamp < t;
        });

    if (it == mFrames.end()) return mFrames.back();
    if (it == mFrames.begin()) return mFrames.front();

    auto prev = it - 1;
    return (std::abs(it->timestamp - timestamp) < std::abs(prev->timestamp - timestamp)) ? *it : *prev;
}

TargetPitchFrame TargetPitchTimeline::GetTargetAtTime(double timestamp) const {
    return GetTargetPitchAt(timestamp);
}

TargetPitchFrame TargetPitchTimeline::GetTargetAtTime(double timestamp, float toleranceSec) const {
    (void)toleranceSec;
    return GetTargetPitchAt(timestamp);
}

bool TargetPitchTimeline::LoadFromFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mTimelineMutex);
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) return false;

    size_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    mFrames.resize(count);
    if (count > 0) {
        in.read(reinterpret_cast<char*>(mFrames.data()), count * sizeof(TargetPitchFrame));
    }
    return true;
}

bool TargetPitchTimeline::SaveToFile(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(mTimelineMutex);
    std::ofstream out(filePath, std::ios::binary);
    if (!out.is_open()) return false;

    size_t count = mFrames.size();
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));
    if (count > 0) {
        out.write(reinterpret_cast<const char*>(mFrames.data()), count * sizeof(TargetPitchFrame));
    }
    return true;
}