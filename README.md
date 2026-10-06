# Apple IIe Emulator

Enhanced Apple IIe emulator in C++20 with SDL2.

- 65C02 CPU, validated against Klaus Dormann's functional test suites
- 64 KB main + 64 KB auxiliary RAM, language card, IIe MMU soft switches
- 40/80-column text, lo-res, hi-res (NTSC artifact colour), mixed mode
- Keyboard, Open/Solid Apple buttons, 1-bit speaker
- Paddles/joystick via the mouse or a game controller
- Colour or green-phosphor monitor (switch in the side panel, Cmd+G or `--green`)
- Disk II controller in slot 6 with two drives (.dsk/.do/.po/.nib, read and write),
  shown in a side panel; boots DOS 3.3 and ProDOS

## Build

Requires CMake 3.20+ and SDL2 (`brew install sdl2`).

```sh
cmake -S . -B build
cmake --build build -j
```

## Run

A ROM image is needed (16 KB `$C000-$FFFF`, or a 32 KB dump whose upper half
is that image). It is not included in the repository.

```sh
./build/apple2e_emulator apple2e.rom
```

Without a path it looks for `apple2e.rom` in the current directory, next to
the executable and in the executable's parent folder, so `./apple2e_emulator`
also works from inside `build/`.

| Key | Action |
|-----|--------|
| F12 | RESET (CTRL-RESET) |
| Shift+F12 | Power cycle |
| Cmd+1 / Cmd+2 | Insert a disk in drive 1 / 2 |
| Cmd+D | Drive sounds on/off |
| Cmd+G | Colour / green monitor |
| Cmd+V | Paste text |
| Cmd+Q | Quit |
| Caps Lock | Toggle Apple CAPS LOCK (on by default) |
| Left / Right Alt | Open Apple / Solid Apple (buttons 0 / 1) |

### Paddles and joystick

- **Mouse** over the Apple screen: X is paddle 0, Y is paddle 1; left and
  right click are buttons 0 and 1.
- **Game controller** (Xbox, PlayStation, ...): left stick is paddles 0/1,
  right stick paddles 2/3; A, B and X are buttons 0, 1 and 2.

Whichever moved last sets the position. Try it in BASIC with
`PRINT PDL(0), PDL(1)`.

### Disks

The Disk II needs its 256-byte boot ROM (341-0027) as `disk2.rom`, looked up
in the same places as `apple2e.rom`. Without it the emulator runs with no disk
controller and boots into BASIC.

In the side panel, click a drive to choose a disk image, right-click it to
eject, or drop an image file onto it. Disks can also be inserted at startup:

```sh
./build/apple2e_emulator --disk1 "DOS 3.3.dsk" --disk2 data.dsk
```

Changes are written back to the image file when the disk is ejected or the
emulator quits (a `*` before the name means unsaved changes). Images whose file
is read-only are write-protected. Sector images are saved only if every sector
still decodes, so a disk that fails to decode is never overwritten.

### Drive sounds

If a `sounds/` folder is found (same places as the ROMs), the drive plays its
mechanical noises: `Spin_Sound.wav` loops while the disk spins,
`Read_1_Sound.wav`/`Read_2_Sound.wav` click on each head step, and
`Grunt_Grind_1_Sound.wav`/`Grunt_Grind_2_Sound.wav` play when the head hits the
track 0 stop. They must be 16-bit PCM WAV files. The samples are not included
in the repository.

### Headless mode

Useful for scripted checks: runs N frames (60 per second) without a window,
prints the text screen and optionally saves a screenshot.

```sh
./build/apple2e_emulator --headless 300 --type $'PRINT 2+2\n' --screenshot out.bmp
./build/apple2e_emulator --headless 1500 --disk1 dos33.dsk --type-delay 700 --type $'CATALOG\n'
```

## CPU tests

Download `6502_functional_test.bin` and `65C02_extended_opcodes_test.bin` from
[Klaus2m5/6502_65C02_functional_tests](https://github.com/Klaus2m5/6502_65C02_functional_tests/tree/master/bin_files), then:

```sh
cmake -S . -B build -DKLAUS_TESTS_DIR=/path/to/bin_files
cmake --build build -j
ctest --test-dir build
```
