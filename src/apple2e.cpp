#include "apple2e.h"

#include "macmenu.h"

#ifdef HAVE_SDL_IMAGE
#include <SDL_image.h>
#endif

#ifdef __APPLE__
#include <SDL_syswm.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>

namespace apple2e {

namespace {
// Make `child` follow `parent` around and stay behind it, like a macOS
// drawer: the side panel slides out from under the monitor's window. Only
// Cocoa can keep two windows ordered like that; false anywhere else.
bool attachWindowBehind(SDL_Window *parent, SDL_Window *child) {
#ifdef __APPLE__
  SDL_SysWMinfo parentInfo, childInfo;
  SDL_VERSION(&parentInfo.version);
  SDL_VERSION(&childInfo.version);
  if (!SDL_GetWindowWMInfo(parent, &parentInfo) ||
      !SDL_GetWindowWMInfo(child, &childInfo) ||
      parentInfo.subsystem != SDL_SYSWM_COCOA ||
      childInfo.subsystem != SDL_SYSWM_COCOA)
    return false;
  id parentWindow = reinterpret_cast<id>(parentInfo.info.cocoa.window);
  id childWindow = reinterpret_cast<id>(childInfo.info.cocoa.window);
  if (!parentWindow || !childWindow)
    return false;
  const long kNSWindowBelow = -1;
  reinterpret_cast<void (*)(id, SEL, id, long)>(objc_msgSend)(
      parentWindow, sel_registerName("addChildWindow:ordered:"), childWindow,
      kNSWindowBelow);
  return true;
#else
  (void)parent;
  (void)child;
  return false;
#endif
}

// Let what is behind `window` show through the pixels its renderer leaves
// transparent (only the panel's handle is drawn beyond the panel). Needs
// Cocoa and the Metal renderer; false otherwise.
bool makeWindowTransparent(SDL_Window *window, SDL_Renderer *renderer) {
#ifdef __APPLE__
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  id layer = reinterpret_cast<id>(SDL_RenderGetMetalLayer(renderer));
  if (!layer || !SDL_GetWindowWMInfo(window, &info) ||
      info.subsystem != SDL_SYSWM_COCOA || !info.info.cocoa.window)
    return false;
  id nsWindow = reinterpret_cast<id>(info.info.cocoa.window);
  auto setBool = reinterpret_cast<void (*)(id, SEL, signed char)>(objc_msgSend);
  auto getObject = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend);
  auto setObject = reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend);
  id clear = getObject(reinterpret_cast<id>(objc_getClass("NSColor")),
                       sel_registerName("clearColor"));
  setBool(nsWindow, sel_registerName("setOpaque:"), 0);
  setObject(nsWindow, sel_registerName("setBackgroundColor:"), clear);
  setBool(nsWindow, sel_registerName("setHasShadow:"), 0);
  setBool(layer, sel_registerName("setOpaque:"), 0);
  return true;
#else
  (void)window;
  (void)renderer;
  return false;
#endif
}

// SDL paints the window's own layer black whenever it is redisplayed; take
// that away again (cheap enough to do on every frame)
void clearWindowBackground(SDL_Window *window) {
#ifdef __APPLE__
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  if (!SDL_GetWindowWMInfo(window, &info) || !info.info.cocoa.window)
    return;
  auto getObject = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend);
  auto setPointer = reinterpret_cast<void (*)(id, SEL, void *)>(objc_msgSend);
  id view = getObject(reinterpret_cast<id>(info.info.cocoa.window),
                      sel_registerName("contentView"));
  id layer = view ? getObject(view, sel_registerName("layer")) : nullptr;
  if (layer)
    setPointer(layer, sel_registerName("setBackgroundColor:"), nullptr);
#else
  (void)window;
#endif
}

