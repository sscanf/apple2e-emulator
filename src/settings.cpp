#include "settings.h"

#include <SDL.h>

#include <fstream>
#include <string>

namespace apple2e {

std::string Settings::folder() {
    char* dir = SDL_GetPrefPath("apple2e-emulator", "Apple IIe");
    if (!dir) return {};
    std::string path(dir);
    SDL_free(dir);
    return path;
}

std::string Settings::defaultPath() {
    std::string dir = folder();
    return dir.empty() ? dir : dir + "settings.ini";
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
        else if (key == "crt") crtMonitor = value == "on";
        else if (key == "crt_curvature") toInt(crtCurvature);
        else if (key == "drive_sounds") driveSounds = value != "off";
        else if (key == "motor_volume") toInt(motorVolume);
        else if (key == "head_volume") toInt(headVolume);
        else if (key == "disk_directory") diskDirectory = value;
        else if (key == "state_directory") stateDirectory = value;
        else if (key == "window_x") toInt(windowX);
        else if (key == "window_y") toInt(windowY);
        else if (key == "window_width") toInt(windowW);
        else if (key == "window_height") toInt(windowH);
    }
}

bool Settings::save(const std::string& path) const {
    std::ofstream file(path, std::ios::trunc);
    file << "monitor=" << (greenMonitor ? "green" : "color") << '\n'
         << "crt=" << (crtMonitor ? "on" : "off") << '\n'
         << "crt_curvature=" << crtCurvature << '\n'
         << "drive_sounds=" << (driveSounds ? "on" : "off") << '\n'
         << "motor_volume=" << motorVolume << '\n'
         << "head_volume=" << headVolume << '\n'
         << "disk_directory=" << diskDirectory << '\n'
         << "state_directory=" << stateDirectory << '\n'
         << "window_x=" << windowX << '\n'
         << "window_y=" << windowY << '\n'
         << "window_width=" << windowW << '\n'
         << "window_height=" << windowH << '\n';
    return static_cast<bool>(file);
}

} // namespace apple2e
