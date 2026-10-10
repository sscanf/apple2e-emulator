#pragma once

#include <SDL.h>

#include <cstdint>
#include <deque>
#include <string>

namespace apple2e {

class StateReader;
class StateWriter;

// Apple IIe keyboard as seen by software: a 7-bit ASCII latch at $C000 whose
// bit 7 (the strobe) is set when a key is pressed and cleared via $C010
class KeyboardController {
public:
    void reset();

    // Translate host keyboard events into Apple key presses
    void handleEvent(const SDL_Event& event);

    // Queue text to be typed (paste / scripted input); '\n' becomes RETURN,
    // kPauseChar waits half a second before typing on (for scripts that must
    // wait for a program to load) and kOpenAppleChar presses the next key
    // with Open Apple held
    static constexpr uint8_t kPauseChar = 0x10;
    static constexpr uint8_t kOpenAppleChar = 0x11;
    void queueText(const std::string& text);
    // Feeds the next queued character once software has consumed the last one
    void update();

    // $C000: latch with strobe in bit 7
    uint8_t data() const { return m_latch | (m_strobe ? 0x80 : 0x00); }
    // $C010: clears the strobe, bit 7 reports whether any key is held
    uint8_t clearStrobe();

    // $C061 / $C062
    bool openApple() const { return m_openApple || m_scriptedOpenApple > 0; }
    bool solidApple() const { return m_solidApple; }

    // Save states (see state.h)
    void saveState(StateWriter& w) const;
    void loadState(StateReader& r);

private:
    void press(uint8_t ascii);

    uint8_t m_latch = 0;
    bool m_strobe = false;
    int m_keysHeld = 0;
    bool m_openApple = false;
    bool m_solidApple = false;
    std::deque<uint8_t> m_pending;
    static constexpr int kPauseFrames = 30;
    int m_pauseFrames = 0;
    int m_scriptedOpenApple = 0;     // frames Open Apple stays held for a scripted key
    bool m_nextKeyOpenApple = false;

    // Option+key on the Mac: the base key, sent unless macOS turns the
    // combination into an ASCII character of its own (e.g. @ or [ on some layouts)
    uint8_t m_optionKey = 0;
    SDL_Keycode m_optionKeycode = SDLK_UNKNOWN;
};

} // namespace apple2e
