#pragma once

#include <SDL.h>

#include <atomic>
#include <memory>
#include <string>

namespace apple2e {

class Disk2Controller;
class VideoController;

// Side panel next to the screen: the two Disk II drives (click a drive to
// insert a disk, right-click to eject, or drop disk images onto it) and the
// colour / green monitor switch
class SidePanel {
public:
    static constexpr int kWidth = 160;

    // `controller` may be null when no Disk II ROM is available
    SidePanel(Disk2Controller* controller, VideoController& video, int x, int height);

    void draw(SDL_Renderer* renderer) const;

    // Returns true if the event was handled by the panel
    bool handleEvent(const SDL_Event& event, SDL_Renderer* renderer);

    // Open the system file dialog for `drive`. It runs on its own thread so
    // emulation (and drive sound) carries on; the disk is inserted by update()
    void chooseDisk(int drive);

    // Call once per frame: inserts the disk once the file dialog has closed
    void update();

private:
    SDL_Rect driveRect(int drive) const;
    int driveAt(int x, int y) const;
    void insert(int drive, const std::string& path);
    void eject(int drive);
    void drawDrive(SDL_Renderer* renderer, int drive) const;
    SDL_Rect monitorSwitchRect() const;
    void drawMonitorSwitch(SDL_Renderer* renderer) const;

    // Result of a file dialog running in the background
    struct DialogResult {
        std::atomic<bool> done{false};
        std::string path;
    };

    Disk2Controller* m_controller;
    VideoController& m_video;
    std::shared_ptr<DialogResult> m_dialog;  // non-null while a dialog is open
    int m_dialogDrive = 0;
    int m_x;
    int m_height;
    std::string m_lastDirectory;
    std::string m_message;  // last error, shown at the bottom of the panel
};

} // namespace apple2e
