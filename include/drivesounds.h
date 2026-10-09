#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace apple2e {

// Mechanical Disk II noises mixed into the audio output: the motor hum while
// the disk spins, a click per head step, the grind of the head hitting the
// track 0 stop, and the door when a disk is taken out. Samples are loaded
// from WAV files; any that are missing are simply not played.
class DriveSounds {
public:
    enum class Event { Step, Bump };

    // Load the samples from `directory`, resampled to `sampleRate`.
    // Returns false if none could be loaded.
    bool load(const std::string& directory, int sampleRate);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on) { m_enabled = on; }

    void setMotor(bool spinning) { m_motorOn = spinning; }
    void trigger(Event event, uint64_t cycle) { m_events.push_back({event, cycle}); }
    void clearEvents() { m_events.clear(); }

    // Disk taken out of a drive (plays right away; user action, not CPU-timed)
    void playEject();

    // Add drive sounds to `n` samples whose first sample is at CPU cycle `startCycle`
    void mix(float* buffer, size_t n, double startCycle, double cyclesPerSample);

private:
    using Sample = std::vector<float>;

    struct Voice {
        const Sample* sample;
        size_t position;
        int delay;  // samples to wait before starting
    };

    struct PendingEvent {
        Event event;
        uint64_t cycle;
    };

    void start(const Sample& sample, int delay);
    bool grindPlaying() const;

    bool m_enabled = true;
    bool m_motorOn = false;

    Sample m_spin;  // seamless loop
    std::vector<Sample> m_steps;
    std::vector<Sample> m_grinds;
    Sample m_eject;
    size_t m_nextStep = 0;
    size_t m_nextGrind = 0;

    size_t m_spinPosition = 0;
    float m_spinLevel = 0;  // fades the motor loop in and out

    std::vector<Voice> m_voices;
    std::vector<PendingEvent> m_events;
};

} // namespace apple2e
