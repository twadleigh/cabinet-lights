# Project Handoff & Live State

**Last Updated:** 2026-11-08
**Current Phase:** Phase 2 firmware implemented, deployed, and bench-verified; final commit-readiness audit passed. Physical cabinet installation from the roadmap is not yet confirmed. Startup default is level 4 (9%).
**Build:** Both `pico2w-usb` and `pico2w-ota` pass with zero warnings/errors. RAM 75,024 B (14.3%); flash 394,808 B (12.6% of 3 MB), plus 1 MB LittleFS. Changes are uncommitted.

## Development Environment

- macOS ARM64 is set up with project-local `.venv` (Python 3.12.13, PlatformIO 6.1.18).
- Fresh macOS/Linux clone: `python3 scripts/bootstrap.py`; then `source .venv/bin/activate`. Requires Git and Python 3.10+; bootstrap finds versioned Python on PATH when system Python is older. `--skip-build` installs only.
- Bootstrap installs tooling, creates ignored `include/secrets.h` only if absent, and builds both environments. Re-run verified to preserve existing credentials byte for byte. Local Wi-Fi credentials are configured; do not print or commit them.
- Platform pinned to `maxgerhardt/platform-raspberrypi` commit `5d4561a05e3b212660ac6fdd3fbfb328d1988aa1`; it pins Arduino-Pico `fd65f6d4ab168d4bb181a3709738fe353ac93585` and toolchain packages.
- Build directly: `.venv/bin/pio run -e pico2w-usb -e pico2w-ota`. Project sources use `-Werror`; `scripts/build_flags.py` suppresses the known dependency overloaded-virtual warning for C++ only.
- USB upload auto-detects ports: `.venv/bin/pio run -e pico2w-usb -t upload`. OTA: `.venv/bin/pio run -e pico2w-ota -t upload`. Optional `--upload-port` is respected.
- Optional firmware `OTA_PASSWORD` and matching uploader `CABINET_OTA_PASSWORD` are supported. `scripts/ota_upload.py` passes authentication within Python, preserving literal characters and removing espota's password-dumping debug mode. Authentication forwarding is host-tested; password-protected board uploads have not been tested.
- Wireless telemetry: `python3 scripts/udp_monitor.py` (UDP 44444). Linux device permissions/udev setup are documented in README.
- `.github/workflows/build.yml` adds macOS/Linux build and host checks. Linux/CI execution is pending; only macOS has been run locally.
- Open the project folder directly in VS Code (the user removed the separate workspace file). Shared `.vscode/settings.json`, `extensions.json`, and `tasks.json` provide PlatformIO IDE/clangd/Python/YAML recommendations and Bootstrap, Build Both (default build shortcut), Host Checks, USB/OTA Upload, Wireless/Serial Monitor, and Refresh C++ Index tasks. Tasks directly use project `.venv`; terminal PATH also includes `.venv/bin` on macOS/Linux. No workstation paths are committed.
- Bootstrap now generates ignored `compile_commands.json` for clangd after building. Clangd queries only the matching PlatformIO ARM compiler glob for embedded headers; Microsoft C++ IntelliSense is disabled in this workspace to avoid duplicate language servers. Run Refresh C++ Index after adding sources or changing compiler configuration.
- The user now uses VS Code locally. PlatformIO IDE is recommended in `.vscode/extensions.json` and installed in VS Code, alongside its C/C++ dependency. Clangd remains the configured C++ language server. Cursor can still use the shared tasks, but its marketplace did not offer PlatformIO IDE during setup. Use the actual VS Code CLI when verifying installation; the shell `code` command previously pointed to Cursor.

## Implemented Review Batches

