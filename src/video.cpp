#include "video.h"

#include "font.h"
#include "memory.h"

#include <algorithm>
#include <cstring>

namespace apple2e {

namespace {

// Lo-res palette (ARGB), also the source of the hi-res artifact colours
constexpr uint32_t kPalette[16] = {
    0xFF000000, 0xFF901740, 0xFF402CA5, 0xFFD043E5,
    0xFF006940, 0xFF808080, 0xFF2F95E5, 0xFFBFABFF,
    0xFF405400, 0xFFD06A1A, 0xFF808080, 0xFFFF96BF,
    0xFF2FBC1A, 0xFFBFD35A, 0xFF6FE8BF, 0xFFFFFFFF,
};
constexpr uint32_t kBlack = kPalette[0];
constexpr uint32_t kWhite = kPalette[15];
constexpr uint32_t kViolet = kPalette[3];
constexpr uint32_t kGreen = kPalette[12];
constexpr uint32_t kBlue = kPalette[6];
constexpr uint32_t kOrange = kPalette[9];
constexpr uint32_t kGreenPhosphor = 0xFF33FF33;

// Text rows (and lo-res rows) are interleaved in groups of eight
uint16_t textRowOffset(int row) {
    return (row & 7) * 0x80 + (row >> 3) * 0x28;
}

uint16_t hiresLineOffset(int y) {
    return (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28;
}

struct DecodedChar {
    uint8_t ascii;   // $20-$7F
    bool inverse;
    bool flashing;
    bool mouseText;
};

// Map a screen byte to a glyph using the IIe character set layout:
//   $00-$3F inverse   $40-$7F flashing (or MouseText/inverse lowercase with ALTCHARSET)
//   $80-$FF normal    ($E0-$FF lowercase)
DecodedChar decodeChar(uint8_t ch, bool altcharset) {
    auto upper = [](uint8_t c) -> uint8_t {
        c &= 0x3F;
        return c < 0x20 ? c + 0x40 : c;
    };

    if (ch >= 0xE0) return {static_cast<uint8_t>(ch & 0x7F), false, false, false};
    if (ch >= 0x80) return {upper(ch), false, false, false};
    if (ch < 0x40) return {upper(ch), true, false, false};
    if (!altcharset) return {upper(ch), false, true, false};
    if (ch < 0x60) return {' ', false, false, true};
    return {ch, true, false, false};
}

} // namespace

VideoController::VideoController(const Memory& memory, const SoftSwitches& switches)
    : m_memory(memory), m_sw(switches) {}

VideoController::~VideoController() {
    if (m_texture) SDL_DestroyTexture(m_texture);
}

bool VideoController::init(SDL_Renderer* renderer) {
    m_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING, kWidth, kHeight);
    return m_texture != nullptr;
}

void VideoController::renderFrame() {
    // Flashing characters alternate roughly twice per second
    if (++m_frameCount % 16 == 0) m_flashInverse = !m_flashInverse;

    bool page2 = m_sw.page2 && !m_sw.store80;
    uint16_t textBase = page2 ? 0x0800 : 0x0400;
    uint16_t hiresBase = page2 ? 0x4000 : 0x2000;

    for (int row = 0; row < 24; row++) {
        bool textRow = m_sw.text || (m_sw.mixed && row >= 20);
        if (textRow) {
            drawTextRow(row, textBase);
        } else if (m_sw.hires) {
            for (int y = row * 8; y < row * 8 + 8; y++) drawHiresLine(y, hiresBase);
        } else {
            drawLoresRow(row, textBase);
        }
    }
}

void VideoController::draw(SDL_Renderer* renderer, const SDL_Rect& dst) {
    SDL_UpdateTexture(m_texture, nullptr, m_framebuffer.data(), kWidth * sizeof(uint32_t));
    SDL_RenderCopy(renderer, m_texture, nullptr, &dst);
}

uint32_t VideoController::foreground() const {
    return m_monochrome ? kGreenPhosphor : kWhite;
}

void VideoController::drawGlyph(int x, int y, int dotWidth, uint8_t ch) {
    DecodedChar dc = decodeChar(ch, m_sw.altcharset);
    bool inverse = dc.inverse || (dc.flashing && m_flashInverse);
    uint32_t fg = foreground();

    for (int row = 0; row < 8; row++) {
        uint8_t dots = (row < 7 && !dc.mouseText) ? kFont[dc.ascii - 0x20][row] << 1 : 0;
        if (inverse) dots ^= 0x7F;

        uint32_t* out = line(y + row) + x;
        for (int d = 0; d < 7; d++) {
            uint32_t color = (dots & (0x40 >> d)) ? fg : kBlack;
            for (int k = 0; k < dotWidth; k++) *out++ = color;
        }
    }
}

void VideoController::drawTextRow(int row, uint16_t base) {
    uint16_t addr = base + textRowOffset(row);
    const uint8_t* main = m_memory.mainRam();
    const uint8_t* aux = m_memory.auxRam();

    for (int col = 0; col < 40; col++) {
        if (m_sw.col80) {
            // 80 columns: aux holds the even columns, main the odd ones
            drawGlyph(col * 14, row * 8, 1, aux[addr + col]);
            drawGlyph(col * 14 + 7, row * 8, 1, main[addr + col]);
        } else {
            drawGlyph(col * 14, row * 8, 2, main[addr + col]);
        }
    }
}

void VideoController::drawLoresRow(int row, uint16_t base) {
    uint16_t addr = base + textRowOffset(row);
    const uint8_t* main = m_memory.mainRam();

    uint32_t fg = foreground();
    for (int col = 0; col < 40; col++) {
        uint8_t byte = main[addr + col];
        for (int y = 0; y < 8; y++) {
            // Low nibble is the upper block, high nibble the lower one
            uint8_t nibble = y < 4 ? (byte & 0x0F) : (byte >> 4);
            uint32_t* out = line(row * 8 + y) + col * 14;
            for (int k = 0; k < 14; k++) {
                if (m_monochrome) {
                    // The colour's 4-bit pattern is shifted out repeatedly
                    // at four times the colour-burst rate
                    int x = col * 14 + k;
                    *out++ = (nibble >> (x & 3)) & 1 ? fg : kBlack;
                } else {
                    *out++ = kPalette[nibble];
                }
            }
        }
    }
}

void VideoController::drawHiresLine(int y, uint16_t base) {
    const uint8_t* main = m_memory.mainRam();
    uint16_t addr = base + hiresLineOffset(y);

    // 40 bytes x 7 dots, LSB first; bit 7 selects the colour group
    std::array<bool, 280> on{};
    std::array<bool, 280> group2{};
    for (int col = 0; col < 40; col++) {
        uint8_t byte = main[addr + col];
        for (int bit = 0; bit < 7; bit++) {
            on[col * 7 + bit] = byte & (1 << bit);
            group2[col * 7 + bit] = byte & 0x80;
        }
    }

    if (m_monochrome) {
        // Each dot is two 560-resolution pixels; bit 7 delays the byte's dots by half a dot
        uint32_t fg = foreground();
        uint32_t* out = line(y);
        std::fill(out, out + kWidth, kBlack);
        for (int x = 0; x < 280; x++) {
            if (!on[x]) continue;
            int px = x * 2 + (group2[x] ? 1 : 0);
            out[px] = fg;
            if (px + 1 < kWidth) out[px + 1] = fg;
        }
        return;
    }

    auto artifact = [&](int x) {
        bool even = (x & 1) == 0;
        if (group2[x]) return even ? kBlue : kOrange;
        return even ? kViolet : kGreen;
    };

    uint32_t* out = line(y);
    for (int x = 0; x < 280; x++) {
        bool left = x > 0 && on[x - 1];
        bool right = x < 279 && on[x + 1];

        uint32_t color;
        if (on[x]) {
            color = (left || right) ? kWhite : artifact(x);
        } else if (left && right) {
            color = artifact(x - 1);  // a gap between two lit dots takes their colour
        } else {
            color = kBlack;
        }
        *out++ = color;
        *out++ = color;
    }
}

std::string VideoController::textDump() const {
    bool page2 = m_sw.page2 && !m_sw.store80;
    uint16_t base = page2 ? 0x0800 : 0x0400;
    const uint8_t* main = m_memory.mainRam();
    const uint8_t* aux = m_memory.auxRam();

    std::string out;
    for (int row = 0; row < 24; row++) {
        uint16_t addr = base + textRowOffset(row);
        std::string text;
        for (int col = 0; col < 40; col++) {
            if (m_sw.col80) text += static_cast<char>(decodeChar(aux[addr + col], m_sw.altcharset).ascii);
            text += static_cast<char>(decodeChar(main[addr + col], m_sw.altcharset).ascii);
        }
        text.erase(text.find_last_not_of(' ') + 1);
        out += text + '\n';
    }
    return out;
}

void VideoController::copyToSurface(SDL_Surface* surface) const {
    for (int y = 0; y < kHeight * 2 && y < surface->h; y++) {
        auto* dst = reinterpret_cast<uint8_t*>(surface->pixels) + y * surface->pitch;
        std::memcpy(dst, &m_framebuffer[(y / 2) * kWidth], kWidth * sizeof(uint32_t));
    }
}

} // namespace apple2e
