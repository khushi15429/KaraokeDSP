#ifndef VOICECOMPARISON_H
#define VOICECOMPARISON_H

#include "TargetPitchTimeline.h"
#include "AudioStream.h"
#include <string>
#include <vector>

struct ComparisonReportMeta {
	std::string songTitle;
	std::string songPath;
	std::string targetJsonPath;
	std::string userId;
};

struct ComparisonSummary {
	int totalFrames = 0;
	int validTargetFrames = 0;
	int validRawFrames = 0;
	int validFinalFrames = 0;
	int validComparisons = 0;
	int validFinalComparisons = 0;
	int inTuneFrames = 0;
	int slightlyFlatFrames = 0;
	int flatFrames = 0;
	int slightlySharpFrames = 0;
	int sharpFrames = 0;
	int unvoicedFrames = 0;
	int noTargetFrames = 0;
	double rawAccuracyPercent = 0.0;
	double finalAccuracyPercent = 0.0;
	double averageRawErrorCents = 0.0;
	double averageFinalErrorCents = 0.0;
	double averageCorrectionSemitones = 0.0;
	double averageCorrectionCents = 0.0;
	double averageAbsRawErrorCents = 0.0;
	double averageAbsFinalErrorCents = 0.0;
	double averageImprovementCents = 0.0;
	double improvementPercent = 0.0;
	int improvedFrames = 0;
	int worsenedFrames = 0;
	int unchangedFrames = 0;
	double recordingDurationSec = 0.0;
	std::string accuracyDefinition;
	std::string error; // P4/P7: non-empty when the report is invalid (no valid target overlap); surfaced to the UI
};

class VoiceComparison {
public:
	static bool GenerateComparisonReport(
		const TargetPitchTimeline& targetTimeline,
		const std::vector<AudioStream::LiveRawFrame>& rawFrames,
		const ComparisonReportMeta& meta,
		const std::string& outFilePath,
		ComparisonSummary& outSummary,
		double tolVeryGood = 25.0,
		double tolGood = 50.0,
		double tolAccept = 75.0
	);
};

#endif // VOICECOMPARISON_H
