#pragma once

#include <SDL.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace apple2e {

class Disk2Controller;
class DriveSounds;
class VideoController;

// Side panel next to the screen: the two Disk II drives (click an empty
// drive to insert a disk, click a full one to eject it, or drop disk images
// onto a drive), motor/head volume sliders, the save state buttons and the
// colour / green monitor switch
class SidePanel {
public:
    static constexpr int kWidth = 160;

    // `controller` may be null when no Disk II ROM is available
    SidePanel(Disk2Controller* controller, VideoController& video, int x, int height);
    ~SidePanel();

    // Pictures of the drives (open / closed / running, for drive 1 and 2) from
    // `directory`; without them the drives are drawn with plain shapes.
    // Returns false if they could not be loaded (or PNG support is missing).
    bool loadDriveImages(const std::string& directory);
    // Position and height of the panel (they change with the monitor style)
    void setGeometry(int x, int height) {
        m_x = x;
        m_height = height;
    }

    // "CRT" checkbox next to the colour / green switch
    void setCrtActions(std::function<bool()> isOn, std::function<void()> toggle) {
        m_crtIsOn = std::move(isOn);
        m_toggleCrt = std::move(toggle);
    }

    // Renderer whose textures are kept between frames (the window's)
    void setMainRenderer(SDL_Renderer* renderer) { m_mainRenderer = renderer; }

    void draw(SDL_Renderer* renderer) const;

    // Returns true if the event was handled by the panel
    bool handleEvent(const SDL_Event& event, SDL_Renderer* renderer);

    // Open the system file dialog for `drive`. It runs on its own thread so
    // emulation (and drive sound) carries on; the disk is inserted by update()
    void chooseDisk(int drive);

    // Call once per frame: inserts the disk once the file dialog has closed
    void update();

    // Motor and head volume sliders control these sounds
    void setDriveSounds(DriveSounds* sounds) { m_sounds = sounds; }

    // Called when a disk is taken out (for the door sound)
    void setEjectAction(std::function<void()> action) { m_onEject = std::move(action); }

    // Choose a save state file to save to / load from (file dialog in the
    // background, like chooseDisk); the chosen path goes to the state actions
    void chooseStateFile(bool save);

    // What to do with a chosen save state path
    void setStateActions(std::function<void(const std::string&)> save,
                         std::function<void(const std::string&)> load) {
        m_onSaveState = std::move(save);
        m_onLoadState = std::move(load);
    }

    // Status line at the bottom of the panel
    void showMessage(const std::string& text, bool isError) {
        m_message = text;
        m_messageIsError = isError;
    }

    // Folder the file dialog opens in (remembered between runs)
    const std::string& lastDirectory() const { return m_lastDirectory; }
    void setLastDirectory(const std::string& dir) { m_lastDirectory = dir; }
    const std::string& lastStateDirectory() const { return m_lastStateDirectory; }
    void setLastStateDirectory(const std::string& dir) { m_lastStateDirectory = dir; }

private:
    SDL_Rect driveRect(int drive) const;
    int driveAt(int x, int y) const;
    void insert(int drive, const std::string& path);
    void eject(int drive);
    void drawDrive(SDL_Renderer* renderer, int drive) const;
    bool drawDriveImage(SDL_Renderer* renderer, int drive, const SDL_Rect& dst) const;
    void drawDriveShapes(SDL_Renderer* renderer, int drive, int x, int y) const;
    SDL_Rect monitorSwitchRect() const;
    SDL_Rect crtCheckboxRect() const;
    SDL_Rect stateButtonRect(int index) const;  // 0 save, 1 load
    SDL_Rect sliderRect(int index) const;       // 0 motor, 1 head (whole row)
    void setSliderFromX(int index, int x);
    void drawSliders(SDL_Renderer* renderer) const;
    void drawStateButtons(SDL_Renderer* renderer) const;
    void drawMonitorSwitch(SDL_Renderer* renderer) const;

    // Result of a file dialog running in the background
    struct DialogResult {
        std::atomic<bool> done{false};
        std::string path;
    };

    Disk2Controller* m_controller;
    VideoController& m_video;
    std::shared_ptr<DialogResult> m_dialog;  // non-null while a dialog is open
    enum class DialogPurpose { Disk, SaveState, LoadState };
    DialogPurpose m_dialogPurpose = DialogPurpose::Disk;
    int m_dialogDrive = 0;
    int m_x;
    int m_height;
    std::string m_lastDirectory;
    std::string m_lastStateDirectory;
    std::string m_message;  // last status or error, shown at the bottom
    bool m_messageIsError = true;
    std::function<void()> m_onEject;
    std::function<bool()> m_crtIsOn;
    std::function<void()> m_toggleCrt;
    DriveSounds* m_sounds = nullptr;

    // Drive pictures: [drive][open, closed, running]. Textures belong to the
    // main renderer; other renderers (screenshots) get temporary ones.
    enum DriveLook { kOpen, kClosed, kRunning, kLooks };
    SDL_Surface* m_driveImages[2][kLooks] = {};
    mutable SDL_Texture* m_driveTextures[2][kLooks] = {};
    SDL_Renderer* m_mainRenderer = nullptr;
    int m_draggedSlider = -1;
    std::function<void(const std::string&)> m_onSaveState;
    std::function<void(const std::string&)> m_onLoadState;
};

} // namespace apple2e
