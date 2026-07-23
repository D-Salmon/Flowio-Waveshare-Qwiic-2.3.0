#!/usr/bin/env python3
"""Rebuild binary/manifest.json from the artifacts that are actually shipped."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import re
from pathlib import Path


ARTIFACT_PATTERNS = (
    (
        re.compile(r"^(esp32s3-spiffs)-(.+)\.bin$"),
        "esp32s3-spiffs",
        "spiffs",
        "esp32-spiffs",
        "/fwupdate/spiffs",
    ),
    (
        re.compile(r"^(esp32s3)-(.+)\.bin$"),
        "esp32s3",
        "esp32s3",
        "esp32-firmware",
        "/fwupdate/flowio",
    ),
    (
        re.compile(r"^(.+)-v?([0-9][0-9A-Za-z._-]*)\.tft$"),
        "nextion",
        "nextion",
        "nextion-tft",
        "/fwupdate/nextion",
    ),
)


def iso_utc(timestamp: float) -> str:
    return dt.datetime.fromtimestamp(timestamp, dt.timezone.utc).isoformat(timespec="seconds")


def classify(path: Path) -> tuple[str, str, str, str, str]:
    for pattern, category, target, kind, route in ARTIFACT_PATTERNS:
        match = pattern.match(path.name)
        if match:
            title = match.group(1)
            version = match.group(2)
            return category, title, version, target, kind, route
    raise ValueError(f"unsupported release artifact name: {path.name}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary-dir", type=Path, default=Path("binary"))
    parser.add_argument(
        "--release",
        default=dt.datetime.now(dt.timezone.utc).date().isoformat().replace("-", "."),
    )
    args = parser.parse_args()

    binary_dir = args.binary_dir
    files = sorted(
        (path for path in binary_dir.iterdir() if path.is_file() and path.name != "manifest.json"),
        key=lambda item: item.name.lower(),
    )
    artifacts: dict[str, list[dict[str, object]]] = {}
    for path in files:
        category, title, version, target, kind, route = classify(path)
        data = path.read_bytes()
        artifacts.setdefault(category, []).append(
            {
                "title": title,
                "label": title,
                "version": version,
                "build_date": iso_utc(path.stat().st_mtime),
                "target": target,
                "path": path.name,
                "kind": kind,
                "route": route,
                "size": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
            }
        )

    manifest = {
        "schema": "flowio.firmware-manifest.v2",
        "generated_at": dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds"),
        "release": args.release,
        "artifacts": dict(sorted(artifacts.items())),
    }
    output = binary_dir / "manifest.json"
    output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote {output} with {len(files)} artifacts")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
