#include "keyboard.h"
#include "state.h"

namespace apple2e {

void KeyboardController::reset() {
    m_latch = 0;
    m_strobe = false;
    m_pending.clear();
}

void KeyboardController::press(uint8_t ascii) {
    m_latch = ascii & 0x7F;
    m_strobe = true;
}

uint8_t KeyboardController::clearStrobe() {
    m_strobe = false;
    return (m_keysHeld > 0 ? 0x80 : 0x00) | m_latch;
}

void KeyboardController::queueText(const std::string& text) {
    for (char c : text) {
        uint8_t ch = static_cast<uint8_t>(c);
        if (ch == '\r') continue;
        if (ch == '\n') ch = 0x0D;
        if (ch >= 0x80) continue;
        m_pending.push_back(ch);
    }
}

void KeyboardController::update() {
    if (m_pauseFrames > 0) {
        m_pauseFrames--;
        return;
    }
    if (!m_strobe && !m_pending.empty()) {
        uint8_t ch = m_pending.front();
        m_pending.pop_front();
        if (ch == kPauseChar) {
            m_pauseFrames = kPauseFrames;
        } else {
            press(ch);
        }
    }
}

void KeyboardController::handleEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_TEXTINPUT: {
            SDL_Keymod mod = SDL_GetModState();
            if (mod & (KMOD_CTRL | KMOD_GUI)) break;  // handled as key events
            for (const char* p = event.text.text; *p; ++p) {
                uint8_t ch = static_cast<uint8_t>(*p);
                if (ch >= 0x80) continue;  // non-ASCII has no Apple equivalent
                press(ch);  // case already follows the Mac's Caps Lock and Shift
            }
            break;
        }

        case SDL_KEYDOWN: {
            SDL_Keycode key = event.key.keysym.sym;
            SDL_Keymod mod = static_cast<SDL_Keymod>(event.key.keysym.mod);

            if (key == SDLK_LALT) m_openApple = true;
            if (key == SDLK_RALT) m_solidApple = true;
            if (!event.key.repeat) m_keysHeld++;

            switch (key) {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:  press(0x0D); return;
                case SDLK_BACKSPACE:
                case SDLK_LEFT:      press(0x08); return;
                case SDLK_RIGHT:     press(0x15); return;
                case SDLK_UP:        press(0x0B); return;
                case SDLK_DOWN:      press(0x0A); return;
                case SDLK_ESCAPE:    press(0x1B); return;
                case SDLK_TAB:       press(0x09); return;
                case SDLK_DELETE:    press(0x7F); return;
                default: break;
            }

            // CTRL+letter produces control codes $01-$1A
            if ((mod & KMOD_CTRL) && key >= SDLK_a && key <= SDLK_z) {
                press(static_cast<uint8_t>(key - SDLK_a + 1));
            }
            break;
        }

        case SDL_KEYUP:
            if (event.key.keysym.sym == SDLK_LALT) m_openApple = false;
            if (event.key.keysym.sym == SDLK_RALT) m_solidApple = false;
            if (m_keysHeld > 0) m_keysHeld--;
            break;

        default:
            break;
    }
}

void KeyboardController::saveState(StateWriter& w) const {
    w.put(m_latch);
    w.put(m_strobe);
}

void KeyboardController::loadState(StateReader& r) {
    r.get(m_latch);
    r.get(m_strobe);
    m_pending.clear();
    m_keysHeld = 0;
}

} // namespace apple2e
