---
title: "Firmware v1.3 Camera Streaming Technical Spec"
version: "1.1"
date: "2026-05-14"
status: "as-implemented"
author: "Firmware Team"
relates-to:
  - "epic-camera-firmware-v1.md"
  - "epic-camera-backend-v1.md"
  - "spec-frontend-v1-3-video-stream.md"
  - "api/docs/FIRMWARE_README.md"
---

# Firmware v1.3 Camera Streaming Technical Spec

## Document Purpose

This document defines the current firmware-backend streaming contract for Timer Camera F.

If this document differs from code in `src/timer_camera/*`, code is the source of truth.

---

## Scope

This document covers:

- camera authentication to backend
- JPEG frame transport over WebSocket
- online/offline lifecycle behavior
- reconnect and heartbeat behavior in firmware
- MQTT command control for stream start/stop

This document does not cover:

- portal UI implementation details
- admin UI workflows
- hardware pin mapping details
- OTA, recording, motion detection, clip storage

---

## High-Level Architecture

Timer Camera F is an independent device that opens an outbound WebSocket connection to backend and sends one complete JPEG frame per binary WS message.

Backend behavior (contract assumptions used by firmware):

- validates `{slug, apiKey}`
- marks camera online on accepted WS session
- stores latest frame in memory
- marks camera offline on close/error

Portal does not connect directly to camera; it consumes backend camera routes.

---

## Required Runtime Inputs

Firmware needs these runtime values:

- device MAC address
- `deviceApiKey` (issued in admin provisioning)
- Wi-Fi SSID/password
- backend WS base URL

Derived value:

- `cameraSlug` = normalized MAC (lowercase hex, no separators)
- example: `AA:BB:CC:DD:EE:FF` -> `aabbccddeeff`

Minimal provisioning shape:

```json
{
  "deviceMac": "AA:BB:CC:DD:EE:FF",
  "deviceApiKey": "plaintext-issued-once",
  "pairingCode": "AB3DEFGH",
  "wifiSsid": "example-ssid",
  "wifiPassword": "example-password",
  "backendWsBaseUrl": "wss://api.phasmida.eu"
}
```

---

## WebSocket Contract

### Endpoint

Firmware connects to:

```text
/ws/camera/{slug}?apiKey={key}
```

Production example:

```text
wss://api.phasmida.eu/ws/camera/aabbccddeeff?apiKey=plaintext-issued-once
```

### Backend-side expectations

- route path: `/ws/camera/:slug`
- `apiKey` in query param
- no custom header/cookie auth required for WS ingest
- WS is the ingest transport for video frames

### Auth and close semantics

Firmware should treat these cases distinctly:

- auth/config error (`4401` semantics)
- unclaimed/not-allowed path (`4403` semantics)
- superseded connection (`4000` semantics)

Important current client implementation detail:

- `timer_camera` currently detects auth/unclaimed reliably from handshake reason strings (`HTTP 401` / `HTTP 403`)
- in current `WStype_DISCONNECTED` handler, close code remains `0`, so 4401/4403 mapping is primarily reason-string based

---

## Frame Payload Contract

Backend accepts:

- binary WS messages as JPEG frame candidates
- minimum JPEG validation based on SOI marker (`0xFF 0xD8`)

Firmware sends:

- one complete JPEG frame per one binary WS message
- no custom wrapper
- no JSON metadata instead of raw JPEG
- no base64 payloads

Recommended frame shape:

```text
[FF D8 ...... FF D9]
```

---

## Online/Offline Lifecycle

Backend-side behavior used by firmware:

- online on accepted WS session
- last-seen refreshed on traffic (`binary`, `text`, `ping`, `pong`)
- offline on `close` or `error`

Heartbeat policy in firmware:

- send WS ping every 30s
- track pong/heartbeat freshness
- if no heartbeat within 60s, trigger controlled reconnect

---

## MQTT Stream Control

In this firmware, `stream-stop` and `stream-start` come over standard device MQTT command topic `phasmida/{slug}/cmd`.

Behavior:

- `stream-stop`
- stops stream and keeps it stopped until `stream-start`
- idempotent (`ack.status = ok`)

