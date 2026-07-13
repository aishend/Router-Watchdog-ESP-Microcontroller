#ifndef OPERATION_COORDINATOR_H
#define OPERATION_COORDINATOR_H

enum class DestructiveOperation
{
    Idle,
    RouterRecovery,
    DeviceRebootPending,
    FirmwareUpdate,
};

namespace OperationCoordinator
{
    void begin();
    bool tryStart(DestructiveOperation operation);
    void finish(DestructiveOperation operation);
    bool isIdle();
    bool isActive(DestructiveOperation operation);
    DestructiveOperation current();
    const char *name();
}

#endif
