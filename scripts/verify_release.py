#!/usr/bin/env python3
"""Fail-fast checks for a Flow.io source/release kit."""

from __future__ import annotations

import gzip
import hashlib
import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def verify_manifest() -> None:
    binary_dir = ROOT / "binary"
    manifest_path = binary_dir / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schema") != "flowio.firmware-manifest.v2":
        fail("manifest schema is not v2")

    entries = [
        entry
        for group in manifest.get("artifacts", {}).values()
        for entry in group
    ]
    listed = {entry["path"] for entry in entries}
    actual = {
        path.name
        for path in binary_dir.iterdir()
        if path.is_file() and path.name != "manifest.json"
    }
    if listed != actual:
        fail(f"manifest/files mismatch: missing={sorted(actual-listed)} stale={sorted(listed-actual)}")

    for entry in entries:
        path = binary_dir / entry["path"]
        data = path.read_bytes()
        if entry.get("size") != len(data):
            fail(f"bad size in manifest for {path.name}")
        if entry.get("sha256") != hashlib.sha256(data).hexdigest():
            fail(f"bad SHA-256 in manifest for {path.name}")


def verify_gzip_assets() -> None:
    for archive in (ROOT / "data").rglob("*.gz"):
        source = archive.with_suffix("")
        if not source.is_file():
            fail(f"gzip asset has no source: {archive.relative_to(ROOT)}")
        try:
            expanded = gzip.decompress(archive.read_bytes())
        except (OSError, EOFError) as exc:
            fail(f"invalid gzip {archive.relative_to(ROOT)}: {exc}")
        if expanded != source.read_bytes():
            fail(f"stale gzip asset: {archive.relative_to(ROOT)}")


def verify_security_defaults() -> None:
    web = (ROOT / "src/Modules/Network/WebInterfaceModule/WebInterfaceServer.cpp").read_text(
        encoding="utf-8"
    )
    updater = (ROOT / "src/Modules/Network/FirmwareUpdateModule/FirmwareUpdateModule.cpp").read_text(
        encoding="utf-8"
    )
    web_header = (
        ROOT / "src/Modules/Network/WebInterfaceModule/WebInterfaceModule.h"
    ).read_text(encoding="utf-8")
    mqtt = (ROOT / "src/Modules/Network/MQTTModule/MQTTTransport.cpp").read_text(encoding="utf-8")
    provisioning = (
        ROOT / "src/Modules/Network/WifiProvisioningModule/WifiProvisioningModule.cpp"
    ).read_text(encoding="utf-8")
    config_store = (ROOT / "src/Core/ConfigStore.cpp").read_text(encoding="utf-8")
    required = (
        ("HTTP authentication middleware", "requestAuthentication(\"Flow.io\", true)", web),
        ("Web authentication rate limit", "webAuthRateLimited_(request", web),
        ("Web authentication failure log", "noteWebAuthFailure_(request)", web),
        ("Web authentication global rate limit", "webAuthGlobalBlockedUntilMs_", web),
        ("Web authentication protected eviction", "oldestEvictable", web),
        ("Web authentication 32-source table", "kWebAuthThrottleSlots = 32U", web_header),
        ("CSRF middleware", "csrfRequestAllowed_(request)", web),
        ("CSRF response token", 'doc["csrf_token"] = csrfToken_', web),
        ("WebSocket origin validation", "websocketHandshakeAllowed_(request)", web),
        ("HTTP security headers", 'response->addHeader("Content-Security-Policy"', web),
        ("unsigned update default-off", "#define FLOW_ALLOW_UNSIGNED_UPDATES 0", updater),
        ("signed local OTA verification", "verifyOtaSignature_(digest", web),
        ("signed local OTA header", 'hasHeader("X-Flow-Signature")', web),
        (
            "local upload authentication and CSRF",
            "!webRequestAuthorized_(request) || !csrfRequestAllowed_(request)",
            web,
        ),
        ("MQTT TLS default-on", "#define FLOW_MQTT_REQUIRE_TLS 1", mqtt),
        ("MQTT CA bundle", "esp_crt_bundle_attach", mqtt),
        ("random provisioning password", "esp_random()", provisioning),
        ("JSON string escaping", "writeJsonEncodedString_", config_store),
        ("valid truncated JSON objects", "appendConfigJsonEntry_", config_store),
    )
    for label, needle, content in required:
        if needle not in content:
            fail(f"security invariant missing: {label}")
    if re.search(r'\\"pass\\":\\"%s\\"', web):
        fail("web response appears to serialize a password")
    if "flowio1234" in provisioning.lower():
        fail("legacy shared provisioning password is still present")

    forbidden_hmi_udp = (
        "HmiUdp",
        "RemoteHmiUdp",
        "FLOW_HMI_REMOTE_UDP",
        "FlowConnectDisplay",
        "42110",
    )
    for source_root in (ROOT / "src", ROOT / "include"):
        for path in source_root.rglob("*"):
            if path.suffix not in {".h", ".hpp", ".c", ".cpp"}:
                continue
            content = path.read_text(encoding="utf-8")
            for needle in forbidden_hmi_udp:
                if needle in content:
                    fail(
                        "removed remote-display transport remains in "
                        f"{path.relative_to(ROOT)}: {needle}"
                    )


def verify_dependencies() -> None:
    ini = (ROOT / "platformio.ini").read_text(encoding="utf-8")
    if re.search(r"@\s*\^", ini):
        fail("platformio.ini contains floating caret dependencies")
    if re.search(r"^\s*https://github\.com/.+\.git\s*$", ini, re.MULTILINE):
        fail("platformio.ini contains an unpinned Git dependency")
    if re.search(r"^\s*milesburton/DallasTemperature\s*$", ini, re.MULTILINE):
        fail("DallasTemperature is not version-pinned")
    required_versions = (
        "bblanchon/ArduinoJson @ 6.21.6",
        "ESP32Async/AsyncTCP @ 3.4.10",
        "ESP32Async/ESPAsyncWebServer @ 3.11.2",
        "nextion_firmware_version = '\"2.0.7\"'",
    )
    for expected in required_versions:
        if expected not in ini:
            fail(f"required hardened version missing: {expected}")


def main() -> int:
    verify_manifest()
    verify_gzip_assets()
    verify_security_defaults()
    verify_dependencies()
    print("release verification: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
