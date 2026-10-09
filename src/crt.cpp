#include "crt.h"

#include <algorithm>
#include <cmath>

namespace apple2e {

namespace {

constexpr int kScale = 2;  // internal resolution of the glass, per logical unit
constexpr int kGlassW = CrtDisplay::kGlass.w * kScale;
constexpr int kGlassH = CrtDisplay::kGlass.h * kScale;
constexpr SDL_Rect kPictureInGlass = {CrtDisplay::kMargin * kScale, CrtDisplay::kMargin * kScale,
                                      560 * kScale, 384 * kScale};

constexpr int kMeshX = 32, kMeshY = 24;

// Unlit glass: the green phosphor tube has a dark teal faceplate
constexpr SDL_Color kGlassGreen = {0x0B, 0x20, 0x1A, 0xFF};
constexpr SDL_Color kGlassColour = {0x10, 0x12, 0x11, 0xFF};
constexpr SDL_Color kCase = {0xD8, 0xD2, 0xBB, 0xFF};
constexpr SDL_Color kRim = {0x14, 0x18, 0x15, 0xFF};

// Bloom: alpha of the two blurred copies added over the picture
constexpr Uint8 kGlowSmallAlpha = 120;
constexpr Uint8 kGlowTinyAlpha = 80;

SDL_Texture* makeTarget(SDL_Renderer* r, int w, int h) {
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
    if (t) SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
    return t;
}

} // namespace

CrtDisplay::CrtDisplay(SDL_Renderer* renderer, SDL_Surface* bezel) : m_renderer(renderer) {
    if (!SDL_RenderTargetSupported(renderer)) return;

    if (bezel) {
        m_bezel = SDL_CreateTextureFromSurface(renderer, bezel);
        if (m_bezel) SDL_SetTextureScaleMode(m_bezel, SDL_ScaleModeLinear);
    }

    m_glass = makeTarget(renderer, kGlassW, kGlassH);
    m_glowSmall = makeTarget(renderer, kPictureInGlass.w / 8, kPictureInGlass.h / 8);
    m_glowTiny = makeTarget(renderer, kPictureInGlass.w / 16, kPictureInGlass.h / 16);
    if (!m_glass || !m_glowSmall || !m_glowTiny) {
        if (m_glass) SDL_DestroyTexture(m_glass);
        m_glass = nullptr;
        return;
    }
    SDL_SetTextureBlendMode(m_glowSmall, SDL_BLENDMODE_ADD);
    SDL_SetTextureBlendMode(m_glowTiny, SDL_BLENDMODE_ADD);
    SDL_SetTextureAlphaMod(m_glowSmall, kGlowSmallAlpha);
    SDL_SetTextureAlphaMod(m_glowTiny, kGlowTinyAlpha);

    // Scanlines: each Apple line is 4 rows of the glass; darken the last one.
    // One texel per row (a texture is stretched, not tiled), applied with MOD.
    SDL_Surface* lines =
        SDL_CreateRGBSurfaceWithFormat(0, 1, kPictureInGlass.h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (lines) {
        const Uint8 levels[4] = {255, 255, 235, 150};
        for (int y = 0; y < kPictureInGlass.h; y++) {
            Uint8 level = levels[y % 4];
            static_cast<Uint32*>(lines->pixels)[y * lines->pitch / 4] =
                SDL_MapRGBA(lines->format, level, level, level, 255);
        }
        m_scanlines = SDL_CreateTextureFromSurface(renderer, lines);
        SDL_FreeSurface(lines);
        if (m_scanlines) SDL_SetTextureBlendMode(m_scanlines, SDL_BLENDMODE_MOD);
    }

    m_vignette = makeOverlay(kGlass.w / 2, kGlass.h / 2, false);
    m_reflection = makeOverlay(kGlass.w / 2, kGlass.h / 2, true);
    buildMesh();
}

CrtDisplay::~CrtDisplay() {
    for (SDL_Texture* t : {m_bezel, m_glass, m_glowSmall, m_glowTiny, m_scanlines, m_vignette, m_reflection}) {
        if (t) SDL_DestroyTexture(t);
    }
}

// Vignette: black, more opaque towards the edges (alpha blended).
// Reflection: a soft white highlight upper right, like a lamp on the glass (added).
SDL_Texture* CrtDisplay::makeOverlay(int w, int h, bool reflection) {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return nullptr;
    for (int y = 0; y < h; y++) {
        auto* row = reinterpret_cast<Uint32*>(static_cast<Uint8*>(s->pixels) + y * s->pitch);
        for (int x = 0; x < w; x++) {
            float nx = 2.0f * x / (w - 1) - 1.0f;
            float ny = 2.0f * y / (h - 1) - 1.0f;
            float a;
            if (reflection) {
                float dx = (nx - 0.42f) / 0.30f, dy = (ny + 0.42f) / 0.22f;
                float core = std::exp(-(dx * dx + dy * dy) * 2.0f);
                float wide = std::exp(-(dx * dx + dy * dy) * 0.35f);
                a = 9.0f * core + 3.0f * wide;
            } else {
                float r2 = nx * nx * 0.85f + ny * ny;
                a = 110.0f * std::pow(std::min(1.0f, r2 / 2.0f), 3.0f);
            }
            Uint8 alpha = static_cast<Uint8>(std::clamp(a, 0.0f, 255.0f));
            Uint8 c = reflection ? 255 : 0;
            row[x] = SDL_MapRGBA(s->format, c, c, c, alpha);
        }
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(m_renderer, s);
    SDL_FreeSurface(s);
    if (t) {
        SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
        SDL_SetTextureBlendMode(t, reflection ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
        if (reflection) SDL_SetTextureAlphaMod(t, 255);
    }
    return t;
}

// The glass is drawn as a grid whose texture coordinates follow a barrel
// distortion: straight lines of the picture bow outwards, and the picture's
// corners stay at the glass corners
void CrtDisplay::setCurvature(float curvature) {
    if (curvature == m_curvature) return;
    m_curvature = curvature;
    buildMesh();
}

void CrtDisplay::buildMesh() {
    m_vertices.clear();
    m_indices.clear();
    for (int j = 0; j <= kMeshY; j++) {
        for (int i = 0; i <= kMeshX; i++) {
            float u = static_cast<float>(i) / kMeshX;
            float v = static_cast<float>(j) / kMeshY;
            float nx = 2 * u - 1, ny = 2 * v - 1;
            float f = (1 + m_curvature * (nx * nx + ny * ny)) / (1 + 2 * m_curvature);
            SDL_Vertex vert;
            vert.position = {kGlass.x + u * kGlass.w, kGlass.y + v * kGlass.h};
            vert.color = {255, 255, 255, 255};
            vert.tex_coord = {0.5f + 0.5f * nx * f, 0.5f + 0.5f * ny * f};
            m_vertices.push_back(vert);
        }
    }
    for (int j = 0; j < kMeshY; j++) {
        for (int i = 0; i < kMeshX; i++) {
            int a = j * (kMeshX + 1) + i, b = a + 1, c = a + kMeshX + 1, d = c + 1;
            m_indices.insert(m_indices.end(), {a, b, c, b, d, c});
        }
    }
}

void CrtDisplay::draw(SDL_Texture* frame, SDL_Point origin, bool green) {
    SDL_Renderer* r = m_renderer;
    SDL_Texture* previousTarget = SDL_GetRenderTarget(r);

    // --- The glass, flat: phosphor picture over the unlit faceplate --------
    // Built on black so the scanlines only darken lit phosphor; the unlit
    // glass colour is added at the end and stays even
    SDL_SetRenderTarget(r, m_glass);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    SDL_BlendMode frameBlend;
    SDL_ScaleMode frameScale;
    SDL_GetTextureBlendMode(frame, &frameBlend);
    SDL_GetTextureScaleMode(frame, &frameScale);

    SDL_SetTextureBlendMode(frame, SDL_BLENDMODE_ADD);
    SDL_SetTextureScaleMode(frame, SDL_ScaleModeNearest);
    SDL_RenderCopy(r, frame, nullptr, &kPictureInGlass);
    if (m_scanlines) SDL_RenderCopy(r, m_scanlines, nullptr, &kPictureInGlass);

    // Bloom: blurred copies of the picture added around the lit dots
    SDL_SetTextureBlendMode(frame, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(frame, SDL_ScaleModeLinear);
    SDL_SetRenderTarget(r, m_glowSmall);
    SDL_RenderCopy(r, frame, nullptr, nullptr);
    SDL_SetRenderTarget(r, m_glowTiny);
    SDL_SetTextureBlendMode(m_glowSmall, SDL_BLENDMODE_NONE);
    SDL_RenderCopy(r, m_glowSmall, nullptr, nullptr);
    SDL_SetTextureBlendMode(m_glowSmall, SDL_BLENDMODE_ADD);

    SDL_SetRenderTarget(r, m_glass);
    // Exactly over the picture: the downsampling is what blurs it
    SDL_RenderCopy(r, m_glowSmall, nullptr, &kPictureInGlass);
    SDL_RenderCopy(r, m_glowTiny, nullptr, &kPictureInGlass);

    SDL_Color glass = green ? kGlassGreen : kGlassColour;
    SDL_BlendMode drawBlend;
    SDL_GetRenderDrawBlendMode(r, &drawBlend);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
    SDL_SetRenderDrawColor(r, glass.r, glass.g, glass.b, 255);
    SDL_RenderFillRect(r, nullptr);
    SDL_SetRenderDrawBlendMode(r, drawBlend);

    SDL_SetTextureBlendMode(frame, frameBlend);
    SDL_SetTextureScaleMode(frame, frameScale);
    SDL_SetRenderTarget(r, previousTarget);

    // --- The tube: case, curved glass, shading and the bezel on top ---------
    if (!m_bezel) {
        SDL_Rect body = {origin.x, origin.y, kWidth, kHeight};
        SDL_SetRenderDrawColor(r, kCase.r, kCase.g, kCase.b, 255);
        SDL_RenderFillRect(r, &body);
        SDL_Rect rim = {origin.x + kGlass.x - 6, origin.y + kGlass.y - 6, kGlass.w + 12, kGlass.h + 12};
        SDL_SetRenderDrawColor(r, kRim.r, kRim.g, kRim.b, 255);
        SDL_RenderFillRect(r, &rim);
    }

    std::vector<SDL_Vertex> vertices = m_vertices;
    for (auto& v : vertices) {
        v.position.x += origin.x;
        v.position.y += origin.y;
    }
    SDL_RenderGeometry(r, m_glass, vertices.data(), static_cast<int>(vertices.size()), m_indices.data(),
                       static_cast<int>(m_indices.size()));

    SDL_Rect glassRect = {origin.x + kGlass.x, origin.y + kGlass.y, kGlass.w, kGlass.h};
    if (m_vignette) SDL_RenderCopy(r, m_vignette, nullptr, &glassRect);
    if (m_reflection) SDL_RenderCopy(r, m_reflection, nullptr, &glassRect);

    if (m_bezel) {
        SDL_Rect body = {origin.x, origin.y, kWidth, kHeight};
        SDL_RenderCopy(r, m_bezel, nullptr, &body);
    }
}

} // namespace apple2e
