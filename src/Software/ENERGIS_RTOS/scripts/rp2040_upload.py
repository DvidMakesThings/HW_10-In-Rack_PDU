#!/usr/bin/env python3
# _FirmwareTools/rp2040_upload.py
# -*- coding: utf-8 -*-
"""
================================================================================
ENERGIS RP2040 UF2 Firmware Uploader (UART-triggered BOOTSEL)
--------------------------------------------------------------------------------
Purpose:
    Remotely trigger BOOTSEL mode on a running RP2040 device via UART, then
    upload a .uf2 firmware image to the exposed RPI-RP2 USB mass storage drive.

Usage:
    python rp2040_upload.py --p COM3 --b 115200 --f firmware.uf2

Arguments:
    --f          Path to the .uf2 firmware file
    --p          Serial port (e.g. COM6 or /dev/ttyUSB0)
    --b          Baudrate for the serial port (e.g. 115200)
    -h, --help   Show this help message and exit

Behavior:
    - Sends the command `BOOTSEL\n` over UART to the target device.
    - The device sets a flag and reboots into USB boot mode.
    - The script waits for the RPI-RP2 volume to appear.
    - The firmware is copied to the USB mass storage.

Platform Support:
    - Windows: scans drive letters D: to Z:
    - Linux:   scans /media/<user>/RPI-RP2
    - macOS:   scans /Volumes/RPI-RP2

Requirements:
    - The target firmware must implement:
          bootloader_trigger = 0xDEADBEEF;
          //watchdog_reboot(0, 0, 0);
      and early boot check for reset_usb_boot() when flag is set.
================================================================================
"""

import argparse
import getpass
import platform
import subprocess
import sys
import time
from pathlib import Path
from shutil import which

import serial
from serial.tools import list_ports


# ---------- Utils ----------

def ts() -> str:
    return time.strftime("%H:%M:%S")


def port_present(target: str) -> bool:
    tgt = target.upper()
    for p in list_ports.comports():
        if p.device.upper() == tgt:
            return True
    return False


def find_rpi_drive() -> Path | None:
    system = platform.system()
    if system == "Windows":
        for letter in "DEFGHIJKLMNOPQRSTUVWXYZ":
            drive = Path(f"{letter}:/")
            if (drive / "INFO_UF2.TXT").exists():
                return drive
    elif system == "Linux":
        user = getpass.getuser()
        base = Path(f"/media/{user}")
        if base.exists():
            for mount in base.rglob("RPI-RP2"):
                return mount
    elif system == "Darwin":
        for mount in Path("/Volumes").rglob("RPI-RP2"):
            return mount
    return None


def wait_drive_disappears(timeout_s: float = 30.0) -> bool:
    """Wait until RPI-RP2 drive is gone (board rebooted)."""
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if find_rpi_drive() is None:
            return True
        time.sleep(0.25)
    return False


def build_putty_cmd(putty_path: str, com_port: str, baud: int, sercfg: str | None, extra: list[str]) -> list[str]:
    # PuTTY serial config: speed,databits,parity,stopbits,flow (flow: N/X/R/D)
    cfg = sercfg if sercfg else f"{baud},8,n,1,N"
    return [putty_path, "-serial", com_port, "-sercfg", cfg] + extra


# ---------- Core ops ----------

def trigger_bootsel(port: str, baudrate: int):
    print(f"[{ts()}] [INFO] Sending BOOTSEL on {port} @ {baudrate}...")
    try:
        with serial.Serial(port, baudrate, timeout=1) as ser:
            time.sleep(0.2)  # let DTR settle
            ser.write(b"BOOTSEL\n")
            ser.flush()
            print(f"[{ts()}] [INFO] Command sent.")
            time.sleep(0.5)
    except serial.SerialException as e:
        print(f"[{ts()}] [ERROR] Failed to open/write {port}: {e}")
        sys.exit(1)


def upload_firmware(uf2_path: Path, wait_appear_s: float = 20.0):
    print(f"[{ts()}] [INFO] Waiting for RPI-RP2 USB drive...")
    deadline = time.time() + wait_appear_s
    drive = None
    while time.time() < deadline:
        drive = find_rpi_drive()
        if drive:
            break
        time.sleep(0.25)
    if not drive:
        print(f"[{ts()}] [ERROR] RPI-RP2 drive not found.")
        sys.exit(3)

    dest = drive / uf2_path.name
    print(f"[{ts()}] [INFO] Uploading '{uf2_path}' → '{dest}'")
    with open(uf2_path, "rb") as src, open(dest, "wb") as dst:
        dst.write(src.read())
    print(f"[{ts()}] [OK]   Firmware uploaded.")

    print(f"[{ts()}] [INFO] Waiting for board to reboot (RPI-RP2 to disappear)...")
    if not wait_drive_disappears(timeout_s=30.0):
        print(f"[{ts()}] [WARN] RPI-RP2 still mounted; continuing anyway.")


