#pragma once

// Public-safe defaults for local development.
// Copy this file to include/secrets.local.h and fill private values there.
// secrets.local.h is gitignored and may override any macro below.

#ifndef PHASMIDA_APP_MQTT_API_KEY
#define PHASMIDA_APP_MQTT_API_KEY ""
#endif

#ifndef PHASMIDA_APP_DEFAULT_WIFI_SSID
#define PHASMIDA_APP_DEFAULT_WIFI_SSID ""
#endif

#ifndef PHASMIDA_APP_DEFAULT_WIFI_PASSWORD
#define PHASMIDA_APP_DEFAULT_WIFI_PASSWORD ""
#endif

#ifndef PHASMIDA_APP_PROVISIONING_AP_PASSWORD
#define PHASMIDA_APP_PROVISIONING_AP_PASSWORD "phasmida123"
#endif

#ifndef PHASMIDA_CAM_DEVICE_API_KEY
#define PHASMIDA_CAM_DEVICE_API_KEY ""
#endif

#ifndef PHASMIDA_CAM_PROVISIONING_AP_PASSWORD
#define PHASMIDA_CAM_PROVISIONING_AP_PASSWORD "phasmida123"
#endif
