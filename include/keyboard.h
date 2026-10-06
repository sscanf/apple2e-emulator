#pragma once

#include <SDL.h>

#include <cstdint>
#include <deque>
#include <string>

namespace apple2e {

// Apple IIe keyboard as seen by software: a 7-bit ASCII latch at $C000 whose
// bit 7 (the strobe) is set when a key is pressed and cleared via $C010
class KeyboardController {
public:
    void reset();

    // Translate host keyboard events into Apple key presses
    void handleEvent(const SDL_Event& event);

    // Queue text to be typed (paste / scripted input); '\n' becomes RETURN
    void queueText(const std::string& text);
    // Feeds the next queued character once software has consumed the last one
    void update();

    // $C000: latch with strobe in bit 7
    uint8_t data() const { return m_latch | (m_strobe ? 0x80 : 0x00); }
    // $C010: clears the strobe, bit 7 reports whether any key is held
    uint8_t clearStrobe();

    // $C061 / $C062
    bool openApple() const { return m_openApple; }
    bool solidApple() const { return m_solidApple; }

private:
    void press(uint8_t ascii);

    uint8_t m_latch = 0;
    bool m_strobe = false;
    int m_keysHeld = 0;
    bool m_capsLock = true;  // Apple CAPS LOCK, on by default as BASIC wants
    bool m_openApple = false;
    bool m_solidApple = false;
    std::deque<uint8_t> m_pending;
};

} // namespace apple2e
