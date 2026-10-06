#include "keyboard.h"

#include <iostream>

namespace apple2e {

KeyboardController::KeyboardController() {
    // Clear keyboard matrix
    m_matrix.fill(std::array<bool, 8>{});
    m_charBuffer.fill(0);
}

void KeyboardController::processKey(SDL_KeyboardEvent& event) {
    bool down = (event.type == SDL_KEYDOWN);

    if (event.repeat) return;  // Ignore repeat events

    AppleKey key = sdlToAppleKey(event.keysym.scancode);

    // Handle modifier keys
    if (key == AppleKey::SHIFT) {
        m_shiftHeld = down;
        return;
    }
    if (key == AppleKey::CONTROL) {
        m_controlHeld = down;
        return;
    }
    if (key == AppleKey::CAPS_LOCK) {
        if (down) {
            m_capsLock = !m_capsLock;
        }
        return;
    }

    // Update matrix
    uint8_t row, col;
    keyToMatrix(key, row, col);

    if (row < 7 && col < 8) {
        m_matrix[row][col] = down;
    }

    // If key is printable, add to character buffer
    if (down && key != AppleKey::RETURN && key != AppleKey::ESCAPE) {
        uint8_t ch = keyToChar(key);
        if (ch != 0) {
            // Add to circular buffer
            uint8_t next = (m_charHead + 1) % m_charBuffer.size();
            if (next != m_charTail) {
                m_charBuffer[m_charHead] = ch;
                m_charHead = next;
                m_charAvailable = true;
            }
        }
    }

    // Handle special keys
    if (key == AppleKey::RETURN && down) {
        m_charBuffer[m_charHead] = 0x0D;  // CR
        m_charHead = (m_charHead + 1) % m_charBuffer.size();
        m_charAvailable = true;
    }

    if (key == AppleKey::ESCAPE && down) {
        m_charBuffer[m_charHead] = 0x1B;  // ESC
        m_charHead = (m_charHead + 1) % m_charBuffer.size();
        m_charAvailable = true;
    }
}

uint8_t KeyboardController::readMatrixRow(uint8_t row) {
    if (row >= 7) return 0;
    uint8_t result = 0;
    for (int col = 0; col < 8; col++) {
        if (m_matrix[row][col]) {
            result |= (1 << col);
        }
    }
    return result;
}

uint8_t KeyboardController::readChar() {
    if (!m_charAvailable) return 0;
    uint8_t ch = m_charBuffer[m_charTail];
    m_charTail = (m_charTail + 1) % m_charBuffer.size();
    if (m_charTail == m_charHead) {
        m_charAvailable = false;
    }
    return ch;
}

bool KeyboardController::hasChar() const {
    return m_charAvailable;
}

void KeyboardController::reset() {
    m_matrix.fill(std::array<bool, 8>{});
    m_charBuffer.fill(0);
    m_charHead = 0;
    m_charTail = 0;
    m_charAvailable = false;
    m_shiftHeld = false;
    m_controlHeld = false;
    m_capsLock = false;
}

AppleKey KeyboardController::sdlToAppleKey(SDL_Scancode scancode) const {
    switch (scancode) {
        // Letters
        case SDL_SCANCODE_A: return AppleKey::A;
        case SDL_SCANCODE_B: return AppleKey::B;
        case SDL_SCANCODE_C: return AppleKey::C;
        case SDL_SCANCODE_D: return AppleKey::D;
        case SDL_SCANCODE_E: return AppleKey::E;
        case SDL_SCANCODE_F: return AppleKey::F;
        case SDL_SCANCODE_G: return AppleKey::G;
        case SDL_SCANCODE_H: return AppleKey::H;
        case SDL_SCANCODE_I: return AppleKey::I;
        case SDL_SCANCODE_J: return AppleKey::J;
        case SDL_SCANCODE_K: return AppleKey::K;
        case SDL_SCANCODE_L: return AppleKey::L;
        case SDL_SCANCODE_M: return AppleKey::M;
        case SDL_SCANCODE_N: return AppleKey::N;
        case SDL_SCANCODE_O: return AppleKey::O;
        case SDL_SCANCODE_P: return AppleKey::P;
        case SDL_SCANCODE_Q: return AppleKey::Q;
        case SDL_SCANCODE_R: return AppleKey::R;
        case SDL_SCANCODE_S: return AppleKey::S;
        case SDL_SCANCODE_T: return AppleKey::T;
        case SDL_SCANCODE_U: return AppleKey::U;
        case SDL_SCANCODE_V: return AppleKey::V;
        case SDL_SCANCODE_W: return AppleKey::W;
        case SDL_SCANCODE_X: return AppleKey::X;
        case SDL_SCANCODE_Y: return AppleKey::Y;
        case SDL_SCANCODE_Z: return AppleKey::Z;

        // Numbers
        case SDL_SCANCODE_0: return AppleKey::ZERO;
        case SDL_SCANCODE_1: return AppleKey::ONE;
        case SDL_SCANCODE_2: return AppleKey::TWO;
        case SDL_SCANCODE_3: return AppleKey::THREE;
        case SDL_SCANCODE_4: return AppleKey::FOUR;
        case SDL_SCANCODE_5: return AppleKey::FIVE;
        case SDL_SCANCODE_6: return AppleKey::SIX;
        case SDL_SCANCODE_7: return AppleKey::SEVEN;
        case SDL_SCANCODE_8: return AppleKey::EIGHT;
        case SDL_SCANCODE_9: return AppleKey::NINE;

        // Special
        case SDL_SCANCODE_RETURN: return AppleKey::RETURN;
        case SDL_SCANCODE_ESCAPE: return AppleKey::ESCAPE;
        case SDL_SCANCODE_SPACE: return AppleKey::SPACE;
        case SDL_SCANCODE_TAB: return AppleKey::TAB;
        case SDL_SCANCODE_CAPSLOCK: return AppleKey::CAPS_LOCK;
        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT: return AppleKey::SHIFT;
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL: return AppleKey::CONTROL;
        case SDL_SCANCODE_DELETE:
        case SDL_SCANCODE_BACKSPACE: return AppleKey::DELETE;

        // Punctuation
        case SDL_SCANCODE_PERIOD: return AppleKey::PERIOD;
        case SDL_SCANCODE_COMMA: return AppleKey::COMMA;
        case SDL_SCANCODE_SEMICOLON: return AppleKey::SEMICOLON;
        case SDL_SCANCODE_EQUALS: return AppleKey::EQUALS;

        default: return AppleKey::A;
    }
}

void KeyboardController::keyToMatrix(AppleKey key, uint8_t& row, uint8_t& col) const {
    // Apple IIe keyboard matrix layout
    // Row 0: Q W E R T Y U I O P
    // Row 1: A S D F G H J K L
    // Row 2: Z X C V B N M
    // Row 3: 1 2 3 4 5 6 7 8 9 0
    // Row 4: Shift, Control, Return, Space, etc.
    // Row 5: Function keys
    // Row 6: Numeric keypad

    switch (key) {
        // Row 0
        case AppleKey::Q: row = 0; col = 0; break;
        case AppleKey::W: row = 0; col = 1; break;
        case AppleKey::E: row = 0; col = 2; break;
        case AppleKey::R: row = 0; col = 3; break;
        case AppleKey::T: row = 0; col = 4; break;
        case AppleKey::Y: row = 0; col = 5; break;
        case AppleKey::U: row = 0; col = 6; break;
        case AppleKey::I: row = 0; col = 7; break;
        case AppleKey::O: row = 1; col = 0; break;
        case AppleKey::P: row = 1; col = 1; break;

        // Row 1
        case AppleKey::A: row = 2; col = 0; break;
        case AppleKey::S: row = 2; col = 1; break;
        case AppleKey::D: row = 2; col = 2; break;
        case AppleKey::F: row = 2; col = 3; break;
        case AppleKey::G: row = 2; col = 4; break;
        case AppleKey::H: row = 2; col = 5; break;
        case AppleKey::J: row = 2; col = 6; break;
        case AppleKey::K: row = 2; col = 7; break;
        case AppleKey::L: row = 3; col = 0; break;

        // Row 2
        case AppleKey::Z: row = 4; col = 0; break;
        case AppleKey::X: row = 4; col = 1; break;
        case AppleKey::C: row = 4; col = 2; break;
        case AppleKey::V: row = 4; col = 3; break;
        case AppleKey::B: row = 4; col = 4; break;
        case AppleKey::N: row = 4; col = 5; break;
        case AppleKey::M: row = 4; col = 6; break;

        // Row 3 (numbers)
        case AppleKey::ONE: row = 3; col = 1; break;
        case AppleKey::TWO: row = 3; col = 2; break;
        case AppleKey::THREE: row = 3; col = 3; break;
        case AppleKey::FOUR: row = 3; col = 4; break;
        case AppleKey::FIVE: row = 3; col = 5; break;
        case AppleKey::SIX: row = 3; col = 6; break;
        case AppleKey::SEVEN: row = 3; col = 7; break;
        case AppleKey::EIGHT: row = 5; col = 0; break;
        case AppleKey::NINE: row = 5; col = 1; break;
        case AppleKey::ZERO: row = 5; col = 2; break;

        // Row 4 (modifiers)
        case AppleKey::SHIFT: row = 5; col = 3; break;
        case AppleKey::CONTROL: row = 5; col = 4; break;
        case AppleKey::RETURN: row = 5; col = 5; break;
        case AppleKey::SPACE: row = 5; col = 6; break;
        case AppleKey::TAB: row = 5; col = 7; break;
        case AppleKey::DELETE: row = 6; col = 0; break;
        case AppleKey::ESCAPE: row = 6; col = 1; break;
        case AppleKey::CAPS_LOCK: row = 6; col = 2; break;

        // Punctuation
        case AppleKey::PERIOD: row = 6; col = 3; break;
        case AppleKey::COMMA: row = 6; col = 4; break;
        case AppleKey::SEMICOLON: row = 6; col = 5; break;
        case AppleKey::EQUALS: row = 6; col = 6; break;

        default: row = 0; col = 0; break;
    }
}

uint8_t KeyboardController::keyToChar(AppleKey key) const {
    // Convert Apple key to ASCII character
    // Take shift and caps lock into account

    bool shift = m_shiftHeld;
    bool caps = m_capsLock;

    switch (key) {
        case AppleKey::A: return shift ? 'A' : (caps ? 'A' : 'a');
        case AppleKey::B: return shift ? 'B' : 'b';
        case AppleKey::C: return shift ? 'C' : 'c';
        case AppleKey::D: return shift ? 'D' : 'd';
        case AppleKey::E: return shift ? 'E' : 'e';
        case AppleKey::F: return shift ? 'F' : 'f';
        case AppleKey::G: return shift ? 'G' : 'g';
        case AppleKey::H: return shift ? 'H' : 'h';
        case AppleKey::I: return shift ? 'I' : 'i';
        case AppleKey::J: return shift ? 'J' : 'j';
        case AppleKey::K: return shift ? 'K' : 'k';
        case AppleKey::L: return shift ? 'L' : 'l';
        case AppleKey::M: return shift ? 'M' : 'm';
        case AppleKey::N: return shift ? 'N' : 'n';
        case AppleKey::O: return shift ? 'O' : 'o';
        case AppleKey::P: return shift ? 'P' : 'p';
        case AppleKey::Q: return shift ? 'Q' : 'q';
        case AppleKey::R: return shift ? 'R' : 'r';
        case AppleKey::S: return shift ? 'S' : 's';
        case AppleKey::T: return shift ? 'T' : 't';
        case AppleKey::U: return shift ? 'U' : 'u';
        case AppleKey::V: return shift ? 'V' : 'v';
        case AppleKey::W: return shift ? 'W' : 'w';
        case AppleKey::X: return shift ? 'X' : 'x';
        case AppleKey::Y: return shift ? 'Y' : 'y';
        case AppleKey::Z: return shift ? 'Z' : 'z';

        case AppleKey::ONE: return shift ? '!' : '1';
        case AppleKey::TWO: return shift ? '@' : '2';
        case AppleKey::THREE: return shift ? '#' : '3';
        case AppleKey::FOUR: return shift ? '$' : '4';
        case AppleKey::FIVE: return shift ? '%' : '5';
        case AppleKey::SIX: return shift ? '^' : '6';
        case AppleKey::SEVEN: return shift ? '&' : '7';
        case AppleKey::EIGHT: return shift ? '*' : '8';
        case AppleKey::NINE: return shift ? '(' : '9';
        case AppleKey::ZERO: return shift ? ')' : '0';

        case AppleKey::SPACE: return ' ';
        case AppleKey::PERIOD: return shift ? '>' : '.';
        case AppleKey::COMMA: return shift ? '<' : ',';
        case AppleKey::SEMICOLON: return shift ? ':' : ';';
        case AppleKey::EQUALS: return shift ? '+' : '=';

        default: return 0;
    }
}

} // namespace apple2e
