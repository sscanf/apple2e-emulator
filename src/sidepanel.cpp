#include "sidepanel.h"

#include "disk2.h"
#include "font.h"
#include "video.h"

#include <cstdio>
#include <filesystem>
#include <thread>
#include <vector>

namespace apple2e {

namespace {

constexpr int kCharWidth = 6;
constexpr int kDriveTop = 16;
constexpr int kDriveSpacing = 156;
constexpr int kBodyWidth = 144;
constexpr int kBodyHeight = 92;
constexpr int kNameChars = kBodyWidth / kCharWidth;

constexpr SDL_Color kPanelBg = {0x1C, 0x1C, 0x1E, 0xFF};
constexpr SDL_Color kBody = {0xD8, 0xCF, 0xB6, 0xFF};
constexpr SDL_Color kBodyEdge = {0x8C, 0x84, 0x70, 0xFF};
constexpr SDL_Color kOpening = {0x2A, 0x2A, 0x2A, 0xFF};
constexpr SDL_Color kSlot = {0x05, 0x05, 0x05, 0xFF};
constexpr SDL_Color kLatch = {0x45, 0x42, 0x3C, 0xFF};
constexpr SDL_Color kLatchHighlight = {0x70, 0x6C, 0x62, 0xFF};
constexpr SDL_Color kLabel = {0x3A, 0x36, 0x30, 0xFF};
constexpr SDL_Color kLedOn = {0xFF, 0x30, 0x20, 0xFF};
constexpr SDL_Color kLedOff = {0x5A, 0x18, 0x14, 0xFF};
constexpr SDL_Color kText = {0xD0, 0xD0, 0xD0, 0xFF};
constexpr SDL_Color kDimText = {0x78, 0x78, 0x78, 0xFF};
constexpr SDL_Color kWarnText = {0xE8, 0xC0, 0x40, 0xFF};
constexpr SDL_Color kErrorText = {0xFF, 0x60, 0x50, 0xFF};
constexpr SDL_Color kGreenText = {0x33, 0xFF, 0x33, 0xFF};
constexpr SDL_Color kSwitchTrack = {0x3A, 0x3A, 0x3C, 0xFF};
constexpr SDL_Color kSwitchKnob = {0xD0, 0xD0, 0xD0, 0xFF};

void fill(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &rect);
}

void drawText(SDL_Renderer* r, int x, int y, const std::string& text, SDL_Color c) {
    std::vector<SDL_Rect> dots;
    for (char ch : text) {
        auto code = static_cast<unsigned char>(ch);
        if (code >= 0x20 && code < 0x80) {
            for (int row = 0; row < 7; row++) {
                uint8_t bits = kFont[code - 0x20][row];
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

// Split text into lines of at most `width` characters, ellipsizing past `maxLines`
std::vector<std::string> wrap(const std::string& text, size_t width, size_t maxLines) {
    std::vector<std::string> lines;
    for (size_t i = 0; i < text.size() && lines.size() < maxLines; i += width) {
        lines.push_back(text.substr(i, width));
    }
    if (text.size() > width * maxLines) lines.back().replace(width - 3, 3, "...");
    return lines;
}

std::string runCommand(const std::string& command) {
    std::string output;
#if defined(__APPLE__) || defined(__linux__)
    if (FILE* pipe = popen(command.c_str(), "r")) {
        char buf[512];
        while (fgets(buf, sizeof(buf), pipe)) output += buf;
        pclose(pipe);
    }
#else
    (void)command;
#endif
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) output.pop_back();
    return output;
}

std::string shellQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) out += (c == '\'') ? std::string("'\\''") : std::string(1, c);
    return out + "'";
}

std::string appleScriptString(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

// Native file chooser; returns an empty string if cancelled or unsupported.
// Blocks until the dialog closes.
std::string openFileDialog(const std::string& prompt, const std::string& directory) {
#if defined(__APPLE__)
    std::string script = "POSIX path of (choose file with prompt " + appleScriptString(prompt);
    if (!directory.empty()) script += " default location (POSIX file " + appleScriptString(directory) + ")";
    script += ")";
    return runCommand("osascript -e " + shellQuote(script) + " 2>/dev/null");
#elif defined(__linux__)
    std::string command = "zenity --file-selection --title=" + shellQuote(prompt) +
                          " --file-filter='Disk images | *.dsk *.do *.po *.nib *.DSK *.DO *.PO *.NIB'";
    if (!directory.empty()) command += " --filename=" + shellQuote(directory + "/");
    return runCommand(command + " 2>/dev/null");
#else
    (void)prompt;
    (void)directory;
    return {};
#endif
}

} // namespace

SidePanel::SidePanel(Disk2Controller* controller, VideoController& video, int x, int height)
    : m_controller(controller), m_video(video), m_x(x), m_height(height) {}

SDL_Rect SidePanel::monitorSwitchRect() const {
    return {m_x + 8, m_height - 86, kBodyWidth, 14};
}

SDL_Rect SidePanel::driveRect(int drive) const {
    // Body plus the file name lines underneath
    return {m_x + 8, kDriveTop + drive * kDriveSpacing, kBodyWidth, kBodyHeight + 30};
}

int SidePanel::driveAt(int x, int y) const {
    SDL_Point p = {x, y};
    for (int d = 0; d < Disk2Controller::kDrives; d++) {
        SDL_Rect r = driveRect(d);
        if (SDL_PointInRect(&p, &r)) return d;
    }
    return -1;
}

void SidePanel::insert(int drive, const std::string& path) {
    m_message = m_controller->insert(drive, path);
    m_lastDirectory = std::filesystem::path(path).parent_path().string();
}

void SidePanel::eject(int drive) {
    m_message = m_controller->eject(drive);
}

void SidePanel::chooseDisk(int drive) {
    if (!m_controller || m_dialog) return;  // one dialog at a time

    // The thread only touches the shared result, so it may outlive the panel
    // (e.g. quitting with the dialog still open)
    m_dialog = std::make_shared<DialogResult>();
    m_dialogDrive = drive;
    std::string prompt = "Insert a disk in drive " + std::to_string(drive + 1);
    std::thread([result = m_dialog, prompt, directory = m_lastDirectory] {
        result->path = openFileDialog(prompt, directory);
        result->done = true;
    }).detach();
}

void SidePanel::update() {
    if (!m_dialog || !m_dialog->done) return;
    if (!m_dialog->path.empty()) insert(m_dialogDrive, m_dialog->path);
    m_dialog.reset();
}

bool SidePanel::handleEvent(const SDL_Event& event, SDL_Renderer* renderer) {
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        SDL_Point p = {event.button.x, event.button.y};
        SDL_Rect sw = monitorSwitchRect();
        if (SDL_PointInRect(&p, &sw)) {
            m_video.setMonochrome(!m_video.monochrome());
            return true;
        }
    }

    if (!m_controller) return false;

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int drive = driveAt(event.button.x, event.button.y);
        if (drive < 0) return false;
        if (event.button.button == SDL_BUTTON_LEFT) chooseDisk(drive);
        if (event.button.button == SDL_BUTTON_RIGHT) eject(drive);
        return true;
    }

