#ifndef PITCHCOMPARISONENGINE_H
#define PITCHCOMPARISONENGINE_H

#include "PitchMetrics.h"
#include "TargetPitchTimeline.h"
#include <cstdint>

class PitchComparisonEngine {
public:
    PitchComparisonEngine();
    ~PitchComparisonEngine() = default;

    void Reset();
    void SetConfidenceThreshold(float threshold);
    void SetInTuneToleranceCents(float cents);

    PitchComparisonResult EvaluateFrame(const PitchFrameData& frameData, const TargetPitchTimeline& timeline);
    PitchComparisonResult EvaluateDirect(double timestampSec, float userHz, float userConfidence, float targetHz);

    // Overload for direct live user pitch vs target pitch evaluations
    inline PitchComparisonResult evaluateFrame(float userHz, float targetHz, double timestampSec = 0.0) {
        return EvaluateDirect(timestampSec, userHz, 1.0f, targetHz);
    }

    PerformanceReport GetPerformanceReport() const;

    static int16_t HzToMidi(float hz);
    static void HzToNoteName(float hz, char* outBuffer, size_t bufferSize);
    static float CalculateCentsError(float userHz, float targetHz);

private:
    float m_confidenceThreshold{ 0.5f };
    float m_inTuneToleranceCents{ 50.0f };
    float m_slightlyOffToleranceCents{ 100.0f };

    double m_songDurationSec{ 0.0 };
    uint32_t m_totalFramesEvaluated{ 0 };
    uint32_t m_targetVoicedFrames{ 0 };
    uint32_t m_userVoicedFrames{ 0 };

    uint32_t m_inTuneFrames{ 0 };
    uint32_t m_slightlyOffFrames{ 0 };
    uint32_t m_outOfTuneFrames{ 0 };

    double m_sumTargetHz{ 0.0 };
    double m_sumUserHz{ 0.0 };
    double m_sumErrorHz{ 0.0 };
    double m_sumErrorCents{ 0.0 };

    uint32_t m_currentInTuneStreak{ 0 };
    uint32_t m_longestInTuneStreak{ 0 };

    int16_t m_highestUserMidi{ static_cast<int16_t>(-1) };
    int16_t m_lowestUserMidi{ static_cast<int16_t>(-1) };

    float m_bestRollingAccuracy{ 0.0f };
    float m_worstRollingAccuracy{ 100.0f };
};

#endif // PITCHCOMPARISONENGINE_H