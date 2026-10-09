#pragma once

#include <string>

namespace apple2e {

// User preferences kept between runs, stored as key=value lines in the
// per-user settings folder (~/Library/Application Support/... on macOS)
struct Settings {
    bool greenMonitor = false;
    bool crtMonitor = false;  // Apple Monitor II with CRT effects
    int crtCurvature = 30;    // percent of the maximum glass curvature
    bool driveSounds = true;
    int motorVolume = 100;  // percent
    int headVolume = 100;
    std::string diskDirectory;   // where the disk file dialog opens
    std::string stateDirectory;  // where the save state dialogs open
    int windowX = 0, windowY = 0, windowW = 0, windowH = 0;  // size 0: default

    // Per-user folder for settings and quick save states (ends with a
    // separator), or empty if the platform gives none
    static std::string folder();
    // Settings file path, or empty if there is no settings folder
    static std::string defaultPath();

    // Missing or unreadable files leave the defaults in place
    void load(const std::string& path);
    bool save(const std::string& path) const;
};

} // namespace apple2e
