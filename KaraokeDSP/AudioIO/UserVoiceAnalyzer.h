#ifndef USERVOICEANALYZER_H
#define USERVOICEANALYZER_H

#include "SongAnalyzer.h" // Reuse structure
#include <fstream>
#include <iostream>
#include <filesystem>

class UserVoiceAnalyzer {
public:
    static bool AnalyzeAndSaveUserVocals(
        const std::vector<float>& userAudioSamples,
        int sampleRate,
        const std::string& userId,
        const std::string& songName)
    {
        // 1. Process user audio using pitch detector logic
        auto userTimeline = SongAnalyzer::AnalyzeAudioBuffer(userAudioSamples, sampleRate);

        if (!userTimeline || userTimeline->GetFrameCount() == 0) {
            std::cerr << "[UserVoiceAnalyzer] No audio detected!" << std::endl;
            return false;
        }

        // 2. Setup folder path (e.g., d:/SwaraagyaSoftware_08_08/cache/user_reports/)
        std::string reportDir = "d:/SwaraagyaSoftware_08_08/cache/user_reports/" + userId + "/";
        std::filesystem::create_directories(reportDir);

        std::string reportFilePath = reportDir + songName + "_vocal_report.json";

        // 3. Export Vocal Analysis Report
        std::ofstream out(reportFilePath);
        if (!out.is_open()) return false;

        auto frames = userTimeline->GetFrames();
        static const char* noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

        out << "{\n";
        out << "  \"userId\": \"" << userId << "\",\n";
        out << "  \"songName\": \"" << songName << "\",\n";
        out << "  \"vocalFrames\": [\n";

        for (size_t i = 0; i < frames.size(); ++i) {
            const auto& frame = frames[i];
            std::string noteName = "---";
            if (frame.isVoiced && frame.midiNote > 0) {
                int octave = (frame.midiNote / 12) - 1;
                noteName = std::string(noteNames[frame.midiNote % 12]) + std::to_string(octave);
            }

            out << "    {\n";
            out << "      \"timestamp\": " << frame.timestamp << ",\n";
            out << "      \"sungFrequencyHz\": " << frame.targetHz << ",\n";
            out << "      \"confidence\": " << frame.confidence << ",\n";
            out << "      \"sungNote\": \"" << noteName << "\"\n";
            out << "    }" << (i < frames.size() - 1 ? "," : "") << "\n";
        }

        out << "  ]\n";
        out << "}\n";
        out.close();

        std::cout << "[UserVoiceAnalyzer] Vocal report saved at: " << reportFilePath << std::endl;
        return true;
    }
};

#endif
