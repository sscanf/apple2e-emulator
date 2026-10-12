#pragma once

namespace apple2e {

// Everything the emulator can be told to do from a hotkey or a menu
enum class Command {
    Reset,
    PowerCycle,
    InsertDisk1,
    InsertDisk2,
    EjectDisk1,
    EjectDisk2,
    QuickSave,
    QuickLoad,
    SaveStateAs,
    LoadStateFrom,
    Paste,
    ToggleDriveSounds,
    ToggleGreen,
    ToggleCrt,
    TogglePanel,
    ToggleMenuBar,
    Quit,
};

} // namespace apple2e
