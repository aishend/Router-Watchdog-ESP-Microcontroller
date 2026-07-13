#include "CommandManager.h"

#include <Arduino.h>

#include "../config/AppConfig.h"
#include "../mqtt/MqttClient.h"
#include "../operations/OperationCoordinator.h"
#include "../watchdog/RouterWatchdog.h"

namespace
{
    constexpr uint8_t COMMAND_HISTORY_SIZE = 8;
    constexpr unsigned long DEVICE_REBOOT_DELAY_MS = 500;

    enum class ExecutionState
    {
        Idle,
        PublishingStarted,
        WaitingToExecute,
    };

    PendingCommand activeCommand;
    ExecutionState executionState = ExecutionState::Idle;
    uint8_t publishAttempts = 0;
    unsigned long nextActionMs = 0;
    String commandHistory[COMMAND_HISTORY_SIZE];
    uint8_t historyIndex = 0;

    bool deadlineReached(unsigned long now, unsigned long deadline)
    {
        return static_cast<long>(now - deadline) >= 0;
    }

    bool commandSeen(const String &id)
    {
        for (const String &knownId : commandHistory)
        {
            if (knownId == id) return true;
        }
        return false;
    }

    void rememberCommand(const String &id)
    {
        commandHistory[historyIndex] = id;
        historyIndex = (historyIndex + 1) % COMMAND_HISTORY_SIZE;
    }

    void publishRejection(const PendingCommand &command, const char *reason)
    {
        CommandResult result;
        result.command_id = command.id;
        result.status = CommandResultStatus::Rejected;
        result.reason = reason;
        MqttClient::publishCommandResult(result);
        Serial.printf("[COMMAND] Rejected id=%s reason=%s\n", command.id.c_str(), reason);
    }

    const char *operationConflictReason()
    {
        switch (OperationCoordinator::current())
        {
        case DestructiveOperation::RouterRecovery:
            return RouterWatchdog::isCooldownActive() ? "retry_cooldown_active" : "router_recovery_in_progress";
        case DestructiveOperation::DeviceRebootPending: return "device_reboot_pending";
        case DestructiveOperation::FirmwareUpdate: return "firmware_update_in_progress";
        case DestructiveOperation::Idle:
        default: return "operation_in_progress";
        }
    }

    DestructiveOperation operationFor(CommandType type)
    {
        return type == CommandType::RebootRouter
                   ? DestructiveOperation::RouterRecovery
                   : DestructiveOperation::DeviceRebootPending;
    }

    void finishStartedPublication(unsigned long now, bool confirmed)
    {
        if (!confirmed)
        {
            Serial.printf("[COMMAND] WARN Executing id=%s without MQTT STARTED confirmation\n",
                          activeCommand.id.c_str());
        }

        executionState = ExecutionState::WaitingToExecute;
        nextActionMs = now + (activeCommand.type == CommandType::RebootRouter
                                  ? AppConfig::COMMAND_STARTED_TO_RELAY_DELAY_MS
                                  : DEVICE_REBOOT_DELAY_MS);
    }

    void tryPublishStarted(unsigned long now)
    {
        CommandResult result;
        result.command_id = activeCommand.id;
        result.status = CommandResultStatus::Started;
        publishAttempts++;

        if (MqttClient::publishCommandResult(result))
        {
            finishStartedPublication(now, true);
            return;
        }

        if (publishAttempts >= AppConfig::COMMAND_RESULT_MAX_ATTEMPTS)
        {
            finishStartedPublication(now, false);
            return;
        }

        nextActionMs = now + AppConfig::COMMAND_RESULT_RETRY_INTERVAL_MS;
    }

    void executeAcceptedCommand()
    {
        if (activeCommand.type == CommandType::RebootRouter)
        {
            if (!RouterWatchdog::requestRouterReboot())
            {
                Serial.println("[COMMAND] WARN Reserved router recovery could not start");
                OperationCoordinator::finish(DestructiveOperation::RouterRecovery);
            }
            activeCommand = PendingCommand();
            executionState = ExecutionState::Idle;
            return;
        }

        Serial.println("[COMMAND] Rebooting device");
        ESP.restart();
    }
}

namespace CommandManager
{
    void begin()
    {
        activeCommand = PendingCommand();
        executionState = ExecutionState::Idle;
        historyIndex = 0;
        for (String &id : commandHistory) id = "";
        Serial.println("[COMMAND] Manager initialized");
    }

    void handleCommand(const PendingCommand &command)
    {
        if (!command.has_command) return;

        if (commandSeen(command.id))
        {
            publishRejection(command, "duplicate_command_id");
            return;
        }
        rememberCommand(command.id);

        if (command.type == CommandType::None)
        {
            publishRejection(command, "unknown_type");
            return;
        }

        if (!OperationCoordinator::isIdle() || executionState != ExecutionState::Idle)
        {
            publishRejection(command, operationConflictReason());
            return;
        }

        if (!OperationCoordinator::tryStart(operationFor(command.type)))
        {
            publishRejection(command, "operation_in_progress");
            return;
        }

        activeCommand = command;
        executionState = ExecutionState::PublishingStarted;
        publishAttempts = 0;
        nextActionMs = millis();
        Serial.printf("[COMMAND] Accepted id=%s\n", command.id.c_str());
    }

    void tick(unsigned long now)
    {
        if (executionState == ExecutionState::Idle || !deadlineReached(now, nextActionMs)) return;

        if (executionState == ExecutionState::PublishingStarted)
        {
            tryPublishStarted(now);
            return;
        }

        executeAcceptedCommand();
    }

    bool hasRouterRebootPendingOrInProgress()
    {
        return OperationCoordinator::isActive(DestructiveOperation::RouterRecovery);
    }
}
