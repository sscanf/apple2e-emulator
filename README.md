# Apple IIe Emulator

Enhanced Apple IIe emulator in C++20 with SDL2.

- 65C02 CPU, validated against Klaus Dormann's functional test suites
- 64 KB main + 64 KB auxiliary RAM, language card, IIe MMU soft switches
- 40/80-column text, lo-res, hi-res (NTSC artifact colour), mixed mode
- Keyboard, Open/Solid Apple buttons, 1-bit speaker
- Microsoft SoftCard (Z80) in slot 4 for CP/M; the Z80 passes ZEXDOC and ZEXALL
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

### CP/M

A Microsoft SoftCard is always installed in slot 4; insert a SoftCard CP/M
disk in drive 1 and the machine boots CP/M (56K 2.20B and 60K 2.23 tested,
with DIR, STAT, PIP, Turbo Pascal and WordStar). Programs that need an
80-column screen use the IIe's built-in 80-column firmware.

Note that SoftCard CP/M translates some control keys for the Apple II+
keyboard (for example CTRL-K types `[`); its CONFIGIO utility changes that.

### Drive sounds

The drive plays its mechanical noises: the motor while the disk spins, a
click on each head step and the grind of the head hitting the track 0 stop.
The repository includes recordings of a real Disk II (see
[assets/sounds](assets/sounds/README.md) about their origin).

To use your own, put them in a `sounds/` folder (same places as the ROMs; it is
ignored by git and takes priority), either with the same file names or as
`motor.wav`, `step1.wav`, `step2.wav` and `grind.wav` (16-bit PCM WAV).

### Headless mode

Useful for scripted checks: runs N frames (60 per second) without a window,
prints the text screen and optionally saves a screenshot.

```sh
./build/apple2e_emulator --headless 300 --type $'PRINT 2+2\n' --screenshot out.bmp
./build/apple2e_emulator --headless 1500 --disk1 dos33.dsk --type-delay 700 --type $'CATALOG\n'
```

In `--type` text, `\x10` waits half a second before typing on, for programs
that discard keys pressed while they load.

## CPU tests

Download `6502_functional_test.bin` and `65C02_extended_opcodes_test.bin` from
[Klaus2m5/6502_65C02_functional_tests](https://github.com/Klaus2m5/6502_65C02_functional_tests/tree/master/bin_files),
and `zexdoc.com` and `zexall.com` from
[anotherlin/z80emu](https://github.com/anotherlin/z80emu/tree/master/testfiles), then:

```sh
cmake -S . -B build -DKLAUS_TESTS_DIR=/path/to/bin_files -DZEX_TESTS_DIR=/path/to/zex
cmake --build build -j
ctest --test-dir build -j4   # the Z80 tests take about 45 s each
```

## License

The emulator source code is released under the [MIT License](LICENSE).
Apple ROM images and disk images are not part of this repository and are not
covered by it, and neither are the drive recordings in `assets/sounds` (see
[their notes](assets/sounds/README.md)).
