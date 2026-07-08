#include "CommandManager.h"

#include <Arduino.h>

#include "../mqtt/MqttClient.h"
#include "../watchdog/RouterWatchdog.h"
#include "CommandResult.h"

namespace
{
    PendingCommand pendingRouterCommand;
    bool routerCommandInProgress = false;

    CommandResult makeCommandResult(const PendingCommand &command, CommandResultStatus status)
    {
        CommandResult result;
        result.command_id = command.id;
        result.status = status;
        return result;
    }

    bool sendCommandResult(const PendingCommand &command, CommandResultStatus status)
    {
        CommandResult result = makeCommandResult(command, status);

        bool sent = MqttClient::publishCommandResult(result);

        if (!sent)
        {
            Serial.println("[COMMAND_MANAGER] WARN Command result delivery failed");
        }

        return sent;
    }
}

namespace CommandManager
{
    void begin()
    {
        routerCommandInProgress = false;
        pendingRouterCommand = PendingCommand();

        Serial.println("[COMMAND_MANAGER] Initialized");
    }

    void handleCommand(const PendingCommand &command)
    {
        if (!command.has_command)
        {
            return;
        }

        switch (command.type)
        {
        case CommandType::RebootRouter:
            if (routerCommandInProgress)
            {
                Serial.println("[COMMAND_MANAGER] WARN Router command rejected, already in progress");
                sendCommandResult(command, CommandResultStatus::Failed);
                return;
            }

            Serial.println("[COMMAND_MANAGER] Reboot router command accepted");

            pendingRouterCommand = command;
            routerCommandInProgress = true;

            if (!RouterWatchdog::requestRouterReboot())
            {
                Serial.println("[COMMAND_MANAGER] WARN Router reboot command failed to start");
                routerCommandInProgress = false;
                sendCommandResult(command, CommandResultStatus::Failed);
            }
            else
            {
                sendCommandResult(command, CommandResultStatus::Started);
            }

            return;

        case CommandType::RebootDevice:
            Serial.println("[COMMAND_MANAGER] Reboot device command received");
            sendCommandResult(command, CommandResultStatus::Started);
            delay(500);
            ESP.restart();
            return;

        case CommandType::None:
        default:
            Serial.println("[COMMAND_MANAGER] WARN Unknown command ignored");
            return;
        }
    }

    void tick(unsigned long)
    {
    }

    void notifyRouterRecoveryFinished()
    {
        if (!routerCommandInProgress)
        {
            return;
        }

        Serial.println("[COMMAND_MANAGER] Router command completed");

        routerCommandInProgress = false;
        sendCommandResult(pendingRouterCommand, CommandResultStatus::Completed);
        pendingRouterCommand = PendingCommand();
    }
}