// Window an input event belongs to (0 if it has none)
Uint32 eventWindowID(const SDL_Event &event) {
  switch (event.type) {
  case SDL_MOUSEMOTION:
    return event.motion.windowID;
  case SDL_MOUSEBUTTONDOWN:
  case SDL_MOUSEBUTTONUP:
    return event.button.windowID;
  case SDL_MOUSEWHEEL:
    return event.wheel.windowID;
  case SDL_DROPFILE:
    return event.drop.windowID;
  default:
    return 0;
  }
}

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
constexpr float kPanelSlideSeconds = 0.25f;
// Strip on the panel's outer edge to pull it out or push it in with the
// mouse; it stays in view when the panel is hidden
constexpr int kHandleWidth = 12;
constexpr int kHandleHeight = 64;
constexpr int kPanelWindowWidth = SidePanel::kWidth + kHandleWidth;

float smoothStep(float t) { return t * t * (3.0f - 2.0f * t); }

float inverseSmoothStep(float value) {
  float lo = 0.0f, hi = 1.0f;
  for (int i = 0; i < 24; i++) {
    float mid = (lo + hi) / 2;
    (smoothStep(mid) < value ? lo : hi) = mid;
  }
  return (lo + hi) / 2;
}
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
  m_crtDisplay.reset();
  if (m_bezel)
    SDL_FreeSurface(m_bezel);
  if (m_hasDisk2)
    m_disk2.flush();
  m_sidePanel.reset(); // its textures belong to one of the renderers
  if (m_resizeCursor)
    SDL_FreeCursor(m_resizeCursor);
  if (m_arrowCursor)
    SDL_FreeCursor(m_arrowCursor);
  if (m_panelRenderer)
    SDL_DestroyRenderer(m_panelRenderer);
  if (m_panelWindow)
    SDL_DestroyWindow(m_panelWindow);
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
    std::cerr << "No Disk II ROM (disk2.rom): running without disk drives. "
                 "Put it in the current directory, next to the executable or "
                 "in a folder above it"
              << std::endl;
  }

  m_sidePanel = std::make_unique<SidePanel>(
      m_hasDisk2 ? &m_disk2 : nullptr, m_video, kPanelX, kLogicalHeight);
  m_sidePanel->setEjectAction([this] { m_driveSounds.playEject(); });
  m_sidePanel->addSlider(
      "Curve", [this] { return m_curvature / kMaxCurvature; },
      [this](float v) { setCurvature(v * kMaxCurvature); }, [this] { return m_crt; });
  m_sidePanel->addSlider(
      "Motor", [this] { return m_driveSounds.motorVolume(); },
      [this](float v) { m_driveSounds.setMotorVolume(v); });
  m_sidePanel->addSlider(
      "Head", [this] { return m_driveSounds.headVolume(); },
      [this](float v) { m_driveSounds.setHeadVolume(v); });
  m_sidePanel->setCrtActions([this] { return m_crt; },
                             [this] { setCrt(!m_crt); });
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
    // A click on a window that is not the active one acts at once, instead
    // of only activating it (the side panel has its own window)
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    m_window = SDL_CreateWindow(
        "Apple IIe", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kLogicalWidth * 3 / 2, kLogicalHeight * 3 / 2,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    createPanelWindow(); // where possible; else the panel shares the window
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
    m_sidePanel->setMainRenderer(m_drawer ? m_panelRenderer : m_renderer);
    m_audio.init();
    SDL_StartTextInput();
    applySettings();
#ifdef __APPLE__
    m_menuEvent = SDL_RegisterEvents(1);
    installMacMenu(m_menuEvent, [this](int command) {
      switch (static_cast<Command>(command)) {
      case Command::ToggleGreen:
        return m_video.monochrome();
      case Command::ToggleCrt:
        return m_crt;
      case Command::TogglePanel:
        return !m_panelHidden;
      case Command::ToggleDriveSounds:
        return m_driveSounds.enabled();
      default:
        return false;
      }
    });
#endif
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
    return "No Disk II controller (disk2.rom missing or bad, see the terminal)";
  return m_disk2.insert(drive, path);
}

