#include "PitchComparisonEngine.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

PitchComparisonEngine::PitchComparisonEngine() {
    Reset();
}

void PitchComparisonEngine::Reset() {
    m_songDurationSec = 0.0;
    m_totalFramesEvaluated = 0;
    m_targetVoicedFrames = 0;
    m_userVoicedFrames = 0;

    m_inTuneFrames = 0;
    m_slightlyOffFrames = 0;
    m_outOfTuneFrames = 0;

    m_sumTargetHz = 0.0;
    m_sumUserHz = 0.0;
    m_sumErrorHz = 0.0;
    m_sumErrorCents = 0.0;

    m_currentInTuneStreak = 0;
    m_longestInTuneStreak = 0;

    m_highestUserMidi = static_cast<int16_t>(-1);
    m_lowestUserMidi = static_cast<int16_t>(-1);

    m_bestRollingAccuracy = 0.0f;
    m_worstRollingAccuracy = 100.0f;
}

void PitchComparisonEngine::SetConfidenceThreshold(float threshold) {
    m_confidenceThreshold = (threshold < 0.0f) ? 0.0f : ((threshold > 1.0f) ? 1.0f : threshold);
}

void PitchComparisonEngine::SetInTuneToleranceCents(float cents) {
    m_inTuneToleranceCents = (cents < 5.0f) ? 5.0f : cents;
}

int16_t PitchComparisonEngine::HzToMidi(float hz) {
    if (hz <= 0.0f) return static_cast<int16_t>(-1);
    float midiFloat = 69.0f + 12.0f * std::log2(hz / 440.0f);
    int midiInt = static_cast<int>(std::round(midiFloat));
    if (midiInt < 0 || midiInt > 127) return static_cast<int16_t>(-1);
    return static_cast<int16_t>(midiInt);
}

void PitchComparisonEngine::HzToNoteName(float hz, char* outBuffer, size_t bufferSize) {
    if (!outBuffer || bufferSize == 0) return;
    int16_t midi = HzToMidi(hz);
    if (midi < 0) {
        snprintf(outBuffer, bufferSize, "---");
        return;
    }

    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int noteIdx = midi % 12;
    int octave = (midi / 12) - 1;

    snprintf(outBuffer, bufferSize, "%s%d", noteNames[noteIdx], octave);
}

float PitchComparisonEngine::CalculateCentsError(float userHz, float targetHz) {
    if (userHz <= 0.0f || targetHz <= 0.0f) return 0.0f;
    return 1200.0f * std::log2(userHz / targetHz);
}

PitchComparisonResult PitchComparisonEngine::EvaluateFrame(const PitchFrameData& frameData, const TargetPitchTimeline& timeline) {
    TargetPitchFrame targetFrame = timeline.GetTargetPitchAt(frameData.timestampSec);
    bool isTargetVoiced = targetFrame.isVoiced || (targetFrame.targetHz > 0.0f);

    float effectiveTargetHz = isTargetVoiced ? targetFrame.targetHz : frameData.targetHz;
    return EvaluateDirect(frameData.timestampSec, frameData.userHz, frameData.userConfidence, effectiveTargetHz);
}

