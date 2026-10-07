#pragma once

#include <SDL.h>

#include <array>
#include <cstdint>
#include <string>

namespace apple2e {

class Memory;
struct SoftSwitches;

// Generates the Apple IIe display from video memory and the IOU switches:
// 40/80-column text, lo-res and hi-res and their double (80-column) variants,
// mixed mode. On a colour monitor
// hi-res shows NTSC artifact colour; on a monochrome one every dot is visible
// and lo-res colours appear as dot patterns, as on the real hardware.
class VideoController {
public:
    static constexpr int kWidth = 560;   // 80 columns x 7 dots
    static constexpr int kHeight = 192;

    VideoController(const Memory& memory, const SoftSwitches& switches);
    ~VideoController();

    bool init(SDL_Renderer* renderer);

    // Use a real Apple IIe character generator ROM instead of the built-in
    // font: 4 KB, or 8 KB with the US set in the upper half (341-0161)
    bool loadCharacterRom(const std::string& path);

    // Monochrome (green phosphor) monitor instead of a colour one
    bool monochrome() const { return m_monochrome; }
    void setMonochrome(bool on) { m_monochrome = on; }

    // Draw the current frame into the framebuffer (advances flash timing)
    void renderFrame();
    // Upload the framebuffer and draw it into `dst` (renderer coordinates)
    void draw(SDL_Renderer* renderer, const SDL_Rect& dst);

    // Text page contents as plain ASCII, 24 lines (for headless runs)
    std::string textDump() const;

    // Copy the framebuffer into the top-left of an ARGB8888 surface,
    // scanlines doubled as displayed
    void copyToSurface(SDL_Surface* surface) const;

private:
    void drawTextRow(int row, uint16_t base);
    void drawLoresRow(int row, uint16_t base);
    void drawHiresLine(int y, uint16_t base);
    void drawDoubleLoresRow(int row, uint16_t base);
    void drawDoubleHiresLine(int y, uint16_t base);
    void drawGlyph(int x, int y, int dotWidth, uint8_t ch);
    uint32_t foreground() const;

    uint32_t* line(int y) { return &m_framebuffer[y * kWidth]; }

    const Memory& m_memory;
    const SoftSwitches& m_sw;

    std::array<uint32_t, kWidth * kHeight> m_framebuffer{};
    SDL_Texture* m_texture = nullptr;
    uint32_t m_frameCount = 0;
    bool m_flashInverse = false;
    bool m_monochrome = false;

    // Glyphs from the character ROM for ASCII $20-$7F: 8 rows of 7 dots, bit 6 leftmost
    bool m_hasCharRom = false;
    std::array<std::array<uint8_t, 8>, 96> m_romGlyphs{};
};

} // namespace apple2e
