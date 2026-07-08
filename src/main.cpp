#include <Arduino.h>

#include "commands/CommandManager.h"
#include "config/AppConfig.h"
#include "drivers/Relay.h"
#include "mqtt/MqttClient.h"
#include "network/NetworkManager.h"
#include "watchdog/RouterWatchdog.h"

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
        Serial.println("[BOOT] WARN placeholder config detected, check AppSecrets.h");
    }

    Relay::begin();
    NetworkManager::begin();
    MqttClient::begin();
    RouterWatchdog::begin();
    CommandManager::begin();

    Serial.printf("[BOOT] Waiting for Wi-Fi startup grace period (%lu ms)\n", AppConfig::STARTUP_GRACE_PERIOD_MS);
}

void loop()
{
    unsigned long now = millis();

    MqttClient::tick(now);

    if (startupGracePeriodElapsed(now))
    {
        RouterWatchdog::tick(now);
        CommandManager::tick(now);
    }

    delay(10);
}
