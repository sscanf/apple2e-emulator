#pragma once

#include "font.h"

#include <SDL.h>

#include <string>
#include <vector>

namespace apple2e {

// Text of the emulator's own interface (side panel, menus): the built-in
// 5x7 font, one renderer unit per dot
inline constexpr int kCharWidth = 6;

inline void drawText(SDL_Renderer* r, int x, int y, const std::string& text, SDL_Color c) {
    std::vector<SDL_Rect> dots;
    for (char ch : text) {
        auto code = static_cast<unsigned char>(ch);
        if (code >= 0x20 && code < 0x80) {
            for (int row = 0; row < 8; row++) {
                uint8_t bits = glyphRow(code, row);
                for (int col = 0; col < 5; col++) {
                    if (bits & (0x10 >> col)) dots.push_back({x + col, y + row, 1, 1});
                }
            }
        }
        x += kCharWidth;
    }
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRects(r, dots.data(), static_cast<int>(dots.size()));
}

} // namespace apple2e
