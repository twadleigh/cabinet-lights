# Cabinet Lighting Controller — Hardware & Architectural Guidelines

## 1. Project Overview & Hardware Platform

- **Target Microcontroller:** Raspberry Pi Pico 2 W (RP2350), clocked at 150 MHz[cite: 1].
- **Core Framework:** Earle Philhower RP2040/RP2350 Arduino Core via PlatformIO (`rpipico2w`)[cite: 1].
- **Toolchain:** PlatformIO on Linux; compile verification via `pio run`.
- **Power Delivery:**
  - 3.3V logic from Pico Pin 36 (`3V3_OUT`)[cite: 1, 3].
  - 5V rail (via buck converter / VSYS) powering 74AHCT541 buffer for MOSFET gate drive[cite: 1, 2].
  - 12V DC external supply with star-ground topology for 8-channel LED strips[cite: 1, 3].

## 2. Strict Pinout & Wiring Rules (DO NOT CHANGE)

- **8-Channel PWM (Level-shifted via 74AHCT541 to IRLZ44N MOSFETs)[cite: 1, 2]:**
  - Strip 1: White = GP16, Blue = GP17, Red = GP18, Green = GP19[cite: 1, 2]
  - Strip 2: White = GP20, Blue = GP21, Red = GP26, Green = GP27[cite: 1, 2]
  - PWM Frequency: 20 kHz (ultrasonic/silent), 10-bit resolution (0–1023 counts)[cite: 1].
- **Sensors & Switches (Active-LOW, INPUT_PULLUP, Grounded = Closed)[cite: 1]:**
  - Left Door Reed: GP0 (Physical Pin 1)[cite: 1]
  - Right Door Reed: GP1 (Physical Pin 2)[cite: 1]
  - Common Switch Return: Ground (Physical Pin 3)[cite: 1]
- **Satellite Pushbuttons (Active-LOW, INPUT_PULLUP)[cite: 1]:**
  - Button 1 (Mode/Advance): GP8 (Physical Pin 11)[cite: 1, 2]
  - Button 2 (Brightness Up): GP9 (Physical Pin 12)[cite: 1, 2]
  - Common Button Return: Ground (Physical Pin 13)[cite: 1, 2]
  - Button 3 (Brightness Down): GP10 (Physical Pin 14)[cite: 1, 2]
  - Button 4 (Task / Warm White): GP11 (Physical Pin 15)[cite: 1, 2]
- **INMP441 I2S Microphone (Digital PCM, Mono L/R tied to GND)[cite: 1]:**
  - BCLK: GP13 (Physical Pin 17)[cite: 1]
  - WS: GP14 (Physical Pin 19)[cite: 1]
  - DATA (SD): GP15 (Physical Pin 20)[cite: 1]
  - Power: 3.3V (Pico Pin 36)[cite: 1, 3]

## 3. Modular Software Architecture

- `include/config.h`: Hardware pin assignments, PWM timing, and perceptual targets[cite: 1].
- `include/led_driver.h` / `src/led_driver.cpp`: 20 kHz 10-bit PWM write routines with CIE L\* gamma correction ($\gamma = 2.4$)[cite: 1].
- `include/door_engine.h` / `src/door_engine.cpp`:
  - Default initialization assumes **CLOSED**[cite: 1].
  - Door open triggers a 5-second linear fade in perceptual lightness space to photopic white (W: 1.0, R: 1.0, G: 0.90, B: 0.86)[cite: 1].
  - Door closed runs a continuous 30-second random ambient hue walk ($S^1$ shortest-arc interpolation) at 50% perceived lightness (`DIM_PERCEPTUAL`)[cite: 1].
- `include/button_manager.h` / `src/button_manager.cpp`:
  - 35 ms non-blocking debounce for switches and buttons[cite: 1].
  - Door states initialize as closed/grounded (`LOW`)[cite: 1].
- `include/audio_dsp.h` / `src/audio_dsp.cpp`:
  - Runs in `setup1()` / `loop1()` on Core 1[cite: 1].
  - Ingests 32-bit I2S frames at 16 kHz[cite: 1].
  - Planned: 4-band IIR biquad filter bank (sub-bass, low-mid, high-mid, treble) + asymmetric envelope followers[cite: 1].

## 4. Operational & Coding Conventions

- **Dual-Core Safety:** Never write to PWM pins or serial from Core 1; Core 1 only processes audio and populates shared structs for Core 0 to consume[cite: 1].
- **No Hardware Register Conflicts:** Use standard Earle Philhower core APIs (`analogWriteFreq`, `analogWriteRange`, `I2S`)[cite: 1].
- **Validation Rule:** Whenever altering code, ensure `pio run` executes cleanly without compiler errors or warnings.

## 5. Long-Term Roadmap & Audio Strategy

- For complete technical specifications, filter equations, and ML architectures, reference `docs/AUDIO_VISION.md`.
- `src/audio_dsp.cpp` is currently a placeholder stub[cite: 1].
- Do not refactor audio processing into an FFT-based visualizer or a phase-locked BPM loop; our design specifically targets multi-band IIR transient envelope following (math-rock aware) and a 4-class DS-CNN macro classifier[cite: 1].
