#include "RouterWatchdog.h"

#include <Arduino.h>

#include "../backend/BackendClient.h"
#include "../config/AppConfig.h"
#include "../drivers/Relay.h"
#include "../network/NetworkManager.h"

namespace {

uint8_t consecutive_failures = 0;
bool last_wifi_connected = false;
bool last_internet_connected = false;
bool have_last_status = false;

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
    Serial.println(consecutive_failures);
}

void runRecoveryCycle()
{
    Serial.println("[WATCHDOG] Recovery triggered");

    Serial.println("[RECOVERY] relay ON, router power removed");
    Relay::turnOn();
    delay(AppConfig::ROUTER_POWER_OFF_TIME_MS);

    Serial.println("[RECOVERY] relay OFF, router power restored");
    Relay::turnOff();

    Serial.print("[RECOVERY] waiting for router boot (");
    Serial.print(AppConfig::ROUTER_BOOT_WAIT_TIME_MS);
    Serial.println(" ms)");
    delay(AppConfig::ROUTER_BOOT_WAIT_TIME_MS);

    Serial.print("[RECOVERY] cooldown active (");
    Serial.print(AppConfig::RECOVERY_COOLDOWN_MS);
    Serial.println(" ms)");
    delay(AppConfig::RECOVERY_COOLDOWN_MS);

    Serial.println("[WATCHDOG] Monitoring resumed");
}

void updateFailureCount(bool check_failed)
{
    if (check_failed) {
        consecutive_failures++;
        Serial.print("[WATCHDOG] Failure count ");
        Serial.print(consecutive_failures);
        Serial.print("/");
        Serial.println(AppConfig::MAX_CONSECUTIVE_FAILURES);
    } else if (consecutive_failures > 0) {
        consecutive_failures = 0;
        Serial.println("[WATCHDOG] Failure count cleared");
    }
}

bool shouldLogStatus(const NetworkStatus &status, bool check_failed)
{
    return !have_last_status ||
        status.wifi_connected != last_wifi_connected ||
        status.internet_connected != last_internet_connected ||
        check_failed;
}

void rememberStatus(const NetworkStatus &status)
{
    last_wifi_connected = status.wifi_connected;
    last_internet_connected = status.internet_connected;
    have_last_status = true;
}

}

namespace RouterWatchdog {

void begin()
{
    Serial.println("[WATCHDOG] Task started");
    Serial.println("[WATCHDOG] Monitoring started");
}

void tick()
{
    NetworkStatus status = NetworkManager::checkStatus();
    bool check_failed = !status.wifi_connected || !status.internet_connected;

    updateFailureCount(check_failed);

    if (shouldLogStatus(status, check_failed)) {
        logNetworkStatus(status);
    }

    rememberStatus(status);
    BackendClient::sendHeartbeat(status, consecutive_failures);

    if (consecutive_failures >= AppConfig::MAX_CONSECUTIVE_FAILURES) {
        runRecoveryCycle();
        consecutive_failures = 0;
        have_last_status = false;
    }
}

}
