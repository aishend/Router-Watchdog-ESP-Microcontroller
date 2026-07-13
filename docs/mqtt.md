# MQTT Contract

This firmware uses MQTT as its only remote transport.

Connections use MQTT over TLS on TCP port `3543`. The device validates the broker
certificate against its configured private CA and never falls back to plaintext.

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
router-watchdog/router-watchdog-001/firmware/desired
router-watchdog/router-watchdog-001/firmware/status
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

Rejected results may add `reason`: `duplicate_command_id`, `unknown_type`,
`operation_in_progress`, `router_recovery_in_progress`, `retry_cooldown_active`,
`device_reboot_pending`, or `firmware_update_in_progress`. Consumers that only read
`commandId` and `status` remain compatible.

`STARTED` is attempted at most three times, one second apart. If all attempts fail,
the locally accepted command still executes.

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

### firmware/status

Published immediately before download and when an update attempt fails:

```json
{
  "currentVersion": "0.1.0",
  "targetVersion": "0.2.0",
  "status": "DOWNLOADING"
}
```

Failure payloads use `"status": "FAILED"` and include an `error` field. Successful
installation is confirmed by the new `firmwareVersion` in the heartbeat after the
automatic restart; the firmware does not publish a `COMPLETED` status.

## Subscribed By Firmware

### firmware/desired

Desired firmware is retained state and must be published with `retain = true`:

```json
{
  "version": "0.2.0",
  "url": "https://203.0.113.10/firmware/router-watchdog-0.2.0.bin",
  "sha256": "64-hexadecimal-characters"
}
```

The firmware ignores a request whose version exactly matches the installed version.
It starts OTA only while the global operation state is idle, Internet was confirmed,
the relay is inactive, and router recovery/cooldown is not active. HTTPS and a valid
CA chain are mandatory. The declared size must fit the OTA flash area and the binary
is committed only after its SHA-256 matches.
Because the desired message remains retained, a temporarily offline device receives
it when MQTT reconnects.

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
