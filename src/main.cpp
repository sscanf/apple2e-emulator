#include "apple2e.h"
#include <iostream>
#include <string>
#include <SDL2/SDL.h>

int main(int argc, char* argv[]) {
    std::string romPath = "apple2e.rom";

    // Check for ROM file argument
    if (argc > 1) {
        romPath = argv[1];
    }

    std::cout << "=== Apple IIe Emulator ===" << std::endl;
    std::cout << "ROM: " << romPath << std::endl;
    std::cout << std::endl;

    apple2e::Apple2e emulator;

    if (!emulator.init(romPath)) {
        std::cerr << "Failed to initialize emulator." << std::endl;
        return 1;
    }

    emulator.run();

    std::cout << "\nEmulator stopped." << std::endl;
    return 0;
}
