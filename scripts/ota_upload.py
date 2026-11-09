#!/usr/bin/env python3
"""Run the platform's espota uploader with optional environment authentication."""
import os
import runpy
import sys


def main():
    uploader, *arguments = sys.argv[1:]
    # espota --debug prints all options, including the authentication password.
    arguments = [arg for arg in arguments if arg not in ("--debug", "-d")]
    password = os.environ.get("CABINET_OTA_PASSWORD", "")
    if password:
        arguments.extend(["--auth", password])
    sys.argv = [uploader, *arguments]
    runpy.run_path(uploader, run_name="__main__")


if __name__ == "__main__":
    main()
