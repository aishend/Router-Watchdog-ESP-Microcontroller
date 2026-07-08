#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "../commands/CommandResult.h"
#include "../network/NetworkStatus.h"

namespace MqttClient
{
    void begin();
    void tick(unsigned long now);
    bool publishHeartbeat(const NetworkStatus &status, uint8_t failures);
    bool publishCommandResult(const CommandResult &result);
}

#endif
