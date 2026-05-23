#!/usr/bin/env python3
"""Reset an ESP32-class board by pulsing serial control lines.

Defaults are aligned with this project's PlatformIO configuration:
  - port: COM6
  - baud: 115200

Requires pyserial:
  pip install pyserial
"""

from __future__ import annotations

import argparse
import sys
import time

try:
    import serial
except ImportError as exc:
    print("Missing dependency: pyserial. Install it with 'pip install pyserial'.", file=sys.stderr)
    raise SystemExit(2) from exc


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Reset a board connected over a serial port by toggling DTR/RTS."
    )
    parser.add_argument("--port", default="COM6", help="Serial port name (default: COM6)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate (default: 115200)")
    parser.add_argument(
        "--pulse-ms",
        type=int,
        default=100,
        help="Reset pulse duration in milliseconds (default: 100)",
    )
    parser.add_argument(
        "--settle-ms",
        type=int,
        default=100,
        help="Delay before and after the reset pulse in milliseconds (default: 100)",
    )
    return parser.parse_args()


def reset_device(port: str, baud: int, pulse_ms: int, settle_ms: int) -> None:
    pulse_s = max(pulse_ms, 0) / 1000.0
    settle_s = max(settle_ms, 0) / 1000.0

    with serial.Serial(port=port, baudrate=baud, timeout=1) as ser:
        ser.dtr = False
        ser.rts = False
        time.sleep(settle_s)

        # Common ESP32 reset pulse over USB-UART control lines.
        ser.rts = True
        time.sleep(pulse_s)
        ser.rts = False
        time.sleep(settle_s)


def main() -> int:
    args = parse_args()

    try:
        reset_device(args.port, args.baud, args.pulse_ms, args.settle_ms)
    except serial.SerialException as exc:
        print(f"Failed to reset device on {args.port}: {exc}", file=sys.stderr)
        return 1

    print(f"Reset pulse sent on {args.port} at {args.baud} baud.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())