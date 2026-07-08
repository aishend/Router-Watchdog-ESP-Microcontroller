#ifndef APP_SECRETS_EXAMPLE_H
#define APP_SECRETS_EXAMPLE_H

// Copy this file to src/config/AppSecrets.h and replace the values.
// AppSecrets.h is ignored by Git and should never be committed.

#define ROUTER_WATCHDOG_WIFI_SSID "your-wifi-name"
#define ROUTER_WATCHDOG_WIFI_PASSWORD "your-wifi-password"
#define ROUTER_WATCHDOG_DEVICE_ID "router-watchdog-001"
#define ROUTER_WATCHDOG_MQTT_HOST "192.168.1.10"
#define ROUTER_WATCHDOG_MQTT_PORT 1883
#define ROUTER_WATCHDOG_MQTT_USER "your-mqtt-user"
#define ROUTER_WATCHDOG_MQTT_PASSWORD "your-mqtt-password"
#define ROUTER_WATCHDOG_MQTT_TOPIC_PREFIX "router-watchdog"
#define ROUTER_WATCHDOG_USE_PRODUCTION_TIMINGS false

#endif
