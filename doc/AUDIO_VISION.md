# Long-Term Audio DSP, Perceptual Kinematics & TinyML Architecture

This document is the authoritative specification for the **Cabinet Lighting Controller's** long-term visual kinematics, real-time Core 1 I2S audio DSP pipeline, math-rock transient envelope follower, and TinyML (2D DS-CNN) acoustic macro-context classifier.

Current Phase 2 firmware implements I2S/DC-blocked peak telemetry, atomic audio snapshots, OTA pause/resume, Normal/Debug modes, and hardware-timed PWM dithering. The 4-band bank, AGC, spatial mapping, and classifier below are the Phase 3/4 design.

---

## 1. System Partitioning & Priority Hierarchy

### 1.1 Dual-Core Hardware Partitioning (RP2350 @ 150 MHz)

The Raspberry Pi Pico 2 W (`RP2350A`, Dual ARM Cortex-M33 with single-precision FPU and ARMv8-M DSP SIMD extensions) is partitioned across its two physical cores to separate hardware-timed PWM from audio processing; worst-case interrupt and render latency still require measurement:

- **Core 0 — Hardware IO, Perceptual Kinematics & Network Services (`setup()` / `loop()` @ ~200 Hz / 5 ms):**
  - **8-Channel Ultrasonic PWM (`20 kHz`, 7500-tick (6.67 ns/tick)):** Drives two 12V RGBW LED strips via a `74AHCT541N` 5V octal buffer and 8× `IRLZ44N` low-side MOSFETs. Channels are paired across 4 hardware PWM slices on the right header:
    - **Slice 0:** `GP16` (`0A` $\rightarrow$ `W1`), `GP17` (`0B` $\rightarrow$ `B1`)
    - **Slice 1:** `GP18` (`1A` $\rightarrow$ `R1`), `GP19` (`1B` $\rightarrow$ `G1`)
    - **Slice 2:** `GP20` (`2A` $\rightarrow$ `W2`), `GP21` (`2B` $\rightarrow$ `B2`)
    - **Slice 5:** `GP26` (`5A` $\rightarrow$ `R2`), `GP27` (`5B` $\rightarrow$ `G2`)
  - **Input Debouncing (`35 ms` non-blocking):** Evaluates Left/Right magnetic door reed switches (`GP0`, `GP1`) and the 4-button satellite pod (`GP8–GP11`).
  - **Perceptual Color & Fade Engine:** Executes empirical gamma-2.4 cross-fades, $S^1$ shortest-arc ambient hue walks, and audio-reactive spatial rendering from Core 1 state snapshots.
  - **Serial & Wireless Services:** Owns all USB `Serial` telemetry, `ArduinoOTA` (`cabinet-lights.local`) firmware updates, and Phase 4 UDP audio capture streaming.
- **Core 1 — Real-Time Audio DSP & Edge Inference (`setup1()` / `loop1()`):**
  - **Stage A (Deterministic Fast Loop — Every `4 ms` / 64 samples @ `16 kHz`):** Continuous I2S DMA ingestion from the `INMP441` MEMS microphone (`GP13` BCLK, `GP14` WS, `GP15` DATA), DC blocking, 4-band Direct-Form II Transposed IIR biquad filtering, adaptive noise-floor subtraction, rolling AGC normalization, and instantaneous-attack exponential decay envelopes.
  - **Stage B (TinyML Semantic Director — Every `1.5 s`):** Low-priority background evaluation of a `[1, 92, 32, 1]` `int8` Log-Mel spectrogram using a 9,376-kernel-weight 2D Depthwise Separable CNN (`DS-CNN`) to classify the room's macro acoustic state (`NO_MUSIC`, `SPEECH_DIALOGUE`, `AMBIENT_POSTROCK`, `MATH_ROCK`).
  - **Strict Dual-Core Safety Invariant:** Core 1 must **never** call `Serial`, `analogWrite`, or touch GPIO outputs. All inter-core communication occurs via lock-free / double-buffered shared state structs (`AudioEnergy`, `AudioDspStatus`, `MacroState`).

---

### 1.2 Two-Tier Finite State Machine (FSM) Priority Hierarchy

Physical user actions unconditionally preempt automated acoustic states:

