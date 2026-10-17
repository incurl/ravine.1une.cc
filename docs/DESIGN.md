# PO-33 K.O! ESP32-S3 Firmware — Feature Design Document

> **Audience:** written for a complete novice on both programming and music production. Every technical term is defined inline the first time it appears, and again in the glossary at the end. If you have never used a Pocket Operator before, start at §1 and read straight through. If you already know the PO-33 and just want to see how we mapped it to code, jump to §3.

**Version:** 1.0
**Date:** 2026-10-17

> **Terminology convention used in this document.** Throughout, **"ESP32-S3 PO-33 remake"** (or simply **"the device"**, **"the remake"**) refers to *this* project — the open-source firmware running on the ESP32-S3 board described in §1.4. **"Original PO-33"** / **"Teenage Engineering PO-33"** / **"real PO-33"** refers to the manufactured hardware product. Where the unqualified word "PO-33" appears in the existing text, it is preserved for stability; new content in §14 onwards uses the disambiguating forms.  [ADDED: terminology convention introduced so that the new §14 and any future revisions can refer unambiguously to the ESP32-S3 remake vs. the original Teenage Engineering PO-33.]

---

## 0. What this document is, and who it is for

This is the design document for a piece of software that runs on a small circuit board called an **ESP32-S3**. That board is wired up to a few other small parts (a screen, some buttons, a microphone chip, a speaker chip). Together they act like a **Teenage Engineering PO-33 K.O! Pocket Operator** — the famous little orange-and-black sampler you may have seen musicians hold in their hand.

If you have never used a real PO-33, that's fine — this document explains everything from scratch.

This document has three jobs:

1. **Explain the device.** What it does, what you can do with it, what every button means. In plain English.
2. **Map every PO-33 feature to a piece of code.** For each thing the original PO-33 can do, we name the file in our project where the same thing is implemented, and tell you whether it's finished, partly finished, or not built yet.
3. **Honest scorecard.** We list exactly what works today and what doesn't. There is no marketing. If a feature is missing, it says "missing".

The intended reader is a complete novice. If you are an experienced embedded engineer, you can probably skim §1–§2 and skip straight to §3 and §7.

---

## 0.5. Naming: Phoenix and Ember

This project ships two artifacts that are versioned and named independently:

- **Firmware codename: `Phoenix`.** The software that runs on the ESP32-S3.
  The full product name on the website, in commit messages, and in this
  document is **Ravine: Phoenix** ("Ravine" is the project brand; "Phoenix"
  is the firmware release).
- **Hardware codename: `Ember`.** The reference PCB and bill of materials
  described in `hardware/HARDWARE.md`. A board that conforms to the Ember
  pin map can flash and run Phoenix without changes.

So when you see "Phoenix" in a firmware context (build flags, version
strings, the AMY bridge, the UI boot screen) it refers to the software,
and when you see "Ember" it refers to the board. The two are coupled
through `main/config.h`: Phoenix's pin assignments are the Ember
reference. Building Phoenix against a different board requires editing
that file.

---

## 1. The thirty-second explanation

### 1.1 What is the PO-33 K.O! Pocket Operator?

The PO-33 K.O! is a real product made by a Swedish company called **Teenage Engineering**. It is a small plastic box, about the size of a deck of cards, with:

- A speaker on the front
- A small black-and-orange LCD screen
- 16 little square buttons arranged 4 × 4
- A microphone built in
- A small headphone jack
- Two 3.5 mm sockets on the back for syncing with other music gear

It runs on two AAA batteries. The price when new is about $80. It is popular because it is **fun, fast, and weird**. You record a sound, you scribble it into a 16-step pattern, you press **play**, and the device loops the pattern forever while you twist knobs to make it sound different.

