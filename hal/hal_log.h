/*
 * hal_log.h — logging abstraction.
 *
 * A thin, printf-style logging interface so that code shared between
 * the firmware and the simulator does not have to know whether it is
 * talking to ESP-IDF's logger or to the C standard library.
 *
 * Levels match the usual severity ordering: a message is emitted when
 * its level is at least as severe as the current threshold.
 *
 * The firmware backend forwards to ESP_LOGx, which adds its own tag,
 * timestamp and colour. The simulator backend writes to stderr.
 *
 * Threading: the log functions are safe to call from any task. They
 * are not intended for use from an ISR.
 */
#ifndef SYNTH_HAL_LOG_H
#define SYNTH_HAL_LOG_H

#include <stdarg.h>
#include <stdbool.h>

#include "common/sys_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* GCC/Clang can check printf-style arguments. Other compilers simply
 * do not get the diagnostic. */
#if defined(__GNUC__) || defined(__clang__)
#  define HAL_LOG_PRINTF(fmt_idx, arg_idx) \
      __attribute__((format(printf, (fmt_idx), (arg_idx))))
#else
#  define HAL_LOG_PRINTF(fmt_idx, arg_idx)
#endif

typedef enum {
    HAL_LOG_LEVEL_NONE = 0,   /* Suppress everything.                  */
    HAL_LOG_LEVEL_ERROR,      /* Unrecoverable problem.                */
    HAL_LOG_LEVEL_WARN,       /* Recoverable problem, degraded state.  */
    HAL_LOG_LEVEL_INFO,       /* Normal lifecycle event.               */
    HAL_LOG_LEVEL_DEBUG,      /* Extra detail for development.         */
    HAL_LOG_LEVEL_VERBOSE     /* Very chatty tracing.                  */
} hal_log_level_t;

/**
 * Initialise the logging backend.
 *
 * On the firmware this installs the ESP-IDF log level. On the host it
 * configures stderr.
 *
 * Returns SYS_OK.
 */
sys_status_t hal_log_init(void);

/**
 * Set the minimum severity that will be emitted.
 */
void hal_log_set_level(hal_log_level_t level);

/**
 * @return the current minimum severity.
 */
hal_log_level_t hal_log_get_level(void);

/**
 * Whether a message at @p level would be emitted. Lets callers skip
 * expensive formatting.
 */
bool hal_log_enabled(hal_log_level_t level);

/**
 * Emit a message.
 *
 * @param level Severity.
 * @param tag   Short subsystem tag, e.g. "display". May be NULL.
 * @param fmt   printf-style format string.
 *
 * The format string is only evaluated (i.e. varargs are only consumed)
 * when the level passes the threshold.
 */
void hal_log_write(hal_log_level_t level, const char *tag, const char *fmt, ...)
    HAL_LOG_PRINTF(3, 4);

/** va_list form of hal_log_write(). */
void hal_log_vwrite(hal_log_level_t level, const char *tag, const char *fmt,
                    va_list args);

/* Convenience wrappers. */
#define HAL_LOG_ERROR(tag, ...)   hal_log_write(HAL_LOG_LEVEL_ERROR,   (tag), __VA_ARGS__)
#define HAL_LOG_WARN(tag, ...)    hal_log_write(HAL_LOG_LEVEL_WARN,    (tag), __VA_ARGS__)
#define HAL_LOG_INFO(tag, ...)    hal_log_write(HAL_LOG_LEVEL_INFO,    (tag), __VA_ARGS__)
#define HAL_LOG_DEBUG(tag, ...)   hal_log_write(HAL_LOG_LEVEL_DEBUG,   (tag), __VA_ARGS__)
#define HAL_LOG_VERBOSE(tag, ...) hal_log_write(HAL_LOG_LEVEL_VERBOSE, (tag), __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* SYNTH_HAL_LOG_H */
