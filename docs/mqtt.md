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

Published when a command starts or is rejected.

```json
{
  "commandId": "cmd_20260706_001",
  "status": "STARTED"
}
```

`status` can be:

| Status | Meaning |
| --- | --- |
| `STARTED` | The ESP accepted the command and is about to execute it. For `REBOOT_ROUTER`, this is published shortly before relay activation. |
| `REJECTED` | The ESP rejected the command. Examples: duplicate `id`, unknown `type`, another command queued, or router recovery already running. |

For `REBOOT_ROUTER`, the ESP publishes `STARTED`, waits `COMMAND_STARTED_TO_RELAY_DELAY_MS`, then activates the relay. It does not publish `COMPLETED`. Backends should treat `STARTED` as the transition to `REBOOT_IN_PROGRESS`, then use heartbeat/availability recovery and their own timeout to decide when the router recovered or failed.

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

Commands must be published with `retain = false`. Commands are temporary events, not state. A retained command can be replayed after a reconnect and trigger an old action again.

Command timeout and expiry are backend responsibilities. The ESP ignores `expiresInSeconds` and `expiresAt` because it has no server-synchronized clock.

Supported command types:

| Type | Behavior |
| --- | --- |
| `REBOOT_ROUTER` | Publishes `STARTED`, waits `COMMAND_STARTED_TO_RELAY_DELAY_MS`, then starts the relay recovery flow. |
| `REBOOT_DEVICE` | Publishes `STARTED`, waits briefly, then restarts the ESP8266. |

Command rules:

- `id` must be unique and stable.
- Commands without `id` are ignored.
- Duplicate command IDs are rejected.
- Unknown command types are rejected.
- If a router reboot command arrives while recovery is already running, it is rejected.
- The MQTT callback only queues commands. Command execution happens from `CommandManager::tick()`.