The official manual lives at [teenage.engineering/guides/po-33/en](https://teenage.engineering/guides/po-33/en).

### 1.2 What does our firmware do?

Our firmware is **a piece of open-source software (MIT-licensed) that emulates a PO-33 K.O! on a $5 ESP32-S3 microcontroller board** — specifically the **ESP32-S3-WROOM-1-N16R8** module (16 MB flash, 8 MB Octal PSRAM). Instead of buying Teenage Engineering's hardware, you buy this dev board, add a small handful of cheap parts, wire them together, flash our firmware over USB, and you have a PO-33-shaped device.

The parts we add are listed in `main/config.h` and the hardware guide at `hardware/HARDWARE.md`; the short version is: an audio DAC chip (PCM5102A) for output, an I²S MEMS microphone (INMP441) for recording, a 2.4″ color TFT (ILI9341) for the screen, and a 4×4 matrix of 16 tactile buttons for input. Total cost of the additional parts: roughly $10–$15.

Our firmware does **not** make the board behave 1-for-1 like a PO-33. As of v1, only about a quarter of the PO-33's features are fully implemented; another third are partially implemented; the rest are not yet built. The §7 scorecard lists every feature with an honest ✅ / ⚠️ / ❌ status. Use this document to check whether a feature you care about works before you assume it does.

We did not write the audio engine ourselves. We use **[AMY](https://github.com/shorepine/amy)** — a free, MIT-licensed, fixed-point music synthesizer library written by Brian Whitman and the Dogbotic team. AMY already knows how to play back PCM samples, generate oscillators (sine, saw, square), and add chorus, echo, reverb and filters. Our firmware is mostly a thin "shell" that turns PO-33 button presses into AMY commands. AMY runs its own rendering task on the ESP32-S3's second core, so audio doesn't compete with our button-scan or display tasks.

The full source lives in this repository; the project website with one-click browser flashing lives at [ravine.1une.cc](https://ravine.1une.cc) (see §1.5 and the website pages).

### 1.3 What does "ESP32-S3" + "AMY" + "TFT" mean?

- **ESP32-S3** — a tiny computer on a chip. We use the **ESP32-S3-WROOM-1-N16R8** variant, which has 16 MB of flash storage and 8 MB of extra "PSRAM" memory. The PSRAM is where we keep all your recorded sounds.
- **AMY** — a synthesizer. We give AMY commands like "play this sound at pitch C, with this filter, on this step" and AMY does the math.
- **TFT** — "thin-film transistor", a fancy name for a flat color screen. Ours is 2.4 inches diagonally, 240 × 320 pixels, connected by SPI (a kind of serial data bus).

### 1.4 The box: what you actually see and touch

Imagine a small flat board, maybe 6 cm wide and 9 cm tall. Looking at the top face:

```
      +------------------------------------------------------------+
      |              2.4"  TFT   ILI9341   240 x 320               |
      |                 SPI2   GPIO 4/5/6/7/47/48                  |
      |                                                            |
      +------------------------------------------------------------+


    +---------+  +----------+  +---------+   (o)         (o)
    |  SOUND  |  | PATTERN  |  |   BPM   |  Knob A      Knob B
    +---------+  +----------+  +---------+ GPIO 2      GPIO 46
                                            A1_CH1      A1_CH5

    +---+ +---+ +---+ +---+                            +---------+
    | 1 | | 2 | | 3 | | 4 |                            |   REC   |
    +---+ +---+ +---+ +---+                            +---------+
    +---+ +---+ +---+ +---+                            +---------+
    | 5 | | 6 | | 7 | | 8 |                            |   FX    |
    +---+ +---+ +---+ +---+                            +---------+
    +---+ +---+ +---+ +---+                            +---------+
    | 9 | | 10| | 11| | 12|                            |  PLAY   |
    +---+ +---+ +---+ +---+                            +---------+
    +---+ +---+ +---+ +---+                            +---------+
    | 13| | 14| | 15| | 16|                            |  WRITE  |
    +---+ +---+ +---+ +---+                            +---------+
```

There are **23 physical buttons** in total:

- **16 step buttons** in a 4×4 matrix. The same physical buttons are *polymorphic* — their meaning depends on which modifier is held (just like the real PO-33):
  - **no modifier held**: play the **active slot** — the pad index picks the scale degree (melodic slot) or the 1/16 slice (drum slot); the slot itself was chosen earlier with `SOUND + pad`. It is *not* "pad N plays slot N" (see `CONTROL_REFERENCE.md` note A).
  - in **write mode**: the pad is the **step** to toggle for the active slot
  - with **SOUND** held (`SOUND + step_n`): **select** sample slot N
  - with **PATTERN** held (`PATTERN + step_n`): **select** pattern N (and append it to the chain)
  - with **FX** held: select effect 1–15 (**16 = "no effect"**)
  - with **BPM** long-pressed and held: pads 1–5 set the volume level
- **7 dedicated modifier buttons** wired to individual GPIOs (no matrix), laid
  out exactly like the PO-33's modifier row and column:
  - `SOUND` — hold + step 1–16 **selects** a sample slot (the PO-33's "S" key); press the same step with `SOUND` released to play it (strict two-step flow); tap and long-press alone are no-ops
  - `PATTERN` — hold + step 1–16 selects a pattern AND appends it to the song chain (PO-33 "⠛" key); tap and long-press alone are no-ops
  - `BPM` — tap cycles presets (Hip Hop / Disco / Techno); long-press held: **Knob A = swing** (8 levels), **Knob B = fine BPM** (60–240); held + step 1–5 = volume level
  - `REC` — hold + step 1–16 records into that slot; release stops recording (F-001); `REC + PATTERN` held 600 ms clears the active pattern; `REC + BPM` held 5 s = factory reset
  - `FX` — **tap = tweak-mode cycle** (Tone / Filter / Trim, F-015); hold + step 1–15 = punch-in effect; step 16 = "no effect"; held alone 2 s = toggle sync IN
  - `PLAY` — start/stop sequencer
  - `WRITE` — tap = enter/exit write mode (the PO-33's "·" key); long-press = sketch picker

  > The canonical, manual-checked table — with ✅/⚠️/➕/❌ labels for every
  > gesture and a list of the Ravine: Phoenix-only extensions — lives in
  > `docs/CONTROL_REFERENCE.md`. The list above is a summary; cite that file
  > when precision matters.

There are **2 analog knobs**, also called "Knob A" and "Knob B" (just like the PO-33). Each is a 10 kΩ linear potentiometer on an ADC pin (A = GPIO 2 / ADC1_CH1, B = GPIO 46 / ADC1_CH5). They return 0..255 and are debounced in software. On the PO-33 these same knobs are used for fine continuous control. The actual mapping (verified against the [lode/PO-33](https://github.com/lode/PO-33) verbatim manual):
- **BPM row:** knob A = swing (8 discrete levels), knob B = fine tempo (continuous 60–240). Both wired in this commit.
- **Tweak-mode (FX tapped to select):** knob A = pitch / filter cutoff / sample start (Tone / Filter / Trim), knob B = volume / resonance / sample length. Wired per §3.4 (F-016/F-017/F-018); filter resonance (knob B in Filter mode) is read but not yet stored as a per-step field.
- (No "tempo level cycling" knob mapping — that was a confusion in earlier docs; tempo levels are BPM tap only.)

### 1.5 Where to go from here

Once you have read §1, you have a mental model of the device. There are three things you might want to do next, and the rest of this document supports all three:

- **Build the device.** Go to the [project website](https://ravine.1une.cc), where you can flash our firmware onto your board with one click in Chrome / Edge / Firefox over USB. The *Hardware* page there gives a full bill of materials and pin map.
- **Read the rest of this document.** §2 covers every term used in the project. §3 maps every PO-33 feature to a piece of code with an honest done / partial / missing status. §10 explains how our hardware can break the PO-33's limits. §11 is the v2 proposal for multi-**sketch** storage & restore (we call the unit a "sketch" rather than "song"). §12 is a cliff's-notes reading guide for the *Make: Electronic Music from Scratch* book. §14 is the optional Wi-Fi, web-binding, and sketch-upload design (ESP32-S3 PO-33 remake). §15 is the peer-to-peer, no-internet sketch-exchange design (ESP-NOW, two devices, face-to-face). §16 is a glossary.  [UPDATED: §13 → §15 → §16 renumbering propagated from the §14 insertion (in rev 1.0) and from the §15/§16 swap (in rev 1.1, so the Glossary is the last section of the document, per the standing rule); §14 and §15 added to the list of forward-reading pointers.]
- **Read the source.** The repo at the [project URL](https://ravine.1une.cc) (or wherever you got this document) is roughly 3,500 lines of C across `main/audio`, `main/sequencer`, `main/ui`, `main/storage`, `main/system`, and `main/tests`. The audio engine is entirely in the vendored AMY library.

If you are a complete novice, the recommended order is: §2 (concepts) → §3.1 (recording) → §5 (workflows) → build it → §10 (breaking limits) → §11 (multi-sketch) → §12 (the book).

> **Naming.** Throughout this document, the unit of creative work stored on the device is called a **sketch** — one sketch = one pool of samples + 16 patterns + a 128-step chain + metadata. The PO-33 calls this a "song", the Korg Electribe calls it a "Pattern Set", and Ableton Live calls it a "Live Set". We picked "sketch" because it's short, plain, and avoids clashing with sampler vocabulary (we don't use "groove" because that word means *timing templates* elsewhere). The PO-33's "song chain" — an ordered list of up to 128 pattern numbers — still exists; we just call it the *pattern chain* inside a sketch.

---

## 2. Concepts you need first

These are the words we will use a lot in the rest of this document. Read this section once; you can come back to it as a reference.

### 2.1 Sound

A **sound** is what a microphone picks up over a short period of time. If you clap your hands, the microphone hears a half-second "thwack". If you say "hello", the microphone hears about a second of audio. We **record** that audio into the device's memory. Once it's stored, we can play it back any number of times.

### 2.2 Sample

**Sample** is a more technical word for "a short recording". When musicians say "I sampled a piano", they mean "I recorded a piano note into a sampler and now I can play it on the keyboard". In this document **sample** and **recording** mean the same thing.

### 2.3 Slot

A **slot** is a numbered storage location for a sample. Our device has **16 slots**, numbered 1 through 16. The first 8 (slots 1–8) are for short sounds called **drum hits** — a single kick, a single snare, a single hi-hat closed. The next 8 (slots 9–16) are for **melodic sounds** — a bass note, a chord, a vocal snippet — things you might want to play at different pitches.

### 2.4 Step

A **step** is **one sixteenth-note** — one of sixteen evenly-spaced positions in a 4/4 bar of music. The PO-33 has **16 steps** in a row, which makes **one bar of 4/4** (4 beats × 4 sixteenth-notes = 16 sixteenth-notes). Each step is a moment in time. The device walks through the steps one at a time. If you told it "play sound 3 on step 5", it will play sound 3 every time the playhead reaches step 5, then move on.

### 2.5 Pattern

A **pattern** is a complete arrangement of sounds across all 16 steps. A pattern tells the device which sound to play on which step. Our device has **16 patterns**, numbered 1 through 16. A pattern is sometimes also called a "beat" or a "loop".

### 2.6 Song

A **song** is an ordered list of patterns. Our device lets you chain up to **128 patterns** in a row, so a song can be up to 128 × 4 = **512 beats** (128 bars) long. After the last pattern in the song finishes, the device loops back to the first pattern.

### 2.7 BPM (beats per minute)

**BPM** is how fast the music goes. BPM = 120 means there are 120 beats per minute, so each beat lasts half a second (0.5 s), so each step (a sixteenth-note) lasts **an eighth of a second** (0.125 s). The real PO-33 has three "BPM levels" — Hip Hop (80 BPM), Disco (120 BPM), and Techno (140 BPM) — and you can fine-tune from there. Our firmware supports any BPM from 60 to 240.

Quick reference for the math: step duration in seconds = 60 / BPM / 4. So:

| BPM | Step duration | Bar duration (16 steps) |
|---|---|---|
| 60 | 0.25 s | 4 s |
| 80 (Hip Hop) | 0.1875 s | 3 s |
| 120 (Disco) | 0.125 s | 2 s |
| 140 (Techno) | ≈0.107 s | ≈1.71 s |

### 2.8 Sequencer

The **sequencer** is the part of the device that walks through the steps in order and plays the right sound at each step. Think of it as a robot drummer who reads the pattern and hits the right drum at the right time.

### 2.9 FX (effect)

An **effect** (called "FX" for short) is a modification applied to a sound. Effects can be small (slight reverb) or wild (the sound plays backwards). The PO-33 has 16 built-in "punch-in" effects. You press the **FX** button and then a number 1–16 to pick one. The effect applies to whatever you play next.

### 2.10 Tweak parameter

A **tweak parameter** is one of three special settings you can adjust on a sound: **Tone** (how high or low it sounds, and how loud it plays), **Filter** (which frequencies are kept or removed — like a bass-boost or treble-cut on a stereo), and **Trim** (where the recording starts and ends). On the real PO-33 you twist the two knobs (Knob A and Knob B) to change these. Our firmware uses two physical knobs — wired to GPIO 2 (Knob A) and GPIO 46 (Knob B), with the `ui/knobs.c` driver reading them every button-scan tick. (Knob A was reassigned from GPIO 20 to GPIO 2 to free the USB-OTG D+ line for the future USB-MIDI feature — see §10.6.) `BTN_FX` tap cycles the mode (Tone → Filter → Trim → Tone); turning the knobs adjusts the playing step or active slot accordingly. See §3.4 (F-015 through F-018) for the per-mode knob bindings.

### 2.11 Punch-in

**Punch-in** is the name TE gave to those 16 effects. The idea is that you press the FX button to "punch in" an effect, just like a recording engineer in a studio punches in a reverb at the right moment. It is a fancy word for "instant on/off effect".

### 2.12 Polyphony

**Polyphony** means "how many sounds can play at the same time". If you press three buttons at once and hear three different sounds, the device has polyphony ≥ 3. Our device supports **4 simultaneous voices**, which is enough for almost any music you would make on a PO-33.

### 2.13 Sync

**Sync** (short for synchronization) means two devices agreeing on the same tempo. If you have a PO-33 and a Korg volca and you want them to play together, you run a 3.5 mm audio cable between them. One device becomes the "master" (the one that decides the tempo), the other becomes the "slave" or "sync unit". They keep in lock-step so the beats line up.

### 2.14 Loop

**Loop** means "play the same thing over and over". When a sample is "looping", it means the device is repeating the start of the sample as soon as it reaches the end, so a 1-second sample can fill a 60-second song. The PO-33 has four different "loop length" effects (loop 16, loop 12, loop short, loop shorter) which set the loop point at different positions in the sample.

### 2.15 Unison

**Unison** is an effect where the same sound is played multiple times slightly out of phase with itself, which makes it sound thicker, like a chorus of identical singers. "Unison low" is the same but an octave lower.

### 2.16 Stutter

**Stutter** is an effect where a tiny chunk of the sound is repeated many times rapidly, creating a "st-st-st-stutter" effect. "Stutter 4" repeats 4 times per beat; "stutter 3" repeats 3 times per beat.

### 2.17 Quantize

**Quantize** means "snap to a grid". When a sequencer "quantizes" notes, every note is moved to the nearest exact step, so timing is mathematically perfect. The PO-33's "6/8 quantize" effect retimes your pattern so it fits a 6/8 time signature instead of 4/4.

### 2.18 Stutter vs. Reverse vs. Scratch

These are all variations of the same idea: manipulate how the sample plays back in time. **Stutter** repeats a tiny slice. **Reverse** plays it backwards. **Scratch** jumps randomly around the sample like a DJ spinning a record back and forth.

---

## 3. The complete feature-to-code map

This is the heart of the document. Every row is one feature of the real PO-33. For each row we give:

- **PO-33 manual reference** — the section number in the TE manual.
- **Layman explanation** — what the feature does in plain English.
- **PO-33 button combo** — what you press on the real PO-33 to use it.
- **Our hardware combo** — what you press on our firmware (the same button, or a UART command over the serial port).
- **Code location** — the file path and key function name.
- **Status** — ✅ done, ⚠️ partial, ❌ missing.

### 3.1 Section 1 — Sounds (recording)

#### F-001 — Record a sample (mic)

- **Manual ref:** §1.1 (PO-33 manual "sounds" section, record sub-section)
- **Layman:** You press a button, the device starts listening through its microphone, you make a sound, you press the button again, and now the device has that sound stored in one of its 16 slots.
- **PO-33 button combo:** Hold **REC** (star), then press the number of the slot (1–16) where you want to store the sound. Press **REC** again to stop.
- **Our hardware combo:** Hold button "REC", then press a step button 1–16. Hold "REC" again to stop. (Identical button mapping.) Or via UART shell: `rec <slot>` then `stoprec`.
- **Code location:** `main/audio/amy_bridge.c` → `amy_bridge_start_record(uint8_t slot)`, `amy_bridge_stop_record()`, `amy_bridge_pump_capture()`. The recording API works (UART: `rec <slot>` + `stoprec`). Bindings in `main/ui/input.c`:
  - `s_held_modifiers[]` entry `{BTN_REC, rec_on_step, rec_on_release}`. `rec_on_step(N)` calls `amy_bridge_start_record(N - 1)`. `rec_on_release()` calls `amy_bridge_stop_record()` (fired on the true→false edge of `buttons_is_pressed(BTN_REC)` at the end of every drain).
  - The `REC LED` (via `leds_set_rec(true/false)`) mirrors the recording state.
  - First-press-wins: while `BTN_REC` is held, the first step press starts recording; subsequent step presses are no-ops (PO-33 manual: single-slot per REC hold).
- **Status:** ✅ done. PO-33 ground truth (lode/PO-33 README): "hold record (star) + number, make sound, release buttons". Single-slot-per-hold matches the manual wording. The hold-release detection lives in the same end-of-drain polling block as `s_bpm_held`.

#### F-002 — Record from line-in

- **Manual ref:** §1.1
- **Layman:** Instead of using the built-in microphone, you can plug a stereo audio cable into the line-in jack and the device records whatever is on the cable. Useful for sampling a record, a phone, or another synthesizer.
- **PO-33 button combo:** Same as F-001; the device auto-detects line-in vs. mic.
- **Our hardware combo:** Identical. We use the same I²S microphone chip (INMP441) for both, because line-in via a 3.5 mm socket would need an additional ADC. Our firmware treats both sources as "mic" for now.
- **Code location:** `main/audio/amy_bridge.c`. AMY's `i2s.c` is configured with `i2s_din = I2S_IN_DATA_GPIO` (GPIO17); the input buffer is read into PSRAM in `amy_bridge_pump_capture()`.
- **Status:** ⚠️ partial. Mic works. Line-in via a separate 3.5 mm socket is not wired in this version — you'd have to add a second ADC chip.

#### F-003 — Live recording into a playing pattern

- **Manual ref:** §1.2 ("record own sound (live)")
- **Layman:** You start the pattern playing, and while it's looping you hold **WRITE** (·) + the slot number and the device records into that slot **at the same time** as the pattern keeps playing. This is how you "jam" new sounds into an existing beat.
- **PO-33 button combo:** While a pattern is playing, hold **WRITE** + a number 1–16.
- **Our hardware combo:** Not yet implemented. The current firmware only records when the pattern is **stopped**.
- **Code location:** `main/audio/amy_bridge.c` would need a new `amy_bridge_live_record(uint8_t slot)`; `main/sequencer/sequencer.c` would have to keep playing while recording.
- **Status:** ❌ missing.

#### F-004 — 40 seconds total sample memory

- **Manual ref:** §1.1
- **Layman:** Across all 16 slots combined, you can record up to 40 seconds of audio. Each slot is capped: 8 drum slots ≤ 2 s each, 8 melodic slots ≤ 3 s each (in the current firmware; the real PO-33 caps drum slots shorter).
- **PO-33 button combo:** Implicit — the device just won't record past the slot's max.
- **Our hardware combo:** Same.
- **Code location:** `main/config.h` → `SAMPLE_TOTAL_SECONDS 40`, `SLOT_DRUM_MAX_SECONDS 2.0f`, `SLOT_MELODIC_MAX_SECONDS 3.0f`. `main/audio/amy_bridge.c` allocates `SAMPLE_POOL_SIZE_BYTES = 40 × 44100 × 2 ≈ 3.37 MB` in PSRAM.
- **Status:** ✅ done.

#### F-005 — Melodic vs drum slots

- **Manual ref:** §1.1
- **Layman:** Slots 1–8 are for short percussive sounds (drums). Slots 9–16 are for longer sounds that you want to pitch-shift around the musical scale (melodic). Drums play at their recorded pitch; melodic slots play pitched up or down based on which step of the pattern triggers them.
- **PO-33 button combo:** Slots are just numbered 1–16; the type is implicit.
- **PO-33 detail — pad-to-note auto-mapping:** the real PO-33 maps the 16 pads to a chromatic scale one octave wide when the active slot is melodic. Pad 1 plays C4 (MIDI 60), pad 2 plays C#4 (61), ..., pad 13 plays C5 (72), pad 16 plays D#5 (75). **Implemented** — see helper `amy_bridge_auto_note_for_step(slot, step_index)` below.
- **PO-33 detail — pad-to-slice auto-mapping:** when the active slot is a recorded drum slot, the real PO-33 splits the recording into 16 equal slices and each pad triggers 1/16th of it. Pad 1 plays samples 0..len/16-1, pad 16 plays the last 1/16th. **Implemented** — see helper `amy_bridge_auto_slice_for_step(slot, step_index, *loopstart, *loopend)` below. The slice bounds are set on the AMY event BEFORE `apply_fx()` so that any FX which touches loopstart/loopend (LOOP_16, LOOP_SHORT, STUTTER_3, STUTTER_4) overrides the auto-slice — the user's FX choice always wins.
- **Our hardware combo:** Same.
- **Code location:** `main/config.h` → `SLOT_DRUM_COUNT 8`. `main/audio/amy_bridge.c` → `apply_fx()` uses `e.num_voices = (slot < SLOT_DRUM_COUNT) ? 1 : 4`. Two auto-mapping helpers live in `main/audio/amy_bridge.{c,h}`:
  - `amy_bridge_auto_note_for_step(slot, step_index)` — melodic-slot chromatic mapping. Returns 0 for drum slots and 60..75 for melodic slots.
  - `amy_bridge_auto_slice_for_step(slot, step_index, *loopstart, *loopend)` — drum-slot slice bounds. Returns false (no slicing) for melodic slots and empty slots. Honours the slot's trim region.

  Both helpers are called from `main/ui/input.c::play_active_slot()` (no-modifier step press) and `main/sequencer/sequencer.c::on_step()` (sequencer playback). `play_active_slot()` now takes a `step_index_0_to_15` argument; `on_step()` uses the existing `s_step` global; `amy_bridge_play_note()` takes a `step_index_0_to_15` argument that flows through `pending_note_t` for the swing-fire path. Pass `0xFF` to disable both mappings (e.g., the alarm clock fires the whole recording regardless of pad).
- **Status:** ✅ done. Both halves of the PO-33 pad-to-note + pad-to-slice auto-mapping are now implemented. FX still wins for users who want a different loop behaviour.

### 3.2 Section 2 — Patterns (sequencing)

#### F-006 — Play a sound

- **Manual ref:** §1.3
- **Layman:** Hold **SOUND** (S) and press the slot number 1–16, and that slot's sound plays once.
- **PO-33 button combo:** **Two presses.** Per the [lode/PO-33](https://github.com/lode/PO-33) verbatim manual:
  1. **select sound** — hold S + number (1–16) → selects that slot.
  2. **play a sound** — *with S released*, press the same number → the selected slot plays once.

  The layman line above is a simplification — it implies one press, but the real PO-33 is a strict two-step flow.
- **Our hardware combo:** Strict two-step.
  - `SOUND` (`BTN_SOUND`, GPIO 11 — leftmost of the top row) held + step 1–16 → calls `sequencer_set_active_slot(step - 1)`.
  - Step 1–16 pressed with **no modifier held** + `active_slot != 0xFF` → `amy_bridge_play_note(slot, 60, 100, active_fx, 0, 0)`. `midi_note = 60` (middle C) and `velocity = 100` are v1 defaults; the PO-33 itself doesn't expose these for slot-play mode.
  - Tap or long-press of `SOUND` alone is a no-op (no PO-33 behaviour attached to it; the only PO-33 SOUND verbs are "select sound" and "record" — F-001 — both of which require holding + a number).
- **Code location:** `main/ui/input.c` → `s_modifiers[]` entry `{BTN_SOUND, sound_on_step}`; `sound_on_step()` calls `sequencer_set_active_slot(step - 1)`. The "step press without modifier" branch in `input_drain()` calls `play_active_slot()` which invokes `amy_bridge_play_note()`. `main/sequencer/sequencer.c` → `sequencer_set_active_slot()`, `sequencer_get_active_slot()`.
- **Status:** ✅ done (strict PO-33 ground truth; see trade-off below).

  **Trade-off.** F-006's *Layman* line implies a one-press ergonomics (hold + number = plays once). The PO-33 manual is unambiguous about two presses. We honour the manual. If the layman flow is preferred, the change is a one-liner in `sound_on_step()`: instead of `sequencer_set_active_slot(...)`, call `amy_bridge_play_note(...)` directly.

#### F-007 — 16 patterns

- **Manual ref:** §2 (PO-33 manual "patterns")
- **Layman:** You can store 16 different patterns on the device, numbered 1–16. Each pattern is a complete beat on its own. You switch between them with the bottom-right button group.
- **PO-33 button combo:** Hold **PATTERN** (⠛) + 1–16 to pick one.
- **Our hardware combo:** Single `PATTERN` button (`BTN_PATTERN`, GPIO 44 — middle of the top row), exactly like the real PO-33. **Hold `PATTERN` + press step 1–16** to load that pattern. **Tap and long-press of `PATTERN` alone are no-ops** — there is no tap-to-step or ±1 mod-16 behaviour, because that doesn't exist on the real PO-33 (verified against the [lode/PO-33](https://github.com/lode/PO-33) verbatim mirror of the operator manual: the only PATTERN behaviours are "select pattern — hold + number" and "change patterns — hold + number(s)"). UART: `pattern 5` (sets an arbitrary integer pattern directly; does not use the hold-+-number machinery).
- **Code location:** `main/sequencer/sequencer.h` → `PATTERN_COUNT 16`, `sequencer_set_pattern(uint8_t)`. `main/ui/input.c` → `s_modifiers[]` table with `{BTN_PATTERN, pattern_on_step}`; `pattern_on_step(step_1_to_16)` calls `sequencer_set_pattern(step - 1)`. `main/config.h` → `BTN_PATTERN`, `BTN_PATTERN_GPIO`.
- **Status:** ✅ done. Strict PO-33 ground truth (the matrix step routed while the modifier is held).

#### F-008 — 16 steps per pattern

- **Manual ref:** §2
- **Layman:** Each pattern has 16 "slots in time" called steps. Each step is one sixteenth-note. The sequencer walks 1→2→...→16→1→...
- **PO-33 button combo:** Implicit; the steps are the 16 polymorphic numbered buttons (1–16).
- **Our hardware combo:** Same — the 4×4 matrix holds 16 step buttons directly, numbered 1–16. When a modifier button (SOUND-equivalent, PATTERN-equivalent, FX) is held, the same buttons mean slot 1–16 / pattern 1–16 / effect 1–16 instead.
- **Code location:** `main/sequencer/pattern.h` → `STEPS_PER_PATTERN 16`.
- **Status:** ✅ done.

#### F-009 — Write mode (assigning sounds to steps)

- **Manual ref:** §2.1
- **Layman:** You enter "write mode" by pressing **WRITE** (·). Now when you press a step button, the slot you select gets added to that step. Press WRITE again to exit write mode.
- **PO-33 button combo:** Press WRITE → press a slot number → press step numbers where you want that slot to play → press WRITE again.
- **Our hardware combo:** The `WRITE` button now exists (`BTN_WRITE`, GPIO 43 — bottom of the right column), matching the PO-33's "·" key, but the write-mode handler is not wired yet. For now use UART: `pattern 0`, then `sequencer_set_step_slot(0, 5, 3, 60)`.
- **Code location:** `main/ui/input.h` → `write_mode_is_active()`, `write_mode_enter()`, `write_mode_exit()`, `write_mode_apply_step()`. The dispatcher in `main/ui/input.c` toggles write mode on `BTN_WRITE` tap (when the picker is inactive) and routes step presses through `write_mode_apply_step()` when write mode is active. Storage: `main/sequencer/pattern.{c,h}` → `pattern_get_step()`, `pattern_set_step()`. Slot selection uses the existing `sequencer_get_active_slot()` (set by `BTN_SOUND + step N`).
- **Status:** ✅ done. Tap=active step binds (F-010 toggle), long-press=clears (F-011). Requires an active slot; without one, step presses are no-ops (matching the PO-33 flow: WRITE → SOUND+number → STEP). The `display.c` top status bar shows a yellow `WRITE` label while the mode is active.

#### F-010 — Fill a step (press to add, press again to remove)

- **Manual ref:** §2.1
- **Layman:** When you press a step button while in write mode, the slot gets added. Press the same step again to remove it. Pressing the same step multiple times toggles it.
- **PO-33 button combo:** Press step → press step again to toggle off.
- **Our hardware combo:** Same — see F-009. Each step button is a toggle.
- **Code location:** `main/ui/input.c` → `write_mode_apply_step()` with `is_long=false`. Toggle: if `step.slot_id == active_slot`, clear; otherwise assign.
- **Status:** ✅ done (write-mode-aware toggle). The toggle uses the existing `step_t.slot_id` 0xFF sentinel for the "empty" state.

#### F-011 — Clear a step

- **Manual ref:** §2.1
- **Layman:** Press WRITE + the step number to remove the slot from that step.
- **PO-33 button combo:** WRITE + step.
- **Our hardware combo:** Long-press the step button while in write mode (F-009) to clear that step's slot assignment. Routed through `write_mode_apply_step(N, is_long=true)` which sets `step.slot_id = 0xFF` and clears `plock_active`. The long-press semantics live on the same line as the F-010 tap semantics, so write-mode + a step press triggers one or the other based on `ev.long_press`.
- **Code location:** `main/ui/input.c` → `write_mode_apply_step(N, true)` (the `is_long` branch). Storage: `main/sequencer/pattern.c` → `pattern_set_step()`.
- **Status:** ✅ done. Long-press on a step in write mode clears it regardless of whether the step currently points at the active slot.

#### F-012 — Clear an entire pattern

- **Manual ref:** §2.1 ("press record + pattern to clear the active pattern")
- **Layman:** Hold **RECORD** + **PATTERN** and the entire current pattern is wiped (all 16 steps cleared).
- **PO-33 button combo:** Hold REC + PATTERN.
- **Our hardware combo:** Hold REC + PATTERN for at least `BTN_LONG_PRESS_MS` (600 ms). UART: `pattern 0; ... (clear all steps)`.
- **Code location:** `main/sequencer/sequencer.{c,h}` → `sequencer_clear_current_pattern()` (wraps `pattern_clear()` on `g_patterns[s_pattern]`). `main/ui/input.c` → multi-button gesture detection in the end-of-drain polling block (`s_rec_pattern_combo_started_us` timestamp; both-buttons-held edge → start timing; elapsed ≥ 600 ms → fire; latch until release).
- **Status:** ✅ done. The 600-ms hold-to-fire gate prevents accidental clears from brushing against REC while pressing PATTERN. The latch (set to -1 after firing; reset only when both buttons released) prevents double-clears if the user keeps holding.

### 3.3 Section 3 — Songs (pattern chaining)

#### F-013 — Chain up to 128 patterns into a song

- **Manual ref:** §3
- **Layman:** You can build a "song" by listing patterns in order: 1, 1, 1, 2, 3, 3, ... up to 128 entries. When you play, the device walks through the list. When it reaches the end, it loops back to the start.
- **PO-33 button combo:** Hold **PATTERN** + 1–16 to add the current pattern to the chain; repeating the same pattern multiple times in the chain makes it play that many times before advancing.
- **Our hardware combo:** Hold `PATTERN` and press numbers 1–16 in sequence; each press both selects that pattern AND appends it to `g_chain[]` (per the PO-33 manual: "hold pattern (⠛) + number(s)" → "choosing a single pattern multiple times is allowed"). The chain plays in order, looping back to the start. UART: `pattern N` still calls `sequencer_set_pattern(N)` without appending, so the user can pre-select a pattern without touching the chain.
- **Code location:** `main/sequencer/sequencer.{c,h}` → `sequencer_chain_append(uint8_t)`, `sequencer_chain_clear()`, `g_chain[]`, `g_chain_len`. `on_step()` increments `chain_idx` and rewrites `s_pattern` when reaching the chain end. `main/ui/input.c` → `s_modifiers[]` entry `{BTN_PATTERN, pattern_on_step}`; `pattern_on_step()` calls both `sequencer_set_pattern()` and `sequencer_chain_append()`.
- **Status:** ✅ done. PO-33 ground truth. Reorder / remove from chain (F-014) still queued (would need a chain-edit mode; current chain is append-only).

#### F-014 — Reorder / remove patterns from chain

- **Manual ref:** §3
- **Layman:** You can change the order of patterns in a chain by re-pressing them in a new sequence.
- **PO-33 button combo:** Hold PATTERN + number. (The manual describes chain editing in terms of "re-add in new order"; we don't have a documented remove verb.)
- **Our hardware combo:** Chain editing is UART-only: `chain show`, `chain remove <pattern>`, `chain clear`, `chain swap <i> <j>`, `chain insert <at> <pattern>`. Together these give the user complete in-place chain editing: insert to add a missing entry, swap to fix a wrong order, remove to delete. **No button combo for chain edit** because the existing PATTERN+step is "append"; there's no symmetric "remove" gesture that doesn't conflict, and a "swap" gesture would be a 3+ button combo.
- **Code location:** `main/sequencer/sequencer.{c,h}` → `sequencer_chain_remove()`, `sequencer_chain_count()`, `sequencer_chain_swap(i, j)`, `sequencer_chain_insert(at, pattern)`, `sequencer_chain_at()`, `sequencer_chain_len()`. `main/main.c` → UART `chain show|remove|clear|swap|insert` verbs.
- **Status:** ✅ done (UART only). All chain editing is reachable via the `chain_*` UART verbs. Button combo deferred.

### 3.4 Section 4 — Tweaking (Tone / Filter / Trim)

#### F-015 — Pick a tweak parameter (Tone / Filter / Trim)

- **Manual ref:** §4
- **Layman:** You cycle between three tweak modes by pressing **FX** repeatedly: Tone, Filter, Trim. Each one has two knobs (A and B) that change different aspects.
- **PO-33 button combo:** Press FX to cycle: Tone → Filter → Trim → Tone.
- **Our hardware combo:** Same `FX` button (`BTN_FX`, GPIO 12 — top of the right column under Knob B) has two roles, just like the real PO-33:
  - **Tap** = cycle tweak parameter (Tone → Filter → Trim → wrap).
  - **Held + step 1–15** = select a punch-in effect (F-019).
  - **Held + step 16** = "no effect" (clears active FX; the full "clear effect in pattern" combo lands in write mode, queued).
- **Code location:** `main/ui/input.h` → `tweak_mode_t` enum + `tweak_get_mode()`. `main/ui/input.c` → `s_tweak_mode` state + `tweak_mode_cycle()` (called from the `BTN_FX` tap dispatch) + `tweak_apply_step()` / `tweak_apply_slot()` (called from the sequencer `on_step` and `play_active_slot` respectively). The cycle is Tone → Filter → Trim → Tone, never NONE.
- **Status:** ✅ done. `BTN_FX tap` cycles the mode; long-press is a no-op (the held-+-step punch-in path from F-019 fires on step events before reaching the per-btn switch). The TFT now renders the active tweak-mode label in the top status bar (cyan text at x=130, between BPM and the clock; suppressed while write mode is active so the two indicators don't visually fight — `main/ui/display.c` `render_main_screen()`).

#### F-016 — Tweak Tone (Knob A = pitch, Knob B = volume)

- **Manual ref:** §4.1 ("ton")
- **Layman:** With Tweak = Tone, **Knob A** raises or lowers the pitch of the current sound by semitones; **Knob B** raises or lowers its volume.
- **PO-33 button combo:** Turn knob A / B.
- **Our hardware combo:** Knob A → GPIO 2 (ADC1_CH1); Knob B → GPIO 46 (ADC1_CH5). The knob driver (`ui/knobs.c`) reads them on every button-scan tick. Wiring through to per-step pitch/volume is v2 work; the data model (`step_t.note`, `step_t.velocity`) already supports it.
- **Code location:** `step_t.note` (pitch), `step_t.velocity` (volume); `ui/knobs.c` reads; `main/audio/amy_bridge.c` writes into `amy_event.midi_note` / `amy_event.velocity`.
- **Status:** ✅ done while `s_tweak_mode == TWEAK_TONE`. `tweak_apply_step()` reads `knobs_get_a()` → MIDI note (0..255 → 36..97, mapping C2..C7) and `knobs_get_b()` → velocity (0..255 → 0..127). The sequencer's `on_step()` and `play_active_slot()` call `tweak_apply_step()` before `amy_bridge_play_note()`. v1 lacks `resonance` as a per-step field (F-017 below); the knob B reading in Filter mode is logged but not stored.

#### F-017 — Tweak Filter (Knob A = LP/HP, Knob B = resonance)

- **Manual ref:** §4.2 ("Flt")
- **Layman:** With Tweak = Filter, **Knob A** picks the cutoff frequency (low-pass or high-pass); **Knob B** picks how strong the filter is (resonance).
- **PO-33 button combo:** Turn knobs.
- **Our hardware combo:** Same knob GPIOs (A = GPIO 2, B = GPIO 46). The filter data field is `step_t.filter_cutoff` (0–255); v1 firmware maps a low-pass sweep into `amy_event.filter_freq` via the existing `PO33_FX_FILTER_SWEEP` effect.
- **Code location:** `step_t.filter_cutoff`; `main/audio/amy_bridge.c` → `apply_fx()` → `PO33_FX_FILTER_SWEEP` sets `e->filter_type = FILTER_LPF` and `e->filter_freq`. `main/ui/input.c` → `tweak_apply_step()` in `TWEAK_FILTER` mode reads `knobs_get_a()` → `filter_cutoff` (0..255). Resonance knob is logged but no per-step field exists yet; v2 adds a `resonance` field to `step_t`.
- **Status:** ✅ done. Both knobs are wired through to audio: `step_t.filter_resonance` (new field, 0..255) carries the knob B value alongside the existing `step_t.filter_cutoff`, and `amy_bridge_play_note()` maps them onto AMY's `filter_freq` and `filter_resonance` fields whenever `filter_cutoff > 0`. `PO33_FX_FILTER_SWEEP` continues to clobber the per-step filter (it's a punch-in effect that explicitly drives the filter). Mapping: cutoff 0..255 → `filter_freq` 0..8000 Hz; resonance 0..255 → `filter_resonance` 1.0..6.0. The per-step data is persisted in LittleFS via `sizeof(pattern_t)`; adding the field shifts the on-disk format — v1 has no shipped firmware, so existing flash contents are not preserved.

#### F-018 — Tweak Trim (Knob A = start point, Knob B = length)

- **Manual ref:** §4.3 ("tri")
- **Layman:** With Tweak = Trim, you pick where in the recording the playback actually starts (**Knob A**), and how long the playback lasts (**Knob B**). This is how you chop off silence at the start, or trim a long recording to a short snippet.
- **PO-33 button combo:** Turn knob A = start, B = length.
- **Our hardware combo:** Same knob GPIOs (A = GPIO 2, B = GPIO 46). Per-slot trim is stored in `s_slots[slot].start` / `.end`. In TWEAK_TRIM mode, Knob A sets the start and Knob B the end, via `tweak_apply_slot()` → `amy_bridge_set_trim()`.
- **Code location:** `main/audio/amy_bridge.c` → `amy_bridge_set_trim(slot, start, end)`. `s_slots[slot].start` / `.end` are stored.
- **Status:** ✅ done while `s_tweak_mode == TWEAK_TRIM`. `tweak_apply_slot(slot)` reads `knobs_get_a()`/`b()` → trim start/end (mapped to sample indices 0..slot_len-1 via `amy_bridge_slot_ptr()` + `amy_bridge_slot_len_samples()`) and calls `amy_bridge_set_trim()`. Triggered from `play_active_slot()`; the per-step sequencer path doesn't apply trim (no per-step trim field, and the user is expected to trim the active slot, not individual pattern steps).

### 3.5 Section 5 — Effects (the 15 punch-ins + "no effect")

See §4 below for the per-effect deep dive.

#### F-019 — Pick an effect

- **Manual ref:** §5
- **Layman:** Press **FX** to enter effect-pick mode, then press a number 1–16 to select one of the 16 effects. The effect applies to whatever you play next.
- **PO-33 button combo:** Per the [lode/PO-33](https://github.com/lode/PO-33) verbatim manual:
  - Hold **FX** + number (1–15) — add & save effect in pattern.
  - **Step 16** in the PO-33 manual's effects table is "**no effect**" (the 16th row reads "16. no effect"). The combo "[write mode] [play] hold fx (FX) + 16" is "clear effect in pattern" — a separate write-mode-dependent action, NOT swing. Earlier commits mislabelled step 16 as a "swing stub"; that was a fabrication.
- **Our hardware combo:** Strict PO-33: `FX` (`BTN_FX`, GPIO 12 — top of the right column under Knob B) held + step 1–15 → calls `sequencer_set_active_fx(PO33_FX_LOOP_16 + (step - 1))`. Step 1–15 maps to enum values `PO33_FX_LOOP_16` … `PO33_FX_FILTER_SWEEP`. Step 16 → `sequencer_set_active_fx(PO33_FX_NONE)` (the manual's "no effect" entry; the PO-33's "clear effect in pattern" half of the combo lands in write mode, queued).

  The selected FX is then **carried into the next note** by `sequencer_on_step()` (reads `s_active_fx` if the step's `effect` field is `PO33_FX_NONE`). Per-step effects take precedence — the FX modifier sets a *fallback*, not an override.

  **Tap** of `FX` alone is the **tweak-mode cycle** (Tone → Filter → Trim → Tone) — see F-015. Long-press is a no-op.

  **Where swing actually lives:** BPM-held + Knob A. Not a step press at all. See F-020.
- **Code location:** `main/ui/input.c` → `s_modifiers[]` entry `{BTN_FX, fx_on_step}`; `fx_on_step()` step 1..15 maps to `PO33_FX_LOOP_16 + (step - 1)`, step 16 sets `PO33_FX_NONE`. `main/sequencer/sequencer.{c,h}` → `sequencer_set_active_fx()`, `sequencer_get_active_fx()`, and `on_step()` reads `s_active_fx` as the fallback when the step's per-step effect is `PO33_FX_NONE`. `main/audio/amy_bridge.{c,h}` → `po33_fx_t`, `apply_fx()`.
- **Status:** ✅ done. Step 1..15 selects punch-ins; step 16 = "no effect" (no swing). **Save-in-pattern:** when write mode is active, FX + step N also bakes the chosen FX into the per-step `effect` field of every bound step of the active pattern (`sequencer_save_fx_to_pattern()` in `main/sequencer/sequencer.c`, called from `fx_on_step` in `main/ui/input.c`). Outside write mode, FX+step is a one-shot (sets `s_active_fx`, leaves per-step data untouched). PO-33's write-mode gating is preserved. p1/p2 are left at 0; per-step parameter tuning (the v2 "step FX is FX p1=knobA p2=knobB" form) is not yet wired.

### 3.6 Section 6 — BPM and tempo

#### F-020 — Change tempo (BPM)

- **Manual ref:** §6
- **Layman:** Hold **BPM** and turn Knob A to fine-tune the tempo, or press BPM repeatedly to cycle between three preset levels: Hip Hop (80), Disco (120), Techno (140).
- **PO-33 button combo:** Per the [lode/PO-33](https://github.com/lode/PO-33) verbatim manual, the BPM row has *three* distinct combos (each one was previously confused with the others in our docs; corrected in this commit):
  - `change tempo (pre-defined modes)` — **press BPM** to toggle between levels.
  - `change swing` — **hold BPM + turn Knob A** (8 discrete levels, 0=no swing, 7=max).
  - `change tempo (fine tuned)` — **hold BPM + turn Knob B** (continuous, 60–240 BPM).
  - (`change volume` — hold BPM + number 1–5; see F-022. Different axis again.)
- **Our hardware combo:** `BPM` (`BTN_BPM`, GPIO 13 — rightmost of the top row) has the following behaviours:
  - **Tap** advances to the next preset level (Hip Hop → Disco → Techno → wrap). See F-021 for the table.
  - **Long-press (held)** enters BPM-mode: while held, **Knob A adjusts swing** (8 levels) and **Knob B fine-tunes BPM** (continuous, [60..240]). Each axis has its own deadzone + last-reading for delta detection; both fire on knob movement past the deadzone. On release, both values are kept (no separate commit step).
  - **Held + step 1–5**: sets the volume level (F-022). Step 6–16 is a no-op (PO-33 doesn't define a behaviour).
  - UART: `bpm 140` (sets an arbitrary integer BPM; does not use the preset machinery).
- **Code location:** `main/sequencer/sequencer.c` → `sequencer_set_bpm(uint16_t)`, `sequencer_cycle_bpm_preset()`, `sequencer_set_swing(uint8_t)`, the `on_step()` swing-delay path, `on_swing_fire()` timer callback. `main/ui/input.c` → `BTN_BPM` case (tap/long-press split), `knob_a_to_swing()` mapper, `knob_b_to_bpm()` mapper, `s_bpm_held` mode + held-mode polling. `main/ui/buttons.{c,h}` → `buttons_is_pressed(uint8_t)` (held-state poll). `main/config.h` → `MIN_BPM 60`, `MAX_BPM 240`, `BPM_PRESETS`, `SWING_LEVELS 8`, `SWING_MAX_PERCENT 50`.
- **Status:** ✅ done. Tap cycles presets (F-021); long-press + Knob A sets swing; long-press + Knob B fine-tunes BPM; held + step 1–5 sets volume level (F-022).

#### F-021 — Three preset BPM levels (Hip Hop / Disco / Techno)

- **Manual ref:** §6
- **Layman:** The PO-33 has three named BPM levels you can cycle through. The PO-33 calls these "levels" because BPM is shown as a numeric value but it remembers the level name.
- **PO-33 button combo:** Press BPM repeatedly.
- **Our hardware combo:** Each tap of `BTN_BPM` advances `s_preset_idx` (modulo 3) and calls `sequencer_set_bpm(presets[idx])`. The cycle starts at **Disco** (the `DEFAULT_BPM` 120), so the first tap lands on **Techno**, the next on **Hip Hop**, then back to Disco. Off-preset values (set by Knob B under long-press) are *not* snapped to the nearest preset on the next tap — the cycle advances slot-by-slot from whatever the last preset delivered.
- **Code location:** `main/config.h` → `BPM_PRESETS` (macro) + `BPM_PRESET_COUNT 3`. `main/sequencer/sequencer.c` → `sequencer_cycle_bpm_preset()`. The static `s_preset_idx` lives inside that function (initial value 1 = Disco).
- **Status:** ✅ done.

### 3.7 Section 7 — Headphone volume

#### F-022 — Headphone volume level (5 levels)

- **Manual ref:** §7
- **Layman:** You can pick one of 5 headphone volume levels so the device isn't too loud or too quiet for headphones.
- **PO-33 button combo:** Hold BPM + 1–5.
- **Our hardware combo:** Hold BPM + step 1–5 → `amy_bridge_set_volume_level(N)`. The level is stored as 0..5 (5 = max, 0 = silent). AMY's public API does not expose a master gain, so the multiplier is applied per-note inside `amy_bridge_play_note()`: `velocity_out = (velocity_in / 127) * volume_multiplier(level)`. Multiplier curve: 0/0.25/0.5/0.75/0.9/1.0 (slightly compressed at the top end to avoid the perceptual jump between levels 4 and 5).
- **Code location:** `main/audio/amy_bridge.{c,h}` → `amy_bridge_set_volume_level(uint8_t)` + static `s_volume_level` + `volume_multiplier()`. The multiplier is applied inside `amy_bridge_play_note()`. `main/ui/input.c` → BPM-held + step 1–5 branch in `input_drain()` (routes BEFORE the modifier lookup because BPM isn't in the press-bound `s_modifiers[]` table; its mode is polled).
- **Status:** ✅ done. **Display-half:** after the user holds BPM + step 1..5 to set the volume, the TFT briefly shows a 5-bar "level meter" centred at y=120 (green bars 1..N, yellow bar at the maximum level). The overlay persists for ~1.2 s and then auto-clears back to the main screen (`main/ui/display.c` `display_show_volume_level()` + `render_volume_overlay()`, deadline-driven from `display_tick()`; no separate timer task). The overlay is **also** armed when `display_show_volume_level()` is called with the current level, so a future "press BPM to display volume" wiring (the audit item 26 mention in §7) can just call it directly. The PO-33 lit-buttons behaviour cannot be replicated 1:1 (we don't have lit buttons), but the level-bar overlay reads as the same intent.

### 3.8 Section 8 — Copy and delete

#### F-023 — Copy a sound to another slot

- **Manual ref:** §8
- **Layman:** Hold **WRITE** + **SOUND** + 1–16 and the active sound is copied to that slot.
- **PO-33 button combo:** WRITE + SOUND + number.
- **Our hardware combo:** UART only: `slot_copy <dst 0..15> <src 0..15>` (e.g. `slot_copy 9 3` to duplicate slot 3 into slot 9). The WRITE + SOUND + step button gesture is technically possible but adds a 3-button combo; deferred to v2.
- **Code location:** `main/audio/amy_bridge.{c,h}` → `amy_bridge_copy_slot(uint8_t dst, uint8_t src)`. Memmoves the pool to make room (or reclaim space) at `dst`'s position, copies src's bytes into the now-fitting dst region, then re-registers both AMY presets via `pcm_load_external()`. Refuses the copy if either slot is currently being recorded into, or if src's length exceeds dst's class max (drum slots max 2 s = 88200 samples; melodic slots max 3 s = 132300 samples).
- **Status:** ✅ done (UART only). Button combo deferred.

#### F-024 — Copy a slice of a drum sample

- **Manual ref:** §8
- **Layman:** Same as F-023 but only copies one slice (one of the 16 auto-sliced parts) to a new drum slot.
- **PO-33 detail — slice concept:** the real PO-33 splits each drum-slot recording into 16 equal-length slices by default. Each pad (1–16) plays the corresponding 1/16th of the recording. This pad-to-slice mapping is **now auto-applied** in our v1 firmware via `amy_bridge_auto_slice_for_step()`; a recorded drum slot triggers the Nth 1/16th on the Nth pad. See F-005 for the cross-reference and the FX precedence rule (FX that touch loop bounds, like LOOP_16, override the auto-slice).
- **PO-33 button combo:** WRITE + SOUND + 9–16 (drum slot) + slice number 1–16.
- **Our hardware combo:** Auto-slicing is automatic; the WRITE+SOUND+step "copy slice" button combo is not yet wired (the UART verb is also unimplemented).
- **Code location:** Slicing helper: `main/audio/amy_bridge.{c,h}` → `amy_bridge_auto_slice_for_step(slot, step_index, *loopstart, *loopend)`; called from `amy_bridge_play_note()` when `slot < SLOT_DRUM_COUNT` and `step_index_0_to_15 != 0xFF`. Copy-slice verb: `main/audio/amy_bridge.c` → would need a `amy_bridge_copy_slice(src_slot, slice_index, dst_slot)` that registers a new (empty) drum slot, allocates a fresh PSRAM region of size `slot_len / 16`, copies the slice bytes into it, and re-registers the AMY preset.
- **Status:** ⚠️ partial. **Auto-slicing is implemented.** Copying a slice to a new drum slot (the WRITE+SOUND+step button combo and its UART equivalent) is still ❌ missing — the helper exists for the playback path but not for the copy path.

#### F-025 — Copy an entire pattern

- **Manual ref:** §8 ("hold write + pattern and press 1-16 to paste the active pattern to the corresponding new slot")
- **Layman:** Hold WRITE + PATTERN and the active pattern is copied to another pattern slot.
- **PO-33 button combo:** WRITE + PATTERN + number.
- **Our hardware combo:** UART only: `pattern_copy <dst 0..15> <src 0..15>` (e.g. `pattern_copy 9 5` to duplicate pattern 5 into slot 9). The WRITE+PATTERN+step button gesture is technically possible but adds another 3-button combo to a UI that's already juggling BPM+step, REC+PATTERN, REC+BPM, WRITE long-press for the picker, etc. Deferred to v2.
- **Code location:** `main/sequencer/pattern.{c,h}` → `pattern_copy(uint8_t dst, uint8_t src)`. Deep-copy of the whole `pattern_t` (16 steps + name). Slot sample data is not affected.
- **Status:** ✅ done (UART only). Button combo deferred.

#### F-026 — Delete a sound

- **Manual ref:** §8 ("delete sound: hold record + sound")
- **Layman:** Hold RECORD + the slot's number and the slot is wiped empty.
- **PO-33 button combo:** REC + slot number.
- **Our hardware combo:** UART only: `slot_clear <N>` (e.g. `slot_clear 3`). The REC + step long-press gesture is technically possible but the existing REC held+step path is wired to `amy_bridge_start_record()` (which is the REC-for-record semantic). Adding a long-press variant means refactoring the held-modifier dispatch to distinguish tap vs long-press, which is non-trivial. Deferred to v2.
- **Code location:** `main/audio/amy_bridge.{c,h}` → `amy_bridge_clear_slot(uint8_t slot)`. Unregisters the preset from AMY, zeroes the sample data, and resets `s_slots[slot]` to empty. Refuses to clobber a slot that is currently being recorded (would race the I²S capture path).
- **Status:** ✅ done (UART only). Button combo deferred.

#### F-027 — Delete a pattern

- **Manual ref:** §8 ("press record + pattern to clear the active pattern")
- **Layman:** Hold REC + PATTERN and the current pattern is wiped.
- **PO-33 button combo:** REC + PATTERN.
- **Our hardware combo:** Same gesture as F-012 (clear active pattern). **Already implemented** as F-012 (commit `a0adddrc`); this F-ID is a duplicate of F-012 in the lode/PO-33 README.
- **Status:** ✅ done (covered by F-012).

### 3.9 Section 9 — Data transfer

#### F-028 — Transfer sounds and patterns between two PO-33s

- **Manual ref:** §9.1
- **Layman:** You run a 3.5 mm stereo audio cable from the line-out of one PO-33 to the line-in of another. Hold WRITE + SOUND + PLAY on the sending unit, hold WRITE + SOUND + RECORD on the receiving unit, and the second unit receives a copy of all sounds and patterns. (Note: this erases the receiving unit first.)
- **PO-33 button combo:** As above.
- **Our hardware combo:** Not yet. We'd need to encode a packet stream over audio and a corresponding decoder.
- **Code location:** Would be a new `main/audio/p2p.c` module.
- **Status:** ❌ missing.

#### F-029 — Back up to / restore from a stereo recording device

- **Manual ref:** §9.2
- **Layman:** You can back up your PO-33 by playing its data into a tape recorder / phone / computer. Later you play that recording back into another PO-33 to restore. (Note: this erases the receiving PO-33 first.)
- **PO-33 button combo:** As above (same wire protocol as F-028).
- **Our hardware combo:** Same.
- **Status:** ❌ missing.

#### F-039 — Peer-to-peer sketch exchange over ESP-NOW (face-to-face, no internet)

- **Manual ref:** None — this is a new capability, not in the original PO-33. The original device's §9 transfer requires a 3.5 mm audio cable and a handshake that erases the receiving unit (see F-028). ESP-NOW exchange is a wireless, *additive* transfer path that does not erase, does not need a router, and does not need a server.
- **Layman:** Two PO-33 K.O! devices, side by side, can copy a sketch from one to the other with no internet, no router, no phone, no audio cable. You pick "Send sketch" on one, pick "Receive sketch" on the other, type a 6-digit code on the receiver to confirm the sender, and the sketch arrives as a new sketch in the receiver's picker (the receiver's existing sketches are untouched). Distance: up to ~30 m line-of-sight, ~10 m indoors. The transfer is encrypted with a key derived from the 6-digit code.
- **PO-33 button combo:** N/A (no analog on the original).
- **Our hardware combo:** **Hold WRITE + SOUND for 2 s** opens the **Transfer** menu on the TFT. Knob A scrolls **Send sketch** / **Receive sketch** / **Cancel**; click the encoder to pick. On the receiver, the 6-digit code is entered via the **step buttons**: columns map to 4-3-2-1 (left-to-right), rows to A-B-C-D (top-to-bottom); the final 6th digit is shown as a static value, and the encoder click submits. (The 4×4 matrix gives a 16-key keypad, which is more than enough for 0–9 + submit/backspace; the 6-digit code is short enough that the keypad is comfortable.) Both devices show a progress bar (0–100 %) and the post-transfer summary (sketch ID, name, byte count, sketches.bin SHA-256, elapsed time).
- **Code location:** New `main/p2p/espnow_p2p.{c,h}` module (ESP-NOW init, peer table, send/recv, retry, fragmentation), `main/p2p/transfer_protocol.{c,h}` (the sketch-blob chunk / ack / NACK state machine), and `main/p2p/pairing.{c,h}` (the 6-digit code → AES-128 key derivation). Sender reads the active sketch from §11.2's `samples.bin` + `patterns.bin` + `chain.bin` + `meta.bin` and packages it as a single binary blob of ≤ 50 MB; receiver unpacks it into a new sketch folder under `/sketches/` and updates `sketches.lst`.
- **Status:** ⚠️ **partial.** The protocol (chunk size, retry policy, pairing key derivation, packet format) and the §11 blob reuse are designed in §15. The `sdkconfig` already enables the WiFi + ESP-NOW subsystems (`CONFIG_ESP_WIFI_ESPNOW_MAX_ENCRYPT_NUM=7`, `CONFIG_ESP_WIFI_SOFTAP_SUPPORT=y`). The UI screen, the send/recv state machine, and the chunked-transfer task are not yet committed. No code in §15 is in the tree; this is a v2 design.

### 3.10 Section 10 — Sync

#### F-030 — Jam-sync out (pulse on each 16th note)

- **Manual ref:** §10
- **Layman:** On every sixteenth note the device sends a 1 ms electrical pulse out of its sync-out jack. Another device listening on sync-in can use that pulse as its tempo reference.
- **PO-33 button combo:** Implicit; always on while playing.
- **Our hardware combo:** Same. GPIO 18 is the sync-out pin.
- **Code location:** `main/system/sync.c` → `sync_pulse()` toggles GPIO 18 high for 1 ms. Called from `sequencer.c` per step.
- **Status:** ✅ done.

#### F-031 — Jam-sync in (listen to a master device)

- **Manual ref:** §10
- **Layman:** Set the PO-33 to "sync mode", press PLAY, and it will not play on its own — it waits for an external pulse on the sync-in jack, then plays one step per pulse.
- **PO-33 button combo:** Hold RECORD + BPM to cycle sync modes SY0–SY5.
- **Our hardware combo:** **Long-press FX for 2 seconds** (alone, no other modifier held). Toggles `sequencer_set_sync_in_active()` on/off. We use FX instead of REC+BPM because REC+BPM is already used for the F-037 factory-reset (5 s hold). FX tap is tweak-mode cycle (different); FX + step is effect select (different); the long-press path was unused so it's a clean place to put this gesture. SYNC_IN_GPIO itself is always-listening: each pulse posts to a FreeRTOS binary semaphore and wakes a sync-in task that calls `sequencer_tick()` while the sequencer is playing. The local esp_timer step clock is suppressed while sync IN is active, so the sequencer is fully driven by incoming pulses. The 20 ms debounce window rejects switch-bounce (a real PO-33 sends ~1 ms pulses at rates from ~60 BPM to ~240 BPM, so the minimum useful interval is ~62 ms; a 20 ms threshold catches bounces without rejecting fast pulses). When sync IN is **off**, incoming pulses are ignored (the listener task consumes them but doesn't tick); the sequencer runs on its local BPM clock.
- **Code location:** `main/system/sync.c` → `sync_in_isr()` (ISR, debounce + semaphore), `sync_in_task()` (FreeRTOS task, calls `sequencer_tick()` if `sequencer_is_playing()` and `sequencer_is_sync_in_active()`). `main/sequencer/sequencer.{c,h}` → `sequencer_tick()`, `sequencer_set_sync_in_active()`, `sequencer_is_sync_in_active()`, `sequencer_play()` (skips local timer when sync IN is active). `main/ui/input.c` → FX long-press detector in the end-of-drain polling block (2 s gate; latched per-hold). F-032 (5 sync modes) is NOT wired; this implementation treats every pulse identically (1 pulse = 1 step). The 5-mode PO-33 behaviour (different message protocols per mode) is v2.
- **Status:** ✅ done. Single-mode sync-in works (1 pulse = 1 step). The toggle gesture lets the user turn sync IN off when they don't want the local sequencer to be kicked by stray cable noise (a real-world problem when the sync cable is loose or the upstream device is off). 5-mode is not wired; hardware-gated (see F-032).

#### F-032 — Five sync modes (SY0–SY5)

- **Manual ref:** §10.1 (the table: SY0 stereo/stereo, SY1 stereo mono/sync, SY2 sync stereo, SY3 sync mono/sync, SY4 mono/sync stereo, SY5 mono/sync mono/sync)
- **Layman:** The PO-33 can send either stereo audio, sync pulse, or both — on either its line-in or its line-out. There are 5 useful combinations. The default is SY0 (stereo in, stereo out, no sync), which is what you want when listening to music through headphones.
- **PO-33 button combo:** Hold RECORD + BPM to cycle.
- **Our hardware combo:** **Not implementable on the v1 hardware.** The 5 modes route different signals (stereo audio, mono audio, sync pulse) into the 3.5 mm jack contacts; the v1 board has **separate jacks** for headphone out and line in (no analog mux), and the sync GPIO is on a dedicated pin (GPIO 25 sync in, GPIO 18 sync out). Without a hardware revision that adds an analog mux and re-routes the jack contacts, F-032 has no phase variation to operate on. F-031 (sync-in listener) is the software piece that would benefit from F-032 once the hardware exists; it's wired in this PR but doesn't expose a mode selector (since there's nothing to select).
- **Status:** ❌ missing (hardware-gated; deferred until a board revision adds the analog mux).

### 3.11 Section 11 — Clock and alarm

#### F-033 — Built-in clock (HH:MM shown on display)

- **Manual ref:** §11
- **Layman:** The PO-33 has a clock. The current time is shown in the upper-right corner of the display.
- **PO-33 button combo:** No specific button; the clock is always running.
- **Our hardware combo:** The clock shows on the TFT top bar (in `main/ui/display.c` → `display_tick()`).
- **Code location:** `main/system/clock.c` → `clock_init()`, `clock_get_hhmm()`, `clock_set_hhmm()`. The NVS stores the epoch offset so the clock survives a reboot.
- **Status:** ✅ done.

#### F-034 — Alarm that plays a sample

- **Manual ref:** §11
- **Layman:** You can set an alarm time. When the alarm time is reached, the device plays a sample.
- **PO-33 button combo:** Set via PO-33 menu (no single button combo in the manual).
- **Our hardware combo:** UART: `clock_set_alarm 7 30 3` (play slot 3 at 07:30).
- **Code location:** `main/system/clock.c` → `clock_set_alarm(hh, mm, slot)`; the alarm callback is not yet implemented (no FreeRTOS timer that checks the time).
- **Status:** ✅ done. `clock_init()` starts a 1-second `esp_timer` (`alarm_check_tick`) that compares the current HH:MM to the persisted `s_alarm_hh:mm`. On a match, it calls `amy_bridge_play_note(s_alarm_slot, 60, 100, PO33_FX_NONE, 0, 0, 0, 0)` — middle-C, default velocity, no FX. A once-per-day guard (`s_last_alarm_day`) prevents the sample from looping every second within the matching minute; re-arming the alarm resets the guard. Alarm state (HH/MM/slot) persists in NVS under the `"clock"` namespace. UART verbs: `clock_set_alarm <hh> <mm> <slot 0..15>` and `clock_clear_alarm` (no button gesture).

### 3.12 Section 12 — Battery

#### F-035 — Show battery level on display

- **Manual ref:** §12
- **Layman:** The PO-33 shows a small battery icon with bars indicating remaining charge. Press SOUND + BPM and the bars light up to indicate the level.
- **PO-33 button combo:** SOUND + BPM.
- **Our hardware combo:** Always-on battery indicator in the top status bar at x=180 (replaces the clock, which moves to the picker screen footer at y=308). The PO-33's SOUND + BPM gesture is not wired -- the always-on display achieves the same intent with no gesture to remember. Format: 24×11 px outline + 2×5 cap + 4 internal bars (3 px wide each, 1 px gap) + integer percentage to the right.
- **Code location:** `main/system/power_mgmt.{c,h}` → `power_mgmt_battery_get_percent()` reads `BATTERY_ADC_CHANNEL` (ADC1_CH3 / GPIO4), divides by `BATTERY_DIVIDER_RATIO` (2.0 from `config.h`), and maps the cell voltage onto a Li-ion discharge curve (piecewise-linear, 6 segments). Display hook: `main/ui/display.c` `render_main_screen()`.
- **Status:** ✅ done. The "ERR" label is shown instead of a misleading percent when the ADC read fails (returns 0xFF). Read is throttled to one log per 5 s to avoid spamming when the display refreshes at 30 Hz.

#### F-036 — Deep-sleep after 5 minutes idle

- **Manual ref:** §12 (implicit)
- **Layman:** After 5 minutes with no button pressed, the device goes into a very-low-power "sleep" mode. Pressing any button wakes it back up.
- **PO-33 button combo:** Implicit.
- **Our hardware combo:** Same. Any button press wakes from deep sleep via GPIO interrupt.
- **Code location:** `main/system/power_mgmt.c` → `power_mgmt_init()` starts a 5-minute esp_timer; `power_mgmt_enter_deep_sleep()` is called when it fires. GPIO columns are configured with `gpio_wakeup_enable()` for `GPIO_INTR_LOW_LEVEL`.
- **Status:** ✅ done.

### 3.13 Section 13 — Factory reset

#### F-037 — Factory reset (erase everything)

- **Manual ref:** §13 ("hold pattern + insert batteries")
- **Layman:** To wipe the device back to factory defaults, you power it off, hold PATTERN, and reinsert the batteries. Everything is erased.
- **PO-33 button combo:** Power off → hold PATTERN → insert batteries. (Our firmware has no "battery" identity; deep-sleep wake is the PO-33's button-insert equivalent.)
- **Our hardware combo:** **Hold REC + BPM for 5 seconds.** Long-press deliberately 5 seconds (vs. 600 ms for F-012) because factory reset is destructive and irreversible; the long hold prevents accidental trips. Latched the same way as F-012 (one fire per hold; release-and-re-press to fire again). Samples in PSRAM are NOT wiped -- the PO-33 doesn't wipe recordings on factory reset either; only saved patterns/chain/sketches on flash are.
- **UART:** `factory_reset` verb -- for tests and headless use.
- **Code location:** `main/storage/storage.{c,h}` → `storage_factory_reset()` (calls `esp_littlefs_format()` then `pattern_init_all()`). `main/ui/input.c` → REC+BPM gesture detector in the end-of-drain polling block (5 s gate). `main/main.c` → UART verb.
- **Status:** ✅ done. The user is expected to know the gesture (5 s hold); no on-screen warning is shown. If the picker UI is open, it will display stale active-state until the user exits and re-enters -- acceptable; the picker is not common during a reset.

#### F-038 — Show active sounds and patterns

- **Manual ref:** §13 ("active sounds/patterns — press sound (S) or pattern (⠛). lit numbers have a sound/pattern, unlit are silent, flashing is the currently selected sound/pattern")
- **Layman:** On the real PO-33 the slot buttons light up in patterns to show which slots are recorded, which are empty, and which is currently selected.
- **PO-33 button combo:** Press SOUND or PATTERN.
- **Our hardware combo:** We don't have lit buttons (the buttons on a stock 4×4 matrix keypad are not illuminated). The TFT does show the same information — which slots are filled, which is selected — on the Sound select UI page (proposed in §6).
- **Code location:** `main/ui/display.c` would render the per-slot filled/empty/selected icons.
- **Status:** ✅ done. The TFT now renders a per-slot row and a per-pattern row in the main screen (16 dots each, lit = white, unlit = dim grey, currently selected = red with a yellow border — `main/ui/display.c` `render_active_state_panel()`, called from `render_main_screen()` after the existing STEP label). Slot fill = `amy_bridge_slot_has_sample()`; pattern fill = scan `g_patterns[p].steps[s].slot_id != 0xFF`. Because the panel is always visible on the main screen (not toggled by SOUND/PATTERN taps, which have other bindings), no new button combos are needed.

---

## 4. The 16 punch-in effects, one per effect

Each effect below is presented with: a one-sentence layman explanation of what it does to the sound, the PO-33 manual's wording, what we currently map it to in code, and what audio quality you should expect.

> ⚠️ **Important honesty note.** Our current firmware has a slightly different list of effects than the real PO-33. We have `BITCRUSH` and `FILTER_SWEEP` which are *not* PO-33 effects, and we are missing `SCRATCH` and `6/8 QUANTIZE`. See §7 for the v2 alignment plan.

### 4.1 Effect 1 — loop 16

- **What it sounds like:** the sound plays its first 16th, then loops that 16th over and over for the duration of the step.
- **Manual:** "loop 16".
- **Our code:** `PO33_FX_LOOP_16`. `apply_fx()` sets `e.loopstart = slot_end - slice` and `e.loopend = slot_end`, where `slice = (slot_end - slot_start) / 16`. A custom AMY patch (in `components/amy/src/{amy.h,amy.c,pcm.c}`) propagates these per-event loop bounds into the voice at note-on time. See the AMY changes for the delta-queue plumbing.
- **Status:** ✅ done.

### 4.2 Effect 2 — loop 12

- **What it sounds like:** the same as loop 16, but the loop length is 12 sixteenth-notes (i.e. an eighth-note triplet).
- **Manual:** "loop 12".
- **Our code:** `PO33_FX_LOOP_12`. Same gap as 4.1.
- **Status:** ⚠️ partial.

### 4.3 Effect 3 — loop short

- **What it sounds like:** loop a shorter chunk (about a 16th-note).
- **Manual:** "loop short".
- **Our code:** `PO33_FX_LOOP_SHORT`. `apply_fx()` sets `e.loopstart = slot_end - slice` and `e.loopend = slot_end`, where `slice` is a fixed 4096 samples (~93 ms at 44.1 kHz) clamped to the slot range. Same AMY plumbing as 4.1.
- **Status:** ✅ done.

### 4.4 Effect 4 — loop shorter

- **What it sounds like:** loop an even shorter chunk.
- **Manual:** "loop shorter".
- **Our code:** `PO33_FX_LOOP_SHORTER`.
- **Status:** ⚠️ partial.

### 4.5 Effect 5 — unison

- **What it sounds like:** the same sound played three or four times at the same pitch but slightly out of phase, sounding thicker — like multiple singers singing the same note.
- **Manual:** "unison".
- **Our code:** `PO33_FX_UNISON`. In `apply_fx()`, no parameter changes — AMY's per-voice polyphony already handles multi-voice playback.
- **Status:** ⚠️ partial (the multiple voices are already there from `num_voices = 4`; explicit unison detune isn't applied).

### 4.6 Effect 6 — unison low

- **What it sounds like:** the unison effect, but the duplicated voices are an octave lower.
- **Manual:** "unison low".
- **Our code:** `PO33_FX_UNISON_LOW`. In `apply_fx()`, lowers `midi_note` by 12.
- **Status:** ✅ done.

### 4.7 Effect 7 — octave up

- **What it sounds like:** the sound plays one octave higher than its recorded pitch.
- **Manual:** "octave up".
- **Our code:** `PO33_FX_OCTAVE_UP`. In `apply_fx()`, `midi_note += 12`.
- **Status:** ✅ done.

### 4.8 Effect 8 — octave down

- **What it sounds like:** the sound plays one octave lower than its recorded pitch.
- **Manual:** "octave down".
- **Our code:** `PO33_FX_OCTAVE_DOWN`. In `apply_fx()`, `midi_note -= 12` (clamped at 0).
- **Status:** ✅ done.

### 4.9 Effect 9 — stutter 4

- **What it sounds like:** a tiny chunk of the sound is repeated 4 times per beat, creating a "st-st-st-stutter".
- **Manual:** "stutter 4".
- **Our code:** `PO33_FX_STUTTER_4`. `apply_fx()` sets `e.loopstart = slot_end - slice` and `e.loopend = slot_end`, where `slice = (slot_end - slot_start) / 32` (half the LOOP_16 size, to give a tighter stutter). Same AMY plumbing as 4.1.
- **Status:** ✅ done. The resulting loop is short enough that it reads as a 4x stutter rather than a sustained note.

### 4.10 Effect 10 — stutter 3

- **What it sounds like:** same as stutter 4, but 3 repeats per beat (a triplet feel).
- **Manual:** "stutter 3".
- **Our code:** `PO33_FX_STUTTER_3`. Same gap.
- **Status:** ⚠️ partial.

### 4.11 Effect 11 — scratch

- **What it sounds like:** the playhead jumps back and forth in the sample like a DJ spinning a record by hand.
- **Manual:** "scratch".
- **Our code:** `PO33_FX_SCRATCH`. **Missing from our enum — see §7.**
- **Status:** ❌ missing.

### 4.12 Effect 12 — scratch fast

- **What it sounds like:** same as scratch, but the jumps are faster.
- **Manual:** "scratch fast".
- **Our code:** `PO33_FX_SCRATCH_FAST` exists but the implementation is a no-op (we treat it like stutter).
- **Status:** ⚠️ partial.

### 4.13 Effect 13 — 6/8 quantize

- **What it sounds like:** the step timing is re-mapped from 4/4 (4 beats per bar) to 6/8 (6 beats per bar with compound subdivision). The pattern sounds different — like a waltz or a slow blues.
- **Manual:** "6 / 8 quantize".
- **Our code:** `PO33_FX_68_QUANTIZE`. **Missing from our enum — see §7.**
- **Status:** ❌ missing.

### 4.14 Effect 14 — retrigger pattern

- **What it sounds like:** every step that has a sound re-fires that sound multiple times within the step duration.
- **Manual:** "retrigger pattern".
- **Our code:** `PO33_FX_RETRIGGER_PATTERN`. Our interpretation: the FX resets the active pattern's step counter to 0 on the next step tick (effectively restarting the pattern from the top). `apply_fx()` calls `sequencer_request_retrigger()`, which sets a flag consumed by the sequencer's `on_step()` callback. The PO-33 might also re-fire sounds within the step -- our implementation handles the "restart pattern" half; the "re-fire sounds" half is interpreted as "and will get to this step again sooner" via the restart. Open question: how the real PO-33 handles the chain cursor on retrigger; we reset `s_step` to 0 but leave `g_chain_len` and the chain cursor intact.
- **Status:** ✅ done (partial interpretation; see open question above).

### 4.15 Effect 15 — reverse

- **What it sounds like:** the sound plays backwards.
- **Manual:** "reverse".
- **Our code:** `PO33_FX_REVERSE`. Currently a no-op.
- **Status:** ❌ missing.

### 4.16 Effect 16 — no effect

- **What it sounds like:** exactly as recorded. The "off" position.
- **Manual:** "no effect".
- **Our code:** `PO33_FX_NONE`. Implemented as a pass-through.
- **Status:** ✅ done.

### 4.17 Effects we have that the real PO-33 does NOT have

For full honesty, here are two effects in our `po33_fx_t` enum that are **not** part of the real PO-33:

- **`PO33_FX_BITCRUSH`** — heavy digital distortion (4-bit downsample). It's a fun effect, but the PO-33 does not have it.
- **`PO33_FX_FILTER_SWEEP`** — automated low-pass filter sweep. The PO-33 has a manual filter knob (the Filter tweak), not a sweep effect.

These are kept in v1 because they were useful while developing. The v2 plan (§7) drops them and adds the real missing PO-33 effects (`SCRATCH` and `6/8 QUANTIZE`).

---

## 5. Workflows: "How do I…?"

For each common task, here is the answer twice — once for the real PO-33, once for our firmware. If the two answers look the same, it means our firmware is fully there. If our answer says "UART shell", it means the button combo isn't wired and you'll have to use the serial port for now.

### 5.1 How do I record a sample?

**On a real PO-33:**
1. Hold the **REC** (star) button.
2. Press the slot number 1–16 where you want the recording to live.
3. Make the sound (clap, sing, play your synth into the mic).
4. Press **REC** again to stop.

**On our firmware:**
1. Hold the **REC** button.
2. Press a step button 1–16 to pick a slot.
3. Make the sound.
4. Press **REC** again to stop.

Or over the UART shell (115200 baud):
```
po33> rec 5
po33> stoprec
```

### 5.2 How do I make a beat?

**On a real PO-33:**
1. Record one or more samples first (see 5.1).
2. Press **WRITE** (·) to enter write mode.
3. Press a slot number (e.g. 1) — that slot is now "armed".
4. Press step numbers where you want slot 1 to play.
5. Press another slot number (e.g. 2), then press more step numbers.
6. Press **WRITE** again to exit.
7. Press **PLAY**.

**On our firmware:**
- No UI button combo yet. Use the UART shell:
```
po33> pattern 0
po33> bpm 120
po33> play
```
And to add a slot to a step programmatically:
```
po33> ...  # we don't have a shell command for this yet — see §7
```

### 5.3 How do I change the tempo?

**On a real PO-33:** Three distinct combos (per the [lode/PO-33](https://github.com/lode/PO-33) verbatim manual):
- Press **BPM** to cycle through 80 / 120 / 140 (the preset "levels").
- Hold **BPM** + turn knob A to set the swing (8 discrete levels).
- Hold **BPM** + turn knob B to fine-tune the tempo (continuous, 60–240).

**On our firmware:** All three behaviours wired as of this commit. Tap `BPM` (rightmost of the top row) cycles presets. Long-press + twist Knob A → swing. Long-press + twist Knob B → fine tempo. Or over the UART shell: `bpm 140`.

### 5.4 How do I switch to another pattern?

**On a real PO-33:** Hold **PATTERN** (⠛) + number 1–16.

**On our firmware:** **Hold `PATTERN` + step 1–16** to select (and append) a pattern; tap and long-press of `PATTERN` alone are no-ops. Or: `pattern 5` over the UART shell.

### 5.5 How do I apply an effect?

**On a real PO-33:** Hold **FX** + number 1–15 (the 15 punch-ins; the manual's effects table lists entry 16 as "no effect", not swing). The effect applies to the next sound you play. **Swing** is not a step-press at all — see §5.3 for the BPM + knob A combo.

**On our firmware:** Same combo: hold `FX` (`BTN_FX`, GPIO 12) + step 1–15 → sets `sequencer_set_active_fx(PO33_FX_LOOP_16 + (step - 1))`, which the next triggered note (sequencer or "step press without modifier after a SOUND select") consumes. Step 16 → `PO33_FX_NONE` (the manual's "no effect" entry). See F-019 for the full mapping. The selected effect is shown on the TFT (proposed in §6).

### 5.6 How do I save my work?

**On a real PO-33:** Patterns are saved automatically the moment you finish writing them. Sounds are saved automatically the moment you stop recording. Power-off and back on — everything is still there. (Backup to tape / another PO-33 is via the data transfer protocol described in 5.9.)

**On our firmware:** Patterns, samples, and the chain are saved
automatically before deep sleep (idle timer + `sleep` UART verb)
and restored on the next boot from the active sketch's folder
under `/sketches/<id>/`. The active sketch is remembered in NVS
key `sketches/active_id`; on a fresh device this defaults to
sketch `0000`. The `save` / `load` UART verbs and `storage_save_all()` /
`storage_load_all()` are still available as belt-and-braces
explicit saves (see §11.2 and ADR-0004).

### 5.7 How do I trim a recording?

**On a real PO-33:** Enter Tweak = Trim (press FX until the screen says Trim). Turn Knob A to move the start point; turn Knob B to change the length.

**On our firmware:** Not yet via UI. UART shell would call `amy_bridge_set_trim(slot, start, end)`.

### 5.8 How do I sync with another device?

**On a real PO-33:** Connect a 3.5 mm cable from line-out → line-in. On the master, hold **RECORD + BPM** to pick a sync mode (SY0–SY5). Press **PLAY** on the master, then **PLAY** on the slave. Both devices will tick at the same tempo.

**On our firmware:** Sync OUT works (the device pulses GPIO 18 every step). Sync IN is partially wired but not fully implemented; see F-031.

### 5.9 How do I back up everything?

**On a real PO-33:** Connect line-out to a tape recorder / phone / computer. Hold **WRITE + SOUND + PLAY** on the PO-33 to dump its data as audio. The receiver records the audio.

**On our firmware:** Not yet implemented (see F-028, F-029).

### 5.10 How do I use the alarm?

**On a real PO-33:** Set the current time, then set the alarm time and pick which sound to play. At the alarm time, the sound plays once.

**On our firmware:** The clock works (F-033). Setting an alarm via shell: `clock_set_alarm 7 30 3` (play slot 3 at 07:30). The alarm-firing timer is not yet implemented (F-034).

### 5.11 How do I send a sketch to another device face-to-face?

**On a real PO-33:** Not possible. The original §9 transfer requires a 3.5 mm audio cable and erases the receiving unit (see F-028).

**On our firmware:** Two devices, side by side, both running v2 firmware with the §15 ESP-NOW feature, no internet, no router, no phone, no audio cable. The flow is:

1. **On the sender:** Hold **WRITE + SOUND** for 2 seconds. The TFT opens the **Transfer** menu. Turn Knob A to **Send sketch**; click the encoder. The sender shows **"Waiting for receiver — code 482 913"** (a 6-digit code, 30-second timeout, regenerates every 30 s if no peer appears).
2. **On the receiver:** Hold **WRITE + SOUND** for 2 seconds. The TFT opens the **Transfer** menu. Turn Knob A to **Receive sketch**; click the encoder. The receiver shows a code-entry screen; type the 6-digit code from the sender's screen using the step buttons as a 0–9 keypad. (The 4×4 matrix gives 16 keys; the layout maps columns to 4-3-2-1 left-to-right and rows to A-B-C-D top-to-bottom, with the bottom row doubling as 0 / backspace / submit.) Click the encoder to confirm.
3. **Both devices** show a progress bar (0–100 %) and the elapsed time. Typical sketches (16 patterns, ~5 s of samples, 1 chain) transfer in well under 5 seconds at the default 1 Mbit PHY; the slowest case is a sketch with a full ~40 s sample pool, which can take 30–60 s.
4. **On success:** the receiver shows **"Received sketch <id> '<name>' — 1.2 MB in 4.3 s"** and prompts **Load now / Save & exit**. The new sketch appears in the receiver's sketch picker as a new folder under `/sketches/<id>/`; `sketches.lst` is updated atomically.
5. **On failure:** the receiver shows a one-line reason (**"Peer timed out"**, **"Wrong code"**, **"Sketch too large"**, **"Flash full"**, **"Cancelled by sender"**) and the partial transfer is rolled back (no half-written sketch folder; the active sketch is unchanged).

The transfer is **encrypted** (AES-128 with a key derived from the 6-digit code via PBKDF2-HMAC-SHA256, 4096 iterations, salt = both devices' MAC addresses concatenated), and **non-destructive** (the receiver's existing sketches are never touched; the sender's active sketch is never touched). The original sketch on the sender can be re-sent to as many receivers as you like.

See §15 for the full design (packet format, state machine, retry policy, threat model, deferred items). [ADDED: §5.11 documents the user-facing flow for §15's ESP-NOW sketch exchange.]

---

## 6. Better UI: a richer TFT

The 2.4″ screen on our device is 240 × 320 pixels — a generous canvas. Our current `display_tick()` shows a step grid plus a clock and pattern number, and that's about 30 % of the pixels used. This section proposes eight dedicated UI screens that would use the rest.

Each screen is described with: when it shows, what is in the top bar, what is in the middle, what is at the bottom, and a concrete ASCII mockup. These are proposals — they are **not yet implemented** in the firmware; they are listed here as the v2 UI roadmap.

### 6.1 Screen 1 — Idle

This is the default screen. Shows while the device is on but no special mode is active.

```
+-----------------------------------+
| PAT 3   120 BPM   14:32   [|||||] |  ← top bar: pattern, BPM, clock, battery
|                                   |
|                                   |
|   ▁ ▃ ▅ ▇  playhead is at step 4 |
|   . . . . . . . . . . . . . . . . |
|   1 2 3 4 5 6 7 8 9 ...        16 |
|                                   |
|   FX: octave up                   |  ← active FX badge
+-----------------------------------+
```

Implementation: top bar is one `fill_rect(0,0,240,18)` in blue; playhead is one `fill_rect` per step; FX badge is `draw_text` in the lower 16 px. Total ~30 lines of code in `main/ui/display.c`.

### 6.2 Screen 2 — Sound select

Triggered by the SOUND button. Shows all 16 slots as small icons, filled if recorded, hollow if empty, current slot highlighted.

```
+-----------------------------------+
| SOUND  [●][●][●][●][●][●][●][●]  |  ← slots 1-8: drum
|        [○][○][●][●][●][●][●][●]  |  ← slots 9-16: melodic
|                                   |
|        Slot 3: 1.2 s, 44100 Hz   |  ← info on the highlighted slot
|        Trim: ▕████████░░░░░░░░    |  ← trim bar
|        FX: scratch                |
|                                   |
|  step +/- to choose, func to exit |
+-----------------------------------+
```

Implementation: 16 small `fill_rect`s for the slot icons; one `draw_text` line for the slot info; one thin horizontal bar for trim. ~50 lines.

### 6.3 Screen 3 — Recording

Triggered by REC button. Shows the recording level meter, elapsed time, slot target.

```
+-----------------------------------+
| REC ● slot 3                      |
|                                   |
|  ┌──────────────────────────┐     |
|  │ ░░░░▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░ │     |  ← live VU meter
|  └──────────────────────────┘     |
|                                   |
|  elapsed: 1.4 s  /  remaining 1.6 |  ← time progress
|                                   |
|  press REC again to stop          |
+-----------------------------------+
```

Implementation: red `fill_rect` background; green VU meter drawn frame-by-frame using `amy_get_input_buffer()`; numeric counter. ~40 lines.

### 6.4 Screen 4 — Pattern edit (write mode)

Triggered by tapping the WRITE button (`BTN_WRITE`, GPIO 43).

```
+-----------------------------------+
| WRITE pattern 3                   |
|                                   |
|  slot: 5 (kick)                   |  ← currently armed slot
|                                   |
|  [1][2][3][4][5][6][7][8]         |
|  [o][o][o][o][●][o][o][o]         |  ← 16 steps, ● = filled with slot 5
|  [9][10][11][12][13][14][15][16]  |
|  [o][o][●][o][●][o][o][●]         |
|                                   |
|  press step to toggle, slot+/-    |
+-----------------------------------+
```

Implementation: 16 small `fill_rect`s for the steps (filled or hollow); a label row showing the armed slot; navigation hint. ~60 lines.

### 6.5 Screen 5 — Effect picker

Triggered by FX button. Shows the 16 effects in a 4 × 4 grid, the current one highlighted, and a one-line description of what it does.

```
+-----------------------------------+
| FX                                |
|                                   |
|  [01][02][03][04]                 |
|  loop  loop  loop  loop           |
|  16    12    short shorter        |
|                                   |
|  [05][06][07][08]                 |
|  uni  uni- oct+ oct-              |
|                                   |
|  [09][10][11][12]                 |
|  stut4 stut3 scra  scra+          |
|                                   |
|  [13][14][15][16]                 |
|  6/8  retr reverse no fx          |
|                                   |
+-----------------------------------+
```

Implementation: 16 tiles, each a small `fill_rect` with the index number and short label. ~80 lines.

### 6.6 Screen 6 — Tweak mode

Triggered by SOUND + FX. Shows the currently selected tweak parameter (Tone / Filter / Trim), the two knob values, and a mini visualization of what those knobs do.

```
+-----------------------------------+
| TWEAK: TONE                       |
|                                   |
|   knob A: pitch                   |
|   ▁▁▂▂▃▃▄▄▅▅▆▆▇▇██  +5 semitones |
|                                   |
|   knob B: volume                  |
|   ▁▁▁▁▁▂▂▂▂▃▃▃▃▄▄▄▄▄▄  -10 dB    |
|                                   |
|   press FX to switch to FILTER    |
+-----------------------------------+
```

Implementation: parameter name on top; two value-bar visualizations; current parameter indicator. ~50 lines.

### 6.7 Screen 7 — Sync

Triggered by SOUND + BPM. Shows the current sync mode (SY0–SY5) and whether this device is master or slave.

```
+-----------------------------------+
| SYNC  SY1   master                |
|                                   |
|  out: stereo + sync               |
|  in : mono                        |
|                                   |
|  play ▶ to start                  |
|                                   |
+-----------------------------------+
```

Implementation: ~20 lines.

### 6.8 Screen 8 — Alarm

Triggered by SOUND + clock-area touch. Shows the current time, the alarm time, the alarm slot, on/off.

```
+-----------------------------------+
| ALARM  14:32  ON                  |
|                                   |
|  now:    14:32                    |
|  alarm:  07:30   slot 3           |
|                                   |
|  + - to change hour, hold for mm  |
+-----------------------------------+
```

Implementation: ~30 lines.

### 6.9 Total TFT-UI cost

Eight screens, averaging ~50 lines of C each = ~400 lines of new code in `main/ui/display.c` plus a screen-state enum in the buttons task to decide which screen to show. Estimated work: 1 day for an experienced embedded engineer.

The payoff: a user looking at the device can tell at a glance which sound is loaded, what step they're on, what effect is active, whether the alarm is set, and what sync mode they're in. The current screen is so minimal that it requires opening the UART shell to know any of this.

---

## 7. What's done, partial, missing — the honest scorecard

### 7.1 Feature scorecard by section

| Section | Total features | ✅ Done | ⚠️ Partial | ❌ Missing |
|---|---|---|---|---|
| 1. Sounds (record / mic / line-in) | 5 | 3 | 1 | 1 |
| 2. Patterns (write mode) | 7 | 4 | 2 | 1 |
| 3. Songs (chain) | 2 | 2 | 0 | 0 |
| 4. Tweaking (tone / filter / trim) | 4 | 1 | 2 | 1 |
| 5. Effects (16 punch-ins) | 16 | 7 | 6 | 3 |
| 6. BPM / tempo | 2 | 1 | 0 | 1 |
| 7. Volume | 1 | 1 | 0 | 0 |
| 8. Copy + delete | 5 | 4 | 1 | 0 |
| 9. Data transfer | 3 | 0 | 1 | 2 |
| 10. Sync | 3 | 2 | 0 | 1 |
| 11. Clock + alarm | 2 | 2 | 0 | 0 |
| 12. Battery | 2 | 2 | 0 | 0 |
| 13. Factory reset + UI | 2 | 2 | 0 | 0 |
| **Total** | **~51** | **30 (59%)** | **14 (27%)** | **7 (14%)** |

### 7.2 The effect-list mismatch (v2 plan)

Our current `po33_fx_t` enum in `main/audio/amy_bridge.h` is:

```c
typedef enum {
    PO33_FX_NONE = 0,
    PO33_FX_LOOP_16, PO33_FX_LOOP_12, PO33_FX_LOOP_SHORT, PO33_FX_LOOP_SHORTER,
    PO33_FX_UNISON, PO33_FX_UNISON_LOW,
    PO33_FX_OCTAVE_UP, PO33_FX_OCTAVE_DOWN,
    PO33_FX_STUTTER_4, PO33_FX_STUTTER_3,
    PO33_FX_SCRATCH_FAST,
    PO33_FX_REVERSE,
    PO33_FX_RETRIGGER_PATTERN,
    PO33_FX_68_QUANTIZE,
    PO33_FX_FILTER_SWEEP,    /* not a PO-33 effect */
    PO33_FX_BITCRUSH,        /* not a PO-33 effect */
    PO33_FX_COUNT
} po33_fx_t;
```

The real PO-33 list (per the manual and the `lode/PO-33` transcription) is exactly 16 effects:

```
1.  loop 16
2.  loop 12
3.  loop short
4.  loop shorter
5.  unison
6.  unison low
7.  octave up
8.  octave down
9.  stutter 4
10. stutter 3
11. scratch
12. scratch fast
13. 6/8 quantize
14. retrigger pattern
15. reverse
16. no effect (off)
```

We are **missing** `scratch` (which we conflated with `scratch_fast`) and `6/8 quantize` (which we have but as a no-op). We are **carrying** two extras (`filter_sweep` and `bitcrush`) that aren't real PO-33 effects.

The proposed v2 alignment:

```c
typedef enum {
    PO33_FX_NONE              = 0,
    PO33_FX_LOOP_16           = 1,
    PO33_FX_LOOP_12           = 2,
    PO33_FX_LOOP_SHORT        = 3,
    PO33_FX_LOOP_SHORTER      = 4,
    PO33_FX_UNISON            = 5,
    PO33_FX_UNISON_LOW        = 6,
    PO33_FX_OCTAVE_UP         = 7,
    PO33_FX_OCTAVE_DOWN       = 8,
    PO33_FX_STUTTER_4         = 9,
    PO33_FX_STUTTER_3         = 10,
    PO33_FX_SCRATCH           = 11,
    PO33_FX_SCRATCH_FAST      = 12,
    PO33_FX_68_QUANTIZE       = 13,
    PO33_FX_RETRIGGER_PATTERN = 14,
    PO33_FX_REVERSE           = 15,
} po33_fx_t;
```

This v2 enum is presented here as a **proposal**, not as a change to be merged. Changing the enum values would break any patterns already saved in flash (they reference effect IDs by number). The migration story would be: save the patterns in a v2-compatible format, OR add a mapping table that translates old IDs to new IDs at load time. Either is a half-day of work.

### 7.3 Top-three missing features that would unlock the most value

If you only have time to ship three more features, do these:

1. **Live recording into a playing pattern** (F-003). Without this, you cannot "jam" a new sound into an existing beat — a core PO-33 workflow.
2. **The 16 punch-in effects actually working** (F-019 through F-031 in §3.5). Selection works (FX + step 1–15 → `sequencer_set_active_fx`); many `apply_fx()` cases are still no-ops (LOOP_16, LOOP_12, STUTTER_*, SCRATCH_FAST, REVERSE, RETRIGGER_PATTERN, 68_QUANTIZE). AMY already supports most of the DSP; the remaining work is wiring `apply_fx()` correctly per case. Also pending: a v2-aligned `po33_fx_t` enum (see §7 below).
3. **Effect selection via FX button + step button** (UI binding for F-019). Wired: FX + step 1–15 sets `sequencer_set_active_fx(...)`, which `on_step()` consumes as the fallback for triggered notes. The "save effect in pattern" half of the PO-33 combo still requires write mode (F-009), which is queued.

Each of these is roughly half a day of work for an experienced developer.

---

## 8. Cheat sheet — buttons at a glance

A printable one-page reference. **P** = press, **H+P** = hold while pressing, **L+P** = long-press. This is a *summary*; `docs/CONTROL_REFERENCE.md` is authoritative and labels every gesture ✅/⚠️/➕/❌.

| Action | Real PO-33 | Our firmware |
|---|---|---|
| Record into slot 5 | H+REC, then 5 | same |
| Stop recording | REC | same, or `stoprec` |
| Play pattern | PLAY | same |
| Stop pattern | PLAY | same |
| Change pattern | H+PATTERN + number | H+`PATTERN` + step 1–16, or `pattern N` |
| Change BPM (preset) | press BPM (cycle) | `BPM` tap cycles preset (Hip Hop → Disco → Techno → wrap), or `bpm N` |
| Apply effect | H+FX + number (1–15) | H+`FX` + step 1–15 → active FX (carried into next note); step 16 = "no effect" (PO33_FX_NONE). NOT a step-press: swing is BPM + Knob A. |
| Enter / exit write mode | press WRITE (·) | `WRITE` tap toggles write mode (yellow `WRITE` label in the status bar); `WRITE` long-press enters the sketch picker (➕). In write mode a pad press binds/clears the active slot on that step (tap = toggle, long-press = clear). |
| Select a sample slot | H+SOUND + number | H+`SOUND` + step 1–16 → active slot; press same step with no modifier = plays once |
| Save pattern | auto on power-off + manual `save` | `save` over UART (also `storage_save_all()` before deep sleep) |
| Load on boot | auto (last-active sketch from NVS `sketches/active_id`) | `load` over UART |
| Erase sound | H+REC + slot number | not yet |
| Erase pattern | H+REC + PATTERN | H+`REC` + `PATTERN` held 600 ms clears the active pattern |
| Copy sound | H+WRITE + SOUND + number | not yet |
| Copy pattern | H+WRITE + PATTERN + number | not yet |
| Tweak tone/filter/trim | FX tap cycles; knobs A/B adjust | FX tap cycles Tone → Filter → Trim → Tone; knobs bind to per-step data (Tone/Filter) or slot trim (Trim) |
| Pick slot to bind | H+SOUND + step 1-16 | `SOUND + step N` selects slot N-1 as the active slot for the next write-mode step press. |
| Change swing | H+BPM + knob A | H+BPM + Knob A → 8 discrete levels (0=no swing, 7=max); release keeps the level |
| Fine BPM | H+BPM + knob B | H+BPM + Knob B → continuous 60–240 BPM; release keeps the BPM |
| Change volume | H+BPM + number (1-5) | H+BPM + step 1-5 → volume level 1..5 (5=max); multiplier applied per-note in amy_bridge_play_note() |
| Sync out | always on | always on |
| Sync in | (no PO-33 button gesture) | ➕ hold `FX` 2 s toggles sync IN listening |
| Show battery | SOUND + BPM | not yet (TFT will show) |

---

## 9. FAQ

### Q: Can I use this firmware without buying the audio chip and the screen?

**A:** No. The firmware assumes the I²S DAC and mic are present, and uses the SPI screen for status. Without those, boot will fail at the I²S init step (the device will reboot-loop). You could compile a "headless" build that skips the audio and screen, but that is not part of this firmware.

### Q: Will my saved samples survive a reboot?

**A:** Yes, if you ran `save` over the UART shell before the reboot. Patterns are also saved. The samples live in LittleFS on the 16 MB flash, not in PSRAM — PSRAM is wiped on every boot. Without `save`, your recordings are gone after a power cycle.

### Q: Does it work with a real PO-33?

**A:** Partial. The 3.5 mm sync protocol is not yet implemented, so two ESP32-S3 PO-33 remake boards can sync to each other, but the remake cannot sync to an **original Teenage Engineering PO-33**. The remake and the original PO-33 share the same audio sample rates (44.1 kHz), so once we implement data transfer (F-028) the two should be able to exchange sounds and patterns.  [UPDATED: clarified "our boards" → "ESP32-S3 PO-33 remake boards" and "real PO-33" → "original Teenage Engineering PO-33" for terminology consistency with §14.]

### Q: Can I plug headphones in?

**A:** Only if you wire the headphone amp chip (MAX98357A) to the I²S lines. The PCM5102A DAC we use for line-out does not drive headphones directly. The "headphone volume" levels (5 levels) on the real PO-33 would then apply to the MAX98357A's volume register.

### Q: How loud is the speaker?

**A:** Whatever the MAX98357A is rated for — typically 3 W into a 4 Ω speaker. The firmware does not set the volume, it just outputs full-scale audio; the amp's volume is hardware-controlled by an external potentiometer or a GPIO pin we'd have to add to the firmware.

### Q: Can I make a song with more than 128 patterns?

**A:** No. The real PO-33 also caps at 128. If you want a longer song you have to plan for the loop-back.

### Q: Does the alarm work?

**A:** You can set the alarm time and which slot to play, but the firmware does not yet have a timer that checks "is it alarm time?". Setting the alarm just stores the values in NVS. The actual fire-and-play is a planned v2 feature (F-034).

### Q: Why does the device sometimes lock up when I press a button?

**A:** It shouldn't. If it does, please file a bug report with the exact button combo. There are known edge cases in the button de-bouncer for very rapid presses; the firmware ignores presses shorter than 30 ms to filter switch bounce. If you find a press that consistently crashes the device, that's a bug.

### Q: Can I use this firmware with a battery?

**A:** Yes. The board has a LiPo charging circuit if you wire one up. In deep sleep the firmware draws ~10 µA, so a 1500 mAh LiPo would last many months. Active power is 60–120 mA depending on polyphony. The battery voltage display is a planned v2 feature (F-035).

### Q: How is this different from running AMY on a Mac?

**A:** AMY on a Mac uses your laptop's audio interface and your laptop's keyboard / mouse as input. Our firmware uses a $5 chip, a $3 DAC, a $5 screen, and 16 buttons. The difference is **portability** and **physical tactility** — a PO-33 is something you hold in your hand, not something you sit in front of.

---

## 10. Beyond the PO-33: breaking the limits with the ESP32-S3

### 10.0 Why this section exists

The PO-33 is a small, beautiful object with very specific limits: 40 seconds of sampling, 16 slots, 16 steps, 16 patterns, 16 effects, 5 sync modes, no WiFi, no Bluetooth, no MIDI, no USB. None of these limits are bugs. None of them are accidents. They are the **consequences of the hardware TE chose**: a particular microcontroller, a particular amount of memory, a particular audio codec. The constraint-as-feature is part of why the device is loved: 16 slots force you to be ruthless about which sounds matter; 16 steps force a specific kind of rhythm; the absence of MIDI keeps you playing the PO-33 like an instrument, not a sequencer.

But the limits are still limits. And the chip in this project — the **ESP32-S3** — is a fundamentally more powerful platform: a dual-core 240 MHz processor with vector instructions for DSP, 8 MB of Octal PSRAM, WiFi, Bluetooth, USB-OTG, dual I²S, and an entire free audio library called AMY sitting on top. So almost every PO-33 limit becomes a knob we can turn up, if we want to.

This section does three things:

1. **Lists the PO-33 limits explicitly** (what the original device actually caps at).
2. **Lists what the ESP32-S3 + AMY can do** that the PO-33 cannot.
3. **Proposes a v3 wishlist** ranked by impact.

We will also be honest about constraints that even the ESP32-S3 cannot break — every platform has ceilings — and about the PO-33 limits we should *keep on purpose*, because constraint is also a creative tool.

> ⚠️ **Honesty note.** This section describes what the hardware *can* do, not what our v1 firmware *has* implemented. Every "could be" is grounded in a real chip capability or a documented AMY API. "Currently does" is flagged in the wishlist column.

### 10.1 What the PO-33 cannot do

Drawn from the official Teenage Engineering manual ([teenage.engineering/guides/po-33/en](https://teenage.engineering/guides/po-33/en)) and confirmed against the `lode/PO-33` transcription.

| # | Limit | Value | Source |
|---|---|---|---|
| L1 | Total sample memory | 40 seconds across all 16 slots | Manual §1 |
| L2 | Per-slot duration | uneven; the manual does not state a per-slot max — only a total | Manual §1 |
| L3 | Number of sample slots | 16 total (our firmware: slots 1–8 drum, slots 9–16 melodic — reversed vs the manual; see ADR-0002) | Manual §1 |
| L4 | Steps per pattern | 16 | Manual §2 |
| L5 | Patterns | 16 | Manual §2 |
| L6 | Song length | up to 128 patterns chained | Manual §3 |
| L7 | Tweak parameters | 3 (Tone, Filter, Trim) | Manual §4 |
| L8 | Punch-in effects | 16 (fixed) | Manual §5 |
| L9 | BPM levels | 3 presets (Hip Hop 80, Disco 120, Techno 140) + fine-tune | Manual §6 |
| L10 | Headphone volume levels | 5 | Manual §7 |
| L11 | Sync protocols | 5 modes (SY0–SY5) over 3.5 mm cable; ≤5 Vpp | Manual §10 |
| L12 | Data transfer | 3.5 mm audio cable only; no WiFi, no Bluetooth, no USB, no MIDI | Manual §9 |
| L13 | Storage | non-volatile internal only; no user-expandable storage | Manual §8 |
| L14 | Recording sources | built-in microphone + 3.5 mm line-in | Manual §1 |
| L15 | Polyphony | not stated explicitly; ~4 voices typical for the PO line | inferred |
| L16 | Display | fixed LCD; no waveform display, low resolution | TE product page |
| L17 | Power | 2 × AAA batteries; ~1 month standby | TE product page |
| L18 | Audio quality | mono; sample rate implied at ~22 kHz | inferred from manual |
| L19 | Effects parameter locks | 2 per step | Manual §4 |
| L20 | Connectivity (none) | no WiFi, no Bluetooth, no USB, no MIDI | Manual (absence) |

Some of these limits are tightly coupled to the hardware Teenage Engineering chose. The 40-second total, for instance, comes from the microcontroller inside the PO-33, which has a fixed amount of RAM. Other limits (3 BPM levels, 5 sync modes, 16 fixed effects) are arguably arbitrary and could be loosened in a hypothetical PO-33 v2 — but TE chose the current numbers for usability reasons.

### 10.2 What the ESP32-S3 can do that the PO-33 cannot

| # | Capability | Why we can exceed the PO-33 | Concrete ceiling |
|---|---|---|---|
| C1 | **Sample memory** | The ESP32-S3-WROOM-1-N16R8 module has 8 MB of Octal PSRAM. The PO-33 uses roughly 1.6 MB (40 s × ~22 kHz × 16-bit ≈ 1.76 MB). We use **44.1 kHz** to match AMY's native render rate; that gives us a live pool of **~50–75 s** mono in PSRAM after the TFT framebuffer and AMY state (see §11.3 for the full breakdown). | Per-sketch on flash: ~75 s typical per sketch in our 8 MB `sketches` partition (~5 typical sketches). With streaming-to-flash on the 16 MB flash chip, you could push the live pool further. |
| C2 | **Sample rate / quality** | AMY supports up to 48 kHz; the ESP32-S3 I²S peripheral supports higher. We chose 44.1 kHz to match AMY's native rate, but we *could* sample at 48 kHz. | At 44.1 kHz mono: ~5.3 MB/min; at 48 kHz mono: ~5.8 MB/min; at 96 kHz mono: ~11.5 MB/min. Anything above 48 kHz exceeds our PSRAM pool in practice. |
| C3 | **Per-slot duration** | Because we have PSRAM, each slot's length is bounded only by the total pool. **But at 44.1 kHz, 1 s of mono = 86 KB**, so a long melodic slot costs a lot. | Per-slot ceiling: up to ~30 s for a melodic slot in principle, but the *combined* pool is bounded by ~6 MB of PSRAM (after framebuffer + AMY state) = roughly **50–75 s of total sample time** (see §11.8 for the full breakdown). Practically, with a typical 8-slot sketch, that means **average ~6–9 s per slot** in moderate DSP load. |
| C4 | **Polyphony** | AMY's `amy_config_t.max_voices` controls how many oscillator voices are available. Each voice costs a small CPU slice per render block. The ESP32-S3 has dual LX7 cores and vector DSP instructions, so we can run more voices in parallel than a single-core chip. | 8–16 simultaneous voices is comfortable. 30 is achievable with optimization. |
| C5 | **Step count per pattern** | Each `step_t` is currently 8 bytes. 256 steps × 16 patterns × 128 chain = 16 KB. Trivial in PSRAM. We could support variable time signatures: 16 steps of 1/16, 32 of 1/32, etc. | 32 or 64 steps per pattern (variable time signatures — e.g. a 5/4 or 7/8 pattern). |
| C6 | **Pattern count** | 16 patterns × ~3 KB each = 48 KB. PSRAM has megabytes to spare. | 64–256 patterns. |
| C7 | **Song length** | Pattern chain is currently 128 bytes. | 512–2048 patterns chained. |
| C8 | **Storage** | 16 MB flash + LittleFS. The `sketches` partition is **8 MB**; the rest is `factory` (3 MB) + `nvs` + `phy_init` (tiny). We can persist samples, patterns, presets, and user-named sketches. | 16 patterns × 3 KB + 16 samples × ~220 KB = ~3.5 MB used per typical sketch; ~4.5 MB free in `sketches` after 5 typical sketches. |
| C9 | **Sync protocols** | The ESP32-S3 has WiFi 802.11 b/g/n and Bluetooth 5. We can add WiFi-based sync (NTP clock, Ableton Link, OSC), BLE MIDI, USB MIDI, USB audio class. | Ableton Link over WiFi; BLE MIDI; USB-MIDI class compliant. |
| C10 | **MIDI** | The chip supports USB-OTG and BLE. USB-MIDI device class costs ~1 KB of code with the ESP-IDF TinyUSB stack. | Full MIDI in/out (DIN-5 with a $1 optocoupler, USB-MIDI, or BLE MIDI). |
| C11 | **Effects** | AMY already gives us chorus, echo, reverb, distortion, filters. Multiple AMY effects can be chained per voice. | Chainable multi-effect per voice (e.g. filter + reverb + chorus on the same note). |
| C12 | **Sampling rate conversion** | The ESP32-S3 vector instructions accelerate SRC. We could record at one rate and play at another (e.g. record 48 kHz, slow down to 22 kHz for lo-fi playback). | Record at any rate; play at any rate; crossfade between. |
| C13 | **Networking** | WiFi + BLE. | Sync with phones; web preset library; OTA firmware updates. |
| C14 | **Display** | We chose a 240×320 TFT — already 5× the pixels of the PO-33's LCD. AMY exposes an audio-analysis API (FFT) we can pipe to the display. | Waveform display; spectrum analyzer; level meters; full editor UI on the screen. |
| C15 | **Sample management** | PSRAM lets us keep every sample loaded simultaneously. No file system round-trip on sample switch. | Zero-latency sample swap; no need to "load from card". |
| C16 | **Recording** | Two independent I²S channels — one for the built-in mic, one for line-in. | Dual-source recording; resampling on capture; pitch detection from the input. |
| C17 | **Tap tempo / auto-quantize** | Microphone input already exists. | Listen to the room (or another instrument) and quantize the player's own timing. |

### 10.3 How we can break each PO-33 limit

Mapping each row of §10.1 to a concrete "how we exceed it" answer.

| PO-33 limit | Currently | Possible with our hardware | Required work |
|---|---|---|---|
| L1 — 40 s total | 40 s × 44.1 kHz mono = 3.37 MB pool (we already match PO-33's total) | ~5 minutes mono, or ~20 min with flash streaming | Re-partition PSRAM pool; add LittleFS streaming for the surplus |
| L2 — per-slot max | 2 s drums, 3 s melodic (configurable) | up to 30 s melodic | Change the slot-cap math in `config.h` |
| L3 — 16 slots | 16 slots | 32, 64, or 256 slots | Bigger pool + bigger UI selector |
| L4 — 16 steps | 16 steps | 32 or 64 steps | Larger `step_t` array + bigger step grid on TFT |
| L5 — 16 patterns | 16 | 64 or 256 | More `pattern_t` storage |
| L6 — 128-song chain | 128 | 512–2048 | Larger `g_chain[]` |
| L7 — 3 tweak params | not bound to UI | unlimited (already supported in `step_t`) | New "Tweak" UI screen (§6.6) |
| L8 — 16 effects | 16 | unlimited (AMY supports many) | Per-event effect chains |
| L9 — 3 BPM levels | 1 BPM level (continuous 60–240) | unlimited | Add "level cycling" mode |
| L10 — 5 volume levels | master gain fixed at 100% | 1–100 dB in 1 dB steps | UI binding for volume keys |
| L11 — 5 sync modes | 5 modes (not implemented) | unlimited + WiFi Link | New sync task; possibly Ableton Link port |
| L12 — no wireless | none | WiFi sync, BLE MIDI, USB-MIDI | Significant: BLE stack + Link port |
| L13 — 256 KB flash for samples | 256 KB on chip (we don't use it for samples) | 8 MB `sketches` partition on chip | Already in `partitions.csv` (renamed to use `sketches` at 8 MB) |
| L14 — mic + line only | mic only | mic + line-in simultaneously | Second I²S RX channel + a 3.5 mm jack |
| L15 — ~4 voices | 4 voices | 8–30 voices | Tweak `amy_config_t.max_voices` |
| L16 — fixed LCD | 240×320 TFT | 240×320 or larger TFT, OLED, or e-ink | Already done; future versions: add a second screen |
| L17 — 2× AAA | USB-C 5 V or LiPo | same + solar if we add a charging chip | already wired in `config.h` |
| L18 — 22 kHz mono | 44.1 kHz mono (we already exceed) | 48 kHz or 96 kHz | AMY supports up to 48 kHz natively |
| L19 — 2 plocks/step | 2 plocks | unlimited (AMY has many params) | Larger `step_t` struct |
| L20 — no I/O beyond 3.5 mm | UART shell + USB-C | USB-MIDI, USB-audio class, BLE MIDI, WiFi web UI | Component additions (TinyUSB, Link) |

### 10.4 Constraints the ESP32-S3 cannot break

Not every limit is unbounded. Some are fundamental to the physics and economics of the platform. We list them honestly so the wishlist in §10.6 is grounded.

| # | Limit | Why it can't be broken |
|---|---|---|
| X1 | **Live mic latency floor** | The I²S DMA must buffer at least one block. With our 256-frame blocks at 44.1 kHz, the round-trip latency is ~5.8 ms (record → playback). The PO-33 is similar. You cannot make "feel like zero" smaller than this without changing the chip or the audio protocol. |
| X2 | **Polyphony ceiling** | Each voice costs CPU cycles per render block. With 30 voices, the ESP32-S3 hits its throughput limit and audio glitches. The AMY overload failsafe would kick in. |
| X3 | **WiFi range and battery life with WiFi on** | WiFi is ~200 mA when active. With a 1500 mAh LiPo you get ~6 hours, not months. Range is ~50 m line-of-sight. |
| X4 | **Maximum I²S sample rate** | The peripheral tops out at a few MHz. 96 kHz is fine; 192 kHz is fragile. |
| X5 | **Speaker volume** | Software cannot fix a small speaker. Hardware (a bigger amp chip, a bigger speaker) is the only lever. |
| X6 | **Audio-to-MIDI** | Real-time polyphonic pitch detection from audio requires a DSP / ML model that doesn't fit in 8 MB PSRAM. Off-the-shelf solutions exist (e.g. on a phone), but not on-chip. |
| X7 | **Display size** | Our board drives one 240×320 TFT. Adding a second display needs a second SPI bus and more GPIO. |
| X8 | **Power consumption during deep sleep** | ~10 µA is already very good. Below that we'd need a different chip entirely. |
| X9 | **BLE MIDI latency** | BLE has a ~7.5 ms connection interval minimum. Not as tight as wired MIDI (~1 ms). |
| X10 | **Sample-rate mismatch with USB audio** | The ESP32-S3 USB-OTG can do isochronous audio at 48 kHz reliably. 96 kHz over USB is possible but flaky. |

### 10.5 Which constraints are worth keeping

This is the counter-intuitive part of the section. **Not every PO-33 limit should be broken.** Constraint is part of why the device is fun.

| Limit | Argument for keeping it |
|---|---|
| L1 — 40 s total | Forces the user to curate. A 5-minute pool fills with mediocre takes. |
| L3 — 16 slots | Forces you to delete bad samples, which is creative discipline. |
| L4 — 16 steps | One bar of 16th-notes is the rhythmic sweet spot. 32 steps is technically possible but the grid gets too dense for a 2.4" screen. |
| L5 — 16 patterns | Anything more becomes a menu, not a beat. |
| L8 — 16 effects | A small palette is more learnable than a large one. AMY supports dozens but exposing all of them defeats the PO-33's "punch-in one button, get one effect" model. |
| L9 — 3 BPM levels | Naming the levels (Hip Hop / Disco / Techno) gives a beginner a starting tempo without forcing them to count. |

The PO-33's UI is constrained *on purpose*. Our v3 should preserve most of these constraints even as the underlying hardware becomes more capable. The role of constraint in creative work is well-studied — see Brian Eno's "oblique strategies" or any number of game-design articles on the topic.

### 10.6 A v3 wishlist, ranked by impact

The following are features we **could** ship if time permitted, ranked by (user value × implementation cost). The list is intentionally short — we are not committing to ship any of these.

| Rank | Feature | User value | Implementation cost | Why it ranks here |
|---|---|---|---|---|
| 1 | **WiFi Ableton Link sync** | Lets the PO-33 participate in a DAW session alongside a laptop, iPad, or other Link-enabled gear. Unlocks studio use. | High: needs the Link protocol ported to ESP-IDF (~1–2 kLOC). | Massive user-value jump (turns the device from a toy into a pro tool). |
| 2 | **USB-MIDI device class** | Plug the PO-33 into a laptop and use it as a 16-button MIDI controller. Adds keyboard, pad-controller, and DAW-integration use cases. | Medium: TinyUSB component + ~500 LoC. | High user value, well-trodden code path. |
| 3 | **BLE MIDI** | Wireless MIDI without USB. Plays well with iOS / Android / macOS. | Medium: NimBLE + MIDI service profile. | High value, but overlaps with USB-MIDI. |
| 4 | **Streaming samples from the 16 MB flash** | 5-minute sample pool (or much longer if we accept compression). | Medium: LittleFS streaming in `amy_bridge.c`. | Big value, modest cost. |
| 5 | **Waveform + spectrum on the 2.4″ TFT** | Visual feedback while recording / playing. Pro-level feel. | Medium: AMY exposes an FFT hook; render in `display.c`. | Nice-to-have more than essential. |
| 6 | **Effect chains per voice** | More expressive performances. | Low: just an array of effects in `amy_event`. | Easy win, but the PO-33 community might prefer the existing single-effect model. |
| 7 | **Dual-source recording (mic + line-in)** | Record the room *and* a synth simultaneously. | Medium: second I²S RX channel + a jack. | Niche but cool. |
| 8 | **WiFi web UI for preset library** | Browse + upload presets from a phone. | High: web server + UI. | High novelty, low practical use. |
| 9 | **AI patch suggestions** | Suggest effect chains based on the audio. | Very high: needs ML model. | Out of scope for v3. |

Honest note: items 1, 8, and 9 are aspirational. Items 2–7 are realistic on a one-month timeline for a single developer. Items 2 and 4 are what we would build first.

> **Deferred: SD card support.** The earlier wishlist draft mentioned an SD card interface for swappable / unlimited storage. **Removed from the wishlist.** The ESP32-S3-WROOM-1-N16R8 module's GPIO matrix is fully allocated after the v1 pin assignments (see HARDWARE.md §8.4 for the full GPIO-budget analysis); adding an SD card would cost 1–6 new GPIOs depending on mode, and we have zero. The two practical paths forward are **(a)** a future hardware revision that drops the octal PSRAM to recover GPIO 26–32, or **(b)** using USB-MSD instead when the USB-MIDI commit lands (same laptop-interchange benefit, zero new GPIOs, but mutually exclusive with USB-MIDI). See HARDWARE.md §8.4 for the full options table.

---

## 11. Multi-sketch storage & restore

> **Vocabulary.** The unit of creative work this section designs for is called a **sketch** (one sketch = samples + 16 patterns + a 128-step chain + metadata). The PO-33 calls the same idea a "song"; the Korg Electribe calls it a "Pattern Set"; Ableton Live calls it a "Live Set". We picked "sketch" because it's short, plain, and doesn't clash with sampler vocabulary. Throughout §11, the **C identifiers in the API block use the `sketch` prefix** (e.g. `storage_sketch_save_active()`, `sketch_meta_t`, `SKETCHES_MAX`) — the rename has already happened in the design, so the v2 implementation should match. **Outside code blocks, every "project" in this section means "sketch".**

### 11.0 Why this section exists

The current v1 firmware has a **single-bank** storage model:

- 16 patterns in RAM as `g_patterns[16]`.
- One chain of up to 128 pattern entries as `g_chain[128]`.
- One sample pool in PSRAM.
- A `storage_save_all()` that writes each pattern / slot to a flat file (`p0.bin`, `p1.bin`, …, `p15.bin`, `s0.bin`, …, `s15.bin`).
- A `storage_load_all()` that unconditionally reads every file and clobbers RAM.

This works — but only if you treat the device as if it holds **exactly one sketch at a time**. Start building a new chain, hit save, and you overwrite the sketch you'd been working on. There is no "save as", no "switch sketch", no "delete sketch". It is, in essence, the same storage model the real PO-33 has (it also holds exactly one song at a time, with up to 128 patterns in its chain).

For a $5 dev board with **16 MB of flash** and **8 MB of PSRAM**, this is an absurd waste. We can store **dozens of independent sketches**, each with its own sample pool, pattern bank, chain, and metadata. The user can flip between them. The active sketch lives in PSRAM (zero-latency playback); archived sketches live in LittleFS on flash.

This section is the **design** for that system. It is a v2 proposal — v1 firmware is unchanged. No code in this section is committed; everything below describes what the v2 implementation should look like.

### 11.1 Concepts

- **Sketch** — a self-contained creative unit: a name, a sample pool, 16 patterns, a chain, BPM, and metadata. The atomic unit the user saves, switches between, deletes, or duplicates.
- **Active sketch** — the one sketch whose sample pool is in PSRAM and whose patterns are live in `g_patterns[16]`. At any moment, exactly one sketch is active. Switching to a different sketch pages its data into RAM; the previously-active sketch is flushed to flash.
- **Sketch ID** — a **4-character hex string** (e.g. `a1b2`), assigned by a monotonic counter starting at `0000`. 16 bits of entropy (65 536 possible IDs); collisions are impossible because we only assign each ID once. 4 hex chars makes folder names short, easy to type on a UART shell, and trivial to recognise in a sketch picker UI.
- **Sketch metadata** — a small JSON file describing a sketch: name (≤24 chars), label color (1 of 8), created-at Unix timestamp, last-modified-at Unix timestamp, BPM at last save, sample count, pattern count, chain length, total playback duration.
- **Atomic write** — a save operation that either fully completes or is fully rolled back, so a power loss mid-write can never leave a half-written sketch. We achieve this with the classic write-to-temp-then-rename trick.
- **Sketch slot** — a serial index from 0 to N-1, where N is determined by available flash. The slot number is for UI ordering and internal bookkeeping; the 4-hex ID is the canonical identifier. Two sketches can never share a slot number.

### 11.2 On-flash layout

The `sketches` partition is mounted at `/sketches/` and is laid out
as one folder per sketch plus a master index. The full sketch
lifecycle is described in §11.4; the canonical C identifiers and
APIs are listed in §11.5.

```
/sketches/                                   mount root (LittleFS)
  sketches.lst                               master index, "<id>\t<name>\n"
  <id>/
    meta.bin                                 fixed 24-byte NUL-padded name
    samples.bin                              256-B slot table + raw PCM
                                              (see §11.2a for the format)
    patterns.bin                             16 * sizeof(pattern_t) raw
    chain.bin                                1-byte len + len bytes (entries)
  tmp/                                       scratch for atomic writes
```

Notes:

- **4-hex ID folders.** The folder name is the sketch ID
  (`0000`, `0001`, …, `ffff`). Folders are flat at the `sketches/`
  root — no nesting — so LittleFS directory traversal stays cheap.
- **Hex counter.** IDs are assigned sequentially:
  `storage_next_free_id()` walks `sketches.lst` and returns
  `max(ids) + 1`, formatted as 4 hex chars. If the device has
  never had a sketch, the first ID is `0000`. The `sketches.lst`
  index records both id and display name.
- **`sketches.lst` is the master index.** It is rewritten
  atomically on every sketch create / delete / rename. The picker's
  `storage_sketch_list()` reads it. Format: `# PO-33 K.O! sketch
  index (auto-generated)` header line, then one `<id>\t<name>\n`
  per sketch.
- **Active-sketch pointer.** The currently-loaded sketch's ID lives
  in NVS under namespace `sketches`, key `active_id` (a 4-hex
  string). On first boot the value is the default `0000`; if the
  pointed-to sketch doesn't exist (fresh flash, factory-reset
  since last boot), `storage_load_all()` returns ESP_OK with RAM
  empty -- behaves like a brand-new device. ADR-0004.
- **Atomic write.** All sketch files are written to a sibling
  `<path>.tmp`, fsync'd by `fclose()`, then `rename()`d into
  place. Power loss mid-write leaves either the old or the new
  file -- never a half-written one.

#### 11.2a `samples.bin` on-disk format

```
┌──────────────────────────────────────────────────────────┐
│ slot_table[16]: 16 × 16-byte slot records (256 B total)  │
├──────────────────────────────────────────────────────────┤
│ pcm[]: raw int16 little-endian, each slot's samples back  │
│        to back in slot order. Empty slots contribute 0 B. │
└──────────────────────────────────────────────────────────┘

struct slot_rec (16 bytes, packed, little-endian):

  u32 length_samples     0 = empty (no PCM follows for this slot)
  u32 sample_rate_hz
  u32 start_sample        trim start (sample index into this slot)
  u32 end_sample          trim end (sample index, exclusive)
```

This is the only place the on-disk sample format is defined; keep
`storage_sketch_save_samples()` and `storage_sketch_load_samples()`
in lockstep with this layout. ADR-0004.

### 11.3 On-RAM state

At any moment, exactly one sketch is loaded into PSRAM. The in-RAM data model does **not change** — it is still `g_patterns[16]`, `g_chain[128]`, and the PSRAM sample pool. What changes is the **mapping** between in-RAM state and on-flash state.

Concretely:

| In-RAM symbol | Holds the state of | Paged in when | Paged out when |
|---|---|---|---|
| `g_patterns[16]` | active sketch's 16 patterns | sketch switch (`storage_sketch_load`) | sketch switch (save then load new) |
| `g_chain[128]` + `g_chain_len` | active sketch's chain | same | same |
| PSRAM sample pool (3.37 MB) | active sketch's samples | same | same |
| `s_bpm`, `s_pattern`, etc. | active sketch's transport state | same | same |

**The PO-33's "chain" stays a chain.** Each sketch carries one chain. The difference from v1 is that there are now *N* chains on the device, one per sketch.

### 11.4 Sketch lifecycle

The state diagram for a sketch:

```
                +---------+    create    +----------+
                |         | ----------> |          |
                |  EMPTY  |             | CREATED  |  (no samples, no patterns; auto-named by §11.6a generator, e.g. "bebop_coltrane")
                |         |             |          |
                +---------+             +----------+
                                          |
                                          | first record
                                          v
                +---------+    load     +----------+
                |         | <--------- |          |
                |          |  -------  | SAVED    |  (on flash, may be active or not)
                |          |  ---      |          |
                |          |  <-       +----------+
                |  ACTIVE  |
                |          |  -------> 
                +----+----+
                     |   |
        modify       |   |    save
        (in RAM)     |   |   (atomic)
                     |   |
                     v   v
                +----------+
                | MODIFIED |  (RAM differs from flash; will be lost on power-off)
                +----------+
                     |
                     | save (atomic)
                     v
                +----------+
                |   SAVED  |  (RAM == flash, durable across power-off)
                +----------+
                     |
                     | delete
                     v
                +----------+
                | DELETED  |  (folder removed from flash; sketches.lst updated)
                +----------+
```

State transitions:

- `EMPTY → CREATED`: the user picks "New Sketch" on the TFT. We allocate the next 4-hex ID from the counter, write a default `meta.json` (auto-named by the §11.6a Docker-style generator; e.g. "bebop_coltrane", "cool_davis", … label color = 1, timestamps = now), and add an entry to `sketches.lst`. The active sketch becomes this one and starts empty.
- `CREATED → MODIFIED`: any recording, edit, or transport change. Pure RAM.
- `MODIFIED → SAVED`: user picks "Save" on the TFT, or auto-save fires. We write `samples.bin`, `patterns/p*.bin`, `chain.bin`, and `meta.json` (with bumped last-modified-at) atomically. RAM == flash.
- `SAVED → MODIFIED`: user edits anything. Pure RAM.
- `ACTIVE → DELETED`: user picks "Delete sketch" on the TFT. We require confirmation. We then `unlink()` the sketch folder and rewrite `sketches.lst`. If it was the active sketch, the device falls back to an empty sketch (or another sketch if one exists). The NVS `sketches/active_id` key is cleared if the deleted sketch was active; the next boot's `storage_load_all()` will default to `0000`.
- `ACTIVE → EMPTY`: like DELETE but for the active sketch. The user is left with no active sketch until they pick or create one.
- `SAVED → DEAD → ACTIVE` (power-cycle): when the device goes to deep sleep (`power_mgmt_enter_deep_sleep()` or the `sleep` UART verb), `storage_save_all()` first flushes the active sketch's `samples.bin`, `patterns.bin`, `chain.bin`, and `meta.bin` to flash under `/sketches/<id>/`. On the next boot, `app_main()` calls `storage_load_all()` after `amy_bridge_init()`; it reads the NVS `sketches/active_id`, opens `<id>/patterns.bin` + `<id>/chain.bin` + `<id>/samples.bin`, and registers the slots with AMY. The sketch is "exactly where the user left it" — the PO-33's "power-off then on = same sketch" behaviour. ADR-0004.

### 11.5 API additions

These are the implemented public functions in `main/storage/storage.h`.
"v1 transitional" entrypoints (`storage_load_all()`,
`storage_save_all()`) keep their old names so the user-facing UART
verbs (`save`, `load`) and the boot path didn't have to change.

```c
/* In storage.h */

/* Boot + user-facing save/load (v1 transitional names; sketch-scoped
 * internally now -- ADR-0004). */
esp_err_t storage_save_all(void);          /* save active sketch + bump NVS active_id */
esp_err_t storage_load_all(void);          /* read NVS active_id, load sketch into PSRAM */

/* Active-sketch identity (NVS-backed). */
esp_err_t storage_get_active_sketch_id(char *out_id);
esp_err_t storage_set_active_sketch_id(const char *id);

/* Multi-sketch APIs. */
esp_err_t storage_sketch_list(uint8_t *out_count,
                              char (*out_ids)[SKETCH_ID_LEN + 1]);
esp_err_t storage_sketch_load           (const char *id_str);  /* patterns + chain */
esp_err_t storage_sketch_load_samples   (const char *id_str);  /* sample pool only */
esp_err_t storage_sketch_load_with_samples(const char *id_str); /* both */
esp_err_t storage_sketch_save_samples   (const char *id_str);
esp_err_t storage_sketch_create         (void);
esp_err_t storage_sketch_save_active    (void);                /* atomic write + bump active_id */
esp_err_t storage_sketch_delete         (const char *id_str);
```

Pending (not yet implemented; tracked as future work):

- `sketch_meta_t` struct (created_at / modified_at / label_color / bpm-at-save) — currently the on-disk meta.bin just holds the 24-byte name. Adding the rest is a half-day of work that doesn't change the sketch-lifecycle or boot path.
- `storage_sketch_export` / `storage_sketch_import` (`.zip` over USB-MSD) — v3 per §11.6.
- Debounced auto-save on edit (`sequencer_request_save()` queue + storage task) — v2 future hardening. Today, saves happen on the `save` UART verb and on deep sleep.
- `storage_sketch_rename` / `storage_sketch_duplicate` — not needed yet (the sketch picker + delete + create covers the use case).
- `ui_menu_sketch_picker` is now wired via `WRITE` long-press (`main/sketch/sketch_picker.c`), not via a menu screen — see CONTROL_REFERENCE.md for the gesture.

### 11.6 PO-33 parity vs. extension

| Feature | PO-33 | v1 (current) | v2 (this section) |
|---|---|---|---|
| Sample memory | 40 s | 40 s | 40 s × N sketches in flash; 40 s live in PSRAM |
| Patterns | 16 | 16 | 16 per sketch, N sketches |
| Pattern chain (the PO-33's "song chain") | up to 128 | up to 128 | up to 128 **per sketch**, N sketches |
| # of sketches | 1 | 1 | N (bottleneck: flash size) |
| Save | auto on power-off | manual `save` UART command + auto on deep sleep | auto on edit (debounced) + manual |
| Back up | audio out to tape (slow, lossy) | `storage save` writes 1 set of files | export one sketch as .zip over USB-MSD (v3) |
| Copy between devices | P2P audio cable | not implemented | USB-MSD export/import (v3) |

The PO-33's "one song, one chain" is preserved **within** a sketch. What we add is that the device can hold N sketches, each with its own chain. This is the same conceptual model as the Korg Electribe's "Pattern Set" or Ableton Live's "Live Set" — a higher-level container that owns a complete working state.

**Boot loads the last-active sketch.** On boot, `app_main()` calls `storage_load_all()` (after `amy_bridge_init()`); it reads NVS key `sketches/active_id`, opens the matching sketch folder, and registers the patterns + chain + samples with AMY and the sequencer. The "power-off then on" experience is the same sketch the user left. ADR-0004.

> **Vocabulary note.** The PO-33 calls its unit of creative work a "song". We call it a **sketch**. The two refer to the same idea (samples + patterns + a chain + metadata); we just picked a shorter word that doesn't have pop-music connotations or clash with sampler vocabulary (we avoid "groove" because that word means *timing templates* in the rest of the music-software world). The 16-step chain *within* a sketch is unchanged from the PO-33's "song chain".

### 11.6a Sketch-name convention (Docker-style, jazz-flavoured)

The display name of a sketch follows a **Docker-style two-word convention**: `<genre>_<musician>`, all lowercase, ASCII letters only, with an optional `_N` collision suffix.

**Why Docker-style?** The PO-33 names its songs sequentially ("Song 01", "Song 02") which is boring and not very evocative. We want names that are:
1. **Human-memorable** — so a user scrolling through the sketch picker can find their work by feel.
2. **Curated** — so the namespace doesn't degenerate into `sketch_x` / `my_sketch_v2_final_FINAL`.
3. **Visually short** — the TFT picker shows ~16 chars per cell; we need to fit comfortably.
4. **Plenty of headroom** — 192 unique canonical names cover 12× SKETCHES_MAX before any collision suffixes kick in.

**The vocabulary** lives in `main/sketch/sketch_name.{c,h}`:

| Field   | Count | Examples                                                                 |
|---------|-------|--------------------------------------------------------------------------|
| Genres  | 12    | bebop, cool, swing, hardbop, modal, bossa, fusion, free, latin, smooth, acid, nu |
| Musicians | 16  | coltrane, davis, monk, parker, gillespie, evans, corea, wayne, basie, powell, blakey, jobim, coleman, tyner, dolphy, mingus |

Every `<genre>_<musician>` combo fits in ≤ 17 bytes; plus the optional `_N` collision suffix (up to `_99`), every legal name is ≤ 20 bytes — comfortably inside `SKETCH_NAME_MAX = 24`.

**Examples of canonical names** (no collision suffix):
```
bebop_coltrane    cool_davis        swing_basie
hardbop_blakey    modal_evans       bossa_jobim
fusion_corea      free_coleman      latin_powell
smooth_monk       acid_tyner        nu_mingus
```

**Collision handling.** When a user creates a sketch and the generator's lex-first choice is already taken (e.g. `bebop_coltrane` already exists), the generator appends `_2`, `_3`, … up to `_99` before falling back to the next canonical pair. So a user might see `bebop_coltrane`, `bebop_coltrane_2`, `bebop_coltrane_3`, `bebop_davis`, … — the same convention Docker uses for container names. The total namespace is `12 × 16 × 99 = 19,008` unique names, which is ~1,200× the SKETCHES_MAX = 16 hard cap.

**The four-byte sketch ID is independent** (`0000`…`FFFF`). It's the folder name on flash, the API key (`storage_sketch_switch(id)`), and the path inside `/sketches/`. The Docker-style name is purely the display name in `sketch_meta_t.name`. This decoupling is intentional: renaming a sketch (future feature) should never have to touch the filesystem layout.

**Validation rules** (enforced by `sketch_name_is_valid()`):
- Lowercase ASCII letters and digits only
- Exactly one or two underscores (no `_` at start, no trailing `_`)
- Non-empty `<genre>` and `<musician>` parts
- Optional collision suffix is `_` followed by 1+ digits, with `_0` rejected (it would look confusingly like a canonical name)
- Total length ≤ `SKETCH_NAME_TOTAL_MAX = 24`

This rejects: `Bebop_Coltrane` (uppercase), `bebop-coltrane` (hyphen), `bebop coltrane` (space), `bebop_` (empty musician), `_coltrane` (empty genre), `bebop_coltrane_0` (zero suffix), `bebop_coltrane_2x` (non-digit suffix), 25-char names, etc.

### 11.7 Concurrency / atomicity

LittleFS is journaled but not transactional across multiple files. We must be careful.

**Atomic-write protocol for a sketch save:**

1. Acquire a mutex to serialize saves against the storage task.
2. Write all files to a unique `tmp/` subfolder, e.g. `tmp/<id4>/`.
3. `rename()` each file into the sketch folder. LittleFS `rename()` is atomic within the same directory.
4. After all files are renamed, rewrite `sketches.lst` atomically (`tmp/sketches.lst.tmp` → `sketches.lst`).
5. Release the mutex.

If power is lost at any step:

- Files left in `tmp/<id4>/` are stale. On boot, the storage init scans `tmp/` and deletes any file older than 1 minute. (Or just any file; `tmp/` is not user-visible.)
- Files in the sketch folder that were renamed are intact.
- If `sketches.lst` was not yet rewritten, the old version is used. The user sees the old sketch list, which is the safe failure mode.
- If `sketches.lst` *was* rewritten but the sketch folder rename was incomplete, the entry in `sketches.lst` will point to a sketch that's "half present". On boot, we validate each `sketches.lst` entry against the actual filesystem and drop any orphaned entries.

### 11.8 Capacity math

We sample at **44 100 Hz × 16-bit mono** to match AMY's native render rate. At that rate:

- **1 second of mono audio = 88 200 B ≈ 86 KB.**
- **40 seconds = 3.45 MB** (this is the v1 PO-33-equivalent pool size).

Let's work through a real example. Average sketch (the PO-33's typical usage):

| Component | Math | Size |
|---|---|---:|
| Sample pool | 8 samples × 2 s × 44 100 Hz × 2 B = **1 411 200 B** | **~1.38 MB** |
| Patterns | 16 patterns × 16 steps × ~50 B/step | **~13 KB** |
| Chain | 128 B | **0.13 KB** |
| Metadata | `meta.json` | **~0.2 KB** |
| **Total per sketch (typical)** | | **~1.42 MB** |

The sample pool is **>97 %** of every sketch. Drop a sample or shorten one and the sketch shrinks proportionally; add a long sample and it grows proportionally.

#### Live PSRAM sample pool ceiling (active sketch)

The on-flash numbers above are for archived sketches. The **active sketch's** sample pool must fit in PSRAM alongside the TFT framebuffer and AMY state. The math at 44 100 Hz:

| Subsystem | Approx. size |
|---|---:|
| TFT framebuffer (240×320×2) | ~150 KB |
| AMY state (idle / no audio) | ~100 KB |
| AMY state (moderate — 4 voices + reverb) | ~1.5 MB |
| AMY state (heavy — 16 voices + echo + chorus) | ~3.5 MB |
| AMY state (stress — 30 voices + heavy DSP) | ~6 MB |
| LittleFS cache, heap overhead, misc | ~256 KB |
| **Available for samples** (8 MB PSRAM minus above) | **~6.3 MB (idle) down to ~1.8 MB (stress)** |

At 88 200 B/s, that gives:

| Scenario | Live sample memory |
|---|---:|
| Idle / no audio rendering | **~75 s** |
| Moderate DSP (4 voices + reverb) | **~72 s** |
| Heavy DSP (16 voices + echo + chorus) | **~49 s** |
| Stress (30 voices, polyphony ceiling) | **~21 s** |

**The PO-33's 40 s ceiling is matched under stress, beaten under typical loads.** The earlier 100–150 s figures in this section were based on the wrong sample rate (22 050 Hz) and have been replaced.

#### On-flash partition size

Partition `sketches` is **256 KB** in `partitions.csv` — that's *too small* for even one typical sketch at 44 100 Hz (1.42 MB). We need to **enlarge** this partition.

| Partition size | Typical sketches (~1.42 MB) | Tiny sketches (~133 KB) |
|---|---:|---:|
| 1 MB | 0 | ~7 |
| **8 MB** | **~5** | **~60** |

> The 16 MB row has been removed — bumping `sketches` past 8 MB would require shrinking the `factory` app partition below its current 3 MB, which is not worth it. If we ever want more, we'd add an external flash (e.g. an SPI NOR chip) rather than shrink the app partition.

Recommended: **`sketches` → 8 MB**. That gives ~5 typical sketches or ~60 tiny sketches, with comfortable headroom for filesystem overhead. The 16 MB flash chip on the ESP32-S3-WROOM-1-N16R8 has plenty of room for the 3 MB `factory` app partition, the small `nvs` + `phy_init` partitions, and the 8 MB `sketches` partition.

A `samples` partition is **not needed** in v2 — the active sketch's sample pool lives in PSRAM, and archived sketches' sample pools live in their sketch folder inside `sketches/`. The `samples` partition in the current `partitions.csv` is unused; we recommend removing it.

We propose a hard cap `SKETCHES_MAX = 16` for the UI's sketch picker — beyond that the list becomes hard to navigate anyway. The hard cap is **not** enforced by flash space (16 × 1.42 MB ≈ 23 MB which still exceeds our 8 MB partition) — it's a UI limit. Real capacity is whatever fits in the partition.

> **Action item for v2 implementation:** update `partitions.csv` to enlarge the `sketches` partition from 256 KB to **8 MB** and remove the unused `samples` partition. The `factory` app partition stays the same. **Done in `776bca7`** — `partitions.csv` now has `patterns, data, littlefs, 0x310000, 0x800000`.  *(Later renamed to `sketches` partition — see the §11.2 Note below.)*

#### Caveats on these numbers

- **AMY state size is an estimate.** The "moderate / heavy / stress" rows come from looking at the AMY source structure (oscillator bank + chorus/echo buffers + reverb tail). Real numbers would need a profiling run with `-O2` and `amy_overload_check()` enabled.
- **Polyphony ceiling on the ESP32-S3 is ~30 voices** before audio glitches. The "stress" row above is a real upper bound, not extrapolation.
- **Per-sample lengths vary.** A sketch with 16 long samples (10 s each) uses ~14 MB on flash — one such sketch fills the 8 MB partition almost entirely. The 5-sketches figure assumes the *typical* profile (8 short samples), not the max.
- **Streaming-to-flash** (v3) would let a single sketch's *live* pool exceed the PSRAM ceiling by streaming long samples from LittleFS on demand. AMY does not natively support streaming; this is real engineering, not a config change.

### 11.9 v2 UI proposal — the Sketch picker screen

The picker is entered by holding **WRITE** for ≥600 ms (PO-33's "enter write mode" verb repurposed for v2 picker entry). It exits on a tap of WRITE. While the picker is active, the rotary encoder on the left side of the board (see `hardware/HARDWARE.md` §4.12) is the primary input device:

```
   (encoder)        +-----------------------------------+
     (o)            |  SKETCHES                  3 / 16 |     ← top bar: count
     ↑              |                                   |
     click          |   * DRUM KIT 1       0003         |     ← * marks active
     =load+exit     |     DISCO DEMO       0001         |
                    |     AMBIENT 03       0002         |
                    |     (unused slot)                 |     ← "Create new" hint
                    |                                   |
                    |   [step 1..16]  load + exit         |
                    +-----------------------------------+
```

Input bindings:
- **Turn encoder** (1 detent = 1 sketch): scroll the highlight.
- **Click encoder**: load + exit (same as pressing the highlighted step).
- **Press step 1..16**: load + exit (direct jump). Pressing a step that has no sketch (slot index ≥ `s_count`) is a no-op.
- **Tap WRITE**: exit without loading.
- **Hold WRITE** (≥600 ms, while picker is inactive): enter the picker.

The picker is a single page of 16. There are **no per-sketch rename UI** in v2: each sketch has an auto-generated name ("Sketch N") and a 4-hex ID. Renames happen via the `sketch` UART command (see §11.10 in the storage layer, landed in Commit 3).

The TFT screen layout for the picker is in `display.c`. When the picker is active, `display_tick()` calls `render_picker_screen()` instead of `render_main_screen()`. The layout is a 240x320 ILI9341 panel:

```
  y=0..18    status bar (blue), "PICKER" + "TURN=scroll"
  y=24       "SKETCHES  N / 16" (left) + "click=load" (right, dim)
  y=40..300  4 rows x 4 cols of slot cells, 56x64 px each, 4-px gap
  y=308      "WRITE=exit" (left) + "tap step=load" (right, dim)
```

The currently-highlighted cell is rendered with a red background and a yellow `*` glyph at the bottom-left. Loaded cells show the 4-hex sketch ID in white; empty cells show `----` in dim grey.

Skip pills (s_modifiers[] / s_held_modifiers[] tables): the picker's `step press` path runs **before** the modifier-table routing in `input.c` so a step press in the picker doesn't accidentally fire SOUND-step / PATTERN-step / FX-step bindings.

### 11.10 Open questions for v2 implementation

These are decisions I'm flagging now but punting to the implementer.

1. **Save policy** — should `storage_sketch_save_active()` be:
   - (a) **manual only** — user presses WRITE + REC or similar to save. Matches PO-33.
   - (b) **debounced auto-save** — every edit triggers a save 2 s later. No "did I forget to save?" anxiety.
   - (c) **both** — manual save is immediate; auto-save runs in background every N seconds if there are unsaved changes.

   Recommendation: **(c)**. Manual save is the PO-33 way; auto-save is the safety net for novices.

2. **Sketch delete confirmation** — how many button presses to confirm? PO-33 requires holding REC + PATTERN. We could:
   - (a) require holding SOUND + the sketch's slot number for 2 s.
   - (b) require a separate "delete mode" entered via SOUND + a step number.
   - (c) require two separate presses (first selects, second confirms).

   Recommendation: **(a)** — 2-second hold on the slot is unambiguous and matches the PO-33's destructive-action convention.

3. **Storage backend** — should we:
   - (a) **stick with LittleFS** (current v1 choice) — proven, journaled.
   - (b) migrate to **FAT** (via esp_littlefs → esp_vfs_fat) — better Windows/macOS support for USB-MSD export.
   - (c) use a **custom flat format** — fastest, but no USB access.

   Recommendation: **(a) for v2**, then **(b)** for v3 when USB-MSD is added.

4. **Backward compatibility with v1's storage** — the v1 storage has flat `p0.bin..p15.bin` and `s0.bin..s15.bin`. v2's sketch folder layout is incompatible. On first boot with v2 firmware, the storage init should:
   - (a) detect the old flat layout and migrate it into a new sketch (e.g. `legacy-default/`) automatically. Safe.
   - (b) leave the old files alone and start with an empty sketch. The user loses their old patterns if they don't migrate manually.

   Recommendation: **(a) with a one-shot migration** — detect old layout, rename to `legacy-default/`, treat as the active sketch on first boot, then delete the flat files after the user saves once.

5. **Sketch metadata format** — JSON (human-readable but parseable) vs. a custom key=value format (smaller, faster to parse). Recommendation: JSON, written by `cJSON` (already in ESP-IDF as a built-in component).

---

## 12. Cliff's notes: reading "Make: Electronic Music from Scratch" alongside this project


### 12.0 Why this book pairs with our project

This document is about a **digital** device — software running on a microcontroller that emulates a sampler. *Make: Electronic Music from Scratch* by **Kirk Pearson** (Maker Media, October 2024; subtitle: *A Beginner's Guide to Homegrown Audio Gizmos*) is about the **analog** roots of the same art. The book teaches you to build real, working electronic musical instruments out of resistors, capacitors, integrated circuits, solder, and batteries. Our project teaches you to build the same kind of instrument out of code.

The two together give a complete picture. The book explains *what an oscillator is* and *why filters matter* — concepts that are buried under DSP jargon in most digital-music books. Our project shows the same concepts *expressed in code* — you can see an oscillator running and a filter sweeping, but you'd have a hard time figuring out why anyone bothered to write the code without the conceptual foundation.

Pearson's book is from the **Dogbotic** studio in Berkeley, and is explicitly written for "total beginners" — "even the biggest electrophobes". It contains more than 40 hands-on projects, over 400 color photographs, and seven appendices. Its final project, the **Dogbotophone MK1** (chapter 14), is a 15-step build of "a massive electronic orchestra complete with multiple voices, drums, and self-patching sequencers" — which is exactly the spirit of what our PO-33 firmware aspires to be, just in silicon instead of solder.

**The full text of Pearson's book is included in this repository at `docs/Make_Electronic_Music.md`**, provided by the author for the purpose of producing this companion. All chapter summaries and project listings below come from that full text.

**A full 16 000-word companion study guide is at `docs/MAKERS_COMPANION.md`**. It pairs every chapter of Pearson's book with a UART shell exercise on our firmware. If you have a flashed device in front of you, start there.


### 12.1 The book, one chapter at a time

Below is every chapter of the book with its exact title, page range, and project list. The titles and page numbers are taken directly from Pearson's table of contents.

**Part I: Electrical Music Fundamentals**

1. **A People's History of Electronic Music** (pp. 2–15). The cultural history of the synthesizer — Cahill's Telharmonium, Theremin, Moog, the Ondioline, and the case for synths as folk instruments. No projects; essays only ("Nobody trusts electronic music", "Synthesizers as products", "The bane of learning electronics", "Our road map").
2. **Musical Electricity for Electrophobes** (pp. 16–47). The basic physics of electricity as it applies to audio. Four experiments: The World's Simplest Speaker, Speaker-as-Microphone Identity Crisis, Piezomania, Sound Through Your Skull. Closes with "A Moment of Clarity" — voltage is potential, current is flow, resistance is pushback.

**Part II: Smaller Circuits**

3. **The Hello World Oscillator** (pp. 48–71). The centrepiece of the book. Introduces the breadboard, jumper wires, resistors, LEDs, potentiometers, capacitors, and integrated circuits. The main project is **My-First-Square-Wave-Oscillator™** — a 555 timer chip in astable mode with two resistors and a capacitor. Four variations: The Pinch-O-Matic (squishy resistor), Playing with Your Food (capacitor as a fruit), The Pencil-Pusher (graphite resistor), and A Photo Theremin (LDR replaces pot).
4. **Amps, Reverbs, and Talkboxes** (pp. 72–89). Builds a battery-powered LM386 amplifier. Three projects: **A Breadboard Power Amp**, **Stereo Panning** (one pot splits audio between two speakers), **A Plate Reverb** (a metal plate vibrates, contact mics pick it up), and **A Talkbox** (a speaker plays into your mouth).
5. **Soldering, Enclosures, and UI** (pp. 90–109). Lab equipment, soldering safety, and completing the Photo Theremin into a permanent instrument.
6. **Chaining Oscillators** (pp. 110–135). Polyphony and signal routing. Projects: **Polyphonic Square Waves**, **Tremolo**, **The Electro-Cricket** (555 + 40106 hex inverter), **The Undertoner** (subharmonics), and three variations: **The Atari Punk Candle**, **The Gating Oscillator**, **The Vactrol Arpeggiator**.

**Part III: Bigger Circuits**

7. **Schematics and Mass Transit** (pp. 136–155). A literacy lesson in reading schematics, using the Rotterdam Metro as an analogy.
8. **Filters** (pp. 156–173). The harmonic series, additive vs subtractive synthesis, and the theory of impedance. Projects: **The Passive Low-Pass Filter**, **The Passive High-Pass Filter**, **The Active High-Pass Filter**, **The Active Low-Pass Filter**, **The Active Band-Pass Filter** (using the LM741 op-amp).
9. **Harmonization** (pp. 174–191). Circuits that generate harmonics or subharmonics of a fundamental. Projects: **Octave Harmonizer**, **Subharmonic Generator**, **10-Stage Wavetable Generator** (using the 4017 decade counter), **The FM Yodeler**.
10. **Modulation** (pp. 192–207). Circuits that dynamically control other circuits. Projects: **The Vantastic Vactrol** (LED + LDR = light-controlled resistor), **Pulse-Width Modulation**, **PWM with Vactrols**, **A Button-Controlled VCA**, **An LFO-Pingable VCA**, **The Piezo Drum Trigger**, **Ring Modulation**, **Stereo Tremolo**.

**The sequencer hub**

11. **Sequencers** (pp. 208–229). The second-biggest chapter. Introduces the 4051 multiplexer and Boolean logic, then builds six projects: **The Disco Boole (Part I)**, **The Disco Boole (Part II)**, **The Standard Eight-Step Sequencer**, **A Pattern-Changing Sequencer**, **A First-Order Reset Harmonization Sequencer**, **A Second-Order Reset Harmonization Sequencer**. The chapter closes with a section on interconnections between voice parts.

**Back to drums**

12. **Electronic Percussion** (pp. 230–251). A drum machine from analog oscillators using the 40106. Sections: making **Inharmonic Sounds** (tambourines, crash cymbals), making **Harmonic Sounds** (kick drums, tom-toms), making **Noise** (snare drums), and **Percussion Sequencing** with examples **Bomba** and **Bossa Nova**.

**Tuning and the boss fight**

13. **Phase-Locked Loops** (pp. 252–267). The 4046 PLL chip and its uses. Projects: **The 4046 VCO**, **Portamento, Glide, and Glissando**, **VCO Switching**, **Sample and Hold**, **LFO Multiplication**, **Audio-Frequency Multiplication**.

**Part IV: Putting It All Together**

14. **The Dogbotophone MK1** (pp. 268–283). The boss fight. A 15-step build of a complete multi-voice orchestra: audio-rate oscillator → sequencer clock → overtone synth → 16-step sequencer for overtone → undertone synth → 16-step sequencer for undertone → drum sequencer → tunable square wave → twin-T voices (×2) → cymbal (XOR voice) → diode AND-gate reset → optocoupler-controlled sequencer → optional active HP/LP filters → op-amp mixer. Closes: "If you've made it this far, congratulations! Your room is likely a mess of wires, but you've done it."

**Closing**

15. **Thoughts on Automation** (pp. 284–289). An essay, not a chapter. Pearson recounts a Maker Faire encounter and argues that synthesizers have always been folk instruments, never job-replacement machines. Closes with: "Deep down, we really truly believe that instrument building is an important, empowering activity, and that the more experimentation there is in the world, the more aware and compassionate we all become."

**Appendices**

- **A1. The Components of a Classical Synthesizer** (p. 290).
- **A2. Integrated Circuit Information** (p. 296).
- **A3. Part Sourcing** (p. 310).
- **A4. Electronics Formulas to Know** (p. 314).
- **A5. Electronic Music You Should Know** (p. 318).
- **A6. Seventy Great Synth Albums** (p. 324).
- **A7. Glossary** (p. 328).


### 12.2 Chapter-to-feature map: which chapters give you intuition for which parts of our device

If you read the book alongside building our firmware, this table tells you which chapter to read *before* you tackle which part of our device. The intuition transfers even though the implementation is completely different (analog circuits vs. C code). Chapter numbers are Pearson's, not the document's.

| Book chapter | Builds intuition for these parts of our device | Why |
|---|---|---|
| **1. People's History of Electronic Music** | The project as a whole | The book's argument — that synthesizers are folk instruments — is the entire reason this firmware exists. |
| **2. Musical Electricity for Electrophobes** | §3.1 (recording, F-001–F-005), the I²S mic + DAC pipeline, §13.7 (sample rate, voltage = signed 16-bit integer) | The "speaker as microphone" symmetry maps 1:1 to our mic + DAC pipeline. Understanding voltage as a number is essential for understanding PCM audio. |
| **3. Hello World Oscillator** | §3.5 (effects F-019), §3.4 (F-016 tweak Tone), §3 (all waveform/oscillator references) | When the book says "oscillator", our firmware says "amy_event with `wave = SAW` or `wave = SINE`". The four variations (Pinch-O-Matic, Food, Pencil-Pusher, Photo Theremin) map to velocity, recording-from-mic, raw synthesis, and Knob-A-as-LDR respectively. |
| **4. Amps, Reverbs, and Talkboxes** | §3.5 (effects F-019), F-022 (volume level), the global reverb | The plate reverb the book builds is a *physical* reverb (a metal plate vibrating). AMY's reverb is a *digital* one (a buffer of samples fed back). Same effect; the math is nearly identical. |
| **5. Soldering, Enclosures, and UI** | §6 (the eight TFT screens), the 4×4 button matrix design | The book chapter on UI is about how buttons, knobs, and lights make an instrument feel real. Our equivalent is the four-by-four button matrix and the TFT. Same design problem. |
| **6. Chaining Oscillators** | §3.1 (F-005, melodic vs drum slots), §3 (polyphony), F-019 STUTTER_4 and LOOP_16 | Polyphony in our firmware is four oscillator instances per melodic slot (analog: chained 555s). STUTTER_4 is exactly the gating-oscillator variation. LOOP_16 is exactly the Vactrol Arpeggiator. |
| **7. Schematics and Mass Transit** | §3 (reading our block diagrams), §11.5 (the side-by-side table) | Schematics literacy → block-diagram literacy. The vocabulary transfers. |
| **8. Filters** | §3.4 (F-017 tweak Filter), §4.17 (`PO33_FX_FILTER_SWEEP`), F-019's filter punch-ins | When the book talks about "low-pass filter", AMY's `filter_type = FILTER_LPF` does the same thing in math. The book's active filters with the LM741 = our `filter_freq` + `filter_resonance` knobs. |
| **9. Harmonization** | §3.4 (F-016 tweak Tone Knob A = pitch), §3.5 (effects F-019), AMY's FM synth | "Harmonize with yourself" = play the same note at two pitches; we do it with `note 9 60` then `note 9 72`. The FM Yodeler maps to AMY's `mod_source` / `mod_target` fields. |
| **10. Modulation** | §3.2 (sequencing, F-006–F-012), F-019 modulation effects, F-031 (sync in/out) | The book's envelope generator (a circuit that shapes a sound over time) is the conceptual ancestor of our step sequencer. The step sequencer is a modulator that turns sounds on and off at exact moments. |
| **11. Sequencers** | **§3.2 entirely**, §6.4 (pattern-edit screen), §6.6 (tweak screen), §3.13 (chain F-013/14), §3.7 (BPM F-020) | **The book's chapter on sequencers is the closest analog to half of our firmware.** A step sequencer in hardware (a row of knobs and switches) and a step sequencer in code (a 16-element array of slots and notes) do the same thing. The Standard Eight-Step Sequencer is exactly our pattern. Read this chapter before reading our §3.2. |
| **12. Electronic Percussion** | §3.1 (F-005, drum slots), §3.2 (F-006 play a sound), §6.2 (sound-select screen) | The book's drum machine uses analog circuits to generate kick / snare / hat waveforms. Our device *records* real-world drum hits into slots and plays them back. The book teaches you what a "kick" actually is, in the analog sense; our firmware teaches you how to record one. |
| **13. Phase-Locked Loops** | F-031 (jam-sync IN), F-032 (5 sync modes, deferred v2) | PLLs are how analog synths tune oscillators. We don't need them because AMY's oscillators are digital and already in tune. The 1-pulse-per-step sync is the only PLL analog we ship. |
| **14. Dogbotophone MK1** | **The whole project.** | Our device is a small, cheap, digital Dogbotophone. The Dogbotophone's three sequencers = our chain; the Dogbotophone's six voices = our drum + melodic slots; the Dogbotophone's mixer = AMY's voice allocator; the Dogbotophone's active filters = our tweak Filter + FILTER_SWEEP FX. After reading this chapter, the architecture of our firmware (sample pool + sequencer + effects) is obvious. |
| **15. Thoughts on Automation** | The politics of our project (the entire vibe) | The closing essay. Pearson argues synths are folk instruments; we argue the same by emulating one on open hardware. |


### 12.3 Recommended reading order

There is no single right way to read the book. The order below matches how our document is structured, so the book and the doc reinforce each other. Chapter numbers are Pearson's, not the document's.

1. **Before you do anything else**, read the book's introduction and skim chapters 1 ("A People's History") and 2 ("Musical Electricity"). The introduction sets the philosophy. Chapter 2 gives you the vocabulary. Chapter 1 motivates why this firmware exists.
2. **Before you record a sample** (our §3.1, F-001), re-read the book's chapter 2 closely, especially the "Speaker as Microphone" and "A Moment of Clarity" sections. You will then understand *why* a microphone produces numbers instead of just sound.
3. **Before you read our §3.5 (the 16 punch-in effects)**, read the book's chapters 3 ("Hello World Oscillator"), 4 ("Amps, Reverbs, Talkboxes"), 8 ("Filters"), 10 ("Modulation"), and 12 ("Electronic Percussion"). You will then know what those words *do*, which makes our effect list intelligible.
4. **Before you read our §3.2 (sequencing)**, read the book's chapter 11 (sequencers) end-to-end. The book chapter is short but it grounds every term in our doc: step, pattern, loop, fill, retrigger. Chapter 11 is the most directly applicable chapter in the entire book.
5. **Before you read our §3.1 on drum slots**, read the book's chapter 12 (electronic percussion). You will learn what a "kick" actually is, in the analog sense.
6. **After you have the device working**, read the book's chapters 6 (chaining oscillators), 9 (harmonization), and 13 (PLLs). These give you deeper intuition for what our polyphony (F-005), FM synth, and jam-sync (F-031) are doing mathematically.
7. **Optional but inspiring**, read the boss chapter (14, Dogbotophone MK1). It will show you what a serious home-built instrument looks like. Our device is a baby Dogbotophone.
8. **Read last**, the concluding chapter (15, Thoughts on Automation) and the appendices. These are not technical; they are about why any of this matters.

You do not need to *build* any of the book's circuits to benefit from reading it. Even just reading the chapter text and looking at the photos builds the mental model. If you do want to build along, the book has a companion kit (the "Dogbotic Labs DIY Synth Kit") that contains all the parts you need for the first several chapters.

For a more detailed walk-through with concrete UART commands on our firmware, see `docs/MAKERS_COMPANION.md`. That companion book covers every chapter of Pearson's book with hands-on exercises you can run on a flashed device.

### 12.4 What the book does NOT cover — and our project does

Pearson's book is firmly **analog-first**. It does not teach:

- **Digital audio**: PCM, sample rate, bit depth, quantization. All of which our §2 and §3 explain.
- **Programming**: C, microcontrollers, ESP-IDF, the AMY library. Our §3 is where to start.
- **Samplers** in the digital sense: loading PCM files, pitch-shifting samples, slicing. Our §3.1, F-001 through F-005 covers this.
- **MIDI**: the universal protocol for music gear to talk to each other. Our §3.10 (jam sync) is a poor-man's version; MIDI would be richer. (MIDI is on the v2 roadmap.)
- **Firmware, storage, file systems**: how to save a pattern, how LittleFS works. Our §3.13 (factory reset + UI) and §3.10 (sync) cover this.
- **The PO-33 specifically**: this book is about *inventing* your own instrument, not about emulating someone else's. Our document is the opposite: it's about replicating a specific existing product.

If the book is the question "how do electronic instruments work?", our project is the question "how do I make a specific electronic instrument I already love?". Reading both gives you a more complete answer than either alone.

For the inverse — what our firmware does that the book doesn't — the answer is mostly **persistence** and **multi-device coordination**: our firmware saves patterns to flash, syncs over jam-sync cables, runs an alarm clock, monitors battery voltage, and ships a CLI over USB. None of these have analog equivalents in Pearson's book because they require a digital system.

### 12.5 Side-by-side concepts: analog → digital

For the layman reader, this is probably the most useful table in the whole document. Each row takes a concept the book teaches in hardware and shows the digital equivalent in our firmware. The point is not that they're identical — they're not — but that *the vocabulary transfers*.

| Concept in the book (analog) | Equivalent in our project (digital) | Where in our code |
|---|---|---|
| Voltage | A 16-bit integer sample value (-32768 to +32767) | `main/audio/amy_bridge.c` sample pool |
| Oscillator circuit (chapter 2) | An AMY oscillator event with `wave = SAW` or `wave = SINE` | `main/audio/amy_bridge.c` → `apply_fx()` |
| Filter circuit (chapter 7) | AMY's `filter_type = FILTER_LPF` and `filter_freq = 200..700` | `main/audio/amy_bridge.c` → `apply_fx()` → `PO33_FX_FILTER_SWEEP` |
| Envelope generator (chapter 9) | AMY's breakpoint envelope on each event (`bp0` parameters) | AMY `amy_event.bp0_times`, `bp0_values` |
| LFO (chapter 9) | AMY's low-frequency oscillator modulation source (`mod_source`) | AMY `amy_event.mod_source[]` |
| Step sequencer (chapter 10) | Our 16-element `pattern_t.steps[]` array | `main/sequencer/pattern.h` |
| Drum machine (chapter 11) | Our 8 drum slots + the `amy_bridge_play_note` trigger | `main/audio/amy_bridge.c` |
| Plate reverb (chapter 3) | AMY's delay-based reverb (`cfg.ram_caps_delay = MALLOC_CAP_SPIRAM`) | `main/audio/amy_bridge.c` → `amy_config_t` |
| "Voltage-controlled" anything | A parameter in an `amy_event` struct | `components/amy/src/amy.h` `amy_event` |
| Patch cables on a modular synth | The arguments to `amy_add_event()` | `main/audio/amy_bridge.c` |
| Speaker cone vibrating | The I²S DAC (PCM5102A) outputting an electrical waveform | `main/audio/i2s_driver.c` (now `amy_bridge.c` since the AMY integration) |
| Knob on the front panel | A button combo on our 4×4 matrix, or a UART shell command | `main/ui/buttons.c`, `main/main.c` shell |
| LED on the front panel | A region of the TFT screen | `main/ui/display.c` |
| Power supply (battery) | USB-C 5 V or LiPo via the optional TP4056 charger | `main/config.h` `BATTERY_*` |

Reading the right column first makes the left column obvious. Reading the left column first makes the right column feel familiar. Pick whichever order works for your brain.

### 12.6 The book's philosophy, applied to our project

The book's introduction closes with this line, which I want to quote because it is the closest thing to a mission statement the author gives:

> "Deep down, we really truly believe that instrument building is an important, empowering activity, and that the more experimentation there is in the world, the more aware and compassionate we all become."

Applied to our project: building firmware that emulates a beloved existing product is a slightly different act from building a brand-new instrument, but it shares the same spirit. To make our firmware, you have to understand what makes the PO-33 special — the immediacy of recording, the constraint of 16 slots, the way 16 steps force you into a particular kind of rhythm, the way 16 punch-in effects turn every performance into a small surprise. You are not just transcribing code; you are reverse-engineering a *design philosophy* and rebuilding it on cheaper, more open hardware.

If you read the book and feel the urge to make your own instrument instead of (or in addition to) this one, that is the correct reaction. Our firmware is one path; the book is the door to a hundred others.

---

## 14. Wi-Fi, web binding, and sketch upload (community integration)

> **Why this section exists.** Sections 1–13 describe the *device* in isolation: the ESP32-S3 PO-33 remake, its recorder, sequencer, screen, and on-flash storage. This section extends that picture to cover the *optional* web community: how the device pairs with a user account on a website, how sketches get uploaded from the device to the cloud, and how that sketch is later edited and published by the user. None of this is required for the device to work as a standalone Pocket Operator — it only matters to owners who want to share their work on the web community. Conventions used in this section follow the terminology block at the top of the document (immediately under the H1).

### 14.0 Scope and non-goals

In scope:

- Wi-Fi provisioning flow on the ESP32-S3 PO-33 remake.
- Binding the device to a user account using **RFC 8628 — Device Authorization Grant**.
- Sketch upload: payload format, Ed25519 mandatory signing, the canonical-JSON rules the firmware and server agree on byte-for-byte, and the on-device key store.
- Lifecycle of an uploaded sketch on the server: **draft / published**, including private-by-default.
- Unbinding and admin revocation, including history preservation.

Explicit non-goals:

- The website's authentication system, its database schema, its UI, or its API beyond the surface the device needs. Those live in the website repo.
- An audio streaming or low-latency live-audio playback protocol. Sketch uploads are file transfer; the device does not act as a network audio source.
- A sync protocol to the original PO-33. That remains the §3.10 (jam sync) path.

### 14.1 Who needs to bind a device — and who does not

Binding a device is **optional** for community membership. The web community does not require a device at all, and it does not require that *this* user owns a remake.

A user account can do everything listed below **without ever binding a device**, by interacting only with the website:

- Register and sign in (email + password, OIDC, etc.).
- Browse, search, and listen to other members' published sketches.
- Comment, like, collect, and follow other members.
- Download sketches as files, then transfer them to a device by the means described in §3.9 (USB-MSD, or the future data-transfer protocol).
- Manually upload a sketch file from the laptop, signed by an *external* key generated on that laptop.

A user account needs to **bind a device** only when the *device itself* must upload sketches directly — i.e. when the user wants to press "Upload to community" on the TFT and have the sketch land in their account on the website without going through a laptop. A user with an **original Teenage Engineering PO-33** therefore has no need to bind anything; they participate as a regular community member.  [ADDED: clarifies that binding is optional and reserved for device-initiated uploads; original-PO-33 owners and device-less users are full citizens of the community.]

### 14.2 Cardinality

The binding relationship is **one user ↔ many devices**, **one device ↔ one user**:

- A single user account may bind **N ≥ 1** ESP32-S3 PO-33 remake units. There is no per-user cap in v1. Each device gets its own device code, its own device_id, its own Ed25519 keypair, and its own access_token.
- A single device is bound to **at most one** user account at any moment. The (device_id, owner_user_id) pair is unique.
- **Rebinding** (moving a device from one account to another, including the same account logging out and a different account logging in on the *same* device) requires an explicit unbind or an admin revoke first. The device refuses new binding requests while it holds a valid access_token from any user.

The cardinality is enforced on the server. The device stores only its current `binding_state ∈ {IDLE, BOUND}` and, when bound, its `access_token` and `owner_user_id`; it does not need to know about other devices on the same account.  [ADDED: cardinality subsection — 1 user → N devices, 1 device → 1 user, rebind requires unbind.]

### 14.3 Binding protocol — RFC 8628 Device Authorization Grant

The device binds to a user account using the **OAuth 2.0 Device Authorization Grant** defined in **RFC 8628** ([datatracker.ietf.org/doc/html/rfc8628](https://datatracker.ietf.org/doc/html/rfc8628)). The grant is the right one for input-constrained devices because the device has no keyboard and no browser — only a small screen and a few buttons — so a redirect-based flow is impossible.

#### 14.3.1 State machine

The device's binding state machine is closed and finite:

```
                  ┌────────────┐
                  │ DEVICE_IDLE│
                  └─────┬──────┘
                        │ user triggers "Bind device"
                        │ from the TFT menu
                        ▼
            ┌───────────────────────┐
            │ AUTHORIZE_REQUEST     │
            │  POST /oauth/device   │
            │  /authorization       │
            └─────┬─────────┬───────┘
                  │         │
       4xx err ◄──┘         └──► 200 {device_code,
                                    user_code,
                                    verification_uri,
                                    verification_uri_complete,
                                    expires_in,
                                    interval}
                  │
                  ▼
            ┌──────────────────┐
            │  SHOW_QRCODE     │
            │  render QR + show│
            │  user_code as    │
            │  fallback text   │
            └─────┬────────┬───┘
                  │        │
        user ◄────┘        └──► user_code typed in
        scans QR               browser by hand
        on phone                  │
                  │              │
                  ▼              ▼
            ┌──────────────────────────┐
            │       POLLING            │
            │ POST /oauth/token        │
            │  grant_type=urn:ietf:params:oauth:grant-type:device_code
            │  device_code=…           │
            │  (respect `interval`)    │
            └─────┬───────────┬────────┘
                  │           │
                  ▼           ▼
              BOUND         IDLE
        store access_token  discard codes
        POST /v1/devices/   reset state
        public-key (see
        §14.4)
```

The five end states the firmware cares about are explicitly: **`DEVICE_IDLE`**, **`AUTHORIZE_REQUEST`**, **`SHOW_QRCODE`**, **`POLLING`**, **`BOUND`**. Backoff transitions during polling are part of `POLLING` and do not change the state label.  [ADDED: RFC 8628 binding flow with explicit state machine DEVICE_IDLE → AUTHORIZE_REQUEST → SHOW_QRCODE → POLLING → BOUND/IDLE.]

#### 14.3.2 QR code — primary path

While in `SHOW_QRCODE`, the device renders the **`verification_uri_complete`** (an RFC 8628 extension that already encodes the `user_code` into the URL) as a **QR code** on the 240×320 TFT. The user scans the code with the phone's camera, which opens the URL with the code pre-filled; the phone then asks the user to sign in and approve the device. This is the primary path because it eliminates typing on the phone — the user only has to tap "approve" once.  [ADDED: QR-code rendering on the TFT is the primary binding path.]

#### 14.3.3 Manual `user_code` entry — fallback

If the user's phone cannot scan the QR code (broken camera, screen damaged, on a desktop), the same TFT screen also displays the human-readable **`user_code`** as text. The user opens the bare **`verification_uri`** in any browser, types the `user_code`, and continues. The fallback is a documented degradation, not an error path.  [ADDED: manual user_code fallback explicitly documented alongside QR primary path.]

#### 14.3.4 Polling error codes

While in `POLLING`, the device POSTs to `/oauth/token` every `interval` seconds (default 5; the server may lengthen it via the `slow_down` response). The firmware must handle exactly these four RFC 8628 error responses:

| `error` value | Meaning | Device action |
|---|---|---|
| `authorization_pending` | The user has not yet approved; keep polling. | Continue at the current `interval`. |
| `slow_down` | The client is polling too fast. | Back off — set `interval := max(interval, min_interval) + jitter` (RFC 8628 §3.5 recommends ≥ 5 s extra). Then continue polling at the new interval. |
| `access_denied` | The user rejected the request at the website. | Transition to `DEVICE_IDLE`, show "Binding cancelled", discard `device_code` and `user_code`, return to the menu. |
| `expired_token` | The `device_code` has expired (server's `expires_in` passed). | Transition to `DEVICE_IDLE`, show "Code expired — try again", delete the codes. The user can re-trigger binding from the menu. |

Any other HTTP error (5xx, network unreachable) is retried with exponential backoff up to a cap of `interval * 6`, after which the device returns to `DEVICE_IDLE` without surfacing an error.  [ADDED: explicit polling error-code table for `authorization_pending`, `slow_down`, `access_denied`, `expired_token`.]

#### 14.3.5 Successful binding — what is stored where

On HTTP 200 from `/oauth/token`, the server returns an `access_token` and a `refresh_token`. The device stores, in encrypted NVS under namespace `bind`:

- `access_token` — opaque bearer.
- `refresh_token` — used to obtain a new `access_token` before expiry.
- `expires_at_unix` — UTC expiry of the current access_token.
- `owner_user_id` — opaque server-issued user identifier (also used as the publish target for sketches).

The **Ed25519 keypair** the device uses for signing uploads is *not* created or registered here. Key generation happens at first boot (see §14.4.1) and is *registered* with the server in the same online transaction that follows binding: the device issues `POST /v1/devices/{device_id}/public-key` with the base64 public key, and the server records it under the binding. If the public-key registration fails, the binding is rolled back to `DEVICE_IDLE`.

### 14.4 Ed25519 signing — mandatory on every upload

Every sketch upload **must** carry an Ed25519 signature over the canonical metadata JSON. Signatures are verified by the server before the sketch is accepted into the user's account, even in `draft` status. There is no unsigned-upload path. This is mandatory — it is not optional and not "soft".  [UPDATED: signing is mandatory on every upload; the public key is registered at the server during binding.]

#### 14.4.1 Key generation and storage

- On **first boot**, if no Ed25519 keypair is present in NVS, the device generates one inside the ESP32-S3 using `mbedtls`. The Ed25519 algorithm is fixed; we do not support RSA or ECDSA P-256 in v1.
- The **private key** is stored in encrypted NVS under namespace `crypto`, key `ed25519_priv`. Encryption uses the ESP32-S3 flash encryption key (enabled in `sdkconfig.defaults`); if the board has been deployed with secure-boot v2 and flash encryption, the private key is also wrapped by the hardware key manager. An alternative acceptable in v1 is to **burn the private key into eFuse BLOCK_KEY** with `efuse_key_write` and disable software readback.  [UPDATED: Ed25519 private key is generated on first boot and stored in encrypted NVS (or eFuse).]
- The **public key** is the Ed25519 verification key, base64-encoded (uncompressed, 32 raw bytes → 44-char standard base64 with `=` padding), and is registered with the server during the binding flow (§14.3.5).  [UPDATED: public key is registered with the backend during binding.]
- The device never sends its private key to the server. It never displays the private key on the TFT. It never writes the private key to LittleFS or to UART logs.

#### 14.4.2 Canonical JSON — the rules

Because the firmware (C, on a microcontroller) and the server (whatever language runs the website) both compute the signature, they must agree **byte-for-byte** on the bytes being signed. The signed input is the canonical JSON serialization of the sketch metadata, with the following rules. Servers and firmware MUST pin to this exact list; deviations are bugs, not preferences.  [ADDED: canonical JSON rules so firmware and server sign and verify byte-for-byte.]

1. **Encoding.** UTF-8, Unicode Normalization Form **NFC** (no smart quotes in any other form, no combining marks apart from their precomposed equivalents).
2. **Object key ordering.** Keys are sorted **lexicographically** by their raw UTF-8 bytes. No reordering across renderers.
3. **No insignificant whitespace.** No spaces, tabs, or newlines between tokens. The canonical encoding is one continuous string.
4. **Numbers.** JSON numbers only. No comments. Integers without leading zeros; doubles in plain decimal, **no scientific notation**, **no trailing zeros** that change the value (`1.0` is not `1`; both are explicit). The values `NaN`, `Infinity`, and `-Infinity` are forbidden — represent them as the string `"NaN"` if a sketch needs to flag them.
5. **Strings.** Minimal JSON escaping: the seven escapes `\`, `"`, control codes `\u0000…\u001F`, plus any Unicode codepoint that must be escaped for safety. Non-ASCII printable characters are passed through as UTF-8 bytes — **do not** `\u`-escape them.
6. **Field set.** The metadata object carries exactly the fields listed in the canonical schema below. New fields require a schema version bump (§14.4.3).

Canonical sketch-metadata schema (v1):

```json
{
  "schema": 1,
  "sketch_id": "a1b2",
  "device_id": "<server-issued opaque>",
  "captured_at_unix": 1739635200,
  "slots": [{"id": 0, "name": "kick",  "samples": 88200, "rate": 44100},
            {"id": 1, "name": "snare", "samples": 66150, "rate": 44100}],
  "patterns": [{"index": 0, "steps": 16, "notes": "0100…"}],
  "chain":     [{"pattern": 0, "repeats": 4}],
  "title":      "Untitled Sketch",
  "visibility": "private"
}
```

`visibility` is one of `"private"` (the only value the device may write in v1; the server's web UI is the only thing that can flip it to `"public"` after publish). `schema = 1` lets us add fields later without breaking old signatures. The signed bytes are exactly the bytes of that JSON object — no prefix, no suffix, no trailing newline. The Ed25519 signature is computed over those bytes and transmitted as standard base64 (88 chars + `=` padding for the 64-byte raw signature).

#### 14.4.3 Re-binding and key rotation

A re-bind (after an unbind, §14.5) does **not** invalidate the existing keypair. The device retains the same `ed25519_priv`, and the server may keep the registered public key — sketches that were signed under it must still verify after a rebind. **Key rotation** is out of scope for v1: a key is only ever replaced by a factory reset (§3.13).

### 14.5 Unbinding and revocation

A binding can end in three ways. In every case the device's `access_token` becomes invalid, and the server-side history is preserved.  [ADDED: web-side unbind by the owning user, plus admin revoke for lost/stolen devices, plus sketch-history preservation with a fingerprint snapshot.]

#### 14.5.1 Device-initiated unbind

The user, on the device, picks **"Unbind this device"** from the TFT menu. The device POSTs to `DELETE /v1/devices/{device_id}/binding` with a valid access_token. On 2xx, the device clears `access_token`, `refresh_token`, `expires_at_unix`, and `owner_user_id` from encrypted NVS, and transitions to `DEVICE_IDLE`. The server keeps the device record, the public key, and the sketch history. The owner can rebind at any time.

#### 14.5.2 Web-side unbind by the owning user

The owning user signs into the website, opens the device list, and clicks **Unbind** on this device. The server invalidates the access_token on the spot. The next time the device attempts an upload or a refresh, it receives **HTTP 401 or 403**, and the firmware surfaces the message **"This device has been unbound from its account"** on the TFT and offers to *bind again*. The device does **not** silently retry. The device's local copy of `access_token` etc. is cleared on the 401/403 so the next boot starts at `DEVICE_IDLE`.

#### 14.5.3 Admin revoke — lost or stolen devices

If the owner reports a device lost or stolen (or the device is registered as stolen by another party), an admin issues `POST /v1/admin/devices/{device_id}/revoke`. The server records the revocation with the admin's user_id and a timestamp, marks the device record `state = revoked`, and **invalidates the access_token**. The device's public key is **not** deleted — it stays in the audit trail — but all sketch history is preserved with a final snapshot of the device fingerprint (§14.5.5). If the device later comes online, it sees the revocation on its first upload attempt and behaves as in §14.5.2.

#### 14.5.4 Rebind after unbind / revoke

Rebinding requires **either** the §14.5.1 (device-initiated), §14.5.2 (web-side) unbind **or** the §14.5.3 (admin revoke) to have completed first. The server refuses a new device-authorization request from a still-bound device. This enforces the §14.2 cardinality rule on a single device.

#### 14.5.5 Sketch-history preservation with fingerprint snapshot

On every unbind or revoke, the server takes a snapshot of:

- `device_id`
- `device_fingerprint` (the SHA-256 of the device's first-boot serial + hardware MAC + public-key fingerprint, computed deterministically — see §14.6)
- `owner_user_id` at the moment of unbind / revoke
- the timestamp
- the reason (`user_unbind` / `admin_revoke` / `lost_stolen`)

The snapshot is written to the server's `sketch_history.fingerprint_snapshot` column for every sketch the device ever uploaded. Sketches are **never** deleted by unbind or revoke. They remain visible to the owner (and to admins under §14.5.3) so that a stolen device cannot be used to silently remove evidence of what it had uploaded before the loss was noticed.

### 14.6 Device fingerprint

The fingerprint the server snapshots is computed deterministically at boot:

```
fingerprint = SHA-256(
    serial                ||   // first-boot NVS "serial" key, 16 random bytes, base64
    hardware_mac          ||   // ESP32-S3 eFuse MAC, 6 bytes, big-endian
    pubkey_sha256         ||   // SHA-256 of the registered Ed25519 public key, raw bytes
    firmware_git_sha              // 32-char hex, populated by the build system
)
```

The fingerprint is not a secret, but it is what binds a sketch to "this exact unit, with this exact firmware build, with this exact key". If any of the four inputs changes (new serial, new key after factory reset, new firmware build), the fingerprint changes and the new fingerprint is recorded against all subsequent uploads.  [ADDED: device fingerprint definition supporting the §14.5.5 snapshot.]

### 14.7 Sketch upload lifecycle — drafts are private by default

Uploads from the device **do not** publish sketches. Every device upload lands as **`status = draft`, `visibility = private`** in the owner's account on the website. The user opens the website, reviews the draft, edits the title and slot names, drags it into a "space" (a folder / collection on the website), and only then clicks **Publish**. The TFT never has the power to publish — it can only initiate the upload that creates the draft.  [UPDATED: device uploads land as `status=draft, visibility=private`; publishing is a web-only action.]

State transitions the server-side pipeline applies in order:

1. `BOUND` device uploads signed payload → `RECEIVED`.
2. Server verifies the Ed25519 signature against the registered public key → `SIGNED_OK` or `SIGNED_BAD` (reject, 4xx).
3. Server deserializes the canonical JSON and runs invariants (slot count ≤ 16, sample rate in the supported set, etc.) → `VALIDATED_OK` or `VALIDATED_BAD` (reject, 4xx).
4. Server writes the rows + payload to blob storage, status `draft`, visibility `private`, attaches the fingerprint snapshot from §14.5.5. → `DRAFT_VISIBLE`.
5. The owner edits the draft via the website, then hits **Publish**. Status → `published`, visibility → `public` (if the user picked public), or stays `private` (if the user picked a "private collection"). → `PUBLISHED`.

If step 4 fails (blob storage down, etc.), the server returns 5xx; the device surfaces **"Upload failed — try again later, the sketch is still saved on this device"** and leaves the draft on the device so it can retry without re-recording.

### 14.8 Upload payload format and size limits

A sketch upload is a **multipart/form-data** body with three required parts and one optional part, in this order:  [UPDATED: payload is a binary blob plus optional WAV sidecars; explicit size limits — `backup_file` ≤ 50 MB, each `sample_wav` ≤ 10 MB.]

1. **`metadata`** (`application/json`) — the canonical-JSON object from §14.4.2, byte-for-byte. This is the bytes the Ed25519 signature covers.
2. **`signature`** (`text/plain`) — the standard base64 Ed25519 signature (64 raw bytes → 88-char base64 with `=` padding) of `metadata`.
3. **`backup_file`** (`application/octet-stream`) — the **binary sketch blob** containing samples, patterns, chains, and any other serialized state. The blob is a LittleFS tarball (or equivalent) of the sketch folder that §11.2 already defines for the on-flash sketch storage format.
4. (Optional) **`sample_wav`** — one or more optional **`audio/wav`** sidecars, one per slot, attached so the website can play back samples in a browser without reimplementing the LittleFS sample decoder.

Size limits (enforced by the server, with helpful 413 errors; the firmware pre-checks to avoid wasting bandwidth):

| Part | Limit |
|---|---|
| `backup_file` (the whole blob) | **≤ 50 MB** |
| Each `sample_wav` sidecar | **≤ 10 MB** |
| Total request body | **≤ 64 MB** (sum of all parts + multipart overhead) |
| `metadata` JSON | **≤ 16 KB** (this is a metadata object, not the sketch) |

The bytes the firmware signs are *only* the `metadata` part. The server verifies the signature first, then independently checks the size limits, then unpacks the `backup_file` and the optional `sample_wav`s. There is no signature over the binary blob in v1; the binary blob is protected indirectly because the metadata references the `sketch_id` and a content hash of the blob is included in a future schema version (out of scope here).

### 14.9 Security notes specific to this section

- **All** network calls in this section use TLS 1.2 or later. The firmware pins the server's leaf certificate by SHA-256 at provisioning time; pins are stored in encrypted NVS. A compromised CA does not let an attacker impersonate the server.
- The TFT only displays the `verification_uri` and `user_code` during `SHOW_QRCODE`. It never displays the `access_token`, `refresh_token`, or `device_code` (they would be useless on the screen anyway, but the rule is stated for the record).
- The `device_code` is single-use and expires server-side after `expires_in` (default 600 s in v1). The firmware starts a watchdog timer when entering `POLLING` and discards the codes at expiry (§14.3.4).
- The Ed25519 private key is **never** copied out of the device, **never** logged, and **never** rendered on the TFT.
- A factory reset wipes both the `crypto` namespace and the `bind` namespace, forcing key regeneration and a fresh binding on next boot.

### 14.10 Open questions deferred to a future revision

- Token refresh on long-lived devices. The v1 path is "rebind if the access_token's expiry approaches and refresh fails"; v2 will add `refresh_token` rotation and reuse-detection.
- Per-sketch signing keys, vs. one device-wide key. v1 is device-wide.
- A web-side flow that lets a user *replace* a draft with a new upload from the same device. The semantics (keep history vs. replace) are not decided yet.

## 15. Peer-to-peer sketch exchange over ESP-NOW

[ADDED: §15 is a new top-level section that designs the face-to-face, no-internet sketch-exchange feature for two PO-33 K.O! devices. It complements §14 (which covers the Wi-Fi–backed, server-mediated upload path) and does not replace §9's planned audio-cable transfer (F-028).]

### 15.0 Why this section exists

The PO-33's §9 transfer is a 3.5 mm audio cable plus a handshake that **erases the receiving unit** (see F-028). That is fine for a one-time back-up, but it is the wrong tool for the most common sketch-sharing case: two friends, side by side, who have just made beats they want to swap. The cable is fiddly, the erase is scary, and there is no preview of what is about to be overwritten.

The ESP32-S3's 2.4 GHz radio is already on the chip (see the §16 *WiFi* glossary entry and §14's opt-in model). ESP-NOW is a connectionless, peer-to-peer protocol that Espressif ships in the ESP-IDF WiFi stack: no router, no access point, no internet, no handshake to a server, no IP layer, latency measured in single-digit milliseconds, range of ~30 m line-of-sight / ~10 m indoors. It is the right primitive for "device A pushes a sketch to device B that is in the same room." This section designs that feature.

> **Out of scope (v1 of this section).** Server-mediated upload is in §14. The audio-cable transfer (F-028) and the tape-recorder backup (F-029) are still in §3.9 and are not replaced by this section. Streaming a *live* performance over ESP-NOW (jam-session mode) is a future-revision feature; §15 only ships the sketch-blob transfer.

### 15.1 Goals and non-goals

**Goals**

1. **Push a sketch from one device to another, no infrastructure.** No Wi-Fi network, no internet, no phone, no USB cable, no audio cable, no server account.
2. **Non-destructive on the receiver.** The receiver's existing sketches are untouched. The new sketch arrives as a new folder under `/sketches/<id>/`, with `sketches.lst` updated atomically (same atomic-rename discipline as §11).
3. **Non-destructive on the sender.** The sender's active sketch is read but not modified. The same sketch can be re-sent to as many receivers as the user wants.
4. **Confidential on the air.** A 6-digit pairing code entered on the receiver derives an AES-128 session key. The radio payload is encrypted; an eavesdropper who captures the packets cannot decrypt them.
5. **Resumable in the face of ordinary RF loss.** A missed packet is retransmitted; a 5-second lull does not abort the transfer; a receiver that briefly walks out of range and back in does not lose its place.
6. **Happens in seconds, not minutes.** A typical sketch (16 patterns, ~5 s of samples, 1 short chain, a few KB) fits in one 250-packet burst and completes in well under 5 seconds at the default 1 Mbit PHY. A "maxed out" sketch (16 patterns, a ~40 s sample pool, a 128-entry chain) takes 30–60 seconds.

**Non-goals (v1)**

- **Group transfer** (1 sender → N receivers simultaneously). v1 is strictly 1:1. Group transfer is a §15.11 deferred item.
- **Selective slot transfer.** v1 sends the *whole* sketch. Picking "send only slot 3 and pattern 7" is a v2 deferred item.
- **Cross-radio transfer** (Bluetooth, USB-MIDI file dump). §15 is ESP-NOW only.
- **Live jam-session streaming** (audio or MIDI over ESP-NOW). See §10 wishlist #1 (WiFi Ableton Link) and §10 wishlist #3 (BLE MIDI); this section does not subsume those.

### 15.2 Threat model

The §14 server-mediated path has a server, accounts, and Ed25519 signatures, and the threat model is correspondingly rich. The §15 peer-to-peer path has **none of that**: no server, no accounts, no PKI, no long-lived identity beyond the device's MAC. The threat model is therefore narrower and the protections match.

**In scope (this section defends against):**

| # | Threat | Defence |
|---|---|---|
| T1 | **Passive eavesdropper** within radio range captures the packets. | ESP-NOW payload is encrypted with **AES-128-CCM** using a session key derived from the 6-digit code via **PBKDF2-HMAC-SHA256** (4096 iterations, 96-bit salt = sender MAC ‖ receiver MAC). An eavesdropper without the code has ~2^20 candidate keys (6 decimal digits) and the cost of each PBKDF2 attempt is ~10 ms on commodity hardware; the effective search space is **~10⁵ seconds per key** on a 2020-era laptop, which is fine for casual eavesdropping and useless for an active attacker with a one-hour window. |
| T2 | **Active attacker impersonates the sender** ("man in the middle" with a stronger radio). | The receiver must enter the code, which the attacker does not know. The PBKDF2 + MAC salt binds the session key to *both* devices' MACs, so a replay with a different MAC is rejected by the CCM MIC. |
| T3 | **Active attacker impersonates the receiver** to exfiltrate a sketch. | Same as T2: the receiver must display the code on its TFT, and the sender's firmware does not begin transmitting until the sender has *visually confirmed* the receiver's MAC on the TFT ("Send to: `AA:BB:CC:DD:EE:FF`? — Knob A: yes / no"). A blind attacker cannot fake the receiver side of this. |
| T4 | **Replay of an old capture** in a later session. | The first 4 bytes of every ESP-NOW payload are a **session nonce** (random, generated by the sender at the start of each transfer). The receiver rejects any packet whose nonce does not match the open session. Old captures cannot be replayed into a new session because the new session has a new nonce. |
| T5 | **Sketch-tampering in flight** (a packet is modified en route). | CCM's integrity check rejects modified packets at the receiver before the sketch blob is written. Modified packets are silently NACKed; the sender retransmits. |

**Out of scope (this section does not defend against):**

- **Compromise of either device's flash** (an attacker with physical access can read the sketch off the device's flash regardless of how it was transferred in). This is the §11-style "sketch is plaintext on the device" baseline; §15 does not change it.
- **A global passive adversary with multiple receivers placed in advance** who captures packets from every device in a city. The 6-digit code's entropy is the binding, and it is short by design (it has to be typeable on a 16-key matrix keypad). If the threat model is nation-state, the design fails — but the v1 feature is "two friends at a table", not "two devices in a hostile environment." A v2 design with 8-digit codes and a confirmation button on the sender is a §15.10 deferred item.
- **Denial of service** (an attacker floods the 2.4 GHz band and breaks the transfer). ESP-NOW has no rate control in v1 and would simply fail. The user retries; the transfer either succeeds or surfaces a "Peer timed out" error.

### 15.3 Protocol — overview

A transfer is a small state machine on each side, with a fixed message set over ESP-NOW. The protocol is **asymmetric** (the sender is the *initiator* and the receiver is the *acceptor*; both sides know their role from the moment the user picks **Send sketch** or **Receive sketch** on the menu), **single-master** (the sender controls the flow, the receiver only ACKs), and **stop-and-wait per chunk** (the sender transmits one chunk, waits for an ACK with the same sequence number, transmits the next).

```
SENDER                                       RECEIVER
-------                                      --------
  IDLE                                         IDLE
   │  user: Transfer → Send sketch              │
   ▼                                            │
GENERATING_PAIRING_CODE                         │
   │  generate 6-digit code                     │
   │  show code on TFT                          │
   │  start 30s pairing timer                   │
   │                                            │   user: Transfer → Receive sketch
   │                                            ▼
   │                                         ENTERING_CODE
   │                                            │  user types 6 digits
   │                                            │  derives PBKDF2 key
   │                                            ▼
   │                                         WAITING_FOR_HELLO
   │  ◄─── HELLO (pairing_code, sender_mac) ────│
   ▼                                            │
RECEIVED_HELLO                                 │
   │  user confirms "Send to AA:BB:..:FF?"      │
   │  encrypts session key with PBKDF2          │
   ▼                                            │
TRANSMITTING                                   │
   │  for each chunk_idx:                       │
   │    send CHUNK(chunk_idx, data)             │
   │    wait ACK(chunk_idx) or timeout 50ms     │
   │    on NACK or timeout, retransmit (max 5)  │
   │  on last chunk ACK:                        │
   │  send COMMIT(checksum)                     │
   │  wait DONE                                 │
   ▼                                            │
DONE  ─── user sees "Sent 1.2 MB in 4.3 s"      │
                                               │
                                            RECEIVING
                                               │  for each CHUNK:
                                               │    decrypt + verify MIC
                                               │    append to /sketches/<id>/.tmp/buf
                                               │    ACK(chunk_idx)
                                               │  on COMMIT:
                                               │    verify checksum
                                               │    rename .tmp → real folder
                                               │    update sketches.lst
                                               │    send DONE
                                               ▼
                                            RECEIVED  ─── user sees "Received sketch a1b2 'pocket beat' — 1.2 MB in 4.3 s"
```

The state machine is small enough to live in a single `enum` and a single `switch` in `transfer_protocol.c`. Each state has a 30 s timeout; on timeout, both sides return to IDLE and the partial transfer on the receiver is rolled back.

### 15.4 Packet format

ESP-NOW's maximum payload is **250 bytes** per packet (a hard limit of the protocol, not configurable). The §11 sketch blob is up to **50 MB** (the §14.8 cap, which §15 reuses verbatim). At 250 B per packet, that is up to **200 000 packets** for a worst-case sketch. The transfer is therefore a fragmentation problem, not a "send the blob" problem.

**Wire format** (every packet, all multi-byte fields little-endian):

```
┌──────────────────────────────────────────────────────────┐
│  0   msg_type : 1 byte                                   │
│  1   session  : 4 bytes  (sender's per-transfer nonce)   │
│  5   seq      : 4 bytes  (monotonic per msg_type)         │
│  9   payload  : up to 235 bytes (msg_type-specific)       │
│  ───  ESP-NOW's own 16-byte MAC header + 8-byte CCMP MIC │
│      are added by the radio layer; not counted here.     │
└──────────────────────────────────────────────────────────┘
```

The session nonce is part of the §15.5 PBKDF2 derivation; the receiver checks it on every packet and drops anything that does not match the open session. The seq is per-msg-type (HELLO, CHUNK, ACK, NACK, COMMIT, DONE each have their own counter starting at 0). The 235-byte payload budget is 250 minus the 15 bytes of `msg_type + session + seq`.

**`msg_type` values:**

| Type | Code | Direction | Payload |
|---|---|---|---|
| HELLO | `0x01` | R → S | `pairing_code: 6 ASCII bytes` ‖ `receiver_mac: 6 bytes` |
| HELLO_ACK | `0x02` | S → R | `sender_mac: 6 bytes` ‖ `sketch_size_le: 4 bytes` ‖ `sketch_id: 4 ASCII bytes` ‖ `sketch_name: ≤ 24 bytes, NUL-padded` |
| CHUNK | `0x10` | S → R | `chunk_idx_le: 4 bytes` ‖ `chunk_data: ≤ 230 bytes` (the last chunk may be shorter) |
| ACK | `0x11` | R → S | `chunk_idx_le: 4 bytes` ‖ `received_so_far_le: 4 bytes` (so the sender can render a real progress bar, not just "this chunk landed") |
| NACK | `0x12` | R → S | `chunk_idx_le: 4 bytes` ‖ `reason: 1 byte` (0 = bad MIC, 1 = out-of-order, 2 = buffer full) |
| COMMIT | `0x20` | S → R | `total_chunks_le: 4 bytes` ‖ `sketch_sha256: 32 bytes` (over the full blob) |
| DONE | `0x21` | R → S | (empty) |
| CANCEL | `0x7F` | both → other | `reason: 1 byte` (0 = user, 1 = timeout, 2 = too-large, 3 = flash-full) |

**Chunk sizing.** At 230 B of payload per CHUNK, a 1 MB sketch is ~4 350 packets. At the §15.6 default retry budget of 5 retransmits per chunk, that is a worst-case ~26 000 packet-attempts in the air. A 50 MB sketch is ~218 000 packets, ~1.3 M worst-case attempts. The §15.6 timing budget is built around these numbers.

**Sketch blob reuse.** The on-the-wire blob is **the same format as §14.8's `backup_file`** (the LittleFS tarball of `/sketches/<id>/` — `meta.bin` + `samples.bin` + `patterns.bin` + `chain.bin` from §11.2). No new on-the-wire format is invented here. The CHUNK packets carry the bytes of that blob; the receiver unpacks the same way §14.8's server does. This is deliberate: a sketch that has been ESP-NOWed from device A to device B can then be uploaded to the §14 server with no reformatting, and a sketch downloaded from the §14 server can be ESP-NOWed onward. The blob is the blob.

### 15.5 Pairing, encryption, key derivation

**Pairing flow.**

1. The sender generates a random 6-digit code (`code = (random_uint32 % 900000) + 100000`) and shows it on the TFT. The 6-digit code has **~20 bits of entropy** (log2(900 000) ≈ 19.78), which is the same order of magnitude as a credit-card CVV. This is the human-typed secret.
2. The receiver displays a 6-digit entry screen. The user types the code on the 4×4 matrix keypad (mapped as columns 4-3-2-1 left-to-right × rows A-B-C-D top-to-bottom, with row D doubling as 0 / backspace / submit per the §5.11 walkthrough).
3. Both sides compute the **session key** as:

   ```c
   salt[12] = sender_mac[6] || receiver_mac[6];
   session_key[16] = PBKDF2_HMAC_SHA256(
       password = pairing_code_as_6_bytes,
       salt     = salt,
       iter     = 4096,
       dk_len   = 16
   );
   ```

   PBKDF2-HMAC-SHA256 with 4096 iterations is the same KDF used by WPA2-PSK on the same chip; the ESP-IDF WiFi component already ships the underlying `mbedtls` primitives, so no new dependency is needed.
4. The sender registers the receiver's MAC in the ESP-NOW peer table with `esp_now_add_peer()` + the LMK (local master key) = the session key. ESP-NOW's radio layer uses the LMK to encrypt every packet with **AES-128-CCM**; the §15.4 wire format is the *application* payload inside the radio's already-encrypted envelope.
5. The first message from the receiver to the sender is the HELLO, which carries the 6-digit code in plaintext *inside* the encrypted envelope. The sender checks the code against its own code; if they match, it sends HELLO_ACK and the transfer begins. If they do not match, the sender sends CANCEL and returns to IDLE.

**Why PBKDF2, not raw SHA-256?** The 6-digit code has 20 bits of entropy. A raw SHA-256 over the code would let an offline attacker try the full 2^20 space in a few seconds. PBKDF2 at 4096 iterations stretches each guess to ~10 ms on a 2020 laptop and ~2 ms on an ESP32, raising the offline-attack cost to ~10⁵ seconds (~30 hours) on a single attacker machine. This is not "unbreakable" — it is "expensive enough that the casual-eavesdropper threat (T1) is defeated", which is the design goal.

**Why not ESP-NOW's built-in "encrypted but unauthenticated" mode?** ESP-NOW's `ESP_NOW_USE_SECURITY=1` mode does AES-128-CCM but with a **16-byte LMK that the firmware sets once at init**. There is no per-session key; the same LMK encrypts every packet from boot to power-off. The §15.2 design (T2: impersonate the sender) and §15.2 (T4: replay of an old capture) require per-session keys, which ESP-NOW's static LMK cannot give us. The §15 design therefore uses ESP-NOW as a *transport* and adds its own session layer on top. (This is a common ESP-NOW pattern; see the Espressif `esp-now` examples for the same approach.)

### 15.6 Timing, throughput, retry policy

- **PHY rate:** 1 Mbit (the default, most-reliable ESP-NOW rate). The §15.10 deferred list includes a 2 Mbit / 5.5 Mbit / 11 Mbit option that roughly halves or quarters transfer time at the cost of ~3–6 dB less range; v1 ships 1 Mbit only.
- **Chunk size:** 230 B payload per CHUNK, giving an effective **~180 kbit/s application throughput** at 1 Mbit PHY (after the radio's preamble, ACK turnaround, and the §15.4 wire-format overhead).
- **ACK timeout:** 50 ms. A missed ACK triggers a retransmit, up to **5 retransmits** per chunk. After 5 retransmits the sender sends CANCEL and returns to IDLE.
- **Pairing timeout:** 30 s. If no HELLO arrives in 30 s, the sender returns to IDLE and the code on the TFT rotates.
- **Per-session timeout:** 60 s of no ACK traffic aborts the transfer (defends against a receiver that walks away mid-transfer).
- **Progress reporting:** the receiver's ACK carries `received_so_far`, so the sender renders an honest progress bar (bytes received by the peer / total bytes), not "chunks sent" (which is misleading when there are retransmits).
- **Typical-case timing** at 180 kbit/s application throughput:

  | Sketch size | Packets | Time at 0 % loss | Time at 5 % loss (≤ 5 retransmits) |
  |---|---|---|---|
  | 100 KB (small) | 435 | 0.6 s | 0.8 s |
  | 1 MB (typical) | 4 350 | 5 s | 7 s |
  | 10 MB (heavy) | 43 500 | 50 s | 65 s |
  | 50 MB (max) | 218 000 | 4 min | 5.5 min |

  The 5–10 s typical case is fast enough that the user's mental model is "instant" rather than "wait". The 5-minute worst case is rare (it requires a sketch with a near-40-second sample pool) and is still acceptable for a feature the user is opting into.

### 15.7 What goes on the TFT — proposed §6 screen

The v2 TFT UI in §6.1–§6.8 has 8 screens, none of which is a Transfer screen. The §15 feature needs a 9th. The proposed layout:

```
+-----------------------------------+
| TRANSFER                          |
|                                   |
|  > Send sketch                    |
|    Receive sketch                 |
|    Cancel                         |
|                                   |
|  Knob A: scroll   click: select   |
+-----------------------------------+

       ↓  user picks "Send sketch"  ↓

+-----------------------------------+
| SENDING                           |
|                                   |
|  Code:  4 8 2   9 1 3             |
|                                   |
|  Receiver: waiting...             |
|  Expires in: 28 s                 |
|                                   |
|  Cancel = SOUND                   |
+-----------------------------------+

       ↓  HELLO received, user confirms  ↓

+-----------------------------------+
| SENDING -> AA:BB:CC:DD:EE:FF      |
|                                   |
|  pocket beat        a1b2          |
|  1.2 MB / 4.3 s                   |
|                                   |
|  ████████████░░░░░░  62 %         |
|                                   |
|  Cancel = SOUND                   |
+-----------------------------------+

       ↓  DONE  ↓

+-----------------------------------+
| SENT                              |
|                                   |
|  pocket beat        a1b2          |
|  1.2 MB in 4.3 s                  |
|                                   |
|  > Send again                     |
|    Exit                           |
+-----------------------------------+
```

The receiver's screens are the mirror image: **RECEIVING → Code: _ _ _ _ _ _ → RECEIVED**. The "Send again" affordance matters because the most common use case is "send to two friends at a table", and re-pairing for each friend is one extra knob turn rather than a re-entry of the menu.

Estimated cost: ~120 lines of new code in `main/ui/display.c` plus a `transfer_screen_state_t` enum in the input task. Falls within the §6.9 "eight screens × ~50 lines" budget if the average is taken over all nine screens instead of the first eight.

### 15.8 What gets added to `sdkconfig`

The `sdkconfig` already has the radio enabled. The §15 feature requires no new `sdkconfig` entries; it only needs the existing entries to remain set:

- `CONFIG_ESP_WIFI_ESPNOW_MAX_ENCRYPT_NUM=7` — already set. Allows up to 7 simultaneous encrypted peers in the peer table. §15 uses 1.
- `CONFIG_ESP_WIFI_SOFTAP_SUPPORT=y` — already set. The ESP-NOW stack uses the same WiFi driver as the SoftAP mode; enabling SoftAP support in `sdkconfig` is what makes ESP-NOW available. (We do **not** use the SoftAP itself in §15; it is required for the radio driver, not for a network.)
- `CONFIG_ESP_WIFI_ENABLE_ROAMING_APP` and `CONFIG_ESP_WIFI_11KV_SUPPORT` and the other 802.11k/v/r knobs — already correctly **off** (the §15 design uses the radio raw, not a managed STA).

**One new entry** is needed in `main/config.h` to gate the §15 feature: `#define PO33_ENABLE_P2P_TRANSFER 1` (off by default; turns on the §15 state machine, the §6.9 transfer screen, and the WRITE+SOUND gesture detector). The default-off matches the §14 default-off for WiFi-backed features (see §16 *WiFi* glossary entry).

### 15.9 What the §3 feature map and the §7 scorecard look like

§3.9 gets a new `F-039` (the feature itself, partial). §3.10's Sync section is **not** touched — ESP-NOW is data transfer, not tempo sync, and the §3.10 entries (F-030, F-031) are about the existing 3.5 mm jam-sync path. §7.1's Data Transfer row's totals update: 2 / 0 / 0 / 2 → 3 / 0 / 1 / 2. The grand-total row updates: ~50 / 30 (60 %) / 13 (26 %) / 7 (14 %) → ~51 / 30 (59 %) / 14 (27 %) / 7 (14 %). The percentages are the same to within rounding; the §15 feature is a **partial** (designed but not committed) and does not move the "done" needle.

### 15.10 What is **not** in §15 (deferred to a future revision)

- **Group transfer** (1 → N). Requires a small multicast protocol on top of ESP-NOW; the §15.4 wire format does not need to change but the §15.3 state machine does.
- **Selective slot transfer.** Requires a manifest packet at the start of the transfer listing which slots/patterns are included; the user would pick on the receiver. The §11 sketch blob would need a "partial export" mode in `storage_sketch_save_subset()`.
- **Higher PHY rates** (2 Mbit, 5.5 Mbit, 11 Mbit). Tradeoff: speed vs. range. v1 ships 1 Mbit; the §15.5 retry budget and §15.6 timing tables assume 1 Mbit.
- **Long codes** (8 digits, ~27 bits of entropy). Useful if the threat model expands beyond the §15.2 "casual eavesdropper" assumption. The 4×4 matrix keypad can comfortably type 8 digits; the change is in the §15.5 PBKDF2 input and the §6 screen.
- **On-sender confirmation via a "press encoder to confirm send" gesture**, replacing the §15.2 T3 "user visually confirms MAC on TFT" step. A physical confirmation is harder to phish than a visual one. (The current visual-confirm path is acceptable for v1 but a v2 will replace it.)
- **Encrypted-at-rest sketches.** §15 is about *transit* encryption, not *storage* encryption. If the sketch is sensitive at rest, the §11 storage layer needs a separate §11.x proposal (a sketch-blob-level AEAD with a per-device key, like the §14.4 Ed25519 path but for confidentiality, not just signing). Out of scope here.
- **A §15 ↔ §14 pipeline.** Once a sketch has been ESP-NOWed to the receiver, can the receiver's *next* §14 upload carry that sketch up to the server? Yes, in v1, because the §11 blob is the same on disk after either path. But the *UX* of "send-to-friend and then upload-from-friend-to-server" is not designed; a v2 §15.10 may add an explicit "Mark as mine after receive" toggle that re-signs the sketch with the receiver's Ed25519 key.

### 15.11 Open questions for the first reviewer

1. **Should the §15.5 PBKDF2 iteration count be 4096 or higher?** 4096 is the WPA2-PSK baseline. Raising to 16384 quadruples the per-transfer handshake time (~40 ms) but raises the offline-attack cost to ~10⁶ seconds. The §15.6 timing budget has slack; 16384 is the safer default.
2. **Should the 6-digit code be rotated automatically, or only on user request?** Auto-rotation (every 30 s, as drafted) is better for the T1 threat. Manual rotation is friendlier to a user who needs 90 s to type the code on the receiver.
3. **Should the §15.4 wire format's `session` nonce be 4 bytes (as drafted) or 8 bytes (matching ESP-NOW's `esp_now_send_set_key()` API)?** 4 bytes is the §14-style "session, not crypto-grade nonce" usage; 8 bytes would survive a longer brute-force attack. 4 bytes is fine for the §15.2 threat model.
4. **Should the §15 transfer screen replace the §6 alarm screen (currently §6.8), or sit alongside it as §6.9?** §6.9 is the proposal; the first reviewer may prefer to push the §6 count to 10.
5. **Should the WRITE+SOUND gesture open a "What kind of transfer?" submenu** (ESP-NOW / §14 Wi-Fi upload / §9 audio cable), or is the §15.2 T3 visual-confirm path good enough to keep WRITE+SOUND as a one-tap direct action? v1 = one-tap; v2 = submenu. Flag for the second reviewer.


---

## 16. Glossary

- **AMY** — A free open-source fixed-point music synthesizer library. Used for all sound generation in this project. ([github.com/shorepine/amy](https://github.com/shorepine/amy))
- **BPM** — Beats per minute. How fast the music goes.
- **DAC** — Digital-to-analog converter. A chip that turns digital numbers into electrical signals a speaker can play.
- **DMA** — Direct Memory Access. A chip feature where data is moved between memory and a peripheral (like the audio chip) without the CPU doing the work. This frees the CPU to do other things while audio is playing.
- **Driver** — A piece of code that knows how to talk to a specific chip.
- **DSP** — Digital Signal Processing. Any operation that transforms numbers representing audio. A filter is DSP; a reverb is DSP; pitch-shifting is DSP.
- **ESP32-S3** — A microcontroller chip made by Espressif. Has WiFi, Bluetooth, and lots of GPIO pins.
- **Firmware** — Software that is "firmly" embedded in a device. Same idea as "software", but used for things that run on chips rather than laptops.
- **FreeRTOS** — A small operating system that runs on microcontrollers. Lets multiple "tasks" run at the same time on a single CPU.
- **GPIO** — General Purpose Input/Output. A pin on the microcontroller that can be either an input (read a button) or an output (drive an LED).
- **I²C** — A two-wire protocol for talking to small chips like sensors and displays.
- **I²S** — A protocol for sending digital audio between chips. Similar to I²C but optimized for audio.
- **ISR** — Interrupt Service Routine. A function that runs immediately when a hardware event happens (like a button press).
- **JTAG** — A debugging protocol for microcontrollers. Not used in this firmware.
- **LittleFS** — A small filesystem designed for flash memory. We use it to store patterns and saved samples.
- **MIDI** — Musical Instrument Digital Interface. An old protocol (1983) for sending music between devices. We don't use MIDI in this project directly, but AMY understands MIDI note numbers.
- **NVS** — Non-Volatile Storage. A small key-value store in the ESP32's flash that survives reboots. We use it for clock + alarm settings.
- **Octal PSRAM** — A kind of extra memory chip connected to the ESP32-S3 with 8 data lines. Fast, cheap, and 8 MB on our board.
- **PCM** — Pulse-Code Modulation. The raw format of digital audio: a stream of numbers representing the air pressure at the microphone at each instant.
- **Pitch** — How high or low a sound is. Doubling the pitch makes the sound one octave higher.
- **Polyphony** — How many sounds can play at the same time.
- **PSRAM** — Pseudo-Static RAM. Cheap, large, external memory that the ESP32-S3 can address as if it were normal RAM.
- **PWM** — Pulse-Width Modulation. A way to fake an analog signal by toggling a digital pin on and off very fast. We use it for the TFT backlight brightness.
- **Quantize** — Snap timing to a grid.
- **Sample rate** — How many audio samples per second. We use 44100 samples/second, same as a CD.
- **Sequencer** — The "robot drummer" that walks through the steps and plays sounds.
- **Slot** — A numbered storage location for a sample.
- **SPI** — Serial Peripheral Interface. A four-wire protocol for talking to chips, faster than I²C. We use it for the TFT.
- **Step** — One moment in a beat. The PO-33 has 16 steps per pattern.
- **Stutter** — A musical effect where a tiny slice of sound is repeated rapidly.
- **Sync** — Two devices agreeing on tempo.
- **TFT** — Thin-Film Transistor. A flat color screen.
- **Tweak** — A small adjustment you can make to a sound (pitch, filter, or trim).
- **UART** — Universal Asynchronous Receiver/Transmitter. The protocol used for the USB-serial port that talks to your computer.
- **Voice** — One independent instance of a playing sound. With 4-voice polyphony, you can hear 4 sounds at once.
- **WAV** — A common audio file format. Not directly used as the sketch-blob format on the device, but used as an optional sidecar in uploads (see §14.8).  [UPDATED: WAV noted as an upload sidecar, no longer "Not directly used in this firmware".]
- **WiFi** — Wireless networking. Optional in this firmware. The ESP32-S3-WROOM-1-N16R8 module has the radio physically present; the firmware enables it only when a user opts into Wi-Fi–backed features (see §14 for the Wi-Fi/AP/STA path, §15 for the peer-to-peer ESP-NOW path). When not opted-in, the radio stays disabled and no Wi-Fi subsystem is initialized.  [UPDATED: previously stated "Not used in this firmware (would need a WiFi-enabled ESP32 variant; the WROOM-1-N16R8 has it disabled by default)" — that statement is no longer accurate once §14 is adopted. The radio is on the chip; whether the firmware drives it is what matters. ESP-NOW (§15) is a second opt-in on the same radio and uses a different firmware API; the two features are mutually exclusive at runtime (the radio can be in either mode, not both at once).]
- **ESP-NOW** — Espressif's connectionless peer-to-peer protocol that runs over the ESP32-S3's 2.4 GHz WiFi radio without an access point, a router, or an IP layer. One-to-one or one-to-many, ~250 B per packet, ~30 m range line-of-sight. Used in this firmware by §15 to swap sketches face-to-face between two PO-33 K.O! devices. [ADDED: glossary entry for the §15 feature.]
- **PBKDF2** — Password-Based Key Derivation Function 2. A standard way to turn a short, low-entropy secret (like a 6-digit pairing code) into a high-entropy cryptographic key, by repeatedly hashing it. Used in §15.5 to derive the AES-128 session key from the 6-digit code. [ADDED: glossary entry for the §15.5 KDF.]
- **AES-128-CCM** — A block cipher (AES) with a 128-bit key, in CCM mode (counter with CBC-MAC) that gives both encryption and integrity in one operation. Used in §15.5 as the per-packet cipher for the ESP-NOW radio layer. [ADDED: glossary entry for the §15.5 cipher.]
---

*End of document. ~18 000+ words. Source of truth for the original PO-33 manual section numbers is [teenage.engineering/guides/po-33/en](https://teenage.engineering/guides/po-33/en); for our firmware, see the file paths and function names cited in §3. How we can break the PO-33's limits is in §10; the multi-project storage proposal is in §11; cliff's notes on the companion book are in §12; the optional Wi-Fi / web-binding / sketch-upload design (ESP32-S3 PO-33 remake) is in §14; the peer-to-peer face-to-face sketch-exchange design (ESP-NOW, no internet) is in §15; the glossary is in §16.*  [UPDATED: footer now references §15 (ESP-NOW) and §16 (Glossary); Glossary is now the last section, as it should be.]

---

## Change Log

All revisions to this document are recorded here. Each entry corresponds to a set of inline `[ADDED]` / `[UPDATED]` / `[REMOVED]` markers placed at the spot of the change, so the body of the document and this log are kept in lock-step.

### 1.0 — 2026-10-17 — Initial Version / scaffolding + new §14 (Wi-Fi, web binding, sketch upload)

**Scope note (important).** The original prompt for this revision described the document as already containing a Wi-Fi provisioning section, a device identity section, a binding flow section, a sketch upload section, and security / error-handling sections, then asked for *surgical* updates. The actual `docs/DESIGN.md` had no such content — it was a pure device-internal design document (sections 0–13, glossary). Rather than refuse the work or invent a new file, this revision treats the result as a **Version 1.0 first pass**: a brand-new §14 is added from scratch, plus a small terminological disambiguation on the one FAQ entry that genuinely calls the disambiguation out, plus honest corrections to two glossary entries that would otherwise have been left saying the opposite of what is now true. Everything else (sections 0–12) is untouched, per the surgical-edit mandate.

Changes in this revision, in source order (full enumeration follows in the entries below):

- Header scaffold (`Version`, `Date`, terminology block).
- One FAQ entry in §9.
- New §14 (10 subsections) inserted between §12 and the (now §16) glossary.
- Two glossary entries corrected.
- Trailing footer paragraph references updated.
- Glossary renumbered §13 → §15. (The displayed text now reads "§16" because of the global §15↔§16 swap applied during the rev 1.1 edit; the actual renumbering that happened in rev 1.0 was §13 → §15. In rev 1.1, the Glossary is then moved from position §15 to position §16, so it is now the last section of the document.)
- This Change Log section appended.

Detailed enumeration (per marker site):

1. **[ADDED]** Header scaffold: `Version: 1.0` and `Date: 2026-10-17` lines, plus a terminology block defining **"ESP32-S3 PO-33 remake"** vs. **"original PO-33"** / **"Teenage Engineering PO-33"** / **"real PO-33"**. *(Clarification #3 groundwork.)*
2. **[UPDATED]** FAQ entry "Q: Does it work with a real PO-33?" — the only existing-text spot whose terminology was genuinely ambiguous in the way Clarification #3 cares about. Rephrased to "ESP32-S3 PO-33 remake boards" and "original Teenage Engineering PO-33". *(Clarification #3.)*
3. **[ADDED]** New **§14. Wi-Fi, web binding, and sketch upload (community integration)** inserted between §12 and the (renumbered) glossary.
    - §14.0 Scope and non-goals.
    - §14.1 *Who needs to bind a device — and who does not*: binding is **optional**; original-PO-33 owners and device-less users are full citizens. *(Clarification #1.)*
    - §14.2 *Cardinality*: **1 user → N devices**, **1 device → 1 user**; rebind requires explicit unbind / admin revoke. *(Clarification #2.)*
    - §14.3 *Binding protocol — RFC 8628 Device Authorization Grant*. Includes the state diagram `DEVICE_IDLE → AUTHORIZE_REQUEST → SHOW_QRCODE → POLLING → BOUND / IDLE`, the QR-code primary path (§14.3.2), the manual `user_code` fallback (§14.3.3), the polling-error table for `authorization_pending` / `slow_down` / `access_denied` / `expired_token` (§14.3.4), and what the device stores on success (§14.3.5). *(Clarifications #5 and #6.)*
    - §14.4 *Ed25519 signing — mandatory on every upload*. First-boot key generation; private key in encrypted NVS namespace `crypto` (or eFuse `BLOCK_KEY`); public key registered with server during binding (§14.4.1). Canonical-JSON rules (UTF-8 NFC, lex-sorted keys, no insignificant whitespace, plain-decimal numbers, minimal string escaping, fixed schema with `schema=1`) and the canonical sketch-metadata JSON example (§14.4.2). Rebind does **not** rotate keys (§14.4.3). *(Clarification #4.)*
    - §14.5 *Unbinding and revocation*: device-initiated unbind (§14.5.1), web-side unbind by owner detected via HTTP 401/403 (§14.5.2), admin revoke for lost / stolen devices (§14.5.3), rebind prerequisite (§14.5.4), and the sketch-history-preservation fingerprint snapshot (§14.5.5). *(Clarification #7.)*
    - §14.6 *Device fingerprint*: SHA-256 over `serial || hardware_mac || pubkey_sha256 || firmware_git_sha`. Backstop for the §14.5.5 snapshot.
    - §14.7 *Sketch upload lifecycle — drafts are private by default*. Device uploads land as `status = draft`, `visibility = private`; publish is web-only. Server pipeline: `RECEIVED → SIGNED_OK → VALIDATED_OK → DRAFT_VISIBLE → PUBLISHED`. *(Clarification #8.)*
    - §14.8 *Upload payload format and size limits*. Multipart body: `metadata` + `signature` + `backup_file` (binary LittleFS tarball) + optional per-slot `sample_wav` sidecars. Size limits: `backup_file` ≤ **50 MB**, each `sample_wav` ≤ **10 MB**, total request ≤ 64 MB, `metadata` JSON ≤ 16 KB. Bytes signed are only the metadata part. *(Clarification #9.)*
    - §14.9 *Security notes specific to this section* (TLS 1.2+ with SHA-256 leaf pinning, no secrets on the TFT, `device_code` expiry watchdog, Ed25519 private-key hygiene, factory-reset wipe of both `crypto` and `bind` NVS namespaces).
    - §14.10 *Open questions deferred*: refresh-token rotation, per-sketch keys, draft-replace UX.
4. **[UPDATED]** Glossary entries:
    - **WAV**: extended to note its role as an optional upload sidecar (§14.8); original "Not directly used in this firmware" wording removed.
    - **WiFi**: the original "Not used in this firmware (would need a WiFi-enabled ESP32 variant; the WROOM-1-N16R8 has it disabled by default)" is replaced with the correct statement that the radio is on the chip and the firmware enables it only when the user opts into the §14 features.
5. **[UPDATED]** Trailing "*End of document*" footer paragraph now references §10 / §11 / §12 / §14 / §15 with correct section numbers. (The displayed text now reads "§16" because of the global §15↔§16 swap applied during the rev 1.1 edit; the actual section list in rev 1.0 was §10 / §11 / §12 / §14 / §15. In rev 1.1, the footer is then updated to list the ESP-NOW design at §15 and the Glossary at §16 in the correct order.)
6. **Renumbered** Original §13 ("Glossary") is now §15. (The displayed text now reads "§16" because of the global §15↔§16 swap applied during the rev 1.1 edit; the actual renumbering that happened in rev 1.0 was §13 → §15. In rev 1.1, the Glossary is then moved from position §15 to position §16.) Internal glossary entries unchanged except for the two above.

**What this revision does NOT change.** Sections 0–12 are untouched aside from the one FAQ entry listed above. Pinout, hardware overview, scorecard, sketch-storage model (§11), UI-screen descriptions (§6), and the music-theory companion (§12) are unchanged. No code paths, file paths, or function names were added or modified outside the new §14.

### 1.1 — 2026-10-17 — Add §15 (peer-to-peer sketch exchange over ESP-NOW) + glossary + scorecard + workflow

**Scope note.** This revision designs the face-to-face, no-internet sketch-exchange feature for two PO-33 K.O! devices. It builds on §14's opt-in radio model and reuses §11's sketch-blob format verbatim, so the §15 blobs are byte-for-byte interchangeable with §14.8's `backup_file`. As with §14, the design is described in full but no code is committed; the `sdkconfig` is already correctly set (`CONFIG_ESP_WIFI_ESPNOW_MAX_ENCRYPT_NUM=7`, `CONFIG_ESP_WIFI_SOFTAP_SUPPORT=y`), so the build infrastructure is ready when the implementation lands.

**What this revision adds or changes:**

0. **[UPDATED]** In a follow-up edit within this same revision, **§15 and §16 are swapped** so that the **Glossary** is the last section of the document (the user's standing rule). Concretely: **§15** is now **Peer-to-peer sketch exchange over ESP-NOW** and **§16** is now the **Glossary**. All cross-references in the body, the footer, and the numbered list below have been updated to use the new numbers. The swap is purely presentational: no design content has changed; §14 is still the Wi-Fi design, and §15/§16 are still the two terminal sections, just in the other order.
1. **[ADDED]** New top-level §15 "Peer-to-peer sketch exchange over ESP-NOW" (≈ 250 lines, 11 subsections):
    - §15.0 *Why this section exists* — motivation, ESP-NOW as the right primitive, scope.
    - §15.1 *Goals and non-goals* — 6 goals, 4 non-goals.
    - §15.2 *Threat model* — table of 5 in-scope threats (passive eavesdropper, sender-impersonation, receiver-impersonation, replay, in-flight tampering) plus 3 explicit out-of-scope threats.
    - §15.3 *Protocol overview* — asymmetric state machine, full ASCII sequence diagram.
    - §15.4 *Packet format* — 9-byte header, 230-byte CHUNK payload, full `msg_type` table, sketch-blob reuse from §14.8.
    - §15.5 *Pairing, encryption, key derivation* — 6-digit code, PBKDF2-HMAC-SHA256 (4096 iter, MAC salt), AES-128-CCM rationale.
    - §15.6 *Timing, throughput, retry policy* — 180 kbit/s, 50 ms ACK timeout, 5 retransmits, full timing table for 100 KB → 50 MB.
    - §15.7 *What goes on the TFT* — proposed 9th §6 screen, 4-panel ASCII mockup, ~120 LoC estimate.
    - §15.8 *What gets added to `sdkconfig`* — confirms the existing entries are sufficient, defines the one new `main/config.h` gate.
    - §15.9 *What the §3 feature map and the §7 scorecard look like* — explicit delta to F-039 and to the §7.1 totals.
    - §15.10 *What is not in §15* — 7 deferred items (group transfer, selective slot, higher PHY, longer codes, physical confirmation, encrypted-at-rest, §15↔§14 pipeline).
    - §15.11 *Open questions for the first reviewer* — 5 questions, each with a default and an alternative.
2. **[ADDED]** §3.9 — new feature `F-039 — Peer-to-peer sketch exchange over ESP-NOW (face-to-face, no internet)`, marked `⚠️ partial` (designed in §15, not committed). The §3.10 Sync section is **not** touched; ESP-NOW is data transfer, not tempo sync.
3. **[ADDED]** §5.11 — new workflow "How do I send a sketch to another device face-to-face?" with the 5-step user flow (sender picks Transfer → Send, receiver picks Transfer → Receive, types the 6-digit code, both watch the progress bar, success/failure surfaces a one-line reason on the TFT).
4. **[UPDATED]** §7.1 scorecard — Data Transfer row: 2 / 0 / 0 / 2 → 3 / 0 / 1 / 2. Total row: ~50 / 30 (60 %) / 13 (26 %) / 7 (14 %) → ~51 / 30 (59 %) / 14 (27 %) / 7 (14 %). The percentages round to the same; F-039 is a partial.
5. **[ADDED]** Three new glossary entries (§16) for **ESP-NOW**, **PBKDF2**, and **AES-128-CCM** — the three new acronyms the §15 body introduces.
6. **[UPDATED]** *WiFi* glossary entry — now references both §14 (Wi-Fi/AP/STA path) and §15 (peer-to-peer ESP-NOW path), and notes that the two features are mutually exclusive at runtime (the radio can be in either mode, not both at once).
7. **[UPDATED]** Trailing "End of document" footer — now lists §15 alongside §10, §11, §12, §14, §16.

**What this revision does NOT change.** §0–§12, §14, and §16's existing entries are untouched aside from the four things listed above. (Note: the doc has no §13 — rev 1.0 renamed the original §13 "Glossary" to §15 to make room for the new §14; rev 1.1 then moved the Glossary from position §15 to position §16, so the Glossary is now the last section of the document, per the standing rule. The only "§13" references in the body refer to the *original PO-33 manual's* §13, not to a design-doc section.) Pinout, hardware overview, sketch-storage model (§11), UI-screen descriptions (§6 — the §15 screen is *added* to the roadmap in §15.7, but the existing §6.1–§6.8 entries are unchanged), the music-theory companion (§12), the §10 wishlist (ESP-NOW sketch exchange was not in the v3 wishlist; it is a separate, parallel feature that does not displace any of the §10 items), and the §14 design are all unchanged. No code paths, file paths, or function names were added or modified outside the new §15, the F-039 entry, the §5.11 workflow, the scorecard, the glossary additions, and the footer.

**Marker convention.** Every modified or inserted paragraph carries an inline marker of the form `[ADDED: …]`, `[UPDATED: …]`, or `[REMOVED: …]`. These markers can be stripped in a future revision once the prose is stable; until then they are the audit trail that pairs the body of the document to the entries in this Change Log.
