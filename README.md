# Cabinet Lighting Controller (RP2350 / Pico 2 W)

A dual-core, perceptually calibrated, and audio-reactive 8-channel (dual RGBW) cabinet lighting controller built on the **Raspberry Pi Pico 2 W (RP2350)**.

The firmware partitions real-time hardware kinematics and ultrasonic 20 kHz PWM generation onto **Core 0**, while dedicating **Core 1** to 16 kHz I2S digital audio ingestion, a 4-band Direct-Form II Transposed IIR biquad filter bank, and an edge TinyML (DS-CNN) macro-context classifier.

---

## Hardware Overview

![Assembled Cabinet Lighting Controller on the Test Bench](doc/cabinet-controller-top.png)

- **MCU:** Raspberry Pi Pico 2 W (RP2350, Dual ARM Cortex-M33 @ 150 MHz with single-precision FPU & DSP SIMD)
- **LED Drive:** 2× 12V RGBW LED strips (8 channels total) driven at **20 kHz, 10-bit (`0–1023`)** via a `74AHCT541N` 5V octal buffer and 8× `IRLZ44N` low-side N-channel MOSFETs (with `180 Ω` gate stoppers and `10 kΩ` pull-downs in a 31-pin keyed female header socket bank)
- **Door Sensing:** Left & Right magnetic reed switches (`INPUT_PULLUP` + `0.1 µF` hardware EMI/debounce caps, active-LOW: grounded = closed)
- **Tactile Control:** 4-button satellite pod (`MODE`, `UP`, `DOWN`, `TASK`, active-LOW)
- **Audio Input:** `INMP441` I2S digital MEMS microphone (16 kHz, 24-bit PCM in 32-bit stereo frames, mono `L/R` tied to `GND`)

### Pinout Reference

| Function | GPIO | Pico Physical Pin | Notes |
| :--- | :--- | :--- | :--- |
| **Strip 1 — White / Blue / Red / Green** | `GP16`, `GP17`, `GP18`, `GP19` | Pins 21, 22, 24, 25 | 20 kHz PWM $\rightarrow$ `74AHCT541N` $\rightarrow$ `IRLZ44N` |
| **Strip 2 — White / Blue / Red / Green** | `GP20`, `GP21`, `GP26`, `GP27` | Pins 26, 27, 31, 32 | 20 kHz PWM $\rightarrow$ `74AHCT541N` $\rightarrow$ `IRLZ44N` |
| **Left / Right Door Reed Switches** | `GP0`, `GP1` | Pins 1, 2 (GND: Pin 3) | `LOW` = Closed, `HIGH` = Open |
| **Buttons: Mode / Up / Down / Task** | `GP8`, `GP9`, `GP10`, `GP11` | Pins 11, 12, 14, 15 (GND: Pin 13) | 35 ms debounced falling-edge triggers |
| **INMP441 I2S (`BCLK`, `WS`, `DATA`)** | `GP13`, `GP14`, `GP15` | Pins 17, 19, 20 (`3V3`: Pin 36, `GND`: Pin 18) | `WS` is constrained to `BCLK + 1` |

---

## Firmware Architecture

- **Core 0 (`setup()` / `loop()` @ ~200 Hz):**
  - **Perceptual Color & Kinematics ([`src/door_engine.cpp`](src/door_engine.cpp)):**
    - **Door Open (or Task Override):** 5-second linear cross-fade in CIE $L^*$ perceptual lightness space ($\gamma = 2.4$) to calibrated photopic task white (`W: 1.00, R: 1.00, G: 0.90, B: 0.86`), with mid-fade reversal locking.
    - **Door Closed (Ambient Modes):** Cycles between a 30-second $S^1$ shortest-arc random hue walk (`HUE_WALK`), locked hue (`STATIC_HUE`), and warm amber/white (`WARM_LOUNGE`), with `10%–100%` brightness stepping.
  - **Ultrasonic PWM Driver ([`src/led_driver.cpp`](src/led_driver.cpp)):** Maps normalized perceptual lightness `[0.0, 1.0]` through $L^{\gamma}$ ($\gamma = 2.4$) to 10-bit hardware PWM duty cycles.
  - **Input Debouncing ([`src/button_manager.cpp`](src/button_manager.cpp)):** Non-blocking 35 ms state machine across both door contacts and all four satellite buttons.
- **Core 1 (`setup1()` / `loop1()`):**
  - **Real-Time Audio DSP ([`src/audio_dsp.cpp`](src/audio_dsp.cpp)):** Lock-free I2S FIFO ingestion and multi-band transient envelope extraction (see [`doc/AUDIO_VISION.md`](doc/AUDIO_VISION.md) for the full 4-band IIR biquad, AGC, and DS-CNN TinyML specification).

---

## Building & Flashing (PlatformIO)

This project uses the [Earle Philhower `arduino-pico` core](https://github.com/earlephilhower/arduino-pico) via PlatformIO.

```bash
# Compile firmware
~/.platformio/penv/bin/pio run

# Flash via USB (picotool)
~/.platformio/penv/bin/pio run -e pico2w-usb -t upload

# Attach serial telemetry monitor (115200 baud)
~/.platformio/penv/bin/pio device monitor
```

---

## Project Documentation

- **[`AGENTS.md`](AGENTS.md):** Hardware invariants, dual-core safety rules, and agentic workflow protocol.
- **[`HANDOFF.md`](HANDOFF.md):** Live project status snapshot and immediate next steps.
- **[`doc/HARDWARE_AND_BOM.md`](doc/HARDWARE_AND_BOM.md):** Complete Bill of Materials (BOM), perfboard spatial layout, socketizing guide, star-ground topology, and pin-by-pin wiring tables.
- **[`doc/AUDIO_VISION.md`](doc/AUDIO_VISION.md):** Authoritative specification for the Core 1 4-band `DF2T` IIR filter bank, adaptive noise floor & AGC, math-rock envelope followers, 8-channel spatial choreography, and 4-class DS-CNN TinyML classifier.
