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

#ifndef ROUTER_WATCHDOG_BACKEND_SECURE
#define ROUTER_WATCHDOG_BACKEND_SECURE true
#endif

#ifndef ROUTER_WATCHDOG_DEVICE_ID
#define ROUTER_WATCHDOG_DEVICE_ID "router-watchdog-001"
#endif

#ifndef ROUTER_WATCHDOG_ARP_SCAN_ENABLED
#define ROUTER_WATCHDOG_ARP_SCAN_ENABLED false
#endif

namespace AppConfig
{
    constexpr const char *FIRMWARE_VERSION = "0.1.0";

    constexpr unsigned long SERIAL_BAUD = 74880;
    constexpr bool DEBUG_LOGS_ENABLED = false;

    constexpr const char *WIFI_SSID = ROUTER_WATCHDOG_WIFI_SSID;
    constexpr const char *WIFI_PASSWORD = ROUTER_WATCHDOG_WIFI_PASSWORD;

    constexpr const char *INTERNET_TEST_HOST = "1.1.1.1";
    constexpr uint16_t INTERNET_TEST_PORT = 80;
    constexpr uint16_t INTERNET_TEST_TIMEOUT_MS = 2000;

    constexpr const char *BACKEND_URL = ROUTER_WATCHDOG_BACKEND_URL;
    constexpr const char *DEVICE_ID = ROUTER_WATCHDOG_DEVICE_ID;
    constexpr bool BACKEND_SECURE = ROUTER_WATCHDOG_BACKEND_SECURE;
    constexpr bool BACKEND_INSECURE_TLS = ROUTER_WATCHDOG_BACKEND_INSECURE_TLS;
    constexpr uint16_t BACKEND_TIMEOUT_MS = 10000;
    constexpr bool ARP_SCAN_ENABLED = ROUTER_WATCHDOG_ARP_SCAN_ENABLED;
    constexpr unsigned long NETWORK_SCAN_INTERVAL_MS = 300000;
    constexpr uint16_t NETWORK_SCAN_MAX_HOSTS = 254;

    constexpr uint8_t MAX_CONSECUTIVE_FAILURES = 3;
    constexpr unsigned long STARTUP_GRACE_PERIOD_MS = 15000;
    constexpr unsigned long CHECK_INTERVAL_MS = 10000;
    constexpr unsigned long ROUTER_POWER_OFF_TIME_MS = 10000;
    constexpr unsigned long ROUTER_RECOVERY_WAIT_TIME_MS = 10000;

    constexpr uint8_t RELAY_GPIO = 4;
    constexpr bool RELAY_ACTIVE_LOW = false;

    inline bool hasPlaceholderSecrets()
    {
        return strcmp(WIFI_SSID, "your-wifi-name") == 0 ||
               strcmp(WIFI_PASSWORD, "your-wifi-password") == 0 ||
               strcmp(BACKEND_URL, "https://your-api-endpoint") == 0;
    }
}

#endif
