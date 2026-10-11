#pragma once

#include <SDL.h>

namespace apple2e {

// The few things about windows that SDL cannot do and each system does its
// own way. They all report failure where the system has no way to do them.

// Make `child` follow `parent` around and stay behind it, like a macOS
// drawer. Only Cocoa can keep two windows ordered like that.
bool attachWindowBehind(SDL_Window* parent, SDL_Window* child);

// Keep `child` in front of `parent` (and off the task bar): X11 windows,
// Windows and macOS. It does not make the child follow the parent, except
// on macOS.
bool attachWindowAbove(SDL_Window* parent, SDL_Window* child);

// Let what is behind `window` show through the pixels its renderer leaves
// transparent. Needs Cocoa and the Metal renderer.
bool makeWindowTransparent(SDL_Window* window, SDL_Renderer* renderer);

// SDL paints a transparent window's own layer black whenever it is
// redisplayed; take that away again (cheap enough to do on every frame)
void clearWindowBackground(SDL_Window* window);

// Call before SDL_Init. On Linux, prefer X11 (through XWayland on a Wayland
// desktop): Wayland does not let a program place its windows, which the
// side panel's window needs. SDL_VIDEODRIVER in the environment still wins.
// Returns true if it asked for X11; call undoPreferX11() if SDL_Init fails.
bool preferX11();
void undoPreferX11();

} // namespace apple2e
