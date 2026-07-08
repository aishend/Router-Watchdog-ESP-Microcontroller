# Router Watchdog ESP8266

Firmware for a NodeMCU/ESP8266 that monitors Wi-Fi and internet connectivity, reports health over MQTT, accepts MQTT commands, and power-cycles a router through a relay after repeated failures.

## What It Does

- Connects to a configured Wi-Fi network.
- Checks internet access with TCP connections to multiple configured targets.
- Sends heartbeat payloads to a configurable MQTT broker.
- Accepts remote commands from MQTT.
- Reboots the router by controlling a relay.
- Can reboot the ESP8266 device by MQTT command.

## Hardware

- NodeMCU ESP8266 ESP-12E.
- Relay module connected to `D2` / `GPIO4`.
- Stable power supply for the ESP8266 and relay.

Relay behavior is controlled by `RELAY_ACTIVE_LOW` in `src/config/AppConfig.h`:

```cpp
constexpr bool RELAY_ACTIVE_LOW = false;
```

Use `false` for relay modules that turn on with `HIGH`. Use `true` for relay modules that turn on with `LOW`.

## Project Structure

```text
src/
  commands/      Command orchestration and command result types
  config/        Build-time configuration and local secrets example
  drivers/       Relay driver
  mqtt/          MQTT transport, topics, command parsing, and payload builders
  network/       Wi-Fi status and internet checks
  watchdog/      Failure tracking and router recovery state machine
  main.cpp       Boot sequence and main loop

docs/
  mqtt.md        MQTT topics and payloads
```

## Configuration

Default public configuration lives in:

```text
src/config/AppConfig.h
```

Private local values should live in:

```text
src/config/AppSecrets.h
```

Create it from the example:

```bash
cp src/config/AppSecrets.example.h src/config/AppSecrets.h
```

Example `AppSecrets.h`:

```cpp
#define ROUTER_WATCHDOG_WIFI_SSID "your-wifi-name"
#define ROUTER_WATCHDOG_WIFI_PASSWORD "your-wifi-password"
#define ROUTER_WATCHDOG_DEVICE_ID "router-watchdog-001"
#define ROUTER_WATCHDOG_MQTT_HOST "192.168.1.10"
#define ROUTER_WATCHDOG_MQTT_PORT 1883
#define ROUTER_WATCHDOG_MQTT_USER "your-mqtt-user"
#define ROUTER_WATCHDOG_MQTT_PASSWORD "your-mqtt-password"
#define ROUTER_WATCHDOG_MQTT_TOPIC_PREFIX "router-watchdog"
#define ROUTER_WATCHDOG_USE_PRODUCTION_TIMINGS false
```

`src/config/AppSecrets.h` is ignored by Git and must not be committed.

## Important Settings

| Setting | Default | Purpose |
| --- | --- | --- |
| `SERIAL_BAUD` | `74880` | Serial monitor speed. |
| `INTERNET_TEST_HOSTS` | `1.1.1.1`, `8.8.8.8`, `9.9.9.9` | Connectivity targets. Internet is OK when any target responds. |
| `INTERNET_TEST_PORT` | `80` | TCP port used for connectivity checks. |
| `INTERNET_TEST_TIMEOUT_MS` | `2000` | Per-target connection timeout. |
| `USE_PRODUCTION_TIMINGS` | `false` | Selects production timings when `true`; test timings when `false`. Set with `ROUTER_WATCHDOG_USE_PRODUCTION_TIMINGS`. |
| `COMMAND_STARTED_TO_RELAY_DELAY_MS` | `750` | Delay between publishing command `STARTED` and cutting router power. |
| `STARTUP_GRACE_PERIOD_MS` | timing profile | Initial wait before monitoring starts. |
| `CHECK_INTERVAL_MS` | timing profile | Time between connectivity checks. |
| `MAX_CONSECUTIVE_FAILURES` | timing profile | Failures required before router reboot. |
| `ROUTER_POWER_OFF_TIME_MS` | timing profile | Time to keep router power off. |
| `ROUTER_RECOVERY_WAIT_TIME_MS` | timing profile | Wait after router power is restored. |
| `MQTT_HOST` | from secrets | MQTT broker host or IP address. |
| `MQTT_PORT` | `1883` | MQTT broker port. |
| `MQTT_USER` | from secrets | MQTT username. |
| `MQTT_PASSWORD` | from secrets | MQTT password. |
| `MQTT_TOPIC_PREFIX` | `router-watchdog` | Prefix for all firmware topics. |
| `MQTT_BUFFER_SIZE` | `512` | MQTT packet buffer size. |
| `MQTT_RECONNECT_INTERVAL_MS` | `5000` | Time between MQTT reconnect attempts. |

