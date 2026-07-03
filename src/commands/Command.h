#ifndef COMMAND_H
#define COMMAND_H

enum class CommandType {
    None,
    RebootRouter,
    RebootDevice
};

struct HeartbeatResponse {
    bool accepted = false;
    CommandType command = CommandType::None;
};


#endif