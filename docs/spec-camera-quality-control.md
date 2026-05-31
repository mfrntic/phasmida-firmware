---
title: "Camera Quality Control — MQTT Command Specification"
version: "1.1"
date: "2026-05-19"
status: "active"
author: "Firmware Team"
relates-to:
  - "spec-firmware-v1-3-camera-streaming.md"
  - "MQTT-PROTOCOL-v1.md"
  - "spec-1-cloud-driven-rgb-set-light.md"
---

# Camera Quality Control — MQTT Command Specification

## Overview

This document defines the MQTT command interface for dynamically controlling **Timer Camera F** JPEG quality and frame resolution without full firmware redeploy.

The mechanism follows the same command-response pattern as other cloud-driven controls (e.g., RGB set-light commands), enabling real-time adjustment of streaming quality based on network conditions, user preferences, or power constraints.

Current firmware now aims for a higher preferred quality profile by default, while still keeping a safe fallback profile if camera init or warmup fails.

---

## Quick Start

### For Backend Developers

1. Publish `set-camera-quality` to the camera MQTT command topic as the backend MQTT client
2. Camera validates parameters and publishes ACK on `cmd/ack`
3. Camera saves settings to NVS (persisted across reboots)
4. Camera reinitializes the OV3660 sensor with new parameters
5. WebSocket stream reconnects and resumes with the new settings

### For Frontend Developers

1. Expose camera quality slider in UI (0–63 for JPEG, where lower = better)
2. Display current frame resolution (QVGA, VGA, SVGA, XGA)
3. Show command ACK status (success, rejected, error)
4. Allow preset buttons: "High Quality", "Balanced", "Low Bandwidth"

---

## Command Contract

### Endpoint

**Topic:** `phasmida/{slug}/cmd`

**Publisher identity:** backend MQTT client (`username=phasmida`, `password=MQTT_BACKEND_PASSWORD`)

**Payload:** JSON object with structure below

### Command Payload Format

```json
{
  "cmdId": "01JCAM8D2Y8AP9R5B7M4",
  "type": "set-camera-quality",
  "params": {
    "jpegQuality": 12,
    "frameSize": 9,
    "frameDelay": 500,
    "sharpness": 2,
    "denoise": 0,
    "lenc": 1,
    "rawGma": 1,
    "aec2": 1,
    "wpc": 1,
    "bpc": 1,
    "gainCeiling": 3
  }
}
```

#### Required Fields

| Field | Type | Description |
|-------|------|-------------|
| `cmdId` | string | Unique command ID (e.g., ULID, UUID). Used for deduplication. |
| `type` | string | Must be `"set-camera-quality"` |
| `params.jpegQuality` | int | JPEG quality 0–63, where lower = better quality, larger file |
| `params.frameSize` | int | Frame size enum: 5, 8, 9, 10, 12, or 13 (see table below) |

#### Optional Tuning Fields

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `params.sharpness` | int | -2..2 | OV3660 sharpness level |
| `params.denoise` | int | 0..8 | OV3660 denoise level |
| `params.lenc` | int | 0 or 1 | Lens correction enable flag |
| `params.rawGma` | int | 0 or 1 | Raw gamma enable flag |
| `params.aec2` | int | 0 or 1 | Secondary exposure control enable flag |
| `params.wpc` | int | 0 or 1 | White pixel correction enable flag |
| `params.bpc` | int | 0 or 1 | Black pixel correction enable flag |
| `params.gainCeiling` | int | 0..6 | `gainceiling_t` enum value |
| `params.frameDelay` | int | 0..2000 | Delay between frames in ms (0 = max FPS) |

If optional fields are omitted, firmware keeps previously persisted NVS values for those fields.

#### Current Firmware Notes

- Firmware currently requires only `cmdId`, `type`, and `params` for `set-camera-quality`.
- `issuedAt` and `ttlMs` are not enforced by the current `timer_camera` implementation.
- `cmd/ack` and `events.ts` use `millis()` (uptime), not Unix epoch time.
- Unknown command types are rejected with `unsupported_command`.

