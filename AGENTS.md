# Cabinet Lighting Controller — Agent Rules & Architecture (`AGENTS.md`)

> **Antigravity Auto-Loaded Context:** This file is automatically injected into every chat session. Read the embedded `HANDOFF.md` snapshot at the bottom of this file before starting any task.

---

## 1. Agentic Protocol & Low-Friction Workflow

1. **Zero-Friction Startup:**
   - Both `AGENTS.md` and `HANDOFF.md` (included below via `@[HANDOFF.md](./HANDOFF.md)`) are auto-loaded into context on Turn 1.
   - Do **not** ask the user to explain the project architecture or current status—pick up directly from `HANDOFF.md`.
   - Only read [doc/AUDIO_VISION.md](doc/AUDIO_VISION.md) when working on Core 1 audio DSP, IIR filter banks, UDP audio capture, or TinyML tasks.
2. **State Maintenance (`HANDOFF.md`):**
   - After any turn involving code changes, build/flash verification, hardware discoveries, or architectural decisions, **overwrite and update [HANDOFF.md](HANDOFF.md)** before concluding your turn.
   - Keep `HANDOFF.md` concise and structured as a **live state snapshot** (not an append-only changelog; Git tracks history).
3. **Compile Verification Gate:**
   - Every code modification must compile cleanly with zero warnings or errors before you finish your turn.
   - Invoke PlatformIO directly (avoiding shell wrappers so command approvals prefix-match cleanly):
     - `~/.platformio/penv/bin/pio run` (or `pio run` if in `PATH`).
     -Note: PlatformIO resides in `~/.platformio/penv/bin/pio`. If the terminal sandbox blocks access to `~/.platformio`, run the build command with `BypassSandbox: true`.

---

## 2. Project Overview & Hardware Platform

- **Target Microcontroller:** Raspberry Pi Pico 2 W (RP2350, Dual Cortex-M33 @ 150 MHz with single-precision FPU & ARM DSP SIMD).
- **Framework & Core:** Earle Philhower `arduino-pico` (`rpipico2w`) via PlatformIO.
- **Power Delivery:**
  - `3.3V` logic from Pico Pin 36 (`3V3_OUT`).
  - `5V` rail (buck converter / `VSYS`) powering the `74AHCT541` octal buffer for `5V` MOSFET gate drive.
  - `12V DC` external supply with star-ground topology driving two 4-channel (RGBW) LED strips via `IRLZ44N` low-side MOSFETs.

---

## 3. Strict Pinout & Wiring Invariants (DO NOT CHANGE)

- **8-Channel PWM (Level-shifted via `74AHCT541` to `IRLZ44N` gates, 20 kHz ultrasonic, 10-bit `0–1023`):**
  - **Strip 1:** White = `GP16` (Pin 21), Blue = `GP17` (Pin 22), Red = `GP18` (Pin 24), Green = `GP19` (Pin 25)
  - **Strip 2:** White = `GP20` (Pin 26), Blue = `GP21` (Pin 27), Red = `GP26` (Pin 31), Green = `GP27` (Pin 32)
- **Door Reed Switches (Active-LOW, `INPUT_PULLUP`, Grounded/`LOW` = Closed, `HIGH` = Open):**
  - Left Door Reed: `GP0` (Pin 1)
  - Right Door Reed: `GP1` (Pin 2)
  - Common Switch Return: `GND` (Pin 3)
- **Satellite Pushbuttons (Active-LOW, `INPUT_PULLUP`, Falling Edge = Press):**
  - Button 1 (Mode / Advance): `GP8` (Pin 11)
  - Button 2 (Brightness Up): `GP9` (Pin 12)
  - Common Button Return: `GND` (Pin 13)
  - Button 3 (Brightness Down): `GP10` (Pin 14)
  - Button 4 (Task / Warm White): `GP11` (Pin 15)
- **INMP441 I2S Microphone (Digital PCM, Mono with `L/R` tied to `GND`, `3.3V` on Pin 36):**
  - `BCLK`: `GP13` (Pin 17)
  - `WS` (`LRCLK`): `GP14` (Pin 19) — *(Hardware constraint: Earle Philhower `I2S` forces `WS = BCLK + 1`)*
  - `DATA` (`SD`): `GP15` (Pin 20)

---

## 4. Dual-Core Partitioning & Coding Rules

- **Core 0 (`setup()` / `loop()` @ ~200 Hz):**
  - Owns all `Serial` telemetry, `analogWrite` PWM outputs, reed switch / button debouncing, door fade kinematics, and future Wi-Fi/OTA services.
  - Default startup door state is always **CLOSED** (`LOW`).
  - Door open triggers a 5-second linear cross-fade in CIE $L^*$ perceptual space ($\gamma = 2.4$) to photopic task white (`W: 1.0, R: 1.0, G: 0.90, B: 0.86`).
  - Door closed runs a 30-second random ambient hue walk ($S^1$ shortest-arc interpolation) at 50% perceptual lightness (`DIM_PERCEPTUAL = 0.50f`).
- **Core 1 (`setup1()` / `loop1()`):**
  - Dedicated exclusively to real-time 16 kHz I2S audio ingestion, 4-band IIR biquad filtering, exponential envelope followers, and background TinyML macro-context classification.
  - **Strict Dual-Core Safety Rule:** Never call `analogWrite`, touch GPIO outputs, or print to `Serial` from Core 1. Core 1 communicates with Core 0 strictly via lock-free / double-buffered shared state structs.
- **Audio DSP Guardrails:**
  - Do **not** refactor audio processing into an FFT visualizer or phase-locked BPM loop. The system uses a deterministic 4-band Direct-Form II Transposed IIR biquad bank + instantaneous-attack exponential decay envelopes (for math-rock odd-meter resilience) paired with a 4-class DS-CNN macro classifier. See [doc/AUDIO_VISION.md](doc/AUDIO_VISION.md).

---

## 5. Live Session Handoff

@[HANDOFF.md](./HANDOFF.md)
