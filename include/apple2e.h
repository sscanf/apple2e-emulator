#pragma once

#include "audio.h"
#include "cpu.h"
#include "io.h"
#include "keyboard.h"
#include "memory.h"
#include "video.h"

#include <SDL.h>

#include <string>

namespace apple2e {

// Main Apple IIe emulator
class Apple2e {
public:
    Apple2e();
    ~Apple2e();

    // Load the ROM and, unless headless, open the window and audio device
    bool init(const std::string& romPath, bool headless = false);

    // Interactive loop at real speed until the window is closed
    void run();

    // Run a number of frames as fast as possible (no window, no audio)
    void runFrames(int frames);

    // Warm reset (CTRL-RESET) or power cycle
    void reset(bool coldStart);

    // Type text as if entered on the keyboard, after `delayFrames` frames
    // (the ROM discards keys pressed while it is still booting)
    void typeText(const std::string& text, int delayFrames = 0);

    // Text screen contents as ASCII
    std::string screenText() const { return m_video.textDump(); }
    bool saveScreenshot(const std::string& path) const { return m_video.saveScreenshot(path); }

private:
    void runFrame();
    void handleEvent(const SDL_Event& event, bool& running);

    // Construction order matters: components reference each other
    uint64_t m_cycles = 0;
    SoftSwitches m_switches;
    KeyboardController m_keyboard;
    AudioController m_audio;
    Memory m_memory;
    IOController m_io;
    CPU m_cpu;
    VideoController m_video;

    std::string m_delayedText;
    int m_typeDelayFrames = 0;

    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    bool m_sdlInitialized = false;
};

} // namespace apple2e
