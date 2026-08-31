#ifndef SONGANALYZER_H
#define SONGANALYZER_H

#include "TargetPitchTimeline.h"
#include "../PitchDetector/PitchDetector.h"
#include <vector>
#include <string>
#include <iostream>
#include <memory>
#include <cmath>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdint>
#include <iomanip>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <cstdio>

class SongAnalyzer {
public:
    static std::unique_ptr<TargetPitchTimeline> AnalyzeAudioBuffer(
        const std::vector<float>& audioSamples,
        int sampleRate,
        int frameSize = 2048,
        int hopSize = 441)
    {
        auto timeline = std::make_unique<TargetPitchTimeline>();
        if (audioSamples.empty() || sampleRate <= 0) {
            std::cout << "[TargetDebug] AnalyzeAudioBuffer: EMPTY input (samples=" << audioSamples.size()
                << ", sr=" << sampleRate << "); no target can be produced." << std::endl;
            return timeline;
        }

        // P0/[SongDebug]: characterize the input so a silent/empty buffer is diagnosable rather than
        // silently producing zero voiced frames downstream.
        double sumSq = 0.0;
        float peak = 0.0f;
        for (float s : audioSamples) { sumSq += static_cast<double>(s) * s; peak = std::max(peak, std::fabs(s)); }
        double rms = std::sqrt(sumSq / static_cast<double>(audioSamples.size()));
        std::cout << "[SongDebug] AnalyzeAudioBuffer: samples=" << audioSamples.size()
            << " sr=" << sampleRate << " durSec=" << (static_cast<double>(audioSamples.size()) / sampleRate)
            << " rms=" << rms << " peak=" << peak << std::endl;
        if (rms < 1e-4) {
            std::cout << "[TargetDebug] WARNING: input audio is effectively silent (rms=" << rms
                << "); target will have no voiced frames." << std::endl;
        }

        PitchDetector detector;
        size_t totalSamples = audioSamples.size();
        std::vector<float> frameBuffer(frameSize, 0.0f);

        for (size_t pos = 0; pos + frameSize <= totalSamples; pos += hopSize) {
            for (size_t i = 0; i < frameSize; ++i) {
                frameBuffer[i] = audioSamples[pos + i];
            }

            float confidence = 0.0f;
            float pitchHz = detector.Process(frameBuffer.data(), frameSize, sampleRate, confidence);
            double timeSeconds = static_cast<double>(pos) / sampleRate;

            if (confidence < 0.45f || pitchHz < 60.0f || pitchHz > 1200.0f) {
                pitchHz = 0.0f;
            }

            TargetPitchFrame frame;
            frame.timeStartSec = timeSeconds;
            frame.timeEndSec = timeSeconds + (static_cast<double>(frameSize) / sampleRate);
            frame.timestamp = timeSeconds;
            frame.targetHz = pitchHz;
            frame.targetF0 = pitchHz;
            frame.confidence = confidence;
            frame.isVoiced = (pitchHz > 0.0f);

            if (pitchHz > 0.0f) {
                frame.midiNote = static_cast<int16_t>(std::round(69.0 + 12.0 * std::log2(pitchHz / 440.0)));
            }
            else {
                frame.midiNote = 0;
            }

            timeline->AddPitchFrame(frame);
        }

        size_t voiced = 0;
        for (const auto& f : timeline->GetFramesRef()) if (f.isVoiced) ++voiced;
        std::cout << "[SongDebug] AnalyzeAudioBuffer produced frames=" << timeline->GetFrameCount()
            << " voiced=" << voiced << std::endl;

        return timeline;
    }

