#ifndef COMMAND_H
#define COMMAND_H

#include <Arduino.h>

enum class CommandType
{
    None,
    RebootRouter,
    RebootDevice
};

struct PendingCommand
{
    bool has_command = false;
    String id;
    CommandType type = CommandType::None;
};

struct HeartbeatResponse
{
    bool accepted = false;
    PendingCommand command;
};

#endif
