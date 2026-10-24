/*
 * hal_log_esp32.c — ESP32-S3 logging backend.
 *
 * Forwards to the ESP-IDF logger. ESP_LOGx adds its own tag,
 * timestamp and colour, so this backend only has to map the HAL's
 * severity levels onto ESP_LOG* macros and keep the threshold in sync
 * with ESP_LOG's own level setting.
 */
#include "boards/synth_board_select.h"

#if defined(BOARD_ESP32S3)

#include "hal/hal_log.h"

#include "esp_log.h"
#include "esp_log_write.h"

static hal_log_level_t s_level = HAL_LOG_LEVEL_INFO;

/* ESP-IDF's own level, mapped onto the HAL's. */
static esp_log_level_t to_esp_level(hal_log_level_t level)
{
    switch (level) {
    case HAL_LOG_LEVEL_NONE:    return ESP_LOG_NONE;
    case HAL_LOG_LEVEL_ERROR:   return ESP_LOG_ERROR;
    case HAL_LOG_LEVEL_WARN:    return ESP_LOG_WARN;
    case HAL_LOG_LEVEL_INFO:    return ESP_LOG_INFO;
    case HAL_LOG_LEVEL_DEBUG:   return ESP_LOG_DEBUG;
    case HAL_LOG_LEVEL_VERBOSE: return ESP_LOG_VERBOSE;
    }
    return ESP_LOG_INFO;
}

static hal_log_level_t from_esp_level(esp_log_level_t level)
{
    switch (level) {
    case ESP_LOG_NONE:    return HAL_LOG_LEVEL_NONE;
    case ESP_LOG_ERROR:   return HAL_LOG_LEVEL_ERROR;
    case ESP_LOG_WARN:    return HAL_LOG_LEVEL_WARN;
    case ESP_LOG_INFO:    return HAL_LOG_LEVEL_INFO;
    case ESP_LOG_DEBUG:   return HAL_LOG_LEVEL_DEBUG;
    case ESP_LOG_VERBOSE: return HAL_LOG_LEVEL_VERBOSE;
    default:              return HAL_LOG_LEVEL_INFO;
    }
}

sys_status_t hal_log_init(void)
{
    /* Adopt whatever level the firmware is already running at, so the
     * HAL does not silently raise or lower the SDK's verbosity. */
    s_level = from_esp_level(esp_log_level_get("*"));
    return SYS_OK;
}

void hal_log_set_level(hal_log_level_t level)
{
    s_level = level;
    esp_log_level_set("*", to_esp_level(level));
}

hal_log_level_t hal_log_get_level(void)
{
    return s_level;
}

bool hal_log_enabled(hal_log_level_t level)
{
    if (s_level == HAL_LOG_LEVEL_NONE) {
        return false;
    }
    /* Levels are ordered least-severe first, so a message passes when
     * its level is at least as severe as the threshold. */
    return level <= s_level;
}

void hal_log_vwrite(hal_log_level_t level, const char *tag, const char *fmt,
                    va_list args)
{
    if (!hal_log_enabled(level)) {
        return;
    }

    const char *use_tag = (tag != NULL) ? tag : "hal";

    switch (level) {
    case HAL_LOG_LEVEL_ERROR:
        esp_log_writev(ESP_LOG_ERROR, use_tag, fmt, args);
        break;
    case HAL_LOG_LEVEL_WARN:
        esp_log_writev(ESP_LOG_WARN, use_tag, fmt, args);
        break;
    case HAL_LOG_LEVEL_INFO:
        esp_log_writev(ESP_LOG_INFO, use_tag, fmt, args);
        break;
    case HAL_LOG_LEVEL_DEBUG:
        esp_log_writev(ESP_LOG_DEBUG, use_tag, fmt, args);
        break;
    case HAL_LOG_LEVEL_VERBOSE:
        esp_log_writev(ESP_LOG_VERBOSE, use_tag, fmt, args);
        break;
    case HAL_LOG_LEVEL_NONE:
        break;
    }
}

void hal_log_write(hal_log_level_t level, const char *tag, const char *fmt, ...)
{
    if (!hal_log_enabled(level)) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    hal_log_vwrite(level, tag, fmt, args);
    va_end(args);
}

#endif /* BOARD_ESP32S3 */
