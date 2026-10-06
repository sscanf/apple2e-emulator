#pragma once

#include <SDL.h>

#include <array>
#include <cstdint>
#include <string>

namespace apple2e {

class Memory;
struct SoftSwitches;

// Generates the Apple IIe display from video memory and the IOU switches:
// 40/80-column text, lo-res and hi-res (with NTSC artifact colour), mixed mode
class VideoController {
public:
    static constexpr int kWidth = 560;   // 80 columns x 7 dots
    static constexpr int kHeight = 192;

    VideoController(const Memory& memory, const SoftSwitches& switches);
    ~VideoController();

    bool init(SDL_Renderer* renderer);

    // Draw the current frame into the framebuffer (advances flash timing)
    void renderFrame();
    // Upload the framebuffer and draw it scaled to the window
    void present(SDL_Renderer* renderer);

    // Text page contents as plain ASCII, 24 lines (for headless runs)
    std::string textDump() const;

    // Write the framebuffer to a BMP file (scanlines doubled, as displayed)
    bool saveScreenshot(const std::string& path) const;

private:
    void drawTextRow(int row, uint16_t base);
    void drawLoresRow(int row, uint16_t base);
    void drawHiresLine(int y, uint16_t base);
    void drawGlyph(int x, int y, int dotWidth, uint8_t ch);

    uint32_t* line(int y) { return &m_framebuffer[y * kWidth]; }

    const Memory& m_memory;
    const SoftSwitches& m_sw;

    std::array<uint32_t, kWidth * kHeight> m_framebuffer{};
    SDL_Texture* m_texture = nullptr;
    uint32_t m_frameCount = 0;
    bool m_flashInverse = false;
};

} // namespace apple2e
