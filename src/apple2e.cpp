#include "apple2e.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace apple2e {

namespace {
constexpr int kDiskSlot = 6;
constexpr int kMouseSlot = 4;
constexpr int kSoftCardSlot = 5;
constexpr int kScreenWidth = VideoController::kWidth;
constexpr int kScreenHeight =
    VideoController::kHeight * 2; // scanlines doubled for 4:3
// Black border around the Apple screen, like a monitor's bezel; it also keeps
// text clear of the window's rounded corners
constexpr int kBorder = 16;
constexpr SDL_Rect kScreenRect = {kBorder, kBorder, kScreenWidth, kScreenHeight};
constexpr int kPanelX = kScreenWidth + 2 * kBorder;
constexpr int kLogicalWidth = kPanelX + SidePanel::kWidth;
constexpr int kLogicalHeight = kScreenHeight + 2 * kBorder;
} // namespace

Apple2e::Apple2e()
    : m_memory(m_switches),
      m_io(m_switches, m_keyboard, m_audio, m_gameIO, m_cycles),
      m_cpu(m_memory), m_video(m_memory, m_switches), m_softCard(m_memory),
      m_mouseCard(m_memory, kMouseSlot),
      m_disk2(m_cycles) {
  m_memory.setIO(&m_io);
  m_memory.setCard(kSoftCardSlot, &m_softCard);
  m_io.setCard(kSoftCardSlot, &m_softCard);
  m_memory.setCard(kMouseSlot, &m_mouseCard);
  m_io.setCard(kMouseSlot, &m_mouseCard);
  m_mouseCard.setIrqCallback([this](bool asserted) { m_cpu.setIrqLine(asserted); });
}

Apple2e::~Apple2e() {
  if (m_hasDisk2)
    m_disk2.flush();
  if (m_renderer)
    SDL_DestroyRenderer(m_renderer);
  if (m_window)
    SDL_DestroyWindow(m_window);
  if (m_sdlInitialized)
    SDL_Quit();
}

bool Apple2e::init(const std::string &romPath, const std::string &diskRomPath,
                   bool headless) {
  if (!m_memory.loadRom(romPath))
    return false;

  m_hasDisk2 = !diskRomPath.empty() && m_disk2.loadRom(diskRomPath);
  if (m_hasDisk2) {
    m_memory.setCard(kDiskSlot, &m_disk2);
    m_io.setCard(kDiskSlot, &m_disk2);
    m_disk2.setHeadEventCallback(
        [this](Disk2Controller::HeadEvent event, uint64_t cycle) {
          if (!m_driveSoundsLoaded)
            return;
          m_driveSounds.trigger(event == Disk2Controller::HeadEvent::Step
                                    ? DriveSounds::Event::Step
                                    : DriveSounds::Event::Bump,
                                cycle);
        });
  } else {
    std::cerr << "No Disk II ROM (disk2.rom): running without disk drives"
              << std::endl;
  }

  m_sidePanel = std::make_unique<SidePanel>(
      m_hasDisk2 ? &m_disk2 : nullptr, m_video, kPanelX, kLogicalHeight);
  m_sidePanel->setEjectAction([this] { m_driveSounds.playEject(); });
  m_sidePanel->setDriveSounds(&m_driveSounds);
  m_sidePanel->setStateActions(
      [this](const std::string &path) { saveStateTo(path); },
      [this](const std::string &path) { loadStateFrom(path); });

  if (!headless) {
    m_settingsPath = Settings::defaultPath();
    if (!m_settingsPath.empty())
      m_settings.load(m_settingsPath);
  }

  if (!headless) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) <
        0) {
      std::cerr << "SDL init failed: " << SDL_GetError() << std::endl;
      return false;
    }
    m_sdlInitialized = true;

    m_window = SDL_CreateWindow(
        "Apple IIe", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kLogicalWidth * 3 / 2, kLogicalHeight * 3 / 2,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!m_window) {
      std::cerr << "SDL window creation failed: " << SDL_GetError()
                << std::endl;
      return false;
    }

    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_renderer) {
      std::cerr << "SDL renderer creation failed: " << SDL_GetError()
                << std::endl;
      return false;
    }

    if (!m_video.init(m_renderer)) {
      std::cerr << "Video init failed: " << SDL_GetError() << std::endl;
      return false;
    }
    SDL_RenderSetLogicalSize(m_renderer, kLogicalWidth, kLogicalHeight);
    m_sidePanel->setMainRenderer(m_renderer);
    m_audio.init();
    SDL_StartTextInput();
    applySettings();
  }

  reset(true);
  return true;
}

