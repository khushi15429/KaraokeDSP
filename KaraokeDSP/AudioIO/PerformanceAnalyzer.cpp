#include "PerformanceAnalyzer.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>

void PerformanceAnalyzer::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_totalSingingTimeSec = 0.0;
    m_evaluatedFrames = 0;
    m_inTuneFrames = 0;
    m_slightlyOffFrames = 0;
    m_outOfTuneFrames = 0;
    m_unvoicedFrames = 0;

    m_sumAbsoluteCents = 0.0;
    m_maxCentsError = 0.0f;

    m_currentStreak = 0;
    m_longestInTuneStreak = 0;

    m_bestAccuracy = 0.0f;
    m_worstAccuracy = 100.0f;

    m_highestUserMidi = static_cast<int16_t>(-1);
    m_lowestUserMidi = static_cast<int16_t>(-1);
}

void PerformanceAnalyzer::AddResult(const PitchComparisonResult& result) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_evaluatedFrames++;
    if (result.timestampSec > m_totalSingingTimeSec) {
        m_totalSingingTimeSec = result.timestampSec;
    }

    if (result.status == PitchStatus::Unvoiced) {
        m_unvoicedFrames++;
        m_currentStreak = 0;
        return;
    }

    float absCents = std::abs(result.errorCents);
    m_sumAbsoluteCents += static_cast<double>(absCents);
    if (absCents > m_maxCentsError) {
        m_maxCentsError = absCents;
    }

    switch (result.status) {
    case PitchStatus::InTune:
        m_inTuneFrames++;
        m_currentStreak++;
        if (m_currentStreak > m_longestInTuneStreak) {
            m_longestInTuneStreak = m_currentStreak;
        }
        break;
    case PitchStatus::SlightlyOff:
        m_slightlyOffFrames++;
        m_currentStreak = 0;
        break;
    case PitchStatus::OutOfTune:
        m_outOfTuneFrames++;
        m_currentStreak = 0;
        break;
    default:
        break;
    }

    uint32_t voicedCount = m_inTuneFrames + m_slightlyOffFrames + m_outOfTuneFrames;
    if (voicedCount > 0) {
        float currentAccuracy = (static_cast<float>(m_inTuneFrames) / static_cast<float>(voicedCount)) * 100.0f;
        if (currentAccuracy > m_bestAccuracy) m_bestAccuracy = currentAccuracy;
        if (currentAccuracy < m_worstAccuracy) m_worstAccuracy = currentAccuracy;
    }
}

float PerformanceAnalyzer::CalculatePerformanceScore() const {
    uint32_t voicedCount = m_inTuneFrames + m_slightlyOffFrames + m_outOfTuneFrames;
    if (voicedCount == 0) return 0.0f;

    float score = ((static_cast<float>(m_inTuneFrames) * 1.0f) +
        (static_cast<float>(m_slightlyOffFrames) * 0.5f)) /
        static_cast<float>(voicedCount) * 100.0f;

    return std::min(100.0f, std::max(0.0f, score));
}

PerformanceReport PerformanceAnalyzer::GetReport() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    PerformanceReport report{};
    report.songDurationSec = m_totalSingingTimeSec;
    report.targetNotesDetected = m_evaluatedFrames - m_unvoicedFrames;
    report.userNotesDetected = m_inTuneFrames + m_slightlyOffFrames + m_outOfTuneFrames;

    if (report.userNotesDetected > 0) {
        float totalVoiced = static_cast<float>(report.userNotesDetected);
        report.inTunePercent = (static_cast<float>(m_inTuneFrames) / totalVoiced) * 100.0f;
        report.slightlyOffPercent = (static_cast<float>(m_slightlyOffFrames) / totalVoiced) * 100.0f;
        report.outOfTunePercent = (static_cast<float>(m_outOfTuneFrames) / totalVoiced) * 100.0f;

        report.avgPitchErrorCents = static_cast<float>(m_sumAbsoluteCents / static_cast<double>(totalVoiced));
        report.overallVocalAccuracy = CalculatePerformanceScore();
        report.bestPitchAccuracy = m_bestAccuracy;
        report.worstPitchAccuracy = (m_worstAccuracy > 100.0f) ? 0.0f : m_worstAccuracy;
    }
    else {
        report.inTunePercent = 0.0f;
        report.slightlyOffPercent = 0.0f;
        report.outOfTunePercent = 0.0f;
        report.avgPitchErrorCents = 0.0f;
        report.overallVocalAccuracy = 0.0f;
        report.bestPitchAccuracy = 0.0f;
        report.worstPitchAccuracy = 0.0f;
    }

    report.longestInTuneStreak = m_longestInTuneStreak;

    float score = report.overallVocalAccuracy;
    if (score >= 90.0f) {
        report.rating = PerformanceRating::Excellent;
    }
    else if (score >= 75.0f) {
        report.rating = PerformanceRating::Good;
    }
    else if (score >= 60.0f) {
        report.rating = PerformanceRating::Average;
    }
    else {
        report.rating = PerformanceRating::NeedsImprovement;
    }

    return report;
}

void PerformanceAnalyzer::PrintLiveStatus(double timeSec, const PitchComparisonResult& result) const {
    const char* statusStr = "UNVOICED";
    if (result.status == PitchStatus::InTune) statusStr = "IN TUNE";
    else if (result.status == PitchStatus::SlightlyOff) statusStr = "SLIGHTLY OFF";
    else if (result.status == PitchStatus::OutOfTune) statusStr = "OUT OF TUNE";

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "[" << timeSec << "s] Target: " << result.targetNote << " (" << result.targetHz << " Hz)\n"
        << "        User  : " << result.userNote << " (" << result.userHz << " Hz)\n"
        << "        Error : " << result.errorCents << " cents\n"
        << "        Status: " << statusStr << "\n" << std::endl;
}

void PerformanceAnalyzer::PrintFinalReport() const {
    PerformanceReport report = GetReport();

    const char* ratingStr = "NEEDS IMPROVEMENT";
    if (report.rating == PerformanceRating::Excellent) ratingStr = "EXCELLENT";
    else if (report.rating == PerformanceRating::Good) ratingStr = "GOOD";
    else if (report.rating == PerformanceRating::Average) ratingStr = "AVERAGE";

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "\n========== KARAOKE PERFORMANCE REPORT ==========\n\n"
        << "Total Singing Time : " << report.songDurationSec << " sec\n"
        << "Evaluated Frames   : " << m_evaluatedFrames << "\n\n"
        << "IN TUNE            : " << report.inTunePercent << "%\n"
        << "SLIGHTLY OFF       : " << report.slightlyOffPercent << "%\n"
        << "OUT OF TUNE        : " << report.outOfTunePercent << "%\n\n"
        << "Average Error      : " << report.avgPitchErrorCents << " cents\n"
        << "Maximum Error      : " << m_maxCentsError << " cents\n\n"
        << "Overall Score      : " << static_cast<int>(std::round(report.overallVocalAccuracy)) << "/100\n"
        << "Performance        : " << ratingStr << "\n\n"
        << "=================================================\n" << std::endl;
}