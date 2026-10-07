cmake_minimum_required(VERSION 3.20)
project(Apple2eEmulator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# compile_commands.json for clangd and other editor tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL2 REQUIRED)

# Emulator core, shared by the app and the tests
add_library(apple2e_core STATIC
    src/apple2e.cpp
    src/audio.cpp
    src/cpu.cpp
    src/disk2.cpp
    src/drivesounds.cpp
    src/gameio.cpp
    src/io.cpp
    src/keyboard.cpp
    src/memory.cpp
    src/sidepanel.cpp
    src/softcard.cpp
    src/video.cpp
    src/z80.cpp
)
target_include_directories(apple2e_core PUBLIC include)
target_link_libraries(apple2e_core PUBLIC SDL2::SDL2)
target_compile_options(apple2e_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
)

add_executable(apple2e_emulator src/main.cpp)
target_link_libraries(apple2e_emulator PRIVATE apple2e_core)

# CPU validation against Klaus Dormann's functional tests. The test images are
# not bundled; point KLAUS_TESTS_DIR at a folder with the .bin files from
# https://github.com/Klaus2m5/6502_65C02_functional_tests/tree/master/bin_files
enable_testing()
add_executable(cpu_test tests/cpu_test.cpp src/cpu.cpp)
target_include_directories(cpu_test PRIVATE include)

# Z80 validation with ZEXDOC/ZEXALL (CP/M programs, not bundled either); see
# https://github.com/anotherlin/z80emu/tree/master/testfiles
add_executable(z80_test tests/z80_test.cpp src/z80.cpp)
target_include_directories(z80_test PRIVATE include)

set(KLAUS_TESTS_DIR "" CACHE PATH "Directory with Klaus Dormann's test .bin files")
if(KLAUS_TESTS_DIR)
    add_test(NAME cpu_6502_functional
             COMMAND cpu_test ${KLAUS_TESTS_DIR}/6502_functional_test.bin 3469)
    add_test(NAME cpu_65c02_extended
             COMMAND cpu_test ${KLAUS_TESTS_DIR}/65C02_extended_opcodes_test.bin 24F1)
endif()

set(ZEX_TESTS_DIR "" CACHE PATH "Directory with zexdoc.com / zexall.com")
if(ZEX_TESTS_DIR)
    add_test(NAME z80_zexdoc COMMAND z80_test ${ZEX_TESTS_DIR}/zexdoc.com)
    add_test(NAME z80_zexall COMMAND z80_test ${ZEX_TESTS_DIR}/zexall.com)
endif()
