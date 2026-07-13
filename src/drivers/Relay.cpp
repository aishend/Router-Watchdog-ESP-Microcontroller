#include "Relay.h"

#include <Arduino.h>

#include "../config/AppConfig.h"

namespace
{
    bool relayActive = false;

    void writeRelay(bool active)
    {
        bool outputHigh = AppConfig::RELAY_ACTIVE_LOW ? !active : active;
        digitalWrite(AppConfig::RELAY_GPIO, outputHigh ? HIGH : LOW);
        relayActive = active;
    }

}

namespace Relay
{

    void begin()
    {
        Serial.println("[RELAY] Initializing");

        const uint8_t inactiveLevel = AppConfig::RELAY_ACTIVE_LOW ? HIGH : LOW;
        digitalWrite(AppConfig::RELAY_GPIO, inactiveLevel);
        pinMode(AppConfig::RELAY_GPIO, OUTPUT);
        digitalWrite(AppConfig::RELAY_GPIO, inactiveLevel);
        relayActive = false;

        Serial.println("[RELAY] OFF");
    }

    void turnOn()
    {
        writeRelay(true);
    }

    void turnOff()
    {
        writeRelay(false);
    }

    bool isActive()
    {
        return relayActive;
    }

}
