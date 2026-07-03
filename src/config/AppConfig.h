#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <Arduino.h>

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

#ifndef ROUTER_WATCHDOG_DEVICE_ID
#define ROUTER_WATCHDOG_DEVICE_ID "router-watchdog-001"
#endif

namespace AppConfig {

constexpr unsigned long SERIAL_BAUD = 74880;

constexpr const char *WIFI_SSID = ROUTER_WATCHDOG_WIFI_SSID;
constexpr const char *WIFI_PASSWORD = ROUTER_WATCHDOG_WIFI_PASSWORD;

constexpr const char *INTERNET_TEST_HOST = "1.1.1.1";
constexpr uint16_t INTERNET_TEST_PORT = 80;
constexpr uint16_t INTERNET_TEST_TIMEOUT_MS = 2000;

constexpr const char *BACKEND_URL = ROUTER_WATCHDOG_BACKEND_URL;
constexpr const char *DEVICE_ID = ROUTER_WATCHDOG_DEVICE_ID;
constexpr uint16_t BACKEND_TIMEOUT_MS = 10000;

constexpr uint8_t MAX_CONSECUTIVE_FAILURES = 3;
constexpr unsigned long STARTUP_GRACE_PERIOD_MS = 15000;
constexpr unsigned long CHECK_INTERVAL_MS = 10000;
constexpr unsigned long ROUTER_POWER_OFF_TIME_MS = 10000;
constexpr unsigned long ROUTER_BOOT_WAIT_TIME_MS = 60000;
constexpr unsigned long RECOVERY_COOLDOWN_MS = 120000;

constexpr uint8_t RELAY_GPIO = 4; // NodeMCU D2
constexpr bool RELAY_ACTIVE_LOW = false;

}

#endif
