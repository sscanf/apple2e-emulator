#include "audio.h"

#include <cstring>
#include <cmath>
#include <iostream>

namespace apple2e {

AudioController::AudioController() {}

AudioController::~AudioController() {}

bool AudioController::init(float sampleRate) {
    m_sampleRate = sampleRate;
    return true;
}

void AudioController::setSpeakerState(bool state) {
    m_targetVolume = state ? 1.0f : 0.0f;
}

void AudioController::audioCallbackInternal(int16_t* buffer, uint32_t frames) {
    // Generate audio samples
    for (uint32_t i = 0; i < frames; i++) {
        // Smooth volume transition
        float volumeDiff = m_targetVolume - m_currentVolume;
        m_currentVolume += volumeDiff * 0.01f;  // Smooth transition

        if (m_currentVolume > 0.001f) {
            // Generate speaker beep (square wave)
            // Apple II speaker frequency is roughly 1000-2000 Hz
            m_oscillatorPhase += m_oscillatorFreq / m_sampleRate;
            if (m_oscillatorPhase >= 1.0f) {
                m_oscillatorPhase -= 1.0f;
            }

            // Square wave
            int16_t sample = (m_oscillatorPhase < 0.5f) ? 32767 : -32768;
            sample = static_cast<int16_t>(static_cast<float>(sample) * m_currentVolume * 0.3f);
            buffer[i] = sample;
        } else {
            buffer[i] = 0;
        }
    }
}

void AudioController::s_audioCallback(void* userdata, uint8_t* stream, int len) {
    AudioController* audio = static_cast<AudioController*>(userdata);
    uint32_t frames = len / sizeof(int16_t);
    int16_t* buffer = reinterpret_cast<int16_t*>(stream);

    audio->audioCallbackInternal(buffer, frames);
}

AudioController::AudioCallback AudioController::getCallback() {
    return [this](int16_t* buffer, uint32_t frames) {
        audioCallbackInternal(buffer, frames);
    };
}

} // namespace apple2e
