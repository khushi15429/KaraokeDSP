#include "AudioStream.h"
#include "../PitchDetector/PitchDetector.h"
#include "VoiceComparison.h"
#include <iostream>
#include <QMediaDevices>
#include <cmath>
#include <cstdio>
#include <thread>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <algorithm>

// --- HELPER CLASSES FOR STUDIO-GRADE SURILA VOICE EFFECTS ---

// Dynamic De-Esser: 'S', 'T', 'Sh' waali harsh frequencies ko automatically damp karta hai
class StudioDeEsser {
private:
    float m_prevSample = 0.0f;
public:
    float Process(float sample) {
        float highFreq = sample - m_prevSample;
        m_prevSample = sample;

        float energy = std::abs(highFreq);
        float gainReduction = 1.0f;

        // Harsh peak detection threshold
        if (energy > 0.12f) {
            gainReduction = 0.65f; // Automatically attenuate harsh sibilance
        }
        return sample * gainReduction;
    }
};

// Global instance for the de-esser (stateless enough for a single-AudioStream app)
static StudioDeEsser g_deEsser;
// NOTE: the slapback delay is now ONLY m_slapbackDelay (a real member, SlapbackDelay.h).
// A second, duplicate delay instance used to run here as well (StudioSlapbackDelay /
// g_slapbackDelay) -- removed because stacking two delay stages back-to-back smears
// the vocal (audibly "muddy"/less clean) rather than adding clean depth.


// --- AUDIOSTREAM IMPLEMENTATION ---

AudioStream::AudioStream(QObject* parent)
    : QObject(parent), m_sampleRate(44100), m_bufferSize(512), m_currentPitchHz(0.0f), m_currentConfidence(0.0f),
    m_vocalReverb(m_sampleRate, 0.75f, 0.5f) {
    m_audioBuffer = new AudioBuffer(65536);
    m_processingTimer = new QTimer(this);
    m_processingTimer->setTimerType(Qt::PreciseTimer);
    connect(m_processingTimer, &QTimer::timeout, [this]() { processAudio(); });

    m_pitchShifter.Initialize(m_sampleRate, 1);
    m_vocalEQ.Initialize(m_sampleRate);
    m_vocalDoubler.Init(m_sampleRate);
    m_slapbackDelay.Init(m_sampleRate, 130.0f); // 130ms delay

    // DEFAULT VOLUME & FX SETTINGS FOR SWEET/SURILA VOCAL
    m_micVolume = 1.0f;
    m_speakerVolume = 1.0f;
    m_earphoneVolume = 1.0f;

    // Balanced Reverb Space (14% Mix)
    m_reverbMix = 0.22f;

    // Smooth Vocal Compressor (Gentle Glue)
    m_compressor.SetThreshold(-18.0f);
    m_compressor.SetRatio(3.0f);
    m_compressor.SetAttack(12.0f);
    m_compressor.SetRelease(100.0f);
    m_compressor.SetMakeupGain(3.5f);

    // Warm & Sweet Harmonic Exciter
    m_harmonicEnhancer.SetExciterMix(0.15f);
    m_harmonicEnhancer.SetSaturationDrive(1.04f);

    std::cout << "[Studio Audio Engine] Initialized with De-Esser, Slapback Delay & Warmth EQ at " << m_sampleRate << " Hz" << std::endl;
}

void AudioStream::SetComparisonReportCallback(ComparisonReportCallback cb) {
    m_comparisonReportCallback = std::move(cb);
}

void AudioStream::SetTargetJsonPath(const std::string& path) {
    m_targetJsonPath = path;
}

void AudioStream::SetSongPath(const std::string& path) {
    m_songPath = path;
}

