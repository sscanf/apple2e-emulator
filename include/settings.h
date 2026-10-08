#pragma once

#include <string>

namespace apple2e {

// User preferences kept between runs, stored as key=value lines in the
// per-user settings folder (~/Library/Application Support/... on macOS)
struct Settings {
    bool greenMonitor = false;
    bool driveSounds = true;
    std::string diskDirectory;  // where the disk file dialog opens
    int windowX = 0, windowY = 0, windowW = 0, windowH = 0;  // size 0: default

    // Settings file path, or empty if the platform gives no settings folder
    static std::string defaultPath();

    // Missing or unreadable files leave the defaults in place
    void load(const std::string& path);
    bool save(const std::string& path) const;
};

} // namespace apple2e