#### Parameter Validation

**JPEG Quality (`jpegQuality`)**

| Value | Approx. Bandwidth | Use Case |
|-------|-------------------|----------|
| 0–5 | Highest quality, riskiest settings | Can become unstable on this camera/driver |
| **6–12** | **High quality** | **Recommended practical range** |
| 13–25 | Balanced | Good compromise |
| 26–63 | Lower quality | Smaller frames, more visible artifacts |

Important: on ESP32 camera driver, **lower value means better JPEG quality**.

**Frame Size (`frameSize`)**

| Value | Resolution | Dimensions | Approx. Bandwidth | Use Case |
|-------|------------|------------|-------------------|----------|
| 5 | QVGA | 320×240 | Smallest | Low bandwidth / debugging |
| 8 | VGA | 640×480 | Medium | Conservative fallback |
| 9 | SVGA | 800×600 | Safe fallback | Recommended recovery profile |
| **10** | **XGA** | **1024×768** | **Preferred default** | **High detail baseline** |
| 12 | SXGA | 1280×1024 | Very large | High detail if stable |
| 13 | UXGA | 1600×1200 | Maximum exposed in current command | Best possible detail if stable |

Important: these numeric values follow the actual `esp32-camera` enum mapping used by the current firmware, not the older draft mapping.

#### Example Payloads

**High Quality (LAN / WiFi 5GHz):**
```json
{
  "cmdId": "01JCAM111111111111111111",
  "type": "set-camera-quality",
  "params": {
    "jpegQuality": 8,
    "frameSize": 10
  }
}
```

**Maximum Detail (best-effort):**
```json
{
  "cmdId": "01JCAM444444444444444444",
  "type": "set-camera-quality",
  "params": {
    "jpegQuality": 6,
    "frameSize": 13
  }
}
```

**Balanced (typical home WiFi):**
```json
{
  "cmdId": "01JCAM222222222222222222",
  "type": "set-camera-quality",
  "params": {
    "jpegQuality": 12,
    "frameSize": 9
  }
}
```

**Low Bandwidth (mobile hotspot):**
```json
{
  "cmdId": "01JCAM333333333333333333",
  "type": "set-camera-quality",
  "params": {
    "jpegQuality": 20,
    "frameSize": 5
  }
}
```

---

## Response (ACK) Contract

### Success Response

**Topic:** `phasmida/{slug}/cmd/ack`

**Payload:**
```json
{
  "cmdId": "01JCAM8D2Y8AP9R5B7M4",
  "status": "ok",
  "ts": 1778145601000,
  "result": {
    "requested": {
      "jpegQuality": 12,
      "frameSize": 13
    },
    "status": "accepted_for_reinit",
    "appliedAt": 1778145601000
  }
}
```

**Meaning:** Settings are validated and persisted, and camera reinitialization has started. This ACK confirms command acceptance, not final effective profile.

### Deferred Effective Profile Event

After camera reinit completes, firmware publishes effective state on events topic.

**Topic:** `phasmida/{slug}/events`

**No fallback (requested applied):**
```json
{
  "apiVersion": 1,
  "msgId": "1717065000-74aa1f",
  "macaddress": "AA:BB:CC:DD:EE:FF",
  "ts": 1717065000,
  "type": "camera-quality-applied",
  "severity": "info",
  "message": "Requested camera profile applied",
  "details": {
    "cmdId": "01JCAM8D2Y8AP9R5B7M4",
    "requested": 13,
    "applied": 13,
    "jpegQuality": 12
  }
}
```

**Fallback happened:**
```json
{
  "apiVersion": 1,
  "msgId": "1717065000-2f91be",
  "macaddress": "AA:BB:CC:DD:EE:FF",
  "ts": 1717065000,
  "type": "camera-quality-fallback",
  "severity": "warning",
  "message": "Requested camera profile was downgraded to a stable fallback",
  "details": {
    "cmdId": "01JCAM8D2Y8AP9R5B7M4",
    "requested": 13,
    "applied": 9,
    "jpegQuality": 12
  }
}
```

