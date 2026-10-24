#
# sim/CMakeLists.txt — host simulator (SDL2) build.
#
# Builds the Ravine Phoenix HAL for the Linux desktop, so the
# application logic can be developed and tested without the device.
#
# The firmware build is entirely separate: it lives in the ESP-IDF
# project rooted at the repository top level and is driven by
# idf.py / tools/build.sh. Nothing here affects it.
#
# Sources are shared with the firmware: the HAL interfaces under hal/,
# boards/ and common/ are compiled into both targets, and each backend
# directory contributes exactly one implementation per target.
#
# This file is reached in two ways:
#
#   1. As a subdirectory of the repository root, with
#      SYNTH_BUILD_SIMULATOR=ON. That is what CLion does, because a
#      CLion CMake profile always uses the project root as its source
#      directory and has no source-directory field of its own.
#
#   2. Directly, via the "host-simulator" CMake preset in this
#      directory's CMakePresets.json:
#
#         cmake --preset host-simulator
#         cmake --build --preset host-simulator
#         ./build-sim/ravine_sim --selftest
#
# The preset is also included by the repository-root CMakePresets.json,
# so `cmake --list-presets` shows the firmware and the simulator side
# by side.
#
cmake_minimum_required(VERSION 3.16)

# When included from the root, the root has already called project().
# When configured directly, this file is the project root and has to
# call project() itself.
if(NOT PROJECT_NAME)
    project(ravine_sim C)
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# The simulator uses POSIX sockets and getaddrinfo(), which strict ISO C
# mode hides. gnu17 exposes the POSIX declarations on glibc.
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Warnings
# ---------------------------------------------------------------------------
add_compile_options(-Wall -Wextra -Wshadow -Wpointer-arith -Wcast-align)

# memmem() and getaddrinfo() are GNU/POSIX extensions.
add_compile_definitions(_GNU_SOURCE)

# ---------------------------------------------------------------------------
# Roots
#
# The HAL lives at the repository top level, outside this directory, so
# that both the firmware and the simulator can use it.
#
# When this file is configured directly (its own CMake project root) the
# repository root is its parent. When it is included from the
# repository root, CMAKE_CURRENT_SOURCE_DIR is the sim/ subdirectory
# and the repository root is still its parent. Either way the parent
# is the repository root.
# ---------------------------------------------------------------------------
get_filename_component(SIM_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# The build output always lands in <repository root>/build-sim, whichever
# directory the configuration was started from.
set(SIM_BINARY_DIR "${SIM_PROJECT_ROOT}/build-sim")

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
set(SIM_SOURCES
    "${SIM_PROJECT_ROOT}/src/main_sim.c"
    "${SIM_PROJECT_ROOT}/src/common/event_queue.c"
    "${SIM_PROJECT_ROOT}/common/sys_status.c"
    "${SIM_PROJECT_ROOT}/src/backend_sim/hal_display_sim.c"
    "${SIM_PROJECT_ROOT}/src/backend_sim/hal_input_sim.c"
    "${SIM_PROJECT_ROOT}/src/backend_sim/hal_audio_sim.c"
    "${SIM_PROJECT_ROOT}/src/backend_sim/hal_storage_sim.c"
    "${SIM_PROJECT_ROOT}/src/backend_sim/hal_log_sim.c"
    "${SIM_PROJECT_ROOT}/src/backend_sim/hal_network_sim.c"
    "${SIM_PROJECT_ROOT}/src/backend_sim/hal_crypto_sim.c"
)

add_executable(ravine_sim ${SIM_SOURCES})

# ---------------------------------------------------------------------------
# Include paths
#
# The project root is on the path so that headers are included by their
# repository-relative path ("boards/synth_board.h", "hal/hal_display.h",
# "common/sys_status.h"). The include/ directory is added separately
# because it holds the umbrella header "synth/hal.h".
# ---------------------------------------------------------------------------
target_include_directories(ravine_sim PRIVATE
    "${SIM_PROJECT_ROOT}"
    "${SIM_PROJECT_ROOT}/include"
)

# ---------------------------------------------------------------------------
# SDL2
# ---------------------------------------------------------------------------
find_package(SDL2 REQUIRED)

if(TARGET SDL2::SDL2)
    target_link_libraries(ravine_sim PRIVATE SDL2::SDL2)
else()
    # Older SDL2 packages do not provide the imported target.
    target_include_directories(ravine_sim PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(ravine_sim PRIVATE ${SDL2_LIBRARIES})
endif()

# ---------------------------------------------------------------------------
# OpenSSL — TLS for the network HAL, SHA-256 and Ed25519 for the crypto HAL.
# ---------------------------------------------------------------------------
find_package(OpenSSL REQUIRED)
target_link_libraries(ravine_sim PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# ---------------------------------------------------------------------------
# Platform libraries
# ---------------------------------------------------------------------------
find_package(Threads REQUIRED)
target_link_libraries(ravine_sim PRIVATE Threads::Threads)

# ---------------------------------------------------------------------------
# Output
#
# Always <repository root>/build-sim, so the binary is at the same
# path no matter which directory the configuration was started from.
# ---------------------------------------------------------------------------
set_target_properties(ravine_sim PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SIM_BINARY_DIR}"
)
