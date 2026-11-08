# Long-Term Audio Architecture & Animation Roadmap

## 1. High-Level Vision & Core Partitioning

The goal is to transition the cabinet lighting controller into an intelligent, audio-reactive system without compromising low-latency PWM performance or tactile control.

- **Core 0 (Hardware & Kinematics - 100–200 Hz):**
  - 8-channel PWM generation (20 kHz, 10-bit) via 74AHCT541 buffer.
  - Door switch kinematics (5-second perceptual fade in CIE L\* space).
  - 4-button debounce and manual mode overrides.
  - Wi-Fi / OTA network services.
  - Renders visual animations based on state structs provided by Core 1.

- **Core 1 (Real-Time Audio DSP & Edge Inference):**
  - **Stage A (Deterministic DSP - Every 4 ms):** Continuous I2S audio ingestion (16 kHz, 16-bit PCM from INMP441). Real-time 4-band IIR biquad filtering and asymmetric envelope followers.
  - **Stage B (TinyML Macro Classifier - Every 1.5–2.0 s):** A low-priority background task evaluating rolling spectrogram windows to classify macro context.

---

## 2. Real-Time Audio DSP: 4-Band IIR Filter Bank

Instead of sliding FFT windows (which introduce 30–60 ms temporal smearing), transient detection runs on the RP2350’s single-precision FPU and ARM DSP SIMD instructions:

### Filter Bank Targets (Fs = 16 kHz, Direct-Form II Transposed):

1. **Band 1 (Sub/Kick: 40–120 Hz):** Low-pass / peaking filter. Tracks bass drum and bass root notes.
   - _Target:_ Drives Warm White (W1, W2) and Red (R1, R2) symmetrically.
   - _Decay Constant:_ $\tau \approx 120\text{–}180\text{ ms}$ (punchy weight).
2. **Band 2 (Low-Mid / Snare: 200–800 Hz):** Bandpass ($Q \approx 1.2$). Tracks snare hits, fundamental guitar chords, and backbeats.
   - _Decay Constant:_ $\tau \approx 90\text{ ms}$.
3. **Band 3 (Mid-High / Tapped Guitars: 1–3.5 kHz):** Bandpass ($Q \approx 1.5$). Captures two-handed fretboard tapping, guitar arpeggios, and rimshots.
   - _Target:_ Ping-pong spatial alternation across Green and Blue channels.
   - _Decay Constant:_ $\tau \approx 60\text{ ms}$.
4. **Band 4 (Air / Transients: > 5 kHz):** High-pass / shelving filter. Captures hi-hat subdivisions, pick clicks, and cymbal strikes.
   - _Decay Constant:_ $\tau \approx 40\text{ ms}$.

### Envelopes & Math-Rock Robustness:

- **No Rigid Grid / Phase-Locked Loops:** Avoid autocorrelation tempo estimators that lose lock on asymmetric or additive meters (e.g., 5/4, 7/8, 11/8).
- **Instantaneous Onset:** Attack is instantaneous ($0\text{ ms}$), followed by true exponential decay: $I_t = I_{t-1} \cdot e^{-\Delta t / \tau}$.
- **Spatial Separation:** Low-end energy grounds the visual field symmetrically across Strip 1 and Strip 2, while high-frequency guitar and cymbal transients alternate across strips to track complex stereo counterpoints.

---

## 3. TinyML Macro Context Classifier (DS-CNN)

The ML model does **not** track beats. It runs in a low-priority thread on Core 1 once every 1.5 seconds to adjust the operational macro state.

- **Input:** 1.5-second audio buffer converted into a [93 × 32 × 1] Log-Mel Spectrogram (32 mel bins, 50–8000 Hz).
- **Architecture:** 2D Depthwise Separable CNN (DS-CNN).
  - Parameters: $\approx 24,500$ ($< 30\text{ KB}$ int8 TFLM flatbuffer).
  - Inference Latency: $< 15\text{ ms}$ on RP2350 Cortex-M33 ($< 1\%$ CPU load).
- **Target Macro Classes:**
  1. `NO_MUSIC`: Quiescent ambient hue walk or soft static warm white floor; DSP idles.
  2. `SPEECH_DIALOGUE`: Solid, flicker-free Warm White task lighting; RGB transients muted to prevent vocal distraction.
  3. `AMBIENT_POSTROCK`: Slow breathing color washes; extended decay times ($\tau \approx 300\text{–}500\text{ ms}$).
  4. `MATH_ROCK`: High-energy, crisp transient separation; fast decays ($\tau \approx 40\text{–}80\text{ ms}$) and active stereo ping-pong.
- **Temporal Hysteresis:** Requires 2 consecutive identical classifications ($\approx 3\text{ seconds}$) before transitioning states.

---

## 4. Staged Development Order

1. **Stage 1 (Current):** Stabilize Core 0 (PWM drivers, door fade, button handling, ambient hue cycle).
2. **Stage 2:** Implement Core 1 4-band IIR biquad filter bank and exponential decay envelopes. Expose energy/brightness arrays via double-buffering to Core 0.
3. **Stage 3:** Enable Wi-Fi UDP streaming to capture 1.5-second in-situ audio WAV files directly from the mounted cabinet.
4. **Stage 4:** Train the int8 DS-CNN classifier, export `model.h`, and link TensorFlow Lite Micro onto Core 1.