**Frontend rule:** ACK marks command as accepted; final UI state must track the deferred event (`details.applied`).

### Rejected Response

**Payload (invalid JPEG quality):**
```json
{
  "cmdId": "01JCAM8D2Y8AP9R5B7M4",
  "status": "rejected",
  "ts": 1778145601000,
  "error": {
    "code": "invalid_jpeg_quality"
  }
}
```

**Payload (invalid frame size):**
```json
{
  "cmdId": "01JCAM8D2Y8AP9R5B7M4",
  "status": "rejected",
  "ts": 1778145601000,
  "error": {
    "code": "invalid_frame_size"
  }
}
```

Note: the current `timer_camera` ACK path returns only `error.code` (without `error.message`).

#### Possible Error Codes

| Code | Meaning | Action |
|------|---------|--------|
| `invalid_jpeg_quality` | jpegQuality outside 0–63 range | Validate input before sending |
| `invalid_frame_size` | frameSize not in {5, 8, 9, 10, 12, 13} | Use valid preset or educate user |
| `invalid_frame_delay` | frameDelay outside 0..2000 range | Clamp or validate frameDelay before sending |
| `invalid_sharpness` | sharpness outside -2..2 | Validate slider range before sending |
| `invalid_denoise` | denoise outside 0..8 | Validate denoise preset before sending |
| `invalid_boolean_tuning_flag` | one of lenc/rawGma/aec2/wpc/bpc is not 0 or 1 | Ensure boolean flags are encoded as 0/1 |
| `invalid_gain_ceiling` | gainCeiling outside 0..6 | Use valid gain ceiling enum |
| `unsupported_command` | Unknown command type sent | Backend bug, check command router |

---

## Deduplication and Idempotency

### How It Works

Camera maintains a circular buffer of **8 recent command IDs**. If the same `cmdId` is received twice:

1. **First receipt:** Command is executed, ACK is sent
2. **Second receipt (duplicate):** Command is skipped

**Benefit:** Network retries (MQTT QoS 1) do not cause double-apply.

Current implementation note: duplicate commands are logged and ignored; ACK re-send for duplicates is not currently implemented.

### Implementation Notes

- Buffer holds only `cmdId` strings; no command state
- Oldest entry is discarded when buffer fills
- Fresh `cmdId` on every new command is required (do NOT reuse)

---

## Lifecycle and State Management

### Timeline

```
┌─────────────────────────────────────────────────────────┐
│ Backend sends set-camera-quality command                │
└────────────────────┬────────────────────────────────────┘
                     │
                     ├─ T+0ms: Camera receives message
                     │
                     ├─ T+10ms: Validates params, saves to NVS
                     │
                     ├─ T+20ms: ACK published to cmd/ack topic
                     │
                     ├─ T+30ms: WebSocket connection closes
                     │
                     ├─ T+50ms: Camera re-reads NVS quality settings
                     │
                     ├─ T+100ms: OV3660 sensor re-initializes
                     │
                     ├─ T+300ms: Camera warmup validates that frames can actually be captured
                     │
                     ├─ T+500ms: WebSocket reconnects to backend
                     │
                     └─ T+500–1500ms: First frame with new quality arrives
```

**Typical downtime:** sub-second to ~1.5 s depending on reconnect and warmup.

### State Transitions

```
STREAMING
  ↓ (receive set-camera-quality)
  ├─ Validate params
  ├─ Save to NVS
  ├─ Publish ACK
  ├─ Close WebSocket
  └─ Trigger CAMERA_INIT
     ├─ Read NVS quality
     ├─ Apply to camera config
     ├─ Initialize OV3660
    ├─ Capture warmup frames to confirm settings are viable
     └─ Reconnect WebSocket
        └─ Resume STREAMING (new quality active)
```

  ### Fallback Behavior

  If the camera reinitializes successfully but **fails to capture any warmup frame**, firmware treats the requested combination as unusable and falls back to safe defaults:

  - `jpegQuality = 12`
  - `frameSize = 9` (SVGA)
  - `sharpness = 2`
  - `denoise = 0`
  - `lenc = 1`
  - `rawGma = 1`
  - `aec2 = 1`
  - `wpc = 1`
  - `bpc = 1`
  - `gainCeiling = 3` (`GAINCEILING_16X`)
  - `frameDelay = 0` (max FPS)

  This protects the deployed camera from getting stuck in a null-frame loop after an overly aggressive command.

