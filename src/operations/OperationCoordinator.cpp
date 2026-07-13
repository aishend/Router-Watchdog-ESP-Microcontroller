#include "OperationCoordinator.h"

#include <Arduino.h>

namespace
{
    DestructiveOperation activeOperation = DestructiveOperation::Idle;
}

namespace OperationCoordinator
{
    void begin()
    {
        activeOperation = DestructiveOperation::Idle;
        Serial.println("[OPERATIONS] Coordinator initialized");
    }

    bool tryStart(DestructiveOperation operation)
    {
        if (operation == DestructiveOperation::Idle || activeOperation != DestructiveOperation::Idle)
        {
            return false;
        }

        activeOperation = operation;
        Serial.printf("[OPERATIONS] Started %s\n", name());
        return true;
    }

    void finish(DestructiveOperation operation)
    {
        if (activeOperation != operation)
        {
            return;
        }

        Serial.printf("[OPERATIONS] Finished %s\n", name());
        activeOperation = DestructiveOperation::Idle;
    }

    bool isIdle() { return activeOperation == DestructiveOperation::Idle; }
    bool isActive(DestructiveOperation operation) { return activeOperation == operation; }
    DestructiveOperation current() { return activeOperation; }

    const char *name()
    {
        switch (activeOperation)
        {
        case DestructiveOperation::RouterRecovery: return "router_recovery";
        case DestructiveOperation::DeviceRebootPending: return "device_reboot_pending";
        case DestructiveOperation::FirmwareUpdate: return "firmware_update";
        case DestructiveOperation::NetworkTest: return "network_test";
        case DestructiveOperation::Idle:
        default: return "idle";
        }
    }
}
