#ifndef KEYSCALEMATCHER_H
#define KEYSCALEMATCHER_H

#include <cmath>
#include <string>
#include <array>
#include <algorithm>

class KeyScaleMatcher {
public:
    enum ScaleType {
        MAJOR = 0,
        MINOR = 1,
        CHROMATIC = 2
    };

    static float GetNearestNoteInScale(float freq, int rootNote, ScaleType scaleType) {
        if (freq <= 30.0f) return freq;

        // Convert Hz to MIDI Note Number
        double midiNote = 69.0 + 12.0 * std::log2(freq / 440.0);
        int nearestMidi = static_cast<int>(std::round(midiNote));

        if (scaleType == CHROMATIC) {
            return static_cast<float>(440.0 * std::pow(2.0, (nearestMidi - 69) / 12.0));
        }

        // Scale Intervals
        // Major: 0, 2, 4, 5, 7, 9, 11
        // Minor: 0, 2, 3, 5, 7, 8, 10
        static const int majorMask[12] = { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 };
        static const int minorMask[12] = { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 };

        const int* mask = (scaleType == MAJOR) ? majorMask : minorMask;

        int bestMidi = nearestMidi;
        int minDistance = 999;

        for (int delta = -6; delta <= 6; ++delta) {
            int candidate = nearestMidi + delta;
            int noteInOctave = (candidate % 12 + 12) % 12;
            int relativeNote = (noteInOctave - rootNote + 12) % 12;

            if (mask[relativeNote] == 1) {
                int dist = std::abs(delta);
                if (dist < minDistance) {
                    minDistance = dist;
                    bestMidi = candidate;
                }
            }
        }

        return static_cast<float>(440.0 * std::pow(2.0, (bestMidi - 69) / 12.0));
    }

    static std::string GetNoteName(float freq) {
        if (freq <= 30.0f) return "---";
        static const std::string NoteNames[12] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        int midiNote = static_cast<int>(std::round(69.0 + 12.0 * std::log2(freq / 440.0)));
        int noteIndex = (midiNote % 12 + 12) % 12;
        int octave = (midiNote / 12) - 1;
        return NoteNames[noteIndex] + std::to_string(octave);
    }
};

#endif // KEYSCALEMATCHER_H