---

## Persistence

### NVS Storage

Settings are **persisted in ESP32 NVS (non-volatile storage)**:

```cpp
// Key names (firmware internal):
"jpeg_quality"  → uint8_t (0–63)
"frame_size"    → uint8_t (5, 8, 9, 10, 12, or 13)
"frame_delay"   → uint16_t (0..2000 ms)
"sharpness"     → int8_t (-2..2)
"denoise"       → uint8_t (0..8)
"lenc"          → uint8_t (0/1)
"raw_gma"       → uint8_t (0/1)
"aec2"          → uint8_t (0/1)
"wpc"           → uint8_t (0/1)
"bpc"           → uint8_t (0/1)
"gain_ceil"     → uint8_t (0..6)
```

**Behavior:**
- Settings survive device reboot
- Factory defaults apply if keys missing:
  - `jpegQuality` = 8
  - `frameSize` = 10 (XGA)
  - `sharpness` = 2
  - `denoise` = 0
  - `lenc` = 1
  - `rawGma` = 1
  - `aec2` = 1
  - `wpc` = 1
  - `bpc` = 1
  - `gainCeiling` = 3
  - `frameDelay` = 0
- Safe fallback profile (when requested settings cannot initialize camera stably) remains:
  - `jpegQuality` = 12
  - `frameSize` = 9 (SVGA)
- Firmware may overwrite invalid or unstable runtime settings with safe defaults after failed warmup

---

## Backend Implementation Guide

### 1. Add Command Handler Route

**Endpoint:** `POST /cameras/:cameraId/set-quality`

**Request:**
```json
{
  "jpegQuality": 55,
  "frameSize": 5
}
```

**Response (202 Accepted):**
```json
{
  "msgId": "01JCAM...",
  "status": "command_sent",
  "ts": 1778145600000
}
```

### 2. Build and Send MQTT Command

**Pseudocode:**
```javascript
function sendSetCameraQuality(cameraSlug, jpegQuality, frameSize) {
  const cmdId = generateULID();

  const payload = {
    cmdId: cmdId,
    type: "set-camera-quality",
    params: {
      jpegQuality: jpegQuality,
      frameSize: frameSize
    }
  };

  const topic = `phasmida/${cameraSlug}/cmd`;
  const ackTopic = `phasmida/${cameraSlug}/cmd/ack`;

  // Publish command with QoS 1 using backend MQTT credentials:
  // username = "phasmida"
  // password = MQTT_BACKEND_PASSWORD
  await mqtt.publish(topic, JSON.stringify(payload), { qos: 1 });

  // Wait for ACK (timeout 5s)
  const ack = await waitForAck(ackTopic, cmdId, 5000);

  return ack;
}
```

### 3. Validate Inputs

**Before sending command:**

```javascript
function validateQualityParams(jpegQuality, frameSize, frameDelay = 0) {
  if (jpegQuality < 0 || jpegQuality > 63) {
    throw new Error("jpegQuality must be 0–63");
  }
  
  const validFrameSizes = [5, 8, 9, 10, 12, 13];
  if (!validFrameSizes.includes(frameSize)) {
    throw new Error(`frameSize must be one of: ${validFrameSizes.join(", ")}`);
  }

  if (!Number.isInteger(frameDelay) || frameDelay < 0 || frameDelay > 2000) {
    throw new Error("frameDelay must be 0–2000");
  }

  return true;
}
```

### 4. ACK Handling

