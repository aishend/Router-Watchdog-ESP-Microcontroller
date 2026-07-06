#ifndef COMMAND_MANAGER_H
#define COMMAND_MANAGER_H

#include "Command.h"
#include "CommandExecutionState.h"

namespace CommandManager
{
    void begin();
    void handleCommand(const PendingCommand &command);
    void tick(unsigned long now);
    void notifyRouterRecoveryFinished();
}

#endif