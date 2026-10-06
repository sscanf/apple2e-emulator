#pragma once

#include "cpu.h"
#include "memory.h"
#include "io.h"
#include "video.h"
#include "audio.h"
#include "keyboard.h"

#include <string>

namespace apple2e {

// Main Apple IIe emulator
class Apple2e {
public:
    Apple2e();
    ~Apple2e();

    // Initialize with ROM file
    bool init(const std::string& romPath);

    // Run emulation loop
    void run();

    // Stop emulation
    void stop();

    // Toggle display
    void toggleDisplay();

    // Get CPU cycle count
    uint64_t getCycles() const { return m_cycles; }

    // Get FPS
    double getFPS() const { return m_fps; }

    // Get current PC
    uint16_t getCurrentPC() const { return m_cpu.pc(); }

private:
    // Memory/I/O callbacks
    uint8_t memoryReadCallback(uint16_t addr);
    void memoryWriteCallback(uint16_t addr, uint8_t val);
    void cycleCallback(uint32_t cycles);

    // Video memory callbacks
    uint8_t videoMemoryReadCallback(uint16_t addr);

    // Keyboard VIA callbacks
    uint8_t via1PortAReadCallback();
    void via1PortBWriteCallback(uint8_t val);

    // Keyboard PIA callbacks
    uint8_t piaPortAReadCallback();
    uint8_t piaPortBReadCallback();

    // Initialize hardware
    void initCPU();
    void initMemory(const std::string& romPath);
    void initIO();
    void initVideo();
    void initAudio();
    void initKeyboard();

    // Handle reset vector
    void handleReset();

    // SDL state
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    bool m_running = false;

    // Hardware components
    CPU m_cpu;
    Memory m_memory;
    IOController m_io;
    VIA m_via1;
    VIA m_via2;
    PIA m_pia;
    VideoController m_video;
    AudioController m_audio;
    KeyboardController m_keyboard;

    // Timing
    uint64_t m_cycles = 0;
    double m_fps = 0;
    uint64_t m_frameStart = 0;
    int m_frameCount = 0;

    // ROM path
    std::string m_romPath;
};

} // namespace apple2e