void Apple2e::loadDriveSounds(const std::string &directory) {
  if (!m_hasDisk2 || m_audio.sampleRate() == 0)
    return; // no drives or no audio device
  if (m_driveSounds.load(directory, m_audio.sampleRate())) {
    m_driveSoundsLoaded = true;
    m_audio.setDriveSounds(&m_driveSounds);
  }
}

std::string Apple2e::insertDisk(int drive, const std::string &path) {
  if (!m_hasDisk2)
    return "No Disk II controller (disk2.rom not found)";
  return m_disk2.insert(drive, path);
}

bool Apple2e::saveScreenshot(const std::string &path) const {
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
      0, kLogicalWidth, kLogicalHeight, 32, SDL_PIXELFORMAT_ARGB8888);
  if (!surface)
    return false;

  SDL_FillRect(surface, nullptr, SDL_MapRGB(surface->format, 0, 0, 0));
  m_video.copyToSurface(surface, kScreenRect.x, kScreenRect.y);
  if (SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface)) {
    m_sidePanel->draw(renderer);
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
  m_softCard.reset();
  m_mouseCard.reset();
  m_cpu.reset();
}

void Apple2e::typeText(const std::string &text, int delayFrames) {
  m_delayedText = text;
  m_typeDelayFrames = delayFrames;
}

// ============================================================
// Save states
// ============================================================

namespace {
constexpr char kStateMagic[8] = {'A', '2', 'E', 'S', 'T', 'A', 'T', 'E'};
constexpr uint32_t kStateVersion = 1;
} // namespace

void Apple2e::writeState(StateWriter &w) const {
  w.putBytes(kStateMagic, sizeof(kStateMagic));
  w.put(kStateVersion);
  w.put(m_cycles);
  w.put(m_switches);
  m_memory.saveState(w);
  m_cpu.saveState(w);
  m_softCard.saveState(w);
  m_keyboard.saveState(w);
  m_gameIO.saveState(w);
  m_mouseCard.saveState(w);
  w.put(m_hasDisk2);
  if (m_hasDisk2)
    m_disk2.saveState(w);
}

void Apple2e::readState(StateReader &r) {
  r.get(m_cycles);
  r.get(m_switches);
  m_memory.loadState(r);
  m_cpu.loadState(r);
  m_softCard.loadState(r);
  m_keyboard.loadState(r);
  m_gameIO.loadState(r);
  m_mouseCard.loadState(r);
  bool hadDisk2 = r.get<bool>();
  if (hadDisk2)
    m_disk2.loadState(r);
}

std::string Apple2e::saveState(const std::string &path) {
  StateWriter w;
  writeState(w);
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file.write(reinterpret_cast<const char *>(w.data().data()),
                  w.data().size()))
    return "Cannot write " + path;
  return {};
}

std::string Apple2e::loadState(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return "Cannot open " + path;
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

  StateReader header(data);
  char magic[sizeof(kStateMagic)];
  header.getBytes(magic, sizeof(magic));
  auto version = header.get<uint32_t>();
  if (!header.ok() || std::memcmp(magic, kStateMagic, sizeof(magic)) != 0)
    return path + " is not a save state";
  if (version != kStateVersion)
    return "Save state version " + std::to_string(version) + " is not supported";

  // Keep the current state to roll back to if the file turns out to be bad,
  // and save disks with pending changes before their contents are replaced
  StateWriter backup;
  writeState(backup);
  if (m_hasDisk2)
    m_disk2.flush();

  StateReader r(data);
  std::vector<uint8_t> skip(sizeof(kStateMagic) + sizeof(uint32_t));
  r.getBytes(skip.data(), skip.size());
  readState(r);
  if (!r.ok() || !r.atEnd()) {
    StateReader restore(backup.data());
    restore.getBytes(skip.data(), skip.size());
    readState(restore);
    return path + " is damaged; state not loaded";
  }

  // Timing derived from the cycle counter restarts at the restored count
  m_audio.resync(m_cycles);
  m_driveSounds.clearEvents();
  return {};
}

