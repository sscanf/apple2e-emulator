#pragma once

#include <SDL.h>

#include <array>
#include <cstdint>

namespace apple2e {

class StateReader;
class StateWriter;

// Game I/O connector: four paddles (analog, 0-255) and three push buttons.
// Driven by the mouse over the Apple screen (X/Y -> paddles 0/1, left/right
// button -> buttons 0/1) or by a game controller (sticks -> paddles 0-3,
// A/B/X -> buttons 0-2); whichever was used last wins.
class GameIO {
public:
    ~GameIO();

    // `screen` is the Apple display area in renderer (logical) coordinates
    void handleEvent(const SDL_Event& event, const SDL_Rect& screen);

    // $C070: start the paddle timers
    void trigger(uint64_t cycle) { m_triggerCycle = cycle; }
    // $C064-$C067: bit 7 stays set for a time proportional to the paddle value
    bool paddleTimerRunning(int paddle, uint64_t cycle) const;
    // $C061-$C063
    bool button(int n) const { return m_mouseButtons[n] || m_padButtons[n]; }

    // Save states (see state.h)
    void saveState(StateWriter& w) const;
    void loadState(StateReader& r);

private:
    void openController(int deviceIndex);

    std::array<uint8_t, 4> m_paddles = {128, 128, 128, 128};
    std::array<bool, 3> m_mouseButtons{};
    std::array<bool, 3> m_padButtons{};
    uint64_t m_triggerCycle = 0;
    SDL_GameController* m_controller = nullptr;
};

} // namespace apple2e
