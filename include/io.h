#pragma once

#include <cstdint>

namespace apple2e {

class Card;
struct SoftSwitches;
class KeyboardController;
class AudioController;

// NTSC timing: 65 cycles per scanline, 262 scanlines per frame
constexpr uint32_t kCyclesPerLine = 65;
constexpr uint32_t kLinesPerFrame = 262;
constexpr uint32_t kCyclesPerFrame = kCyclesPerLine * kLinesPerFrame;  // 17030
constexpr uint32_t kVisibleLines = 192;
constexpr double kCpuClockHz = 1020484.0;

// $C000-$C0FF: keyboard, speaker, soft switches and slot I/O
class IOController {
public:
    IOController(SoftSwitches& switches, KeyboardController& keyboard,
                 AudioController& audio, const uint64_t& cycles);

    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t val);

    void setCard(int slot, Card* card) { m_cards[slot] = card; }

private:
    // Switches that respond to both reads and writes
    void accessCommon(uint16_t addr, bool isRead);
    void accessLanguageCard(uint16_t addr, bool isRead);
    bool inVerticalBlank() const;

    SoftSwitches& m_sw;
    KeyboardController& m_keyboard;
    AudioController& m_audio;
    const uint64_t& m_cycles;
    Card* m_cards[8] = {};
};

} // namespace apple2e
