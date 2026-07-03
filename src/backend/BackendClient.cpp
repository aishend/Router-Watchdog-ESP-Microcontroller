#include "BackendClient.h"

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#include "../config/AppConfig.h"
#include <ArduinoJson.h>
#include "../commands/Command.h"

namespace
{

    String buildHeartbeatJson(const NetworkStatus &status, uint8_t failures)
    {
        String json;
        json.reserve(220);

        json += "{\"deviceId\":\"";
        json += AppConfig::DEVICE_ID;
        json += "\",\"ip\":\"";
        json += status.ip_address.toString();
        json += "\",\"gateway\":\"";
        json += status.gateway_address.toString();
        json += "\",\"failures\":";
        json += String(static_cast<unsigned int>(failures));
        json += ",\"uptime\":";
        json += String(millis() / 1000UL);
        json += "}";

        return json;
    }

    CommandType parseCommand(const String &command)
    {
        if (command == "REBOOT_ROUTER")
        {
            return CommandType::RebootRouter;
        }

        if (command == "REBOOT_DEVICE")
        {
            return CommandType::RebootDevice;
        }

        return CommandType::None;
    }

    HeartbeatResponse parseHeartbeatResponse(int status_code, const String &body)
    {
        HeartbeatResponse response;
        response.accepted = status_code >= 200 && status_code < 300;

        if (!response.accepted || body.length() == 0)
        {
            return response;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body);

        if (error)
        {
            Serial.print("[BACKEND] Failed to parse response JSON: ");
            Serial.println(error.c_str());
            return response;
        }

        JsonVariant command = doc["command"];

        if (command.isNull())
        {
            return response;
        }

        const char *command_id = command["id"] | "";
        const char *command_type = command["type"] | "NONE";

        response.command.has_command = true;
        response.command.id = command_id;
        response.command.type = parseCommand(String(command_type));

        Serial.print("[BACKEND] Command id=");
        Serial.print(response.command.id);
        Serial.print(" type=");
        Serial.println(command_type);

        return response;
    }
}

namespace BackendClient
{

    void begin()
    {
        Serial.println("[BACKEND] Client initialized");
        if (AppConfig::BACKEND_INSECURE_TLS)
        {
            Serial.println("[BACKEND] WARNING: TLS certificate validation disabled");
        }
    }

    HeartbeatResponse sendHeartbeat(const NetworkStatus &status, uint8_t failures)
    {
        HeartbeatResponse response;

        if (!status.wifi_connected)
        {
            Serial.println("[BACKEND] Heartbeat skipped, Wi-Fi not connected");
            return response;
        }

        Serial.println("[BACKEND] Sending heartbeat");
        Serial.print("[HTTPS] POST ");
        Serial.println(AppConfig::BACKEND_URL);

        BearSSL::WiFiClientSecure client;
        if (AppConfig::BACKEND_INSECURE_TLS)
        {
            client.setInsecure();
        }
        client.setBufferSizes(512, 512);

        HTTPClient http;
        http.setTimeout(AppConfig::BACKEND_TIMEOUT_MS);

        if (!http.begin(client, AppConfig::BACKEND_URL))
        {
            Serial.println("[HTTPS] begin() failed");
            return response;
        }

        http.addHeader("Content-Type", "application/json");

        int status_code = http.POST(buildHeartbeatJson(status, failures));

        if (status_code <= 0)
        {
            Serial.print("[HTTPS] POST failed: ");
            Serial.println(http.errorToString(status_code));
            http.end();
            return response;
        }

        Serial.print("[HTTPS] Response status=");
        Serial.println(status_code);

        String body = http.getString();

        Serial.print("[HTTPS] Response body=");
        Serial.println(body);

        response = parseHeartbeatResponse(status_code, body);

        if (!response.accepted)
        {
            Serial.println("[BACKEND] Heartbeat rejected by backend");
        }

        http.end();

        return response;
    }

}
