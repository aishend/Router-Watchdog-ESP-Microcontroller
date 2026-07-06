#include "CommandHandler.h"

#include <Arduino.h>

#include "../drivers/Relay.h"
#include "../config/AppConfig.h"
#include "../watchdog/RouterWatchdog.h"
namespace CommandHandler
{

    CommandExecutionState execute(const PendingCommand &command)
    {
        switch (command.type)
        {
        case CommandType::RebootRouter:
            Serial.println("[COMMAND] REBOOT_ROUTER requested");
            RouterWatchdog::requestRecovery(millis());
            return CommandExecutionState::Running;

        case CommandType::RebootDevice:
            Serial.println("[COMMAND] REBOOT_DEVICE requested");
            ESP.restart();
            return CommandExecutionState::Running;

        case CommandType::None:
        default:
            return CommandExecutionState::Failed;
        }
    }

}