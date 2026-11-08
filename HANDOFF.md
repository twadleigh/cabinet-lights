# Project Handoff & Live State (`HANDOFF.md`)

**Last Updated:** 2026-11-08  
**Current Phase:** Stage 1 Complete $\rightarrow$ Ready for Stage 2 (Core 1 4-Band IIR Filter Bank)  
**Build Status:** ✅ Passing (`pico2w-usb` — 0 warnings, 0 errors; RAM 13.4%, Flash 7.4%)

---

## 1. Current Codebase Status

| Module | Files | Status | Notes |
| :--- | :--- | :--- | :--- |
| **Docs & Rules** | [README.md](README.md), [AGENTS.md](AGENTS.md), [doc/AUDIO_VISION.md](doc/AUDIO_VISION.md) | ✅ Complete | Root `README.md`, auto-loaded `AGENTS.md` (with embedded `HANDOFF.md`), and audio DSP spec in place. |
| **Config** | [include/config.h](include/config.h) | ✅ Complete | Pinout, 20 kHz PWM constants, $\gamma = 2.4$, perceptual color targets defined. |
| **LED Driver** | [include/led_driver.h](include/led_driver.h), [src/led_driver.cpp](src/led_driver.cpp) | ✅ Complete | 20 kHz, 10-bit (`0–1023`) PWM with CIE $L^*$ power-law gamma mapping (`powf(val, 2.4f)`). |
| **Door Engine** | [include/door_engine.h](include/door_engine.h), [src/door_engine.cpp](src/door_engine.cpp) | ✅ Complete | Smooth 5s perceptual fade (with mid-fade reversal lock); 3 ambient modes (`HUE_WALK`, `STATIC_HUE`, `WARM_LOUNGE`); brightness stepping (`10%–100%`); manual Task White override. |
| **Button Manager** | [include/button_manager.h](include/button_manager.h), [src/button_manager.cpp](src/button_manager.cpp) | ✅ Complete | 35 ms non-blocking debounce updates all 6 pins (`doorLeft`, `doorRight`, `btnMode`, `btnUp`, `btnDown`, `btnTask`) every tick before prioritizing button events. |
| **Audio DSP (Core 1)** | [include/audio_dsp.h](include/audio_dsp.h), [src/audio_dsp.cpp](src/audio_dsp.cpp) | 🚧 Stage 2 Next | Dual-core safe (`AudioDspStatus` reported to Core 0 without touching `Serial` on Core 1). Drains 16 kHz / 32-bit I2S FIFO into placeholder `AudioEnergy` struct. |

---

## 2. What Changed Last Turn

1. **Added Root [README.md](README.md):**
   - Documented hardware architecture, full pinout reference table, Core 0 / Core 1 software partitioning, PlatformIO build/flash commands, and documentation links.
2. **Completed Stage 1 Fixes:**
   - Removed `Serial` calls from Core 1 (`AudioDspStatus` polled by Core 0), ensured all 6 input pins update every debounce tick, and wired `MODE`, `UP`, `DOWN`, and `TASK` satellite buttons into `door_engine`.

---

## 3. Immediate Next Steps

- [ ] **Step 1 (Bench Verification — Optional):** Flash `pico2w-usb` and verify button actions (`MODE`, `UP`, `DOWN`, `TASK`) and reed switch fade transitions on hardware.
- [ ] **Step 2 (Stage 2 Audio DSP — Core 1):** Implement the 4-band Direct-Form II Transposed IIR biquad filter bank (`Fs = 16 kHz`: Sub 40–120 Hz, Low-Mid 200–800 Hz, Mid-High 1–3.5 kHz, Air >5 kHz) and instantaneous-attack exponential decay envelopes ($\tau = 150, 90, 60, 40\text{ ms}$) in [src/audio_dsp.cpp](src/audio_dsp.cpp) per [doc/AUDIO_VISION.md](doc/AUDIO_VISION.md).
- [ ] **Step 3 (Stage 2 Visual Mapping — Core 0):** Add an audio-reactive mode in `door_engine` that maps the double-buffered 4-band `AudioEnergy` envelopes symmetrically to low-end (`W1/W2`, `R1/R2`) and ping-pong across `G1/B1` vs. `G2/B2` for high-mid/treble transients.