SDL_Surface *Apple2e::renderScreenshot(int scale) const {
  SDL_Point size = logicalSize();
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
      0, size.x * scale, size.y * scale, 32, SDL_PIXELFORMAT_ARGB8888);
  if (!surface)
    return nullptr;

  // Same drawing as the window, with a software renderer on the surface
  bool ok = false;
  if (SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface)) {
    SDL_RenderSetScale(renderer, static_cast<float>(scale),
                       static_cast<float>(scale));
    SDL_Texture *frame = m_video.createFrameTexture(renderer);
    if (frame) {
      std::unique_ptr<CrtDisplay> crt;
      if (m_crt) {
        crt = std::make_unique<CrtDisplay>(renderer, m_bezel);
        crt->setCurvature(m_curvature);
      }
      compose(renderer, frame, crt.get());
      SDL_RenderPresent(renderer);
      crt.reset();
      SDL_DestroyTexture(frame);
      ok = true;
    }
    SDL_DestroyRenderer(renderer);
  }
  if (!ok) {
    SDL_FreeSurface(surface);
    return nullptr;
  }
  return surface;
}

bool Apple2e::saveScreenshot(const std::string &path, int scale) const {
  SDL_Surface *surface = renderScreenshot(scale);
  if (!surface)
    return false;
  bool ok = SDL_SaveBMP(surface, path.c_str()) == 0;
  SDL_FreeSurface(surface);
  return ok;
}

// ============================================================
// Monitor style
// ============================================================

SDL_Rect Apple2e::screenRect() const {
  return m_crt ? CrtDisplay::kPicture : kScreenRect;
}

int Apple2e::panelX() const { return m_crt ? CrtDisplay::kWidth : kPanelX; }

// Size of the main window's picture: the monitor, plus the side panel unless
// it is hidden or lives in its own window
SDL_Point Apple2e::logicalSize() const {
  int panel = m_drawer || m_panelHidden ? 0 : SidePanel::kWidth;
  if (m_crt)
    return {CrtDisplay::kWidth + panel, CrtDisplay::kHeight};
  return {kPanelX + panel, kLogicalHeight};
}

void Apple2e::compose(SDL_Renderer *renderer, SDL_Texture *frame,
                      CrtDisplay *crt) const {
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  if (m_crt && crt && crt->ok()) {
    crt->draw(frame, {0, 0}, m_video.monochrome());
  } else {
    SDL_RenderCopy(renderer, frame, nullptr, &kScreenRect);
  }
  if (!m_drawer && !m_panelHidden)
    m_sidePanel->draw(renderer);
}

void Apple2e::setCrt(bool on) {
  SDL_Point oldSize = logicalSize();
  m_crt = on;
  relayout(oldSize);
}

void Apple2e::setPanelHidden(bool hidden, bool animate) {
  SDL_Point oldSize = logicalSize();
  m_panelHidden = hidden;
  if (m_drawer && animate) {
    m_panelSliding = true; // updatePanelSlide() moves its window
    return;
  }
  m_panelSliding = false;
  m_slideT = m_panelShown = hidden ? 0.0f : 1.0f;
  relayout(oldSize);
}

void Apple2e::updatePanelSlide(float seconds) {
  if (!m_panelSliding)
    return;
  float step = seconds / kPanelSlideSeconds;
  m_slideT = m_panelHidden ? std::max(0.0f, m_slideT - step)
                           : std::min(1.0f, m_slideT + step);
  m_panelShown = smoothStep(m_slideT);
  if (m_slideT <= 0.0f || m_slideT >= 1.0f)
    m_panelSliding = false;
  placePanelWindow();
}

// The side panel in a window of its own, kept behind the main one: hiding it
// slides that window under the monitor and the main window never changes
void Apple2e::createPanelWindow() {
#ifndef __APPLE__
  return; // nothing else keeps a second window glued behind the first
#endif
  m_panelWindow = SDL_CreateWindow(
      "Apple IIe drives", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      kPanelWindowWidth * 3 / 2, kLogicalHeight * 3 / 2,
      SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SKIP_TASKBAR);
  if (m_panelWindow)
    m_panelRenderer = SDL_CreateRenderer(m_panelWindow, -1, SDL_RENDERER_ACCELERATED);
  if (m_panelRenderer && attachWindowBehind(m_window, m_panelWindow)) {
    m_drawer = true;
    m_panelTransparent = makeWindowTransparent(m_panelWindow, m_panelRenderer);
    // The main window was made wide enough for the panel too
    SDL_SetWindowSize(m_window, kPanelX * 3 / 2, kLogicalHeight * 3 / 2);
    SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_RaiseWindow(m_window);
    return;
  }
  if (m_panelRenderer)
    SDL_DestroyRenderer(m_panelRenderer);
  if (m_panelWindow)
    SDL_DestroyWindow(m_panelWindow);
  m_panelRenderer = nullptr;
  m_panelWindow = nullptr;
}