- `stream-start`
- resumes init flow (`CAMERA_INIT -> WS_CONNECTING -> STREAMING`)
- idempotent (`ack.status = ok`)

Notes:

- stream stop/start flag is runtime-only (not persisted to NVS)
- after reboot, default stream state is ON
- in `AUTH_FAILED` state, stream control commands are rejected with `auth_failed`

---

## Reconnect Policy (Implemented)

Current implementation has two reconnect paths:

- fast path during failed connect handshakes: 500ms steps up to 5s
- standard path after active sessions: exponential 1s -> 2s -> 4s -> ... capped at 60s

Reconnect is triggered by:

- network/socket errors
- disconnect events
- heartbeat timeout

For auth failures:

- firmware logs auth failure explicitly
- firmware must not spin in an aggressive reconnect loop

---

## Firmware State Machine (Recommended + Implemented Shape)

```text
BOOT
  -> LOAD_CONFIG
  -> WIFI_CONNECTING
  -> MQTT_CONNECTING
  -> CAMERA_INIT
  -> WS_CONNECTING
  -> STREAMING

branches:
  -> WIFI_PROVISIONING
  -> STREAM_PAUSED
  -> AUTH_FAILED
  -> CAMERA_INIT_FAILED
```

State semantics summary:

- `LOAD_CONFIG`: read Wi-Fi/MQTT/camera settings from NVS
- `WIFI_PROVISIONING`: enter provisioning if no Wi-Fi creds
- `CAMERA_INIT`: initialize sensor + warmup
- `WS_CONNECTING`: open WS session
- `STREAMING`: capture/send JPEG loop
- `STREAM_PAUSED`: keep MQTT command channel alive, do not stream frames
- `AUTH_FAILED`: credential failure path

---

## Main Loop Summary

1. load config
2. connect Wi-Fi
3. initialize camera
4. open WS `/ws/camera/{slug}?apiKey={key}`
5. on open: reset reconnect state and stream
6. in stream loop:
   - capture frame
   - send as WS binary
   - release framebuffer immediately
7. in parallel:
   - maintain heartbeat (`ping`/`pong`)
   - process MQTT command control
8. on close/error/timeout:
   - leave stream loop
   - reconnect with backoff

---

## Operational Recommendations

These are practical recommendations (not strict backend hard limits):

- prefer stable quality/rate over maximum quality
- use predictable frame cadence
- avoid blocking loops that can starve watchdog
- if network is slow, prefer latest-frame-wins behavior

Backend stores latest frame, so latest-frame-wins is compatible.

---

## Logging Requirements

Firmware logs should clearly distinguish:

- missing Wi-Fi config
- Wi-Fi connection failures
- camera init failures
- WS DNS/TCP/TLS failures
- WS auth failures
- heartbeat timeout
- reconnect attempt and current backoff
- frame send failures

---

## Acceptance Criteria

Implementation is integration-ready when all are true:

1. device opens WS on `/ws/camera/{slug}?apiKey={key}` with valid credentials
2. bad credentials lead to auth-failure flow without crashing device
3. unclaimed/forbidden route follows unclaimed failure flow
4. firmware sends raw JPEG as one binary WS message per frame
5. backend snapshot/stream token flows can consume ingested frames
6. firmware reconnects automatically after disconnect
7. heartbeat timeout causes controlled reconnect
8. firmware avoids tight reconnect loops on auth failure

---

## Out of Scope

Not defined by this contract:

- exact PlatformIO project layout
- exact board pin mapping
- specific captive portal library choice
- low-level bring-up tuning outside protocol behavior

---

## Short Implementation Checklist

1. derive slug from MAC
2. load `apiKey`
3. connect Wi-Fi
4. initialize camera
5. connect WS `/ws/camera/{slug}?apiKey={key}`
6. send complete JPEG frames as binary messages
7. run heartbeat + reconnect discipline
8. treat auth failure as credential/config issue

---

## Informational Note (Not Firmware Contract)

Portal can force preview mode independently of ingest path readiness.
That does not change firmware/backend WS ingest contract.
