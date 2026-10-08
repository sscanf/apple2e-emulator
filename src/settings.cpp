#include "settings.h"

#include <SDL.h>

#include <fstream>
#include <string>

namespace apple2e {

std::string Settings::defaultPath() {
    char* dir = SDL_GetPrefPath("apple2e-emulator", "Apple IIe");
    if (!dir) return {};
    std::string path = std::string(dir) + "settings.ini";
    SDL_free(dir);
    return path;
}

void Settings::load(const std::string& path) {
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        auto toInt = [&](int& out) {
            try {
                out = std::stoi(value);
            } catch (...) {
                // keep the default
            }
        };

        if (key == "monitor") greenMonitor = value == "green";
        else if (key == "drive_sounds") driveSounds = value != "off";
        else if (key == "disk_directory") diskDirectory = value;
        else if (key == "window_x") toInt(windowX);
        else if (key == "window_y") toInt(windowY);
        else if (key == "window_width") toInt(windowW);
        else if (key == "window_height") toInt(windowH);
    }
}

bool Settings::save(const std::string& path) const {
    std::ofstream file(path, std::ios::trunc);
    file << "monitor=" << (greenMonitor ? "green" : "color") << '\n'
         << "drive_sounds=" << (driveSounds ? "on" : "off") << '\n'
         << "disk_directory=" << diskDirectory << '\n'
         << "window_x=" << windowX << '\n'
         << "window_y=" << windowY << '\n'
         << "window_width=" << windowW << '\n'
         << "window_height=" << windowH << '\n';
    return static_cast<bool>(file);
}

} // namespace apple2e
