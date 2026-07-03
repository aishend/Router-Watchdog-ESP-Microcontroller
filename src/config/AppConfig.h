#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <Arduino.h>
#include <string.h>

#if __has_include("AppSecrets.h")
#include "AppSecrets.h"
#endif

#ifndef ROUTER_WATCHDOG_WIFI_SSID
#define ROUTER_WATCHDOG_WIFI_SSID "your-wifi-name"
#endif

#ifndef ROUTER_WATCHDOG_WIFI_PASSWORD
#define ROUTER_WATCHDOG_WIFI_PASSWORD "your-wifi-password"
#endif

#ifndef ROUTER_WATCHDOG_BACKEND_URL
#define ROUTER_WATCHDOG_BACKEND_URL "https://your-api-endpoint"
#endif

#ifndef ROUTER_WATCHDOG_BACKEND_INSECURE_TLS
#define ROUTER_WATCHDOG_BACKEND_INSECURE_TLS true
#endif

#ifndef ROUTER_WATCHDOG_DEVICE_ID
#define ROUTER_WATCHDOG_DEVICE_ID "router-watchdog-001"
#endif

namespace AppConfig
{

    constexpr unsigned long SERIAL_BAUD = 74880; // Serial monitor baud rate.

    constexpr const char *WIFI_SSID = ROUTER_WATCHDOG_WIFI_SSID;         // Wi-Fi network name.
    constexpr const char *WIFI_PASSWORD = ROUTER_WATCHDOG_WIFI_PASSWORD; // Wi-Fi password.

    constexpr const char *INTERNET_TEST_HOST = "1.1.1.1"; // Host used to test internet access.
    constexpr uint16_t INTERNET_TEST_PORT = 80;           // TCP port used for the internet test.
    constexpr uint16_t INTERNET_TEST_TIMEOUT_MS = 2000;   // Max time to wait for the internet test.

    constexpr const char *BACKEND_URL = ROUTER_WATCHDOG_BACKEND_URL;            // HTTPS endpoint that receives heartbeats.
    constexpr const char *DEVICE_ID = ROUTER_WATCHDOG_DEVICE_ID;                // Device identifier sent in each heartbeat.
    constexpr bool BACKEND_INSECURE_TLS = ROUTER_WATCHDOG_BACKEND_INSECURE_TLS; // true for MVP/testing; false requires certificate validation.
    constexpr uint16_t BACKEND_TIMEOUT_MS = 10000;                              // Max time to wait for the backend response.

    constexpr uint8_t MAX_CONSECUTIVE_FAILURES = 3;               // Failures required before restarting the router.
    constexpr unsigned long STARTUP_GRACE_PERIOD_MS = 15000;      // Initial wait before monitoring starts.
    constexpr unsigned long CHECK_INTERVAL_MS = 10000;            // Time between connectivity checks.
    constexpr unsigned long ROUTER_POWER_OFF_TIME_MS = 10000;     // Time to keep router power off.
    constexpr unsigned long ROUTER_RECOVERY_WAIT_TIME_MS = 300000; // Wait after power is restored.

    constexpr uint8_t RELAY_GPIO = 4;        // GPIO connected to the relay module (NodeMCU D2).
    constexpr bool RELAY_ACTIVE_LOW = false; // Set true if the relay turns on with LOW.

    inline bool hasPlaceholderSecrets()
    {
        return strcmp(WIFI_SSID, "your-wifi-name") == 0 ||
               strcmp(WIFI_PASSWORD, "your-wifi-password") == 0 ||
               strcmp(BACKEND_URL, "https://your-api-endpoint") == 0;
    }

}

#endif
