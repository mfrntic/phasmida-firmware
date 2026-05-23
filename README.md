# Phasmida Firmware Core

Firmware monorepo for two ESP32 device targets used in Phasmida:

- core_s3: M5Stack CoreS3 sensor hub (UI + probes + MQTT)
- timer_camera_f: M5Stack Timer Camera F streaming and quality control

Build system: PlatformIO

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

## Quick start

1. Clone repository.
2. Create local secrets override file:

```powershell
Copy-Item include\secrets.example.h include\secrets.local.h
```

3. Edit include\secrets.local.h and set private values (never commit this file):

```c
#pragma once

#define PHASMIDA_APP_MQTT_API_KEY "<core-device-api-key>"
#define PHASMIDA_APP_DEFAULT_WIFI_SSID "<wifi-ssid>"
#define PHASMIDA_APP_DEFAULT_WIFI_PASSWORD "<wifi-password>"

#define PHASMIDA_CAM_DEVICE_API_KEY "<camera-device-api-key>"
```

4. Build firmware.

## Build commands

PowerShell with bundled PlatformIO executable:

```powershell
$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe run -e core_s3
$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe run -e timer_camera_f
```

Upload examples:

```powershell
$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe run -e core_s3 -t upload
$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe run -e timer_camera_f -t upload
```

VS Code task currently included in workspace:

- PlatformIO: Build
- PlatformIO: Build (core_s3)
- PlatformIO: Build (timer_camera_f)

Adjust COM ports in platformio.ini for your machine before upload/monitor.

## Runtime configuration model

Configuration precedence is:

1. compile-time local overrides from include\secrets.local.h
2. runtime values stored in NVS
3. tracked public-safe defaults from include\secrets.example.h

This keeps public repository safe while preserving existing firmware behavior.

## MQTT and protocol

See docs/MQTT-PROTOCOL-v1.md for contract and payloads.

Current implementation note: MQTT currently uses plain TCP on port 1883 in firmware.

## Tools

MQTT camera emulator script no longer ships with embedded credentials.

Use one of:

- --password
- --api-key
- PHASMIDA_MQTT_API_KEY environment variable

## Troubleshooting

- Build fails with missing symbols:
  - Verify include\secrets.local.h exists and macro names are correct.
- Upload fails:
  - Check board COM port in platformio.ini.
- Provisioning does not start:
  - Confirm device has no valid Wi-Fi credentials in NVS.

## Contributing and security

- Contribution guide: CONTRIBUTING.md
- Security policy: SECURITY.md

## License

See LICENSE.