```javascript
function handleCameraAck(ackPayload) {
  if (ackPayload.status === "ok") {
    console.log(`✓ Camera quality applied:`, ackPayload.result);
    // Update UI, log to analytics, etc.
  } else if (ackPayload.status === "rejected") {
    console.error(`✗ Command rejected:`, ackPayload.error.code);
    // Show user error message
  } else if (ackPayload.status === "error") {
    console.error(`✗ Command error:`, ackPayload.error);
    // Retry or abort
  }
}
```

---

## Frontend Implementation Guide

### 1. UI Component: Quality Slider

**HTML:**
```html
<div id="camera-quality-control">
  <label>Camera Quality</label>
  
  <!-- Preset Buttons -->
  <button onclick="setCameraPreset('high')">High Quality</button>
  <button onclick="setCameraPreset('balanced')">Balanced</button>
  <button onclick="setCameraPreset('low')">Low Bandwidth</button>
  
  <!-- Manual Slider -->
  <input 
    type="range" 
    id="quality-slider"
    min="0" 
    max="63" 
    value="12"
    oninput="onQualityChange(this.value)"
  />
  <span id="quality-label">12 (Balanced)</span>
  
  <!-- Resolution Selector -->
  <select id="frame-size" onchange="onFrameSizeChange(this.value)">
    <option value="5">QVGA (320×240) — Low Bandwidth</option>
    <option value="8">VGA (640×480) — Conservative</option>
    <option value="9">SVGA (800×600) — Safe Fallback</option>
    <option value="10" selected>XGA (1024×768) — Preferred Default</option>
    <option value="12">SXGA (1280×1024) — High Detail</option>
    <option value="13">UXGA (1600×1200) — Maximum Detail</option>
  </select>
  
  <!-- Status -->
  <div id="quality-status">Ready</div>
  <div id="quality-spinner" style="display: none;">Applying...</div>
</div>
```

### 2. JavaScript: Send Command

```javascript
// Preset mappings
const QUALITY_PRESETS = {
  max:      { jpegQuality: 6,  frameSize: 13, label: "Maximum Detail" },
  high:     { jpegQuality: 8,  frameSize: 10, label: "High Quality" },
  balanced: { jpegQuality: 12, frameSize: 9,  label: "Balanced" },
  low:      { jpegQuality: 20, frameSize: 5,  label: "Low Bandwidth" }
};

async function setCameraQuality(jpegQuality, frameSize) {
  const cameraId = getCurrentCameraId();
  
  try {
    // Show loading state
    document.getElementById('quality-spinner').style.display = 'block';
    document.getElementById('quality-status').textContent = 'Sending...';
    
    // Send to backend
    const response = await fetch(`/api/cameras/${cameraId}/set-quality`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        jpegQuality: jpegQuality,
        frameSize: frameSize
      })
    });
    
    if (!response.ok) throw new Error('Failed to send command');
    
    const result = await response.json();
    
    // Wait for ACK (poll backend for command ACK status)
    const ack = await pollForAck(result.msgId, 10000);
    
    if (ack.status === 'ok') {
      document.getElementById('quality-status').textContent = 
        `✓ Applied: Quality ${ack.result.jpegQuality}, Size ${ack.result.frameSize}`;
      updateQualityLabel(jpegQuality, frameSize);
    } else {
      throw new Error(ack.error?.code || 'Unknown error');
    }
  } catch (err) {
    document.getElementById('quality-status').textContent = `✗ Error: ${err.message}`;
    console.error('Camera quality command failed:', err);
  } finally {
    document.getElementById('quality-spinner').style.display = 'none';
  }
}

function setCameraPreset(preset) {
  const p = QUALITY_PRESETS[preset];
  setCameraQuality(p.jpegQuality, p.frameSize);
  document.getElementById('quality-label').textContent = p.label;
}

function onQualityChange(value) {
  const frameSize = parseInt(document.getElementById('frame-size').value);
  const label = getQualityLabel(value);
  document.getElementById('quality-label').textContent = `${value} (${label})`;
  // Auto-apply after user stops dragging (debounce ~500ms)
  debounce(() => setCameraQuality(parseInt(value), frameSize), 500);
}

function onFrameSizeChange(value) {
  const quality = parseInt(document.getElementById('quality-slider').value);
  setCameraQuality(quality, parseInt(value));
}

function getQualityLabel(value) {
  value = parseInt(value);
  if (value <= 5) return 'Maximum';
  if (value <= 12) return 'High';
  if (value <= 25) return 'Balanced';
  if (value <= 40) return 'Reduced';
  return 'Low';
}

function updateQualityLabel(quality, frameSize) {
  const frameSizeLabel = ({ 5: 'QVGA', 8: 'VGA', 9: 'SVGA', 10: 'XGA', 12: 'SXGA', 13: 'UXGA' })[frameSize] || '?';
  document.getElementById('quality-label').textContent = 
    `${quality} (${getQualityLabel(quality)}) — ${frameSizeLabel}`;
}
```

