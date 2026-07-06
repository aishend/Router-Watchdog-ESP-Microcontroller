# Backend API Contract

This document describes the HTTP API expected by the Router Watchdog firmware. A backend developer should be able to implement the server from this contract without reading the firmware code.

## Base URL

Configure the firmware with the full heartbeat endpoint:

```cpp
#define ROUTER_WATCHDOG_BACKEND_URL "https://your-api.example.com/api/v1/heartbeat"
```

The firmware posts heartbeats to that exact URL. It derives the other endpoints from the same host and `/api/v1` prefix:

```text
POST /api/v1/heartbeat
POST /api/v1/command-results
POST /api/v1/network-clients/report
```

For local development over HTTP:

```cpp
#define ROUTER_WATCHDOG_BACKEND_URL "http://192.168.20.14:8080/api/v1/heartbeat"
#define ROUTER_WATCHDOG_BACKEND_SECURE false
```

## General Requirements

- Accept `Content-Type: application/json`.
- Return any `2xx` status for successful requests.
- Keep responses small. The device is an ESP8266 with limited memory.
- Treat `deviceId` / `watchdogDeviceId` as the stable device identifier.
- Use idempotent command handling. Duplicate HTTP requests should not create duplicate commands or corrupt command state.

Authentication is not implemented by the firmware yet. Put the API behind a private network, VPN, reverse proxy rule, or add firmware/server auth together before exposing it publicly.

## POST /api/v1/heartbeat

Sent periodically after the startup grace period.

Request body:

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

Fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `deviceId` | string | Firmware device identifier. |
| `ip` | string | Current ESP8266 LAN IP. |
| `gateway` | string | Current gateway/router IP. |
| `failures` | number | Consecutive connectivity failures observed by the watchdog. |
| `uptime` | number | Device uptime in seconds. |
| `rssi` | number | Wi-Fi signal strength in dBm. |
| `freeHeap` | number | Free heap bytes reported by the ESP8266. |
| `firmwareVersion` | string | Firmware version string. |

Response with no command:

```json
{}
```

Response with a command:

```json
{
  "command": {
    "id": "cmd_20260706_001",
    "type": "REBOOT_ROUTER"
  }
}
```

Supported command types:

| Type | Behavior |
| --- | --- |
| `REBOOT_ROUTER` | Starts the relay recovery flow: router power off, wait, power on, recovery wait. |
| `REBOOT_DEVICE` | Reports `COMPLETED`, waits briefly, then restarts the ESP8266. |

Command rules:

- `command.id` must be unique and stable.
- Commands without `command.id` are ignored by the firmware.
- Return only one active command at a time per device.
- After returning a command, keep it pending until `/api/v1/command-results` confirms completion or failure.
- Unknown command types are treated as failed by the firmware.
- `REBOOT_DEVICE` sends `COMPLETED` before restarting the ESP8266.

## POST /api/v1/command-results

Sent after a command finishes or fails.

Request body:

```json
{
  "commandId": "cmd_20260706_001",
  "status": "COMPLETED"
}
```

Fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `commandId` | string | The `command.id` returned by the heartbeat endpoint. |
| `status` | string | `COMPLETED` or `FAILED`. |

Response:

```json
{}
```

Return `2xx` when the result is stored. If the response is not `2xx` or the request fails, the firmware logs the delivery failure. The current firmware does not keep a durable retry queue, so the backend should make this endpoint reliable and fast.

## POST /api/v1/network-clients/report

Sent after LAN client discovery scans when at least one client is found. This endpoint is only used when the firmware is built with:

```cpp
#define ROUTER_WATCHDOG_ARP_SCAN_ENABLED true
```

Request body:

```json
{
  "watchdogDeviceId": "router-watchdog-001",
  "clients": [
    {
      "ip": "192.168.1.25",
      "mac": "AA:BB:CC:DD:EE:FF",
      "hostname": null,
      "vendor": null
    }
  ]
}
```

Fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `watchdogDeviceId` | string | Firmware device identifier. |
| `clients` | array | Discovered LAN clients. |
| `clients[].ip` | string | Client IP address. |
| `clients[].mac` | string | Client MAC address. |
| `clients[].hostname` | null | Reserved for future hostname discovery. |
| `clients[].vendor` | null | Reserved for future MAC vendor lookup. |

