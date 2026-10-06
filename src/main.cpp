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

constexpr const char* kSoundsDir = "sounds";

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
              << "  --headless FRAMES   run without a window and print the text screen\n"
              << "  --type TEXT         type TEXT one second after boot (newlines become RETURN)\n"
              << "  --type-delay FRAMES frames to wait before typing (default 60; 60 per second)\n"
              << "  --screenshot FILE   with --headless, also save the final frame as BMP\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::string romPath;
    std::string typed;
    std::string screenshotPath;
    std::string diskPaths[2];
    int headlessFrames = -1;
    int typeDelayFrames = 60;

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
    for (int drive = 0; drive < 2; drive++) {
        if (diskPaths[drive].empty()) continue;
        std::string error = emulator.insertDisk(drive, diskPaths[drive]);
        if (!error.empty()) std::cerr << error << std::endl;
    }
    emulator.typeText(typed, typeDelayFrames);

    if (headlessFrames >= 0) {
        emulator.runFrames(headlessFrames);
        std::cout << emulator.screenText();
        if (!screenshotPath.empty() && !emulator.saveScreenshot(screenshotPath)) {
            std::cerr << "Failed to save screenshot: " << SDL_GetError() << std::endl;
            return 1;
        }
    } else {
        std::string soundsDir = findFile(kSoundsDir);
        if (!soundsDir.empty()) emulator.loadDriveSounds(soundsDir);
        emulator.run();
    }
    return 0;
}
