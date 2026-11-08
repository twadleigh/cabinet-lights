#!/usr/bin/env python3
"""
UDP Wireless Telemetry Monitor for Cabinet Lighting Controller (Pico 2 W).
Listens on UDP port 44444 (default) for real-time broadcast logs from Core 0.
"""
import argparse
import socket
import sys
from datetime import datetime


def main() -> None:
    parser = argparse.ArgumentParser(description="Listen for Pico 2 W UDP telemetry broadcasts.")
    parser.add_argument("--port", type=int, default=44444, help="UDP port to listen on (default: 44444)")
    parser.add_argument("--duration", type=float, default=0.0, help="Stop after N seconds (0 = run forever)")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", args.port))
    sock.settimeout(0.5)

    print(f"Listening for Cabinet Controller UDP telemetry on 0.0.0.0:{args.port} ...", flush=True)
    start_time = datetime.now().timestamp()

    try:
        while True:
            if args.duration > 0 and (datetime.now().timestamp() - start_time) >= args.duration:
                break
            try:
                data, addr = sock.recvfrom(1024)
                line = data.decode("utf-8", errors="replace").rstrip()
                ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                print(f"[{ts} {addr[0]}] {line}", flush=True)
            except socket.timeout:
                continue
    except KeyboardInterrupt:
        print("\nStopped UDP monitor.")
        sys.exit(0)


if __name__ == "__main__":
    main()
