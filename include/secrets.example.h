#pragma once

// Public-safe defaults for local development.
// Copy this file to include/secrets.local.h and fill private values there.
// secrets.local.h is gitignored and may override any macro below.

// Backward compatibility: old name used in earlier firmware revisions.
#if !defined(PHASMIDA_CORE_DEVICE_API_KEY) && defined(PHASMIDA_APP_MQTT_API_KEY)
#define PHASMIDA_CORE_DEVICE_API_KEY PHASMIDA_APP_MQTT_API_KEY
#endif

#ifndef PHASMIDA_CORE_DEVICE_API_KEY
#define PHASMIDA_CORE_DEVICE_API_KEY ""
#endif

// Optional per-target hardcoded keys for core firmware variants.
// If omitted, both targets fall back to PHASMIDA_CORE_DEVICE_API_KEY.
#ifndef PHASMIDA_CORE_S3_DEVICE_API_KEY
#define PHASMIDA_CORE_S3_DEVICE_API_KEY PHASMIDA_CORE_DEVICE_API_KEY
#endif

#ifndef PHASMIDA_ATOMS3_LITE_DEVICE_API_KEY
#define PHASMIDA_ATOMS3_LITE_DEVICE_API_KEY PHASMIDA_CORE_DEVICE_API_KEY
#endif

#ifndef PHASMIDA_CAM_DEVICE_API_KEY
#define PHASMIDA_CAM_DEVICE_API_KEY ""
#endif