Response:

```json
{}
```

## Minimal Node/Express Server

This is a small reference server. It stores data in memory, which is useful for development only.

```js
const express = require("express");

const app = express();
app.use(express.json({ limit: "32kb" }));

const devices = new Map();
const pendingCommands = new Map();
const commandResults = new Map();
const networkReports = [];

app.post("/api/v1/heartbeat", (req, res) => {
  const heartbeat = req.body;

  if (!heartbeat.deviceId) {
    return res.status(400).json({ error: "deviceId is required" });
  }

  devices.set(heartbeat.deviceId, {
    ...heartbeat,
    lastSeenAt: new Date().toISOString()
  });

  const command = pendingCommands.get(heartbeat.deviceId);

  if (!command) {
    return res.status(200).json({});
  }

  command.deliveredAt = command.deliveredAt ?? new Date().toISOString();
  return res.status(200).json({
    command: {
      id: command.id,
      type: command.type
    }
  });
});

app.post("/api/v1/command-results", (req, res) => {
  const { commandId, status } = req.body;

  if (!commandId || !["COMPLETED", "FAILED"].includes(status)) {
    return res.status(400).json({ error: "commandId and valid status are required" });
  }

  commandResults.set(commandId, {
    commandId,
    status,
    receivedAt: new Date().toISOString()
  });

  for (const [deviceId, command] of pendingCommands.entries()) {
    if (command.id === commandId) {
      pendingCommands.delete(deviceId);
      break;
    }
  }

  return res.status(200).json({});
});

app.post("/api/v1/network-clients/report", (req, res) => {
  const { watchdogDeviceId, clients } = req.body;

  if (!watchdogDeviceId || !Array.isArray(clients)) {
    return res.status(400).json({ error: "watchdogDeviceId and clients are required" });
  }

  networkReports.push({
    watchdogDeviceId,
    clients,
    receivedAt: new Date().toISOString()
  });

  return res.status(200).json({});
});

app.post("/dev/devices/:deviceId/commands", (req, res) => {
  const { deviceId } = req.params;
  const { type } = req.body;

  if (!["REBOOT_ROUTER", "REBOOT_DEVICE"].includes(type)) {
    return res.status(400).json({ error: "Unsupported command type" });
  }

  const command = {
    id: `cmd_${Date.now()}`,
    type,
    createdAt: new Date().toISOString()
  };

  pendingCommands.set(deviceId, command);
  return res.status(201).json(command);
});

app.get("/dev/devices", (req, res) => {
  return res.json(Array.from(devices.values()));
});

app.listen(8080, () => {
  console.log("Router Watchdog API listening on http://0.0.0.0:8080");
});
```

Run it:

```bash
npm init -y
npm install express
node server.js
```

Create a test command:

```bash
curl -X POST http://localhost:8080/dev/devices/router-watchdog-001/commands \
  -H "Content-Type: application/json" \
  -d '{"type":"REBOOT_ROUTER"}'
```

## Persistence Model

For a real backend, store at least:

| Table / Collection | Purpose |
| --- | --- |
| `devices` | Last heartbeat, IP, gateway, RSSI, heap, firmware version, last seen time. |
| `commands` | Command id, device id, type, status, created time, delivered time, completed/failed time. |
| `network_client_reports` | Report id, watchdog device id, received time. |
| `network_clients` | Report id, IP, MAC, hostname, vendor. |

Recommended command statuses:

```text
PENDING -> DELIVERED -> COMPLETED
PENDING -> DELIVERED -> FAILED
```

Mark a command as `DELIVERED` when the heartbeat endpoint returns it to the device. Mark it as `COMPLETED` or `FAILED` when `/api/v1/command-results` arrives. If a delivered command does not receive a result within your expected timeout window, mark it as stale or failed on the backend side.

## Deployment Notes

- For HTTPS deployments, configure `ROUTER_WATCHDOG_BACKEND_SECURE true`.
- `ROUTER_WATCHDOG_BACKEND_INSECURE_TLS true` skips certificate validation. It is convenient during MVP work, but it should be disabled once certificate validation is implemented and tested.
- For local HTTP testing, set `ROUTER_WATCHDOG_BACKEND_SECURE false`.
- Keep request bodies small and response bodies smaller. The firmware only needs the optional `command` object from heartbeat responses.
