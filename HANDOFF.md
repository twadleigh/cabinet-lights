# Project Handoff & Live State (`HANDOFF.md`)

**Last Updated:** 2026-11-08  
**Current Phase:** Phase 2 Finalization (Pre-Commit Code Review Complete $\rightarrow$ Implementing Review Batches A, B & Hardware-Timed $\Sigma\Delta$ Dither Next)  
**Build Status:** ✅ Passing (`pico2w-ota` default & `pico2w-usb` — 0 warnings, 0 errors; RAM 14.3%, Flash 12.5% of 3MB + 1MB LittleFS OTA partition)

---

## 1. Current Codebase Status

| Module | Files | Status | Notes |
| :--- | :--- | :--- | :--- |
| **Docs & Rules** | [README.md](README.md), [AGENTS.md](AGENTS.md), [doc/HARDWARE_AND_BOM.md](doc/HARDWARE_AND_BOM.md), [doc/AUDIO_VISION.md](doc/AUDIO_VISION.md) | 🔄 Pending Sync | Bench photos converted to `.png` (`doc/cabinet-controller-top.png`, `doc/cabinet-controller-bottom.png`). Doc drift cleanup queued in Batch A below. |
| **Config & Secrets** | [include/config.h](include/config.h), [include/secrets.example.h](include/secrets.example.h), [platformio.ini](platformio.ini) | ✅ Verified | `PWM_FREQ_HZ = 20000UL`, `PWM_MAX_DUTY = 7500` (`6.67 ns` native ticks), `PWM_PEDESTAL_TICKS = 20` (`133 ns` cutoff), `PWM_MIN_PULSE_TICKS = 30` (`200 ns` matched minimum pulse). Stale `PIN_BTN_*` aliases and `DIM_PERCEPTUAL` removed; compile-time `static_assert` invariants added. |
| **OTA & Auto-Discovery** | [include/ota_manager.h](include/ota_manager.h), [src/ota_manager.cpp](src/ota_manager.cpp), [scripts/ota_discover.py](scripts/ota_discover.py), [scripts/udp_monitor.py](scripts/udp_monitor.py) | ✅ Verified | `WiFi.noLowPowerMode()` + boot-time `LittleFS` mount/cleanup + pre-OTA UDP handshake (`OTA_PREP` $\rightarrow$ `OTA_READY`) in `scripts/ota_discover.py`. |
| **LED Driver** | [include/led_driver.h](include/led_driver.h), [src/led_driver.cpp](src/led_driver.cpp) | 🔄 Queued Upgrade | Currently running software-loop `1 kHz` $\Sigma\Delta$ dither (`PWM_PEDESTAL_TICKS = 20`, `PWM_MIN_PULSE_TICKS = 30`). Queued for upgrade to hardware-timed `20 kHz` PWM wrap ISR $\Sigma\Delta$ dithering on Core 0. |
| **Door Engine** | [include/door_engine.h](include/door_engine.h), [src/door_engine.cpp](src/door_engine.cpp) | ✅ Verified | 2 controller modes (`MODE_NORMAL`: 30s random $S^1$ hue walk at 13-step `BRIGHTNESS_TABLE` (`0%–100%`) when door closed + 5s perceptual fade to 100% Task White when door open; `MODE_DEBUG`: auto-cycles all 8 MOSFET channels at 100% for 1.5s each). |
| **Button Manager** | [include/button_manager.h](include/button_manager.h), [src/button_manager.cpp](src/button_manager.cpp) | ✅ Verified | 35 ms non-blocking debounce on 6 pins: `doorLeft` (`GP0`), `doorRight` (`GP1`), `btnModeNext` (Btn 1 / `GP8`), `btnModePrev` (Btn 2 / `GP9`), `btnBrightUp` (Btn 3 / `GP10`), `btnBrightDown` (Btn 4 / `GP11`). |
| **Audio DSP (Core 1)** | [include/audio_dsp.h](include/audio_dsp.h), [src/audio_dsp.cpp](src/audio_dsp.cpp) | ✅ Bench Verified | Drains 16 kHz / 32-bit I2S FIFO with 1-pole DC blocker (`y[n] = x[n] - x[n-1] + 0.995*y[n-1]`) and peak-hold envelope. Queued for seqlock/atomic cross-core synchronization and post-OTA auto-resume in Batch B. |

---

## 2. Immediate Next Up (Follow-On Thread: Pre-Commit Code Review Fixes)

Before committing Phase 2, implement the three agreed batches from the Phase 2 Code Review (all findings and design details are self-contained below):