- **Batch A:** Brightness helper, corrected brightness comments, unified debug channel table, dead hue math removal, hardware PRNG seed, non-blocking I2S fault handling, correct subnet broadcast, DMA-ownership/OTA comments, and controls/PWM documentation sync.
- **Batch B:** Atomic status/handshake/filesystem flags, atomic sequence-protected audio snapshots with memory barriers and atomic payloads; `AUDIO_DSP_PAUSED` plus Core 1 I2S restart after failed/abandoned OTA. Pause generations prevent stale acknowledgement reuse. PREP expires after 15 seconds even if Wi-Fi drops. OTA errors defer audio restart until `ArduinoOTA.handle()` returns, avoiding races with Update file cleanup. Door fade uses constant velocity through reversals, and hue continues across Debug transitions. Optional authentication added.
- **Hardware PWM dither:** Core 0 RAM-resident 20 kHz PWM wrap ISR, Q0.16 error accumulators, paired direct register writes, synchronized slices 0/1/2/5. TOP=7499 gives exactly 7500 ticks; compare 7500 is fully on. Perceptual conversion occurs outside ISR; frame publication briefly masks Core 0 interrupts. Below the matched floor dither off/30 ticks; otherwise adjacent integer ticks. Pedestal=20, minimum pulse=30; GPIO mapping unchanged. Control loop is now ~200 Hz.
- **Current audio remains Phase 2:** 16 kHz/32-bit I2S, DC blocker, peak-hold telemetry. Four-band IIR/AGC/spatial audio rendering and TinyML remain future work.
- **Canonical controls:** GP8 Mode Next, GP9 Mode Prev, GP10 Brightness Up, GP11 Brightness Down. Normal: 30 s shortest-arc random hue walk, 13 brightness steps 0–100% (default level 4 / 9%; level 0 is off), 5 s full-scale door fade. Debug: eight full-duty channels, 1.5 s each.

## Verification Completed

- Final commit-readiness audit: both firmware environments rebuilt with zero warnings/errors; host regression checks and Python compilation passed; `git diff --check` passed. `include/secrets.h`, `.venv`, and generated compilation database are ignored and credentials are not tracked. All implementation/support files remain uncommitted, including the new bootstrap, VS Code tasks/settings, CI workflow, and host checks.

- Both firmware environments compiled cleanly, including with configured local Wi-Fi credentials; bootstrap re-run passed, including new compilation-database generation. Workspace/task/settings JSON validates. Clangd resolves the ARM cross-compiler system headers and checks main.cpp and led_driver.cpp with zero errors.
- `python3 scripts/check_host.py` passes: PWM mean duty over complete accumulator cycles, minimum pulse, channel mapping/full off/full on, constant-speed door reversal, hue continuity, brightness bounds, I2S fault/pause/resume, stale ACK rejection, concurrent coherent snapshots, and literal authentication forwarding. Python scripts compile; `git diff --check` passes.
- PWM handler disassembly verified in SRAM at `0x20000120`, with inline register writes and no calls into flash.
- Actual Pico 2 W USB auto-detection, upload and flash verification succeeded on this Mac.
- Actual mDNS discovery, OTA_PREP/OTA_READY, two OTA uploads/reboots, UDP telemetry and live microphone data verified. Final OTA upload succeeded after the forced failure test.
- With USB disconnected, verified over Wi-Fi: abandoned PREP resumed audio after 15 seconds; a deliberately unavailable OTA TCP callback produced errors (2)/(4), followed by Core 1 ONLINE and live mic data. No firmware was sent during the failure test.
- User visually confirmed dimming looks good with the deployed hardware-timed dither.
- User reports all remaining manual checks passed (door/reversal, both reed switches, buttons/brightness bounds, Debug sequence, external-power startup).
- Startup default changed to level 4 (9%, counting level 0 as off); both firmware builds pass with zero warnings/errors and host regression checks pass. OTA deployment succeeded; live wireless telemetry and microphone data confirmed afterward. User adjusted brightness after the update; current level 3 (7%) is a manual selection, not the configured startup default.

## Remaining Work

1. Commit the completed Phase 2 firmware/tooling changes when desired. Confirm physical cabinet installation before declaring the entire Phase 2 roadmap complete. Manual bench checks passed; no oscilloscope timing measurements were made. Linux CI and password-protected board OTA remain unverified as noted above.
2. Phase 3: in-situ four-band DF2T IIR bank, AGC, transient envelopes, spatial mapping.
3. Phase 4: UDP audio capture and DS-CNN macro classifier. Preserve `doc/AUDIO_VISION.md` guardrails; no FFT visualizer or phase-locked BPM loop.
