#!/usr/bin/env python3
"""Compile real firmware modules against host hardware stubs and check behavior."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="cabinet-host-") as tmp:
    binary = str(Path(tmp) / "regressions")
    subprocess.run([
        *shlex.split(os.environ.get("CXX", "c++")), "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-pthread",
        "-I" + str(ROOT / "test/host"), "-I" + str(ROOT / "include"),
        str(ROOT / "test/host/regressions.cpp"), "-o", binary,
    ], check=True)
    subprocess.run([binary], check=True)
    # External APIs require exact include spelling; aliases live only in temp storage.
    aliases = {"LittleFS": "little_fs", "MD5Builder": "md5_builder", "PicoOTA": "pico_ota",
               "SimpleMDNS": "simple_mdns", "StreamString": "stream_string", "Updater": "updater",
               "WiFi": "wifi", "WiFiUdp": "wifi_udp", "include/UdpContext": "include/udp_context"}
    for api, filename in aliases.items():
        alias = Path(tmp) / (api + ".h")
        alias.parent.mkdir(parents=True, exist_ok=True)
        alias.write_text('#include "' + str(ROOT / "test/host" / (filename + ".h")) + '"\n')
    ota_binary = str(Path(tmp) / "ota_regressions")
    subprocess.run([
        *shlex.split(os.environ.get("CXX", "c++")), "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-pthread",
        "-I" + tmp, "-I" + str(ROOT / "test/host"), "-I" + str(ROOT / "include"),
        "-I" + str(ROOT / "lib/cabinet_ota/src"),
        str(ROOT / "test/host/ota_regressions.cpp"), "-o", ota_binary,
    ], check=True)
    for scenario in ("fs_failure", "cleanup_failure", "prep_expiry", "auth_timeout", "bad_auth", "wrong_peer",
                     "main", "pause_failure", "begin_failure", "receive_timeout", "success", "connect_failure",
                     "filesystem_upload", "no_auth", "no_guard"):
        subprocess.run([ota_binary, scenario], check=True)
    # Exercise the actual uploader entry point without a board or real secrets.
    stub = Path(tmp) / "uploader.py"
    stub.write_text(
        'import os, sys\n'
        'assert "--debug" not in sys.argv\n'
        'assert sys.argv[sys.argv.index("--auth") + 1] == os.environ["CABINET_OTA_PASSWORD"]\n'
    )
    subprocess.run([
        sys.executable, str(ROOT / "scripts/ota_upload.py"), str(stub), "--debug",
    ], env=dict(os.environ, CABINET_OTA_PASSWORD='test $value; $(never) `never` "quote" \' apostrophe'), check=True)
    print("OTA authentication preserves literal password characters without debug logging.")

subprocess.run([sys.executable, str(ROOT / "test/host/ota_preflight.py")], check=True)
subprocess.run([sys.executable, str(ROOT / "scripts/check_model_spec.py")], check=True)
