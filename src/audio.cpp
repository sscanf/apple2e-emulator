#include "audio.h"

#include "io.h"

#include <iostream>

namespace apple2e {

namespace {
constexpr float kVolume = 0.25f;
constexpr float kDcBlock = 0.995f;
constexpr double kMaxQueuedSeconds = 0.1;  // drop audio rather than build up lag
}

AudioController::~AudioController() {
    if (m_device) SDL_CloseAudioDevice(m_device);
}

bool AudioController::init(int sampleRate) {
    SDL_AudioSpec want{};
    want.freq = sampleRate;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;

    SDL_AudioSpec have{};
    m_device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!m_device) {
        std::cerr << "Audio disabled: " << SDL_GetError() << std::endl;
        return false;
    }

    m_cyclesPerSample = kCpuClockHz / have.freq;
    SDL_PauseAudioDevice(m_device, 0);
    return true;
}

void AudioController::toggleSpeaker(uint64_t cycle) {
    m_toggles.push_back(cycle);
}

void AudioController::endFrame(uint64_t cycle) {
    if (!m_device) {
        m_level ^= m_toggles.size() & 1;
        m_toggles.clear();
        return;
    }

    m_samples.clear();
    size_t next = 0;
    while (m_nextSampleCycle < static_cast<double>(cycle)) {
        while (next < m_toggles.size() && m_toggles[next] <= m_nextSampleCycle) {
            m_level = !m_level;
            next++;
        }

        float in = m_level ? kVolume : -kVolume;
        float out = in - m_prevIn + kDcBlock * m_prevOut;
        m_prevIn = in;
        m_prevOut = out;
        m_samples.push_back(static_cast<int16_t>(out * 32767.0f));

        m_nextSampleCycle += m_cyclesPerSample;
    }
    m_toggles.erase(m_toggles.begin(), m_toggles.begin() + next);

    uint32_t queuedBytes = SDL_GetQueuedAudioSize(m_device);
    double queuedSeconds = queuedBytes / (sizeof(int16_t) * kCpuClockHz / m_cyclesPerSample);
    if (queuedSeconds < kMaxQueuedSeconds) {
        SDL_QueueAudio(m_device, m_samples.data(),
                       static_cast<uint32_t>(m_samples.size() * sizeof(int16_t)));
    }
}

} // namespace apple2e
