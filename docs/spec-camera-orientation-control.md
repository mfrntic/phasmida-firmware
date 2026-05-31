---
title: "Camera Orientation Control — MQTT Command Specification"
version: "1.0"
date: "2026-05-29"
status: "active"
author: "Firmware Team"
relates-to:
  - "spec-firmware-v1-3-camera-streaming.md"
  - "MQTT-PROTOCOL-v1.md"
  - "spec-camera-quality-control.md"
---

# Camera Orientation Control — MQTT Command Specification

## Overview

This document defines the MQTT command interface for dynamically controlling **Timer Camera F** image orientation without full firmware redeploy.

Current firmware supports two persisted orientation modes:

- `0` degrees: sensor image is sent without the default 180 degree correction
- `180` degrees: sensor image is rotated by 180 degrees and matches the current housing-upright default

The selected orientation is saved to NVS and survives restart or power loss.

---

## Quick Start

### For Backend Developers

1. Publish `set-camera-orientation` to the camera MQTT command topic as the backend MQTT client
2. Camera validates the requested rotation and publishes ACK on `cmd/ack`
3. Camera saves the orientation to NVS
4. Camera reinitializes the OV3660 sensor with the new orientation
5. WebSocket stream reconnects and resumes with the new orientation

### For Frontend Developers

1. Expose an orientation selector with `0°` and `180°`
2. Display command ACK status to the user
3. Persist UI state from backend/device state if available
4. Avoid offering `90°` or `270°` for this firmware API version

---

## Command Contract

### Endpoint

**Topic:** `phasmida/{slug}/cmd`

**Publisher identity:** backend MQTT client (`username=phasmida`, `password=MQTT_BACKEND_PASSWORD`)

**Payload:** JSON object with structure below

### Command Payload Format

```json
{
  "cmdId": "01JWCAMORIENT1234567890",
  "type": "set-camera-orientation",
  "params": {
    "rotation": 180
  }
}
```

#### Required Fields

| Field | Type | Description |
|-------|------|-------------|
| `cmdId` | string | Unique command ID used for deduplication |
| `type` | string | Must be `"set-camera-orientation"` |
| `params.rotation` | int | Allowed values: `0` or `180` |

#### Parameter Validation

| Value | Meaning |
|-------|---------|
| `0` | Disable the current 180 degree correction (`vflip=0`, `hmirror=0`) |
| `180` | Enable 180 degree correction (`vflip=1`, `hmirror=1`) |

Firmware rejects any other value.

---

## ACK Contract

### Success Response

```json
{
  "cmdId": "01JWCAMORIENT1234567890",
  "status": "ok",
  "ts": 1778145601000,
  "result": {
    "rotation": 180,
    "vflip": 1,
    "hmirror": 1,
    "appliedAt": 1778145601000
  }
}
```

### Rejected Response

```json
{
  "cmdId": "01JWCAMORIENT1234567890",
  "status": "rejected",
  "ts": 1778145601000,
  "error": {
    "code": "invalid_rotation"
  }
}
```

Note: current `timer_camera` ACK path sends only `error.code` (without `error.message`).

#### Possible Error Codes

| Code | Meaning | Action |
|------|---------|--------|
| `invalid_rotation` | rotation is not `0` or `180` | Restrict UI/backend values to supported options |
| `unsupported_command` | Unknown command type sent | Backend bug, check command router |

---

## Persistence Rules

- Orientation is stored in NVS as the sensor flags `vflip` and `hmirror`
- Successful command changes survive restart and power loss
- Existing devices migrate to the current default orientation during firmware upgrade
- Migration preserves the current visual behavior of already deployed devices

---

## Operational Notes

- If streaming is active, firmware disconnects the current WebSocket session and reinitializes the camera immediately
- If streaming is paused, firmware stores the new orientation and applies it on the next camera initialization
- MQTT deduplication rules are the same as for `set-camera-quality`
- `ts` and `result.appliedAt` in ACK payload are `millis()` values (uptime), not Unix epoch time

---

## Backend / UI Guidance

- Preferred control model: segmented switch or dropdown with `0°` and `180°`
- Do not expose raw `vflip` or `hmirror` flags in the public UI for this version
- If future product requirements need `90°` or `270°`, implement that in a different media or rendering layer rather than this firmware command

---

## Verification Checklist

- [ ] Valid `set-camera-orientation` with `rotation=0` is accepted and ACK'd
- [ ] Valid `set-camera-orientation` with `rotation=180` is accepted and ACK'd
- [ ] Invalid rotation is rejected with `invalid_rotation`
- [ ] Camera stream visibly changes orientation after command
- [ ] Orientation persists after device restart