    if (event.type == SDL_DROPFILE) {
        // Drop events carry no position: use where the pointer is over the window
        int gx, gy, wx, wy;
        float lx, ly;
        SDL_GetGlobalMouseState(&gx, &gy);
        SDL_GetWindowPosition(SDL_RenderGetWindow(renderer), &wx, &wy);
        SDL_RenderWindowToLogical(renderer, gx - wx, gy - wy, &lx, &ly);
        int drive = driveAt(static_cast<int>(lx), static_cast<int>(ly));

        insert(drive < 0 ? 0 : drive, event.drop.file);
        SDL_free(event.drop.file);
        return true;
    }

    return false;
}

void SidePanel::drawDrive(SDL_Renderer* r, int drive) const {
    const DiskImage& disk = m_controller->disk(drive);
    SDL_Rect area = driveRect(drive);
    int x = area.x;
    int y = area.y;
    int cx = x + kBodyWidth / 2;

    // Case
    fill(r, {x, y, kBodyWidth, kBodyHeight}, kBodyEdge);
    fill(r, {x + 1, y + 1, kBodyWidth - 2, kBodyHeight - 2}, kBody);
    drawText(r, x + 8, y + 7, "DRIVE " + std::to_string(drive + 1), kLabel);

    // Door opening with the disk slot
    fill(r, {x + 8, y + 24, kBodyWidth - 16, 28}, kOpening);
    fill(r, {x + 14, y + 37, kBodyWidth - 28, 3}, kSlot);

    // Door latch: across the slot when a disk is in, raised when empty
    if (disk.loaded()) {
        fill(r, {cx - 7, y + 20, 14, 36}, kLatch);
        fill(r, {cx - 5, y + 22, 3, 32}, kLatchHighlight);
    } else {
        fill(r, {cx - 16, y + 18, 32, 7}, kLatch);
        fill(r, {cx - 14, y + 19, 28, 2}, kLatchHighlight);
    }

    // Activity light
    fill(r, {x + 10, y + 68, 10, 6}, m_controller->active(drive) ? kLedOn : kLedOff);
    drawText(r, x + 25, y + 68, "IN USE", kLabel);
    drawText(r, x + kBodyWidth - 50, y + 68, "disk II", kLabel);

    // Image name
    int textY = y + kBodyHeight + 6;
    if (m_dialog && m_dialogDrive == drive) {
        drawText(r, x, textY, "Choosing a disk...", kWarnText);
        return;
    }
    if (!disk.loaded()) {
        drawText(r, x, textY, "(empty)", kDimText);
        return;
    }
    std::string name = std::filesystem::path(disk.path()).filename().string();
    if (disk.dirty()) name = "*" + name;
    for (const auto& line : wrap(name, kNameChars, 2)) {
        drawText(r, x, textY, line, kText);
        textY += 10;
    }
    if (disk.writeProtected()) drawText(r, x, textY, "write-protected", kWarnText);
}

