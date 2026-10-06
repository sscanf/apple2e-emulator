#include "apple2e.h"

#include <cstdio>
#include <iostream>

namespace apple2e {

Apple2e::Apple2e()
    : m_memory(m_switches),
      m_io(m_switches, m_keyboard, m_audio, m_cycles),
      m_cpu(m_memory),
      m_video(m_memory, m_switches) {
    m_memory.setIO(&m_io);
}

Apple2e::~Apple2e() {
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window) SDL_DestroyWindow(m_window);
    if (m_sdlInitialized) SDL_Quit();
}

bool Apple2e::init(const std::string& romPath, bool headless) {
    if (!m_memory.loadRom(romPath)) return false;

    if (!headless) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << std::endl;
            return false;
        }
        m_sdlInitialized = true;

        m_window = SDL_CreateWindow("Apple IIe", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    VideoController::kWidth * 3 / 2, VideoController::kHeight * 3,
                                    SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (!m_window) {
            std::cerr << "SDL window creation failed: " << SDL_GetError() << std::endl;
            return false;
        }

        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
        if (!m_renderer) {
            std::cerr << "SDL renderer creation failed: " << SDL_GetError() << std::endl;
            return false;
        }

        if (!m_video.init(m_renderer)) {
            std::cerr << "Video init failed: " << SDL_GetError() << std::endl;
            return false;
        }
        m_audio.init();
        SDL_StartTextInput();
    }

    reset(true);
    return true;
}

void Apple2e::reset(bool coldStart) {
    if (coldStart) {
        // Zeroed RAM invalidates the power-up byte at $03F4, so the ROM cold starts
        m_memory.clearRam();
        m_switches = SoftSwitches{};
        m_keyboard.reset();
    }
    m_switches.resetMMU();
    m_cpu.reset();
}

void Apple2e::typeText(const std::string& text, int delayFrames) {
    m_delayedText = text;
    m_typeDelayFrames = delayFrames;
}

void Apple2e::runFrame() {
    if (!m_delayedText.empty() && m_typeDelayFrames-- <= 0) {
        m_keyboard.queueText(m_delayedText);
        m_delayedText.clear();
    }
    m_keyboard.update();

    uint64_t frameEnd = m_cycles + kCyclesPerFrame;
    while (m_cycles < frameEnd) {
        m_cycles += m_cpu.step();
    }

    m_audio.endFrame(m_cycles);
    m_video.renderFrame();
}

void Apple2e::runFrames(int frames) {
    for (int i = 0; i < frames; i++) runFrame();
}

void Apple2e::handleEvent(const SDL_Event& event, bool& running) {
    if (event.type == SDL_QUIT) {
        running = false;
        return;
    }

    if (event.type == SDL_KEYDOWN) {
        SDL_Keycode key = event.key.keysym.sym;
        SDL_Keymod mod = static_cast<SDL_Keymod>(event.key.keysym.mod);

        // Emulator hotkeys; everything else goes to the Apple keyboard
        if (key == SDLK_F12) {
            reset(mod & KMOD_SHIFT);
            return;
        }
        if ((mod & KMOD_GUI) && key == SDLK_v) {
            if (char* clip = SDL_GetClipboardText()) {
                m_keyboard.queueText(clip);
                SDL_free(clip);
            }
            return;
        }
        if ((mod & KMOD_GUI) && key == SDLK_q) {
            running = false;
            return;
        }
    }

    m_keyboard.handleEvent(event);
}

void Apple2e::run() {
    std::cout << "F12: RESET   Shift+F12: reboot   Cmd+V: paste   Cmd+Q / close window: quit\n";

    const double counterHz = static_cast<double>(SDL_GetPerformanceFrequency());
    const double frameSeconds = kCyclesPerFrame / kCpuClockHz;
    double nextFrame = SDL_GetPerformanceCounter() / counterHz;

    uint64_t fpsStart = SDL_GetPerformanceCounter();
    int fpsFrames = 0;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) handleEvent(event, running);

        runFrame();
        m_video.present(m_renderer);

        // Pace to the real machine's ~59.92 Hz frame rate
        nextFrame += frameSeconds;
        double now = SDL_GetPerformanceCounter() / counterHz;
        if (nextFrame > now) {
            SDL_Delay(static_cast<uint32_t>((nextFrame - now) * 1000.0));
        } else if (now - nextFrame > 0.25) {
            nextFrame = now;  // fell far behind (e.g. window dragged); don't try to catch up
        }

        fpsFrames++;
        uint64_t counter = SDL_GetPerformanceCounter();
        double elapsed = (counter - fpsStart) / counterHz;
        if (elapsed >= 1.0) {
            char title[64];
            std::snprintf(title, sizeof(title), "Apple IIe  |  %.1f fps", fpsFrames / elapsed);
            SDL_SetWindowTitle(m_window, title);
            fpsStart = counter;
            fpsFrames = 0;
        }
    }
}

} // namespace apple2e