// Size the panel's window for the main one (same height and zoom)
void Apple2e::layoutPanelWindow() {
  if (!m_drawer)
    return;
  int w, h;
  SDL_GetWindowSize(m_window, &w, &h);
  SDL_Point size = logicalSize();
  int panelW = static_cast<int>(std::lround(kPanelWindowWidth * static_cast<float>(h) / size.y));
  SDL_SetWindowSize(m_panelWindow, panelW, h);
  SDL_RenderSetLogicalSize(m_panelRenderer, kPanelWindowWidth, size.y);
  placePanelWindow();
}

// How far the panel's window travels between hidden and shown, in window
// units: the panel's width without the handle
float Apple2e::panelTravel() const {
  int panelW, panelH;
  SDL_GetWindowSize(m_panelWindow, &panelW, &panelH);
  return panelW * static_cast<float>(SidePanel::kWidth) / kPanelWindowWidth;
}

// Shown, the panel's window touches the main window's right edge; hidden, the
// panel is behind the main window and only the handle sticks out
void Apple2e::placePanelWindow() {
  if (!m_drawer)
    return;
  int x, y, w, h;
  SDL_GetWindowPosition(m_window, &x, &y);
  SDL_GetWindowSize(m_window, &w, &h);
  float shownX = static_cast<float>(x + w);
  float hiddenX = shownX - panelTravel();
  int panelX = static_cast<int>(std::lround(hiddenX + (shownX - hiddenX) * m_panelShown));
  SDL_SetWindowPosition(m_panelWindow, panelX, y);
}

// The handle on the panel's window: drag it to slide the panel (it settles on
// the nearer side when released), or click it to hide / show the panel
bool Apple2e::handlePanelHandle(const SDL_Event &event) {
  bool leftButton = (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) &&
                    event.button.button == SDL_BUTTON_LEFT;
  if (event.type == SDL_MOUSEMOTION && !m_handleDragged) {
    SDL_Point p = {event.motion.x, event.motion.y};
    SDL_Rect tab = panelHandleRect();
    bool over = SDL_PointInRect(&p, &tab);
    if (!m_resizeCursor)
      m_resizeCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    if (!m_arrowCursor)
      m_arrowCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    SDL_SetCursor(over ? m_resizeCursor : m_arrowCursor);
    return false;
  }
  SDL_Point click = {event.button.x, event.button.y};
  SDL_Rect tab = panelHandleRect();
  if (leftButton && event.type == SDL_MOUSEBUTTONDOWN && SDL_PointInRect(&click, &tab)) {
    int y;
    SDL_GetGlobalMouseState(&m_dragStartX, &y); // the window moves under the pointer
    m_dragStartShown = m_panelShown;
    m_handleDragged = true;
    m_handleMoved = false;
    m_panelSliding = false;
    return true;
  }
  if (!m_handleDragged)
    return false;

  if (event.type == SDL_MOUSEMOTION) {
    int x, y;
    SDL_GetGlobalMouseState(&x, &y);
    if (std::abs(x - m_dragStartX) > 3)
      m_handleMoved = true;
    if (m_handleMoved) {
      m_panelShown = std::clamp(m_dragStartShown + (x - m_dragStartX) / panelTravel(), 0.0f, 1.0f);
      placePanelWindow();
    }
  } else if (leftButton && event.type == SDL_MOUSEBUTTONUP) {
    m_handleDragged = false;
    m_panelHidden = m_handleMoved ? m_panelShown < 0.5f : !m_panelHidden;
    m_slideT = inverseSmoothStep(m_panelShown);
    m_panelSliding = true; // the rest of the way
    SDL_RaiseWindow(m_window);
  }
  return true;
}

// The handle: a tab half way up the panel's outer edge
SDL_Rect Apple2e::panelHandleRect() const {
  return {SidePanel::kWidth, logicalSize().y / 2 - kHandleHeight / 2, kHandleWidth, kHandleHeight};
}

