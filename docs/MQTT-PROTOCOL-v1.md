# Phasmida MQTT Protocol v1

Contract between device firmware and Phasmida backend.

Version: 1.0  
Status: As implemented (main branch)  
Owners: Backend + Firmware

---

## 1. Overview

Devices communicate with backend through MQTT.
Backend subscribes to uplink topics and publishes downlink commands.

Current firmware behavior:

- Transport: MQTT 3.1.1 over plain TCP
- Default endpoint: `api.phasmida.eu:1883`
- Optional TLS path: port `8883` exists, but current firmware uses `setInsecure()` (no server certificate validation)
- Payload format: UTF-8 JSON

Time semantics:

- `core_s3`: Unix epoch ms (`timestampMs`, `ts`) from `TimeSync`
- `timer_camera_f`: `millis()` for `cmd/ack.ts` and `events.ts`

---

## 2. Device Identity

Each device uses MAC address as primary identity.

Two MAC forms:

| Form | Example | Usage |
|------|---------|-------|
| Display | `AA:BB:CC:DD:EE:FF` | JSON payload field `macaddress` |
| Slug (lowercase hex, no separators) | `aabbccddeeff` | MQTT topic path, broker username |

Firmware uses slug in MQTT topics/username and display form in payloads.

---

## 3. Broker Authentication

Each device has its own MQTT credentials.

### Device

| Field | Value |
|------|-------|
| `username` | device slug (for example `aabbccddeeff`) |
| `password` | per-device API key |
| `clientId` | `phasmida-{slug}` |

### Backend

| Field | Value |
|------|-------|
| `username` | `phasmida` |
| `password` | `MQTT_BACKEND_PASSWORD` |
| `clientId` | `phasmida-backend-{instanceId}` |

Rules:

- Single `clientId` session behavior follows broker defaults.
- On auth failures, firmware uses reconnect backoff.

---

## 4. Topic Structure

All topics use prefix `phasmida/{slug}/`.

### Uplink (device -> backend)

| Topic | Purpose | QoS | Retained |
|------|---------|-----|----------|
| `phasmida/{slug}/telemetry` | Sensor readings | 1 | no |
| `phasmida/{slug}/status` | Online/offline lifecycle | 1 | yes |
| `phasmida/{slug}/events` | Discrete events/errors | 1 | no |
| `phasmida/{slug}/cmd/ack` | Command execution ACK | 1 | no |

### Downlink (backend -> device)

| Topic | Purpose | QoS | Retained |
|------|---------|-----|----------|
| `phasmida/{slug}/cmd` | Commands | 1 | no |

Devices should subscribe to `phasmida/{slug}/cmd` after MQTT connect.

---

## 5. Telemetry Payload

Topic: `phasmida/{slug}/telemetry`  
QoS: 1  
Retained: no

Example:

```json
{
  "apiVersion": 1,
  "msgId": "01HXYZK4F7Z3M9N2P0R5S8T1V4",
  "macaddress": "AA:BB:CC:DD:EE:FF",
  "sensorType": "env-pro",
  "timestampMs": 1735000000000,
  "measurements": [
    { "metric": "temperature", "value": 23.4, "unit": "C" },
    { "metric": "humidity", "value": 51.2, "unit": "percent" }
  ]
}
```

Notes:

- `apiVersion` is `1`
- `msgId` is firmware-generated unique ID
- one payload should not contain duplicated `metric` names
- multi-probe devices publish separate telemetry payloads per probe

---

## 6. Status Payload (LWT)

Topic: `phasmida/{slug}/status`  
QoS: 1  
Retained: yes

Online example:

```json
{
  "state": "online",
  "ts": 1735000000000,
  "fwVersion": "1.0.3",
  "ip": "192.168.1.42"
}
```

LWT payload (set at MQTT connect):

```json
{
  "state": "offline",
  "ts": 0,
  "reason": "unexpected"
}
```

Target-specific behavior:

- `core_s3` publishes online status and 5-minute heartbeats
- `timer_camera_f` currently sets LWT but does not publish periodic online heartbeats

---

## 7. Events Payload

Topic: `phasmida/{slug}/events`  
QoS: 1  
Retained: no

Example:

```json
{
  "apiVersion": 1,
  "msgId": "01HXYZK5G8A4N1P2Q5R7S9T0V3",
  "macaddress": "AA:BB:CC:DD:EE:FF",
  "ts": 1735000000000,
  "type": "sensor-error",
  "severity": "warning",
  "message": "DHT22 read timeout",
  "details": { "attempts": 3 }
}
```

---

## 8. Commands (Downlink)

Topic: `phasmida/{slug}/cmd`  
QoS: 1  
Retained: no

