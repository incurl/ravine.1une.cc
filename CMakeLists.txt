#
# CMakeLists.txt — project root.
#
# Two build targets share this repository:
#
#   1. The ESP32-S3 firmware (ESP-IDF). This is the default, and it is
#      driven by idf.py (see tools/build.sh) and by CLion's ESP-IDF
#      CMake profile.
#
#   2. The SDL2 host simulator. It is driven by the "host-simulator"
#      CMake preset:
#
#         cmake --preset host-simulator
#         cmake --build --preset host-simulator
#
#      That preset lives in sim/CMakePresets.json and is included by
#      this directory's CMakePresets.json, so `cmake --list-presets`
#      shows the firmware and simulator presets side by side.
#
# How the two are kept apart
# -------------------------
# A CMake project has exactly one project() call per source directory.
# The firmware needs the ESP-IDF toolchain, the simulator needs the
# host's gcc plus SDL2 and OpenSSL, and the two must never share an
# object file. So the root file dispatches on a cache variable:
#
#   - With no variable set (the normal case, and what idf.py does),
#     this file configures the ESP-IDF firmware exactly as before.
#   - With SYNTH_BUILD_SIMULATOR=ON, it configures the host simulator
#     instead, by delegating to sim/CMakeLists.txt.
#
# CLion always uses the project root as the CMake source directory —
# a CMake profile has no source-directory field — so this dispatch is
# what lets the simulator appear as a CLion CMake profile and run
# configuration alongside the firmware one.
#
cmake_minimum_required(VERSION 3.24)

# The simulator is a plain host build; it must not go through the
# ESP-IDF project machinery. Check before anything else is set up.
if(SYNTH_BUILD_SIMULATOR)
    # sim/CMakeLists.txt defines its own project() call and its own
    # toolchain requirements (SDL2, OpenSSL, pthreads).
    add_subdirectory(sim)
    return()
endif()

set(ENV{IDF_PATH} "$ENV{IDF_PATH}")
list(APPEND CMAKE_MODULE_PATH "$ENV{IDF_PATH}/tools/cmake")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_po33_ko)
