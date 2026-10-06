cmake_minimum_required(VERSION 3.20)
project(Apple2eEmulator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/cpu/CPU.cpp
    src/cpu/MemoryMapper.cpp
    src/video/VideoGenerator.cpp
    src/audio/AudioEngine.cpp
    src/input/InputHandler.cpp
    src/system/Apple2eSystem.cpp
    src/rom/ROMLoader.cpp
)

# Executable
add_executable(apple2e_emulator ${SOURCES})

# Include directories
target_include_directories(apple2e_emulator PRIVATE
    src
    src/cpu
    src/video
    src/audio
    src/input
    src/system
    src/rom
)

# Link libraries
target_link_libraries(apple2e_emulator PRIVATE
    SDL2::SDL2
    SDL2_image::SDL2_image
)

# Platform-specific settings
if(WIN32)
    set_target_properties(apple2e_emulator PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
