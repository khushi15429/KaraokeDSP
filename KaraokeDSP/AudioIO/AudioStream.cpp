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

AudioStream::AudioStream(QObject* parent)
    : QObject(parent), m_sampleRate(44100), m_bufferSize(512), m_currentPitchHz(0.0f), m_currentConfidence(0.0f),
    m_vocalReverb(m_sampleRate, 0.40f, 0.60f) { // Optimized space dampening
    m_audioBuffer = new AudioBuffer(65536);
    m_processingTimer = new QTimer(this);
    m_processingTimer->setTimerType(Qt::PreciseTimer);
    connect(m_processingTimer, &QTimer::timeout, [this]() { processAudio(); });
    m_pitchShifter.Initialize(m_sampleRate, 1);
    m_vocalEQ.Initialize(m_sampleRate);

    m_micVolume = 1.0f;
    m_speakerVolume = 1.0f;
    m_earphoneVolume = 1.0f;

    // Smooth Reverb Blend for Sweetness (8% Subdued Spatial Air)
    m_reverbMix = 0.08f;

    // Gentle Warm Vocal Compression Settings
    m_compressor.SetThreshold(-20.0f);
    m_compressor.SetRatio(2.0f);
    m_compressor.SetAttack(15.0f);
    m_compressor.SetRelease(150.0f);
    m_compressor.SetMakeupGain(2.0f);

    // TEST DEFAULT: m_correctionStrength now actually affects correction (previously
    // unused/dead). 0.65 = 65%, per your requested test range.
    m_correctionStrength = 0.65f;
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
    if (m_melodyCorrectionEnabled && m_targetTimeline) {
        TargetPitchFrame targetFrame = m_targetTimeline->GetTargetPitchAt(audioClockSec);
        if (targetFrame.isVoiced && targetFrame.targetHz > 0.0f)
            targetPitchHz = targetFrame.targetHz;
    }

    // Dynamic Smooth Pitch Correction Engine
    float correctionSemitones = m_smoothedCorrectionSemitones; // HOLD last value by default (don't snap to 0 on a brief confidence dip)
    bool correctionComputedThisTick = false;
    float debugSemitoneDiff = 0.0f;
    if (micPitchHz > 0.0f && targetPitchHz > 0.0f && micConfidence > 0.45f) {
        float semitoneDiff = 12.0f * std::log2(targetPitchHz / micPitchHz);
        debugSemitoneDiff = semitoneDiff;
        // FIX: was hardcoded 0.30f, ignoring m_correctionStrength entirely (the UI knob
        // had zero effect). Now actually uses it. Clamp widened from +-2 to +-6 semitones
        // so real off-key singing isn't silently capped.
        correctionSemitones = std::clamp(semitoneDiff * m_correctionStrength, -6.0f, 6.0f);
        correctionComputedThisTick = true;
    }

    m_smoothedCorrectionSemitones = m_smoothedCorrectionSemitones * 0.85f + correctionSemitones * 0.15f;
    const float appliedSemitones = m_smoothedCorrectionSemitones + m_pitchShift;

    // DEBUG: throttle to ~once every 500ms (every 50th tick at 10ms/tick) so console isn't flooded
    if ((m_logCounter++ % 50) == 0) {
        std::cout << "[PitchDebug] mic=" << micPitchHz << "Hz conf=" << micConfidence
            << " target=" << targetPitchHz << "Hz diff=" << debugSemitoneDiff
            << "st strength=" << m_correctionStrength
            << " raw_corr=" << correctionSemitones
            << " smoothed=" << m_smoothedCorrectionSemitones
            << " applied=" << appliedSemitones << "st"
            << " computed=" << (correctionComputedThisTick ? "yes" : "held") << std::endl;
    }

    std::vector<float> processedVoice(static_cast<size_t>(framesRead));

    // FIX (Bug #2 — the actual cause of "double voice"): ALWAYS route through the
    // pitch shifter, never branch to a raw bypass. RubberBand has an internal FIFO
    // with inherent latency; switching between "shifted" and "raw passthrough" paths
    // tick-to-tick left stale buffered samples in the FIFO that later got mixed with
    // fresh raw audio -- that mismatch IS the doubling/echo artifact. Always processing
    // through one single consistent path (even at ratio ~1.0) keeps latency constant
    // and eliminates the doubling, at the cost of a small constant added latency
    // (not a duplicated/smeared signal).
    m_pitchShifter.SetPitchSemitones(appliedSemitones);
    m_pitchShifter.Process(monoInput.data(), processedVoice.data(), framesRead);

    // Smooth Vocal Chain (EQ -> Harmonic Air -> Compression -> Reverb)
    for (int i = 0; i < framesRead; ++i) {
        processedVoice[i] = m_vocalEQ.Process(processedVoice[i]);
        processedVoice[i] = m_harmonicEnhancer.Process(processedVoice[i]);
        if (m_compressorEnabled)
            processedVoice[i] = m_compressor.Process(processedVoice[i]);
    }

    if (m_reverbEnabled) {
        m_vocalReverb.SetWetMix(m_reverbMix);
        m_vocalReverb.Process(processedVoice.data(), framesRead);
    }

    // BACKGROUND SONG MIXING
    std::vector<float> mixed(static_cast<size_t>(framesRead * channels), 0.0f);
    {
        std::lock_guard<std::mutex> lock(m_songBufferMutex);
        size_t samplesNeeded = static_cast<size_t>(framesRead * channels);
        if (!m_outputBuffer.empty()) {
            size_t samplesToCopy = std::min(samplesNeeded, m_outputBuffer.size());
            std::copy_n(m_outputBuffer.begin(), samplesToCopy, mixed.begin());
            m_outputBuffer.erase(m_outputBuffer.begin(), m_outputBuffer.begin() + samplesToCopy);
        }
    }

    const float activeMicVol = m_micVolume;
    const float activeSongVol = m_earphoneVolume;
    const float activeSpeakerVol = m_speakerVolume;

    for (int frame = 0; frame < framesRead; ++frame) {
        float vSample = processedVoice[static_cast<size_t>(frame)] * activeMicVol;

        for (int ch = 0; ch < channels; ++ch) {
            size_t idx = static_cast<size_t>(frame * channels + ch);
            float sSample = mixed[idx] * activeSongVol;
            mixed[idx] = std::clamp((sSample + vSample) * activeSpeakerVol, -1.0f, 1.0f);
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