# Apple IIe Emulator

Enhanced Apple IIe emulator in C++20 with SDL2.

- 65C02 CPU, validated against Klaus Dormann's functional test suites
- 64 KB main + 64 KB auxiliary RAM, language card, IIe MMU soft switches
- 40/80-column text, lo-res, hi-res (NTSC artifact colour), mixed mode
- Keyboard, Open/Solid Apple buttons, 1-bit speaker
- No disk drive yet: it boots straight into Applesoft BASIC

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

| Key | Action |
|-----|--------|
| F12 | RESET (CTRL-RESET) |
| Shift+F12 | Power cycle |
| Cmd+V | Paste text |
| Cmd+Q | Quit |
| Caps Lock | Toggle Apple CAPS LOCK (on by default) |
| Left / Right Alt | Open Apple / Solid Apple |

### Headless mode

Useful for scripted checks: runs N frames (60 per second) without a window,
prints the text screen and optionally saves a screenshot.

```sh
./build/apple2e_emulator --headless 300 --type $'PRINT 2+2\n' --screenshot out.bmp
```

## CPU tests

Download `6502_functional_test.bin` and `65C02_extended_opcodes_test.bin` from
[Klaus2m5/6502_65C02_functional_tests](https://github.com/Klaus2m5/6502_65C02_functional_tests/tree/master/bin_files), then:

```sh
cmake -S . -B build -DKLAUS_TESTS_DIR=/path/to/bin_files
cmake --build build -j
ctest --test-dir build
```
