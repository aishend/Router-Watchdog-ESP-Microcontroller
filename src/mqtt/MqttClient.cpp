#include "MqttClient.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "../commands/Command.h"
#include "../commands/CommandManager.h"
#include "../config/AppConfig.h"

namespace
{
    WiFiClient wifiClient;
    PubSubClient mqttClient(wifiClient);

    unsigned long lastReconnectAttemptMs = 0;

    String topic(const char *suffix)
    {
        String value = String(AppConfig::MQTT_TOPIC_PREFIX);
        value += "/";
        value += AppConfig::DEVICE_ID;
        value += "/";
        value += suffix;
        return value;
    }

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

    const char *commandResultStatusToString(CommandResultStatus status)
    {
        switch (status)
        {
        case CommandResultStatus::Completed:
            return "COMPLETED";

        case CommandResultStatus::Failed:
            return "FAILED";

        default:
            return "FAILED";
        }
    }

    void handleMessage(char *topicName, byte *payload, unsigned int length)
    {
        String commandsTopic = topic("commands");

        if (commandsTopic != topicName)
        {
            return;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload, length);

        if (error)
        {
            Serial.printf("[MQTT] WARN Invalid command JSON: %s\n", error.c_str());
            return;
        }

        const char *commandId = doc["id"] | "";
        const char *commandType = doc["type"] | "NONE";

        if (commandId[0] == '\0')
        {
            Serial.println("[MQTT] WARN Command ignored, missing id");
            return;
        }

        PendingCommand command;
        command.has_command = true;
        command.id = commandId;
        command.type = parseCommandType(commandType);

        Serial.printf("[MQTT] Command received id=%s type=%s\n", command.id.c_str(), commandType);
        CommandManager::handleCommand(command);
    }

    bool publishJson(const char *suffix, const String &payload, bool retained = false)
    {
        if (!mqttClient.connected())
        {
            Serial.printf("[MQTT] WARN Publish skipped, not connected topic=%s\n", suffix);
            return false;
        }

        String fullTopic = topic(suffix);
        bool published = mqttClient.publish(fullTopic.c_str(), payload.c_str(), retained);

        if (!published)
        {
            Serial.printf("[MQTT] WARN Publish failed topic=%s\n", fullTopic.c_str());
        }

        return published;
    }

    String buildHeartbeatJson(const NetworkStatus &status, uint8_t failures)
    {
        JsonDocument doc;

        doc["deviceId"] = AppConfig::DEVICE_ID;
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

    String buildCommandResultJson(const CommandResult &result)
    {
        JsonDocument doc;

        doc["commandId"] = result.command_id;
        doc["status"] = commandResultStatusToString(result.status);

        String json;
        serializeJson(doc, json);
        return json;
    }

    bool reconnect()
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            return false;
        }

        String clientId = String(AppConfig::DEVICE_ID);
        String availabilityTopic = topic("availability");

        Serial.printf("[MQTT] Connecting to %s:%u\n", AppConfig::MQTT_HOST, AppConfig::MQTT_PORT);

        bool connected;
        if (strlen(AppConfig::MQTT_USER) > 0)
        {
            connected = mqttClient.connect(
                clientId.c_str(),
                AppConfig::MQTT_USER,
                AppConfig::MQTT_PASSWORD,
                availabilityTopic.c_str(),
                0,
                true,
                "offline");
        }
        else
        {
            connected = mqttClient.connect(
                clientId.c_str(),
                availabilityTopic.c_str(),
                0,
                true,
                "offline");
        }

        if (!connected)
        {
            Serial.printf("[MQTT] WARN Connect failed state=%d\n", mqttClient.state());
            return false;
        }

        Serial.println("[MQTT] Connected");
        mqttClient.publish(availabilityTopic.c_str(), "online", true);
        mqttClient.subscribe(topic("commands").c_str());
        return true;
    }
}

namespace MqttClient
{
    void begin()
    {
        mqttClient.setServer(AppConfig::MQTT_HOST, AppConfig::MQTT_PORT);
        mqttClient.setCallback(handleMessage);
        mqttClient.setBufferSize(AppConfig::MQTT_BUFFER_SIZE);

        Serial.println("[MQTT] Client initialized");
    }

    void tick(unsigned long now)
    {
        if (!mqttClient.connected())
        {
            if (lastReconnectAttemptMs == 0 ||
                now - lastReconnectAttemptMs >= AppConfig::MQTT_RECONNECT_INTERVAL_MS)
            {
                lastReconnectAttemptMs = now;
                reconnect();
            }

            return;
        }

        mqttClient.loop();
    }

    bool publishHeartbeat(const NetworkStatus &status, uint8_t failures)
    {
        return publishJson("heartbeat", buildHeartbeatJson(status, failures));
    }

    bool publishCommandResult(const CommandResult &result)
    {
        if (result.command_id.length() == 0)
        {
            Serial.println("[MQTT] WARN Command result skipped, missing command id");
            return false;
        }

        return publishJson("command-results", buildCommandResultJson(result));
    }
}
