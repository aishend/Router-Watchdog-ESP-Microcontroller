#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include "Command.h"
#include "CommandExecutionState.h"
namespace CommandHandler
{
    CommandExecutionState execute(const PendingCommand &command);
}

#endif