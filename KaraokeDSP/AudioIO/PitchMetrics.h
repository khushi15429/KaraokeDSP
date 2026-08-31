#ifndef PITCHMETRICS_H
#define PITCHMETRICS_H

#include <cstdint>
#include <string>

enum class PitchStatus {
    Unvoiced = 0,
    InTune,
    SlightlyOff,
    OutOfTune
};

enum class PerformanceRating {
    NeedsImprovement = 0,
    Average,
    Good,
    Excellent
};

struct PitchFrameData {
    double timestampSec = 0.0;
    float userHz = 0.0f;
    float userConfidence = 0.0f;
    float targetHz = 0.0f;
};

struct PitchComparisonResult {
    double timestampSec = 0.0;
    float targetHz = 0.0f;
    float userHz = 0.0f;
    float errorHz = 0.0f;
    float errorCents = 0.0f;
    char userNote[16] = "";
    char targetNote[16] = "";
    PitchStatus status = PitchStatus::Unvoiced;
};

struct PerformanceReport {
    double songDurationSec = 0.0;
    uint32_t targetNotesDetected = 0;
    uint32_t userNotesDetected = 0;
    float inTunePercent = 0.0f;
    float slightlyOffPercent = 0.0f;
    float outOfTunePercent = 0.0f;
    float avgPitchErrorCents = 0.0f;

    // Added Missing Members
    float avgTargetPitchHz = 0.0f;
    float avgUserPitchHz = 0.0f;
    float avgPitchErrorHz = 0.0f;
    int16_t highestUserMidi = -1;
    int16_t lowestUserMidi = -1;

    float overallVocalAccuracy = 0.0f;
    float bestPitchAccuracy = 0.0f;
    float worstPitchAccuracy = 0.0f;
    uint32_t longestInTuneStreak = 0;
    PerformanceRating rating = PerformanceRating::NeedsImprovement;
};

#endif // PITCHMETRICS_H