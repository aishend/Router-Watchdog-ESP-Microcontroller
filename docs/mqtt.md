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

### command-results

Published when a command starts, finishes, or fails.

```json
{
  "commandId": "cmd_20260706_001",
  "status": "STARTED"
}
```

`status` can be:

| Status | Meaning |
| --- | --- |
| `STARTED` | The ESP received the command and started the requested action. |
| `COMPLETED` | The command finished successfully. |
| `FAILED` | The command could not be completed. |

For `REBOOT_ROUTER`, the ESP publishes `STARTED` before cutting router power. Backends can treat the following network loss as expected and mark the command as `REBOOT_IN_PROGRESS` internally until `COMPLETED` arrives or their own timeout expires.

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
  "type": "REBOOT_ROUTER",
  "expiresInSeconds": 60
}
```

Commands must be published with `retain = false`. Commands are temporary events, not state. A retained command can be replayed after a reconnect and trigger an old action again.

Optional expiration fields:

| Field | Behavior |
| --- | --- |
| `expiresInSeconds` | Command is ignored when the value is `0` or negative. |
| `expiresAt` | Command is ignored when the value is less than or equal to the device uptime in seconds. |

Supported command types:

| Type | Behavior |
| --- | --- |
| `REBOOT_ROUTER` | Starts the relay recovery flow and reports completion after recovery wait. |
| `REBOOT_DEVICE` | Reports completion, waits briefly, then restarts the ESP8266. |

Command rules:

- `id` must be unique and stable.
- Commands without `id` are ignored.
- Unknown command types are ignored.
- If a router reboot command arrives while recovery is already running, it is reported as `FAILED`.