1. **Tier 1 — Physical Overrides (Debounced on Core 0):**
   - **Doors Open (`PIN_DOOR_LEFT == HIGH || PIN_DOOR_RIGHT == HIGH`):** Highest priority. Smoothly cross-fades all 8 channels over 5 seconds in perceptual lightness space to 100% calibrated Photopic Task White (`W: 1.00, R: 1.00, G: 0.90, B: 0.86`), overriding all ambient or audio animations.
   - **Manual Mode Selection:** Button 1 (`PIN_BTN_MODE_NEXT` / `GP8`) and Button 2 (`PIN_BTN_MODE_PREV` / `GP9`) cycle Normal and Debug. Normal runs the 30-second hue walk and door fade; Debug cycles eight channels at full duty. Buttons 3/4 (`GP10`/`GP11`) adjust ambient brightness (or future mode-specific parameters). Debug is an explicit bench override of door rendering.
2. **Tier 2 — Acoustic Macro-Context States (Active when in `AUDIO_REACTIVE` Mode with Doors Closed):**
   - Controlled by the Core 1 TinyML classifier (with 2-step / 3.0-second temporal hysteresis) to dynamically adjust DSP decay time constants ($\tau_k$), stereo ping-pong width, and RGB transient muting.

---

## 2. Perceptual Color Science & Door Kinematics (Core 0)

1. **Empirical Per-Channel Power-Law Mapping ($\gamma = 2.4$):**
   - The implemented per-channel `powf(L, 2.4)` curve is an empirical perceptual mapping, not the piecewise CIE L* luminance transform. Pedestal/minimum-pulse compensation follows this curve.
   - All internal colors operate in normalized perceptual lightness $L \in [0,1]$. At the hardware boundary, active ticks are $A=L^{2.4}(7500-20)$. Zero lightness is strictly off; for $0<A<10$, dither between 0 and 30 ticks with probability $A/10$. Otherwise dither between adjacent integers around $20+A$, with full scale equal to 7500 ticks.
   - `PWM_PEDESTAL_TICKS = 20` (133 ns), `PWM_MIN_PULSE_TICKS = 30` (200 ns), and 6.67 ns native ticks match the measured gate-drive floor. The RAM-resident 20 kHz wrap ISR uses Q0.16 accumulators and paired register writes. Hardware wrap is 7499, giving 7500 ticks per period; compare 7500 is fully on.
2. **5-Second Perceptual Cross-Fade & Constant-Speed Reversal:**
   - Integrate progress each update: $u_{n+1}=\operatorname{clamp}(u_n+s\Delta t/5000\mathrm{ms},0,1)$, where $s=+1$ for an open door and $s=-1$ otherwise.
   - Render $L_c=(1-u)L_{c,\mathrm{ambient}}+uL_{c,\mathrm{open}}$, with open white $(W,R,G,B)=(1,1,0.90,0.86)$. Reversals preserve progress and speed; a reversal at 40% returns to ambient in two seconds. Hue and fade state advance during Debug mode.
3. **$S^1$ Shortest-Arc Ambient Hue Walk:**
   - Every `HUE_CYCLE_PERIOD_MS = 30000UL`, a random target hue $\theta_{\text{target}} \in [0^\circ, 360^\circ)$ is chosen.
   - Angular interpolation follows the shortest geodesic arc on the circle $S^1$:
     $$\Delta\theta = \left((\theta_{\text{target}} - \theta_{\text{source}} + 540^\circ) \bmod 360^\circ\right) - 180^\circ \in [-180^\circ, +180^\circ)$$
     $$\theta(t) = \left(\theta_{\text{source}} + \Delta\theta \cdot \frac{t - t_{\text{hue}}}{30000} + 360^\circ\right) \bmod 360^\circ$$

---

## 3. Real-Time Audio DSP Architecture (Core 1)

### 3.1 INMP441 I2S Digital Audio Ingestion & Bench Characterization