PitchComparisonResult PitchComparisonEngine::EvaluateDirect(double timestampSec, float userHz, float userConfidence, float targetHz) {
    PitchComparisonResult result{};
    result.timestampSec = timestampSec;
    result.userHz = userHz;
    result.targetHz = targetHz;

    HzToNoteName(userHz, result.userNote, sizeof(result.userNote));
    HzToNoteName(targetHz, result.targetNote, sizeof(result.targetNote));

    m_totalFramesEvaluated++;
    if (timestampSec > m_songDurationSec) {
        m_songDurationSec = timestampSec;
    }

    // Evaluate target presence
    if (targetHz > 0.0f) {
        m_targetVoicedFrames++;
    }

    // Unvoiced / Silence classification check
    if (userHz <= 0.0f || userConfidence < m_confidenceThreshold || targetHz <= 0.0f) {
        result.status = PitchStatus::Unvoiced;
        result.errorHz = 0.0f;
        result.errorCents = 0.0f;
        m_currentInTuneStreak = 0;
        return result;
    }

    // Voiced frame processing
    m_userVoicedFrames++;
    result.errorHz = userHz - targetHz;
    result.errorCents = CalculateCentsError(userHz, targetHz);

    m_sumTargetHz += targetHz;
    m_sumUserHz += userHz;
    m_sumErrorHz += std::abs(result.errorHz);
    m_sumErrorCents += std::abs(result.errorCents);

    // Track user vocal range
    int16_t userMidi = HzToMidi(userHz);
    if (userMidi >= 0) {
        if (m_lowestUserMidi < 0 || userMidi < m_lowestUserMidi) {
            m_lowestUserMidi = userMidi;
        }
        if (m_highestUserMidi < 0 || userMidi > m_highestUserMidi) {
            m_highestUserMidi = userMidi;
        }
    }

    // Status classification based on absolute cents error
    float absCents = std::abs(result.errorCents);
    if (absCents <= m_inTuneToleranceCents) {
        result.status = PitchStatus::InTune;
        m_inTuneFrames++;
        m_currentInTuneStreak++;
        if (m_currentInTuneStreak > m_longestInTuneStreak) {
            m_longestInTuneStreak = m_currentInTuneStreak;
        }
    }
    else if (absCents <= m_slightlyOffToleranceCents) {
        result.status = PitchStatus::SlightlyOff;
        m_slightlyOffFrames++;
        m_currentInTuneStreak = 0;
    }
    else {
        result.status = PitchStatus::OutOfTune;
        m_outOfTuneFrames++;
        m_currentInTuneStreak = 0;
    }

    // Update rolling accuracy statistics
    if (m_userVoicedFrames > 0) {
        float currentAccuracy = (static_cast<float>(m_inTuneFrames) / static_cast<float>(m_userVoicedFrames)) * 100.0f;
        if (currentAccuracy > m_bestRollingAccuracy) m_bestRollingAccuracy = currentAccuracy;
        if (currentAccuracy < m_worstRollingAccuracy) m_worstRollingAccuracy = currentAccuracy;
    }

    return result;
}

PerformanceReport PitchComparisonEngine::GetPerformanceReport() const {
    PerformanceReport report{};
    report.songDurationSec = m_songDurationSec;
    report.targetNotesDetected = m_targetVoicedFrames;
    report.userNotesDetected = m_userVoicedFrames;

    if (m_userVoicedFrames > 0) {
        report.avgTargetPitchHz = static_cast<float>(m_sumTargetHz / m_userVoicedFrames);
        report.avgUserPitchHz = static_cast<float>(m_sumUserHz / m_userVoicedFrames);
        report.avgPitchErrorHz = static_cast<float>(m_sumErrorHz / m_userVoicedFrames);
        report.avgPitchErrorCents = static_cast<float>(m_sumErrorCents / m_userVoicedFrames);

        report.inTunePercent = (static_cast<float>(m_inTuneFrames) / static_cast<float>(m_userVoicedFrames)) * 100.0f;
        report.slightlyOffPercent = (static_cast<float>(m_slightlyOffFrames) / static_cast<float>(m_userVoicedFrames)) * 100.0f;
        report.outOfTunePercent = (static_cast<float>(m_outOfTuneFrames) / static_cast<float>(m_userVoicedFrames)) * 100.0f;

        report.overallVocalAccuracy = report.inTunePercent;
        report.bestPitchAccuracy = m_bestRollingAccuracy;
        report.worstPitchAccuracy = (m_worstRollingAccuracy > 100.0f) ? 0.0f : m_worstRollingAccuracy;
    }
    else {
        report.avgTargetPitchHz = 0.0f;
        report.avgUserPitchHz = 0.0f;
        report.avgPitchErrorHz = 0.0f;
        report.avgPitchErrorCents = 0.0f;

        report.inTunePercent = 0.0f;
        report.slightlyOffPercent = 0.0f;
        report.outOfTunePercent = 0.0f;

        report.overallVocalAccuracy = 0.0f;
        report.bestPitchAccuracy = 0.0f;
        report.worstPitchAccuracy = 0.0f;
    }

    report.longestInTuneStreak = m_longestInTuneStreak;
    report.highestUserMidi = m_highestUserMidi;
    report.lowestUserMidi = m_lowestUserMidi;

    // Qualitative Performance Rating Assignment
    if (report.overallVocalAccuracy >= 90.0f) {
        report.rating = PerformanceRating::Excellent;
    }
    else if (report.overallVocalAccuracy >= 75.0f) {
        report.rating = PerformanceRating::Good;
    }
    else if (report.overallVocalAccuracy >= 60.0f) {
        report.rating = PerformanceRating::Average;
    }
    else {
        report.rating = PerformanceRating::NeedsImprovement;
    }

    return report;
}