#ifndef SCALELOCKER_H
#define SCALELOCKER_H

#include <cmath>
#include <algorithm>

class ScaleLocker {
public:
    /**
     * @brief Real-time Auto-Tune: User ki voice ko Song Track ke sur me match karta hai.
     * @param userFreq User ke mic ki voice pitch (Hz)
     * @param trackFreq Song ki background music pitch (Hz)
     * @return Track se harmonized aur tuned frequency Output
     */
    static float MatchVoiceToTrack(float userFreq, float trackFreq) {
        // Safe check for invalid/silent frequencies
        if (userFreq <= 20.0f || userFreq >= 5000.0f) {
            return userFreq;
        }

        // 1. User Voice ko MIDI Note me convert karo
        double userMidi = 69.0 + 12.0 * std::log2(static_cast<double>(userFreq) / 440.0);

        int targetMidiNote = 0;

        // 2. Agar Song ka music background me baj raha hai:
        if (trackFreq > 20.0f) {
            // Track ki frequency ko MIDI me convert karo
            double trackMidi = 69.0 + 12.0 * std::log2(static_cast<double>(trackFreq) / 440.0);

            // Calculation: User ki aawaaz ko track ke closest harmonic note par shift karo
            double noteDifference = userMidi - trackMidi;
            int semitoneShift = static_cast<int>(std::round(noteDifference));

            targetMidiNote = static_cast<int>(std::round(trackMidi)) + semitoneShift;
        }
        else {
            // Agar song mute/pause hai, to auto nearest semitone lock
            targetMidiNote = static_cast<int>(std::round(userMidi));
        }

        // 3. Target MIDI note ko wapas Perfect Tuned Hz me convert karo
        float tunedFrequency = static_cast<float>(440.0 * std::pow(2.0, (targetMidiNote - 69) / 12.0));

        return tunedFrequency;
    }
};

#endif // SCALELOCKER_H