- **32-Bit Stereo Frame Constraint (Critical Hardware Detail):**
  - The `INMP441` is a 24-bit two's-complement I2S MEMS microphone that requires **64 BCLK cycles per frame** (32 bits per channel slot).
  - Initializing Earle Philhower's `I2S` library with `i2s.setBitsPerSample(16)` fails (outputs all zeros `00000`) because the PIO state machine only clocks 32 BCLK cycles per frame.
  - **Mandatory Initialization:** Configure `i2s.setBitsPerSample(32)` at $F_s = 16,000\text{ Hz}$ (`BCLK = 1.024 MHz`) and read 32-bit stereo frame pairs via `i2s.read32(&leftSample, &rightSample)`. With `SEL` (`L/R`) tied to `GND`, the microphone populates the **Left** slot (`leftSample`), and `int16_t pcm16 = static_cast<int16_t>(leftSample >> 16)` extracts the signed 16-bit waveform sample.
- **Empirical Bench SPL Characterization (`16-bit` scale `0–32,767`):**
  - **Quiescent Room Floor (`40–50 dB SPL`):** Raw uncalibrated magnitude sits around `~1,600` counts ($\approx 4.8\%$ full scale) due to room noise and mic DC/thermal floor.
  - **Trebly / Ambient Music (`60–65 dB SPL`):** Raw peaks reach `~1,700–1,800` counts (high-frequency acoustic waves carry far less physical air-displacement energy than bass and attenuate rapidly across a room).
  - **Moderate Bass-Heavy Music (`65–70 dB SPL`):** Raw peaks reach `~2,000–3,500` counts.
  - **Sharp Hand Clap at `6–12 inches` (`>95 dB SPL`):** Peaks reach `~26,400` counts ($\approx 81\%$ full scale without clipping).
- **First-Order DC Blocker:**
  - Before entering the biquad bank, static DC offset is stripped using a 1st-order high-pass filter ($f_c \approx 12.7\text{ Hz}$ at $F_s = 16\text{ kHz}$, $R = 0.995$):
    $$y_{\text{dc}}[n] = x[n] - x[n-1] + 0.995 \cdot y_{\text{dc}}[n-1]$$

---

### 3.2 Why FFTs and BPM/PLL Trackers Fail on Math-Rock

1. **Temporal Smearing of Sliding FFTs:**
   - Resolving low-frequency bass/kick fundamentals ($40\text{–}120\text{ Hz}$) at $F_s = 16\text{ kHz}$ with an FFT requires a 512- to 1024-sample window ($32\text{–}64\text{ ms}$), smearing sharp percussive transients across tens of milliseconds and causing perceptible visual lag.
2. **Failure of Autocorrelation & Phase-Locked Loops on Odd/Additive Meters:**
   - Standard beat trackers compute autocorrelation $R(\tau) = \sum_t E(t) E(t - \tau)$ over a 2–4 second window to lock a PLL oscillator onto a single periodic inter-onset interval (assuming isochronous $4/4$ or $3/4$ meters).
   - In math rock (*Don Caballero, American Football, TTNG, Toe*), rhythmic structure is built from **non-isochronous additive groupings** ($7/8$ as $2+2+3$ or $3+2+2$; $5/4$ as $3+2$; $11/8$ as $3+3+3+2$), alternating time signatures, metric modulations, and rapid two-handed fretboard tapping.
   - Across a 2–4 second window, asymmetric beat intervals create competing sub-peaks in autocorrelation, causing PLLs and comb filters to lose lock, phase-slip, or flash out of time.
3. **Transient Envelopes Without Beat Locking:**
   - Planned blocks contain 64 samples (4 ms at 16 kHz). Instantaneous attack means no additional envelope smoothing after a block is processed; it does not mean zero acoustic-to-light latency.
   - End-to-end delay includes sample age (up to one block), frequency-dependent IIR delay, DSP scheduling, snapshot publication, Core 0 rendering (~5 ms currently), and the next PWM period (50 µs). Measure these separately before making a response-time claim.
   - Phase 3 target: service each 64-sample block within 4 ms and keep scheduling-plus-render delay below 10 ms under normal networking, excluding filter delay and OTA. This is a target, not measured acceptance. OTA deliberately suspends audio and blocks rendering.

---

### 3.3 4-Band Direct-Form II Transposed (`DF2T`) IIR Biquad Filter Bank

Each band is implemented as a second-order Direct-Form II Transposed biquad filter executed in single-precision hardware floating point (`VMLA.F32`) on Core 1:

```cpp
struct BiquadDF2T {
  float b0, b1, b2; // Numerator coefficients (normalized by a0)
  float a1, a2;     // Denominator coefficients (normalized by a0)
  float s1, s2;     // State delay registers
};

static inline float processBiquad(BiquadDF2T& f, float in) {
  float out = f.b0 * in + f.s1;
  f.s1 = f.b1 * in - f.a1 * out + f.s2;
  f.s2 = f.b2 * in - f.a2 * out;
  return out;
}
```

#### Filter Bank Specification ($F_s = 16,000\text{ Hz}$):

| Band | Name | Passband Range | Nominal $f_0$ | Topology & $Q$ | Musical Target | Visual Channel Assignment | Nominal Decay $\tau_k$ |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Band 1** | **Sub / Kick** | $40\text{–}120\text{ Hz}$ | $80\text{ Hz}$ | Bandpass ($Q = 1.0$) or Low-Pass ($f_c = 110\text{ Hz}, Q = 0.707$) | Kick drum thump & bass guitar root notes | **Symmetric:** Strip 1 & 2 Warm White (`W1, W2`) + Red (`R1, R2`) weight | $\tau_1 = 150\text{ ms}$ ($120\text{–}180\text{ ms}$) |
| **Band 2** | **Low-Mid / Snare** | $200\text{–}800\text{ Hz}$ | $400\text{ Hz}$ | Bandpass ($Q = 1.2$) | Snare body, fundamental guitar chords, backbeats | **Symmetric:** Strip 1 & 2 Red (`R1, R2`) | $\tau_2 = 90\text{ ms}$ |
| **Band 3** | **Mid-High / Tapping** | $1.0\text{–}3.5\text{ kHz}$ | $2,200\text{ Hz}$ | Bandpass ($Q = 1.5$) | Two-handed fretboard tapping, guitar arpeggios, rimshots | **Spatial Ping-Pong:** Alternates Strip 1 vs. Strip 2 Green/Blue (`G1/B1` $\leftrightarrow$ `G2/B2`) | $\tau_3 = 60\text{ ms}$ |
| **Band 4** | **Air / Transients** | $> 5.0\text{ kHz}$ | $5,200\text{ Hz}$ | High-Pass ($Q = 0.707$) | Hi-hat subdivisions, pick clicks, cymbal strikes | **Spatial Counter-Shimmer:** Opposite strip `B/G` accent (`B2/G2` $\leftrightarrow$ `B1/G1`) | $\tau_4 = 40\text{ ms}$ |

#### RBJ Audio EQ Cookbook Coefficient Formulas ($F_s = 16,000\text{ Hz}$):

Given center/cutoff frequency $f_0$ and quality factor $Q$:
$$\omega_0 = \frac{2\pi f_0}{F_s}, \qquad \alpha = \frac{\sin\omega_0}{2Q}, \qquad a_0 = 1 + \alpha$$

- **Low-Pass Filter:**
  $$b_0 = \frac{(1 - \cos\omega_0)/2}{a_0}, \quad b_1 = \frac{1 - \cos\omega_0}{a_0}, \quad b_2 = b_0, \quad a_1 = \frac{-2\cos\omega_0}{a_0}, \quad a_2 = \frac{1 - \alpha}{a_0}$$
- **Constant-0dB-Peak Bandpass Filter:**
  $$b_0 = \frac{\alpha}{a_0}, \quad b_1 = 0, \quad b_2 = \frac{-\alpha}{a_0}, \quad a_1 = \frac{-2\cos\omega_0}{a_0}, \quad a_2 = \frac{1 - \alpha}{a_0}$$
- **High-Pass Filter:**
  $$b_0 = \frac{(1 + \cos\omega_0)/2}{a_0}, \quad b_1 = \frac{-(1 + \cos\omega_0)}{a_0}, \quad b_2 = b_0, \quad a_1 = \frac{-2\cos\omega_0}{a_0}, \quad a_2 = \frac{1 - \alpha}{a_0}$$

---

### 3.4 Adaptive Noise Floor, Rolling AGC & Exponential Envelopes

Evaluated once per 64-sample block ($\Delta t = 4.0\text{ ms}$, or `250 Hz` update rate) for each band $k \in \{1, 2, 3, 4\}$:

