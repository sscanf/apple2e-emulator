#pragma once

#include <SDL.h>

#include <functional>

namespace apple2e {

// The macOS menu bar, with every emulator command. Choosing an item pushes an
// SDL event of type `eventType` whose `user.code` is the command's number
// (see Command in apple2e.h). `isOn` tells which of the on/off items carry a
// tick. Does nothing on other systems.
void installMacMenu(Uint32 eventType, std::function<bool(int command)> isOn);

} // namespace apple2e
