#pragma once

#include "audio.h"
#include "cpu.h"
#include "disk2.h"
#include "sidepanel.h"
#include "softcard.h"
#include "drivesounds.h"
#include "gameio.h"
#include "io.h"
#include "keyboard.h"
#include "memory.h"
#include "mousecard.h"
#include "video.h"

#include <SDL.h>

#include <memory>
#include <string>

namespace apple2e {

// Main Apple IIe emulator
class Apple2e {
public:
    Apple2e();
    ~Apple2e();

    // Load the ROMs and, unless headless, open the window and audio device.
    // Without a Disk II ROM the machine runs with no disk controller.
    bool init(const std::string& romPath, const std::string& diskRomPath, bool headless = false);

    // Colour or green-phosphor monitor
    void setMonochrome(bool on) { m_video.setMonochrome(on); }

    // Load Disk II mechanical sound samples (motor, head steps) from a folder
    void loadDriveSounds(const std::string& directory);

    // Insert a disk image into drive 0/1; returns an error message on failure
    std::string insertDisk(int drive, const std::string& path);

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
    // Save the screen and disk panel as a BMP
    bool saveScreenshot(const std::string& path) const;

private:
    void runFrame();
    void handleEvent(const SDL_Event& event, bool& running);

    // Construction order matters: components reference each other
    uint64_t m_cycles = 0;
    SoftSwitches m_switches;
    KeyboardController m_keyboard;
    AudioController m_audio;
    GameIO m_gameIO;
    Memory m_memory;
    IOController m_io;
    CPU m_cpu;
    VideoController m_video;
    SoftCard m_softCard;
    MouseCard m_mouseCard;
    Disk2Controller m_disk2;
    bool m_hasDisk2 = false;
    std::unique_ptr<SidePanel> m_sidePanel;
    DriveSounds m_driveSounds;
    bool m_driveSoundsLoaded = false;

    std::string m_delayedText;
    int m_typeDelayFrames = 0;

    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    bool m_sdlInitialized = false;
};

} // namespace apple2e
