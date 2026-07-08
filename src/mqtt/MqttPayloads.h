#ifndef MQTT_PAYLOADS_H
#define MQTT_PAYLOADS_H

#include <Arduino.h>

#include "../commands/Command.h"
#include "../commands/CommandResult.h"
#include "../network/NetworkStatus.h"

namespace MqttPayloads
{
    bool parseCommand(const byte *payload, unsigned int length, PendingCommand &command);
    String buildHeartbeat(const NetworkStatus &status, uint8_t failures);
    String buildCommandResult(const CommandResult &result);
}

#endif
