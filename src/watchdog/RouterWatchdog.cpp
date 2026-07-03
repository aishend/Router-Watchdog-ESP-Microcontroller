#include "RouterWatchdog.h"

#include <Arduino.h>

#include "../backend/BackendClient.h"
#include "../config/AppConfig.h"
#include "../drivers/Relay.h"
#include "../network/NetworkManager.h"

namespace {

enum class RecoveryState {
    Monitoring,
    PowerOff,
    RecoveryWait,
};

struct WatchdogState {
    uint8_t consecutive_failures = 0;
    unsigned long last_check_ms = 0;
    unsigned long recovery_until_ms = 0;
    bool last_wifi_connected = false;
    bool last_internet_connected = false;
    bool have_last_status = false;
    RecoveryState recovery_state = RecoveryState::Monitoring;
};

WatchdogState state;

bool hasElapsed(unsigned long now, unsigned long deadline)
{
    return static_cast<long>(now - deadline) >= 0;
}

bool checkIntervalElapsed(unsigned long now)
{
    return state.last_check_ms == 0 ||
        now - state.last_check_ms >= AppConfig::CHECK_INTERVAL_MS;
}

void logNetworkStatus(const NetworkStatus &status)
{
    if (status.wifi_connected) {
        Serial.print("[NETWORK] Wi-Fi connected | IP=");
        Serial.print(status.ip_address);
        Serial.print(" | Gateway=");
        Serial.print(status.gateway_address);
        Serial.print(" | Internet=");
        Serial.print(status.internet_connected ? "OK" : "DOWN");
    } else {
        Serial.print("[NETWORK] Wi-Fi not connected");
    }

    Serial.print(" | Failures=");
    Serial.println(state.consecutive_failures);
}

void startRecovery(unsigned long now)
{
    Serial.println("[WATCHDOG] Recovery triggered");

    Serial.println("[RECOVERY] relay ON, router power removed");
    Relay::turnOn();
    state.recovery_state = RecoveryState::PowerOff;
    state.recovery_until_ms = now + AppConfig::ROUTER_POWER_OFF_TIME_MS;
}

void continueRecovery(unsigned long now)
{
    if (state.recovery_state == RecoveryState::Monitoring ||
        !hasElapsed(now, state.recovery_until_ms)) {
        return;
    }

    if (state.recovery_state == RecoveryState::PowerOff) {
        Serial.println("[RECOVERY] relay OFF, router power restored");
        Relay::turnOff();

        Serial.print("[RECOVERY] waiting for router recovery (");
        Serial.print(AppConfig::ROUTER_RECOVERY_WAIT_TIME_MS);
        Serial.println(" ms)");
        state.recovery_state = RecoveryState::RecoveryWait;
        state.recovery_until_ms = now + AppConfig::ROUTER_RECOVERY_WAIT_TIME_MS;
        return;
    }

    if (state.recovery_state == RecoveryState::RecoveryWait) {
        state.consecutive_failures = 0;
        state.have_last_status = false;
        state.last_check_ms = 0;
        state.recovery_state = RecoveryState::Monitoring;
        Serial.println("[WATCHDOG] Monitoring resumed");
    }
}

void updateFailureCount(bool check_failed)
{
    if (check_failed) {
        state.consecutive_failures++;
        Serial.print("[WATCHDOG] Failure count ");
        Serial.print(state.consecutive_failures);
        Serial.print("/");
        Serial.println(AppConfig::MAX_CONSECUTIVE_FAILURES);
    } else if (state.consecutive_failures > 0) {
        state.consecutive_failures = 0;
        Serial.println("[WATCHDOG] Failure count cleared");
    }
}

bool shouldLogStatus(const NetworkStatus &status, bool check_failed)
{
    return !state.have_last_status ||
        status.wifi_connected != state.last_wifi_connected ||
        status.internet_connected != state.last_internet_connected ||
        check_failed;
}

void rememberStatus(const NetworkStatus &status)
{
    state.last_wifi_connected = status.wifi_connected;
    state.last_internet_connected = status.internet_connected;
    state.have_last_status = true;
}

}

namespace RouterWatchdog {

void begin()
{
    Serial.println("[WATCHDOG] Monitoring started");
}

void tick(unsigned long now)
{
    continueRecovery(now);

    if (state.recovery_state != RecoveryState::Monitoring ||
        !checkIntervalElapsed(now)) {
        return;
    }

    state.last_check_ms = now;

    NetworkStatus status = NetworkManager::checkStatus();
    bool check_failed = !status.wifi_connected || !status.internet_connected;

    updateFailureCount(check_failed);

    if (shouldLogStatus(status, check_failed)) {
        logNetworkStatus(status);
    }

    rememberStatus(status);
    BackendClient::sendHeartbeat(
        status,
        state.consecutive_failures);

    if (state.consecutive_failures >= AppConfig::MAX_CONSECUTIVE_FAILURES) {
        startRecovery(now);
    }
}

}
