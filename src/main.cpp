#include <Arduino.h>

#include "backend/BackendClient.h"
#include "config/AppConfig.h"
#include "drivers/Relay.h"
#include "network/NetworkManager.h"
#include "watchdog/RouterWatchdog.h"

namespace {

unsigned long last_check_ms = 0;

bool startupGracePeriodElapsed(unsigned long now)
{
    return now >= AppConfig::STARTUP_GRACE_PERIOD_MS;
}

bool checkIntervalElapsed(unsigned long now)
{
    return last_check_ms == 0 ||
        now - last_check_ms >= AppConfig::CHECK_INTERVAL_MS;
}

}

void setup()
{
    Serial.begin(AppConfig::SERIAL_BAUD);
    Serial.println();
    Serial.println("[BOOT] Router watchdog starting");

    Relay::begin();
    NetworkManager::begin();
    BackendClient::begin();
    RouterWatchdog::begin();

    Serial.print("[BOOT] Waiting for Wi-Fi startup grace period (");
    Serial.print(AppConfig::STARTUP_GRACE_PERIOD_MS);
    Serial.println(" ms)");
}

void loop()
{
    unsigned long now = millis();

    if (startupGracePeriodElapsed(now) && checkIntervalElapsed(now)) {
        last_check_ms = now;
        RouterWatchdog::tick();
    }

    delay(10);
}
