#include "MqttClient.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "../commands/CommandManager.h"
#include "../config/AppConfig.h"
#include "MqttPayloads.h"
#include "MqttTopics.h"

namespace
{
    WiFiClient wifiClient;
    PubSubClient mqttClient(wifiClient);

    unsigned long lastReconnectAttemptMs = 0;

    void handleMessage(char *topicName, byte *payload, unsigned int length)
    {
        String commandsTopic = MqttTopics::build("commands");

        if (commandsTopic != topicName)
        {
            return;
        }

        PendingCommand command;
        if (!MqttPayloads::parseCommand(payload, length, command))
        {
            return;
        }

        Serial.printf("[MQTT] Command received id=%s\n", command.id.c_str());
        CommandManager::handleCommand(command);
    }

    bool publishJson(const char *suffix, const String &payload, bool retained = false)
    {
        if (!mqttClient.connected())
        {
            Serial.printf("[MQTT] WARN Publish skipped, not connected topic=%s\n", suffix);
            return false;
        }

        String fullTopic = MqttTopics::build(suffix);
        bool published = mqttClient.publish(fullTopic.c_str(), payload.c_str(), retained);

        if (!published)
        {
            Serial.printf("[MQTT] WARN Publish failed topic=%s\n", fullTopic.c_str());
        }

        return published;
    }

    bool reconnect()
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            return false;
        }

        String clientId = String(AppConfig::DEVICE_ID);
        String availabilityTopic = MqttTopics::build("availability");

        Serial.printf("[MQTT] Connecting to %s:%u\n", AppConfig::MQTT_HOST, AppConfig::MQTT_PORT);

        bool connected = mqttClient.connect(
            clientId.c_str(),
            AppConfig::MQTT_USER,
            AppConfig::MQTT_PASSWORD,
            availabilityTopic.c_str(),
            0,
            true,
            "offline");

        if (!connected)
        {
            Serial.printf("[MQTT] WARN Connect failed state=%d\n", mqttClient.state());
            return false;
        }

        Serial.println("[MQTT] Connected");
        if (!mqttClient.publish(availabilityTopic.c_str(), "online", true))
        {
            Serial.println("[MQTT] WARN Failed to publish online availability");
        }

        String commandsTopic = MqttTopics::build("commands");
        if (!mqttClient.subscribe(commandsTopic.c_str()))
        {
            Serial.printf("[MQTT] WARN Subscribe failed topic=%s\n", commandsTopic.c_str());
        }

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
        return publishJson("heartbeat", MqttPayloads::buildHeartbeat(status, failures));
    }

    bool publishCommandResult(const CommandResult &result)
    {
        if (result.command_id.length() == 0)
        {
            Serial.println("[MQTT] WARN Command result skipped, missing command id");
            return false;
        }

        return publishJson("command-results", MqttPayloads::buildCommandResult(result));
    }
}
