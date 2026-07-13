#ifndef COMMAND_H
#define COMMAND_H

#include <Arduino.h>

enum class CommandType
{
    None,
    RebootRouter,
    RebootDevice,
    RunNetworkTest
};

struct PendingCommand
{
    bool has_command = false;
    String id;
    CommandType type = CommandType::None;
};

#endif