1. **Batch A — Churn Cleanup & Documentation Sync:**
   - **Code cleanup:**
     - `src/door_engine.cpp`: Fix `DEFAULT_BRIGHTNESS_INDEX = 10; // 55%` comment; remove dead `m = 0.0f` in `hueToRGB()`; deduplicate brightness UP/DOWN into a helper; consolidate `DEBUG_CHANNEL_NAMES`, `DEBUG_MODE_LABELS`, and channel switch into a single table; seed PRNG with `randomSeed(rp2040.hwrand32())` in `doorEngineInit()`.
     - `src/audio_dsp.cpp`: Replace `while (1) { delay(100); }` on I2S fault with `return;` so Core 1 still services OTA pause requests; update comment in `audioDspCore1Update()` explaining that `i2s.end()` must run on Core 1 so `AudioBufferManager` buffers aren't freed while Core 1 is inside `read32()`.
     - `src/ota_manager.cpp` & `include/ota_manager.h`: Compute subnet broadcast via `WiFi.localIP() | (~WiFi.subnetMask())`; document why both `OTA_PREP` and `ArduinoOTA.onStart()` call `audioDspStopForOta()`.
   - **Documentation sync (`AGENTS.md`, `README.md`, `doc/AUDIO_VISION.md`, `doc/HARDWARE_AND_BOM.md`):**
     - Canonical button mapping: Btn 1 (`GP8`) = Mode Next, Btn 2 (`GP9`) = Mode Prev, Btn 3 (`GP10`) = Brightness Up (or mode-specific parameter +), Btn 4 (`GP11`) = Brightness Down (or mode-specific parameter -). Remove stale `TASK` override / `HUE_WALK` / `STATIC_HUE` / `WARM_LOUNGE` references.
     - Update PWM specification from `10-bit (0–1023)` to `20 kHz, 7500-tick (6.67 ns)` with `PWM_PEDESTAL_TICKS = 20`, `PWM_MIN_PULSE_TICKS = 30`, and hardware-timed $\Sigma\Delta$ dithering.
     - Document `include/secrets.h`, `pio run -e pico2w-ota -t upload`, and `scripts/udp_monitor.py` in `README.md`.
2. **Batch B — Robustness Fixes:**
   - **Audio DSP Resume after Aborted/Failed OTA (`audio_dsp.*`, `ota_manager.cpp`):** Add `AUDIO_DSP_PAUSED` status and `audioDspResumeAfterOta()` so Core 1 automatically restarts `I2S` if `ArduinoOTA.onError` fires or if `OTA_PREP` times out (`15 s`) without an upload starting.
   - **Lock-Free Cross-Core State (`audio_dsp.cpp`, `ota_manager.cpp`):** Protect `sharedEnergy` with an atomic sequence counter (seqlock + `__dmb()`) and use `std::atomic` for cross-core status/handshake flags (`dspStatus`, `otaPauseRequested`, `otaPausedAck`, `fsReady`).
   - **Constant-Velocity Door Fade & Background Hue Walk (`door_engine.cpp`):** Replace `rampStartProgress` re-anchoring with a constant-rate integrator (`doorProgress += ±dt / DOOR_FADE_RAMP_MS`, clamped to `[0.0, 1.0]`) so mid-fade reversals maintain constant speed; remove `lastHueSampleTime = millis()` reset when returning from `MODE_DEBUG` to `MODE_NORMAL`.
   - **Optional OTA Authentication (`include/secrets.example.h`, `src/ota_manager.cpp`):** Support optional `OTA_PASSWORD` if non-empty.
3. **Hardware-Timed $\Sigma\Delta$ Dither (`src/led_driver.cpp`):**
   - Move the first-order $\Sigma\Delta$ modulator (`0 <-> 30` sub-threshold dither and `N <-> N+1` continuous tick dither) into a RAM-resident (`__not_in_flash_func`) `20 kHz` hardware PWM wrap ISR (`PWM_DEFAULT_IRQ_NUM()`) on Core 0 using `Q0.16` integer fixed-point accumulators (`uint16_t` overflow carry) and direct `pwm_set_both_levels()` register writes.
   - Eliminates `loop()` / Wi-Fi jitter on low-end dimming, pushes the Step 1 (`4%`) dither pattern from `~333 Hz` to `~6.67 kHz`, and keeps sub-threshold dimming smooth even while `ArduinoOTA` is servicing network packets.

---

## 3. Master Execution Plan

1. **Phase 1 — Bench Verification (✅ COMPLETE)**
2. **Phase 2 — Wi-Fi OTA, Wireless Telemetry & Bench Door/Button/Dither Fine-Tuning (🔄 Completing Review Batches A, B & Hardware Dither $\rightarrow$ Commit)**
3. **Phase 3 — In-Situ Audio DSP (4-Band IIR Filter Bank, AGC & Spatial Animation)**
4. **Phase 4 — In-Situ UDP Audio Capture & TinyML DS-CNN Classifier**
