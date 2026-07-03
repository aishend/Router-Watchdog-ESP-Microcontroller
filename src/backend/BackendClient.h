#ifndef BACKEND_CLIENT_H
#define BACKEND_CLIENT_H

#include <stdint.h>
#include "../commands/Command.h"

#include "../network/NetworkStatus.h"

namespace BackendClient
{

    void begin();
    HeartbeatResponse sendHeartbeat(const NetworkStatus &status, uint8_t failures);

}

#endif