1. **Block Rectified Amplitude & Spectral Tilt Pre-Gain:**
   - Compute average or peak rectified amplitude over the 64 samples, scaled by per-band acoustic compensation gain $g_k$ (boosting Bands 3 & 4 to compensate for high-frequency room attenuation):
     $$E_k[m] = g_k \cdot \frac{1}{64}\sum_{n=0}^{63} \left|y_k[n]\right|$$
2. **Slow-Tracking Noise Floor Subtraction ($\tau_{\text{floor}} \approx 500\text{ ms}$):**
   - A leaky integrator tracks the ambient noise floor $B_k[m]$ ($\alpha_{\text{floor}} = e^{-4 / 500} \approx 0.992$):
     $$B_k[m] = 0.992 \cdot B_k[m-1] + 0.008 \cdot E_k[m]$$
   - Subtract the baseline with sensitivity factor $\eta_k \approx 1.3$ to reject continuous room hum:
     $$R_k[m] = \max\!\left(0.0,\; E_k[m] - \eta_k \cdot B_k[m]\right)$$
3. **Rolling Automatic Gain Control (AGC Peak Follower):**
   - Ensures quiet `60 dB SPL` listening and loud `85 dB SPL` sessions both span the full `[0.0, 1.0]` visual range. Each band tracks a rolling peak ceiling $P_k[m]$ with instant expansion and slow decay ($\gamma_{\text{agc}} \approx 0.995$, bounded below by a minimum floor $P_{k,\min}$ so silence never amplifies noise):
     $$P_k[m] = \begin{cases} R_k[m] & \text{if } R_k[m] > P_k[m-1] \\ \max\!\left(P_{k,\min},\; 0.995 \cdot P_k[m-1] + 0.005 \cdot R_k[m]\right) & \text{otherwise} \end{cases}$$
     $$\hat{E}_k[m] = \text{clamp}\!\left(\frac{R_k[m]}{P_k[m]},\; 0.0,\; 1.0\right)$$
4. **Instantaneous-Attack, Exponential-Decay Envelopes ($I_k[m]$):**
   - Attack is instantaneous ($0\text{ ms}$); release follows true exponential decay $e^{-\Delta t / \tau_k}$:
     $$I_k[m] = \begin{cases} \hat{E}_k[m] & \text{if } \hat{E}_k[m] \ge I_k[m-1] \\ I_k[m-1] \cdot e^{-4\text{ ms} / \tau_k} & \text{otherwise} \end{cases}$$
   - Per-block decay multipliers at $\Delta t = 4\text{ ms}$ (in default `MATH_ROCK` / crisp transient mode):
     - **Band 1 ($\tau_1 = 150\text{ ms}$):** $\text{decay}_1 = e^{-4/150} \approx 0.9737$
     - **Band 2 ($\tau_2 = 90\text{ ms}$):** $\text{decay}_2 = e^{-4/90} \approx 0.9565$
     - **Band 3 ($\tau_3 = 60\text{ ms}$):** $\text{decay}_3 = e^{-4/60} \approx 0.9355$
     - **Band 4 ($\tau_4 = 40\text{ ms}$):** $\text{decay}_4 = e^{-4/40} \approx 0.9048$
5. **Spatial Ping-Pong Onset Parity (`pingPongParity`):**
   - When Band 3 (Mid-High tapping) or Band 4 (Air) exhibits a positive onset edge ($\hat{E}_3[m] - I_3[m-1] > \theta_{\text{onset}}$ after a `45 ms` refractory cooldown), Core 1 toggles `pingPongParity ^= 1`.

---

### 3.5 8-Channel Visual Choreography & Spatial Counterpoint (Core 0)

In `AMBIENT_MODE_AUDIO_REACTIVE`, the 8 channels (`s1.w, s1.r, s1.g, s1.b` and `s2.w, s2.r, s2.g, s2.b`) are synthesized from three perceptual layers scaled by `ambientBrightness`:

1. **Steady Ambient Baseline Floor (`15%–20%`):**
   - Prevents the cabinet from strobing into pitch black between musical notes:
     $$\text{Channel Output} = \text{Steady Baseline Floor (20\%)} + \text{Dynamic Transient Envelope (0–80\%)}$$