    static bool LoadOrAnalyzeSong(
        TargetPitchTimeline& outTimeline,
        const std::string& songFilePath,
        const std::vector<float>& audioSamples,
        int sampleRate,
        bool vocalsOnly = false)
    {
        std::string cacheFilePath = GetCacheFilePath(songFilePath, vocalsOnly);
        std::cout << "[SongAnalyzer] Target Cache file path: " << cacheFilePath << std::endl;

        // Pehle check karein agar JSON file already bani hui hai
        if (LoadFromJSONCache(outTimeline, cacheFilePath)) {
            std::cout << "[SongAnalyzer] Existing JSON Cache Loaded Successfully!" << std::endl;
            return true;
        }

        std::cout << "[SongAnalyzer] Analyzing audio samples to generate new JSON cache..." << std::endl;
        auto tempTimeline = AnalyzeAudioBuffer(audioSamples, sampleRate);

        if (tempTimeline) {
            size_t frameCount = tempTimeline->GetFrameCount();
            size_t voicedCount = 0;
            for (const auto& frame : tempTimeline->GetFrames()) {
                if (frame.isVoiced) voicedCount++;
            }

            std::cout << "[SongAnalyzer] Analysis finished - Total Frames: " << frameCount
                << ", Voiced Frames: " << voicedCount << std::endl;

            if (voicedCount == 0) {
                // P0: do not cache or return an all-unvoiced target. This is almost always a silent/instrumental
                // input (e.g. a demucs vocal stem that came out empty). Returning false lets the caller retry
                // with full-song audio instead of persisting a useless target that every report would reject.
                std::cout << "[TargetDebug] Analysis produced 0 voiced frames; NOT caching and returning false "
                    "(caller may retry with full-song audio). Source likely silent/instrumental." << std::endl;
                return false;
            }

            // Post-process timeline to reduce octave/harmonic errors and spiky noise
            try {
                PostProcessTimeline(*tempTimeline, audioSamples, sampleRate);
            }
            catch (...) {
                std::cerr << "[SongAnalyzer] Warning: PostProcessTimeline failed, continuing without post-processing" << std::endl;
            }

            // Always attempt to save the JSON cache (even if frameCount == 0)
            bool savedPrimary = SaveToJSONCache(*tempTimeline, cacheFilePath);
            if (savedPrimary) {
                std::cout << "----------------------------------------------------" << std::endl;
                std::cout << "[SUCCESS] Pitch Cache File CREATED at:\n>> " << cacheFilePath << std::endl;
                std::cout << "----------------------------------------------------" << std::endl;
            }
            else {
                std::cerr << "[ERROR] Could not save JSON file to disk: " << cacheFilePath << std::endl;
            }

            // Additionally, when analyzing vocals-only, also save a copy next to the source song/vocals file
            if (vocalsOnly) {
                try {
                    std::filesystem::path srcPath(cacheFilePath);
                    std::filesystem::path songPath(songFilePath);
                    std::string altDir = songPath.parent_path().string();
                    if (!altDir.empty()) {
                        std::filesystem::create_directories(altDir);
                        std::filesystem::path altPath = std::filesystem::path(altDir) / srcPath.filename();
                        // If primary save succeeded, copy; otherwise attempt to write directly
                        if (savedPrimary) {
                            std::error_code ec;
                            std::filesystem::copy_file(srcPath, altPath, std::filesystem::copy_options::overwrite_existing, ec);
                            if (!ec) {
                                std::cout << "[SongAnalyzer] Also saved copy of JSON near song: " << altPath.string() << std::endl;
                            }
                            else {
                                std::cout << "[SongAnalyzer] Could not copy JSON to song folder: " << ec.message() << std::endl;
                            }
                        }
                        else {
                            // Try saving directly to altPath
                            if (SaveToJSONCache(*tempTimeline, altPath.string())) {
                                std::cout << "[SongAnalyzer] Saved JSON beside song at: " << altPath.string() << std::endl;
                            }
                        }
                    }
                }
                catch (const std::exception& e) {
                    std::cerr << "[SongAnalyzer] Error while copying JSON to song folder: " << e.what() << std::endl;
                }
            }

            outTimeline.Clear();
            for (const auto& tempFrame : tempTimeline->GetFrames()) {
                outTimeline.AddPitchFrame(tempFrame);
            }
            return true;
        }

        return false;
    }