void Apple2e::drawPanelHandle(SDL_Renderer *renderer) const {
  SDL_Rect tab = panelHandleRect();
  // Rounded on the outside
  SDL_SetRenderDrawColor(renderer, 0x3A, 0x3B, 0x3E, 255);
  SDL_Rect parts[] = {{tab.x, tab.y, tab.w - 3, tab.h},
                      {tab.x + tab.w - 3, tab.y + 1, 1, tab.h - 2},
                      {tab.x + tab.w - 2, tab.y + 2, 1, tab.h - 4},
                      {tab.x + tab.w - 1, tab.y + 4, 1, tab.h - 8}};
  SDL_RenderFillRects(renderer, parts, 4);
  // Grip: three ridges, each a light line with a shadow
  for (int i = 0; i < 3; i++) {
    int x = tab.x + 2 + i * 3;
    int top = tab.y + 12, bottom = tab.y + tab.h - 13;
    SDL_SetRenderDrawColor(renderer, 0x8A, 0x8C, 0x90, 255);
    SDL_RenderDrawLine(renderer, x, top, x, bottom);
    SDL_SetRenderDrawColor(renderer, 0x1C, 0x1C, 0x1E, 255);
    SDL_RenderDrawLine(renderer, x + 1, top, x + 1, bottom);
  }
}

// The window's zoom now (it shows `oldSize`), limited so that `newSize` fits
// the usable area of the window's display
float Apple2e::zoomFor(SDL_Point oldSize, SDL_Point newSize) const {
  int w, h;
  SDL_GetWindowSize(m_window, &w, &h);
  float zoom = static_cast<float>(w) / oldSize.x;
  SDL_Rect usable;
  if (SDL_GetDisplayUsableBounds(SDL_GetWindowDisplayIndex(m_window), &usable) == 0) {
    zoom = std::min({zoom, usable.w * 0.95f / newSize.x, usable.h * 0.95f / newSize.y});
  }
  return zoom;
}

// Place the panel and size the window for the current layout
void Apple2e::applyLayout(float zoom) {
  SDL_Point size = logicalSize();
  m_sidePanel->setGeometry(m_drawer ? 0 : panelX(), size.y);
  if (!m_window || !m_renderer)
    return;
  SDL_RenderSetLogicalSize(m_renderer, size.x, size.y);
  SDL_SetWindowSize(m_window, static_cast<int>(std::lround(size.x * zoom)),
                    static_cast<int>(std::lround(size.y * zoom)));
  layoutPanelWindow();
}

// The layout changed (monitor style or side panel): resize the window for it,
// keeping its zoom
void Apple2e::relayout(SDL_Point oldSize) {
  applyLayout(m_window ? zoomFor(oldSize, logicalSize()) : 1.0f);
}

void Apple2e::setCurvature(float curvature) {
  m_curvature = std::clamp(curvature, 0.0f, kMaxCurvature);
  if (m_crtDisplay)
    m_crtDisplay->setCurvature(m_curvature);
}

