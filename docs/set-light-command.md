# `set-light` Command — Cloud Integration Specification

**Feature:** RGB Strip Control (SK6812 RGB Unit)  
**Firmware commit:** `8bb5f56`  
**MQTT protocol:** see `docs/MQTT-PROTOCOL-v1.md` for transport, auth, and ACK framing  
**Status:** Implemented

---

## Overview

The `set-light` command controls an SK6812 RGB Unit physically attached to the device (4 units × 3 LEDs = 12 LEDs, GPIO17 / PORT.C). It supports a non-blocking linear color fade, per-command brightness, and an optional hold timer.

The command is independent of `set-led`, which controls a separate WS2812 bottom strip. The two strips operate in parallel without interference.

---

## Initial state (on boot)

On every boot, the RGB strip is **off** — no color, no fade, Idle state. The device does not persist the last color across reboots.

The first `phasmida/{slug}/status` publish after boot will always contain:

```json
"led": { "active": false, "activeColor": "#000000", "brightness": 0, "holdExpiresAt": 0, "lastCmdId": "" }
```

**To set an initial color:** send a `set-light` command after receiving `{ "state": "online" }` on the status topic. The standard reconnect flow (MQTT-PROTOCOL-v1.md §10) triggers a backend queue flush on each device online event — backend should include the desired initial `set-light` in that flush if a persistent color is required.

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
    "brightness": 200,
    "fadeMs": 3000,
    "holdMs": 60000
  }
}
```

### `params` fields

| Field | Type | Required | Range | Description |
|-------|------|----------|-------|-------------|
| `targetColor` | string | yes | `#RRGGBB` hex | Target color. Must start with `#` followed by exactly 6 hex characters (case-insensitive). |
| `brightness` | int | yes | 0 – 255 | LED intensity. Applied per-LED via `nscale8_video()` — not a global dimmer. 0 = off, 255 = full. |
| `fadeMs` | int | no | 0 – 600 000 | Transition duration in ms. `0` = instant apply. Default: `0`. |
| `holdMs` | int | no | 0 – * | How many ms to hold the color after fade completes, then auto-off. `0` = stay on indefinitely until next command. Default: `0`. |

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
    "fadeMs": 3000,
    "holdMs": 60000,
    "appliedAt": 1735000000000,
    "holdExpiresAt": 1735003663000
  }
}
```

| `result` field | Description |
|----------------|-------------|
| `activeColor` | Echo of `targetColor` |
| `brightness` | Echo of `brightness` |
| `fadeMs` | Echo of `fadeMs` |
| `holdMs` | Echo of `holdMs` |
| `appliedAt` | Unix ms when the device received and applied the command (from NTP-synced clock) |
| `holdExpiresAt` | Unix ms when the hold expires (`appliedAt + fadeMs + holdMs`); `0` if `holdMs == 0` |

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

| `error.code` | Condition |
|--------------|-----------|
| `invalid_color` | `targetColor` missing, not a string, or not valid `#RRGGBB` format |
| `invalid_brightness` | `brightness` missing, not an int, or outside 0 – 255 |
| `invalid_fade_ms` | `fadeMs` > 600 000 |

---

## Behavior semantics

### Fade

When `fadeMs > 0`, the strip performs a linear color transition from the **current physical color** (whatever the strip is showing at the moment the command arrives) to `targetColor`. Progress is computed non-blocking on every firmware loop tick:

```
progress = min(elapsed_ms * 255 / fadeMs, 255)
outputColor = blend(fromColor, targetColor, progress)
```

When `fadeMs == 0`, `targetColor` is applied instantly.

### Hold

After the fade completes (or immediately if `fadeMs == 0`):

- If `holdMs > 0`: the strip holds `targetColor` for `holdMs` ms, then turns off automatically (Idle state).
- If `holdMs == 0`: the strip stays on indefinitely until the next `set-light` command or the watchdog fires.

### Last-write-wins

If a new `set-light` command arrives while a fade or hold is in progress, it takes effect immediately. The new fade starts from the **currently interpolated color** at the moment of receipt — there is no visible jump.

This means backend can send rapid successive commands without needing to wait for ACK or query current state first.

### Watchdog (auto-off after 15 minutes)

The device automatically turns off the RGB strip if no `set-light` command is received within **15 minutes** of the last one. The timer resets on every accepted command (post-validation), regardless of whether the hold is still active.

This prevents the strip from staying on indefinitely if the backend loses connectivity or forgets to send an explicit off command.

### Turning the strip off explicitly

Send a command with black color and zero brightness:

```json
{
  "type": "set-light",
  "params": { "targetColor": "#000000", "brightness": 0, "fadeMs": 0, "holdMs": 0 }
}
```

Or fade to off:

```json
{
  "type": "set-light",
  "params": { "targetColor": "#000000", "brightness": 0, "fadeMs": 2000, "holdMs": 0 }
}
```

---

## LED state in status payload

Every `phasmida/{slug}/status` publish includes a `led` object reflecting the current RGB strip state. Backend should use this to synchronize on device reconnect — no need for a separate query.

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
    "holdExpiresAt": 1735003663000,
    "lastCmdId": "01HXYZK6H9B5O2Q3R6S8T0U1V4"
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `active` | bool | `true` if the strip is currently Fading or in Hold state |
| `activeColor` | string | Last `targetColor`; `"#000000"` if never set or currently Idle |
| `brightness` | int | Last `brightness`; `0` if Idle |
| `holdExpiresAt` | int | Unix ms when hold expires; `0` if no active hold or `holdMs` was `0` |
| `lastCmdId` | string | `cmdId` of the last successfully applied `set-light` command; `""` if none |

**Reconnect sync pattern:** On receiving `{ "state": "online" }` from a device, backend can compute `now > led.holdExpiresAt` to determine if the previous hold has expired during the offline period, and decide whether to re-issue the last command.

---

## Constraints and limits

| Constraint | Value | Notes |
|------------|-------|-------|
| Max `fadeMs` | 600 000 ms (10 min) | Enforced by firmware; exceeding → `invalid_fade_ms` rejection |
| Watchdog | 900 000 ms (15 min) | Firmware-side; resets on each accepted command |
| LED count | 12 (4 units × 3) | All LEDs always set to the same color (uniform, no per-LED addressing in v1) |
| Color format | `#RRGGBB` only | Strict: must have `#` prefix, exactly 6 hex chars, no short form |
| Brightness range | 0 – 255 | Linear scale applied per-LED via `nscale8_video()` |
| Independent of `set-led` | yes | Bottom WS2812 strip is unaffected by `set-light` commands |

---

## Example scenarios

### Slow warm fade, stay on

```json
{
  "type": "set-light",
  "params": { "targetColor": "#FFAA00", "brightness": 180, "fadeMs": 5000, "holdMs": 0 }
}
```

### Flash alert for 30 seconds, then off

```json
{
  "type": "set-light",
  "params": { "targetColor": "#FF0000", "brightness": 255, "fadeMs": 0, "holdMs": 30000 }
}
```

### Gentle sunrise (10 min fade, hold 1 hour)

```json
{
  "type": "set-light",
  "params": { "targetColor": "#FFF0C0", "brightness": 220, "fadeMs": 600000, "holdMs": 3600000 }
}
```

### Turn off immediately

```json
{
  "type": "set-light",
  "params": { "targetColor": "#000000", "brightness": 0, "fadeMs": 0, "holdMs": 0 }
}
```
