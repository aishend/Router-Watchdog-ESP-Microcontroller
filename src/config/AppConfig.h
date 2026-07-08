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

#ifndef ROUTER_WATCHDOG_MQTT_HOST
#define ROUTER_WATCHDOG_MQTT_HOST "192.168.1.10"
#endif

#ifndef ROUTER_WATCHDOG_MQTT_PORT
#define ROUTER_WATCHDOG_MQTT_PORT 1883
#endif

#ifndef ROUTER_WATCHDOG_MQTT_USER
#define ROUTER_WATCHDOG_MQTT_USER "your-mqtt-user"
#endif

#ifndef ROUTER_WATCHDOG_MQTT_PASSWORD
#define ROUTER_WATCHDOG_MQTT_PASSWORD "your-mqtt-password"
#endif

#ifndef ROUTER_WATCHDOG_MQTT_TOPIC_PREFIX
#define ROUTER_WATCHDOG_MQTT_TOPIC_PREFIX "router-watchdog"
#endif

#ifndef ROUTER_WATCHDOG_DEVICE_ID
#define ROUTER_WATCHDOG_DEVICE_ID "router-watchdog-001"
#endif

#ifndef ROUTER_WATCHDOG_USE_PRODUCTION_TIMINGS
#define ROUTER_WATCHDOG_USE_PRODUCTION_TIMINGS false
#endif

namespace AppConfig
{
    constexpr const char *FIRMWARE_VERSION = "0.1.0";

    constexpr unsigned long SERIAL_BAUD = 74880;

    constexpr const char *WIFI_SSID = ROUTER_WATCHDOG_WIFI_SSID;
    constexpr const char *WIFI_PASSWORD = ROUTER_WATCHDOG_WIFI_PASSWORD;

    constexpr const char *INTERNET_TEST_HOSTS[] = {
        "1.1.1.1",
        "8.8.8.8",
        "9.9.9.9",
    };
    constexpr size_t INTERNET_TEST_HOST_COUNT = sizeof(INTERNET_TEST_HOSTS) / sizeof(INTERNET_TEST_HOSTS[0]);
    constexpr uint16_t INTERNET_TEST_PORT = 80;
    constexpr uint16_t INTERNET_TEST_TIMEOUT_MS = 2000;

    constexpr const char *DEVICE_ID = ROUTER_WATCHDOG_DEVICE_ID;

    constexpr const char *MQTT_HOST = ROUTER_WATCHDOG_MQTT_HOST;
    constexpr uint16_t MQTT_PORT = ROUTER_WATCHDOG_MQTT_PORT;
    constexpr const char *MQTT_USER = ROUTER_WATCHDOG_MQTT_USER;
    constexpr const char *MQTT_PASSWORD = ROUTER_WATCHDOG_MQTT_PASSWORD;
    constexpr const char *MQTT_TOPIC_PREFIX = ROUTER_WATCHDOG_MQTT_TOPIC_PREFIX;
    constexpr uint16_t MQTT_BUFFER_SIZE = 512;
    constexpr unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;

    constexpr bool USE_PRODUCTION_TIMINGS = ROUTER_WATCHDOG_USE_PRODUCTION_TIMINGS;

    constexpr uint8_t TEST_MAX_CONSECUTIVE_FAILURES = 3;
    constexpr unsigned long TEST_STARTUP_GRACE_PERIOD_MS = 15000;
    constexpr unsigned long TEST_CHECK_INTERVAL_MS = 10000;
    constexpr unsigned long TEST_ROUTER_POWER_OFF_TIME_MS = 10000;
    constexpr unsigned long TEST_ROUTER_RECOVERY_WAIT_TIME_MS = 10000;

    constexpr uint8_t PRODUCTION_MAX_CONSECUTIVE_FAILURES = 5;
    constexpr unsigned long PRODUCTION_STARTUP_GRACE_PERIOD_MS = 60000;
    constexpr unsigned long PRODUCTION_CHECK_INTERVAL_MS = 30000;
    constexpr unsigned long PRODUCTION_ROUTER_POWER_OFF_TIME_MS = 15000;
    constexpr unsigned long PRODUCTION_ROUTER_RECOVERY_WAIT_TIME_MS = 180000;

    constexpr uint8_t MAX_CONSECUTIVE_FAILURES = USE_PRODUCTION_TIMINGS ? PRODUCTION_MAX_CONSECUTIVE_FAILURES : TEST_MAX_CONSECUTIVE_FAILURES;
    constexpr unsigned long STARTUP_GRACE_PERIOD_MS = USE_PRODUCTION_TIMINGS ? PRODUCTION_STARTUP_GRACE_PERIOD_MS : TEST_STARTUP_GRACE_PERIOD_MS;
    constexpr unsigned long CHECK_INTERVAL_MS = USE_PRODUCTION_TIMINGS ? PRODUCTION_CHECK_INTERVAL_MS : TEST_CHECK_INTERVAL_MS;
    constexpr unsigned long ROUTER_POWER_OFF_TIME_MS = USE_PRODUCTION_TIMINGS ? PRODUCTION_ROUTER_POWER_OFF_TIME_MS : TEST_ROUTER_POWER_OFF_TIME_MS;
    constexpr unsigned long ROUTER_RECOVERY_WAIT_TIME_MS = USE_PRODUCTION_TIMINGS ? PRODUCTION_ROUTER_RECOVERY_WAIT_TIME_MS : TEST_ROUTER_RECOVERY_WAIT_TIME_MS;

    constexpr unsigned long COMMAND_STARTED_TO_RELAY_DELAY_MS = 750;

    constexpr uint8_t RELAY_GPIO = 4;
    constexpr bool RELAY_ACTIVE_LOW = true;

    inline bool hasPlaceholderSecrets()
    {
        return strcmp(WIFI_SSID, "your-wifi-name") == 0 ||
               strcmp(WIFI_PASSWORD, "your-wifi-password") == 0 ||
               strcmp(MQTT_HOST, "192.168.1.10") == 0 ||
               strcmp(MQTT_USER, "your-mqtt-user") == 0 ||
               strcmp(MQTT_PASSWORD, "your-mqtt-password") == 0;
    }
}

#endif
