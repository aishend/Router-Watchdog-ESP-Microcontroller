#include "MqttPayloads.h"

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

#include "../config/AppConfig.h"

namespace
{
    uint32_t heartbeatSequence = 0;

    CommandType parseCommandType(const char *type)
    {
        if (strcmp(type, "REBOOT_ROUTER") == 0)
        {
            return CommandType::RebootRouter;
        }

        if (strcmp(type, "REBOOT_DEVICE") == 0)
        {
            return CommandType::RebootDevice;
        }

        return CommandType::None;
    }

    bool commandExpired(const JsonDocument &doc)
    {
        if (doc["expiresInSeconds"].is<int32_t>() &&
            doc["expiresInSeconds"].as<int32_t>() <= 0)
        {
            return true;
        }

        if (doc["expiresAt"].is<unsigned long>())
        {
            unsigned long nowSeconds = millis() / 1000UL;
            unsigned long expiresAt = doc["expiresAt"].as<unsigned long>();
            return expiresAt <= nowSeconds;
        }

        return false;
    }

    const char *commandResultStatusToString(CommandResultStatus status)
    {
        switch (status)
        {
        case CommandResultStatus::Started:
            return "STARTED";

        case CommandResultStatus::Completed:
            return "COMPLETED";

        case CommandResultStatus::Failed:
            return "FAILED";

        default:
            return "FAILED";
        }
    }
}

namespace MqttPayloads
{
    bool parseCommand(const byte *payload, unsigned int length, PendingCommand &command)
    {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload, length);

        if (error)
        {
            Serial.printf("[MQTT] WARN Invalid command JSON: %s\n", error.c_str());
            return false;
        }

        const char *commandId = doc["id"] | "";
        const char *commandType = doc["type"] | "NONE";

        if (commandId[0] == '\0')
        {
            Serial.println("[MQTT] WARN Command ignored, missing id");
            return false;
        }

        CommandType parsedType = parseCommandType(commandType);
        if (parsedType == CommandType::None)
        {
            Serial.printf("[MQTT] WARN Unknown command type=%s\n", commandType);
            return false;
        }

        if (commandExpired(doc))
        {
            Serial.printf("[MQTT] WARN Command ignored, expired id=%s\n", commandId);
            return false;
        }

        command.has_command = true;
        command.id = commandId;
        command.type = parsedType;

        return true;
    }

    String buildHeartbeat(const NetworkStatus &status, uint8_t failures)
    {
        JsonDocument doc;

        doc["deviceId"] = AppConfig::DEVICE_ID;
        doc["sequence"] = ++heartbeatSequence;
        doc["ip"] = status.ip_address.toString();
        doc["gateway"] = status.gateway_address.toString();
        doc["wifiConnected"] = status.wifi_connected;
        doc["internetConnected"] = status.internet_connected;
        doc["failures"] = failures;
        doc["uptime"] = millis() / 1000UL;
        doc["rssi"] = WiFi.RSSI();
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["firmwareVersion"] = AppConfig::FIRMWARE_VERSION;

        String json;
        serializeJson(doc, json);
        return json;
    }

    String buildCommandResult(const CommandResult &result)
    {
        JsonDocument doc;

        doc["commandId"] = result.command_id;
        doc["status"] = commandResultStatusToString(result.status);

        String json;
        serializeJson(doc, json);
        return json;
    }
}
