#include "menubar.h"

#include "uitext.h"

#include <algorithm>

namespace apple2e {

namespace {

constexpr int kTitlePad        = 6; // either side of a title
constexpr int kItemHeight      = 11;
constexpr int kSeparatorHeight = 5;
constexpr int kTickWidth       = 9; // column for the ticks
constexpr int kShortcutGap     = 3; // characters between label and shortcut

constexpr SDL_Color kBarBg     = {0x2A, 0x2B, 0x2E, 0xFF};
constexpr SDL_Color kEdge      = {0x55, 0x56, 0x5A, 0xFF};
constexpr SDL_Color kHighlight = {0x2F, 0x5F, 0xA8, 0xFF};
constexpr SDL_Color kText      = {0xE0, 0xE0, 0xE0, 0xFF};
constexpr SDL_Color kDimText   = {0x90, 0x90, 0x94, 0xFF};

void fill(SDL_Renderer *r, const SDL_Rect &rect, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &rect);
}

int textWidth(const std::string &text) {
    return static_cast<int>(text.size()) * kCharWidth;
}

// The emulator's hotkeys use the key next to Alt. Windows keeps nearly all
// of those combinations for itself, so its menus show none.
std::string hotkey(const std::string &key) {
#ifdef _WIN32
    (void)key;
    return {};
#else
    return "Super+" + key;
#endif
}

} // namespace

MenuBar::MenuBar(std::function<void(Command)> run, std::function<bool(Command)> isOn)
    : m_run(std::move(run))
    , m_isOn(std::move(isOn)) {
    m_menus = {
        {"File",
         {{"Insert Disk in Drive 1...", Command::InsertDisk1, hotkey("1")},
          {"Insert Disk in Drive 2...", Command::InsertDisk2, hotkey("2")},
          {"Eject Drive 1", Command::EjectDisk1, hotkey("Shift+1")},
          {"Eject Drive 2", Command::EjectDisk2, hotkey("Shift+2")},
          {},
          {"Save State", Command::QuickSave, hotkey("S")},
          {"Load State", Command::QuickLoad, hotkey("L")},
          {"Save State As...", Command::SaveStateAs, hotkey("Shift+S")},
          {"Load State From...", Command::LoadStateFrom, hotkey("Shift+L")},
          {},
          {"Quit", Command::Quit, hotkey("Q")}}},
        {"Edit", {{"Paste", Command::Paste, hotkey("V")}}},
        {"Machine",
         {{"Reset", Command::Reset, "F12"},
          {"Power Cycle", Command::PowerCycle, "Shift+F12"},
          {},
          {"Drive Sounds", Command::ToggleDriveSounds, hotkey("D"), true}}},
        {"View",
         {{"Green Monitor", Command::ToggleGreen, hotkey("G"), true},
          {"Apple Monitor II (CRT)", Command::ToggleCrt, hotkey("M"), true},
          {"Side Panel", Command::TogglePanel, hotkey("B"), true},
          {},
          {"Menu Bar", Command::ToggleMenuBar, "F10", true}}},
    };
    int x = 2;
    for (auto &menu : m_menus) {
        menu.x = x;
        x += textWidth(menu.title) + 2 * kTitlePad;
    }
}

void MenuBar::setVisible(bool visible) {
    m_visible = visible;
    m_open = m_hover = -1;
}

SDL_Rect MenuBar::titleRect(int menu) const {
    const Menu &m = m_menus[menu];
    return {m.x, 0, textWidth(m.title) + 2 * kTitlePad, kHeight};
}

SDL_Rect MenuBar::dropRect(int menu) const {
    const Menu &m     = m_menus[menu];
    size_t      label = 0, shortcut = 0;
    int         height = 2;
    for (const Item &item : m.items) {
        label    = std::max(label, item.label.size());
        shortcut = std::max(shortcut, item.shortcut.size());
        height += item.label.empty() ? kSeparatorHeight : kItemHeight;
    }
    int chars = static_cast<int>(label + (shortcut ? kShortcutGap + shortcut : 0));
    return {m.x, kHeight, kTickWidth + chars * kCharWidth + 8, height};
}

SDL_Rect MenuBar::itemRect(int menu, int item) const {
    SDL_Rect drop = dropRect(menu);
    int      y    = drop.y + 1;
    for (int i = 0; i < item; i++) {
        y += m_menus[menu].items[i].label.empty() ? kSeparatorHeight : kItemHeight;
    }
    bool separator = m_menus[menu].items[item].label.empty();
    return {drop.x + 1, y, drop.w - 2, separator ? kSeparatorHeight : kItemHeight};
}

