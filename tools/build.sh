#!/usr/bin/env bash
#
# tools/build.sh — portable ESP-IDF build wrapper for ravine.1une.cc
#
# The ESP-IDF toolchain (gcc, openocd, esptool.py, idf.py itself) is not
# on $PATH by default. `idf.py` needs IDF_PATH, IDF_TOOLS_PATH, a venv
# of pinned Python deps, and several PATH entries. All of that is
# managed by `source $IDF_PATH/export.sh` from the IDF install.
#
# This wrapper:
#   1. Resolves IDF_PATH automatically (EIM-managed install, or $IDF_PATH
#      from the environment, or $1 on the command line).
#   2. Sources export.sh in a child process (subshell) so it cannot leak
#      half-set env vars into the caller's shell.
#   3. Invokes `idf.py` with whatever args you pass, defaulting to `build`.
#
# Usage:
#   ./tools/build.sh                          # idf.py build
#   ./tools/build.sh menuconfig               # idf.py menuconfig
#   ./tools/build.sh -p /dev/ttyUSB0 flash    # idf.py -p ... flash
#   ./tools/build.sh monitor                  # idf.py monitor
#   ./tools/build.sh -- IDF_PATH=/opt/esp/... # override the auto-detect
#
# Exit codes: idf.py's own exit code, or 1 if IDF_PATH cannot be resolved.
#

set -euo pipefail

# --- 1. Resolve IDF_PATH -------------------------------------------------
#
# Resolution order (first match wins):
#   (a) --idf-path=<path> on the command line (or as $1 if you pass
#       it bare), for portability on machines without EIM.
#   (b) $IDF_PATH already set in the environment (exported by your
#       shell rc or by an IDE).
#   (c) The newest EIM-managed install under ~/.espressif/v* (sorted
#       numerically; v6.1 > v6.0 > v5.x).

resolve_idf_path() {
    # (a) CLI flag
    for arg in "$@"; do
        case "$arg" in
            --idf-path=*) echo "${arg#--idf-path=}"; return 0 ;;
        esac
    done

    # (b) env
    if [[ -n "${IDF_PATH:-}" && -f "${IDF_PATH}/tools/idf.py" ]]; then
        echo "${IDF_PATH}"; return 0
    fi

    # (c) newest EIM install
    if [[ -d "${HOME}/.espressif" ]]; then
        local candidate
        candidate=$(ls -1d "${HOME}"/.espressif/v*/esp-idf 2>/dev/null \
                    | sort -V | tail -n 1)
        if [[ -n "$candidate" && -f "${candidate}/tools/idf.py" ]]; then
            echo "$candidate"; return 0
        fi
    fi

    return 1
}

IDF_PATH_RESOLVED=$(resolve_idf_path "$@") || {
    echo "ERROR: could not locate ESP-IDF." >&2
    echo "  Set IDF_PATH in your environment, pass --idf-path=<path>," >&2
    echo "  or install IDF via the Espressif installer (EIM) so it lives" >&2
    echo "  under ~/.espressif/v*/esp-idf/." >&2
    exit 1
}
export IDF_PATH="${IDF_PATH_RESOLVED}"
export IDF_PATH_FORCE=1   # prevent export.sh from auto-detecting the wrong dir

# --- 2. Source export.sh in a subshell -----------------------------------
# `bash -c` runs the source in a child, so idf.py is on PATH only for
# the idf.py invocation below. The user's interactive shell is untouched.
#
# `set -e` does not propagate into the source (export.sh calls `return`
# at the end, not `exit`), so we use `&&` chaining.

# detect_python.sh sets $ESP_PYTHON which export.sh then consumes. We
# must run them in the *current* shell, not a subshell, or the env
# vars set by export.sh leak out of the subshell and never reach
# idf.py. We do the source in two passes and silence export.sh's
# "Detected Python" banner so the output stays clean.
# shellcheck disable=SC1091
. "${IDF_PATH}/tools/detect_python.sh"
# shellcheck disable=SC1091
. "${IDF_PATH}/export.sh" >/dev/null

# --- 3. Invoke idf.py -----------------------------------------------------
#
# Default action: `build` (matches what CLion's "Build" button does).
# Forward all CLI args. Strip our --idf-path flag before forwarding.

ARGS=()
for arg in "$@"; do
    case "$arg" in
        --idf-path=*) ;;     # consumed; don't pass to idf.py
        *) ARGS+=("$arg") ;;
    esac
done
if [[ ${#ARGS[@]} -eq 0 ]]; then
    ARGS=(build)
fi

cd "$(dirname "$0")/.."    # project root, regardless of caller CWD

echo "+ idf.py ${ARGS[*]}"
exec idf.py "${ARGS[@]}"