Timing profiles:

| Setting | Test | Production |
| --- | ---: | ---: |
| `STARTUP_GRACE_PERIOD_MS` | `15000` | `60000` |
| `CHECK_INTERVAL_MS` | `10000` | `30000` |
| `MAX_CONSECUTIVE_FAILURES` | `3` | `5` |
| `ROUTER_POWER_OFF_TIME_MS` | `10000` | `15000` |
| `ROUTER_RECOVERY_WAIT_TIME_MS` | `10000` | `180000` |

Read [docs/mqtt.md](docs/mqtt.md) for the topic contract, payloads, and command format.

Heartbeat payload:

```json
{
  "deviceId": "router-watchdog-001",
  "sequence": 1234,
  "ip": "192.168.1.100",
  "gateway": "192.168.1.1",
  "wifiConnected": true,
  "internetConnected": true,
  "failures": 0,
  "uptime": 123,
  "rssi": -58,
  "freeHeap": 41200,
  "firmwareVersion": "0.1.0"
}
```

Supported MQTT commands:

| Command | Behavior |
| --- | --- |
| `REBOOT_ROUTER` | Publishes `STARTED`, waits `COMMAND_STARTED_TO_RELAY_DELAY_MS`, then starts the relay recovery flow. |
| `REBOOT_DEVICE` | Publishes `STARTED`, waits briefly, then restarts the ESP8266. |

Heartbeats are skipped while MQTT is disconnected.

## Build

```bash
pio run
```

Current PlatformIO environment:

```ini
[env:esp12e]
platform = espressif8266
board = esp12e
framework = arduino
upload_speed = 57600
upload_resetmethod = nodemcu
monitor_speed = 74880
monitor_dtr = 0
monitor_rts = 0
lib_deps =
    bblanchon/ArduinoJson
    knolleary/PubSubClient
```

## Upload

```bash
pio run -t upload
```

## Serial Monitor

```bash
pio device monitor
```

Configured speed: `74880`.

Typical startup output:

```text
[BOOT] Router watchdog starting
[RELAY] Initializing
[NETWORK] Initialization started
[MQTT] Client initialized
[WATCHDOG] Monitoring started
[BOOT] Waiting for Wi-Fi startup grace period (15000 ms)
```

Typical healthy operation:

```text
[NETWORK] Wi-Fi connected | IP=192.168.1.100 | Gateway=192.168.1.1 | Internet=OK | Failures=0
[MQTT] Connecting to 192.168.1.10:1883
[MQTT] Connected
```

## Recovery Flow

When connectivity fails `MAX_CONSECUTIVE_FAILURES` times in a row, the watchdog:

1. Turns the relay on and cuts router power.
2. Waits for `ROUTER_POWER_OFF_TIME_MS`.
3. Turns the relay off and restores router power.
4. Waits for `ROUTER_RECOVERY_WAIT_TIME_MS`.
5. Resumes monitoring and waits for Wi-Fi plus internet confirmation.
6. Clears failure state and allows future router reboot commands.

The recovery flow is state-based and avoids long blocking delays in the main loop.

## Future Improvements

- Persist recent command IDs in LittleFS or EEPROM so duplicate command protection survives device restarts.
- Tune production timing values after real router measurements.

## Release Checklist

- `src/config/AppSecrets.h` exists locally and contains real Wi-Fi/MQTT values.
- Relay wiring matches `RELAY_GPIO` and `RELAY_ACTIVE_LOW`.
- MQTT broker and consumers follow [docs/mqtt.md](docs/mqtt.md).
- `pio run` succeeds.
- Real credentials and private broker details are not committed.
