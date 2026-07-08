#ifndef COMMAND_RESULT_H
#define COMMAND_RESULT_H

#include <Arduino.h>

enum class CommandResultStatus
{
    Started,
    Rejected
};

struct CommandResult
{
    String command_id;
    CommandResultStatus status = CommandResultStatus::Rejected;
};

#endif
