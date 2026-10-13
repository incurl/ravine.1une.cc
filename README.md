# ESP32-S3 PO-33 K.O! — Pocket Operator Emulator Firmware

A firmware that emulates the **Teenage Engineering PO-33 K.O!** micro-sampler on an **ESP32-S3-WROOM-1-N16R8**.

**All sound is produced by [AMY](https://github.com/shorepine/amy)** — vendored under `components/amy`. AMY provides oscillators, PCM sample playback, filters, chorus/echo/reverb, a sequencer, voice stealing and I²S driver setup. Our firmware is a thin UI + sequencer + storage layer over AMY.

> **Status:** Targets **ESP-IDF v6.1** APIs.

---

## 🌐 Project Website

**👉 [ravine.1une.cc](https://ravine.1une.cc)**

The project ships with a full static website (Eleventy + Tailwind, deployed
via GitHub Pages):

- 📖 **Documentation hub** — the build guide, hardware pin map, UART shell
  reference, and feature-to-implementation map, all linked from one place.
- 🔥 **Browser firmware flasher** — flash the ESP32-S3 directly from
  Chrome / Edge using [ESP Web Tools](https://esphome.github.io/esp-web-tools/).
  No `idf.py`, no toolchain — just a USB cable.
- 📚 **Reader app** — the two companion books (Makers' Companion + Music
  Course) split into per-chapter pages with a sidebar, prev/next
  navigation, and a font-size toggle.

Source lives in [`website/`](website/) — see
[`website/README.md`](website/README.md) for the dev workflow and
[ADR-0001](docs/architecture-decisions.md#adr-0001--use-eleventy-11ty-for-the-static-website)
for the static-site-generator decision.

---

## ✨ Features

| Spec | Implementation |
|---|---|
| Sample memory | **40 s mono** @ 44100 Hz 16-bit = **~3.37 MB PSRAM pool** |
| Sample slots | **16** (8 drum ≤ 2 s, 8 melodic ≤ 3 s) |
| Polyphony | 4 voices via AMY synth |
| Synthesis | AMY Juno-6 + DX7 + PCM sampler + custom |
| Recording | External I²S mic (INMP441) → PSRAM slot |
| Trimming / slicing | Per-slot start/end |
| Sequencer | 16-step × **16 patterns**, 128-pattern chain mode |
| Effects | All 16 PO-33 punch-in effects mapped to AMY primitives |
| Display | **2.4″ TFT (ILI9341, 240×320, SPI)** + custom 2D draw API |
| Sync | Jam-sync pulse output on GPIO |
| Storage | LittleFS on 16 MB flash (patterns only — samples live in PSRAM) |
| Power | 5-min idle → deep sleep, GPIO-wake |
| Clock / alarm | RTC-backed, NVS-persisted |
| Shell | UART @ 115 200 baud command interface |

---

## 🏗️ Build

The project ships IDE configs for **CLion** and **VS Code**; both go
through the same `tools/build.sh` wrapper, which sources the ESP-IDF
v6.1 `export.sh` for you. If you just want a CLI build:

```bash
# 1. ESP-IDF v6.1
git clone --recursive https://github.com/espressif/esp-idf.git -b v6.1
cd esp-idf && ./install.sh esp32s3 && source export.sh

# 2. Vendor AMY sources into components/amy/src/ (see components/amy/README.md)
git clone --depth 1 https://github.com/shorepine/amy.git /tmp/amy
cp -r /tmp/amy/src/* components/amy/src/
rm -rf /tmp/amy

# 3. Build / flash (CLI)
cd /path/to/ravine.1une.cc
./tools/build.sh build                    # or just: idf.py build
./tools/build.sh -p /dev/ttyUSB0 flash    # or: idf.py -p ... flash
./tools/build.sh monitor                  # or: idf.py monitor
```

**IDEs:**
- **CLion** — see `tools/clion-setup.md`. Two options: the official
  ESP-IDF plugin (one-click build / flash / monitor), or plain CMake
  with the project's `tools/build.sh` as a Shell Script run config.
- **VS Code** — install the recommended extensions (`.vscode/extensions.json`),
  then run the **Build firmware** task (Ctrl+Shift+B). The task list
  covers build / clean / menuconfig / flash / monitor / size report.

`components/amy/CMakeLists.txt` adds the platform flags needed for ESP32-S3:

```cmake
target_compile_definitions(amy PRIVATE
    AMY_MCU ESP_PLATFORM AMY_WAVETABLE
    BLOCK_SIZE_BITS=8 AMY_NCHANS=2
    AMY_NO_PCM_PRESETS
)
```

---

## 🗂️ Project Structure

```
.
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   ├── main.c
│   ├── config.h
│   ├── audio/
│   │   └── amy_bridge.{c,h}     # thin wrapper over AMY
│   ├── sequencer/
│   │   ├── pattern.{c,h}
│   │   └── sequencer.{c,h}     # calls amy_bridge_play_note() per step
│   ├── ui/
│   │   ├── buttons.{c,h}
│   │   ├── display.{c,h}
│   │   ├── knobs.{c,h}           # Knob A (GPIO 2 / ADC1_CH1) + Knob B (GPIO 46 / ADC1_CH5)
│   │   └── leds.{c,h}
│   ├── storage/
│   │   └── storage.{c,h}        # patterns + samples → LittleFS
│   ├── system/
│   │   ├── power_mgmt.{c,h}
│   │   ├── clock.{c,h}
│   │   └── sync.{c,h}
│   └── tests/
│       ├── test_amy_bridge.c
│       ├── test_sequencer.c
│       └── test_patterns.c
└── components/
    └── amy/                     # vendored AMY library
        ├── CMakeLists.txt
        ├── idf_component.yml
        ├── README.md            # how to fetch AMY sources
        └── src/                 # AMY C/H sources (gitignored)
├── docs/
│   └── DESIGN.md                # feature-to-implementation map (~16k words)
├── hardware/
│   ├── HARDWARE.md              # beginner build guide (~5.6k words)
│   ├── README.md                # conventions for this folder
│   ├── schematic/               # future: KiCad .kicad_sch
│   ├── pcb/                     # future: KiCad .kicad_pcb + gerbers
│   ├── enclosure/               # future: .step / .stl / .scad
│   ├── datasheets/              # future: PDF excerpts of PCM5102A, INMP441, ILI9341
│   └── assembly-photos/         # future: build photos
└── website/                     # static site (Eleventy + Tailwind)
    └── (built to _site/, deployed via GitHub Pages)
```

---

## 🔌 Wiring (unchanged from earlier plan)

### Audio I/O (PCM5102A DAC + INMP441 mic)

| Signal | ESP32-S3 GPIO |
|---|---|
| DAC BCLK | GPIO9 |
| DAC LRCK | GPIO10 |
| DAC DIN  | GPIO8  |
| Mic BCLK | GPIO15 |
| Mic LRCK | GPIO16 |
| Mic DOUT | GPIO17 |

### 4x4 Button Matrix

| Row / Col | GPIO |
|---|---|
| Row 0–3 | GPIO35, GPIO36, GPIO37, GPIO38 |
| Col 0–3 | GPIO33, GPIO34, GPIO39, GPIO40 |

### 2.4″ TFT (ILI9341, SPI)

| Signal | ESP32-S3 GPIO |
|---|---|
| MOSI | GPIO6 |
| SCK  | GPIO7 |
| CS   | GPIO5 |
| DC   | GPIO4 |
| RST  | GPIO48 |
| BL   | GPIO47 (LEDC PWM) |

### Other

| Function | GPIO |
|---|---|
| Sync OUT | GPIO18 |
| Sync IN  | GPIO25 (was GPIO 19; reserved GPIO 19 for USB-MIDI) |
| LED REC  | GPIO21 |
| LED PLAY | GPIO14 |

---

## 📚 Documentation

- **[`docs/DESIGN.md`](docs/DESIGN.md)** — feature-to-implementation map (~16 000 words). What the firmware does, how every PO-33 feature maps to code, the honest ✅ / ⚠️ / ❌ scorecard, and how the ESP32-S3 can break the PO-33's limits.
- **[`docs/TIER_A_SUMMARY.md`](docs/TIER_A_SUMMARY.md)** — handoff doc for the 13 Tier A features (9 commits, scorecard 30% → 46%). Includes the AMY patch description, memory budget, build-verification checklist, and the explicit v1-out-of-scope list.
- **[`hardware/HARDWARE.md`](hardware/HARDWARE.md)** — beginner build guide (~5 600 words). Bill of materials, pin map, 10-step build order, troubleshooting, and the no-solder alternative.
- **[`hardware/README.md`](hardware/README.md)** — conventions for the `hardware/` folder (where future schematic / PCB / enclosure files go).
- **[`website/`](website/)** — the static site at [ravine.1une.cc](https://ravine.1une.cc). Includes a browser firmware flasher (`/download`) and a reader app for the two companion books (`/reader/`).

## 🧪 Tests

```bash
idf.py -C build test    # runs Unity tests under main/tests/
```

---

## 📟 UART Shell

```
help              # show all commands
play              # start
bpm 140           # set tempo
pattern 5         # choose pattern
rec 2             # record into slot 2
stoprec           # stop recording
free              # heap stats
save              # write patterns + samples to flash
load              # reload from flash
sleep             # enter deep sleep immediately
```

---

## 📊 Memory budget (post-init)

| Consumer | Size |
|---|---|
| 40 s × 44100 Hz mono pool (PSRAM) | 3.37 MB |
| AMY-side sample metadata (~80 bytes per registered slot × 16) | ~1.3 KB |
| 2.4″ TFT framebuffer | 150 KB |
| AMY state (oscs, events, voices, reverb/echo tails) | ~1.5 MB |
| LittleFS working memory | ~64 KB |
| Misc / heap overhead | ~256 KB |
| **PSRAM headroom** | **~2.5 MB free** |

The PSRAM sample pool is shared directly with AMY via a local `pcm_load_external()` patch (see `docs/TIER_A_SUMMARY.md` Patch 2); AMY keeps only a small metadata struct per registered slot, no copy of the sample data. Worst case (all 16 slots at max length) is ~1.3 KB of AMY-side metadata, leaving ~2.5 MB PSRAM headroom.

---

## ⚠️ Known Limitations / Roadmap

- Targets ESP-IDF v6.1. AMY's `i2s.c` uses `driver/i2s_std.h` (the new v6.x channel API).
- AMY's I²S RX block is shared; `amy_bridge_pump_capture()` runs from the button-scan task (10 ms) and pulls the latest block from AMY's internal `amy_in_block[]`.
- Per-step parameter locks are encoded in `amy_event` fields — see `apply_fx()` in `amy_bridge.c`.
- Power consumption on deep sleep ~ 10 µA — measured in v5.x, expected same on v6.1.

---

## 📝 License

See `LICENSE`. AMY is MIT-licensed (https://github.com/shorepine/amy).