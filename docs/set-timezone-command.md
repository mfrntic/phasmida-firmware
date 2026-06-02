# set-timezone command (core_s3)

Purpose: configure device local timezone from cloud at runtime.

## Topic and QoS

- Topic: `phasmida/{slug}/cmd`
- QoS: `1`
- Retained: `false`

## Envelope

```json
{
  "cmdId": "01HXYZK6H9B5O2Q3R6S8T0U1V4",
  "type": "set-timezone",
  "issuedAt": 1735000000000,
  "ttlMs": 30000,
  "params": {
    "posixTz": "CET-1CEST,M3.5.0/2,M10.5.0/3"
  }
}
```

## Validation

- `params.posixTz` is required.
- Value must be non-empty printable ASCII.
- Maximum length is 64 chars.

If validation fails, ACK status is `rejected` with code `invalid_timezone`.

## Behavior

1. Device applies timezone immediately (no reboot required).
2. If apply succeeds, timezone is persisted in NVS key `timezone`.
3. On next boot, persisted timezone is auto-applied.
4. If timezone is not configured, device clock uses UTC mode.

## ACK examples

Success:

```json
{
  "cmdId": "01HXYZK6H9B5O2Q3R6S8T0U1V4",
  "status": "ok",
  "ts": 1735000001000,
  "result": {
    "appliedTz": "CET-1CEST,M3.5.0/2,M10.5.0/3",
    "clockMode": "local"
  }
}
```

Invalid timezone:

```json
{
  "cmdId": "01HXYZK6H9B5O2Q3R6S8T0U1V4",
  "status": "rejected",
  "ts": 1735000001000,
  "error": {
    "code": "invalid_timezone",
    "message": "params.posixTz must be a non-empty POSIX TZ string (max 64 chars)"
  }
}
```

Apply failure:

```json
{
  "cmdId": "01HXYZK6H9B5O2Q3R6S8T0U1V4",
  "status": "error",
  "ts": 1735000001000,
  "error": {
    "code": "timezone_apply_failed",
    "message": "Failed to apply timezone on device"
  }
}
```

## UI rule (LIGHTING screen)

- If timezone is not configured: show `HH:MM:SS UTC`.
- If timezone is configured: show `HH:MM:SS`.
- Clock refresh updates only the clock area to avoid screen flicker.
