#!/usr/bin/env python3
"""Fail when the compiled application exceeds its configured OTA partition budget."""

from __future__ import annotations

import argparse
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / ".pio/build/Waveshare-ESP32-S3/firmware.bin"
PARTITION_SIZE = 0x200000


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--max-percent", type=float, default=85.0)
    args = parser.parse_args()
    if not 0.0 < args.max_percent <= 100.0:
        parser.error("--max-percent must be in ]0, 100]")
    if not FIRMWARE.is_file():
        parser.error(f"firmware not found: {FIRMWARE}")

    size = FIRMWARE.stat().st_size
    percent = size * 100.0 / PARTITION_SIZE
    print(f"firmware size: {size}/{PARTITION_SIZE} bytes ({percent:.2f}%)")
    if percent > args.max_percent:
        parser.error(
            f"firmware exceeds budget: {percent:.2f}% > {args.max_percent:.2f}%"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
