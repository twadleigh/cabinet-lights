# CabinetOTA

Local fork of Arduino-Pico ArduinoOTA at commit
`fd65f6d4ab168d4bb181a3709738fe353ac93585`, files
`libraries/ArduinoOTA/src/ArduinoOTA.{h,cpp}`. Original copyright notices
and LGPL-2.1-or-later license are retained. Public class/global/header names
are changed to CabinetOTA so the bundled library cannot be selected accidentally.

Changes:

- Mandatory abortable `onBeforeUpdate` after authentication, before any
  LittleFS or Updater access. Missing/failed guard and filesystem uploads reject.
- Scoped UpdaterClass instance closes its file on all returns, before the
  application resumes Core 1. Upstream global Update retains the file on errors.
- Filesystem mount failures notify the error callback.
- Authentication waits expire after 15 seconds; replies must come from the
  initiating peer. Stream receive timeout uses wrap-safe elapsed time and exits after 10 seconds without data, matching the sender's per-chunk budget. A one-second limit proved too short on the installed board.

The espota wire protocol and optional password authentication remain compatible.
Host tests compile this actual transport with fake network/storage boundaries;
board validation is still required. On core upgrades, compare this fork against
upstream and rerun transport, lifecycle and firmware tests. Do not edit package caches.

The local `tools/cabinet_espota.py` is copied from `tools/espota.py` at the same pinned core revision. It preserves a final OK consumed alongside the last chunk acknowledgement and treats EOF without OK as failure, avoiding the upstream infinite result wait. PlatformIO selects this sender through `scripts/ota_discover.py`; `scripts/ota_upload.py` still forwards optional credentials without debug logging.
