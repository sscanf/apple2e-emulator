#include "macmenu.h"

#include "apple2e.h"

#import <Cocoa/Cocoa.h>

using apple2e::Command;

// Target of every menu item: turns the choice into an SDL event, so the
// command runs on the emulator's side like a key press
@interface Apple2eMenuTarget : NSObject <NSMenuItemValidation> {
@public
  Uint32 eventType;
  std::function<bool(int)> isOn;
}
- (void)run:(id)sender;
@end

@implementation Apple2eMenuTarget
- (void)run:(id)sender {
  SDL_Event event;
  SDL_zero(event);
  event.type = eventType;
  event.user.code = static_cast<Sint32>([sender tag]);
  SDL_PushEvent(&event);
}

- (BOOL)validateMenuItem:(NSMenuItem *)item {
  switch (static_cast<Command>([item tag])) {
  case Command::ToggleGreen:
  case Command::ToggleCrt:
  case Command::TogglePanel:
  case Command::ToggleDriveSounds:
    [item setState:isOn && isOn(static_cast<int>([item tag])) ? NSControlStateValueOn
                                                               : NSControlStateValueOff];
    break;
  case Command::ToggleMenuBar: // the menu bar in the window: not on macOS
  default:
    break;
  }
  return YES;
}
@end

namespace apple2e {

namespace {

Apple2eMenuTarget *g_target = nil;

NSMenuItem *add(NSMenu *menu, NSString *title, Command command, NSString *key,
                NSEventModifierFlags modifiers = NSEventModifierFlagCommand) {
  NSMenuItem *item = [menu addItemWithTitle:title action:@selector(run:) keyEquivalent:key];
  [item setKeyEquivalentModifierMask:modifiers];
  [item setTarget:g_target];
  [item setTag:static_cast<NSInteger>(command)];
  return item;
}

NSMenu *addMenu(NSMenu *bar, NSString *title) {
  NSMenuItem *item = [bar addItemWithTitle:title action:nil keyEquivalent:@""];
  NSMenu *menu = [[NSMenu alloc] initWithTitle:title];
  [item setSubmenu:menu];
  return menu;
}

} // namespace

void installMacMenu(Uint32 eventType, std::function<bool(int command)> isOn) {
  @autoreleasepool {
    // Keep the system from adding dictation and emoji items to the Edit menu
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setBool:YES forKey:@"NSDisabledDictationMenuItem"];
    [defaults setBool:YES forKey:@"NSDisabledCharacterPaletteMenuItem"];

    g_target = [[Apple2eMenuTarget alloc] init];
    g_target->eventType = eventType;
    g_target->isOn = std::move(isOn);

    const NSEventModifierFlags cmd = NSEventModifierFlagCommand;
    const NSEventModifierFlags cmdShift = cmd | NSEventModifierFlagShift;
    unichar f12Char = NSF12FunctionKey;
    NSString *f12 = [NSString stringWithCharacters:&f12Char length:1];

    NSMenu *bar = [[NSMenu alloc] initWithTitle:@""];

    NSMenu *app = addMenu(bar, @"Apple IIe");
    [app addItemWithTitle:@"Hide Apple IIe" action:@selector(hide:) keyEquivalent:@"h"];
    NSMenuItem *others = [app addItemWithTitle:@"Hide Others"
                                        action:@selector(hideOtherApplications:)
                                 keyEquivalent:@"h"];
    [others setKeyEquivalentModifierMask:cmd | NSEventModifierFlagOption];
    [app addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
    [app addItem:[NSMenuItem separatorItem]];
    add(app, @"Quit Apple IIe", Command::Quit, @"q");

    NSMenu *file = addMenu(bar, @"File");
    add(file, @"Insert Disk in Drive 1…", Command::InsertDisk1, @"1");
    add(file, @"Insert Disk in Drive 2…", Command::InsertDisk2, @"2");
    add(file, @"Eject Drive 1", Command::EjectDisk1, @"1", cmdShift);
    add(file, @"Eject Drive 2", Command::EjectDisk2, @"2", cmdShift);
    [file addItem:[NSMenuItem separatorItem]];
    add(file, @"Save State", Command::QuickSave, @"s");
    add(file, @"Load State", Command::QuickLoad, @"l");
    add(file, @"Save State As…", Command::SaveStateAs, @"s", cmdShift);
    add(file, @"Load State From…", Command::LoadStateFrom, @"l", cmdShift);

    NSMenu *edit = addMenu(bar, @"Edit");
    add(edit, @"Paste", Command::Paste, @"v");

    NSMenu *machine = addMenu(bar, @"Machine");
    add(machine, @"Reset", Command::Reset, f12, 0);
    add(machine, @"Power Cycle", Command::PowerCycle, f12, NSEventModifierFlagShift);
    [machine addItem:[NSMenuItem separatorItem]];
    add(machine, @"Drive Sounds", Command::ToggleDriveSounds, @"d");

    NSMenu *view = addMenu(bar, @"View");
    add(view, @"Green Monitor", Command::ToggleGreen, @"g");
    add(view, @"Apple Monitor II (CRT)", Command::ToggleCrt, @"m");
    add(view, @"Side Panel", Command::TogglePanel, @"b");

    NSMenu *window = addMenu(bar, @"Window");
    [window addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@""];
    [window addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];

    [NSApp setMainMenu:bar];
    [NSApp setWindowsMenu:window];
  }
}

} // namespace apple2e
