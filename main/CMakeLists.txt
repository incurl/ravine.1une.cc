idf_component_register(
    SRC_DIRS
        "."
        "audio"
        "ui"
        "system"
        "storage"
        "sequencer"
        "sketch"
    EXCLUDE_SRCS "tests/*.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        driver
        esp_adc
        esp_driver_pcnt
        esp_driver_ledc
        esp_lcd
        amy
        freertos
        littlefs
        esp_timer
        nvs_flash
        esp_lcd_ili9341
        synth_hal
    EMBED_FILES
    REQUIRES
)

# Unity-based test files live under main/tests/ but are not part of the
# firmware build. They belong in a separate test component (or a
# dedicated test runner) that links unity and runs on the host or in
# a dedicated firmware-test app. Keeping them out of SRC_DIRS prevents
# the firmware build from trying to compile them and failing on the
# missing unity.h dependency.
#
# Note: SRC_DIRS must list each subdir explicitly because
# idf_component_register uses non-recursive file(GLOB) - SRC_DIRS .
# alone picks up only main/*.c, not subdirectories.
