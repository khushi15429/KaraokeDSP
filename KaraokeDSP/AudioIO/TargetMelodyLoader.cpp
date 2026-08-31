#include "TargetMelodyLoader.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

float TargetMelodyLoader::MidiToHz(int midiNote) {
    if (midiNote < 0 || midiNote > 127) return 0.0f;
    return 440.0f * std::pow(2.0f, static_cast<float>(midiNote - 69) / 12.0f);
}

void TargetMelodyLoader::MidiToNoteName(int midiNote, char* outBuffer, size_t bufferSize) {
    if (!outBuffer || bufferSize == 0) return;
    if (midiNote < 0 || midiNote > 127) {
        snprintf(outBuffer, bufferSize, "---");
        return;
    }

    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int noteIdx = midiNote % 12;
    int octave = (midiNote / 12) - 1;

    snprintf(outBuffer, bufferSize, "%s%d", noteNames[noteIdx], octave);
}

bool TargetMelodyLoader::LoadFromFile(const std::string& filePath, TargetPitchTimeline& outTimeline) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    return LoadFromJSONString(buffer.str(), outTimeline);
}

bool TargetMelodyLoader::LoadFromJSONString(const std::string& jsonContent, TargetPitchTimeline& outTimeline) {
    outTimeline.Clear();

    size_t notesArrayPos = jsonContent.find("\"notes\"");
    if (notesArrayPos == std::string::npos) {
        return false;
    }

    size_t startArray = jsonContent.find('[', notesArrayPos);
    size_t endArray = jsonContent.find(']', startArray);
    if (startArray == std::string::npos || endArray == std::string::npos) {
        return false;
    }

    std::string arrayContent = jsonContent.substr(startArray + 1, endArray - startArray - 1);

    size_t objectStart = 0;
    while ((objectStart = arrayContent.find('{', objectStart)) != std::string::npos) {
        size_t objectEnd = arrayContent.find('}', objectStart);
        if (objectEnd == std::string::npos) break;

        std::string noteObjStr = arrayContent.substr(objectStart, objectEnd - objectStart + 1);

        TargetNoteFrame frame{};
        frame.confidence = 1.0f;

        // Parse "start"
        size_t pos = noteObjStr.find("\"start\"");
        if (pos != std::string::npos) {
            frame.timeStartSec = std::stod(noteObjStr.substr(noteObjStr.find(':', pos) + 1));
        }

        // Parse "end"
        pos = noteObjStr.find("\"end\"");
        if (pos != std::string::npos) {
            frame.timeEndSec = std::stod(noteObjStr.substr(noteObjStr.find(':', pos) + 1));
        }

        // Parse "midi"
        pos = noteObjStr.find("\"midi\"");
        if (pos != std::string::npos) {
            int parsedMidi = std::stoi(noteObjStr.substr(noteObjStr.find(':', pos) + 1));
            if (parsedMidi >= 0 && parsedMidi <= 127) {
                frame.midiNote = static_cast<int16_t>(parsedMidi);
            }
            else {
                frame.midiNote = static_cast<int16_t>(-1);
            }
        }

        // Parse "hz"
        pos = noteObjStr.find("\"hz\"");
        if (pos != std::string::npos) {
            frame.targetHz = std::stof(noteObjStr.substr(noteObjStr.find(':', pos) + 1));
        }

        // Calculate missing targetHz from MIDI if needed
        if (frame.targetHz <= 0.0f && frame.midiNote >= 0) {
            frame.targetHz = MidiToHz(static_cast<int>(frame.midiNote));
        }

        // Generate note name if not populated
        if (frame.midiNote >= 0) {
            MidiToNoteName(static_cast<int>(frame.midiNote), frame.noteName, sizeof(frame.noteName));
        }

        // Parse "vibrato"
        pos = noteObjStr.find("\"vibrato\"");
        if (pos != std::string::npos) {
            std::string vibStr = noteObjStr.substr(noteObjStr.find(':', pos) + 1);
            frame.isVibrato = (vibStr.find("true") != std::string::npos);
        }

        // Add valid note segments
        if (frame.timeEndSec > frame.timeStartSec && (frame.targetHz > 0.0f || frame.midiNote >= 0)) {
            outTimeline.AddNoteFrame(frame);
        }

        objectStart = objectEnd + 1;
    }

    return (outTimeline.GetFrameCount() > 0);
}

void TargetMelodyLoader::GenerateTestScaleTimeline(TargetPitchTimeline& outTimeline) {
    outTimeline.Clear();

    // TEST UTILITY ONLY: C4 Major Scale
    static const int testMidiNotes[] = { 60, 62, 64, 65, 67, 69, 71, 72 };
    double currentTime = 1.0;
    double noteDuration = 1.0;

    for (int midi : testMidiNotes) {
        TargetNoteFrame frame{};
        frame.timeStartSec = currentTime;
        frame.timeEndSec = currentTime + noteDuration;
        frame.midiNote = static_cast<int16_t>(midi);
        frame.targetHz = MidiToHz(midi);
        frame.confidence = 1.0f;
        frame.isVibrato = false;
        MidiToNoteName(midi, frame.noteName, sizeof(frame.noteName));

        outTimeline.AddNoteFrame(frame);
        currentTime += noteDuration;
    }
}