# Security hardening

This kit uses fail-closed defaults for its network attack surface.

## Web administration

Outside access-point provisioning mode, every HTTP route and the WebSocket
handshake require HTTP Digest authentication. During first boot, the firmware
uses `FLOW_WEB_ADMIN_USER` (default: `admin`) and either:

- the build-time `FLOW_WEB_ADMIN_PASSWORD`, when non-empty; or
- a random 24-character password generated once and persisted in NVS.

The generated password is printed once to the physical serial console. Wi-Fi,
MQTT and web passwords are never returned by configuration GET endpoints.
Submitting an empty Wi-Fi or MQTT password preserves the existing secret. Send
`clear_pass=1` only when the password must be erased.

The access-point portal exposes only captive-portal assets and Wi-Fi/MQTT
provisioning endpoints without authentication. Operational controls, diagnostics,
configuration import, reboot, reset and update routes remain protected.
Its WPA2 password comes from `FLOW_PROVISIONING_AP_PASSWORD`, or is generated
randomly at boot and printed to the physical serial console when that build-time
value is empty. The former shared `flowio1234` password is no longer used.

Digest authentication prevents sending the password itself in clear text, but
HTTP traffic is not encrypted. Put the device on a trusted management VLAN and
do not expose port 80 to the Internet.

State-changing HTTP requests require the 128-bit per-boot token exposed as
`csrf_token` by the authenticated `/api/web/meta` response. Browser clients
send it in `X-Flow-CSRF`; requests with a cross-site `Origin` or
`Sec-Fetch-Site` are rejected. The `/wslog` handshake requires an `Origin`
matching the request `Host`. Command-line clients must first read
`/api/web/meta`, then include `X-Flow-CSRF` on `POST`, `PUT`, `PATCH` and
`DELETE` requests.

All HTTP responses include a restrictive baseline CSP, clickjacking,
MIME-sniffing, referrer, permissions and cross-origin resource headers. Inline
scripts and styles remain allowed because the embedded rescue and serial pages
still depend on them.

## Firmware updates

`FLOW_ALLOW_UNSIGNED_UPDATES` defaults to `0`. This rejects HTTP uploads and
remote update jobs because a filename, extension or ESP image header does not
prove authenticity.

For development only, a local build may opt in with:

```ini
-D FLOW_ALLOW_UNSIGNED_UPDATES=1
```

Production re-enablement should first use ESP32-S3 Secure Boot v2, flash
encryption, anti-rollback and signed artifacts. External FlowIO, Nextion and
SPIFFS update formats also need their own authenticated manifest/signature
verification before the network update switch is enabled.

## MQTT

MQTT uses `mqtts://`, the ESP certificate bundle and port 8883 by default.
Explicit `mqtt://` configuration is rejected while `FLOW_MQTT_REQUIRE_TLS=1`.

The optional remote-display HMI UDP transport has been removed. The firmware
does not open UDP port 42110; the local Nextion remains connected over UART.
