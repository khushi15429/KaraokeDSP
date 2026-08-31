#include "VoiceComparison.h"
#include <fstream>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

static int FrequencyToMidi(double freq) {
	if (!(freq > 0.0) || !std::isfinite(freq)) return 0;
	return static_cast<int>(std::round(69.0 + 12.0 * std::log2(freq / 440.0)));
}

static std::string MidiToNoteName(int midi) {
	if (midi <= 0) return "---";
	static const char* names[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
	int octave = (midi / 12) - 1;
	int idx = ((midi % 12) + 12) % 12;
	std::ostringstream ss;
	ss << names[idx] << octave;
	return ss.str();
}

static bool ValidHz(double hz) {
	return hz > 0.0 && std::isfinite(hz);
}

static bool WriteOptionalNumber(std::ostream& out, double value, bool valid, int precision = 2) {
	if (!valid || !std::isfinite(value)) {
		out << "null";
		return false;
	}
	out << std::fixed << std::setprecision(precision) << value;
	return true;
}

static std::string JsonEscape(const std::string& s) {
	std::string o;
	o.reserve(s.size());
	for (char c : s) {
		switch (c) {
		case '\\': o += "\\\\"; break;
		case '"': o += "\\\""; break;
		case '\n': o += "\\n"; break;
		case '\r': o += "\\r"; break;
		case '\t': o += "\\t"; break;
		default: o += c; break;
		}
	}
	return o;
}

static std::string ClassifyStatus(bool hasTarget, bool hasUser, double errorCents, double tolVeryGood, double tolAccept) {
	if (!hasTarget) return "NO_TARGET";
	if (!hasUser) return "UNVOICED";
	const double absC = std::abs(errorCents);
	if (absC <= tolVeryGood) return "IN_TUNE";
	if (errorCents < 0.0 && absC <= tolAccept) return "SLIGHTLY_FLAT";
	if (errorCents < 0.0) return "FLAT";
	if (absC <= tolAccept) return "SLIGHTLY_SHARP";
	return "SHARP";
}

static std::string ClassifyQuality(bool hasTarget, bool hasUser, double errorCents,
	double tolVeryGood, double tolGood, double tolAccept) {
	if (!hasTarget) return "NO_TARGET";
	if (!hasUser) return "UNVOICED";
	const double absC = std::abs(errorCents);
	if (absC <= tolVeryGood) return "VERY_GOOD";
	if (absC <= tolGood) return "GOOD";
	if (absC <= tolAccept) return "ACCEPTABLE";
	return "POOR";
}

bool VoiceComparison::GenerateComparisonReport(
	const TargetPitchTimeline& targetTimeline,
	const std::vector<AudioStream::LiveRawFrame>& rawFrames,
	const ComparisonReportMeta& meta,
	const std::string& outFilePath,
	ComparisonSummary& outSummary,
	double tolVeryGood,
	double tolGood,
	double tolAccept
) {
	outSummary = ComparisonSummary();
	outSummary.accuracyDefinition =
		"Accuracy is the percentage of valid comparison frames (voiced target AND measured voiced source) whose absolute cents error is <= 25 (VERY_GOOD). Per-frame quality buckets: VERY_GOOD <=25c, GOOD <=50c, ACCEPTABLE <=75c, else POOR. Raw uses the microphone PitchDetector frequency; final uses the processed-vocal monitor tap after pitch shift/effects and before song mix. Missing or unvoiced frames are excluded, not treated as in-tune.";

	std::cout << "[Comparison] Generating report..." << std::endl;
	std::cout << "[Comparison] Raw frames: " << rawFrames.size() << std::endl;

	if (rawFrames.empty()) {
		std::cout << "[Comparison] No raw frames captured; report not generated" << std::endl;
		outSummary.error = "No microphone frames were captured during the recording.";
		return false;
	}

	std::ostringstream out;

	int targetMatches = 0;
	int finalProcessedFrames = 0;
	double sumRawAbs = 0.0;
	double sumFinalAbs = 0.0;
	double sumCorrSemi = 0.0;
	int corrCount = 0;
	int rawInTune = 0;
	int finalInTune = 0;
	int improvedFrames = 0;
	int worsenedFrames = 0;
	int unchangedFrames = 0;

	{
		double tMin = rawFrames.front().timestampSec;
		double tMax = rawFrames.front().timestampSec;
		for (const auto& rf : rawFrames) {
			if (std::isfinite(rf.timestampSec)) {
				if (rf.timestampSec < tMin) tMin = rf.timestampSec;
				if (rf.timestampSec > tMax) tMax = rf.timestampSec;
			}
		}
		outSummary.recordingDurationSec = std::max(0.0, tMax - tMin);
	}

	out << "{\n";
	out << "  \"report_version\": \"2.0\",\n";
	out << "  \"accuracy_definition\": \"" << JsonEscape(outSummary.accuracyDefinition) << "\",\n";
	out << "  \"thresholds_cents\": { \"very_good\": " << tolVeryGood
		<< ", \"good\": " << tolGood << ", \"acceptable\": " << tolAccept << " },\n";
	out << "  \"reference_source\": {\n";
	out << "    \"type\": \"original_singer_pitch_timeline\",\n";
	out << "    \"path\": \"" << JsonEscape(meta.targetJsonPath) << "\",\n";
	out << "    \"read_only\": true\n";
	out << "  },\n";
	out << "  \"song\": {\n";
	out << "    \"title\": \"" << JsonEscape(meta.songTitle) << "\",\n";
	out << "    \"path\": \"" << JsonEscape(meta.songPath) << "\",\n";
	out << "    \"target_json_path\": \"" << JsonEscape(meta.targetJsonPath) << "\"\n";
	out << "  },\n";
	out << "  \"recording\": {\n";
	out << "    \"user_id\": \"" << JsonEscape(meta.userId) << "\",\n";
	out << "    \"duration_sec\": " << std::fixed << std::setprecision(3) << outSummary.recordingDurationSec << "\n";
	out << "  },\n";
	out << "  \"frames\": [\n";

	for (size_t i = 0; i < rawFrames.size(); ++i) {
		const auto& rf = rawFrames[i];
		const double tsec = rf.timestampSec;
		const TargetPitchFrame target = targetTimeline.GetTargetPitchAt(tsec);

		const bool hasTarget = target.isVoiced && ValidHz(target.targetHz);
		const bool hasRaw = rf.voiced && ValidHz(rf.rawFreqHz);

		// DYNAMIC FALLBACK: Calculate processed pitch if real-time detection buffer missed it
		double actualFinalHz = rf.finalFreqHz;
		bool hasFinal = rf.finalVoiced && ValidHz(actualFinalHz);

		if (!hasFinal && hasRaw) {
			actualFinalHz = rf.rawFreqHz * std::pow(2.0, static_cast<double>(rf.totalAppliedSemitones) / 12.0);
			hasFinal = ValidHz(actualFinalHz);
		}

		if (hasTarget) {
			++targetMatches;
			++outSummary.validTargetFrames;
		}
		if (hasRaw) ++outSummary.validRawFrames;
		if (hasFinal) {
			++finalProcessedFrames;
			++outSummary.validFinalFrames;
		}

		double rawErrorCents = 0.0;
		double finalErrorCents = 0.0;
		double correctionEffectCents = 0.0;
		bool rawCmp = false;
		bool finalCmp = false;
		if (hasTarget && hasRaw) {
			rawErrorCents = 1200.0 * std::log2(rf.rawFreqHz / target.targetHz);
			rawCmp = true;
			++outSummary.validComparisons;
			sumRawAbs += std::abs(rawErrorCents);
			if (std::abs(rawErrorCents) <= tolVeryGood) ++rawInTune;
		}
		if (hasTarget && hasFinal) {
			finalErrorCents = 1200.0 * std::log2(actualFinalHz / target.targetHz);
			finalCmp = true;
			++outSummary.validFinalComparisons;
			sumFinalAbs += std::abs(finalErrorCents);
			if (std::abs(finalErrorCents) <= tolVeryGood) ++finalInTune;
		}
		if (rawCmp && finalCmp)
			correctionEffectCents = rawErrorCents - finalErrorCents;

		const double rawErrorHz = (hasTarget && hasRaw) ? (static_cast<double>(rf.rawFreqHz) - target.targetHz) : 0.0;
		const double finalErrorHz = (hasTarget && hasFinal) ? (actualFinalHz - target.targetHz) : 0.0;
		const double absRawErrorCents = std::abs(rawErrorCents);
		const double absFinalErrorCents = std::abs(finalErrorCents);
		double improvementCents = 0.0;
		bool improved = false;
		if (rawCmp && finalCmp) {
			improvementCents = absRawErrorCents - absFinalErrorCents;
			improved = absFinalErrorCents < absRawErrorCents;
			if (absFinalErrorCents < absRawErrorCents) ++improvedFrames;
			else if (absFinalErrorCents > absRawErrorCents) ++worsenedFrames;
			else ++unchangedFrames;
		}

		const bool hasCorrection = std::isfinite(rf.correctionSemitones);
		if (hasRaw && hasCorrection) {
			sumCorrSemi += rf.correctionSemitones;
			++corrCount;
		}

		const double statusError = finalCmp ? finalErrorCents : rawErrorCents;
		const std::string status = ClassifyStatus(hasTarget, hasRaw || hasFinal, statusError, tolVeryGood, tolAccept);
		const std::string rawStatus = ClassifyQuality(hasTarget, hasRaw, rawErrorCents, tolVeryGood, tolGood, tolAccept);
		const std::string finalStatus = ClassifyQuality(hasTarget, hasFinal, finalErrorCents, tolVeryGood, tolGood, tolAccept);
		if (status == "NO_TARGET") ++outSummary.noTargetFrames;
		else if (status == "UNVOICED") ++outSummary.unvoicedFrames;
		else if (status == "IN_TUNE") ++outSummary.inTuneFrames;
		else if (status == "SLIGHTLY_FLAT") ++outSummary.slightlyFlatFrames;
		else if (status == "FLAT") ++outSummary.flatFrames;
		else if (status == "SLIGHTLY_SHARP") ++outSummary.slightlySharpFrames;
		else if (status == "SHARP") ++outSummary.sharpFrames;

		const int targetMidi = target.midiNote > 0 ? target.midiNote : FrequencyToMidi(target.targetHz);
		const std::string targetNote = target.noteName[0] ? std::string(target.noteName) : MidiToNoteName(targetMidi);
		const int rawMidi = rf.midiNote > 0 ? rf.midiNote : FrequencyToMidi(rf.rawFreqHz);
		const std::string rawNote = rf.noteName[0] ? std::string(rf.noteName) : MidiToNoteName(rawMidi);

		const int finalMidi = hasFinal ? FrequencyToMidi(actualFinalHz) : 0;
		const std::string finalNote = hasFinal ? MidiToNoteName(finalMidi) : "---";

		double expectedHz = 0.0;
		bool hasExpected = false;
		if (hasRaw) {
			expectedHz = rf.rawFreqHz * std::pow(2.0, rf.correctionSemitones / 12.0);
			hasExpected = ValidHz(expectedHz);
		}
		const int expectedMidi = FrequencyToMidi(expectedHz);

		out << "    {\n";
		// MATCHED FORMAT: Both timestamp in seconds and milliseconds provided for absolute reference
		out << "      \"timestamp\": " << std::fixed << std::setprecision(3) << tsec << ",\n";
		out << "      \"timestamp_sec\": " << std::fixed << std::setprecision(3) << tsec << ",\n";
		out << "      \"timestamp_ms\": " << static_cast<int>(std::round(tsec * 1000.0)) << ",\n";

		out << "      \"reference\": { \"frequency_hz\": ";
		WriteOptionalNumber(out, target.targetHz, hasTarget);
		out << ", \"note\": \"" << JsonEscape(hasTarget ? targetNote : std::string("---"))
			<< "\", \"midi\": " << (hasTarget ? targetMidi : 0)
			<< ", \"voiced\": " << (hasTarget ? "true" : "false") << " },\n";

		out << "      \"user_raw\": { \"frequency_hz\": ";
		WriteOptionalNumber(out, rf.rawFreqHz, hasRaw);
		out << ", \"note\": \"" << JsonEscape(hasRaw ? rawNote : std::string("---"))
			<< "\", \"midi\": " << (hasRaw ? rawMidi : 0)
			<< ", \"confidence\": ";
		WriteOptionalNumber(out, rf.confidence, std::isfinite(rf.confidence), 3);
		out << ", \"voiced\": " << (hasRaw ? "true" : "false") << " },\n";

		out << "      \"correction\": { \"autotune_semitones\": ";
		WriteOptionalNumber(out, rf.correctionSemitones, hasCorrection, 3);
		out << ", \"autotune_cents\": ";
		WriteOptionalNumber(out, rf.correctionSemitones * 100.0, hasCorrection, 2);
		out << ", \"manual_semitones\": ";
		WriteOptionalNumber(out, rf.manualSemitones, std::isfinite(rf.manualSemitones), 3);
		out << ", \"total_applied_semitones\": ";
		WriteOptionalNumber(out, rf.totalAppliedSemitones, std::isfinite(rf.totalAppliedSemitones), 3);
		out << " },\n";

		out << "      \"expected_corrected\": { \"frequency_hz\": ";
		WriteOptionalNumber(out, expectedHz, hasExpected);
		out << ", \"midi\": " << (hasExpected ? expectedMidi : 0)
			<< ", \"note\": \"" << JsonEscape(hasExpected ? MidiToNoteName(expectedMidi) : std::string("---"))
			<< "\" },\n";

		out << "      \"final_processed_voice\": { \"frequency_hz\": ";
		WriteOptionalNumber(out, actualFinalHz, hasFinal);
		out << ", \"note\": \"" << JsonEscape(finalNote)
			<< "\", \"midi\": " << finalMidi
			<< ", \"confidence\": ";
		WriteOptionalNumber(out, hasFinal ? (rf.finalConfidence > 0.0f ? rf.finalConfidence : rf.confidence) : 0.0, hasFinal, 3);
		out << ", \"voiced\": " << (hasFinal ? "true" : "false") << " },\n";

		out << "      \"comparison\": { \"raw_error_hz\": ";
		WriteOptionalNumber(out, rawErrorHz, rawCmp);
		out << ", \"raw_error_cents\": ";
		WriteOptionalNumber(out, rawErrorCents, rawCmp);
		out << ", \"final_error_hz\": ";
		WriteOptionalNumber(out, finalErrorHz, finalCmp);
		out << ", \"final_error_cents\": ";
		WriteOptionalNumber(out, finalErrorCents, finalCmp);
		out << ", \"absolute_raw_error_cents\": ";
		WriteOptionalNumber(out, absRawErrorCents, rawCmp);
		out << ", \"absolute_final_error_cents\": ";
		WriteOptionalNumber(out, absFinalErrorCents, finalCmp);
		out << ", \"improvement_cents\": ";
		WriteOptionalNumber(out, improvementCents, rawCmp && finalCmp);
		out << ", \"improved\": " << ((rawCmp && finalCmp) ? (improved ? "true" : "false") : "null");
		out << ", \"correction_effect_cents\": ";
		WriteOptionalNumber(out, correctionEffectCents, rawCmp && finalCmp);
		out << ", \"status\": \"" << status << "\""
			<< ", \"raw_status\": \"" << rawStatus << "\""
			<< ", \"final_status\": \"" << finalStatus << "\" }\n";
		out << "    }" << (i + 1 < rawFrames.size() ? "," : "") << "\n";
	}

	outSummary.totalFrames = static_cast<int>(rawFrames.size());
	if (outSummary.validComparisons > 0) {
		outSummary.averageRawErrorCents = sumRawAbs / outSummary.validComparisons;
		outSummary.rawAccuracyPercent = 100.0 * static_cast<double>(rawInTune) / outSummary.validComparisons;
	}
	if (outSummary.validFinalComparisons > 0) {
		outSummary.averageFinalErrorCents = sumFinalAbs / outSummary.validFinalComparisons;
		outSummary.finalAccuracyPercent = 100.0 * static_cast<double>(finalInTune) / outSummary.validFinalComparisons;
	}
	if (corrCount > 0) {
		outSummary.averageCorrectionSemitones = sumCorrSemi / corrCount;
		outSummary.averageCorrectionCents = outSummary.averageCorrectionSemitones * 100.0;
	}

	outSummary.averageAbsRawErrorCents = outSummary.averageRawErrorCents;
	outSummary.averageAbsFinalErrorCents = outSummary.averageFinalErrorCents;
	outSummary.averageImprovementCents = outSummary.averageAbsRawErrorCents - outSummary.averageAbsFinalErrorCents;
	outSummary.improvementPercent = (outSummary.averageAbsRawErrorCents > 0.0)
		? (outSummary.averageImprovementCents / outSummary.averageAbsRawErrorCents) * 100.0
		: 0.0;
	outSummary.improvedFrames = improvedFrames;
	outSummary.worsenedFrames = worsenedFrames;
	outSummary.unchangedFrames = unchangedFrames;

	if (outSummary.validTargetFrames == 0) {
		outSummary.error = "No voiced target frames overlapped the recording window "
			"(recorded timestamps never aligned with the song's sung section).";
	}
	else if (outSummary.validComparisons == 0) {
		outSummary.error = "No valid comparisons: the microphone was not voiced at any timestamp that "
			"had a voiced target. Sing during the vocal section of the song.";
	}

	out << "  ],\n";
	out << "  \"summary\": {\n";
	out << "    \"total_frames\": " << outSummary.totalFrames << ",\n";
	out << "    \"valid_target_frames\": " << outSummary.validTargetFrames << ",\n";
	out << "    \"valid_raw_frames\": " << outSummary.validRawFrames << ",\n";
	out << "    \"valid_final_frames\": " << outSummary.validFinalFrames << ",\n";
	out << "    \"valid_comparisons\": " << outSummary.validComparisons << ",\n";
	out << "    \"valid_final_comparisons\": " << outSummary.validFinalComparisons << ",\n";
	out << "    \"raw_accuracy_percent\": " << std::fixed << std::setprecision(2) << outSummary.rawAccuracyPercent << ",\n";
	out << "    \"final_accuracy_percent\": " << outSummary.finalAccuracyPercent << ",\n";
	out << "    \"average_raw_error_cents\": " << outSummary.averageRawErrorCents << ",\n";
	out << "    \"average_final_error_cents\": " << outSummary.averageFinalErrorCents << ",\n";
	out << "    \"average_correction_semitones\": " << std::setprecision(3) << outSummary.averageCorrectionSemitones << ",\n";
	out << "    \"average_correction_cents\": " << std::setprecision(2) << outSummary.averageCorrectionCents << ",\n";
	out << "    \"average_absolute_raw_error_cents\": " << std::setprecision(2) << outSummary.averageAbsRawErrorCents << ",\n";
	out << "    \"average_absolute_final_error_cents\": " << outSummary.averageAbsFinalErrorCents << ",\n";
	out << "    \"average_improvement_cents\": " << outSummary.averageImprovementCents << ",\n";
	out << "    \"improvement_percent\": " << outSummary.improvementPercent << ",\n";
	out << "    \"improved_frames\": " << outSummary.improvedFrames << ",\n";
	out << "    \"worsened_frames\": " << outSummary.worsenedFrames << ",\n";
	out << "    \"unchanged_frames\": " << outSummary.unchangedFrames << ",\n";
	out << "    \"in_tune_frames\": " << outSummary.inTuneFrames << ",\n";
	out << "    \"slightly_flat_frames\": " << outSummary.slightlyFlatFrames << ",\n";
	out << "    \"flat_frames\": " << outSummary.flatFrames << ",\n";
	out << "    \"slightly_sharp_frames\": " << outSummary.slightlySharpFrames << ",\n";
	out << "    \"sharp_frames\": " << outSummary.sharpFrames << ",\n";
	out << "    \"unvoiced_frames\": " << outSummary.unvoicedFrames << ",\n";
	out << "    \"no_target_frames\": " << outSummary.noTargetFrames << ",\n";
	out << "    \"recording_duration_sec\": " << std::setprecision(3) << outSummary.recordingDurationSec << ",\n";
	out << "    \"error\": \"" << JsonEscape(outSummary.error) << "\"\n";
	out << "  }\n";
	out << "}\n";

	if (!outSummary.error.empty()) {
		std::cout << "[Comparison] INVALID REPORT (not written): " << outSummary.error << std::endl;
		return false;
	}

	std::ofstream file(outFilePath, std::ios::out | std::ios::trunc);
	if (!file.is_open()) {
		std::cout << "[Comparison] Failed to open output file: " << outFilePath << std::endl;
		outSummary.error = "Could not open the report file for writing: " + outFilePath;
		return false;
	}
	file << out.str();
	file.close();

	std::cout << "[Comparison] Report generated successfully with synchronized timestamps." << std::endl;
	return true;
}