/*
 * synth_board_select.h — picks exactly one build target.
 *
 * The HAL is compiled twice from the same sources: once for the
 * ESP32-S3 firmware and once for the SDL2 host simulator. This header
 * resolves which one, so every backend file can be guarded uniformly
 * without hard-coding a target name at each call site.
 *
 * Detection is by header presence rather than by a build-system define:
 *
 *   - The firmware target always has the FreeRTOS headers on its
 *     include path.
 *   - The simulator target always has the SDL2 headers.
 *
 * Why this shape (CLion/IDE friendliness):
 *   Each backend .c file wraps its *entire* body — including its
 *   platform includes — in the matching guard. The IDE therefore only
 *   ever parses the branch that matches the active configuration, so
 *   there are no false "unresolved symbol" or "unknown type" errors
 *   from the inactive backend. The headers under hal/ include neither
 *   ESP-IDF nor SDL2 at all, so they always index cleanly.
 *
 * If a project ever needs an explicit override, define exactly one of
 * BOARD_ESP32S3 / BOARD_SIMULATOR on the compiler command line and the
 * auto-detection below steps aside.
 */
#ifndef SYNTH_BOARD_SELECT_H
#define SYNTH_BOARD_SELECT_H

#if defined(BOARD_ESP32S3) && defined(BOARD_SIMULATOR)
#error "Define at most one of BOARD_ESP32S3 / BOARD_SIMULATOR."
#endif

#if !defined(BOARD_ESP32S3) && !defined(BOARD_SIMULATOR)

#  if defined(__has_include)
#    if __has_include("freertos/FreeRTOS.h")
#      define BOARD_ESP32S3 1
#    elif __has_include("SDL2/SDL.h")
#      define BOARD_SIMULATOR 1
#    endif
#  endif

#endif /* !defined(BOARD_*) */

/*
 * Neither header was present — e.g. a bare host compiler invocation
 * with no SDL2. Fall back to the simulator, which is the more portable
 * of the two targets (it needs no RTOS and no vendor SDK).
 */
#if !defined(BOARD_ESP32S3) && !defined(BOARD_SIMULATOR)
#  define BOARD_SIMULATOR 1
#endif

/* ---- Compile-time target assertions ------------------------------ */

#if defined(BOARD_ESP32S3) && defined(BOARD_SIMULATOR)
#error "Both BOARD_ESP32S3 and BOARD_SIMULATOR are defined."
#endif

#if !defined(BOARD_ESP32S3) && !defined(BOARD_SIMULATOR)
#error "No build target selected."
#endif

#endif /* SYNTH_BOARD_SELECT_H */