std::string Apple2e::quickStatePath() {
  std::string dir = Settings::folder();
  return dir.empty() ? dir : dir + "quicksave.a2state";
}

void Apple2e::saveStateTo(const std::string &path) {
  std::string error = saveState(path);
  std::string name = std::filesystem::path(path).filename().string();
  m_sidePanel->showMessage(error.empty() ? "Saved " + name : error,
                           !error.empty());
}

void Apple2e::loadStateFrom(const std::string &path) {
  std::string error = loadState(path);
  std::string name = std::filesystem::path(path).filename().string();
  m_sidePanel->showMessage(error.empty() ? "Loaded " + name : error,
                           !error.empty());
}

void Apple2e::quickSave() {
  std::string path = quickStatePath();
  std::string error = path.empty() ? "No settings folder" : saveState(path);
  m_sidePanel->showMessage(error.empty() ? "State saved" : error, !error.empty());
}

void Apple2e::quickLoad() {
  std::string path = quickStatePath();
  std::string error = path.empty() ? "No settings folder" : loadState(path);
  if (!error.empty() && !std::ifstream(path))
    error = "No saved state yet (Cmd+S saves one)";
  m_sidePanel->showMessage(error.empty() ? "State loaded" : error, !error.empty());
}

void Apple2e::applySettings() {
  m_video.setMonochrome(m_settings.greenMonitor);
  m_driveSounds.setEnabled(m_settings.driveSounds);
  m_driveSounds.setMotorVolume(m_settings.motorVolume / 100.0f);
  m_driveSounds.setHeadVolume(m_settings.headVolume / 100.0f);
  m_sidePanel->setLastDirectory(m_settings.diskDirectory);
  m_sidePanel->setLastStateDirectory(m_settings.stateDirectory);

  if (m_settings.windowW > 0 && m_settings.windowH > 0) {
    SDL_SetWindowSize(m_window, m_settings.windowW, m_settings.windowH);
    // Only restore the position if it is still on a connected display
    SDL_Point corner = {m_settings.windowX, m_settings.windowY};
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
      SDL_Rect bounds;
      if (SDL_GetDisplayBounds(i, &bounds) == 0 &&
          SDL_PointInRect(&corner, &bounds)) {
        SDL_SetWindowPosition(m_window, corner.x, corner.y);
        break;
      }
    }
  }
}

void Apple2e::saveSettings() {
  if (m_settingsPath.empty() || !m_window)
    return;
  m_settings.greenMonitor = m_video.monochrome();
  m_settings.driveSounds = m_driveSounds.enabled();
  m_settings.motorVolume =
      static_cast<int>(m_driveSounds.motorVolume() * 100 + 0.5f);
  m_settings.headVolume =
      static_cast<int>(m_driveSounds.headVolume() * 100 + 0.5f);
  m_settings.diskDirectory = m_sidePanel->lastDirectory();
  m_settings.stateDirectory = m_sidePanel->lastStateDirectory();
  SDL_GetWindowPosition(m_window, &m_settings.windowX, &m_settings.windowY);
  SDL_GetWindowSize(m_window, &m_settings.windowW, &m_settings.windowH);
  if (!m_settings.save(m_settingsPath))
    std::cerr << "Could not save settings to " << m_settingsPath << std::endl;
}

void Apple2e::runFrame() {
  if (!m_delayedText.empty() && m_typeDelayFrames-- <= 0) {
    m_keyboard.queueText(m_delayedText);
    m_delayedText.clear();
  }
  m_keyboard.update();

  m_mouseCard.vblank();
  uint64_t frameEnd = m_cycles + kCyclesPerFrame;
  while (m_cycles < frameEnd) {
    // While the SoftCard's Z80 owns the bus the 6502 is halted
    m_cycles += m_softCard.z80Active() ? m_softCard.step() : m_cpu.step();
  }

  m_driveSounds.setMotor(m_hasDisk2 && m_disk2.spinning());
  m_audio.endFrame(m_cycles);
  m_video.renderFrame();
}

