#pragma once

#include <cstdint>
#include <vector>
#include <functional>

namespace apple2e {

// Audio output for Apple IIe speaker
class AudioController {
public:
    AudioController();
    ~AudioController();

    // Initialize audio
    bool init(float sampleRate = 44100.0f);

    // Set speaker state (from VIA)
    void setSpeakerState(bool state);

    // Get audio callback for SDL
    using AudioCallback = std::function<void(int16_t* buffer, uint32_t frames)>;
    AudioCallback getCallback();

    // Get current sample rate
    float getSampleRate() const { return m_sampleRate; }

private:
    void audioCallbackInternal(int16_t* buffer, uint32_t frames);

    float m_sampleRate = 44100.0f;
    bool m_speakerState = false;
    float m_currentVolume = 0.0f;
    float m_targetVolume = 0.0f;

    // Simple oscillator for speaker beep
    float m_oscillatorPhase = 0.0f;
    float m_oscillatorFreq = 1000.0f; // Hz

    // Audio callback function pointer
    static void s_audioCallback(void* userdata, uint8_t* stream, int len);
};

} // namespace apple2e
