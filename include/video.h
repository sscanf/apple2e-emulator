#pragma once

#include <cstdint>
#include <functional>
#include <SDL2/SDL.h>
#include <array>

namespace apple2e {

// Apple IIe video modes
enum class VideoMode {
    OFF        = 0,  // Display off
    TEXT_40    = 1,  // 40-column text (24 rows)
    LORES      = 2,  // Lo-res graphics (160x192, 16 colors)
    HIRES      = 3,  // Hi-res graphics (560x192, 6 colors)
    TEXT_80    = 4,  // 80-column text (24 rows)
    COLOR_HIRES = 5, // Color hi-res (560x192)
    DOUBLE_HIRES = 6,// Double hi-res (560x384)
};

// Apple IIe character ROM (6x8 font)
// 256 characters, each 6 bytes wide, 8 rows tall
extern const uint8_t kCharacterROM[256][6];

// NTSC timing constants
constexpr uint32_t kNTSCClock = 14178952;  // Hz
constexpr uint32_t kPixelClock = kNTSCClock / 2; // ~7.089 MHz
constexpr double kVideoRefresh = 59.480;    // Hz (color subcarrier)
constexpr int kScreenWidth = 560;            // Native horizontal pixels
constexpr int kScreenHeight = 384;           // Max vertical lines

// Display memory layout
// $0400-$07DF: Page 1 text
// $07E0-$0BDF: Page 2 text
// $0400-$07DF: Page 1 lores (lower half)
// $0800-$0BDF: Page 2 lores (lower half)
// $2000-$27DF: Page 1 hires
// $2800-$2BDF: Page 2 hires

class VideoController {
public:
    VideoController();
    ~VideoController();

    // Initialize SDL renderer
    bool init(SDL_Window* window);

    // Set display mode
    void setMode(VideoMode mode);
    VideoMode getMode() const { return m_mode; }

    // Display on/off
    void setDisplayOn(bool on) { m_displayOn = on; }
    bool isDisplayOn() const { return m_displayOn; }

    // Memory access
    void setMemoryReadCallback(std::function<uint8_t(uint16_t)> cb);

    // Render current frame
    void render(SDL_Renderer* renderer);

    // Get display dimensions
    int getWidth() const { return m_displayOn ? kScreenWidth : 0; }
    int getHeight() const { return m_displayOn ? kScreenHeight : 0; }

    // Text mode cursor
    void setCursorVisible(bool visible);
    void setCursorPosition(uint8_t row, uint8_t col);

private:
    VideoMode m_mode = VideoMode::OFF;
    bool m_displayOn = false;

    // Frame buffer (scaled up)
    std::array<uint32_t, kScreenWidth * kScreenHeight> m_frameBuffer;

    // Memory callback
    std::function<uint8_t(uint16_t)> m_memoryRead;

    // Cursor state
    bool m_cursorVisible = true;
    uint8_t m_cursorRow = 0;
    uint8_t m_cursorCol = 0;

    // Character ROM lookup
    uint8_t lookupChar(uint8_t ch) const;

    // Render helpers
    void renderOff(SDL_Renderer* renderer);
    void renderText(SDL_Renderer* renderer);
    void renderLores(SDL_Renderer* renderer);
    void renderHires(SDL_Renderer* renderer);
    void renderColorHires(SDL_Renderer* renderer);

    // Color palettes
    static constexpr uint32_t kColors[16] = {
        0x000000, // 0: Black
        0x0000AA, // 1: Blue
        0x00AA00, // 2: Green
        0x00AAAA, // 3: Cyan
        0xAA0000, // 4: Red
        0xAA00AA, // 5: Magenta
        0xAA5500, // 6: Brown
        0xAAAAAA, // 7: Light gray
        0x555555, // 8: Dark gray
        0x5555FF, // 9: Light blue
        0x55FF55, // 10: Light green
        0x55FFFF, // 11: Light cyan
        0xFF5555, // 12: Light red
        0xFF55FF, // 13: Light magenta
        0xFFFF55, // 14: Yellow
        0xFFFFFF, // 15: White
    };

    // Hi-res colors (6 colors)
    static constexpr uint32_t kHiresColors[6] = {
        0x000000, // Black
        0x00AA00, // Green
        0xAA00AA, // Magenta
        0xAAAAAA, // Light gray
        0xAA5500, // Brown
        0xFFFFFF, // White
    };
};

} // namespace apple2e
