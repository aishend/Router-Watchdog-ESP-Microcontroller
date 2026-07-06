# Router Watchdog ESP8266

Firmware for a NodeMCU/ESP8266 that monitors Wi-Fi and internet connectivity, reports health to a backend, accepts remote commands, and power-cycles a router through a relay after repeated failures.

## What It Does

- Connects to a configured Wi-Fi network.
- Checks internet access with a TCP connection to `1.1.1.1:80`.
- Sends heartbeat payloads to a configurable HTTP/HTTPS backend.
- Accepts backend commands from heartbeat responses.
- Reboots the router by controlling a relay.
- Can reboot the ESP8266 device by backend command.
- Optionally discovers LAN clients with ARP scan and reports them to the backend.
- Uses a small centralized serial logger for consistent runtime output.

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
  backend/       Backend HTTP client and API payloads
  commands/      Backend command orchestration
  config/        Build-time configuration and local secrets example
  drivers/       Relay driver
  logging/       Serial logger
  network/       Wi-Fi status, internet checks, optional ARP scanner
  watchdog/      Failure tracking and router recovery state machine
  main.cpp       Boot sequence and main loop

docs/
  backend-api.md Backend API contract and example server
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
#define ROUTER_WATCHDOG_BACKEND_URL "https://your-api.example.com/api/v1/heartbeat"
#define ROUTER_WATCHDOG_DEVICE_ID "router-watchdog-001"
#define ROUTER_WATCHDOG_BACKEND_SECURE true
#define ROUTER_WATCHDOG_BACKEND_INSECURE_TLS true
#define ROUTER_WATCHDOG_ARP_SCAN_ENABLED false
```

`src/config/AppSecrets.h` is ignored by Git and must not be committed.

## Important Settings

| Setting | Default | Purpose |
| --- | --- | --- |
| `SERIAL_BAUD` | `74880` | Serial monitor speed. |
| `STARTUP_GRACE_PERIOD_MS` | `15000` | Initial wait before monitoring starts. |
| `CHECK_INTERVAL_MS` | `10000` | Time between connectivity checks. |
| `MAX_CONSECUTIVE_FAILURES` | `3` | Failures required before router reboot. |
| `ROUTER_POWER_OFF_TIME_MS` | `10000` | Time to keep router power off. |
| `ROUTER_RECOVERY_WAIT_TIME_MS` | `10000` | Wait after router power is restored. |
| `BACKEND_TIMEOUT_MS` | `10000` | Backend request timeout. |
| `BACKEND_SECURE` | from secrets | Use HTTPS when `true`, HTTP when `false`. |
| `BACKEND_INSECURE_TLS` | from secrets | Skips TLS certificate validation when HTTPS is enabled. |
| `ARP_SCAN_ENABLED` | from secrets | Enables optional LAN client discovery. |
| `NETWORK_SCAN_INTERVAL_MS` | `300000` | Time between ARP discovery scans. |
| `NETWORK_SCAN_MAX_HOSTS` | `254` | Maximum subnet hosts scanned per pass. |
| `DEBUG_LOGS_ENABLED` | `false` | Enables verbose payload and scan logs. |

For local HTTP backend development:

```cpp
#define ROUTER_WATCHDOG_BACKEND_URL "http://192.168.20.14:8080/api/v1/heartbeat"
#define ROUTER_WATCHDOG_BACKEND_SECURE false
```

For HTTPS deployments:

```cpp
#define ROUTER_WATCHDOG_BACKEND_SECURE true
```

`ROUTER_WATCHDOG_BACKEND_INSECURE_TLS true` is convenient during MVP testing because it skips certificate validation. Do not treat it as production-grade TLS validation.

## Backend Contract

The firmware posts heartbeats to the exact URL configured in `ROUTER_WATCHDOG_BACKEND_URL`, usually:

```text
POST /api/v1/heartbeat
```

It also derives these endpoints from the same `/api/v1` base:

```text
POST /api/v1/command-results
POST /api/v1/network-clients/report
```

Read [docs/backend-api.md](docs/backend-api.md) for the complete backend contract, payloads, command format, persistence model, and a minimal Node/Express server.

Heartbeat payload:

```json
{
  "deviceId": "router-watchdog-001",
  "ip": "192.168.1.100",
  "gateway": "192.168.1.1",
  "failures": 0,
  "uptime": 123,
  "rssi": -58,
  "freeHeap": 41200,
  "firmwareVersion": "0.1.0"
}
```

Supported backend commands:

| Command | Behavior |
| --- | --- |
| `REBOOT_ROUTER` | Starts the relay recovery flow and reports completion after recovery wait. |
| `REBOOT_DEVICE` | Reports completion, waits briefly, then restarts the ESP8266. |

## Optional ARP Scan

LAN client discovery is disabled by default:

```cpp
#define ROUTER_WATCHDOG_ARP_SCAN_ENABLED false
```

Enable it only when the backend needs LAN client reports:

```cpp
#define ROUTER_WATCHDOG_ARP_SCAN_ENABLED true
```

When enabled, the firmware scans up to `NETWORK_SCAN_MAX_HOSTS` hosts every `NETWORK_SCAN_INTERVAL_MS` and sends discovered clients to:

```text
POST /api/v1/network-clients/report
```

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
[BACKEND] Client initialized
[WATCHDOG] Monitoring started
[BOOT] Waiting for Wi-Fi startup grace period (15000 ms)
[BOOT] ARP scan=disabled
```

Typical healthy operation:

```text
[NETWORK] Wi-Fi connected | IP=192.168.1.100 | Gateway=192.168.1.1 | Internet=OK | Failures=0
[BACKEND] Sending heartbeat via HTTPS POST https://your-api.example.com/api/v1/heartbeat
[BACKEND] HTTPS heartbeat response status=200
```

## Recovery Flow

When connectivity fails `MAX_CONSECUTIVE_FAILURES` times in a row, the watchdog:

1. Turns the relay on and cuts router power.
2. Waits for `ROUTER_POWER_OFF_TIME_MS`.
3. Turns the relay off and restores router power.
4. Waits for `ROUTER_RECOVERY_WAIT_TIME_MS`.
5. Clears failure state and resumes monitoring.
6. Reports command completion if the recovery was triggered by `REBOOT_ROUTER`.

The recovery flow is state-based and avoids long blocking delays in the main loop.

## Release Checklist

- `src/config/AppSecrets.h` exists locally and contains real Wi-Fi/backend values.
- `ROUTER_WATCHDOG_ARP_SCAN_ENABLED` is set intentionally.
- Relay wiring matches `RELAY_GPIO` and `RELAY_ACTIVE_LOW`.
- Backend implements the contract in [docs/backend-api.md](docs/backend-api.md).
- `pio run` succeeds.
- Real credentials and private backend URLs are not committed.
