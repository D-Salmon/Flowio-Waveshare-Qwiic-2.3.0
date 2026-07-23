#!/usr/bin/env python3
"""
Inject build reference macros into the active PlatformIO environment.

Stable product versions stay in platformio.ini (`custom_version` / `FIRMW`),
while this script appends a build reference generated at compile time.
"""

from __future__ import annotations

from datetime import datetime
import os
import re

Import("env")  # type: ignore[name-defined]


def _strip_quotes(value: str) -> str:
    if len(value) >= 2 and value[0] == value[-1] and value[0] in ("'", '"'):
        return value[1:-1]
    return value


core_version = _strip_quotes(str(env.GetProjectOption("custom_version", "0.0.0")))
build_ref = os.environ.get("FLOW_BUILD_REF", "").strip()
if build_ref:
    if not re.fullmatch(r"\d{8}\.\d{6}", build_ref):
        raise ValueError("FLOW_BUILD_REF must use YYYYMMDD.HHMMSS")
else:
    build_ref = datetime.now().strftime("%Y%m%d.%H%M%S")
full_version = f"{core_version}+{build_ref}"

env.Append(
    CPPDEFINES=[
        ("FLOW_BUILD_REF", f'\\"{build_ref}\\"'),
        ("FLOW_FIRMWARE_VERSION_FULL", f'\\"{full_version}\\"'),
    ]
)

print(f"[build-version] core={core_version} build_ref={build_ref} full={full_version}")