### 3. Polling for ACK

```javascript
async function pollForAck(msgId, timeoutMs = 10000) {
  const startTime = Date.now();
  
  while (Date.now() - startTime < timeoutMs) {
    try {
      const response = await fetch(`/api/commands/${msgId}/ack`);
      if (response.ok) {
        const ack = await response.json();
        if (ack) return ack;  // ACK received
      }
    } catch (err) {
      // Network error, retry
    }
    
    // Wait 500ms before next poll
    await new Promise(r => setTimeout(r, 500));
  }
  
  throw new Error('ACK timeout');
}
```

---

## Testing Checklist

### Camera Firmware

- [ ] Build compiles without errors
- [ ] MQTT connects to broker on startup
- [ ] MQTT subscription to `phasmida/{slug}/cmd` succeeds
- [ ] Valid `set-camera-quality` command is accepted and ACK'd
- [ ] Invalid `jpegQuality` is rejected with error code
- [ ] Invalid `frameSize` is rejected with error code
- [ ] Invalid tuning values (`sharpness`, `denoise`, boolean flags, `gainCeiling`) are rejected with specific error code
- [ ] Settings persist in NVS across reboot
- [ ] Optional tuning fields persist in NVS across reboot
- [ ] WebSocket reconnects within 1 second after command
- [ ] First frame after command uses new quality
- [ ] If requested quality cannot capture any frame, firmware falls back to safe defaults
- [ ] Duplicate `cmdId` is deduplicated (ACK re-sent, no double-apply)

### Backend

- [ ] `POST /cameras/:id/set-quality` endpoint created
- [ ] Input validation passes valid/invalid params correctly
- [ ] MQTT command is published with correct topic and payload
- [ ] ACK response is captured and returned to frontend
- [ ] Error handling for invalid camera ID, missing params, etc.
- [ ] Command history is logged for audit trail

### Frontend

- [ ] Quality slider updates label correctly
- [ ] Frame size selector shows all 6 options
- [ ] Preset buttons send correct parameters
- [ ] Spinner shows during command submission
- [ ] Success message displays ACK result
- [ ] Error message displays rejection reason
- [ ] UI remains responsive during command (no freezes)
- [ ] Multiple commands sent in succession are handled correctly

### Integration

- [ ] Send command → Camera ACKs → UI shows success (E2E test)
- [ ] Network delay (simulate high latency) doesn't break flow
- [ ] Rapid preset switches don't cause race conditions
- [ ] Camera streams video continuously during quality changes
- [ ] Users can adjust quality based on observed bandwidth/latency

---

## Troubleshooting

### Issue: "Command rejected — invalid_jpeg_quality"

**Causes:**
- Frontend sent value outside 0–63 range
- Type mismatch (string instead of int)

**Fix:** Validate on frontend before sending

### Issue: "Command timeout" on ACK

**Causes:**
- Camera is offline
- MQTT broker unreachable
- Camera firmware didn't process message
- Network congestion delaying ACK

**Fix:**
- Check camera online status
- Verify MQTT connection
- Check camera logs (serial monitor)
- Retry with longer timeout

### Issue: Camera ignores command, keeps old quality

