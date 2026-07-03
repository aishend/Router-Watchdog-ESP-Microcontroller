# Router Watchdog ESP8266

Firmware for a NodeMCU/ESP8266 that monitors Wi-Fi/internet connectivity, sends heartbeats to an HTTPS backend, and restarts the router through a relay after consecutive failures.

## Hardware

- NodeMCU ESP8266 ESP-12E
- Relay module connected to `D2` / `GPIO4`
- Stable power supply for the ESP8266 and relay

## Features

- Connects to the configured Wi-Fi network.
- Checks internet access through a TCP connection to `1.1.1.1:80`.
- Sends heartbeat JSON payloads to a configurable HTTPS backend.
- Counts consecutive Wi-Fi/internet failures.
- Triggers the relay to cut and restore router power after the failure limit is reached.
- Waits for an initial startup grace period to avoid false failures while Wi-Fi is still connecting.

## Project Structure

```text
src/
  backend/       HTTPS client and JSON payload
  config/        Central firmware configuration
  drivers/       Relay control
  network/       Wi-Fi and internet checks
  watchdog/      Failure handling and recovery logic
  main.cpp       Startup and main loop
```

## Configuration

Public/default configuration values are defined in:

```text
src/config/AppConfig.h
```

Private values should be placed in a local secrets file:

```text
src/config/AppSecrets.h
```

Create it from the example:

```bash
cp src/config/AppSecrets.example.h src/config/AppSecrets.h
```

Then edit `src/config/AppSecrets.h`:

```cpp
#define ROUTER_WATCHDOG_WIFI_SSID "your-wifi-name"
#define ROUTER_WATCHDOG_WIFI_PASSWORD "your-wifi-password"
#define ROUTER_WATCHDOG_BACKEND_URL "https://your-api.example.com/api/v1/heartbeat"
#define ROUTER_WATCHDOG_DEVICE_ID "router-watchdog-001"
#define ROUTER_WATCHDOG_BACKEND_INSECURE_TLS true
```

`src/config/AppSecrets.h` is ignored by Git and must not be committed.

Most important public/default fields:

```cpp
STARTUP_GRACE_PERIOD_MS
CHECK_INTERVAL_MS
MAX_CONSECUTIVE_FAILURES
RELAY_GPIO
RELAY_ACTIVE_LOW
BACKEND_INSECURE_TLS
```

Do not commit real Wi-Fi credentials, private backend URLs, or customer/company identifiers.

Note: `BACKEND_INSECURE_TLS` defaults to `true` for MVP convenience. The connection is encrypted, but the server certificate is not validated while this option is enabled.

## Build

```bash
pio run
```

## Upload

```bash
pio run -t upload
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
```

## Serial Monitor

```bash
pio device monitor
```

Configured speed: `74880`.

Expected startup output:

```text
[BOOT] Router watchdog starting
[RELAY] Initializing
[RELAY] OFF
[NETWORK] Initialization started
[NETWORK] Wi-Fi connection requested
[BACKEND] Client initialized
[BACKEND] WARNING: TLS certificate validation disabled
[WATCHDOG] Task started
[WATCHDOG] Monitoring started
[BOOT] Waiting for Wi-Fi startup grace period (15000 ms)
```

Expected normal operation:

```text
[NETWORK] Wi-Fi connected | IP=192.168.1.100 | Gateway=192.168.1.1 | Internet=OK | Failures=0
[BACKEND] Sending heartbeat
[HTTPS] Response status=200
```

## Recovery Flow

When the number of consecutive failures reaches `MAX_CONSECUTIVE_FAILURES`, the firmware:

1. Turns the relay on and cuts router power.
2. Waits for `ROUTER_POWER_OFF_TIME_MS`.
3. Turns the relay off and restores power.
4. Waits for `ROUTER_BOOT_WAIT_TIME_MS`.
5. Waits for the cooldown period defined by `RECOVERY_COOLDOWN_MS`.
6. Resumes monitoring.

The recovery flow is state-based rather than implemented with long blocking delays, so the firmware loop keeps running while it waits for power-off, boot, and cooldown timers.

## Backend

The heartbeat payload has this format:

```json
{
  "deviceId": "router-watchdog-001",
  "ip": "192.168.1.100",
  "gateway": "192.168.1.1",
  "failures": 0,
  "uptime": 123
}
```

The backend should return an HTTP `2xx` status for the heartbeat to be considered accepted.
