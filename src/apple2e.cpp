#include "apple2e.h"

#include <iostream>
#include <cstring>
#include <SDL2/SDL.h>

namespace apple2e {

Apple2e::Apple2e() {}

Apple2e::~Apple2e() {
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window) SDL_DestroyWindow(m_window);
    SDL_Quit();
}

bool Apple2e::init(const std::string& romPath) {
    m_romPath = romPath;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create window
    m_window = SDL_CreateWindow(
        "Apple IIe Emulator",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        840, 600,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!m_window) {
        std::cerr << "SDL window creation failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create renderer
    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_renderer) {
        std::cerr << "SDL renderer creation failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // Initialize hardware components
    initMemory(romPath);
    initCPU();
    initIO();
    initVideo();
    initAudio();
    initKeyboard();

    // Reset CPU (reads reset vector from ROM)
    m_cpu.reset();

    m_running = true;
    m_frameStart = SDL_GetPerformanceCounter();
    m_frameCount = 0;

    std::cout << "Apple IIe emulator initialized." << std::endl;
    std::cout << "Press ESC to quit." << std::endl;

    return true;
}

void Apple2e::run() {
    SDL_Event event;
    bool running = true;

    while (running) {
        // Process events
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_KEYDOWN:
                    m_keyboard.processKey(event.key);
                    if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                        running = false;
                    }
                    if (event.key.keysym.scancode == SDL_SCANCODE_D) {
                        toggleDisplay();
                    }
                    break;

                case SDL_KEYUP:
                    m_keyboard.processKey(event.key);
                    break;

                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                        running = false;
                    }
                    break;
            }
        }

        // Run CPU for ~1 frame (60Hz)
        // Apple IIe runs at ~1.023 MHz
        // ~17,000 cycles per frame at 60Hz
        constexpr uint32_t kCyclesPerFrame = 17048;
        m_cpu.step();  // Single step for now

        // Run remaining cycles (simplified - in a real emulator, we'd sync to video)
        for (uint32_t i = 1; i < kCyclesPerFrame; i++) {
            m_cpu.step();
        }

        // Render
        m_video.render(m_renderer);

        // Calculate FPS
        m_frameCount++;
        auto now = SDL_GetPerformanceCounter();
        double elapsed = static_cast<double>(now - m_frameStart) / SDL_GetPerformanceFrequency();
        if (elapsed >= 1.0) {
            m_fps = m_frameCount / elapsed;
            m_frameCount = 0;
            m_frameStart = now;
        }

        // Update window title with FPS
        char title[128];
        snprintf(title, sizeof(title),
            "Apple IIe Emulator | FPS: %.1f | PC: %04X | Cycles: %llu",
            m_fps, m_cpu.pc(), m_cycles);
        SDL_SetWindowTitle(m_window, title);
    }

    m_running = false;
}

void Apple2e::stop() {
    m_running = false;
}

void Apple2e::toggleDisplay() {
    m_video.setDisplayOn(!m_video.isDisplayOn());
}

// ============================================================
// Memory/I/O Callbacks
// ============================================================

uint8_t Apple2e::memoryReadCallback(uint16_t addr) {
    // Apple IIe memory map handling

    // I/O space: $C000-$C0FF (takes precedence over ROM)
    if (addr >= 0xC000 && addr <= 0xC0FF) {
        return m_io.read(addr);
    }

    // ROM space: $C100-$FFFF (Apple IIe has 32KB ROM from $C000)
    const auto& romData = m_memory.getRomData();
    if (!romData.empty()) {
        size_t romSize = romData.size();
        uint16_t romAddr = addr;
        if (romAddr < romSize) {
            return romData[romAddr];
        }
    }

    // Check if address is in a banked region
    // $4000-$BFFF can be banked
    if (addr >= 0x4000 && addr <= 0xBFFF) {
        const uint8_t* mem = m_memory.getMemory();
        if (mem) {
            return mem[addr];
        }
    }

    // Main RAM: $0000-$3FFF, $C100-$DFFF
    const uint8_t* mem = m_memory.getMemory();
    if (mem) {
        return mem[addr];
    }

    return 0;
}

