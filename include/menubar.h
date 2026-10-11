#pragma once

#include "command.h"

#include <SDL.h>

#include <functional>
#include <string>
#include <vector>

namespace apple2e {

// Menu bar drawn inside the emulator's window, for systems where the menus
// belong to the window (Linux, Windows; macOS has its own menu bar). It lies
// over the top edge of the picture, which is border in both monitor styles,
// and can be hidden.
class MenuBar {
public:
    static constexpr int kHeight = 12;

    // `run` carries out a chosen command; `isOn` tells which on/off items
    // carry a tick
    MenuBar(std::function<void(Command)> run, std::function<bool(Command)> isOn);

    void setVisible(bool visible);
    bool visible() const { return m_visible; }

    // `width` is the window's, in renderer units
    void draw(SDL_Renderer* renderer, int width) const;

    // Mouse and key events of the window; true if the menu used the event
    bool handleEvent(const SDL_Event& event);

private:
    struct Item {
        std::string label;  // empty: a separator line
        Command command = Command::Reset;
        std::string shortcut;
        bool checkable = false;
    };
    struct Menu {
        std::string title;
        std::vector<Item> items;
        int x = 0;  // of its title in the bar
    };

    SDL_Rect titleRect(int menu) const;
    SDL_Rect dropRect(int menu) const;
    SDL_Rect itemRect(int menu, int item) const;
    int menuAt(int x, int y) const;
    int itemAt(int x, int y) const;

    std::function<void(Command)> m_run;
    std::function<bool(Command)> m_isOn;
    std::vector<Menu> m_menus;
    bool m_visible = true;
    int m_open = -1;   // menu whose list is showing
    int m_hover = -1;  // item under the pointer in that list
};

} // namespace apple2e
