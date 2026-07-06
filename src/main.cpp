#include <Arduino.h>

#include "backend/BackendClient.h"
#include "config/AppConfig.h"
#include "drivers/Relay.h"
#include "network/NetworkManager.h"
#include "watchdog/RouterWatchdog.h"
#include "commands/CommandManager.h"

namespace
{

    bool startupGracePeriodElapsed(unsigned long now)
    {
        return now >= AppConfig::STARTUP_GRACE_PERIOD_MS;
    }

}

void setup()
{
    Serial.begin(AppConfig::SERIAL_BAUD);
    Serial.println();
    Serial.println("[BOOT] Router watchdog starting");

    if (AppConfig::hasPlaceholderSecrets())
    {
        Serial.println("[BOOT] WARNING: placeholder config detected, check AppSecrets.h");
    }

    Relay::begin();
    NetworkManager::begin();
    BackendClient::begin();
    RouterWatchdog::begin();
    CommandManager::begin();

    Serial.print("[BOOT] Waiting for Wi-Fi startup grace period (");
    Serial.print(AppConfig::STARTUP_GRACE_PERIOD_MS);
    Serial.println(" ms)");
}

void loop()
{
    unsigned long now = millis();

    if (startupGracePeriodElapsed(now))
    {
        RouterWatchdog::tick(now);
        CommandManager::tick(now);
    }

    delay(10);
}
