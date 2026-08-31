#include "PitchShifter.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <rubberband/RubberBandStretcher.h>

using namespace RubberBand;

PitchShifter::PitchShifter() {}
PitchShifter::~PitchShifter() {}

bool PitchShifter::Initialize(int sampleRate, int channels) {
    m_sampleRate = sampleRate;
    m_channels = channels;

    // DOUBLE VOICE FIX:
    // OptionFormantPreserved ko hata kar OptionWindowStandard aur OptionPhaseLaminar lagaya hai
    // Isse chorusing/phasing aur double voice ka effect khatam hoke clean melodious tone aati hai.
    RubberBandStretcher::Options options =
        RubberBandStretcher::OptionProcessRealTime |
        RubberBandStretcher::OptionPitchHighQuality |
        RubberBandStretcher::OptionWindowStandard |
        RubberBandStretcher::OptionPhaseLaminar |
        RubberBandStretcher::OptionChannelsTogether;

    m_stretcher = std::make_unique<RubberBandStretcher>(
        static_cast<size_t>(sampleRate), static_cast<size_t>(channels), options);

    m_stretcher->setTimeRatio(1.0);
    m_targetPitchRatio = 1.0f;
    m_currentPitchRatio = 1.0f;
    m_stretcher->setPitchScale(1.0);

    m_outputFifo.clear();
    return true;
}

void PitchShifter::SetPitch(float pitchRatio) {
    if (pitchRatio < 0.25f) pitchRatio = 0.25f;
    if (pitchRatio > 4.0f) pitchRatio = 4.0f;

    // Direct scale badalne se double voice rumble aata hai, isliye target update kar rahe hain
    m_targetPitchRatio = pitchRatio;
    m_pitchRatio = pitchRatio;
}

void PitchShifter::SetPitchSemitones(float semitones) {
    float ratio = std::pow(2.0f, semitones / 12.0f);
    SetPitch(ratio);
}

int PitchShifter::Process(const float* in, float* out, int frames) {
    if (!in || !out || frames <= 0) return 0;

    if (!m_stretcher) {
        std::memcpy(out, in, static_cast<size_t>(frames) * sizeof(float));
        return frames;
    }

    // Smooth Pitch Interpolation (Eliminates pitch jitter & double voice artifacts)
    if (std::abs(m_currentPitchRatio - m_targetPitchRatio) > 0.001f) {
        m_currentPitchRatio += (m_targetPitchRatio - m_currentPitchRatio) * 0.1f; // Smooth transition
        m_stretcher->setPitchScale(static_cast<double>(m_currentPitchRatio));
    }

    const float* inChannels[1] = { in };
    m_stretcher->process(inChannels, static_cast<size_t>(frames), false);

    int available = static_cast<int>(m_stretcher->available());
    if (available > 0) {
        std::vector<float> retrieved(static_cast<size_t>(available));
        float* outChannels[1] = { retrieved.data() };
        int got = static_cast<int>(m_stretcher->retrieve(outChannels, static_cast<size_t>(available)));
        for (int i = 0; i < got; ++i)
            m_outputFifo.push_back(retrieved[static_cast<size_t>(i)]);
    }

    for (int i = 0; i < frames; ++i) {
        if (!m_outputFifo.empty()) {
            out[i] = m_outputFifo.front();
            m_outputFifo.pop_front();
        }
        else {
            out[i] = 0.0f;
        }
    }

    return frames;
}