2. **Symmetric Low-End Foundation (`Strip 1 == Strip 2` on `W` and `R`):**
   - **Band 1 (`subBass`):** Drives `W1` and `W2` symmetrically (plus a warm `0.35×` contribution to `R1, R2`), anchoring kick drums and bass notes evenly across the entire cabinet.
   - **Band 2 (`lowMid`):** Drives `R1` and `R2` symmetrically for snare hits and chord weight.
3. **Asymmetric Stereo Ping-Pong (`Strip 1` vs. `Strip 2` on `G` and `B`):**
   - **Band 3 (`highMid` tapped guitars) & Band 4 (`treble` cymbals/hi-hats):** Alternate across `G1/B1` and `G2/B2` according to `pingPongParity` and **Dynamic Spatial Width**:
     - When overall musical energy is sparse, both strips blend toward a unified wash.
     - As transient density increases, stereo separation expands so consecutive tapped guitar notes and hi-hat subdivisions physically bounce left-to-right between Strip 1 and Strip 2.

---

## 4. TinyML Macro-Context Classifier (Stage 4 — Core 1)

### 4.1 Semantic Role

The neural network **never** attempts to detect individual beats. It is planned to execute once every **1.5 seconds** on Core 1 (runtime and CPU load unmeasured) to identify the overarching acoustic environment and adjust DSP parameters accordingly.

### 4.2 Log-Mel Spectrogram Feature Extraction

- **Audio Window:** `1.5 seconds` @ $16,000\text{ Hz}$ mono PCM $= 24,000\text{ samples}$.
- **STFT Framing:** `512`-sample (`32 ms`) periodic Hann window with `256`-sample (`16 ms`, 50% overlap) hop stride, no centering or end padding: `1 + floor((24000 - 512) / 256) = 92` full frames (discard final 192 samples).
- **Mel Filterbank:** `32` triangular Mel bins spanning $50\text{ Hz} \text{ to } 8,000\text{ Hz}$, log-compressed ($\log_{10}(E_{\text{mel}} + \epsilon)$) and quantized to `int8` ($[-128, +127]$).
- **Input Tensor Dimensions:** `[1, 92, 32, 1]` (`2,944 bytes`).

Training/runtime preprocessing contract (future implementation): signed PCM16 / 32768, continuous DC blocker R=0.995 with reset at stream start, no AGC on classifier input; periodic Hann `0.5 - 0.5*cos(2*pi*n/512)`. Use an unnormalized real FFT, power `abs(FFT)^2 / 512` for bins 0..256 without doubling. Place 34 equally spaced HTK mel points (`2595*log10(1+f/700)`) from 50 to 8000 Hz; evaluate triangular weights at FFT-bin frequencies, peak height 1, no area normalization. Features are `log10(max(mel_power, 1e-12))`, oldest time first, ascending mel, NHWC. No per-clip normalization. Export calibration-derived input scale/zero-point and quantize with nearest rounding (ties away from zero), clamp to [-128,127]; do not invent scale values before training. Share golden PCM/features/tensor fixtures across training and firmware before model acceptance.

### 4.3 2D Depthwise Separable CNN (`DS-CNN`) Topology

```text
Input Tensor: [1, 92, 32, 1] (int8 Log-Mel Spectrogram)
  │
  ├─► Stem Conv2D: 16 filters, 3×3 kernel, stride (2,2), BatchNorm + ReLU   ──► [46 × 16 × 16]
  ├─► DS-Block 1:  DWConv 3×3 (s 1,1) + PWConv 1×1 (32 filters) + BN + ReLU ──► [46 × 16 × 32]
  ├─► DS-Block 2:  DWConv 3×3 (s 2,2) + PWConv 1×1 (32 filters) + BN + ReLU ──► [23 ×  8 × 32]
  ├─► DS-Block 3:  DWConv 3×3 (s 1,1) + PWConv 1×1 (64 filters) + BN + ReLU ──► [23 ×  8 × 64]
  ├─► DS-Block 4:  DWConv 3×3 (s 2,2) + PWConv 1×1 (64 filters) + BN + ReLU ──► [12 ×  4 × 64]
  ├─► GlobalAveragePooling2D (collapses 12×4 spatial grid -> time-shift invariant) ──► [64]
  └─► Dense (64 -> 4 units) + Softmax ──► [NO_MUSIC, SPEECH_DIALOGUE, AMBIENT_POSTROCK, MATH_ROCK]
```