int MenuBar::menuAt(int x, int y) const {
    SDL_Point p = {x, y};
    for (int i = 0; i < static_cast<int>(m_menus.size()); i++) {
        SDL_Rect rect = titleRect(i);
        if (SDL_PointInRect(&p, &rect))
            return i;
    }
    return -1;
}

int MenuBar::itemAt(int x, int y) const {
    if (m_open < 0)
        return -1;
    SDL_Point p = {x, y};
    for (int i = 0; i < static_cast<int>(m_menus[m_open].items.size()); i++) {
        SDL_Rect rect = itemRect(m_open, i);
        if (!m_menus[m_open].items[i].label.empty() && SDL_PointInRect(&p, &rect))
            return i;
    }
    return -1;
}

void MenuBar::draw(SDL_Renderer *r, int width) const {
    if (!m_visible)
        return;
    fill(r, {0, 0, width, kHeight}, kBarBg);
    fill(r, {0, kHeight - 1, width, 1}, kEdge);
    for (int i = 0; i < static_cast<int>(m_menus.size()); i++) {
        SDL_Rect title = titleRect(i);
        if (i == m_open)
            fill(r, title, kHighlight);
        drawText(r, title.x + kTitlePad, 2, m_menus[i].title, kText);
    }
    if (m_open < 0)
        return;

    SDL_Rect drop = dropRect(m_open);
    fill(r, drop, kEdge);
    fill(r, {drop.x + 1, drop.y, drop.w - 2, drop.h - 1}, kBarBg);
    const auto &items = m_menus[m_open].items;
    for (int i = 0; i < static_cast<int>(items.size()); i++) {
        const Item &item = items[i];
        SDL_Rect    rect = itemRect(m_open, i);
        if (item.label.empty()) {
            fill(r, {rect.x + 3, rect.y + rect.h / 2, rect.w - 6, 1}, kEdge);
            continue;
        }
        bool hovered = i == m_hover;
        if (hovered)
            fill(r, rect, kHighlight);
        if (item.checkable && m_isOn && m_isOn(item.command)) {
            // A tick: short stroke down, long stroke up
            SDL_SetRenderDrawColor(r, kText.r, kText.g, kText.b, 255);
            SDL_RenderDrawLine(r, rect.x + 2, rect.y + 5, rect.x + 3, rect.y + 7);
            SDL_RenderDrawLine(r, rect.x + 3, rect.y + 7, rect.x + 6, rect.y + 2);
        }
        drawText(r, rect.x + kTickWidth, rect.y + 2, item.label, kText);
        drawText(r, rect.x + rect.w - 4 - textWidth(item.shortcut), rect.y + 2, item.shortcut, hovered ? kText : kDimText);
    }
}

bool MenuBar::handleEvent(const SDL_Event &event) {
    if (!m_visible)
        return false;

    if (event.type == SDL_KEYDOWN) {
        if (m_open >= 0 && event.key.keysym.sym == SDLK_ESCAPE) {
            m_open = m_hover = -1;
            return true;
        }
        return false;
    }
    if (event.type == SDL_MOUSEMOTION) {
        if (m_open < 0)
            return false;
        int menu = menuAt(event.motion.x, event.motion.y);
        if (menu >= 0)
            m_open = menu; // sweep along the bar
        m_hover = itemAt(event.motion.x, event.motion.y);
        return true;
    }
    if (event.type == SDL_MOUSEBUTTONUP) {
        return m_open >= 0 || event.button.y < kHeight;
    }
    if (event.type != SDL_MOUSEBUTTONDOWN)
        return false;

    int menu = menuAt(event.button.x, event.button.y);
    if (menu >= 0) {
        m_open  = menu == m_open ? -1 : menu;
        m_hover = -1;
        return true;
    }
    if (m_open < 0)
        return event.button.y < kHeight; // idle part of the bar

    int     item    = itemAt(event.button.x, event.button.y);
    Command command = item >= 0 ? m_menus[m_open].items[item].command : Command::Reset;
    m_open = m_hover = -1; // a click anywhere else just closes the list
    if (item >= 0 && m_run)
        m_run(command);
    return true;
}

} // namespace apple2e
