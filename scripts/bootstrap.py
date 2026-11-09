#!/usr/bin/env python3
"""Set up the same isolated build/deploy tools on macOS and Linux."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import venv

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-build", action="store_true", help="Install tools without compiling")
    args = parser.parse_args()
    if sys.version_info < (3, 10):
        for minor in range(14, 9, -1):
            candidate = shutil.which(f"python3.{minor}")
            if candidate:
                os.execv(candidate, [candidate, str(Path(__file__).resolve()), *sys.argv[1:]])
        parser.error("Install Python 3.10+ and put it on PATH (macOS: brew install python)")
    if not shutil.which("git"):
        parser.error("Install Git first (macOS: Xcode command line tools; Linux: your package manager)")
    env_dir = ROOT / ".venv"
    python = env_dir / "bin" / "python"
    if python.exists():
        version_ok = subprocess.run([str(python), "-c", "import sys; sys.exit(sys.version_info < (3, 10))"]).returncode == 0
        if not version_ok:
            parser.error("Existing .venv uses an older Python. Remove .venv and rerun bootstrap.")
    if not python.exists():
        try:
            venv.EnvBuilder(with_pip=True).create(env_dir)
        except (OSError, subprocess.CalledProcessError) as exc:
            parser.exit(1, f"Cannot create virtual environment: {exc}\nOn Debian/Ubuntu install python3-venv.\n")
    subprocess.run([str(python), "-m", "pip", "install", "-r", str(ROOT / "requirements-dev.txt")], check=True)
    secrets = ROOT / "include" / "secrets.h"
    try:
        with secrets.open("x") as output:
            output.write((ROOT / "include" / "secrets.example.h").read_text())
        secrets.chmod(0o600)
        print("Created include/secrets.h; configure Wi-Fi before deploying.", flush=True)
    except FileExistsError:
        print("Keeping existing include/secrets.h.", flush=True)
    if not args.skip_build:
        subprocess.run([str(env_dir / "bin" / "pio"), "run", "-e", "pico2w-usb", "-e", "pico2w-ota"], cwd=ROOT, check=True)
        subprocess.run([str(env_dir / "bin" / "pio"), "run", "-e", "pico2w-ota", "-t", "compiledb"], cwd=ROOT, check=True)
    print("Ready: source .venv/bin/activate, then pio run", flush=True)


if __name__ == "__main__":
    main()
