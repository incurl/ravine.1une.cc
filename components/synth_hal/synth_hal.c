/*
 * synth_hal.c — ESP-IDF component anchor for the Ravine Phoenix HAL.
 *
 * This file exists so that the component has at least one compilation
 * unit of its own; the real code lives in the top-level src/ tree and
 * is pulled in by this component's CMakeLists.txt.
 *
 * See the headers under hal/ for the interface, and
 * boards/synth_board.h for the shared board configuration.
 */
#include "boards/synth_board_select.h"

#if defined(BOARD_ESP32S3)

#include "hal/hal_log.h"
#include "synth/hal.h"

static const char *TAG = "synth_hal";

/*
 * Bring up the HAL subsystems that the application layer will use.
 *
 * This is deliberately NOT called automatically. The legacy firmware in
 * main/main.c initialises the display, buttons, knobs, encoder,
 * storage and audio itself, and during the transition period it must
 * keep doing so — two owners of the same hardware is exactly the
 * situation the HAL's SYS_ERR_BUSY guards exist to prevent.
 *
 * Once the application layer is migrated, this becomes the single
 * initialisation entry point for the HAL.
 */
sys_status_t synth_hal_init(void)
{
    sys_status_t st;

    st = hal_log_init();
    if (st != SYS_OK) {
        return st;
    }

    HAL_LOG_INFO(TAG, "%s HAL initialising", SYNTH_BOARD_NAME);

    st = hal_display_init();
    if (st != SYS_OK) {
        HAL_LOG_ERROR(TAG, "display init failed: %s", sys_status_str(st));
        return st;
    }

    st = hal_input_init();
    if (st != SYS_OK) {
        HAL_LOG_ERROR(TAG, "input init failed: %s", sys_status_str(st));
        return st;
    }

    st = hal_storage_init();
    if (st != SYS_OK) {
        HAL_LOG_ERROR(TAG, "storage init failed: %s", sys_status_str(st));
        return st;
    }

    HAL_LOG_INFO(TAG, "HAL ready");
    return SYS_OK;
}

#endif /* BOARD_ESP32S3 */
