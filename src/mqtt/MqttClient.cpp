#include "MqttClient.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <PubSubClient.h>
#include <time.h>

#include "../commands/CommandManager.h"
#include "../config/AppConfig.h"
#include "../ota/FirmwareUpdater.h"
#include "MqttPayloads.h"
#include "MqttTopics.h"

namespace
{
    BearSSL::WiFiClientSecure secureClient;
    BearSSL::X509List *trustAnchor = nullptr;
    PubSubClient mqttClient(secureClient);

    unsigned long lastReconnectAttemptMs = 0;
    unsigned long lastAvailabilityAttemptMs = 0;
    bool availabilityPublished = false;

    bool deadlineElapsed(unsigned long now, unsigned long previous, unsigned long interval)
    {
        return previous == 0 || now - previous >= interval;
    }

    bool clockIsValid()
    {
        return time(nullptr) >= AppConfig::TLS_MIN_VALID_EPOCH;
    }

    void handleMessage(char *topicName, byte *payload, unsigned int length)
    {
        String commandsTopic = MqttTopics::build("commands");

        String desiredFirmwareTopic = MqttTopics::build("firmware/desired");
        if (desiredFirmwareTopic == topicName)
        {
            FirmwareUpdateRequest request;
            if (MqttPayloads::parseFirmwareUpdate(payload, length, request))
            {
                FirmwareUpdater::requestUpdate(request);
            }
            return;
        }

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
        if (WiFi.status() != WL_CONNECTED || trustAnchor == nullptr || !clockIsValid())
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
        availabilityPublished = mqttClient.publish(availabilityTopic.c_str(), "online", true);
        lastAvailabilityAttemptMs = millis();

        String commandsTopic = MqttTopics::build("commands");
        if (!mqttClient.subscribe(commandsTopic.c_str()))
        {
            Serial.printf("[MQTT] WARN Subscribe failed topic=%s\n", commandsTopic.c_str());
        }

        String desiredFirmwareTopic = MqttTopics::build("firmware/desired");
        if (!mqttClient.subscribe(desiredFirmwareTopic.c_str()))
        {
            Serial.printf("[MQTT] WARN Subscribe failed topic=%s\n", desiredFirmwareTopic.c_str());
        }

        return true;
    }
}

namespace MqttClient
{
    void begin()
    {
        configTime(0, 0, AppConfig::NTP_SERVERS[0], AppConfig::NTP_SERVERS[1]);
        if (strlen(AppConfig::MQTT_CA_CERT) > 0)
        {
            trustAnchor = new BearSSL::X509List(AppConfig::MQTT_CA_CERT);
            secureClient.setTrustAnchors(trustAnchor);
        }
        else
        {
            Serial.println("[MQTT] ERROR CA certificate is not configured; TLS connections disabled");
        }

        mqttClient.setServer(AppConfig::MQTT_HOST, AppConfig::MQTT_PORT);
        mqttClient.setCallback(handleMessage);
        mqttClient.setBufferSize(AppConfig::MQTT_BUFFER_SIZE);

        Serial.println("[MQTT] Client initialized");
    }

    void tick(unsigned long now)
    {
        if (!mqttClient.connected())
        {
            availabilityPublished = false;
            if (lastReconnectAttemptMs == 0 ||
                now - lastReconnectAttemptMs >= AppConfig::MQTT_RECONNECT_INTERVAL_MS)
            {
                lastReconnectAttemptMs = now;
                reconnect();
            }

            return;
        }

        mqttClient.loop();

        if (!availabilityPublished &&
            deadlineElapsed(now, lastAvailabilityAttemptMs, AppConfig::MQTT_AVAILABILITY_RETRY_INTERVAL_MS))
        {
            String topic = MqttTopics::build("availability");
            lastAvailabilityAttemptMs = now;
            availabilityPublished = mqttClient.publish(topic.c_str(), "online", true);
        }
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

    bool publishFirmwareStatus(const String &targetVersion, const char *status, const String &error)
    {
        return publishJson("firmware/status", MqttPayloads::buildFirmwareStatus(targetVersion, status, error));
    }
}