    // Load an existing SongAnalyzer cache JSON without analyzing or writing a new file.
    static bool TryLoadExistingCache(
        TargetPitchTimeline& outTimeline,
        const std::string& songFilePath,
        std::string& loadedPath)
    {
        loadedPath.clear();
        std::vector<std::string> candidates;
        candidates.push_back(GetCacheFilePath(songFilePath, true));
        candidates.push_back(GetCacheFilePath(songFilePath, false));

        try {
            const std::string cacheDir = GetCacheDirectory();
            const std::string stem = std::filesystem::path(songFilePath).stem().string();
            std::vector<std::filesystem::path> extras;
            for (const auto& entry : std::filesystem::directory_iterator(cacheDir)) {
                if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
                extras.push_back(entry.path());
            }
            std::sort(extras.begin(), extras.end(), [](const std::filesystem::path& a, const std::filesystem::path& b) {
                std::error_code ec1, ec2;
                auto ta = std::filesystem::last_write_time(a, ec1);
                auto tb = std::filesystem::last_write_time(b, ec2);
                return ta > tb;
                });
            for (const auto& p : extras) {
                const std::string name = p.filename().string();
                if (!stem.empty() && name.find(stem) != std::string::npos)
                    candidates.push_back(p.string());
            }
            // NOTE: name-agnostic fallback. Vocal-stem caches (e.g. "vocals_vocals_<hash>.json") do NOT contain
            // the original song's filename stem, so this blind append is what makes them loadable at all.
            // LoadFromJSONCache now rejects zero-voiced caches, and entries are newest-first, so the freshest
            // healthy target wins. [TargetDebug] below logs exactly which file was used to spot cross-song matches.
            if (!extras.empty()) {
                std::cout << "[TargetDebug] TryLoadExistingCache: " << extras.size()
                    << " cache file(s) in dir; stem=\"" << stem << "\" (name-agnostic fallback enabled)" << std::endl;
            }
            for (const auto& p : extras)
                candidates.push_back(p.string());
        }
        catch (const std::exception& e) {
            std::cerr << "[SongAnalyzer] Cache scan error: " << e.what() << std::endl;
        }

        std::vector<std::string> unique;
        for (const auto& c : candidates) {
            if (std::find(unique.begin(), unique.end(), c) == unique.end())
                unique.push_back(c);
        }
        for (const auto& c : unique) {
            if (LoadFromJSONCache(outTimeline, c)) {
                loadedPath = c;
                std::cout << "[SongAnalyzer] Using existing target JSON (not regenerated): " << loadedPath << std::endl;
                return true;
            }
        }
        return false;
    }

private:
    // Simple Goertzel/DFT power measurement at a single frequency over a window
    static double MeasurePowerAtFreq(const std::vector<float>& samples, int sampleRate, int centerIndex, int windowSize, double freq) {
        if (samples.empty() || windowSize <= 0) return 0.0;
        int half = windowSize / 2;
        int start = std::max(0, centerIndex - half);
        int end = std::min((int)samples.size(), centerIndex + half);
        int N = end - start;
        if (N <= 0) return 0.0;

        double real = 0.0, imag = 0.0;
        const double twoPiF = 2.0 * M_PI * freq / static_cast<double>(sampleRate);
        for (int n = 0; n < N; ++n) {
            double s = samples[start + n];
            double angle = twoPiF * n;
            real += s * std::cos(angle);
            imag -= s * std::sin(angle);
        }
        return real * real + imag * imag;
    }

