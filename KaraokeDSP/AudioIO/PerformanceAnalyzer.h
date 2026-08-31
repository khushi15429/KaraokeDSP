#ifndef PERFORMANCEANALYZER_H
#define PERFORMANCEANALYZER_H

#include "PitchMetrics.h"
#include <mutex>
#include <cstdint>

class PerformanceAnalyzer {
public:
    PerformanceAnalyzer() = default;
    ~PerformanceAnalyzer() = default;

    void Reset();
    void AddResult(const PitchComparisonResult& result);
    PerformanceReport GetReport() const;
    float CalculatePerformanceScore() const;
    void PrintLiveStatus(double timeSec, const PitchComparisonResult& result) const;
    void PrintFinalReport() const;

    inline void reset() { Reset(); }
    inline void addFrameEvaluation(const PitchComparisonResult& result) { AddResult(result); }
    inline float getScore() const { return CalculatePerformanceScore(); }
    inline float getAccuracyPercentage() const { return CalculatePerformanceScore(); }
    inline PerformanceReport getSummary() const { return GetReport(); }
    inline PerformanceReport getReport() const { return GetReport(); }

private:
    mutable std::mutex m_mutex;

    double m_totalSingingTimeSec{ 0.0 };
    uint32_t m_evaluatedFrames{ 0 };
    uint32_t m_inTuneFrames{ 0 };
    uint32_t m_slightlyOffFrames{ 0 };
    uint32_t m_outOfTuneFrames{ 0 };
    uint32_t m_unvoicedFrames{ 0 };

    double m_sumAbsoluteCents{ 0.0 };
    float m_maxCentsError{ 0.0f };

    uint32_t m_currentStreak{ 0 };
    uint32_t m_longestInTuneStreak{ 0 };

    float m_bestAccuracy{ 0.0f };
    float m_worstAccuracy{ 100.0f };

    int16_t m_highestUserMidi{ static_cast<int16_t>(-1) };
    int16_t m_lowestUserMidi{ static_cast<int16_t>(-1) };
};

#endif // PERFORMANCEANALYZER_H