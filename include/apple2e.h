#pragma once

#include "command.h"
#include "menubar.h"
#include "audio.h"
#include "cpu.h"
#include "crt.h"
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
    bool init(const std::string &romPath, const std::string &diskRomPath, bool headless = false);

    // Use a real character generator ROM for text (see VideoController)
    bool loadCharacterRom(const std::string &path) { return m_video.loadCharacterRom(path); }

    // Colour or green-phosphor monitor
    void setMonochrome(bool on) { m_video.setMonochrome(on); }

    // Apple Monitor II look (CRT effects in the monitor's case) or a flat screen
    void setCrt(bool on);
    bool crt() const { return m_crt; }

    // Hide the side panel (drives and controls), leaving only the screen
    // (`animate` slides it under the monitor instead of jumping)
    void setPanelHidden(bool hidden, bool animate = false);
    bool panelHidden() const { return m_panelHidden; }
    // Curvature of the CRT glass, 0 (flat) to kMaxCurvature
    static constexpr float kMaxCurvature = 0.10f;
    void                   setCurvature(float curvature);
    float                  curvature() const { return m_curvature; }
    // Monitor case artwork for the CRT look (monitor2.png in `directory`)
    bool loadMonitorImage(const std::string &directory);

    // Pictures of the disk drives for the side panel (see SidePanel)
    bool loadDriveImages(const std::string &directory) { return m_sidePanel->loadDriveImages(directory); }

    // Load Disk II mechanical sound samples (motor, head steps) from a folder
    void loadDriveSounds(const std::string &directory);

    // Insert a disk image into drive 0/1; returns an error message on failure
    std::string insertDisk(int drive, const std::string &path);
    // Put back the disks that were in the drives when the last session ended
    void insertLastDisks();

    // Interactive loop at real speed until the window is closed
    void run();

    // Run a number of frames as fast as possible (no window, no audio)
    void runFrames(int frames);

    // Warm reset (CTRL-RESET) or power cycle
    void reset(bool coldStart);

    // Type text as if entered on the keyboard, after `delayFrames` frames
    // (the ROM discards keys pressed while it is still booting)
    void typeText(const std::string &text, int delayFrames = 0);

    // Save states: the whole machine (CPUs, memory, cards, disks with their
    // contents). Both return an error message, empty on success; a failed
    // load leaves the machine as it was.
    std::string saveState(const std::string &path);
    std::string loadState(const std::string &path);
    // Quick slot in the settings folder (Cmd+S / Cmd+L)
    static std::string quickStatePath();

    // Text screen contents as ASCII
    std::string screenText() const { return m_video.textDump(); }
    // Save the screen and disk panel as a BMP
    // `scale` multiplies the resolution (the window's logical size is 1)
    bool saveScreenshot(const std::string &path, int scale = 1) const;
    // The same picture as a new ARGB8888 surface (caller frees); null on error
    SDL_Surface *renderScreenshot(int scale) const;

private:
    void runFrame();
    // Draw screen (flat or CRT) and side panel with `renderer`
    void compose(SDL_Renderer *renderer, SDL_Texture *frame, CrtDisplay *crt) const;
    // Screen, panel and window size for the current monitor style
    SDL_Rect  screenRect() const;
    int       panelX() const;
    SDL_Point logicalSize() const;
    void      relayout(SDL_Point oldSize);
    // `fromMenu` tells the two ways a command can arrive apart (see .cpp)
    void       runCommand(Command command, bool fromMenu);
    bool       commandIsOn(Command command) const;
    void       applyLayout(float zoom);
    void       createPanelWindow();
    void       layoutPanelWindow();
    void       placePanelWindow();
    void       placeWindowsAbove();
    void       setPanelWindowsParked(bool parked);
    static int watchMainWindow(void *userdata, SDL_Event *event);
    bool       overPanelHandle(const SDL_Event &event, int x, int y) const;
    float      panelTravel() const;
    bool       handlePanelHandle(const SDL_Event &event);
    SDL_Rect   panelHandleRect() const;
    void       drawPanelHandle(SDL_Renderer *renderer) const;
    float      zoomFor(SDL_Point oldSize, SDL_Point newSize) const;
    void       updatePanelSlide(float seconds);
    void       writeState(StateWriter &w) const;
    void       readState(StateReader &r);
    void       quickSave();
    void       quickLoad();
    // Save/load to a path, reporting the outcome in the side panel
    void saveStateTo(const std::string &path);
    void loadStateFrom(const std::string &path);
    void handleEvent(const SDL_Event &event, bool &running);

    // Construction order matters: components reference each other
    uint64_t                   m_cycles = 0;
    SoftSwitches               m_switches;
    KeyboardController         m_keyboard;
    AudioController            m_audio;
    GameIO                     m_gameIO;
    Memory                     m_memory;
    IOController               m_io;
    CPU                        m_cpu;
    VideoController            m_video;
    SoftCard                   m_softCard;
    MouseCard                  m_mouseCard;
    Disk2Controller            m_disk2;
    bool                       m_hasDisk2 = false;
    std::unique_ptr<SidePanel> m_sidePanel;
    bool                       m_crt         = false;
    bool                       m_panelHidden = false;
    float                      m_panelShown  = 1.0f; // visible fraction of the side panel
    float                      m_slideT      = 1.0f; // the same before easing
    // Where the system allows it (macOS), the panel has its own window behind
    // the main one, and slides out from under it
    bool          m_drawer           = false;
    SDL_Window   *m_panelWindow      = nullptr;
    SDL_Renderer *m_panelRenderer    = nullptr;
    bool          m_panelTransparent = false; // only the handle shows beyond the panel
    // X11 and Windows: the panel's window is in front and beside the main
    // one, cut to the part that shows, and the handle has its own window
    bool                     m_panelAbove         = false;
    bool                     m_panelWindowShown   = true;
    int                      m_inputSourceFrames  = 0;     // frames left to put the keyboard layout back
    bool                     m_panelWindowsParked = false; // hidden with the minimised main window
    SDL_Window              *m_handleWindow       = nullptr;
    SDL_Renderer            *m_handleRenderer     = nullptr;
    std::unique_ptr<MenuBar> m_menuBar;                 // in the window, where menus go there
    Uint32                   m_menuEvent           = 0; // SDL event type of menu bar choices
    Command                  m_lastCommand         = Command::Reset;
    bool                     m_lastCommandFromMenu = false;
    Uint32                   m_lastCommandTicks    = 0;
    // Dragging the panel by its handle
    bool                        m_handleDragged  = false;
    bool                        m_handleMoved    = false;
    int                         m_dragStartX     = 0; // pointer, in screen coordinates
    float                       m_dragStartShown = 0.0f;
    SDL_Cursor                 *m_resizeCursor   = nullptr;
    SDL_Cursor                 *m_arrowCursor    = nullptr;
    bool                        m_panelSliding   = false;
    float                       m_curvature      = 0.03f;
    SDL_Surface                *m_bezel          = nullptr; // monitor case artwork
    std::unique_ptr<CrtDisplay> m_crtDisplay;               // for the window's renderer
    DriveSounds                 m_driveSounds;
    bool                        m_driveSoundsLoaded = false;

    std::string m_delayedText;
    int         m_typeDelayFrames = 0;

    // Preferences restored at start-up and saved on exit (not when headless)
    Settings    m_settings;
    std::string m_settingsPath;
    void        applySettings();
    void        saveSettings();

    SDL_Window   *m_window         = nullptr;
    SDL_Renderer *m_renderer       = nullptr;
    bool          m_sdlInitialized = false;
};

} // namespace apple2e
