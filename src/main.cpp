#define SDL_MAIN_HANDLED
#include "apple2e.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr const char* kDefaultRom = "apple2e.rom";

// Without an explicit path, look for the ROM in the current directory, next to
// the executable, and one level above it (the project root for build/ trees)
std::string findDefaultRom() {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates = {kDefaultRom};
    if (char* base = SDL_GetBasePath()) {
        fs::path exeDir(base);  // ends with a separator, so parent_path() is the dir itself
        SDL_free(base);
        candidates.push_back(exeDir / kDefaultRom);
        candidates.push_back(exeDir.parent_path().parent_path() / kDefaultRom);
    }

    std::error_code ec;
    for (const auto& path : candidates) {
        if (fs::is_regular_file(path, ec)) return path.string();
    }
    return kDefaultRom;
}

void usage(const char* argv0) {
    std::cerr << "usage: " << argv0 << " [rom] [--headless FRAMES] [--type TEXT] [--screenshot FILE]\n"
              << "  rom                 Apple IIe ROM image (default: apple2e.rom here,\n"
              << "                      next to the executable or in its parent folder)\n"
              << "  --headless FRAMES   run without a window and print the text screen\n"
              << "  --type TEXT         type TEXT one second after boot (newlines become RETURN)\n"
              << "  --screenshot FILE   with --headless, also save the final frame as BMP\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::string romPath;
    std::string typed;
    std::string screenshotPath;
    int headlessFrames = -1;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--headless" && i + 1 < argc) {
            headlessFrames = std::atoi(argv[++i]);
        } else if (arg == "--type" && i + 1 < argc) {
            typed = argv[++i];
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

    if (romPath.empty()) romPath = findDefaultRom();

    apple2e::Apple2e emulator;
    if (!emulator.init(romPath, headlessFrames >= 0)) {
        std::cerr << "Failed to initialize emulator." << std::endl;
        return 1;
    }
    constexpr int kBootFrames = 60;
    emulator.typeText(typed, kBootFrames);

    if (headlessFrames >= 0) {
        emulator.runFrames(headlessFrames);
        std::cout << emulator.screenText();
        if (!screenshotPath.empty() && !emulator.saveScreenshot(screenshotPath)) {
            std::cerr << "Failed to save screenshot: " << SDL_GetError() << std::endl;
            return 1;
        }
    } else {
        emulator.run();
    }
    return 0;
}
