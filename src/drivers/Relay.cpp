#include "Relay.h"

#include <Arduino.h>

#include "../config/AppConfig.h"
#include "../logging/Logger.h"

namespace
{

    void writeRelay(bool active)
    {
        bool outputHigh = AppConfig::RELAY_ACTIVE_LOW ? !active : active;
        digitalWrite(AppConfig::RELAY_GPIO, outputHigh ? HIGH : LOW);
    }

}

namespace Relay
{

    void begin()
    {
        Log::info("RELAY", "Initializing");

        pinMode(AppConfig::RELAY_GPIO, OUTPUT);
        writeRelay(false);

        Log::info("RELAY", "OFF");
    }

    void turnOn()
    {
        writeRelay(true);
    }

    void turnOff()
    {
        writeRelay(false);
    }

}
