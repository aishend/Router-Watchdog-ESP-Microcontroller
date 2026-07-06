#ifndef APP_SECRETS_EXAMPLE_H
#define APP_SECRETS_EXAMPLE_H

// Copy this file to src/config/AppSecrets.h and replace the values.
// AppSecrets.h is ignored by Git and should never be committed.

#define ROUTER_WATCHDOG_WIFI_SSID "your-wifi-name"
#define ROUTER_WATCHDOG_WIFI_PASSWORD "your-wifi-password"
#define ROUTER_WATCHDOG_BACKEND_URL "https://your-api.example.com/api/v1/heartbeat"
#define ROUTER_WATCHDOG_DEVICE_ID "router-watchdog-001"
#define ROUTER_WATCHDOG_BACKEND_SECURE true
#define ROUTER_WATCHDOG_BACKEND_INSECURE_TLS true

#endif
