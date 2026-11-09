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
