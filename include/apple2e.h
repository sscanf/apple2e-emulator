#pragma once

#include "audio.h"
#include "cpu.h"
#include "disk2.h"
#include "sidepanel.h"
#include "softcard.h"
#include "state.h"
#include "drivesounds.h"
#include "gameio.h"
#include "io.h"
#include "keyboard.h"
#include "memory.h"
#include "mousecard.h"
#include "settings.h"
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

    // Use a real character generator ROM for text (see VideoController)
    bool loadCharacterRom(const std::string& path) { return m_video.loadCharacterRom(path); }

    // Colour or green-phosphor monitor
    void setMonochrome(bool on) { m_video.setMonochrome(on); }

    // Pictures of the disk drives for the side panel (see SidePanel)
    bool loadDriveImages(const std::string& directory) { return m_sidePanel->loadDriveImages(directory); }

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

    // Save states: the whole machine (CPUs, memory, cards, disks with their
    // contents). Both return an error message, empty on success; a failed
    // load leaves the machine as it was.
    std::string saveState(const std::string& path);
    std::string loadState(const std::string& path);
    // Quick slot in the settings folder (Cmd+S / Cmd+L)
    static std::string quickStatePath();

    // Text screen contents as ASCII
    std::string screenText() const { return m_video.textDump(); }
    // Save the screen and disk panel as a BMP
    bool saveScreenshot(const std::string& path) const;

private:
    void runFrame();
    void writeState(StateWriter& w) const;
    void readState(StateReader& r);
    void quickSave();
    void quickLoad();
    // Save/load to a path, reporting the outcome in the side panel
    void saveStateTo(const std::string& path);
    void loadStateFrom(const std::string& path);
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

    // Preferences restored at start-up and saved on exit (not when headless)
    Settings m_settings;
    std::string m_settingsPath;
    void applySettings();
    void saveSettings();

    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    bool m_sdlInitialized = false;
};

} // namespace apple2e
