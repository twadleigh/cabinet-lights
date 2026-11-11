# Bench validation after repository review

Status: pending on the new firmware. Previous Phase 2 bench results do not validate these changes. Record date, Git SHA/build identity, supply/load, instruments, duration and observations in HANDOFF.md. Keep credentials out of logs. Do not run all tests blindly: resolve the power-path questions first.

## 1. Unpowered inspection and continuity

Disconnect USB and external power before inspecting/reseating parts. Identify the buck controller/module (the `4R7` inductor marking is insufficient), trace VO+/VSYS/buffer VCC and determine whether external-source isolation exists. Confirm actual buffer input bias/OE wiring. Do not alter invariant GPIO assignments. Check each reed: magnet near closes contact, magnet away opens it; record installed part number if available.

The [hardware guide](HARDWARE_AND_BOM.md) explains the unresolved power and reset behavior. The onboard USB diode does not isolate the buck output. Do not connect simultaneous USB and external power until reverse-current behavior or added ORing is established. Determine an appropriate isolated power arrangement for flashing; USB-only can itself energize a directly connected buck output and buffer rail.

## 2. Electrical checks (instruments required)

Once the actual source path is understood, measure USB-only, external-only and combined-source rail voltages and reverse current as applicable. Check the buffer supply against its datasheet, including diode drops. Start with the small bench strip segments. Observe buffer outputs/MOSFET gate voltages at power-on, MCU reset and shutdown; test absent-MCU behavior only after unpowered preparation and with the load safely limited. MCU reset does not disable the permanently enabled buffer. Record whether input pull-downs/controlled OE are needed.

Confirm channel routing and PWM with a scope if available: 20 kHz period, low-level minimum pulses, full-on levels, and behavior during Wi-Fi/OTA. A meter or visual observation cannot establish nanosecond pulse timing. Full-strip current, MOSFET/buffer/socket temperature and wiring/connector capacity remain separate acceptance checks under the actual cabinet load; a short test segment does not establish full-load thermal safety.

## 3. Deploy and check controls/audio

Use `.venv/bin/pio run -e pico2w-usb -t upload` only after power arrangements are settled; keep BOOTSEL recovery available. Confirm new `[HEALTH] Build:` identity over Serial or UDP. At boot, initial door state is CLOSED; an open contact becomes OPEN after debounce. Test both doors separately and together, 5-second full-scale fade and mid-fade reversal, Normal/Debug, and all 13 brightness steps (default 9%, off at step 0).

Press mode+brightness together: both apply once. Press Next+Prev or Up+Down together: that pair cancels when debounced in the same sample. Held buttons must not repeat; release/repress must work. Test at both brightness limits. Clap/silence should produce a finite decaying mic envelope; `[HEALTH] Samples` should advance near 16,000/s during normal capture. The four reported bands are still scaled copies of one peak, not a filter bank.

## 4. OTA success and recovery

Run `.venv/bin/pio run -e pico2w-ota -t upload` and verify reboot/build identity and audio recovery. Controls/rendering pause during transfer; hardware PWM retains commanded levels. Test abandoned PREP by sending exactly `OTA_PREP` to the board's UDP 44444, observing READY/PAUSED and then automatic resume after 15 seconds without an upload. Repeat with Wi-Fi interrupted after PREP, restore Wi-Fi and check reconnect/recovery. An interrupted upload must eventually return audio to ONLINE; retry immediately and verify success. Do not interrupt boot-time firmware installation to simulate a failed transfer.

If a nonempty OTA password is already configured, test matching, missing and wrong `CABINET_OTA_PASSWORD`: correct succeeds; wrong/missing must not replace firmware; audio resumes after rejection/timeout and a correct immediate retry succeeds. Do not rotate or print credentials. If authentication is disabled, record that fact and leave protected-board acceptance pending until credentials are deliberately configured. This is useful basic LAN protection for the hobby project, not encrypted transport or signed firmware verification; UDP PREP is unauthenticated.

Host tests inject filesystem and pause failures. Do not damage a working filesystem to reproduce them on a board. Storage-fault telemetry should leave lighting/audio operational and OTA disabled.

## 5. Finite software soak

Capture **30 minutes** with telemetry enabled:

```sh
python3 scripts/udp_monitor.py --duration 1800 > /tmp/cabinet_soak.log
```

Exercise doors/buttons for 5 minutes, normal operation for 10, one Wi-Fi disconnect/reconnect and abandoned PREP for 5, then normal operation for 10. Record OTA success/failure tests separately so expected reboots and long loop intervals are identifiable. Note host capture times for every deliberate event.

Acceptance: no unexplained reboot/build change, persistent DSP/storage fault, or unexplained loop stall. `[HEALTH]` fields:

- `UptimeMs`: 64-bit accumulated uptime, including setup; `Seq` increments each report (wraps at 2^32). Unexpected restarts of both suggest reset; UDP gaps alone do not.
- `MaxLoopMs`: lifetime maximum Core 0 loop interval, including blocking OTA. It is not an ISR latency measurement; relate spikes to deliberate events and retain pre-OTA baseline.
- `DSP`: 0 uninitialized, 1 online, 2 fault, 3 paused. `Samples` counts processed frames modulo 2^32; pause intentionally stops progress. The library exposes no instrumented overflow count here, so do not infer zero dropped samples.
- `FS`: boot storage health. `OTA`: READY/PREPARED/UPLOADING/STORAGE_FAULT/OFFLINE; while a transfer blocks the loop there may be no periodic upload report. `Reconnects` excludes initial association. Build identity is compile date/time; pair it with the recorded Git SHA.

UDP telemetry reports commanded light values and software state. It cannot prove current, gate waveform, reverse-current isolation, or component temperatures. Leave unavailable instrument checks pending rather than marking them passed.
