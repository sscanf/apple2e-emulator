#include "drivesounds.h"

#include <SDL.h>

#include <algorithm>
#include <filesystem>
#include <iostream>

namespace apple2e {

namespace {

// A set of drive recordings and the gains that balance them against the
// speaker (which peaks at 0.25). The first set whose motor file exists in the
// folder is used.
struct SoundSet {
    const char* motor;
    const char* steps[2];
    const char* grinds[2];
    const char* eject;
    float motorGain;
    float stepGain;
    float grindGain;
    float ejectGain;
};

constexpr SoundSet kSoundSets[] = {
    // The recordings bundled in assets/sounds; they peak at about 0.016
    // (motor), 0.05 (steps) and 0.55 (grind)
    {"Spin_Sound.wav", {"Read_1_Sound.wav", "Read_2_Sound.wav"},
     {"Grunt_Grind_1_Sound.wav", "Grunt_Grind_2_Sound.wav"}, "Sqweak_Sound.wav", 4.0f, 2.0f, 0.35f, 1.0f},
    // Generic names for user-supplied recordings, played at their own level
    {"motor.wav", {"step1.wav", "step2.wav"}, {"grind.wav", nullptr}, "eject.wav", 1.0f, 1.0f, 1.0f, 1.0f},
};

constexpr float kSpinFadePerSample = 1.0f / 1500;  // ~30 ms fade in/out
constexpr size_t kMaxVoices = 8;

// Load a 16-bit WAV as mono float at `rate` (linear resampling), scaled by `gain`
std::vector<float> loadWav(const std::filesystem::path& path, int rate, float gain) {
    SDL_AudioSpec spec;
    Uint8* data = nullptr;
    Uint32 length = 0;
    if (!SDL_LoadWAV(path.string().c_str(), &spec, &data, &length)) return {};

    std::vector<float> samples;
    if (spec.format == AUDIO_S16LSB) {
        const auto* pcm = reinterpret_cast<const int16_t*>(data);
        size_t frames = length / (sizeof(int16_t) * spec.channels);
        std::vector<float> source(frames);
        for (size_t i = 0; i < frames; i++) {
            float sum = 0;
            for (int c = 0; c < spec.channels; c++) sum += pcm[i * spec.channels + c];
            source[i] = sum / (32768.0f * spec.channels);
        }

        double step = static_cast<double>(spec.freq) / rate;
        size_t outFrames = static_cast<size_t>(frames / step);
        samples.resize(outFrames);
        for (size_t i = 0; i < outFrames; i++) {
            double pos = i * step;
            size_t i0 = static_cast<size_t>(pos);
            size_t i1 = std::min(i0 + 1, frames - 1);
            float frac = static_cast<float>(pos - i0);
            samples[i] = (source[i0] * (1 - frac) + source[i1] * frac) * gain;
        }
    } else {
        std::cerr << "Skipping " << path << ": only 16-bit PCM WAV is supported" << std::endl;
    }

    SDL_FreeWAV(data);
    return samples;
}

// Turn a recording into a seamless loop by crossfading its tail into its head
std::vector<float> makeLoop(std::vector<float> s, size_t fade) {
    if (s.size() < fade * 3) return s;
    size_t tail = s.size() - fade;
    for (size_t i = 0; i < fade; i++) {
        float t = static_cast<float>(i) / fade;
        s[i] = s[i] * t + s[tail + i] * (1 - t);
    }
    s.resize(tail);
    return s;
}

} // namespace

bool DriveSounds::load(const std::string& directory, int sampleRate) {
    namespace fs = std::filesystem;
    fs::path dir(directory);
    std::error_code ec;

    for (const SoundSet& set : kSoundSets) {
        if (!fs::exists(dir / set.motor, ec)) continue;

        m_spin = makeLoop(loadWav(dir / set.motor, sampleRate, set.motorGain), sampleRate / 20);
        for (const char* name : set.steps) {
            auto sample = loadWav(dir / name, sampleRate, set.stepGain);
            if (!sample.empty()) m_steps.push_back(std::move(sample));
        }
        for (const char* name : set.grinds) {
            if (!name) continue;
            auto sample = loadWav(dir / name, sampleRate, set.grindGain);
            if (!sample.empty()) m_grinds.push_back(std::move(sample));
        }
        m_eject = loadWav(dir / set.eject, sampleRate, set.ejectGain);
        break;
    }

    bool any = !m_spin.empty() || !m_steps.empty() || !m_grinds.empty() || !m_eject.empty();
    if (any) std::cout << "Drive sounds loaded from " << directory << std::endl;
    return any;
}

void DriveSounds::playEject() {
    if (m_enabled && !m_eject.empty()) start(m_eject, 0, false);
}

void DriveSounds::start(const Sample& sample, int delay, bool head) {
    if (m_voices.size() >= kMaxVoices) m_voices.erase(m_voices.begin());
    m_voices.push_back({&sample, 0, delay, head});
}

bool DriveSounds::grindPlaying() const {
    return std::any_of(m_voices.begin(), m_voices.end(), [this](const Voice& v) {
        return std::any_of(m_grinds.begin(), m_grinds.end(), [&](const Sample& g) { return v.sample == &g; });
    });
}

void DriveSounds::mix(float* buffer, size_t n, double startCycle, double cyclesPerSample) {
    // Schedule the events that fall within this buffer at their exact sample
    double endCycle = startCycle + n * cyclesPerSample;
    auto due = std::partition(m_events.begin(), m_events.end(),
                              [endCycle](const PendingEvent& e) { return e.cycle < endCycle; });
    for (auto it = m_events.begin(); it != due; ++it) {
        int delay = std::max(0, static_cast<int>((it->cycle - startCycle) / cyclesPerSample));
        if (!m_enabled) continue;

        if (it->event == Event::Step && !m_steps.empty()) {
            start(m_steps[m_nextStep++ % m_steps.size()], delay, true);
        } else if (it->event == Event::Bump && !m_grinds.empty() && !grindPlaying()) {
            // Repeated bumps (recalibration) keep one grind going rather than stacking
            start(m_grinds[m_nextGrind++ % m_grinds.size()], delay, true);
        }
    }
    m_events.erase(m_events.begin(), due);

    bool motor = m_enabled && m_motorOn && !m_spin.empty();
    for (size_t i = 0; i < n; i++) {
        // Motor loop
        if (motor) {
            m_spinLevel = std::min(1.0f, m_spinLevel + kSpinFadePerSample);
        } else {
            m_spinLevel = std::max(0.0f, m_spinLevel - kSpinFadePerSample);
        }
        if (m_spinLevel > 0) {
            buffer[i] += m_spin[m_spinPosition] * m_spinLevel * m_motorVolume;
            m_spinPosition = (m_spinPosition + 1) % m_spin.size();
        }

        // One-shot sounds
        for (auto& voice : m_voices) {
            if (voice.delay > 0) {
                voice.delay--;
            } else if (voice.position < voice.sample->size()) {
                buffer[i] += (*voice.sample)[voice.position++] * (voice.head ? m_headVolume : 1.0f);
            }
        }
    }

    m_voices.erase(std::remove_if(m_voices.begin(), m_voices.end(),
                                  [](const Voice& v) { return v.position >= v.sample->size(); }),
                   m_voices.end());
}

} // namespace apple2e
