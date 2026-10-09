#pragma once

#include <SDL.h>

#include <vector>

namespace apple2e {

// Apple Monitor II look: the Apple picture on a curved, glowing CRT behind
// the monitor's bezel. Everything uses SDL_Renderer features (render
// targets, blend modes, geometry), so it runs on the GPU. Textures belong to
// one renderer: make one CrtDisplay per renderer.
class CrtDisplay {
public:
    // Monitor layout in renderer (logical) units, relative to its top-left
    static constexpr int kWidth = 690;   // bezel artwork
    static constexpr int kHeight = 512;
    static constexpr SDL_Rect kGlass = {41, 40, 608, 432};  // tube opening
    static constexpr int kMargin = 24;                      // glass around the picture
    // Where the 560x384 Apple picture sits (before the curvature)
    static constexpr SDL_Rect kPicture = {kGlass.x + kMargin, kGlass.y + kMargin, 560, 384};

    // `bezel` is the monitor artwork (kWidth x kHeight, any resolution, with a
    // transparent tube opening); null draws a plain case
    CrtDisplay(SDL_Renderer* renderer, SDL_Surface* bezel);
    ~CrtDisplay();
    CrtDisplay(const CrtDisplay&) = delete;
    CrtDisplay& operator=(const CrtDisplay&) = delete;

    // False if the renderer lacks what the effect needs (render targets)
    bool ok() const { return m_glass != nullptr; }

    // Draw the monitor with its top-left at `origin`. `frame` is the Apple
    // display texture (560x192); `green` selects the green phosphor glass.
    void draw(SDL_Texture* frame, SDL_Point origin, bool green);

private:
    void buildMesh();
    SDL_Texture* makeOverlay(int w, int h, bool reflection);

    SDL_Renderer* m_renderer;
    SDL_Texture* m_bezel = nullptr;
    SDL_Texture* m_glass = nullptr;      // picture + glow, 2x resolution
    SDL_Texture* m_glowSmall = nullptr;  // downsampled copies for the bloom
    SDL_Texture* m_glowTiny = nullptr;
    SDL_Texture* m_scanlines = nullptr;
    SDL_Texture* m_vignette = nullptr;
    SDL_Texture* m_reflection = nullptr;

    std::vector<SDL_Vertex> m_vertices;  // curved glass, at origin (0, 0)
    std::vector<int> m_indices;
};

} // namespace apple2e
