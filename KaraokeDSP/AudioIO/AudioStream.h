#ifndef AUDIOSTREAM_H
#define AUDIOSTREAM_H

#include "TargetPitchTimeline.h"
#include "AudioBuffer.h"
#include "../KeyScaleMatcher/KeyScaleMatcher.h"
#include "../PitchDetector/PitchDetector.h"
#include "../PitchShifter/PitchShifter.h"
#include "../DSP/VocalReverb.h"
#include "../DSP/Compressor.h"
#include "../DSP/VocalWarmthEQ.h"
#include "../DSP/VocalHarmonicEnhancer.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <mutex>
#include <functional>
#include <string>
#include <QObject>
#include <QTimer>
#include <QAudioDevice>
#include <QAudioSource>
#include <QAudioSink>
#include <QIODevice>
#include <QAudioFormat>
#include <QMediaDevices>

class AudioStream : public QObject {
public:
    struct LiveRawFrame {
        double timestampSec = 0.0;
        float rawFreqHz = 0.0f;
        float confidence = 0.0f;
        bool voiced = false;
        int16_t midiNote = 0;
        char noteName[16] = { 0 };
        float correctionSemitones = 0.0f;   // autotune-only correction (== m_currentCorrectionSemitones)
        float manualSemitones = 0.0f;        // report-only: manual UI pitch-shift (m_pitchShift) at this frame
        float totalAppliedSemitones = 0.0f;  // report-only: actual shift sent to the pitch shifter (autotune + manual)
        float finalFreqHz = 0.0f;
        float finalConfidence = 0.0f;
        bool finalVoiced = false;
        int16_t finalMidi = 0;
        char finalNoteName[16] = { 0 };
    };

    using ComparisonReportCallback = std::function<void(bool success, const std::string& jsonPath, const std::string& message)>;

    explicit AudioStream(QObject* parent = nullptr);
    AudioStream(int sampleRate, int bufferSize, QObject* parent = nullptr);
    ~AudioStream();

    bool Open();
    bool Open(int inputDeviceIndex, int outputDeviceIndex);
    bool Start();
    void Stop();

    bool Initialize(int sampleRate, int bufferSize);
    void ProcessBuffer(const float* buffer, int numFrames);

    void SetMicVolume(float vol);
    void SetSpeakerVolume(float vol);
    void SetEarphoneVolume(float vol);
    void SetPitchShift(float shift);
    void SetMusicalKey(int rootNote, KeyScaleMatcher::ScaleType scaleType);

    void pushSongBuffer(const std::vector<float>& buffer);
    void pushSongBuffer(const float* data, int sampleCount);
    void ClearSongBuffer();
    const std::vector<float>& GetOutputBuffer() const;
    AudioBuffer* GetSongBuffer() noexcept { return m_audioBuffer; }
    int GetSampleRate() const noexcept { return m_sampleRate; }
    int GetChannelCount() const noexcept { return m_audioFormat.channelCount(); }

    float GetCurrentPitchHz() const { return m_currentPitchHz; }
    float GetCurrentConfidence() const { return m_currentConfidence; }
    float GetCurrentCorrectionSemitones() const { return m_currentCorrectionSemitones; }
    float GetCurrentProcessedPitchHz() const { return m_currentProcessedPitchHz; }
    double GetPlaybackClockSec() const { return static_cast<double>(m_playbackClockFrames) / static_cast<double>(m_sampleRate > 0 ? m_sampleRate : 44100); }

    void SetTargetPitchTimeline(const TargetPitchTimeline* timeline);
    void SetTargetJsonPath(const std::string& path);
    void SetSongPath(const std::string& path);
    void SetSongPlaybackPosition(double positionSeconds);
    void SetCorrectionStrength(float strength);
    void EnableMelodyCorrection(bool enable);

    void EnableReverb(bool enable);
    void SetReverbMix(float mix);

    void EnableCompressor(bool enable);
    void SetCompressorThreshold(float thresholdDb);
    void SetCompressorRatio(float ratio);
    void SetCompressorAttack(float attackMs);
    void SetCompressorRelease(float releaseMs);
    void SetCompressorMakeupGain(float makeupGainDb);

