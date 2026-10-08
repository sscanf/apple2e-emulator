#pragma once

#include <SDL.h>

#include <cstdint>
#include <vector>

namespace apple2e {

class DriveSounds;

// 1-bit speaker: every access to $C030 flips the cone. Toggles are
// timestamped in CPU cycles and turned into samples at the end of each frame.
class AudioController {
public:
    ~AudioController();

    // Opens the output device; SDL audio must already be initialised
    bool init(int sampleRate = 44100);

    void toggleSpeaker(uint64_t cycle);

    // Optional extra source mixed into the output
    void setDriveSounds(DriveSounds* sounds) { m_driveSounds = sounds; }
    int sampleRate() const { return m_sampleRate; }

    // Render all samples up to `cycle` and queue them for playback
    void endFrame(uint64_t cycle);

    // Restart sample timing at `cycle` (after the cycle counter jumps, e.g.
    // when a save state is loaded)
    void resync(uint64_t cycle);

private:
    SDL_AudioDeviceID m_device = 0;
    int m_sampleRate = 0;
    DriveSounds* m_driveSounds = nullptr;
    double m_cyclesPerSample = 0;
    double m_nextSampleCycle = 0;

    std::vector<uint64_t> m_toggles;
    bool m_level = false;

    // DC-blocking filter state (the cone rests at either level)
    float m_prevIn = 0;
    float m_prevOut = 0;

    std::vector<float> m_mix;
    std::vector<int16_t> m_samples;
};

} // namespace apple2e
