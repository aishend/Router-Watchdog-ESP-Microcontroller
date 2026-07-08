#include "CommandManager.h"

#include <Arduino.h>

#include "../mqtt/MqttClient.h"
#include "../watchdog/RouterWatchdog.h"
#include "CommandResult.h"

namespace
{
    constexpr uint8_t COMMAND_HISTORY_SIZE = 8;
    constexpr unsigned long DEVICE_REBOOT_DELAY_MS = 500;

    PendingCommand queuedCommand;
    bool hasQueuedCommand = false;
    bool routerCommandInProgress = false;
    bool deviceRebootPending = false;
    unsigned long deviceRebootAtMs = 0;
    String commandHistory[COMMAND_HISTORY_SIZE];
    uint8_t nextCommandHistoryIndex = 0;

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

    bool commandSeen(const String &commandId)
    {
        for (uint8_t i = 0; i < COMMAND_HISTORY_SIZE; i++)
        {
            if (commandHistory[i] == commandId)
            {
                return true;
            }
        }

        return false;
    }

    void rememberCommand(const String &commandId)
    {
        commandHistory[nextCommandHistoryIndex] = commandId;
        nextCommandHistoryIndex = (nextCommandHistoryIndex + 1) % COMMAND_HISTORY_SIZE;
    }

    bool canQueueCommand()
    {
        return !hasQueuedCommand && !deviceRebootPending;
    }

    void rejectCommand(const PendingCommand &command, const char *reason)
    {
        Serial.printf("[COMMAND_MANAGER] WARN Command rejected id=%s reason=%s\n", command.id.c_str(), reason);
        sendCommandResult(command, CommandResultStatus::Rejected);
    }

    void executeRouterReboot()
    {
        if (!RouterWatchdog::canRequestRouterReboot())
        {
            PendingCommand command = queuedCommand;
            queuedCommand = PendingCommand();
            hasQueuedCommand = false;
            rejectCommand(command, "watchdog_not_ready");
            return;
        }

        if (!sendCommandResult(queuedCommand, CommandResultStatus::Started))
        {
            return;
        }

        routerCommandInProgress = true;
        PendingCommand command = queuedCommand;
        queuedCommand = PendingCommand();
        hasQueuedCommand = false;

        if (!RouterWatchdog::requestRouterReboot())
        {
            Serial.println("[COMMAND_MANAGER] WARN Router reboot command failed to start");
            routerCommandInProgress = false;
            rejectCommand(command, "router_reboot_start_failed");
        }
    }

    void executeDeviceReboot(unsigned long now)
    {
        if (!sendCommandResult(queuedCommand, CommandResultStatus::Started))
        {
            return;
        }

        Serial.println("[COMMAND_MANAGER] Reboot device command started");
        queuedCommand = PendingCommand();
        hasQueuedCommand = false;
        deviceRebootPending = true;
        deviceRebootAtMs = now + DEVICE_REBOOT_DELAY_MS;
    }

    bool hasElapsed(unsigned long now, unsigned long deadline)
    {
        return static_cast<long>(now - deadline) >= 0;
    }
}

namespace CommandManager
{
    void begin()
    {
        queuedCommand = PendingCommand();
        hasQueuedCommand = false;
        routerCommandInProgress = false;
        deviceRebootPending = false;
        nextCommandHistoryIndex = 0;

        for (uint8_t i = 0; i < COMMAND_HISTORY_SIZE; i++)
        {
            commandHistory[i] = "";
        }

        Serial.println("[COMMAND_MANAGER] Initialized");
    }

    void handleCommand(const PendingCommand &command)
    {
        if (!command.has_command)
        {
            return;
        }

        if (commandSeen(command.id))
        {
            rejectCommand(command, "duplicate_command_id");
            return;
        }

        rememberCommand(command.id);

        if (!canQueueCommand())
        {
            rejectCommand(command, "command_already_queued");
            return;
        }

        if (command.type == CommandType::None)
        {
            rejectCommand(command, "unknown_type");
            return;
        }

        if (command.type == CommandType::RebootRouter && routerCommandInProgress)
        {
            rejectCommand(command, "router_reboot_in_progress");
            return;
        }

        queuedCommand = command;
        hasQueuedCommand = true;
        Serial.printf("[COMMAND_MANAGER] Command queued id=%s\n", command.id.c_str());
    }

    void tick(unsigned long now)
    {
        if (deviceRebootPending && hasElapsed(now, deviceRebootAtMs))
        {
            ESP.restart();
        }

        if (!hasQueuedCommand)
        {
            return;
        }

        switch (queuedCommand.type)
        {
        case CommandType::RebootRouter:
            executeRouterReboot();
            return;

        case CommandType::RebootDevice:
            executeDeviceReboot(now);
            return;

        case CommandType::None:
        default:
            rejectCommand(queuedCommand, "unknown_type");
            queuedCommand = PendingCommand();
            hasQueuedCommand = false;
            return;
        }
    }

    void notifyRouterRecoveryFinished()
    {
        routerCommandInProgress = false;
    }
}