- All convolutions use SAME padding (extra padding on bottom/right), depth multiplier 1. Reproduce the dimensions/counts with `python3 scripts/check_model_spec.py`: `9,376` kernel weights and `1,484,544` MACs including dense, excluding bias/BN/pooling/activation/softmax and preprocessing. Int8 kernels alone occupy 9,376 bytes. Biases, folded BN, quantization metadata and flatbuffer structure add storage. Exported model size, tensor arena and inference time remain unmeasured.
- **Scheduling prerequisite:** A synchronous interpreter call is not preemptible merely because it is described as low priority. Implement bounded inference work slices interleaved with audio service (including splitting any operator exceeding the remaining 4 ms budget), or demonstrate a real preemptive strategy before Phase 4 integration. Reject/defer classifier work that cannot meet the measured deadline. An assumed 14 ms monolithic call would already miss multiple blocks.
- Budget DMA capacity in stereo bytes (`64 frames × 8 bytes = 512 bytes` per 4 ms); include worst-case service gaps, feature extraction and margin. DMA buffering alone does not remove latency. Measure maximum block age, actual overflow events, inference duration and render delay with networking/telemetry enabled. Current processed-sample telemetry is not an overflow counter.

### 4.4 Target Macro Classes & Temporal Hysteresis

| Class ID | Macro State | Acoustic Signature | Lighting & DSP Parameter Adaptation |
| :---: | :--- | :--- | :--- |
| **0** | `STATE_NO_MUSIC` | Silence, room floor, HVAC / appliance hum | Reverts to quiescent 30s $S^1$ ambient hue walk; DSP transient envelopes muted. |
| **1** | `STATE_SPEECH_DIALOGUE` | Conversation, podcasts, TV dialogue | Solid, flicker-free warm white glow; **all RGB transients muted** so speech never causes distracting flashes. |
| **2** | `STATE_AMBIENT_POSTROCK` | Slow chord swells, low spectral flux | Smooth breathing color washes; extended decay constants ($\tau_1 = 400\text{ ms}$, $\tau_{3,4} = 250\text{ ms}$); stereo ping-pong disabled (symmetric wash). |
| **3** | `STATE_MATH_ROCK` | High spectral flux, odd-meter drums, fretboard tapping | Crisp transient pops; fast decay constants ($\tau = 150, 90, 60, 40\text{ ms}$); **full stereo ping-pong active**. |

- **Temporal Hysteresis:** Requires `HYSTERESIS_CONFIRMATION_COUNT = 2` consecutive identical classifications (`~3.0 seconds`) before transitioning `current_state`, preventing momentary vocal shouts or brief musical pauses from flipping modes prematurely.

---

## 5. Staged Development Roadmap

1. **Phase 1 — Implemented and previously bench-verified:** Core 0 PWM, doors/buttons and Core 1 I2S sanity. Recheck affected behavior after review fixes using [bench validation](bench_validation.md).
2. **Phase 2 — Implemented; review fixes await deployment/bench validation:** Non-blocking Wi-Fi connection/retry, guarded OTA and Serial/UDP telemetry. Active OTA transfers block Core 0 controls/rendering while hardware PWM holds commanded levels. Physical cabinet installation remains unconfirmed.
3. **Phase 3 — In-Situ Audio DSP & Spatial Choreography (Flashed & Tuned via OTA):**
   - Implement the Core 1 DC blocker, 4-band `DF2T` IIR biquad filter bank, adaptive noise floor + rolling AGC peak tracker, instantaneous-attack exponential decay envelopes, and `pingPongParity` detector.
   - Add `AMBIENT_MODE_AUDIO_REACTIVE` on Core 0 and tune the per-band gains and thresholds in-situ against the real room acoustics over Wi-Fi.
4. **Phase 4 — In-Situ UDP Audio Capture & TinyML DS-CNN Classifier:**
   - Stream 1.5-second in-situ 16 kHz PCM clips over Wi-Fi UDP from the mounted cabinet microphone to build the training dataset (`NO_MUSIC`, `SPEECH_DIALOGUE`, `AMBIENT_POSTROCK`, `MATH_ROCK`).
   - Train the `int8` 2D DS-CNN classifier, export `include/model.h`, and flash via OTA onto Core 1 with 2-step hysteresis state transitions.