Command envelope example:

```json
{
  "cmdId": "01HXYZK6H9B5O2Q3R6S8T0U1V4",
  "type": "reboot",
  "params": {},
  "issuedAt": 1735000000000,
  "ttlMs": 30000
}
```

Envelope notes:

- `cmdId` and `type` are required
- `core_s3` and `atoms3_lite`: `issuedAt` and `ttlMs` enable expiry enforcement when both are present
- manual/test publishes may omit both `issuedAt` and `ttlMs`; firmware accepts them as non-expiring commands
- sending only one of `issuedAt` / `ttlMs`, or sending zero values, is rejected with `invalid_timing`
- `timer_camera_f`: `issuedAt`/`ttlMs` are currently not enforced

### Implemented commands by target

| Command | `core_s3` | `timer_camera_f` |
|---------|-----------|------------------|
| `request-telemetry` | yes | no |
| `reboot` | yes | no |
| `set-config` | yes | no |
| `set-timezone` | yes | no |
| `set-led` | yes | no |
| `set-light` | yes | no |
| `force-light-on` | yes | no |
| `start-rgb-verification` | yes | no |
| `factory-reset` | yes | no |
| `stream-stop` | no | yes |
| `stream-start` | no | yes |
| `set-camera-quality` | no | yes |
| `set-camera-orientation` | no | yes |

Unknown command handling:

- status: `rejected`
- error code: `unsupported_command`

Reserved (not implemented):

- `firmware-update`

---

## 9. Command ACK

Topic: `phasmida/{slug}/cmd/ack`  
QoS: 1  
Retained: no

Example:

```json
{
  "cmdId": "01HXYZK6H9B5O2Q3R6S8T0U1V4",
  "status": "ok",
  "ts": 1735000000000,
  "result": {
    "requested": { "jpegQuality": 12, "frameSize": 13 },
    "status": "accepted_for_reinit",
    "appliedAt": 1735000000000
  }
}
```

Status values:

- `ok`
- `error`
- `rejected`
- `expired` (`core_s3` command TTL path)

Time field `ts`:

- `core_s3`: Unix epoch ms
- `timer_camera_f`: `millis()`

---

## 10. Connect and Reconnect

CONNECT parameters (implemented):

- `clientId = phasmida-{slug}`
- `username = {slug}`
- `password = api_key`
- `keepAlive = 60`
- will payload on status topic

Reconnect strategy in firmware:

- exponential backoff with jitter
- auth failures use delayed reconnect start

---

## 11. Idempotency and Deduplication

QoS 1 is at-least-once.

| Direction | Dedup owner | Key |
|-----------|-------------|-----|
| Telemetry | Backend | `(device_id, msgId)` |
| Events | Backend | `(device_id, msgId)` |
| Commands | Firmware | `cmdId` |
| ACK | Backend | `cmdId` |

Current target-specific duplicate handling:

- `core_s3`: duplicate command is re-ACKed with `status: ok`
- `timer_camera_f`: duplicate command is ignored without ACK resend

---

## 12. Limits

| Limit | Value |
|------|-------|
| Max telemetry payload | 8 KB |
| Max status/events/ack payload | 2 KB |
| Min telemetry interval | 5 s |
| Default telemetry interval | 60 s |

---

## 13. Versioning

- Current protocol: v1
- Non-breaking changes can stay in v1
- Breaking changes require a versioned migration path

---

## 14. Security (Current Firmware State)

- Default firmware transport is plain TCP on port `1883`
- TLS path on `8883` exists, but current implementation does not validate certificates
- Device auth is username/password per device
- mTLS is not implemented

---

## 15. Example Connection (Current Runtime)

```text
client.connect({
  protocol: "mqtt",
  host: "api.phasmida.eu",
  port: 1883,
  clientId: "phasmida-aabbccddeeff",
  username: "aabbccddeeff",
  password: "<api_key>",
  keepAlive: 60,
  cleanSession: true,
  will: {
    topic: "phasmida/aabbccddeeff/status",
    qos: 1,
    retain: true,
    payload: '{"state":"offline","ts":0,"reason":"unexpected"}'
  }
})
```

---

## 16. Broker Auth Model

Broker uses HTTP-backed auth/ACL checks (mosquitto-go-auth + backend API).

At a high level:

- CONNECT checks device credentials
- publish/subscribe checks ACL per username/topic
- backend user `phasmida` has wildcard operational permissions

---

## 17. Open Items

- strict TLS certificate validation in firmware
- mTLS per device
- OTA command path (`firmware-update`)
- stronger runtime parity between `core_s3` and `timer_camera_f` command envelope handling
