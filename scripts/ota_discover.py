Import("env")
import os
import socket
import time
import sys
from pathlib import Path
sys.path.insert(0, str(Path(env.subst("$PROJECT_DIR")) / "scripts"))
from ota_limits import validate_image


def prepare_pico_for_ota(ip, udp_port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(0.4)
    try:
        for attempt in range(1, 6):
            sock.sendto(b"OTA_PREP", (ip, udp_port))
            try:
                data, peer = sock.recvfrom(128)
                text = data.decode("utf-8", errors="ignore").strip()
                if text == "OTA_READY" and peer == (ip, udp_port):
                    print(f"[OTA Auto-Discovery] Pre-OTA handshake OK ({text}) on attempt {attempt}")
                    time.sleep(0.05)
                    return True
            except socket.timeout:
                pass
    finally:
        sock.close()
    print("[OTA Auto-Discovery] No valid OTA_READY reply; upload aborted.")
    return False


def discover_pico_ip(source, target, env):
    if not source:
        raise RuntimeError("OTA artifact is missing; refusing upload")
    size, limit = validate_image(str(source[0]), env.GetProjectOption("board_build.filesystem_size", "1m"))
    print(f"[OTA] Artifact {size} bytes; conservative staging limit {limit} bytes")
    hostname = f"{env.GetProjectOption('custom_ota_hostname', 'cabinet-lights')}.local"
    target_host = env.subst("$UPLOAD_PORT") or hostname
    # Inject authentication inside Python so arbitrary passwords never pass
    # through SCons substitution, shell quoting, or command-line logging.
    original_uploader = os.path.join(env.subst("$PROJECT_DIR"), "lib", "cabinet_ota", "tools", "cabinet_espota.py")
    env.Replace(
        UPLOADER=os.path.join(env.subst("$PROJECT_DIR"), "scripts", "ota_upload.py"),
        UPLOADERFLAGS=[original_uploader, *env["UPLOADERFLAGS"]],
    )
    udp_port = int(env.GetProjectOption("custom_udp_telemetry_port", "44444"))
    resolved_ip = None

    # 1. Try OS / mDNS resolution first
    try:
        ip = socket.gethostbyname(target_host)
        if ip and not ip.startswith("127."):
            print(f"[OTA Auto-Discovery] Resolved {target_host} -> {ip}")
            resolved_ip = ip
    except Exception:
        pass

    # 2. Fallback: Listen for 1 Hz UDP telemetry broadcast from the Pico 2 W
    if not resolved_ip and target_host == hostname:
        print(f"[OTA Auto-Discovery] Listening on UDP :{udp_port} for {hostname} broadcast...")
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("0.0.0.0", udp_port))
        sock.settimeout(3.5)
        try:
            deadline = time.monotonic() + 3.5
            while time.monotonic() < deadline:
                sock.settimeout(max(0.01, deadline - time.monotonic()))
                data, addr = sock.recvfrom(1024)
                if data.startswith(b"[RUNNING] Door:"):
                    resolved_ip = addr[0]
                    print(f"[OTA Auto-Discovery] Found Pico 2 W via UDP telemetry at {resolved_ip}")
                    break
        except Exception as exc:
            print(f"[OTA Auto-Discovery] Warning: Could not auto-discover ({exc}); using {hostname}")
        finally:
            sock.close()

    if not resolved_ip:
        raise RuntimeError(f"Cannot resolve OTA target {target_host}; refusing upload")
    env.Replace(UPLOAD_PORT=resolved_ip)
    if not prepare_pico_for_ota(resolved_ip, udp_port):
        raise RuntimeError("Audio pause was not acknowledged; refusing upload")


env.AddPreAction("upload", discover_pico_ip)