void Apple2e::memoryWriteCallback(uint16_t addr, uint8_t val) {
    // I/O space: $C000-$C0FF
    if (addr >= 0xC000 && addr <= 0xC0FF) {
        m_io.write(addr, val);
        return;
    }

    // ROM space: read-only
    if (addr >= 0xE000 && addr <= 0xFFFF) {
        return;
    }

    // Main RAM
    uint8_t* mem = m_memory.getMemory();
    if (mem) {
        mem[addr] = val;
    }
}

void Apple2e::cycleCallback(uint32_t cycles) {
    m_cycles += cycles;
    m_io.cycle(cycles);
}

// ============================================================
// Initialization
// ============================================================

void Apple2e::initMemory(const std::string& romPath) {
    // Load ROM
    if (!m_memory.loadRom(romPath)) {
        std::cerr << "Warning: Failed to load ROM, using empty memory" << std::endl;
    }
}

void Apple2e::initCPU() {
    m_cpu.setReadCallback([this](uint16_t addr) {
        return this->memoryReadCallback(addr);
    });
    m_cpu.setWriteCallback([this](uint16_t addr, uint8_t val) {
        this->memoryWriteCallback(addr, val);
    });
    m_cpu.setCycleCallback([this](uint32_t cycles) {
        this->cycleCallback(cycles);
    });
}

void Apple2e::initIO() {
    // Set up I/O controller
    m_io.setVia1(&m_via1);
    m_io.setVia2(&m_via2);
    m_io.setPIA(&m_pia);

    // Configure VIA1 (keyboard, cassette, speaker)
    // Port A: keyboard data (input)
    m_via1.setPortAReadCallback([this]() {
        return this->via1PortAReadCallback();
    });

    // Port B: speaker control (output)
    m_via1.setPortBWriteCallback([this](uint8_t val) {
        this->via1PortBWriteCallback(val);
    });

    // Configure PIA (keyboard matrix)
    // Port A: keyboard matrix rows (output)
    // Port B: keyboard matrix columns (input)
    m_pia.setPortBReadCallback([this]() {
        return this->piaPortBReadCallback();
    });

    // Set memory callbacks for I/O
    m_memory.setIoReadCallback([this](uint16_t addr) {
        return this->m_io.read(addr);
    });
    m_memory.setIoWriteCallback([this](uint16_t addr, uint8_t val) {
        this->m_io.write(addr, val);
    });
}

void Apple2e::initVideo() {
    m_video.init(m_window);
    m_video.setMode(VideoMode::TEXT_40);

    // Set memory read callback for video
    m_video.setMemoryReadCallback([this](uint16_t addr) {
        return this->memoryReadCallback(addr);
    });
}

void Apple2e::initAudio() {
    m_audio.init(44100.0f);
}

void Apple2e::initKeyboard() {
    // PIA Port A controls keyboard matrix row strobes
    m_pia.setPortAWriteCallback([this](uint8_t val) {
        // Row strobe output
        (void)val;
    });

    // VIA1 Port A reads keyboard data register
    // (character buffer from keyboard)
}

uint8_t Apple2e::via1PortAReadCallback() {
    // Read keyboard character from buffer
    if (m_keyboard.hasChar()) {
        return m_keyboard.readChar();
    }
    return 0xFF;  // No key pressed (floating bus)
}

void Apple2e::via1PortBWriteCallback(uint8_t val) {
    // Port B bit 7 controls speaker relay
    bool speakerOn = val & 0x80;
    m_audio.setSpeakerState(speakerOn);
}

uint8_t Apple2e::piaPortBReadCallback() {
    // Read keyboard matrix columns
    // The PIA scans rows and reads columns
    // For simplicity, check if any key is pressed
    uint8_t result = 0xFF;  // Default: no keys pressed

    // Check keyboard matrix
    for (uint8_t row = 0; row < 7; row++) {
        uint8_t rowData = m_keyboard.readMatrixRow(row);
        result &= rowData;
    }

    return result;
}

} // namespace apple2e