void Apple2e::runFrames(int frames) {
  for (int i = 0; i < frames; i++)
    runFrame();
}

void Apple2e::handleEvent(const SDL_Event &event, bool &running) {
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
      if (char *clip = SDL_GetClipboardText()) {
        m_keyboard.queueText(clip);
        SDL_free(clip);
      }
      return;
    }
    if ((mod & KMOD_GUI) && key == SDLK_q) {
      running = false;
      return;
    }
    if ((mod & KMOD_GUI) && (mod & KMOD_SHIFT) &&
        (key == SDLK_s || key == SDLK_l)) {
      m_sidePanel->chooseStateFile(key == SDLK_s); // Save as / Load from file
      return;
    }
    if ((mod & KMOD_GUI) && key == SDLK_s) {
      quickSave();
      return;
    }
    if ((mod & KMOD_GUI) && key == SDLK_l) {
      quickLoad();
      return;
    }
    if ((mod & KMOD_GUI) && key == SDLK_g) {
      m_video.setMonochrome(!m_video.monochrome());
      return;
    }
    if ((mod & KMOD_GUI) && key == SDLK_d) {
      m_driveSounds.setEnabled(!m_driveSounds.enabled());
      return;
    }
    if ((mod & KMOD_GUI) && (key == SDLK_1 || key == SDLK_2)) {
      m_sidePanel->chooseDisk(key == SDLK_1 ? 0 : 1);
      return;
    }
  }

  // Once software enables the mouse card, the host mouse drives it (and the
  // pointer is hidden over the screen, the Apple draws its own cursor);
  // otherwise it acts as paddles/buttons
  const SDL_Rect screen = kScreenRect;
  if (event.type == SDL_MOUSEMOTION) {
    SDL_Point p = {event.motion.x, event.motion.y};
    bool overScreen = SDL_PointInRect(&p, &screen);
    SDL_ShowCursor(m_mouseCard.enabled() && overScreen ? SDL_DISABLE : SDL_ENABLE);
  }
  if (m_mouseCard.handleEvent(event, screen))
    return;
  m_gameIO.handleEvent(event, screen);
  if (m_sidePanel->handleEvent(event, m_renderer))
    return;
  m_keyboard.handleEvent(event);
}

void Apple2e::run() {
  std::cout << "F12: RESET   Shift+F12: reboot   Cmd+1/Cmd+2: insert disk   "
               "Cmd+D: drive sounds on/off\n"
               "Cmd+G: colour/green monitor   Cmd+S/Cmd+L: save/load state "
               "(add Shift to choose the file)   "
               "Cmd+V: paste   Cmd+Q: quit\n";

  const double counterHz = static_cast<double>(SDL_GetPerformanceFrequency());
  const double frameSeconds = kCyclesPerFrame / kCpuClockHz;
  double nextFrame = SDL_GetPerformanceCounter() / counterHz;

  uint64_t fpsStart = SDL_GetPerformanceCounter();
  int fpsFrames = 0;

  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event))
      handleEvent(event, running);
    m_sidePanel->update();

    runFrame();
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    m_video.draw(m_renderer, kScreenRect);
    m_sidePanel->draw(m_renderer);
    SDL_RenderPresent(m_renderer);

    // Pace to the real machine's ~59.92 Hz frame rate
    nextFrame += frameSeconds;
    double now = SDL_GetPerformanceCounter() / counterHz;
    if (nextFrame > now) {
      SDL_Delay(static_cast<uint32_t>((nextFrame - now) * 1000.0));
    } else if (now - nextFrame > 0.25) {
      nextFrame =
          now; // fell far behind (e.g. window dragged); don't try to catch up
    }

    fpsFrames++;
    uint64_t counter = SDL_GetPerformanceCounter();
    double elapsed = (counter - fpsStart) / counterHz;
    if (elapsed >= 1.0) {
      char title[64];
      std::snprintf(title, sizeof(title), "Apple IIe  |  %.1f fps",
                    fpsFrames / elapsed);
      SDL_SetWindowTitle(m_window, title);
      fpsStart = counter;
      fpsFrames = 0;
    }
  }

  saveSettings();
}

} // namespace apple2e