    static void ApplyMedianSmoothing(TargetPitchTimeline& timeline, int windowSize = 7) {
        if (windowSize <= 1) return;
        auto frames = timeline.GetFrames();
        size_t N = frames.size();
        std::vector<float> out(N);
        int half = windowSize / 2;
        for (size_t i = 0; i < N; ++i) {
            std::vector<float> vals;
            for (int k = -half; k <= half; ++k) {
                int idx = (int)i + k;
                if (idx >= 0 && idx < (int)N) {
                    if (frames[idx].isVoiced && frames[idx].targetHz > 0.0f)
                        vals.push_back(frames[idx].targetHz);
                }
            }
            if (!vals.empty()) {
                std::sort(vals.begin(), vals.end());
                out[i] = vals[vals.size() / 2];
            }
            else out[i] = frames[i].targetHz;
        }
        // apply back
        for (size_t i = 0; i < N; ++i) {
            if (frames[i].isVoiced && out[i] > 0.0f) {
                frames[i].targetHz = out[i];
                frames[i].targetF0 = out[i];
                if (frames[i].targetHz > 0.0f) {
                    frames[i].midiNote = static_cast<int16_t>(std::round(69.0 + 12.0 * std::log2(frames[i].targetHz / 440.0)));
                    int octave = (frames[i].midiNote / 12) - 1;
                    std::string noteNames[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
                    std::snprintf(frames[i].noteName, sizeof(frames[i].noteName), "%s%d", noteNames[frames[i].midiNote % 12].c_str(), octave);
                }
            }
        }
        timeline.SetFrames(frames);
    }

    static void ApplyOctaveCorrection(TargetPitchTimeline& timeline, const std::vector<float>& audioSamples, int sampleRate) {
        auto frames = timeline.GetFrames();
        size_t N = frames.size();
        if (audioSamples.empty() || sampleRate <= 0) return;
        int win = 4096; // window for DFT
        for (size_t i = 0; i < N; ++i) {
            auto& f = frames[i];
            if (!f.isVoiced || f.targetHz <= 0.0f) continue;
            int centerIndex = static_cast<int>(std::round(f.timestamp * sampleRate));
            double pF = MeasurePowerAtFreq(audioSamples, sampleRate, centerIndex, win, f.targetHz);
            double pHalf = MeasurePowerAtFreq(audioSamples, sampleRate, centerIndex, win, f.targetHz * 0.5);
            // If half-octave (fundamental) is stronger than current by some factor, prefer half
            if (pHalf > pF * 1.5 && f.targetHz * 0.5 >= 60.0) {
                f.targetHz = static_cast<float>(f.targetHz * 0.5);
                f.targetF0 = f.targetHz;
                f.midiNote = static_cast<int16_t>(std::round(69.0 + 12.0 * std::log2(f.targetHz / 440.0)));
                int octave = (f.midiNote / 12) - 1;
                std::string noteNames[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
                std::snprintf(f.noteName, sizeof(f.noteName), "%s%d", noteNames[f.midiNote % 12].c_str(), octave);
            }
        }
        timeline.SetFrames(frames);
    }

    static void PostProcessTimeline(TargetPitchTimeline& timeline, const std::vector<float>& audioSamples, int sampleRate) {
        // 1) Median smoothing to remove spikes
        ApplyMedianSmoothing(timeline, 7);
        // 2) Octave correction using spectral checks
        ApplyOctaveCorrection(timeline, audioSamples, sampleRate);
    }
    static std::string GenerateHash(const std::string& input) {
        uint32_t hash = 5381;
        for (char c : input) {
            hash = ((hash << 5) + hash) + static_cast<uint32_t>(c);
        }
        std::stringstream ss;
        ss << std::hex << hash;
        return ss.str().substr(0, 8);
    }

    static std::string GetCacheDirectory() {
        // Aapke Project Source folder ka explicit location
        std::string cacheDir = "D:/SwaraagyaSoftware_08_08/cache/pitch_timelines/";
        try {
            std::filesystem::create_directories(cacheDir);
        }
        catch (const std::exception& e) {
            std::cerr << "[SongAnalyzer] Directory creation error: " << e.what() << std::endl;
        }
        return cacheDir;
    }

    static std::string GetCacheFilePath(const std::string& songFilePath, bool vocalsOnly = false) {
        std::filesystem::path path(songFilePath);
        std::string filename = path.stem().string();
        std::string hash = GenerateHash(songFilePath);
        std::string suffix = vocalsOnly ? "_vocals" : "";
        return GetCacheDirectory() + filename + suffix + "_" + hash + ".json";
    }

    static bool SaveToJSONCache(const TargetPitchTimeline& timeline, const std::string& cacheFilePath) {
        std::string finalPath = cacheFilePath;
        std::cout << "[SongAnalyzer] Attempting to write JSON to: " << finalPath << std::endl;

        // If a cache file already exists, create a uniquely named copy instead of overwriting
        try {
            std::filesystem::path p(cacheFilePath);
            if (std::filesystem::exists(p)) {
                // append timestamp to filename: stem_YYYYMMDD_HHMMSS.ext
                std::time_t t = std::time(nullptr);
                std::tm tm;
#ifdef _MSC_VER
                localtime_s(&tm, &t);
#else
                localtime_r(&t, &tm);
#endif
                char buf[32];
                std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
                std::string stem = p.stem().string();
                std::string ext = p.extension().string();
                std::filesystem::path alt = p.parent_path() / (stem + std::string("_") + buf + ext);
                finalPath = alt.string();
                std::cout << "[SongAnalyzer] Cache exists, writing to new file: " << finalPath << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[SongAnalyzer] Warning: could not check existing cache file: " << e.what() << std::endl;
        }

        std::ofstream out(finalPath, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            int err = errno;
            char errBuf[256] = { 0 };
#ifdef _MSC_VER
            strerror_s(errBuf, sizeof(errBuf), err);
#else
#if (_POSIX_C_SOURCE >= 200112L) && !defined(__APPLE__)
            strerror_r(err, errBuf, sizeof(errBuf));
#else
            const char* s = std::strerror(err);
            if (s) strncpy(errBuf, s, sizeof(errBuf) - 1);
#endif
#endif
            std::cerr << "[SongAnalyzer] Unable to open stream for file: " << finalPath
                << " (errno=" << err << ") " << errBuf << std::endl;
            return false;
        }

        auto frames = timeline.GetFrames();
        static const char* noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

        out << "[\n";
        for (size_t i = 0; i < frames.size(); ++i) {
            const auto& frame = frames[i];
            std::string noteName = "---";
            if (frame.isVoiced && frame.midiNote > 0) {
                int octave = (frame.midiNote / 12) - 1;
                noteName = std::string(noteNames[frame.midiNote % 12]) + std::to_string(octave);
            }

            out << "  {\"timestamp\": " << std::fixed << std::setprecision(3) << frame.timestamp
                << ", \"frequencyHz\": " << std::fixed << std::setprecision(2) << frame.targetHz
                << ", \"confidence\": " << std::fixed << std::setprecision(2) << frame.confidence
                << ", \"voiced\": " << (frame.isVoiced ? "true" : "false")
                << ", \"midiNote\": " << frame.midiNote
                << ", \"noteName\": \"" << noteName << "\"}";

            if (i < frames.size() - 1) out << ",";
            out << "\n";
        }
        out << "]\n";
        out.close();
        std::cout << "[SongAnalyzer] JSON write completed: " << cacheFilePath << " (" << std::filesystem::file_size(cacheFilePath) << " bytes)" << std::endl;
        return true;
    }

    static bool LoadFromJSONCache(TargetPitchTimeline& outTimeline, const std::string& cacheFilePath) {
        std::ifstream in(cacheFilePath);
        if (!in.is_open()) return false;

        outTimeline.Clear();

        std::string line;
        while (std::getline(in, line)) {
            if (line.find("timestamp") == std::string::npos) continue;

            TargetPitchFrame frame;
            try {
                auto extractValue = [&](const std::string& key) -> std::string {
                    size_t pos = line.find("\"" + key + "\":");
                    if (pos == std::string::npos) return "";
                    size_t start = line.find_first_not_of(" :", pos + key.length() + 2);
                    size_t end = line.find_first_of(",}", start);
                    return line.substr(start, end - start);
                    };

                std::string tsStr = extractValue("timestamp");
                std::string freqStr = extractValue("frequencyHz");
                std::string confStr = extractValue("confidence");
                std::string voicedStr = extractValue("voiced");
                std::string midiStr = extractValue("midiNote");

                if (!tsStr.empty()) frame.timestamp = std::stod(tsStr);
                if (!freqStr.empty()) frame.targetHz = std::stof(freqStr);
                if (!confStr.empty()) frame.confidence = std::stof(confStr);
                frame.isVoiced = (voicedStr.find("true") != std::string::npos);

                frame.timeStartSec = frame.timestamp;
                frame.timeEndSec = frame.timestamp + 0.023;
                frame.targetF0 = frame.targetHz;

                if (!midiStr.empty()) {
                    try { frame.midiNote = static_cast<int16_t>(std::stoi(midiStr)); }
                    catch (...) { frame.midiNote = 0; }
                }
                if (frame.midiNote == 0 && frame.isVoiced && frame.targetHz > 0.0f) {
                    frame.midiNote = static_cast<int16_t>(std::round(69.0 + 12.0 * std::log2(frame.targetHz / 440.0)));
                }

                if (frame.midiNote > 0) {
                    static const char* noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
                    int octave = (frame.midiNote / 12) - 1;
                    std::snprintf(frame.noteName, sizeof(frame.noteName), "%s%d", noteNames[frame.midiNote % 12], octave);
                }
                else {
                    std::snprintf(frame.noteName, sizeof(frame.noteName), "---");
                }

                outTimeline.AddPitchFrame(frame);
            }
            catch (...) {
                continue;
            }
        }
        in.close();

        if (outTimeline.GetFrameCount() > 0) {
            size_t frameCount = outTimeline.GetFrameCount();
            size_t voicedCount = 0;
            for (const auto& f : outTimeline.GetFramesRef()) {
                if (f.isVoiced && f.targetHz > 0.0f) ++voicedCount;
            }
            std::cout << "[TargetDebug] Cache load: " << cacheFilePath << " frames=" << frameCount
                << " voiced=" << voicedCount << std::endl;
            if (voicedCount == 0) {
                // P0: a target with zero voiced frames is unusable; reject so it self-heals via regeneration
                // instead of silently producing valid_target_frames=0 in every comparison report.
                std::cout << "[TargetDebug] REJECTED cache (0 voiced frames), forcing regeneration: "
                    << cacheFilePath << std::endl;
                outTimeline.Clear();
                return false;
            }
            std::cout << "[SongAnalyzer] Loaded " << frameCount << " frames (" << voicedCount
                << " voiced) from Cache JSON!" << std::endl;
            return true;
        }

        return false;
    }
};

#endif // SONGANALYZER_H