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
        if (strcmp(type, "RUN_NETWORK_TEST") == 0)
        {
            return CommandType::RunNetworkTest;
        }

        return CommandType::None;
    }

    const char *commandResultStatusToString(CommandResultStatus status)
    {
        switch (status)
        {
        case CommandResultStatus::Started:
            return "STARTED";

        case CommandResultStatus::Rejected:
            return "REJECTED";

        default:
            return "REJECTED";
        }
    }
}

namespace MqttPayloads
{
    bool parseFirmwareUpdate(const byte *payload, unsigned int length, FirmwareUpdateRequest &request)
    {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        if (error)
        {
            Serial.printf("[MQTT] WARN Invalid firmware JSON: %s\n", error.c_str());
            return false;
        }

        const char *version = doc["version"] | "";
        const char *url = doc["url"] | "";
        const char *sha256 = doc["sha256"] | "";
        if (version[0] == '\0' || url[0] == '\0' || strlen(sha256) != 64)
        {
            Serial.println("[MQTT] WARN Firmware request requires version, url and 64-character sha256");
            return false;
        }

        request.pending = true;
        request.version = version;
        request.url = url;
        request.sha256 = sha256;
        return true;
    }

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

        command.has_command = true;
        command.id = commandId;
        command.type = parseCommandType(commandType);

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
        if (result.reason.length() > 0)
        {
            doc["reason"] = result.reason;
        }

        String json;
        serializeJson(doc, json);
        return json;
    }

    String buildFirmwareStatus(const String &targetVersion, const char *status, const String &error)
    {
        JsonDocument doc;
        doc["currentVersion"] = AppConfig::FIRMWARE_VERSION;
        doc["targetVersion"] = targetVersion;
        doc["status"] = status;
        if (error.length() > 0)
        {
            doc["error"] = error;
        }

        String json;
        serializeJson(doc, json);
        return json;
    }
}
