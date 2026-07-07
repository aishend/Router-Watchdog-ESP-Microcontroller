#include "CommandManager.h"

#include <Arduino.h>

#include "../backend/BackendClient.h"
#include "../logging/Logger.h"
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

        bool sent = BackendClient::sendCommandResult(result);

        if (!sent)
        {
            Log::warn("COMMAND_MANAGER", "Command result delivery failed");
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

        Log::info("COMMAND_MANAGER", "Initialized");
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
                Log::warn("COMMAND_MANAGER", "Router command rejected, already in progress");
                sendCommandResult(command, CommandResultStatus::Failed);
                return;
            }

            Log::info("COMMAND_MANAGER", "Reboot router command accepted");

            pendingRouterCommand = command;
            routerCommandInProgress = true;

            if (!RouterWatchdog::requestRouterReboot())
            {
                Log::warn("COMMAND_MANAGER", "Router reboot command failed to start");
                routerCommandInProgress = false;
                sendCommandResult(command, CommandResultStatus::Failed);
            }

            return;

        case CommandType::RebootDevice:
            Log::info("COMMAND_MANAGER", "Reboot device command received");
            sendCommandResult(command, CommandResultStatus::Completed);
            delay(500);
            ESP.restart();
            return;

        case CommandType::None:
        default:
            Log::warn("COMMAND_MANAGER", "Unknown command received");
            sendCommandResult(command, CommandResultStatus::Failed);
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

        Log::info("COMMAND_MANAGER", "Router command completed");

        routerCommandInProgress = false;
        sendCommandResult(pendingRouterCommand, CommandResultStatus::Completed);
        pendingRouterCommand = PendingCommand();
    }
}
