# Router Watchdog ESP8266

Firmware for an ESP8266 that monitors Wi-Fi and Internet access, reports health over
MQTT/TLS, power-cycles a router through a relay, accepts remote commands, and installs
verified OTA releases.

## Safety First

The ESP8266 and relay must be powered independently from the router. Otherwise the
controller turns itself off and cannot restore router power.

Relay polarity is deliberately required in the local `src/config/AppSecrets.h`:

```cpp
#define ROUTER_WATCHDOG_RELAY_ACTIVE_LOW true
```

- Use `true` only when the relay activates with LOW.
- Use `false` only when it activates with HIGH.
- Test first with the router disconnected, using the relay LED or a multimeter.
- Add the appropriate physical pull-up/pull-down so the relay remains inactive while
  the ESP pin is high impedance during boot.

An incorrect value can cut router power during startup. The driver writes the inactive
level before changing the GPIO to OUTPUT, then confirms that level again.

## Architecture

```text
src/
  commands/      MQTT command lifecycle and bounded publication retries
  config/        Build-time configuration and local secrets example
  drivers/       Fail-safe relay driver
  mqtt/          MQTT/TLS transport, topics and JSON payloads
  network/       Incremental round-robin connectivity probes
  operations/    Exclusive destructive-operation coordinator
  ota/           HTTPS download, size and SHA-256 verification
  watchdog/      Recovery state machine and retry policy
```

Only one destructive operation can exist: router recovery, pending device reboot, or
firmware update. Relay timers run before network work in every loop.

## Configuration

Copy the example and fill in local values:

```bash
cp src/config/AppSecrets.example.h src/config/AppSecrets.h
```

Important settings:

| Setting | Default | Purpose |
| --- | ---: | --- |
| `MQTT_PORT` | `3543` | MQTT over TLS listener. |
| `MQTT_CA_CERT` | empty | Public private-CA certificate used to validate MQTT and OTA servers. |
| `INTERNET_TEST_PORT` | `53` | TCP connectivity probe against configured DNS services. |
| `INTERNET_TEST_TIMEOUT_MS` | `750` | Maximum blocking time for one target. |
| `MAX_CONSECUTIVE_FAILURES` | profile | Failures before immediate recovery. |
| `FAST_RECOVERY_ATTEMPTS` | `6` | Number of attempts in the fast phase. |
| `FAST_RECOVERY_INTERVAL_MS` | 20 minutes | Observation window between attempts 1–6. |
| `SLOW_RECOVERY_INTERVAL_MS` | 4 hours | Observation window after attempt 6. |
| `ROUTER_POWER_OFF_TIME_MS` | profile | Exact router power-off duration. |

The Internet probe tests one target per loop iteration in round-robin order. A cycle
succeeds on the first connection and fails only after all three targets fail. MQTT
failure alone never counts as Internet failure and never triggers router recovery.

## Recovery Policy

The states are `Monitoring`, `PowerOff`, and `RecoveryCooldown`.

1. At the failure threshold, attempt 1 starts immediately.
2. The relay cuts power for `ROUTER_POWER_OFF_TIME_MS`.
3. Power is restored and network checks continue throughout the cooldown.
4. Recovery during the window resets failures and attempts immediately.
5. Attempts 2–6 start no sooner than 20 minutes after the preceding power restore.
6. After attempt 6, retries continue every four hours indefinitely.

The fast-attempt counter saturates at six and is held only in RAM. Manual router or
device reboot commands and OTA are rejected/deferred during recovery and cooldown.

## MQTT/TLS Infrastructure

The broker must use:

```conf
listener 3543
allow_anonymous false
password_file /etc/mosquitto/passwd
acl_file /etc/mosquitto/acl
cafile /etc/mosquitto/ca.crt
certfile /etc/mosquitto/server.crt
keyfile /etc/mosquitto/server.key
```

Create a private CA and issue a server certificate whose Subject Alternative Name
contains the stable public HQ IP. Store only the public CA certificate in firmware;
the CA and server private keys stay on secured infrastructure. The ESP synchronizes
time through NTP before TLS validation and fails closed if time or CA is unavailable.
If the public IP changes, reissue the certificate and update configuration.

Example device ACL (replace the device ID):

```conf
user router-watchdog-001
topic write router-watchdog/router-watchdog-001/heartbeat
topic write router-watchdog/router-watchdog-001/availability
topic write router-watchdog/router-watchdog-001/command-results
topic write router-watchdog/router-watchdog-001/firmware/status
topic read  router-watchdog/router-watchdog-001/commands
topic read  router-watchdog/router-watchdog-001/firmware/desired
```

The Last Will remains retained `offline`. A failed retained `online` publication is
retried every five seconds, without a tight loop. See [docs/mqtt.md](docs/mqtt.md) for
the complete contract and rejection reasons.

## OTA

Publish retained desired state:

```json
{
  "version": "0.2.0",
  "url": "https://203.0.113.10/firmware/router-watchdog-0.2.0.bin",
  "sha256": "64-hexadecimal-characters"
}
```

HTTPS, certificate validation, known Internet access, inactive relay, idle operation,
content length, available flash, and SHA-256 are mandatory. Status progresses through
`ACCEPTED` and `DOWNLOADING`, or `FAILED`. Success is the next heartbeat containing
the new `firmwareVersion`. The first OTA-capable build must be installed by USB.

Generate the digest with:

```bash
sha256sum .pio/build/esp12e/firmware.bin
```

## Reproducible Build

Platform and library versions are pinned in `platformio.ini`. Override the fallback
release version from a CI/build environment with
`ROUTER_WATCHDOG_FIRMWARE_VERSION`.

```bash
pio run
```

The output is `.pio/build/esp12e/firmware.bin`.

## Physical Acceptance Tests

Before deployment, verify both relay polarities without the router attached, exact
power-off timing, recovery during cooldown, six 20-minute attempts followed by
four-hour attempts, command rejection during each destructive operation, invalid TLS
certificate rejection, broker ACL isolation, and a real SHA-256-verified OTA cycle.
These electrical, timing, broker, and certificate checks cannot be proven by compilation.

## Hourly Network Quality Test

Configure HTTPS endpoints in `AppSecrets.h`:

```cpp
#define ROUTER_WATCHDOG_SPEEDTEST_DOWNLOAD_URL "https://203.0.113.10/speedtest/download.bin"
#define ROUTER_WATCHDOG_SPEEDTEST_UPLOAD_URL "https://203.0.113.10/speedtest/upload"
```

The automatic test runs every 60–65 minutes, only when Internet is known available
and no recovery, reboot, OTA, or other test is active. It downloads up to 100 KB and
uploads a generated 50 KB stream. Results are published to `network-test/results`.
It can also be requested with a non-retained `RUN_NETWORK_TEST` command. Both HTTPS
endpoints must present certificates issued by the CA configured in the firmware.
