#ifndef BACKEND_CLIENT_H
#define BACKEND_CLIENT_H

#include <stdint.h>
#include "../commands/Command.h"
#include "../commands/CommandResult.h"
#include "../network/NetworkStatus.h"
#include "../network/DiscoveredClient.h"

namespace BackendClient
{
    void begin();
    HeartbeatResponse sendHeartbeat(const NetworkStatus &status, uint8_t failures);
    bool sendCommandResult(const CommandResult &result);
    bool sendNetworkClientsReport(const DiscoveredClient *clients, uint8_t count);
}

#endif
