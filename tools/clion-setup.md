# CLion setup for ravine.1une.cc (ESP-IDF v6.1, ESP32-S3)

This project builds with ESP-IDF v6.1. CLion talks to ESP-IDF via CMake
(the project ships a `CMakePresets.json`), but the IDF toolchain is
not on `$PATH` by default — the CMake configuration step needs
`IDF_PATH` in the environment to find `tools/cmake/project.cmake`.

There are two ways to make CLion build the firmware:

| Method | What you get | When to use |
|---|---|---|
| **A. CLion ESP-IDF plugin** (recommended) | First-class: `idf.py` build / flash / monitor as Run/Debug configurations, code insight into IDF headers, menuconfig GUI, builtin QEMU simulator. | Day-to-day development. |
| **B. Plain CMake + this project's `tools/build.sh`** | `idf.py` invocations as Shell Script run configurations. Code insight is still good (CLion finds the toolchain from the CMake profile), but flash / monitor / menuconfig are manual `idf.py` calls. | Don't want to install a plugin. Or `idf.py` versions disagree (the plugin pins its own). |

Both methods are documented below.

## Prerequisite: install ESP-IDF v6.1

If you don't have it yet, the Espressif installer (EIM) is the
fastest path. On Linux:

```bash
# one-time
git clone --recursive --branch v6.1 \
    https://github.com/espressif/esp-idf.git \
    ~/.espressif/v6.1/esp-idf
cd ~/.espressif/v6.1/esp-idf
./install.sh esp32s3
```

EIM keeps it under `~/.espressif/v*/esp-idf/`. `tools/build.sh`
auto-detects that layout. If you put it somewhere else, set
`IDF_PATH` in your shell rc or pass `--idf-path=/opt/esp/...` to
`build.sh`.

## Method A — CLion ESP-IDF plugin (recommended)

1. **Install the plugin.** CLion → Settings → Plugins → Marketplace →
   search **"ESP-IDF"** (by Espressif Systems). Install, restart.
2. **Tell the plugin where the SDK is.** Settings → Languages &
   Frameworks → **ESP-IDF**:
   - **ESP-IDF path:** your `~/.espressif/v6.1/esp-idf`
     (or wherever you installed it; the plugin validates the
     expected sub-paths and warns if it can't find `tools/idf.py`).
   - **Python interpreter from the ESP-IDF virtual environment** is
     auto-detected. Leave it alone.
   - **OpenOCD path / ESP32-S3 target:** default values are fine.
3. **Open the project.** File → Open → select the project root
   (`ravine.1une.cc/`). CLion sees `CMakeLists.txt` and `CMakePresets.json`
   and offers to use the **default** preset.
4. **Pick the preset.** Settings → Build, Execution, Deployment →
   CMake → Profile "default" (or "release" / "size-analysis"). The
   presets in this repo map 1:1 to the plugin's Debug / Release /
   Size-Analysis profiles.
5. **First configure.** CLion will run `cmake -G Ninja` against the
   top-level `CMakeLists.txt`. This invokes ESP-IDF's
   `project.cmake`, which reads `sdkconfig.defaults` and produces
   `build/sdkconfig.h`. If the first configure fails with
   `IDF_PATH not set`, your plugin config (step 2) didn't stick.
6. **Build.** Hammer-icon → "Build 'default'". Output ends with
   `Project build complete. To flash, run: idf.py flash`. The
   binary is at `build/esp32_po33_ko.bin`.
7. **Flash + monitor.** Top-right run-config dropdown →
   **"esp32_po33_ko flash"** (the plugin generates this when you
   set up the project). Set the serial port under
   Run → Edit Configurations → esp32s3 → Serial port:
   `/dev/ttyUSB0` (Linux) or `COM7` (Windows).

> **Heads up:** the plugin sometimes complains that
> `idf.py` is not on `$PATH` *inside* a `Run` step. That's a
> pre-existing shell config problem, not a project bug. The
> workaround is to use the *plugin's* "ESP-IDF console" Run, which
> sources `export.sh` for you.

## Method B — plain CMake + tools/build.sh

No plugin required. CLion uses its native CMake support; you provide
the toolchain through the project's own wrapper.

1. **Skip the plugin step.** Don't install the ESP-IDF plugin.
2. **Open the project.** File → Open → `ravine.1une.cc/`.
3. **Set IDF_PATH for CLion's CMake.** Settings → Build, Execution,
   Deployment → CMake → click **Environment** → add:
   ```
   IDF_PATH=/home/<you>/.espressif/v6.1/esp-idf
   IDF_PATH_FORCE=1
   ```
   (Use the actual path on your machine.) Save.
4. **Pick a preset.** CMake tab → Profile: **default** /
   **release** / **size-analysis** — all three are in
   `CMakePresets.json` at the project root.
5. **Build.** Hammer-icon → "Build 'default'".
6. **Flash / monitor / menuconfig** are not in CLion's Build
   dropdown. Add them as Shell Script run configurations:
   - Run → Edit Configurations → `+` → **Shell Script**
   - Script text: `./tools/build.sh -p /dev/ttyUSB0 flash` (or
     `monitor`, or `menuconfig`)
   - Working directory: `$ProjectFileDir$`
   - Save. You can now pick "Flash" / "Monitor" from the run-config
     dropdown.

## Why `IDF_PATH_FORCE=1`?

`export.sh` has a "smart" auto-detect that tries to find the IDF
install from the calling script's path. It is occasionally wrong
(e.g. when the project lives on a path that shadows the IDF
install). Setting `IDF_PATH_FORCE=1` makes it use whatever
`IDF_PATH` is in the environment, no questions asked. Our root
`CMakeLists.txt` already passes `set(ENV{IDF_PATH} "$ENV{IDF_PATH}")`
through, so this is belt-and-suspenders, not a workaround for our
project.

## Cross-checking the toolchain

If a build fails with a confusing error, sanity-check that CLion
is talking to the IDF you think it is:

1. Settings → Build, Execution, Deployment → CMake → Environment →
   confirm `IDF_PATH` is what you expect.
2. Open CLion's terminal (`Alt+F12`). Run `./tools/build.sh
   --version`. The wrapper uses the *same* resolver as Method B,
   so if `tools/build.sh` says `ESP-IDF v6.1` but CLion's CMake
   configure fails, the two paths have diverged.
3. Check `build/CMakeCache.txt` — look for `CMAKE_TOOLCHAIN_FILE`
   and `IDF_TOOLCHAIN`. They should point into the IDF path from
   step 1.

## Why no `.idea/` files in git

`.idea/` is gitignored. Anything in `workspace.xml`,
`runConfigurations/`, the CMake profile environment you set in
step 3 — all of that is *per-user* and lives only on your
machine. The **shared** config (this document + `CMakePresets.json`
+ `tools/build.sh`) is what new contributors check in. Run
configurations in particular live in `.idea/runConfigurations/*.xml`
locally; they are not part of the project source.

## Re-running the migration

The v6.0 → v6.1 work is recorded in commit `272002e`. If you need
to roll back to v6.0:

```bash
git revert 272002e   # or: git checkout 272002e~1
# update IDF_PATH in CLion's CMake environment to v6.0
./tools/build.sh build
```
