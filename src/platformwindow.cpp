// System headers here define many short macros (X11's None, Bool, Status;
// Windows' min, max, near, far), so this code lives apart from the rest.
#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#include "platformwindow.h"

#if defined(__APPLE__)
#include <SDL_syswm.h>
#include <objc/message.h>
#include <objc/runtime.h>
#elif defined(_WIN32)
#include <SDL_syswm.h>
#elif defined(__linux__) && defined(__has_include)
#if __has_include(<X11/Xlib.h>)
#define APPLE2E_X11 1
#include <SDL_syswm.h>
#include <dlfcn.h>
#endif
#endif

#include <cstdlib>

namespace apple2e {

#if defined(__APPLE__)
namespace {

id cocoaWindow(SDL_Window *window) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_COCOA)
        return nullptr;
    return reinterpret_cast<id>(info.info.cocoa.window);
}

bool addChild(SDL_Window *parent, SDL_Window *child, long order) {
    id parentWindow = cocoaWindow(parent);
    id childWindow  = cocoaWindow(child);
    if (!parentWindow || !childWindow)
        return false;
    reinterpret_cast<void (*)(id, SEL, id, long)>(objc_msgSend)(parentWindow, sel_registerName("addChildWindow:ordered:"), childWindow, order);
    return true;
}

} // namespace
#endif

bool attachWindowBehind(SDL_Window *parent, SDL_Window *child) {
#if defined(__APPLE__)
    return addChild(parent, child, -1); // NSWindowBelow
#else
    (void)parent;
    (void)child;
    return false;
#endif
}

bool attachWindowAbove(SDL_Window *parent, SDL_Window *child) {
#if defined(__APPLE__)
    return addChild(parent, child, 1); // NSWindowAbove
#elif defined(_WIN32)
    SDL_SysWMinfo parentInfo, childInfo;
    SDL_VERSION(&parentInfo.version);
    SDL_VERSION(&childInfo.version);
    if (!SDL_GetWindowWMInfo(parent, &parentInfo) || !SDL_GetWindowWMInfo(child, &childInfo) || parentInfo.subsystem != SDL_SYSWM_WINDOWS
        || childInfo.subsystem != SDL_SYSWM_WINDOWS)
        return false;
    // An owned window stays in front of its owner and has no task bar button
    SetWindowLongPtr(childInfo.info.win.window, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(parentInfo.info.win.window));
    return true;
#elif defined(APPLE2E_X11)
    SDL_SysWMinfo parentInfo, childInfo;
    SDL_VERSION(&parentInfo.version);
    SDL_VERSION(&childInfo.version);
    if (!SDL_GetWindowWMInfo(parent, &parentInfo) || !SDL_GetWindowWMInfo(child, &childInfo) || parentInfo.subsystem != SDL_SYSWM_X11
        || childInfo.subsystem != SDL_SYSWM_X11)
        return false;
    // Window managers keep a transient window in front of the one it is
    // transient for. libX11 is looked up at run time, as SDL itself does, so
    // that the program still starts where it is not installed.
    static void *lib = dlopen("libX11.so.6", RTLD_LAZY);
    if (!lib)
        return false;
    using SetTransient = int (*)(Display *, Window, Window);
    using Flush        = int (*)(Display *);
    auto setTransient  = reinterpret_cast<SetTransient>(dlsym(lib, "XSetTransientForHint"));
    auto flush         = reinterpret_cast<Flush>(dlsym(lib, "XFlush"));
    if (!setTransient)
        return false;
    setTransient(childInfo.info.x11.display, childInfo.info.x11.window, parentInfo.info.x11.window);
    if (flush)
        flush(childInfo.info.x11.display);
    return true;
#else
    (void)parent;
    (void)child;
    return false;
#endif
}

bool makeWindowTransparent(SDL_Window *window, SDL_Renderer *renderer) {
#if defined(__APPLE__)
    id layer    = reinterpret_cast<id>(SDL_RenderGetMetalLayer(renderer));
    id nsWindow = cocoaWindow(window);
    if (!layer || !nsWindow)
        return false;
    auto setBool   = reinterpret_cast<void (*)(id, SEL, signed char)>(objc_msgSend);
    auto getObject = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend);
    auto setObject = reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend);
    id   clear     = getObject(reinterpret_cast<id>(objc_getClass("NSColor")), sel_registerName("clearColor"));
    setBool(nsWindow, sel_registerName("setOpaque:"), 0);
    setObject(nsWindow, sel_registerName("setBackgroundColor:"), clear);
    setBool(nsWindow, sel_registerName("setHasShadow:"), 0);
    setBool(layer, sel_registerName("setOpaque:"), 0);
    return true;
#else
    (void)window;
    (void)renderer;
    return false;
#endif
}

void clearWindowBackground(SDL_Window *window) {
#if defined(__APPLE__)
    id nsWindow = cocoaWindow(window);
    if (!nsWindow)
        return;
    auto getObject  = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend);
    auto setPointer = reinterpret_cast<void (*)(id, SEL, void *)>(objc_msgSend);
    id   view       = getObject(nsWindow, sel_registerName("contentView"));
    id   layer      = view ? getObject(view, sel_registerName("layer")) : nullptr;
    if (layer)
        setPointer(layer, sel_registerName("setBackgroundColor:"), nullptr);
#else
    (void)window;
#endif
}

bool preferX11() {
#if defined(__linux__)
    if (std::getenv("SDL_VIDEODRIVER"))
        return false; // the user chose
    setenv("SDL_VIDEODRIVER", "x11", 1);
    return true;
#else
    return false;
#endif
}

void undoPreferX11() {
#if defined(__linux__)
    unsetenv("SDL_VIDEODRIVER");
#endif
}

} // namespace apple2e