def watch_and_spawn_putty(com_port: str, baud: int, sercfg: str | None,
                          putty_path: str | None, poll: float, cooldown: float,
                          kill_on_disconnect: bool, extra: list[str]):
    putty = putty_path or which("putty")
    if not putty:
        print(f"[{ts()}] [ERROR] PuTTY not found. Install it or pass --putty C:\\Path\\putty.exe")
        sys.exit(4)

    cmd = build_putty_cmd(putty, com_port, baud, sercfg, extra)
    print(f"[{ts()}] [INFO] Watching for {com_port}. Will run: {' '.join(cmd)}")

    child: subprocess.Popen | None = None
    last_launch = 0.0

    try:
        while True:
            present = port_present(com_port)

            if present:
                # recycle finished child
                if child and (child.poll() is not None):
                    child = None

                # launch (or relaunch) PuTTY when port is present and no child
                if child is None and (time.time() - last_launch) > cooldown:
                    try:
                        print(f"[{ts()}] [INFO] {com_port} present → launching PuTTY")
                        child = subprocess.Popen(cmd)
                        last_launch = time.time()
                    except Exception as e:
                        print(f"[{ts()}] [WARN] Failed to start PuTTY: {e}")
                        time.sleep(max(poll, 1.0))
                        continue
            else:
                if child and kill_on_disconnect:
                    if child.poll() is None:
                        print(f"[{ts()}] [INFO] {com_port} disappeared → terminating PuTTY")
                        try:
                            child.terminate()
                        except Exception:
                            pass
                    child = None

            time.sleep(poll)
    except KeyboardInterrupt:
        print(f"\n[{ts()}] [INFO] Exiting watcher.")
        sys.exit(0)


# ---------- CLI ----------

def main():
    ap = argparse.ArgumentParser(
        description="ENERGIS RP2040 Firmware Uploader (+ optional PuTTY watcher)",
        formatter_class=argparse.RawTextHelpFormatter
    )
    ap.add_argument("--f", help="Path to .uf2 firmware file")           # was required=True
    ap.add_argument("--term-only", action="store_true",                 # NEW
                    help="Skip BOOTSEL/UF2; just watch the COM port and launch PuTTY")
    ap.add_argument("--p", required=True, help="Serial port for BOOTSEL trigger and PuTTY")
    ap.add_argument("--b", required=True, type=int, help="Baudrate for UART (and PuTTY)")
    ap.add_argument("--auto-putty", action="store_true",
                    help="After upload (or in term-only), watch for the COM port and (re)launch PuTTY")
    ap.add_argument("--putty", default=None, help="Path to putty.exe (defaults to 'putty' in PATH)")
    ap.add_argument("--sercfg", default=None, help="PuTTY -sercfg string, e.g. '115200,8,n,1,N'")
    ap.add_argument("--poll", type=float, default=0.5, help="Watcher polling interval seconds")
    ap.add_argument("--cooldown", type=float, default=1.5, help="Min seconds between PuTTY launches")
    ap.add_argument("--kill-on-disconnect", action="store_true", help="Terminate PuTTY when the COM disappears")

    # Parse known args and forward any extras (like -sessionlog) to PuTTY
    args, putty_extra = ap.parse_known_args()
    putty_extra = [x for x in putty_extra if x != "--"]  # just in case

    print("=== ENERGIS RP2040 Firmware Uploader ===")

    if not args.term_only:
        if not args.f:
            print(f"[{ts()}] [ERROR] Missing --f <file.uf2> (or pass --term-only).")
            sys.exit(2)
        uf2_file = Path(args.f)
        if not uf2_file.exists() or uf2_file.suffix.lower() != ".uf2":
            print(f"[{ts()}] [ERROR] File not found or not a .uf2: {uf2_file}")
            sys.exit(2)
        trigger_bootsel(args.p, args.b)
        upload_firmware(uf2_file)
    else:
        print(f"[{ts()}] [INFO] Terminal-only mode: skipping BOOTSEL/UF2 upload.")

    # In term-only you still want the watcher, so allow either flag to enable it
    if args.auto_putty or args.term_only:
        watch_and_spawn_putty(
            com_port=args.p,
            baud=args.b,
            sercfg=args.sercfg,
            putty_path=args.putty,
            poll=args.poll,
            cooldown=args.cooldown,
            kill_on_disconnect=args.kill_on_disconnect,
            extra=putty_extra
        )
    else:
        print(f"[{ts()}] [INFO] Done.")

if __name__ == "__main__":
    main()