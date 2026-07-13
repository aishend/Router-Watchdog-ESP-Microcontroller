#ifndef MQTT_PAYLOADS_H
#define MQTT_PAYLOADS_H

#include <Arduino.h>

#include "../commands/Command.h"
#include "../commands/CommandResult.h"
#include "../network/NetworkStatus.h"
#include "../ota/FirmwareUpdateRequest.h"

namespace MqttPayloads
{
    bool parseCommand(const byte *payload, unsigned int length, PendingCommand &command);
    bool parseFirmwareUpdate(const byte *payload, unsigned int length, FirmwareUpdateRequest &request);
    String buildHeartbeat(const NetworkStatus &status, uint8_t failures);
    String buildCommandResult(const CommandResult &result);
    String buildFirmwareStatus(const String &targetVersion, const char *status, const String &error);
}

#endif