**Causes:**
- Firmware version doesn't support `set-camera-quality` yet
- Duplicate `cmdId` (firmware thinks it's a retry, skips)
- Command type mispelled

**Fix:**
- Verify firmware version ≥ 1.3.0
- Generate new `cmdId` for each command
- Check command type exactly matches `"set-camera-quality"`

### Issue: Frame size doesn't change, only quality does

**Causes:**
- Camera hardware limitations
- Frame size value invalid
- Requested combination initializes but cannot capture frames, so firmware falls back to safe defaults

**Fix:**
- Use frame sizes: 5 (QVGA), 8 (VGA), 9 (SVGA), 10 (XGA), 12 (SXGA), 13 (UXGA)
- Check firmware logs for init errors

### Issue: ACK says ok, but stream still looks unchanged

**Causes:**
- Old firmware without runtime camera reinit support
- New settings were applied, but requested combo failed warmup and firmware fell back to defaults
- Viewer/backend is showing cached or delayed frames

**Fix:**
- Check serial log for `Reinitializing camera with updated settings...`
- Check for `Warmup failed ...` followed by `Falling back to safe defaults ...`
- Verify new frame byte sizes in serial log after reconnect

---

## Performance Notes

### Bandwidth Impact

| Config | Frame | Frames/sec | Bandwidth |
|--------|-------|-----------|-----------|
| Q=20, QVGA | small | 10+ fps | low |
| Q=12, SVGA | medium | safe fallback | moderate |
| Q=8, XGA | large | preferred default | high |
| Q=6, UXGA | very large | lowest fps | very high |
| Q=1, QVGA | may fail on current camera | n/a | unstable |

### CPU/Memory Constraints

- **ESP32-S3 RAM:** ~8 MB total; camera uses ~2–4 MB for buffers
- **PSRAM:** ~8 MB available for framebuffer
- **Processing time:** OV3660 JPEG encode takes ~30–100 ms depending on quality
- **No blocking operations** during frame capture

### Latency

- **Command to application:** ~100–300 ms (MQTT round-trip)
- **Camera reboot to stream:** ~300–500 ms
- **Frame delivery latency:** Unchanged (~100–500 ms depending on network)

---

## References

- [Firmware v1.3 Camera Streaming Spec](spec-firmware-v1-3-camera-streaming.md)
- [MQTT Protocol v1](MQTT-PROTOCOL-v1.md)
- [Cloud-Driven RGB Set-Light Command](spec-1-cloud-driven-rgb-set-light.md)
- [ESP32-CAM OV3660 Datasheet](https://www.espressif.com)
- [ArduinoJson Library](https://arduinojson.org/)

---

## Changelog

### v1.0 (2026-05-19)

- Initial specification
- `set-camera-quality` command contract
- MQTT QoS 1 with deduplication
- ACK response schema
- Backend implementation guide
- Frontend UI component examples
- Testing checklist
- Troubleshooting guide

### v1.1 (2026-05-19)

- Corrected JPEG quality semantics: lower = better
- Corrected frame size enum mapping to 5/8/9/10
- Documented backend identity for command dispatch (`phasmida` + `MQTT_BACKEND_PASSWORD`)
- Documented runtime camera reinitialization and warmup validation
- Documented automatic fallback to safe defaults when requested settings produce no frames

### v1.2 (2026-05-19)

- Added optional OV3660 tuning fields to `set-camera-quality` payload: `sharpness`, `denoise`, `lenc`, `rawGma`, `aec2`, `wpc`, `bpc`, `gainCeiling`
- Updated success ACK schema to include full applied tuning state
- Added new reject error codes for tuning validation failures
- Corrected persisted `frame_size` value set to `{5, 8, 9, 10}`
- Expanded NVS persistence section with new tuning keys and defaults

---

## Contact & Support

For issues or questions:
- **Firmware:** See camera firmware logs (serial monitor)
- **Backend:** Check MQTT message flow and logs
- **Frontend:** Check browser console for errors
- **General:** Create issue in repository with reproduction steps