// Two-position slide switch: COLOR [==o] GREEN
void SidePanel::drawMonitorSwitch(SDL_Renderer* r) const {
    SDL_Rect area = monitorSwitchRect();
    bool green = m_video.monochrome();
    int x = area.x;
    int y = area.y;

    drawText(r, x, y + 4, "COLOR", green ? kDimText : kText);
    fill(r, {x + 36, y + 2, 28, 11}, kBodyEdge);
    fill(r, {x + 37, y + 3, 26, 9}, kSwitchTrack);
    fill(r, {green ? x + 51 : x + 37, y + 3, 12, 9}, green ? kGreenText : kSwitchKnob);
    drawText(r, x + 70, y + 4, "GREEN", green ? kGreenText : kDimText);
    drawText(r, x + 112, y + 4, "Cmd+G", kDimText);
}

void SidePanel::draw(SDL_Renderer* r) const {
    fill(r, {m_x, 0, kWidth, m_height}, kPanelBg);
    drawMonitorSwitch(r);

    if (!m_controller) {
        drawText(r, m_x + 8, 16, "No disk2.rom found:", kWarnText);
        drawText(r, m_x + 8, 28, "disk drives disabled", kDimText);
        return;
    }

    for (int d = 0; d < Disk2Controller::kDrives; d++) drawDrive(r, d);

    int y = m_height - 64;
    for (const auto& line : wrap(m_message, kNameChars - 2, 2)) {
        drawText(r, m_x + 8, y, line, kErrorText);
        y += 10;
    }
    drawText(r, m_x + 8, m_height - 34, "Click: insert disk", kDimText);
    drawText(r, m_x + 8, m_height - 24, "Right-click: eject", kDimText);
    drawText(r, m_x + 8, m_height - 14, "Or drop a file here", kDimText);
}

} // namespace apple2e
