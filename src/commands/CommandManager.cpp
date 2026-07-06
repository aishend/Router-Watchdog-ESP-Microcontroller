#include "CommandManager.h"

#include <Arduino.h>

#include "CommandExecutionState.h"
#include "CommandHandler.h"
#include "CommandResult.h"
#include "../backend/BackendClient.h"

namespace
{
    PendingCommand currentCommand;
    bool commandRunning = false;

    void sendResult(CommandResultStatus status)
    {
        if (!currentCommand.has_command)
        {
            return;
        }

        CommandResult result;
        result.command_id = currentCommand.id;
        result.status = status;

        bool sent = BackendClient::sendCommandResult(result);

        if (sent)
        {
            Serial.println("[COMMAND_MANAGER] Command result sent");
            currentCommand = PendingCommand{};
            commandRunning = false;
        }
        else
        {
            Serial.println("[COMMAND_MANAGER] Command result send failed");
        }
    }
}

namespace CommandManager
{

    void begin()
    {
        Serial.println("[COMMAND_MANAGER] Initialized");
    }

    void handleCommand(const PendingCommand &command)
    {
        if (commandRunning)
        {
            Serial.println("[COMMAND_MANAGER] Command ignored, another command is running");
            return;
        }

        currentCommand = command;
        commandRunning = true;

        CommandExecutionState state =
            CommandHandler::execute(command);

        switch (state)
        {
        case CommandExecutionState::Completed:
            sendResult(CommandResultStatus::Completed);
            break;

        case CommandExecutionState::Failed:
            sendResult(CommandResultStatus::Failed);
            break;

        case CommandExecutionState::Running:
            Serial.println("[COMMAND_MANAGER] Command is running");
            break;
        }
    }

    void tick(unsigned long)
    {
    }

    void notifyRouterRecoveryFinished()
    {
        Serial.println("[COMMAND_MANAGER] notifyRouterRecoveryFinished called");

        if (!commandRunning)
        {
            Serial.println("[COMMAND_MANAGER] No command running");
            return;
        }

        Serial.print("[COMMAND_MANAGER] Current command id=");
        Serial.println(currentCommand.id);

        Serial.print("[COMMAND_MANAGER] Current command type=");
        Serial.println(static_cast<int>(currentCommand.type));

        if (currentCommand.type != CommandType::RebootRouter)
        {
            Serial.println("[COMMAND_MANAGER] Current command is not RebootRouter");
            return;
        }

        Serial.println("[COMMAND_MANAGER] Router recovery finished");
        sendResult(CommandResultStatus::Completed);
    }

}