    void deleteLater();
    bool reportReady = true;

    std::vector<LiveRawFrame> GetAndClearLiveRawFrames();
    bool m_micCaptureEnabled = false;
    std::vector<float> m_micSamplesForAnalysis;

    void StartMicCapture();
    void StopMicCaptureAndAnalyze(const std::string& userId, const std::string& songNamePrefix);
    void SetComparisonReportCallback(ComparisonReportCallback cb);

private:
    void processAudio();
    bool initializeAudioDevices(int inputDeviceIndex, int outputDeviceIndex);
    QAudioFormat findCompatibleFormat(const QAudioDevice& inputDevice, const QAudioDevice& outputDevice);
    void logDeviceInfo(const QAudioDevice& device, const QString& type);
    void cleanup();
    bool decodeDeviceSamples(const char* bytes, qint64 byteCount,
        std::vector<float>& interleavedSamples) const;
    void encodeDeviceSamples(const std::vector<float>& interleavedSamples,
        std::vector<char>& bytes) const;
    void fillNoteFromHz(float hz, int16_t& midi, char* noteName, size_t noteNameSize) const;

    int m_sampleRate = 44100;
    int m_bufferSize = 512;
    float m_micVolume = 1.0f;
    float m_speakerVolume = 1.0f;
    float m_earphoneVolume = 1.0f;
    float m_pitchShift = 0.0f;
    float m_currentPitchHz = 0.0f;
    float m_currentConfidence = 0.0f;
    float m_currentProcessedPitchHz = 0.0f;
    float m_currentProcessedConfidence = 0.0f;
    std::vector<float> m_pitchInputHistory;
    std::vector<float> m_processedPitchHistory;

    std::vector<float> m_outputBuffer;
    mutable std::mutex m_songBufferMutex;
    AudioBuffer* m_audioBuffer = nullptr;

    QAudioSource* m_audioSource = nullptr;
    QAudioSink* m_audioSink = nullptr;
    QIODevice* m_inputDevice = nullptr;
    QIODevice* m_outputDevice = nullptr;
    QAudioFormat m_audioFormat;
    QTimer* m_processingTimer = nullptr;

    PitchDetector m_pitchDetector;
    PitchDetector m_processedPitchDetector;
    PitchShifter m_pitchShifter;
    const TargetPitchTimeline* m_targetTimeline = nullptr;
    std::string m_targetJsonPath;
    std::string m_songPath;
    double m_songPosition = 0.0;                 // coarse QMediaPlayer video position (display only; no longer authoritative)
    uint64_t m_playbackClockFrames = 0;          // P1: sample-accurate audio clock (frames processed since Start()); authoritative timeline position
    float m_correctionStrength = 1.0f;
    bool m_melodyCorrectionEnabled = false;
    float m_currentCorrectionSemitones = 0.0f;
    float m_smoothedCorrectionSemitones = 0.0f;
    uint64_t m_logCounter = 0;

    VocalReverb m_vocalReverb;
    bool m_reverbEnabled = false;
    float m_reverbMix = 0.25f;

    Compressor m_compressor;
    VocalWarmthEQ m_vocalEQ; // VOICE-ENHANCEMENT ITEM 6: multi-band presence/clarity EQ
    VocalHarmonicEnhancer m_harmonicEnhancer; // ITEMS 3b/3c: exciter + saturation
    double m_smoothedPitchHz = 0.0;   // ITEM 1/5: EMA-tracked slow pitch drift
    bool m_correctionActive = false;  // ITEM 7: hysteresis state
    bool m_compressorEnabled = false;

    uint64_t m_framesProcessed = 0;
    uint64_t m_underruns = 0;
    uint64_t m_overruns = 0;
    uint64_t m_errors = 0;
    bool m_isRunning = false;

    std::vector<LiveRawFrame> m_liveRawFrames;
    std::mutex m_liveFramesMutex;
    ComparisonReportCallback m_comparisonReportCallback;
};

#endif // AUDIOSTREAM_H