# Phasmida Firmware Core

Firmware monorepo for two ESP32 device targets used in Phasmida:

- core_s3: M5Stack CoreS3 sensor hub (UI + probes + MQTT)
- timer_camera_f: M5Stack Timer Camera F streaming and quality control

Build system: PlatformIO

Supported PlatformIO environments:

- `core_s3` (default): M5Stack CoreS3 sensor hub firmware
- `timer_camera_f`: M5Stack Timer Camera F firmware

## Repository layout

```text
include/                 Shared headers and compile-time config
src/core_s3/             CoreS3 firmware sources
src/timer_camera/        Timer Camera firmware sources
src/shared/              Shared implementation (Wi-Fi, time sync, identity)
docs/                    Protocol and product docs
tools/                   Local helper scripts
platformio.ini           PlatformIO environments
```

## Prerequisites

- VS Code with PlatformIO extension
- Python 3.10+ (for helper scripts)
- USB drivers for your board (COM port visibility)

Optional but recommended:

- Serial terminal (PlatformIO monitor or equivalent)
- MQTT broker access for runtime validation

## Quick start

1. Clone repository.
2. Create local secrets override file:

```powershell
Copy-Item include\secrets.example.h include\secrets.local.h
```

3. Edit include\secrets.local.h and set private values (never commit this file):

```c
#pragma once

#define PHASMIDA_CORE_DEVICE_API_KEY "<core-device-api-key>"

#define PHASMIDA_CAM_DEVICE_API_KEY "<camera-device-api-key>"
```

4. Build firmware.

## First-time setup checklist

1. Verify `platformio.ini` COM ports for your machine:
  - `core_s3` typically uses `upload_port`/`monitor_port` from `[env:core_s3]`
  - `timer_camera_f` typically uses `upload_port`/`monitor_port` from `[env:timer_camera_f]`
2. Confirm `include\secrets.local.h` is present and contains valid API keys.
3. Build at least one target (`core_s3` or `timer_camera_f`).
4. Flash and open serial monitor to confirm clean boot.

## Build commands

PowerShell with bundled PlatformIO executable:

```powershell
$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe run -e core_s3
$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe run -e timer_camera_f
```

Clean build examples:

```powershell
$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe run -e core_s3 -t clean
$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe run -e timer_camera_f -t clean
```

Upload examples:

```powershell
$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe run -e core_s3 -t upload
$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe run -e timer_camera_f -t upload
```

Serial monitor examples:

```powershell
$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe device monitor -e core_s3
$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe device monitor -e timer_camera_f
```

One-command upload + monitor examples:

```powershell
$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe run -e core_s3 -t upload -t monitor
$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe run -e timer_camera_f -t upload -t monitor
```

VS Code task currently included in workspace:

- PlatformIO: Build
- PlatformIO: Build (core_s3)
- PlatformIO: Build (timer_camera_f)

Adjust COM ports in platformio.ini for your machine before upload/monitor.

If you prefer VS Code tasks, use:

- `PlatformIO: Build`
- `PlatformIO: Build (core_s3)`
- `PlatformIO: Build (timer_camera_f)`

## Runtime configuration model

Configuration precedence is:

1. compile-time local overrides from include\secrets.local.h
2. runtime values stored in NVS
3. tracked public-safe defaults from app/camera config headers

This keeps public repository safe while preserving existing firmware behavior.

Notes:

- `include\secrets.local.h` is intentionally gitignored and machine-local.
- Runtime values in NVS may override some defaults after device provisioning.

## MQTT and protocol

See docs/MQTT-PROTOCOL-v1.md for contract and payloads.

Current implementation note: MQTT currently uses plain TCP on port 1883 in firmware.

Integration tip:

- Keep topic naming and payload shape aligned with `docs/MQTT-PROTOCOL-v1.md` when adding new telemetry or commands.

## Tools

MQTT camera emulator script no longer ships with embedded credentials.

Use one of:

- --password
- --api-key
- PHASMIDA_MQTT_API_KEY environment variable

Useful scripts in `tools/`:

- `reset_device.py`: helper reset/provisioning workflow utility
- `camera_quality_tuner.py`: assist tuning camera quality parameters
- `jpg_to_header.py`: convert image assets to firmware header format

## Troubleshooting

- Build fails with missing symbols:
  - Verify include\secrets.local.h exists and macro names are correct.
- Upload fails:
  - Check board COM port in platformio.ini.
- Provisioning does not start:
  - Confirm device has no valid Wi-Fi credentials in NVS.
- Serial monitor shows no output:
  - Verify correct `monitor_port` and that monitor speed is `115200`.
- Build unexpectedly uses wrong target:
  - Specify `-e core_s3` or `-e timer_camera_f` explicitly.

## Typical development workflow

1. Update code/config for one target.
2. Run target build.
3. Flash device.
4. Open serial monitor and verify startup logs.
5. Validate MQTT behavior against protocol doc.
6. Repeat for second target when shared code changes.

## Release-safety reminders

- Never commit `include\secrets.local.h`.
- Avoid embedding credentials in scripts or docs.
- Keep protocol or payload changes synchronized with docs.

## Contributing and security

- Contribution guide: CONTRIBUTING.md
- Security policy: SECURITY.md

## License

See LICENSE.
