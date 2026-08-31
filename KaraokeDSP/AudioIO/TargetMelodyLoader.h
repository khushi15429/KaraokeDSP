#ifndef TARGETMELODYLOADER_H
#define TARGETMELODYLOADER_H

#include "TargetPitchTimeline.h"
#include <string>

class TargetMelodyLoader {
public:
    TargetMelodyLoader() = default;
    ~TargetMelodyLoader() = default;

    // Loads melody notes from a JSON sidecar file into the timeline
    static bool LoadFromFile(const std::string& filePath, TargetPitchTimeline& outTimeline);

    // Parses raw JSON string into the timeline
    static bool LoadFromJSONString(const std::string& jsonContent, TargetPitchTimeline& outTimeline);

    // TEST UTILITY ONLY: Generates a basic test scale (C Major) for testing without a sidecar file
    static void GenerateTestScaleTimeline(TargetPitchTimeline& outTimeline);
    static std::string MidiToNoteName(int midiNote); // <-- Add this line

private:
    // Helper to calculate exact frequency in Hz from MIDI note number
    static float MidiToHz(int midiNote);

    // Helper to generate pitch string name (e.g., 69 -> "A4")
    static void MidiToNoteName(int midiNote, char* outBuffer, size_t bufferSize);
};

#endif // TARGETMELODYLOADER_H