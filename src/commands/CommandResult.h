#ifndef COMMAND_RESULT_H
#define COMMAND_RESULT_H

#include <Arduino.h>

enum class CommandResultStatus
{
    Started,
    Completed,
    Failed
};

struct CommandResult
{
    String command_id;
    CommandResultStatus status = CommandResultStatus::Completed;
};

#endif
