#include "apple2e.h"

#include <cstdio>
#include <iostream>

namespace apple2e {

namespace {
constexpr int kDiskSlot = 6;
constexpr int kScreenWidth = VideoController::kWidth;
constexpr int kScreenHeight = VideoController::kHeight * 2;  // scanlines doubled for 4:3
constexpr int kLogicalWidth = kScreenWidth + DiskPanel::kWidth;
}

Apple2e::Apple2e()
    : m_memory(m_switches),
      m_io(m_switches, m_keyboard, m_audio, m_gameIO, m_cycles),
      m_cpu(m_memory),
      m_video(m_memory, m_switches),
      m_disk2(m_cycles) {
    m_memory.setIO(&m_io);
}

Apple2e::~Apple2e() {
    if (m_hasDisk2) m_disk2.flush();
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window) SDL_DestroyWindow(m_window);
    if (m_sdlInitialized) SDL_Quit();
}

bool Apple2e::init(const std::string& romPath, const std::string& diskRomPath, bool headless) {
    if (!m_memory.loadRom(romPath)) return false;

    m_hasDisk2 = !diskRomPath.empty() && m_disk2.loadRom(diskRomPath);
    if (m_hasDisk2) {
        m_memory.setCard(kDiskSlot, &m_disk2);
        m_io.setCard(kDiskSlot, &m_disk2);
        m_disk2.setHeadEventCallback([this](Disk2Controller::HeadEvent event, uint64_t cycle) {
            if (!m_driveSoundsLoaded) return;
            m_driveSounds.trigger(event == Disk2Controller::HeadEvent::Step ? DriveSounds::Event::Step
                                                                            : DriveSounds::Event::Bump,
                                  cycle);
        });
    } else {
        std::cerr << "No Disk II ROM (disk2.rom): running without disk drives" << std::endl;
    }

    m_diskPanel = std::make_unique<DiskPanel>(m_hasDisk2 ? &m_disk2 : nullptr, kScreenWidth, kScreenHeight);

    if (!headless) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << std::endl;
            return false;
        }
        m_sdlInitialized = true;

        m_window = SDL_CreateWindow("Apple IIe", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    kLogicalWidth * 3 / 2, kScreenHeight * 3 / 2,
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
        SDL_RenderSetLogicalSize(m_renderer, kLogicalWidth, kScreenHeight);
        m_audio.init();
        SDL_StartTextInput();
    }

    reset(true);
    return true;
}

void Apple2e::loadDriveSounds(const std::string& directory) {
    if (!m_hasDisk2 || m_audio.sampleRate() == 0) return;  // no drives or no audio device
    if (m_driveSounds.load(directory, m_audio.sampleRate())) {
        m_driveSoundsLoaded = true;
        m_audio.setDriveSounds(&m_driveSounds);
    }
}

std::string Apple2e::insertDisk(int drive, const std::string& path) {
    if (!m_hasDisk2) return "No Disk II controller (disk2.rom not found)";
    return m_disk2.insert(drive, path);
}

bool Apple2e::saveScreenshot(const std::string& path) const {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kLogicalWidth, kScreenHeight, 32,
                                                          SDL_PIXELFORMAT_ARGB8888);
    if (!surface) return false;

    m_video.copyToSurface(surface);
    if (SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface)) {
        m_diskPanel->draw(renderer);
        SDL_RenderPresent(renderer);
        SDL_DestroyRenderer(renderer);
    }

    bool ok = SDL_SaveBMP(surface, path.c_str()) == 0;
    SDL_FreeSurface(surface);
    return ok;
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

    m_driveSounds.setMotor(m_hasDisk2 && m_disk2.spinning());
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
        if ((mod & KMOD_GUI) && key == SDLK_d) {
            m_driveSounds.setEnabled(!m_driveSounds.enabled());
            return;
        }
        if ((mod & KMOD_GUI) && (key == SDLK_1 || key == SDLK_2)) {
            m_diskPanel->chooseDisk(key == SDLK_1 ? 0 : 1);
            return;
        }
    }

    m_gameIO.handleEvent(event, {0, 0, kScreenWidth, kScreenHeight});
    if (m_diskPanel->handleEvent(event, m_renderer)) return;
    m_keyboard.handleEvent(event);
}

void Apple2e::run() {
    std::cout << "F12: RESET   Shift+F12: reboot   Cmd+1/Cmd+2: insert disk   Cmd+D: drive sounds on/off\n"
                 "Cmd+V: paste   Cmd+Q: quit\n";

    const double counterHz = static_cast<double>(SDL_GetPerformanceFrequency());
    const double frameSeconds = kCyclesPerFrame / kCpuClockHz;
    double nextFrame = SDL_GetPerformanceCounter() / counterHz;

    uint64_t fpsStart = SDL_GetPerformanceCounter();
    int fpsFrames = 0;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) handleEvent(event, running);
        m_diskPanel->update();

        runFrame();
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
        SDL_RenderClear(m_renderer);
        m_video.draw(m_renderer, {0, 0, kScreenWidth, kScreenHeight});
        m_diskPanel->draw(m_renderer);
        SDL_RenderPresent(m_renderer);

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
