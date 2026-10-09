#define SDL_MAIN_HANDLED
#include "apple2e.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr const char* kDefaultRom = "apple2e.rom";
constexpr const char* kDiskRom = "disk2.rom";
constexpr const char* kVideoRom = "video.rom";  // optional character generator

constexpr const char* kSoundsDir = "sounds";              // your own recordings (not in git)
constexpr const char* kBundledSoundsDir = "assets/sounds"; // shipped with the repository
constexpr const char* kDriveImagesDir = "assets/drives";
constexpr const char* kMonitorImageDir = "assets/monitor";

// Look for a support file or folder in the current directory, next to the
// executable, and one level above it (the project root for build/ trees).
// Empty if absent.
std::string findFile(const std::string& name) {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates = {name};
    if (char* base = SDL_GetBasePath()) {
        fs::path exeDir(base);  // ends with a separator, so parent_path() is the dir itself
        SDL_free(base);
        candidates.push_back(exeDir / name);
        candidates.push_back(exeDir.parent_path().parent_path() / name);
    }

    std::error_code ec;
    for (const auto& path : candidates) {
        if (fs::exists(path, ec)) return path.string();
    }
    return {};
}

void usage(const char* argv0) {
    std::cerr << "usage: " << argv0 << " [rom] [--disk1 FILE] [--disk2 FILE] [--headless FRAMES]\n"
              << "       [--type TEXT] [--screenshot FILE]\n"
              << "  rom                 Apple IIe ROM image (default: apple2e.rom here,\n"
              << "                      next to the executable or in its parent folder)\n"
              << "  --disk1/--disk2 F   insert a disk image (.dsk/.do/.po/.nib) in drive 1/2;\n"
              << "                      needs disk2.rom (Disk II boot ROM), looked up like the ROM\n"
              << "  --green             start with a green-phosphor monitor (toggle with Cmd+G;\n"
              << "                      otherwise the monitor last used is restored)\n"
              << "  --crt               Apple Monitor II look with CRT effects (Cmd+M)\n"
              << "  --load-state FILE   start from a save state\n"
              << "  --save-state FILE   with --headless, save the state at the end\n"
              << "  --headless FRAMES   run without a window and print the text screen\n"
              << "  --type TEXT         type TEXT one second after boot (newlines become RETURN,\n"
              << "                      \\x10 pauses half a second)\n"
              << "  --type-delay FRAMES frames to wait before typing (default 60; 60 per second)\n"
              << "  --screenshot FILE   with --headless, also save the final frame as BMP\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::string romPath;
    std::string typed;
    std::string screenshotPath;
    std::string loadStatePath;
    std::string saveStatePath;
    std::string diskPaths[2];
    int headlessFrames = -1;
    int typeDelayFrames = 60;
    bool green = false;
    bool crt = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--headless" && i + 1 < argc) {
            headlessFrames = std::atoi(argv[++i]);
        } else if (arg == "--type" && i + 1 < argc) {
            typed = argv[++i];
        } else if ((arg == "--disk1" || arg == "--disk2") && i + 1 < argc) {
            diskPaths[arg == "--disk1" ? 0 : 1] = argv[++i];
        } else if (arg == "--type-delay" && i + 1 < argc) {
            typeDelayFrames = std::atoi(argv[++i]);
        } else if (arg == "--green") {
            green = true;
        } else if (arg == "--crt") {
            crt = true;
        } else if (arg == "--load-state" && i + 1 < argc) {
            loadStatePath = argv[++i];
        } else if (arg == "--save-state" && i + 1 < argc) {
            saveStatePath = argv[++i];
        } else if (arg == "--screenshot" && i + 1 < argc) {
            screenshotPath = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            usage(argv[0]);
            return 0;
        } else if (!arg.empty() && arg[0] == '-') {
            usage(argv[0]);
            return 1;
        } else {
            romPath = arg;
        }
    }

    if (romPath.empty()) romPath = findFile(kDefaultRom);
    if (romPath.empty()) romPath = kDefaultRom;  // let init report it missing

    apple2e::Apple2e emulator;
    if (!emulator.init(romPath, findFile(kDiskRom), headlessFrames >= 0)) {
        std::cerr << "Failed to initialize emulator." << std::endl;
        return 1;
    }
    if (green) emulator.setMonochrome(true);  // otherwise the saved preference applies
    if (std::string dir = findFile(kDriveImagesDir); !dir.empty()) emulator.loadDriveImages(dir);
    if (std::string dir = findFile(kMonitorImageDir); !dir.empty()) emulator.loadMonitorImage(dir);
    if (crt) emulator.setCrt(true);
    if (std::string videoRom = findFile(kVideoRom); !videoRom.empty()) emulator.loadCharacterRom(videoRom);
    for (int drive = 0; drive < 2; drive++) {
        if (diskPaths[drive].empty()) continue;
        std::string error = emulator.insertDisk(drive, diskPaths[drive]);
        if (!error.empty()) std::cerr << error << std::endl;
    }
    if (!loadStatePath.empty()) {
        std::string error = emulator.loadState(loadStatePath);
        if (!error.empty()) {
            std::cerr << error << std::endl;
            return 1;
        }
    }
    emulator.typeText(typed, typeDelayFrames);

    if (headlessFrames >= 0) {
        emulator.runFrames(headlessFrames);
        std::cout << emulator.screenText();
        if (!saveStatePath.empty()) {
            std::string error = emulator.saveState(saveStatePath);
            if (!error.empty()) {
                std::cerr << error << std::endl;
                return 1;
            }
        }
        if (!screenshotPath.empty() && !emulator.saveScreenshot(screenshotPath)) {
            std::cerr << "Failed to save screenshot: " << SDL_GetError() << std::endl;
            return 1;
        }
    } else {
        std::string soundsDir = findFile(kSoundsDir);
        if (soundsDir.empty()) soundsDir = findFile(kBundledSoundsDir);
        if (!soundsDir.empty()) emulator.loadDriveSounds(soundsDir);
        emulator.run();
    }
    return 0;
}
