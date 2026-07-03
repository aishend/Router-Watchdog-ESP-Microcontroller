#include "CommandHandler.h"

#include <Arduino.h>

#include "../drivers/Relay.h"
#include "../config/AppConfig.h"

namespace CommandHandler
{

    void execute(CommandType command)
    {
        switch (command)
        {
        case CommandType::RebootRouter:
            Serial.println("[COMMAND] Executing REBOOT_ROUTER");
            Relay::turnOn();
            delay(AppConfig::ROUTER_POWER_OFF_TIME_MS);
            Relay::turnOff();
            Serial.println("[COMMAND] Router reboot command finished");
            break;

        case CommandType::RebootDevice:
            Serial.println("[COMMAND] Executing REBOOT_DEVICE");
            delay(200);
            ESP.restart();
            break;

        case CommandType::None:
        default:
            break;
        }
    }

}