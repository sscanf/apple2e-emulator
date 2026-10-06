#pragma once

#include <SDL.h>

#include <string>

namespace apple2e {

class Disk2Controller;

// Side panel showing the two Disk II drives: click a drive to insert a disk,
// right-click to eject, or drop disk images onto it
class DiskPanel {
public:
    static constexpr int kWidth = 160;

    // `controller` may be null when no Disk II ROM is available
    DiskPanel(Disk2Controller* controller, int x, int height);

    void draw(SDL_Renderer* renderer) const;

    // Returns true if the event was handled by the panel
    bool handleEvent(const SDL_Event& event, SDL_Renderer* renderer);

    // Ask for a disk image with the system file dialog and insert it
    void chooseDisk(int drive);

private:
    SDL_Rect driveRect(int drive) const;
    int driveAt(int x, int y) const;
    void insert(int drive, const std::string& path);
    void eject(int drive);
    void drawDrive(SDL_Renderer* renderer, int drive) const;

    Disk2Controller* m_controller;
    int m_x;
    int m_height;
    std::string m_lastDirectory;
    std::string m_message;  // last error, shown at the bottom of the panel
};

} // namespace apple2e
