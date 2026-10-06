#pragma once

#include <cstdint>
#include <array>
#include <SDL2/SDL.h>

namespace apple2e {

// Apple IIe keyboard mapping
// The Apple IIe keyboard has 52 keys with special modifiers

// Key codes matching Apple IIe keyboard layout
enum class AppleKey : uint8_t {
    // Letters
    A = 0x04, B = 0x05, C = 0x06, D = 0x07, E = 0x08,
    F = 0x09, G = 0x0A, H = 0x0B, I = 0x0C, J = 0x0D,
    K = 0x0E, L = 0x0F, M = 0x10, N = 0x11, O = 0x12,
    P = 0x13, Q = 0x14, R = 0x15, S = 0x16, T = 0x17,
    U = 0x18, V = 0x19, W = 0x1A, X = 0x1B, Y = 0x1C,
    Z = 0x1D,

    // Numbers
    ZERO = 0x1E, ONE = 0x24, TWO = 0x25, THREE = 0x26,
    FOUR = 0x27, FIVE = 0x28, SIX = 0x29, SEVEN = 0x2A,
    EIGHT = 0x2B, NINE = 0x2C,

    // Special keys
    RETURN = 0x23, ESCAPE = 0x33, DELETE = 0x35,
    SPACE = 0x22, TAB = 0x30, CAPS_LOCK = 0x3E,
    SHIFT = 0x38, CONTROL = 0x36,

    // Function keys
    F1 = 0x3A, F2 = 0x3B, F3 = 0x3C, F4 = 0x3D,

    // Punctuation
    PERIOD = 0x34, COMMA = 0x2E, SEMICOLON = 0x20,
    EQUALS = 0x2D,

    // Numeric keypad
    NUM_0 = 0x44, NUM_1 = 0x45, NUM_2 = 0x46, NUM_3 = 0x47,
    NUM_4 = 0x48, NUM_5 = 0x49, NUM_6 = 0x4A, NUM_7 = 0x4B,
    NUM_8 = 0x4C, NUM_9 = 0x4E,
    NUM_PLUS = 0x43, NUM_MINUS = 0x42, NUM_MULTIPLY = 0x41,
    NUM_DIVIDE = 0x4F, NUM_DECIMAL = 0x4D,
    NUM_ENTER = 0x4B,
};

// Apple IIe keyboard controller
class KeyboardController {
public:
    KeyboardController();

    // Process SDL key event and update internal state
    void processKey(SDL_KeyboardEvent& event);

    // Read keyboard matrix (for PIA)
    uint8_t readMatrixRow(uint8_t row);

    // Read character from keyboard data register (for VIA)
    uint8_t readChar();
    bool hasChar() const;

    // Reset keyboard buffer
    void reset();

    // Get shift/control state
    bool isShiftHeld() const { return m_shiftHeld; }
    bool isControlHeld() const { return m_controlHeld; }

private:
    // 7 rows x 8 columns keyboard matrix
    std::array<std::array<bool, 8>, 7> m_matrix;

    // Keyboard data register (character buffer)
    std::array<uint8_t, 8> m_charBuffer;
    uint8_t m_charHead = 0;
    uint8_t m_charTail = 0;
    bool m_charAvailable = false;

    // Modifier states
    bool m_shiftHeld = false;
    bool m_controlHeld = false;
    bool m_capsLock = false;

    // Map SDL scancode to Apple IIe key
    AppleKey sdlToAppleKey(SDL_Scancode scancode) const;

    // Convert Apple key to matrix row/col
    void keyToMatrix(AppleKey key, uint8_t& row, uint8_t& col) const;

    // Convert Apple key to ASCII character
    uint8_t keyToChar(AppleKey key) const;
};

} // namespace apple2e
