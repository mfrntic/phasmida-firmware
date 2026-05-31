# `set-light` Command — Cloud Integration Specification

**Feature:** RGB Strip Control (SK6812 RGB Unit)  
**MQTT protocol:** see `docs/MQTT-PROTOCOL-v1.md` for transport/auth/ACK framing  
**Status:** Implemented (current behavior)

---

## Overview

The `set-light` command controls the SK6812 RGB Unit attached to CoreS3 PORT.C (GPIO17 data), with up to 12 LEDs (4 units × 3 LEDs).

`set-light` is independent of `set-led`:

- `set-light` controls the SK6812 RGB Unit.
- `set-led` controls the separate M5GO3 bottom WS2812 strip.

Both can run in parallel.

---

## Current runtime behavior

- Apply is immediate (no fade state machine).
- The command sets a target RGB color and brightness.
- Visual output is uniform over all SK6812 LEDs.
- Brightness is applied per LED via `nscale8_video()`.
- Command is blocked while an RGB verification session is active (`start-rgb-verification`).

---

## Initial state and persistence

On boot, LEDs start from off and then firmware restores the last persisted `set-light` state from NVS if available.

Persistence includes:

- last color
- last brightness
- last successful `cmdId`

If no persisted state exists, status reports the default off state.

---

## Command payload

Topic: `phasmida/{slug}/cmd`  
QoS: 1, Retained: **no**

```json
{
  "cmdId": "01HXYZK6H9B5O2Q3R6S8T0U1V4",
  "type": "set-light",
  "issuedAt": 1735000000000,
  "ttlMs": 30000,
  "params": {
    "targetColor": "#FF6200",
    "brightness": 200
  }
}
```

### `params` fields

| Field | Type | Required | Range | Description |
|-------|------|----------|-------|-------------|
| `targetColor` | string | yes | `#RRGGBB` | Must start with `#` and contain exactly 6 hex chars. |
| `brightness` | int | yes | 0-255 | Per-command brightness scaling. `0` means off output. |

Notes:

- Current firmware does not implement `fadeMs` / `holdMs` behavior.
- Unknown extra `params` fields are ignored by current parser.

---

## ACK payload

Topic: `phasmida/{slug}/cmd/ack`  
QoS: 1, Retained: no

### Success

```json
{
  "cmdId": "01HXYZK6H9B5O2Q3R6S8T0U1V4",
  "status": "ok",
  "ts": 1735000000000,
  "result": {
    "activeColor": "#FF6200",
    "brightness": 200,
    "appliedAt": 1735000000000
  }
}
```

| `result` field | Description |
|----------------|-------------|
| `activeColor` | Echo of `targetColor` |
| `brightness` | Echo of `brightness` |
| `appliedAt` | Unix ms when command was applied (`TimeSync` source) |

### Validation rejection

```json
{
  "cmdId": "01HXYZK6H9B5O2Q3R6S8T0U1V4",
  "status": "rejected",
  "ts": 1735000000000,
  "error": {
    "code": "invalid_color",
    "message": "targetColor must be a valid #RRGGBB hex string"
  }
}
```

Known `error.code` values for `set-light` path:

| `error.code` | Condition |
|--------------|-----------|
| `invalid_color` | `targetColor` missing/invalid format |
| `invalid_brightness` | `brightness` missing or out of 0-255 |
| `verification_in_progress` | `set-light` sent during active verification session |

---

## LED state in status payload

Each `phasmida/{slug}/status` publish includes current `set-light` state:

```json
{
  "state": "online",
  "ts": 1735000000000,
  "fwVersion": "1.0.3",
  "ip": "192.168.1.42",
  "led": {
    "active": true,
    "activeColor": "#FF6200",
    "brightness": 200,
    "lastCmdId": "01HXYZK6H9B5O2Q3R6S8T0U1V4"
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `active` | bool | `true` when light is considered active (`brightness > 0` or non-black state) |
| `activeColor` | string | Last target color rendered as `#RRGGBB` |
| `brightness` | int | Last applied brightness |
| `lastCmdId` | string | Last successfully applied `set-light` `cmdId` |

---

## Constraints

| Constraint | Value |
|-----------|-------|
| LED count | 12 max (4 units × 3) |
| Color format | Strict `#RRGGBB` |
| Brightness range | 0-255 |
| Strip independence | `set-light` does not affect `set-led` strip |

---

## Example scenarios

### Apply orange at medium brightness

```json
{
  "type": "set-light",
  "params": { "targetColor": "#FFAA00", "brightness": 180 }
}
```

### Turn strip off explicitly

```json
{
  "type": "set-light",
  "params": { "targetColor": "#000000", "brightness": 0 }
}
```