bool Apple2e::loadMonitorImage(const std::string &directory) {
#ifdef HAVE_SDL_IMAGE
  std::string path =
      (std::filesystem::path(directory) / "monitor2.png").string();
  SDL_Surface *image = IMG_Load(path.c_str());
  if (!image)
    return false;
  if (m_bezel)
    SDL_FreeSurface(m_bezel);
  m_bezel = image;
  m_crtDisplay.reset(); // remade with the artwork on the next frame
  return true;
#else
  (void)directory;
  return false;
#endif
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
  // Both before the window size, which was saved for this layout
  setCrt(m_settings.crtMonitor);
  setPanelHidden(!m_settings.sidePanel);
  setCurvature(m_settings.crtCurvature / 100.0f * kMaxCurvature);
  m_driveSounds.setEnabled(m_settings.driveSounds);
  m_driveSounds.setMotorVolume(m_settings.motorVolume / 100.0f);
  m_driveSounds.setHeadVolume(m_settings.headVolume / 100.0f);
  m_sidePanel->setLastDirectory(m_settings.diskDirectory);
  m_sidePanel->setLastStateDirectory(m_settings.stateDirectory);

  if (m_settings.windowW > 0 && m_settings.windowH > 0) {
    // The saved width may be for another layout (panel inside or not)
    SDL_Point size = logicalSize();
    SDL_SetWindowSize(m_window, m_settings.windowH * size.x / size.y, m_settings.windowH);
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
  layoutPanelWindow();
}

void Apple2e::saveSettings() {
  if (m_settingsPath.empty() || !m_window)
    return;
  m_settings.greenMonitor = m_video.monochrome();
  m_settings.crtMonitor = m_crt;
  m_settings.sidePanel = !m_panelHidden;
  m_settings.crtCurvature =
      static_cast<int>(m_curvature / kMaxCurvature * 100 + 0.5f);
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

void Apple2e::runCommand(Command command, bool fromMenu) {
  // A shortcut that is also a menu item's may arrive both as the key press
  // and as the menu's choice, depending on the system: act on it once
  Uint32 now = SDL_GetTicks();
  bool repeated = command == m_lastCommand && fromMenu != m_lastCommandFromMenu &&
                  now - m_lastCommandTicks < 200;
  m_lastCommand = command;
  m_lastCommandFromMenu = fromMenu;
  m_lastCommandTicks = now;
  if (repeated)
    return;

  switch (command) {
  case Command::Reset:
    reset(false);
    break;
  case Command::PowerCycle:
    reset(true);
    break;
  case Command::InsertDisk1:
  case Command::InsertDisk2:
    m_sidePanel->chooseDisk(command == Command::InsertDisk1 ? 0 : 1);
    break;
  case Command::EjectDisk1:
  case Command::EjectDisk2:
    m_sidePanel->ejectDisk(command == Command::EjectDisk1 ? 0 : 1);
    break;
  case Command::QuickSave:
    quickSave();
    break;
  case Command::QuickLoad:
    quickLoad();
    break;
  case Command::SaveStateAs:
  case Command::LoadStateFrom:
    m_sidePanel->chooseStateFile(command == Command::SaveStateAs);
    break;
  case Command::Paste:
    if (char *clip = SDL_GetClipboardText()) {
      m_keyboard.queueText(clip);
      SDL_free(clip);
    }
    break;
  case Command::ToggleDriveSounds:
    m_driveSounds.setEnabled(!m_driveSounds.enabled());
    break;
  case Command::ToggleGreen:
    m_video.setMonochrome(!m_video.monochrome());
    break;
  case Command::ToggleCrt:
    setCrt(!m_crt);
    break;
  case Command::TogglePanel:
    setPanelHidden(!m_panelHidden, true);
    break;
  case Command::Quit: {
    SDL_Event quit;
    SDL_zero(quit);
    quit.type = SDL_QUIT;
    SDL_PushEvent(&quit);
    break;
  }
  }
}

void Apple2e::handleEvent(const SDL_Event &event, bool &running) {
  if (event.type == SDL_QUIT) {
    running = false;
    return;
  }
  if (m_menuEvent != 0 && event.type == m_menuEvent) {
    runCommand(static_cast<Command>(event.user.code), true);
    return;
  }
  if (event.type == SDL_WINDOWEVENT) {
    bool main = event.window.windowID == SDL_GetWindowID(m_window);
    if (event.window.event == SDL_WINDOWEVENT_CLOSE)
      running = false; // with two windows, closing one is not SDL_QUIT
    else if (main && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
      layoutPanelWindow();
    else if (main && event.window.event == SDL_WINDOWEVENT_MOVED)
      placePanelWindow();
    return;
  }
  // The panel's own window: its mouse events are in the panel's coordinates
  // and are not for the Apple's mouse or paddles
  if (m_drawer) {
    Uint32 id = eventWindowID(event);
    if (id != 0 && id == SDL_GetWindowID(m_panelWindow)) {
      if (handlePanelHandle(event))
        return;
      if (m_panelShown >= 1.0f)
        m_sidePanel->handleEvent(event, m_panelRenderer);
      // Typing goes on in the main window after a click on the panel
      if (event.type == SDL_MOUSEBUTTONUP)
        SDL_RaiseWindow(m_window);
      return;
    }
    if (event.type == SDL_DROPFILE) { // on the monitor: into drive 1
      m_sidePanel->insertDisk(0, event.drop.file);
      SDL_free(event.drop.file);
      return;
    }
  }

  if (event.type == SDL_KEYDOWN) {
    SDL_Keycode key = event.key.keysym.sym;
    SDL_Keymod mod = static_cast<SDL_Keymod>(event.key.keysym.mod);

    // Emulator hotkeys; everything else goes to the Apple keyboard
    bool cmd = mod & KMOD_GUI, shift = mod & KMOD_SHIFT;
    std::optional<Command> command;
    if (key == SDLK_F12)
      command = shift ? Command::PowerCycle : Command::Reset;
    else if (cmd && key == SDLK_v)
      command = Command::Paste;
    else if (cmd && key == SDLK_q)
      command = Command::Quit;
    else if (cmd && key == SDLK_s)
      command = shift ? Command::SaveStateAs : Command::QuickSave;
    else if (cmd && key == SDLK_l)
      command = shift ? Command::LoadStateFrom : Command::QuickLoad;
    else if (cmd && key == SDLK_m)
      command = Command::ToggleCrt;
    else if (cmd && key == SDLK_b)
      command = Command::TogglePanel;
    else if (cmd && key == SDLK_g)
      command = Command::ToggleGreen;
    else if (cmd && key == SDLK_d)
      command = Command::ToggleDriveSounds;
    else if (cmd && key == SDLK_1)
      command = shift ? Command::EjectDisk1 : Command::InsertDisk1;
    else if (cmd && key == SDLK_2)
      command = shift ? Command::EjectDisk2 : Command::InsertDisk2;
    if (command) {
      runCommand(*command, false);
      return;
    }
  }

  // Once software enables the mouse card, the host mouse drives it (and the
  // pointer is hidden over the screen, the Apple draws its own cursor);
  // otherwise it acts as paddles/buttons
  const SDL_Rect screen = screenRect();
  if (event.type == SDL_MOUSEMOTION) {
    SDL_Point p = {event.motion.x, event.motion.y};
    bool overScreen = SDL_PointInRect(&p, &screen);
    SDL_ShowCursor(m_mouseCard.enabled() && overScreen ? SDL_DISABLE : SDL_ENABLE);
    if (m_arrowCursor)
      SDL_SetCursor(m_arrowCursor); // back from the panel handle's cursor
  }
  if (m_mouseCard.handleEvent(event, screen))
    return;
  m_gameIO.handleEvent(event, screen);
  if (!m_drawer && !m_panelHidden && m_sidePanel->handleEvent(event, m_renderer))
    return;
  m_keyboard.handleEvent(event);
}

void Apple2e::run() {
  std::cout << "F12: RESET   Shift+F12: reboot   Cmd+1/Cmd+2: insert disk   "
               "Cmd+D: drive sounds on/off\n"
               "Cmd+G: colour/green monitor   Cmd+M: CRT on/off   "
               "Cmd+S/Cmd+L: save/load state "
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
    updatePanelSlide(static_cast<float>(frameSeconds));
    if (m_crt && !m_crtDisplay) {
      m_crtDisplay = std::make_unique<CrtDisplay>(m_renderer, m_bezel);
      m_crtDisplay->setCurvature(m_curvature);
    }
    compose(m_renderer, m_video.frameTexture(), m_crtDisplay.get());
    SDL_RenderPresent(m_renderer);
    if (m_drawer) {
      // Beyond the panel only the handle shows: the rest is see-through,
      // or the panel's own colour where that is not possible
      if (m_panelTransparent) {
        clearWindowBackground(m_panelWindow);
        SDL_SetRenderDrawColor(m_panelRenderer, 0, 0, 0, 0);
      } else
        SDL_SetRenderDrawColor(m_panelRenderer, 0x1C, 0x1C, 0x1E, 255);
      SDL_RenderClear(m_panelRenderer);
      m_sidePanel->draw(m_panelRenderer);
      drawPanelHandle(m_panelRenderer);
      SDL_RenderPresent(m_panelRenderer);
    }

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
