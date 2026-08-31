#ifndef PITCHCORRECTOR_H
#define PITCHCORRECTOR_H

#include <cmath>
#include <vector>
#include <algorithm>
#include <string>

class PitchCorrector {
public:
    PitchCorrector() = default;

    // Convert Frequency (Hz) to MIDI Note Number
    static float FrequencyToMidi(float freq) {
        if (freq <= 0.0f) return 0.0f;
        return 69.0f + 12.0f * std::log2(freq / 440.0f);
    }

    // Convert MIDI Note Number back to Frequency (Hz)
    static float MidiToFrequency(float midiNote) {
        return 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
    }

    // Snap the input pitch to the nearest note in the active Scale
    float CorrectPitch(float inputFreq, const std::vector<int>& activeScaleNotes, float correctionAmount = 0.8f) {
        if (inputFreq < 60.0f || inputFreq > 1000.0f || activeScaleNotes.empty()) {
            return inputFreq; // Skip non-vocal frequencies / silence
        }

        float currentMidi = FrequencyToMidi(inputFreq);
        int nearestMidiNote = std::round(currentMidi);

        // Find the closest valid note in the scale
        int bestTargetNote = nearestMidiNote;
        int minDistance = 999;

        for (int note : activeScaleNotes) {
            // Check scale pitch class matching
            int dist = std::abs((nearestMidiNote % 12) - (note % 12));
            if (dist > 6) dist = 12 - dist; // Octave wrap check

            if (dist < minDistance) {
                minDistance = dist;
                // Calculate target MIDI note considering octave offset
                int octave = nearestMidiNote / 12;
                bestTargetNote = octave * 12 + note;
            }
        }

        float targetFreq = MidiToFrequency(static_cast<float>(bestTargetNote));

        // Smooth Interpolation: Natural vocal tuning without harsh robotic cuts
        float correctedFreq = inputFreq + (targetFreq - inputFreq) * correctionAmount;
        return correctedFreq;
    }
};

#endif // PITCHCORRECTOR_H