void AudioStream::fillNoteFromHz(float hz, int16_t& midi, char* noteName, size_t noteNameSize) const {
    midi = 0;
    if (noteName && noteNameSize > 0) noteName[0] = '\0';
    if (!(hz > 0.0f) || !std::isfinite(hz) || !noteName || noteNameSize == 0) return;
    int midiI = static_cast<int>(std::round(69.0 + 12.0 * std::log2(hz / 440.0)));
    midi = static_cast<int16_t>(midiI);
    int octave = (midiI / 12) - 1;
    static const char* noteNames[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    std::snprintf(noteName, noteNameSize, "%s%d", noteNames[((midiI % 12) + 12) % 12], octave);
}

void AudioStream::StartMicCapture() {
    if (m_micCaptureEnabled) return;
    if (!m_isRunning || !m_inputDevice) {
        std::cout << "[AudioStream] Cannot start mic capture: audio not running or input device missing" << std::endl;
        return;
    }
    m_micCaptureEnabled = true;
    m_micSamplesForAnalysis.clear();
    {
        std::lock_guard<std::mutex> lk(m_liveFramesMutex);
        m_liveRawFrames.clear();
        m_liveRawFrames.reserve(18000);
    }
    m_pitchInputHistory.clear();
    m_processedPitchHistory.clear();
    m_smoothedCorrectionSemitones = 0.0f;
    m_currentCorrectionSemitones = 0.0f;
    const size_t reserveSamples = static_cast<size_t>(std::max(m_sampleRate, 1)) * 60 * 5;
    if (m_micSamplesForAnalysis.capacity() < reserveSamples)
        m_micSamplesForAnalysis.reserve(reserveSamples);
    std::cout << "[AudioStream] Mic capture started" << std::endl;
}

void AudioStream::StopMicCaptureAndAnalyze(const std::string& userId, const std::string& songNamePrefix) {
    if (!m_micCaptureEnabled) return;
    m_micCaptureEnabled = false;
    std::vector<LiveRawFrame> frames = GetAndClearLiveRawFrames();
    m_micSamplesForAnalysis.clear();
    std::cout << "[AudioStream] Mic capture stopped. Live frames collected: " << frames.size() << std::endl;

    const TargetPitchTimeline* timeline = m_targetTimeline;
    ComparisonReportMeta meta;
    meta.userId = userId.empty() ? "localUser" : userId;
    meta.songTitle = songNamePrefix;
    meta.songPath = m_songPath;
    meta.targetJsonPath = m_targetJsonPath;
    auto callback = m_comparisonReportCallback;

    if (!timeline) {
        std::cout << "[Comparison] No target timeline loaded; report not generated" << std::endl;
        if (callback) callback(false, std::string(), "No target melody is loaded for this song.");
        return;
    }

    auto timelineSnapshot = std::make_shared<TargetPitchTimeline>();
    timelineSnapshot->SetFrames(timeline->GetFrames());

    size_t voicedTargetCount = 0;
    for (const auto& tf : timelineSnapshot->GetFramesRef()) {
        if (tf.isVoiced && tf.targetHz > 0.0f && std::isfinite(tf.targetHz)) ++voicedTargetCount;
    }
    std::cout << "[TargetDebug] Snapshot for report: frames=" << timelineSnapshot->GetFrameCount()
        << " voiced=" << voicedTargetCount << std::endl;
    if (voicedTargetCount == 0) {
        std::cout << "[Comparison] ERROR: target timeline has 0 voiced frames; no valid comparison possible. "
            "Report not generated (would be misleading)." << std::endl;
        if (callback) callback(false, std::string(),
            "The target melody has no voiced frames; regenerate the target from the song's vocal audio.");
        return;
    }

    std::thread([frames = std::move(frames), timelineSnapshot, meta, songNamePrefix, callback]() mutable {
        try {
            std::time_t t = std::time(nullptr);
            char buf[32] = { 0 };
#ifdef _MSC_VER
            std::tm tm{};
            localtime_s(&tm, &t);
            std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
#else
            std::tm tm{};
            localtime_r(&t, &tm);
            std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
#endif
            std::string reportDir = "D:/SwaraagyaSoftware_08_08/cache/user_reports/" + meta.userId + "/";
            std::filesystem::create_directories(reportDir);
            std::string songBase = songNamePrefix.empty() ? "song" : songNamePrefix;
            std::string outPath = reportDir + songBase + "_voice_comparison_" + buf + ".json";

            ComparisonSummary summary;
            bool ok = VoiceComparison::GenerateComparisonReport(*timelineSnapshot, frames, meta, outPath, summary);
            if (callback) {
                if (ok) callback(true, outPath, std::string());
                else callback(false, std::string(),
                    summary.error.empty() ? std::string("Report generation failed.") : summary.error);
            }
        }
        catch (const std::exception& e) {
            std::cout << "[Comparison] Exception during report generation: " << e.what() << std::endl;
            if (callback) callback(false, std::string(), std::string("Exception during report generation: ") + e.what());
        }
        }).detach();
}

std::vector<AudioStream::LiveRawFrame> AudioStream::GetAndClearLiveRawFrames() {
    std::lock_guard<std::mutex> lk(m_liveFramesMutex);
    std::vector<LiveRawFrame> copy = m_liveRawFrames;
    m_liveRawFrames.clear();
    return copy;
}

AudioStream::AudioStream(int sampleRate, int bufferSize, QObject* parent)
    : QObject(parent), m_sampleRate(sampleRate), m_bufferSize(bufferSize), m_currentPitchHz(0.0f), m_currentConfidence(0.0f),
    m_vocalReverb(m_sampleRate, 0.75f, 0.5f) {
    m_audioBuffer = new AudioBuffer(65536);
    m_processingTimer = new QTimer(this);
    m_processingTimer->setTimerType(Qt::PreciseTimer);
    connect(m_processingTimer, &QTimer::timeout, [this]() { processAudio(); });
    m_pitchShifter.Initialize(m_sampleRate, 1);
    m_vocalEQ.Initialize(m_sampleRate);
    m_vocalDoubler.Init(m_sampleRate);
    m_slapbackDelay.Init(m_sampleRate, 130.0f);

    m_micVolume = 1.0f;
    m_speakerVolume = 1.0f;
    m_earphoneVolume = 1.0f;
    m_reverbMix = 0.22f;

    m_compressor.SetThreshold(-18.0f);
    m_compressor.SetRatio(3.0f);
    m_compressor.SetAttack(12.0f);
    m_compressor.SetRelease(100.0f);
    m_compressor.SetMakeupGain(3.5f);

    m_harmonicEnhancer.SetExciterMix(0.15f);
    m_harmonicEnhancer.SetSaturationDrive(1.04f);

    std::cout << "[Reverb] Initialized with sample rate: " << m_sampleRate << " Hz" << std::endl;
}

AudioStream::~AudioStream() {
    cleanup();
    delete m_audioBuffer;
}

bool AudioStream::Open() {
    return Open(-1, -1);
}

bool AudioStream::Open(int inputDeviceIndex, int outputDeviceIndex) {
    if (!initializeAudioDevices(inputDeviceIndex, outputDeviceIndex)) {
        return false;
    }
    return true;
}

bool AudioStream::Start() {
    if (m_isRunning) return true;
    if (!m_audioSource || !m_audioSink) return false;

    m_inputDevice = m_audioSource->start();
    m_outputDevice = m_audioSink->start();

    if (!m_inputDevice || !m_outputDevice) return false;

    m_playbackClockFrames = 0;
    m_framesProcessed = 0;
    m_pitchInputHistory.clear();
    m_processedPitchHistory.clear();
    m_smoothedCorrectionSemitones = 0.0f;
    m_currentCorrectionSemitones = 0.0f;
    m_logCounter = 0;

    m_processingTimer->start(10);
    m_isRunning = true;
    EnableReverb(true);
    EnableCompressor(true);

    return true;
}

void AudioStream::Stop() {
    if (!m_isRunning) return;

    m_processingTimer->stop();
    m_isRunning = false;

    if (m_audioSource) m_audioSource->stop();
    if (m_audioSink) m_audioSink->stop();

    m_audioBuffer->Clear();
}

bool AudioStream::Initialize(int sampleRate, int bufferSize) {
    m_sampleRate = sampleRate;
    m_bufferSize = bufferSize;
    m_vocalDoubler.Init(m_sampleRate);
    m_slapbackDelay.Init(m_sampleRate, 130.0f);
    return true;
}

void AudioStream::ProcessBuffer(const float* buffer, int numFrames) {
    if (!buffer || numFrames <= 0) return;
    PitchDetector detector;
    float confidence = 0.0f;
    float pitchHz = detector.Process(buffer, numFrames, m_sampleRate, confidence);

    m_currentPitchHz = pitchHz;
    m_currentConfidence = confidence;
}

void AudioStream::SetMicVolume(float vol) { m_micVolume = vol; }
void AudioStream::SetSpeakerVolume(float vol) { m_speakerVolume = vol; }
void AudioStream::SetEarphoneVolume(float vol) { m_earphoneVolume = vol; }
void AudioStream::SetPitchShift(float shift) { m_pitchShift = shift; }
void AudioStream::SetMusicalKey(int rootNote, KeyScaleMatcher::ScaleType scaleType) {
    (void)rootNote;
    (void)scaleType;
}

void AudioStream::pushSongBuffer(const std::vector<float>& buffer) {
    std::lock_guard<std::mutex> lock(m_songBufferMutex);
    m_outputBuffer.insert(m_outputBuffer.end(), buffer.begin(), buffer.end());
}

void AudioStream::pushSongBuffer(const float* data, int sampleCount) {
    if (!data || sampleCount <= 0) return;
    std::lock_guard<std::mutex> lock(m_songBufferMutex);
    m_outputBuffer.insert(m_outputBuffer.end(), data, data + sampleCount);
}

void AudioStream::ClearSongBuffer() {
    std::lock_guard<std::mutex> lock(m_songBufferMutex);
    m_outputBuffer.clear();
}

std::vector<float> AudioStream::GetOutputBuffer() const {
    return std::vector<float>(m_outputBuffer.begin(), m_outputBuffer.end());
}

bool AudioStream::decodeDeviceSamples(const char* bytes, qint64 byteCount, std::vector<float>& samples) const {
    const int bps = m_audioFormat.bytesPerSample();
    if (!bytes || byteCount <= 0 || bps <= 0) return false;
    const size_t count = static_cast<size_t>(byteCount / bps);
    samples.resize(count);
    for (size_t i = 0; i < count; ++i) {
        const char* p = bytes + i * static_cast<size_t>(bps);
        float v = 0.0f;
        switch (m_audioFormat.sampleFormat()) {
        case QAudioFormat::Float: { float x; std::memcpy(&x, p, sizeof(x)); v = std::isfinite(x) ? x : 0.0f; break; }
        case QAudioFormat::Int16: { int16_t x; std::memcpy(&x, p, sizeof(x)); v = static_cast<float>(x) / 32768.0f; break; }
        case QAudioFormat::Int32: { int32_t x; std::memcpy(&x, p, sizeof(x)); v = static_cast<float>(x) / 2147483648.0f; break; }
        case QAudioFormat::UInt8: { uint8_t x; std::memcpy(&x, p, sizeof(x)); v = (static_cast<float>(x) - 128.0f) / 128.0f; break; }
        default: return false;
        }
        samples[i] = std::clamp(v, -1.0f, 1.0f);
    }
    return true;
}

void AudioStream::encodeDeviceSamples(const std::vector<float>& samples, std::vector<char>& bytes) const {
    const int bps = m_audioFormat.bytesPerSample();
    if (bps <= 0) { bytes.clear(); return; }
    bytes.resize(samples.size() * static_cast<size_t>(bps));
    for (size_t i = 0; i < samples.size(); ++i) {
        const float v = std::clamp(samples[i], -1.0f, 1.0f);
        char* p = bytes.data() + i * static_cast<size_t>(bps);
        switch (m_audioFormat.sampleFormat()) {
        case QAudioFormat::Float: std::memcpy(p, &v, sizeof(v)); break;
        case QAudioFormat::Int16: { const int16_t x = static_cast<int16_t>(std::lround(v * 32767.0f)); std::memcpy(p, &x, sizeof(x)); break; }
        case QAudioFormat::Int32: { const int32_t x = static_cast<int32_t>(std::llround(v * 2147483647.0)); std::memcpy(p, &x, sizeof(x)); break; }
        case QAudioFormat::UInt8: { const uint8_t x = static_cast<uint8_t>(std::lround((v + 1.0f) * 127.5f)); std::memcpy(p, &x, sizeof(x)); break; }
        default: bytes.clear(); return;
        }
    }
}

void AudioStream::deleteLater() {
    QObject::deleteLater();
}

void AudioStream::processAudio() {
    if (!m_isRunning || !m_outputDevice) return;

    const int channels = std::max(1, m_audioFormat.channelCount());
    const int defaultFrames = std::max(1, m_sampleRate / 100);
    std::vector<float> monoInput;
    int framesRead = 0;
    bool hadRealAudio = false;

    if (m_inputDevice) {
        qint64 bytesAvailable = m_inputDevice->bytesAvailable();
        if (bytesAvailable > 0) {
            std::vector<char> inputBytes(static_cast<size_t>(bytesAvailable));
            qint64 bytesRead = m_inputDevice->read(inputBytes.data(), bytesAvailable);
            if (bytesRead > 0) {
                std::vector<float> interleavedInput;
                if (decodeDeviceSamples(inputBytes.data(), bytesRead, interleavedInput)) {
                    framesRead = static_cast<int>(interleavedInput.size() / static_cast<size_t>(channels));
                    if (framesRead > 0) {
                        hadRealAudio = true;
                        monoInput.resize(static_cast<size_t>(framesRead));
                        for (int frame = 0; frame < framesRead; ++frame) {
                            float sum = 0.0f;
                            for (int ch = 0; ch < channels; ++ch)
                                sum += interleavedInput[static_cast<size_t>(frame * channels + ch)];
                            monoInput[static_cast<size_t>(frame)] = sum / static_cast<float>(channels);
                        }
                    }
                }
            }
        }
    }

    if (framesRead <= 0) {
        framesRead = defaultFrames;
        monoInput.assign(static_cast<size_t>(framesRead), 0.0f);
    }

    const double activeSampleRate = static_cast<double>(m_sampleRate > 0 ? m_sampleRate : 44100);
    const double audioClockSec = static_cast<double>(m_playbackClockFrames) / activeSampleRate;
    ++m_logCounter;

    float micPitchHz = m_currentPitchHz;
    float micConfidence = m_currentConfidence;
    constexpr size_t pitchWindowSamples = 2048;
    if (hadRealAudio) {
        m_pitchInputHistory.insert(m_pitchInputHistory.end(), monoInput.begin(), monoInput.end());
        if (m_pitchInputHistory.size() > pitchWindowSamples)
            m_pitchInputHistory.erase(m_pitchInputHistory.begin(), m_pitchInputHistory.end() - pitchWindowSamples);
        if (!m_pitchInputHistory.empty()) {
            micPitchHz = m_pitchDetector.Process(m_pitchInputHistory.data(), static_cast<int>(m_pitchInputHistory.size()), m_sampleRate, micConfidence);
            m_currentPitchHz = micPitchHz;
            m_currentConfidence = micConfidence;
        }
    }

    float targetPitchHz = 0.0f;
    bool targetVoiced = false;
    if (m_melodyCorrectionEnabled && m_targetTimeline) {
        TargetPitchFrame targetFrame = m_targetTimeline->GetTargetPitchAt(audioClockSec);
        targetVoiced = targetFrame.isVoiced && targetFrame.targetHz > 0.0f;
        if (targetVoiced)
            targetPitchHz = targetFrame.targetHz;
    }

    bool isConsonant = false;
    if (hadRealAudio && framesRead > 1) {
        int zeroCrossings = 0;
        for (int i = 1; i < framesRead; ++i) {
            if ((monoInput[i - 1] >= 0.0f) != (monoInput[i] >= 0.0f))
                ++zeroCrossings;
        }
        float zcr = static_cast<float>(zeroCrossings) / static_cast<float>(framesRead);
        constexpr float kConsonantZcrThreshold = 0.35f;
        isConsonant = (zcr > kConsonantZcrThreshold);
    }

    float correctionSemitones = 0.0f;
    if (micPitchHz > 60.0f && micConfidence > 0.40f && !isConsonant) {
        auto hzToMidi = [](float hz) -> float {
            return 69.0f + 12.0f * std::log2(hz / 440.0f);
            };
        float userPitchMidi = hzToMidi(micPitchHz);
        float targetNoteMidi;
        if (targetPitchHz > 60.0f) {
            // Mode 1: song's target-pitch timeline is available -- correct toward the
            // original singer's actual note (existing behavior).
            targetNoteMidi = std::round(hzToMidi(targetPitchHz));
        }
        else {
            // Mode 2 (CHROMATIC FALLBACK): no target timeline data at this instant
            // (song not loaded / timeline gap) -- instead of leaving correction fully
            // off (which is what made the voice sound "flat"/uncorrected), snap toward
            // the nearest chromatic semitone to the singer's own pitch. This keeps
            // singing in-tune with itself even without a reference target.
            targetNoteMidi = std::round(userPitchMidi);
        }

        float rawError = targetNoteMidi - userPitchMidi;
        float errorSemitones = rawError - 12.0f * std::round(rawError / 12.0f);
        float errorCents = errorSemitones * 100.0f;
        float absErrorCents = std::abs(errorCents);

        float dynamicStrength;
        if (absErrorCents > 50.0f)      dynamicStrength = 0.65f;
        else if (absErrorCents > 20.0f) dynamicStrength = 0.45f;
        else                             dynamicStrength = 0.30f;

        correctionSemitones = errorSemitones * dynamicStrength * m_correctionStrength;

        constexpr float kPitchLockThresholdCents = 30.0f;
        constexpr float kPitchLockPullStrength = 0.65f;
        if (absErrorCents < kPitchLockThresholdCents) {
            correctionSemitones = errorSemitones * kPitchLockPullStrength * m_correctionStrength;
        }

        correctionSemitones = std::clamp(correctionSemitones, -12.0f, 12.0f);

        // Responsive retune speed (0.50f)
        constexpr float kSmoothingCoeff = 0.65f;
        m_smoothedCorrectionSemitones = m_smoothedCorrectionSemitones * (1.0f - kSmoothingCoeff) + correctionSemitones * kSmoothingCoeff;
        m_currentCorrectionSemitones = m_smoothedCorrectionSemitones;
    }
    else {
        m_smoothedCorrectionSemitones *= 0.9f;
        m_currentCorrectionSemitones = m_smoothedCorrectionSemitones;
    }

    const float appliedSemitones = m_currentCorrectionSemitones + m_pitchShift;

    std::vector<float> outputBuffer(static_cast<size_t>(framesRead));

    // Continuous Pitch Shifter
    m_pitchShifter.SetPitchSemitones(appliedSemitones);
    m_pitchShifter.Process(monoInput.data(), outputBuffer.data(), framesRead);

    // DRY/WET BLEND: real studio vocal correction is rarely 100% "wet" (fully
    // corrected) -- blending in some of the original (dry, uncorrected) signal keeps
    // the singer's natural character/imperfections instead of sounding "obviously
    // auto-tuned"/robotic. m_correctionWetMix is the corrected-signal proportion
    // (e.g. 0.70 = 70% corrected + 30% original).
    if (m_correctionWetMix < 0.999f) {
        for (int i = 0; i < framesRead; ++i) {
            outputBuffer[i] = outputBuffer[i] * m_correctionWetMix + monoInput[i] * (1.0f - m_correctionWetMix);
        }
    }

    // 1. Noise Gate
    {
        constexpr float kGateThreshold = 0.01f;
        float rms = 0.0f;
        for (int i = 0; i < framesRead; ++i)
            rms += outputBuffer[i] * outputBuffer[i];
        rms = std::sqrt(rms / static_cast<float>(framesRead));
        if (rms < kGateThreshold) {
            std::fill(outputBuffer.begin(), outputBuffer.begin() + framesRead, 0.0f);
        }
    }

    // 2. Studio De-Esser (Remove Harsh 'S' sounds)
    for (int i = 0; i < framesRead; ++i) {
        outputBuffer[i] = g_deEsser.Process(outputBuffer[i]);
    }

    // 3. Multi-band Vocal EQ + Air Sheen + Vocal Doubler + Slapback Delay
    for (int i = 0; i < framesRead; ++i) {
        outputBuffer[i] = m_vocalEQ.Process(outputBuffer[i]);
        outputBuffer[i] = m_airSheen.Process(outputBuffer[i]);
        // VocalDoubler removed here -- was causing an unwanted "double voice" effect.
        outputBuffer[i] = m_slapbackDelay.Process(outputBuffer[i], 0.10f);
    }

    // 4. Vocal Compressor
    if (m_compressorEnabled) {
        for (int i = 0; i < framesRead; ++i)
            outputBuffer[i] = m_compressor.Process(outputBuffer[i]);
    }

    // 5. Harmonic Enhancer
    for (int i = 0; i < framesRead; ++i) {
        outputBuffer[i] = m_harmonicEnhancer.Process(outputBuffer[i]);
    }

    // (duplicate second slapback delay removed here -- m_slapbackDelay earlier in the
    // chain, stage 3, is now the only delay stage)

    // 7. Reverb Processing
    if (m_reverbEnabled) {
        m_vocalReverb.SetWetMix(m_reverbMix > 0.05f ? m_reverbMix : 0.14f);
        m_vocalReverb.Process(outputBuffer.data(), framesRead);
    }

    if (m_micCaptureEnabled) {
        float processedPitchHz = 0.0f;
        float processedConfidence = 0.0f;
        m_processedPitchHistory.insert(m_processedPitchHistory.end(), outputBuffer.begin(), outputBuffer.end());
        if (m_processedPitchHistory.size() > pitchWindowSamples)
            m_processedPitchHistory.erase(m_processedPitchHistory.begin(), m_processedPitchHistory.end() - pitchWindowSamples);
        if (!m_processedPitchHistory.empty()) {
            processedPitchHz = m_processedPitchDetector.Process(m_processedPitchHistory.data(), static_cast<int>(m_processedPitchHistory.size()), m_sampleRate, processedConfidence);
            m_currentProcessedPitchHz = processedPitchHz;
            m_currentProcessedConfidence = processedConfidence;
        }

        LiveRawFrame frame;
        frame.timestampSec = audioClockSec;
        frame.rawFreqHz = micPitchHz;
        frame.confidence = micConfidence;
        frame.voiced = (micConfidence > 0.45f && micPitchHz > 0.0f);
        if (frame.voiced) {
            fillNoteFromHz(micPitchHz, frame.midiNote, frame.noteName, sizeof(frame.noteName));
        }
        frame.correctionSemitones = m_currentCorrectionSemitones;
        frame.manualSemitones = m_pitchShift;
        frame.totalAppliedSemitones = appliedSemitones;
        frame.finalFreqHz = processedPitchHz;
        frame.finalConfidence = processedConfidence;
        frame.finalVoiced = (processedConfidence > 0.45f && processedPitchHz > 0.0f);
        if (frame.finalVoiced) {
            fillNoteFromHz(processedPitchHz, frame.finalMidi, frame.finalNoteName, sizeof(frame.finalNoteName));
        }

        {
            std::lock_guard<std::mutex> lk(m_liveFramesMutex);
            m_liveRawFrames.push_back(frame);
        }
    }

    // READ SONG AUDIO STREAM FROM BUFFER QUEUE
    std::vector<float> mixed(static_cast<size_t>(framesRead * channels), 0.0f);
    size_t songFifoBefore = 0;
    size_t songSamplesCopied = 0;
    {
        std::lock_guard<std::mutex> lock(m_songBufferMutex);
        size_t samplesNeeded = static_cast<size_t>(framesRead * channels);
        songFifoBefore = m_outputBuffer.size();
        if (!m_outputBuffer.empty()) {
            size_t samplesToCopy = std::min(samplesNeeded, m_outputBuffer.size());
            std::copy_n(m_outputBuffer.begin(), samplesToCopy, mixed.begin());
            m_outputBuffer.erase(m_outputBuffer.begin(), m_outputBuffer.begin() + samplesToCopy);
            songSamplesCopied = samplesToCopy;
        }
    }

    // DECLICK Logic
    if (songSamplesCopied < static_cast<size_t>(framesRead * channels) && songSamplesCopied > 0) {
        constexpr size_t kFadeSamples = 64;
        size_t fadeStart = (songSamplesCopied > kFadeSamples) ? (songSamplesCopied - kFadeSamples) : 0;
        size_t fadeLen = songSamplesCopied - fadeStart;
        for (size_t i = 0; i < fadeLen; ++i) {
            float gain = 1.0f - (static_cast<float>(i + 1) / static_cast<float>(fadeLen + 1));
            mixed[fadeStart + i] *= gain;
        }
    }

    // SAFE MIXING WITH FALLBACK VOLUME PROTECTION
    const float activeMicVol = (m_micVolume > 0.01f) ? m_micVolume : 1.0f;
    const float activeSongVol = (m_earphoneVolume > 0.01f) ? m_earphoneVolume : 1.0f;
    const float activeSpeakerVol = (m_speakerVolume > 0.01f) ? m_speakerVolume : 1.0f;

    for (int frame = 0; frame < framesRead; ++frame) {
        float voice = outputBuffer[static_cast<size_t>(frame)] * activeMicVol;

        for (int ch = 0; ch < channels; ++ch) {
            size_t idx = static_cast<size_t>(frame * channels + ch);

            float songSample = mixed[idx] * activeSongVol;
            float voiceSample = voice;

            float finalMix = (songSample + voiceSample) * activeSpeakerVol;

            // Soft-knee limiter
            constexpr float kLimiterThreshold = 0.85f;
            if (std::abs(finalMix) > kLimiterThreshold) {
                float sign = (finalMix >= 0.0f) ? 1.0f : -1.0f;
                float excess = std::abs(finalMix) - kLimiterThreshold;
                finalMix = sign * (kLimiterThreshold + std::tanh(excess * 3.0f) / 3.0f);
            }
            mixed[idx] = std::clamp(finalMix, -1.0f, 1.0f);
        }
    }

    std::vector<char> outputBytes;
    encodeDeviceSamples(mixed, outputBytes);
    if (!outputBytes.empty()) {
        m_outputDevice->write(outputBytes.data(), static_cast<qint64>(outputBytes.size()));
    }

    m_framesProcessed += framesRead;
    m_playbackClockFrames += static_cast<uint64_t>(framesRead);
}

bool AudioStream::initializeAudioDevices(int inputDeviceIndex, int outputDeviceIndex) {
    QList<QAudioDevice> inputDevices = QMediaDevices::audioInputs();
    QList<QAudioDevice> outputDevices = QMediaDevices::audioOutputs();

    if (inputDevices.isEmpty() || outputDevices.isEmpty()) return false;

    QAudioDevice inputDevice = (inputDeviceIndex < 0 || inputDeviceIndex >= inputDevices.size()) ? QMediaDevices::defaultAudioInput() : inputDevices[inputDeviceIndex];
    QAudioDevice outputDevice = (outputDeviceIndex < 0 || outputDeviceIndex >= outputDevices.size()) ? QMediaDevices::defaultAudioOutput() : outputDevices[outputDeviceIndex];

    m_audioFormat = findCompatibleFormat(inputDevice, outputDevice);
    if (!m_audioFormat.isValid()) return false;

    m_sampleRate = m_audioFormat.sampleRate();
    m_pitchShifter.Initialize(m_sampleRate, 1);
    m_vocalEQ.Initialize(m_sampleRate);
    m_vocalDoubler.Init(m_sampleRate);
    m_slapbackDelay.Init(m_sampleRate, 130.0f);

    m_audioSource = new QAudioSource(inputDevice, m_audioFormat, this);
    m_audioSink = new QAudioSink(outputDevice, m_audioFormat, this);

    if (!m_audioSource || !m_audioSink) return false;

    m_audioSource->setBufferSize(std::max(m_bufferSize, 2048) * m_audioFormat.bytesPerSample() * m_audioFormat.channelCount());
    m_audioSink->setBufferSize(std::max(m_bufferSize, 4096) * m_audioFormat.bytesPerSample() * m_audioFormat.channelCount());

    return true;
}

QAudioFormat AudioStream::findCompatibleFormat(const QAudioDevice& inputDevice, const QAudioDevice& outputDevice) {
    QAudioFormat format;
    QList<QAudioFormat::SampleFormat> sampleFormats = { QAudioFormat::Float, QAudioFormat::Int16, QAudioFormat::Int32, QAudioFormat::UInt8 };
    QList<int> sampleRates = { 44100, 48000, 22050, 16000 };
    QList<int> channelCounts = { 2, 1 };

    for (int sr : sampleRates) {
        for (int cc : channelCounts) {
            for (QAudioFormat::SampleFormat sf : sampleFormats) {
                format.setSampleRate(sr);
                format.setChannelCount(cc);
                format.setSampleFormat(sf);
                if (inputDevice.isFormatSupported(format) && outputDevice.isFormatSupported(format))
                    return format;
            }
        }
    }
    return inputDevice.preferredFormat();
}

void AudioStream::logDeviceInfo(const QAudioDevice& device, const QString& type) {
    std::cout << "[AudioStream] " << type.toStdString() << " Device: " << device.description().toStdString() << std::endl;
}

void AudioStream::SetTargetPitchTimeline(const TargetPitchTimeline* timeline) { m_targetTimeline = timeline; }
void AudioStream::SetSongPlaybackPosition(double positionSeconds) { m_songPosition = positionSeconds; }
void AudioStream::SetCorrectionStrength(float strength) { m_correctionStrength = std::clamp(strength, 0.0f, 1.0f); }
void AudioStream::SetCorrectionWetMix(float wetMix) { m_correctionWetMix = std::clamp(wetMix, 0.0f, 1.0f); }
void AudioStream::EnableMelodyCorrection(bool enable) { m_melodyCorrectionEnabled = enable; }
void AudioStream::EnableReverb(bool enable) { m_reverbEnabled = enable; }
void AudioStream::SetReverbMix(float mix) { m_reverbMix = std::clamp(mix, 0.0f, 1.0f); }
void AudioStream::EnableCompressor(bool enable) { m_compressorEnabled = enable; }
void AudioStream::SetCompressorThreshold(float thresholdDb) { m_compressor.SetThreshold(thresholdDb); }
void AudioStream::SetCompressorRatio(float ratio) { m_compressor.SetRatio(ratio); }
void AudioStream::SetCompressorAttack(float attackMs) { m_compressor.SetAttack(attackMs); }
void AudioStream::SetCompressorRelease(float releaseMs) { m_compressor.SetRelease(releaseMs); }
void AudioStream::SetCompressorMakeupGain(float makeupGainDb) { m_compressor.SetMakeupGain(makeupGainDb); }

void AudioStream::cleanup() {
    Stop();
    delete m_audioSource; m_audioSource = nullptr;
    delete m_audioSink; m_audioSink = nullptr;
    m_inputDevice = nullptr; m_outputDevice = nullptr;
}