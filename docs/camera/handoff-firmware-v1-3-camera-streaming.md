---
title: "Firmware v1.3 Camera Streaming Handoff"
version: "1.0"
date: "2026-05-14"
status: "handoff-ready"
owner: "Backend Team"
source-of-truth:
  - "spec-firmware-v1-3-camera-streaming.md"
---

# Firmware v1.3 Camera Streaming Handoff

## What The Firmware Team Must Implement

1. Generate `slug` locally from MAC address:
- `AA:BB:CC:DD:EE:FF` -> `aabbccddeeff`
- exactly 12 characters, lowercase hex

2. Otvoriti outbound WebSocket:
- `/ws/camera/{slug}?apiKey={deviceApiKey}`

3. after `open` odmah krenuti slati frameove:
- jedna WS binary poruka = jedan kompletan JPEG frame
- backend verificationva minimalno SOI (`FF D8`)

4. Heartbeat i reconnect su firmware odgovornost:
- ping svakih 30s
- if there is no pong/heartbeat confirmation for 60s, close WS and reconnect
- exponential backoff: 1s, 2s, 4s, 8s ... cap 60s

## Required Input Data For Firmware

- `deviceMac`
- `deviceApiKey` (from `POST /admin/devices`)
- `pairingCode` (from `POST /admin/devices`, used for user claim)
- Wi-Fi SSID/password
- backend base URL

## Precondition Before Ingest

Device must be claimed by a user through:
- `POST /devices/claim`

If it is not claimed, WS will be rejected with close code `4403`.

## WS Close Code Handling

- `4401`: credential/config error (do not treat as a normal transient network failure)
- `4403`: device is not claimed (provisioning/claim problem)
- `4000`: backend closed the old connection because a new one arrived with the same credentials; this is not an error, reconnect normally

## Out Of Firmware Scope

- Portal UI
- stream token lifecycle for browser
- admin linking of camera to sensor

## Quick Validation (Acceptance Checklist)

- device connects to `/ws/camera/{slug}?apiKey={key}`
- bad key returns `4401`
- unclaimed device returns `4403`
- valid WS ingest enables backend stream-token/stream/snapshot path
- reconnect works automatically without manual reset

## Team Handoff

Send to firmware team:
1. This handoff as the operational document.
2. The full spec as the normative source of truth for edge cases and details.
