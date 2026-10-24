/*
 * hal_log_sim.c — SDL2 logging backend (host simulator).
 *
 * Writes to stderr, with a short level prefix so that the simulator's
 * output can be filtered the same way the firmware's log output can.
 */
#include "boards/synth_board_select.h"

#if defined(BOARD_SIMULATOR)

#include "hal/hal_log.h"

#include <stdarg.h>
#include <stdio.h>

static hal_log_level_t s_level = HAL_LOG_LEVEL_INFO;

static const char *level_prefix(hal_log_level_t level)
{
    switch (level) {
    case HAL_LOG_LEVEL_ERROR:   return "E";
    case HAL_LOG_LEVEL_WARN:    return "W";
    case HAL_LOG_LEVEL_INFO:    return "I";
    case HAL_LOG_LEVEL_DEBUG:   return "D";
    case HAL_LOG_LEVEL_VERBOSE: return "V";
    case HAL_LOG_LEVEL_NONE:    return "-";
    }
    return "?";
}

sys_status_t hal_log_init(void)
{
    return SYS_OK;
}

void hal_log_set_level(hal_log_level_t level)
{
    s_level = level;
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
    return level <= s_level;
}

void hal_log_vwrite(hal_log_level_t level, const char *tag, const char *fmt,
                    va_list args)
{
    if (!hal_log_enabled(level)) {
        return;
    }
    fprintf(stderr, "[%s][%s] ", level_prefix(level), tag ? tag : "hal");
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
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

#endif /* BOARD_SIMULATOR */
