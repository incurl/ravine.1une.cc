#
# components/synth_hal/CMakeLists.txt — ESP-IDF component for the HAL.
#
# ESP-IDF discovers every directory under components/ automatically, so
# this file registers a component that pulls in the HAL sources, which
# live at the repository top level (outside components/) so that the
# host simulator can compile the same files.
#
# The HAL is split into:
#   - the interface headers (hal/, boards/, common/) — no implementation
#   - the ESP32 backends (src/backend_esp32/)
#   - the shared queue and status helpers (src/common/)
#
# The simulator backends (src/backend_sim/) are NOT listed here, so they
# can never be compiled into the firmware image.
#
# Dependency direction
# --------------------
# The ESP32 backends call into the legacy drivers, which live in the
# "main" component. ESP-IDF's component graph is one-directional: a
# component may require others, and the requirement list is what
# determines the link order. The "main" component therefore lists
# "synth_hal" in its REQUIRES, and this component must NOT require
# "main" — that would be a cycle.
#
# The legacy symbols the backends call (buttons_init, display_init,
# amy_bridge_init, storage_init, ...) are resolved at link time because
# libmain.a is linked into the final image alongside every component
# library. No explicit link target is needed here.
#
idf_component_register(
    SRC_DIRS
        "."
    INCLUDE_DIRS
        "."
    REQUIRES
        driver
        esp_adc
        esp_driver_i2s
        esp_driver_ledc
        esp_driver_pcnt
        esp_event
        esp_http_client
        esp_lcd
        esp_netif
        esp_timer
        esp_wifi
        freertos
        littlefs
        nvs_flash
        mbedtls
)

# ---------------------------------------------------------------------------
# Pull in the top-level HAL sources.
#
# They are outside this component directory, so they are added with
# absolute paths rather than through SRC_DIRS.
# ---------------------------------------------------------------------------
get_filename_component(SYNTH_HAL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

target_sources(${COMPONENT_LIB} PRIVATE
    "${SYNTH_HAL_ROOT}/src/common/event_queue.c"
    "${SYNTH_HAL_ROOT}/common/sys_status.c"
    "${SYNTH_HAL_ROOT}/src/backend_esp32/hal_display_esp32.c"
    "${SYNTH_HAL_ROOT}/src/backend_esp32/hal_input_esp32.c"
    "${SYNTH_HAL_ROOT}/src/backend_esp32/hal_audio_esp32.c"
    "${SYNTH_HAL_ROOT}/src/backend_esp32/hal_storage_esp32.c"
    "${SYNTH_HAL_ROOT}/src/backend_esp32/hal_log_esp32.c"
    "${SYNTH_HAL_ROOT}/src/backend_esp32/hal_network_esp32.c"
    "${SYNTH_HAL_ROOT}/src/backend_esp32/hal_crypto_esp32.c"
)

target_include_directories(${COMPONENT_LIB} PUBLIC
    "${SYNTH_HAL_ROOT}"
    "${SYNTH_HAL_ROOT}/include"
)

# The ESP32 backends include headers from the legacy "main" component
# (main/config.h, main/ui/display.h, main/ui/buttons.h, ...). Those
# headers use plain "config.h"-style includes relative to main/, so the
# main component's own include directory has to be on the path.
#
# main/ is added as a PRIVATE include directory rather than a REQUIRES
# dependency: depending on "main" from here would be a cycle, because
# "main" requires this component.
target_include_directories(${COMPONENT_LIB} PRIVATE
    "${SYNTH_HAL_ROOT}/main"
)
