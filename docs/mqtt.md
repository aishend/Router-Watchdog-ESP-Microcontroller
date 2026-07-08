# MQTT Contract

This firmware uses MQTT as its only remote transport.

## Topics

All topics use this shape:

```text
{MQTT_TOPIC_PREFIX}/{DEVICE_ID}/{suffix}
```

With the defaults, the topics are:

```text
router-watchdog/router-watchdog-001/heartbeat
router-watchdog/router-watchdog-001/commands
router-watchdog/router-watchdog-001/command-results
router-watchdog/router-watchdog-001/availability
```

## Published By Firmware

### heartbeat

Published after each watchdog connectivity check.

```json
{
  "deviceId": "router-watchdog-001",
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

### command-results

Published after a command finishes or fails.

```json
{
  "commandId": "cmd_20260706_001",
  "status": "COMPLETED"
}
```

`status` is `COMPLETED` or `FAILED`.

### availability

Retained message. The firmware publishes:

```text
online
```

The broker publishes this last will when the MQTT connection drops unexpectedly:

```text
offline
```

## Subscribed By Firmware

### commands

Publish command messages to the device-specific commands topic.

```json
{
  "id": "cmd_20260706_001",
  "type": "REBOOT_ROUTER"
}
```

Supported command types:

| Type | Behavior |
| --- | --- |
| `REBOOT_ROUTER` | Starts the relay recovery flow and reports completion after recovery wait. |
| `REBOOT_DEVICE` | Reports completion, waits briefly, then restarts the ESP8266. |

Command rules:

- `id` must be unique and stable.
- Commands without `id` are ignored.
- Unknown command types are reported as `FAILED`.
- If a router reboot command arrives while recovery is already running, it is reported as `FAILED`.
