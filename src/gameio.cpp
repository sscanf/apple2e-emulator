#include "gameio.h"
#include "state.h"

#include <algorithm>
#include <iostream>

namespace apple2e {

namespace {

// PREAD counts one unit every ~11 cycles, so a paddle at 255 times out after ~2.8 ms
constexpr uint64_t kCyclesPerPaddleUnit = 11;

uint8_t scale(int value, int min, int max) {
    int clamped = std::clamp(value, min, max);
    return static_cast<uint8_t>((clamped - min) * 255 / (max - min));
}

} // namespace

GameIO::~GameIO() {
    if (m_controller) SDL_GameControllerClose(m_controller);
}

bool GameIO::paddleTimerRunning(int paddle, uint64_t cycle) const {
    return cycle - m_triggerCycle < m_paddles[paddle] * kCyclesPerPaddleUnit;
}

void GameIO::openController(int deviceIndex) {
    if (m_controller || !SDL_IsGameController(deviceIndex)) return;
    m_controller = SDL_GameControllerOpen(deviceIndex);
    if (m_controller) {
        std::cout << "Game controller: " << SDL_GameControllerName(m_controller) << std::endl;
    }
}

void GameIO::handleEvent(const SDL_Event& event, const SDL_Rect& screen) {
    switch (event.type) {
        case SDL_MOUSEMOTION: {
            SDL_Point p = {event.motion.x, event.motion.y};
            if (!SDL_PointInRect(&p, &screen)) break;
            m_paddles[0] = scale(p.x, screen.x, screen.x + screen.w - 1);
            m_paddles[1] = scale(p.y, screen.y, screen.y + screen.h - 1);
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            SDL_Point p = {event.button.x, event.button.y};
            bool down = event.type == SDL_MOUSEBUTTONDOWN;
            if (down && !SDL_PointInRect(&p, &screen)) break;
            if (event.button.button == SDL_BUTTON_LEFT) m_mouseButtons[0] = down;
            if (event.button.button == SDL_BUTTON_RIGHT) m_mouseButtons[1] = down;
            break;
        }

        case SDL_CONTROLLERDEVICEADDED:
            openController(event.cdevice.which);
            break;

        case SDL_CONTROLLERDEVICEREMOVED:
            if (m_controller && event.cdevice.which ==
                                    SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_controller))) {
                SDL_GameControllerClose(m_controller);
                m_controller = nullptr;
                m_padButtons = {};
            }
            break;

        case SDL_CONTROLLERAXISMOTION: {
            int paddle = -1;
            switch (event.caxis.axis) {
                case SDL_CONTROLLER_AXIS_LEFTX:  paddle = 0; break;
                case SDL_CONTROLLER_AXIS_LEFTY:  paddle = 1; break;
                case SDL_CONTROLLER_AXIS_RIGHTX: paddle = 2; break;
                case SDL_CONTROLLER_AXIS_RIGHTY: paddle = 3; break;
                default: break;
            }
            if (paddle >= 0) m_paddles[paddle] = scale(event.caxis.value, -32768, 32767);
            break;
        }

        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP: {
            bool down = event.type == SDL_CONTROLLERBUTTONDOWN;
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_A: m_padButtons[0] = down; break;
                case SDL_CONTROLLER_BUTTON_B: m_padButtons[1] = down; break;
                case SDL_CONTROLLER_BUTTON_X: m_padButtons[2] = down; break;
                default: break;
            }
            break;
        }

        default:
            break;
    }
}

void GameIO::saveState(StateWriter& w) const {
    w.put(m_paddles);
    w.put(m_triggerCycle);
}

void GameIO::loadState(StateReader& r) {
    r.get(m_paddles);
    r.get(m_triggerCycle);
}

} // namespace apple2e
