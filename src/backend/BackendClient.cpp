#include "BackendClient.h"

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266WiFi.h>
#include "../config/AppConfig.h"
#include <ArduinoJson.h>
#include "../commands/Command.h"

namespace
{

    String buildHeartbeatJson(const NetworkStatus &status, uint8_t failures)
    {
        String json;
        json.reserve(300);

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

        json += ",\"rssi\":";
        json += String(WiFi.RSSI());

        json += ",\"freeHeap\":";
        json += String(ESP.getFreeHeap());

        json += ",\"firmwareVersion\":\"";
        json += AppConfig::FIRMWARE_VERSION;

        json += "\"}";

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

    String commandResultStatusToString(CommandResultStatus status)
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

    String buildCommandResultJson(const CommandResult &result)
    {
        String json;
        json.reserve(120);

        json += "{\"commandId\":\"";
        json += result.command_id;
        json += "\",\"status\":\"";
        json += commandResultStatusToString(result.status);
        json += "\"}";

        return json;
    }

    const char *backendProtocolLabel()
    {
        return AppConfig::BACKEND_SECURE ? "HTTPS" : "HTTP";
    }

    bool beginBackendRequest(HTTPClient &http, WiFiClient &plain_client, BearSSL::WiFiClientSecure &secure_client, const String &url)
    {
        if (!AppConfig::BACKEND_SECURE)
        {
            return http.begin(plain_client, url);
        }

        if (AppConfig::BACKEND_INSECURE_TLS)
        {
            secure_client.setInsecure();
        }

        secure_client.setBufferSizes(512, 512);
        return http.begin(secure_client, url);
    }
}

namespace BackendClient
{

    void begin()
    {
        Serial.println("[BACKEND] Client initialized");
        Serial.print("[BACKEND] Transport=");
        Serial.println(backendProtocolLabel());

        if (AppConfig::BACKEND_SECURE && AppConfig::BACKEND_INSECURE_TLS)
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
        Serial.print("[");
        Serial.print(backendProtocolLabel());
        Serial.print("] POST ");
        Serial.println(AppConfig::BACKEND_URL);

        String url = String(AppConfig::BACKEND_URL);
        WiFiClient plain_client;
        BearSSL::WiFiClientSecure secure_client;

        HTTPClient http;
        http.setTimeout(AppConfig::BACKEND_TIMEOUT_MS);

        if (!beginBackendRequest(http, plain_client, secure_client, url))
        {
            Serial.print("[");
            Serial.print(backendProtocolLabel());
            Serial.println("] begin() failed");
            return response;
        }

        http.addHeader("Content-Type", "application/json");

        int status_code = http.POST(buildHeartbeatJson(status, failures));

        if (status_code <= 0)
        {
            Serial.print("[");
            Serial.print(backendProtocolLabel());
            Serial.print("] POST failed: ");
            Serial.println(http.errorToString(status_code));
            http.end();
            return response;
        }

        Serial.print("[");
        Serial.print(backendProtocolLabel());
        Serial.print("] Response status=");
        Serial.println(status_code);

        String body = http.getString();

        Serial.print("[");
        Serial.print(backendProtocolLabel());
        Serial.print("] Response body=");
        Serial.println(body);

        response = parseHeartbeatResponse(status_code, body);

        if (!response.accepted)
        {
            Serial.println("[BACKEND] Heartbeat rejected by backend");
        }

        http.end();

        return response;
    }

    bool sendCommandResult(const CommandResult &result)
    {
        if (result.command_id.length() == 0)
        {
            Serial.println("[BACKEND] Command result skipped, missing command id");
            return false;
        }

        String url = String(AppConfig::BACKEND_URL);
        url.replace("/heartbeat", "/command-results");

        Serial.println("[BACKEND] Sending command result");
        Serial.print("[");
        Serial.print(backendProtocolLabel());
        Serial.print("] POST ");
        Serial.println(url);

        WiFiClient plain_client;
        BearSSL::WiFiClientSecure secure_client;

        HTTPClient http;
        http.setTimeout(AppConfig::BACKEND_TIMEOUT_MS);

        if (!beginBackendRequest(http, plain_client, secure_client, url))
        {
            Serial.print("[");
            Serial.print(backendProtocolLabel());
            Serial.println("] begin() failed");
            return false;
        }

        http.addHeader("Content-Type", "application/json");

        int status_code = http.POST(buildCommandResultJson(result));

        if (status_code <= 0)
        {
            Serial.print("[");
            Serial.print(backendProtocolLabel());
            Serial.print("] POST command result failed: ");
            Serial.println(http.errorToString(status_code));
            http.end();
            return false;
        }

        Serial.print("[");
        Serial.print(backendProtocolLabel());
        Serial.print("] Command result response status=");
        Serial.println(status_code);

        String body = http.getString();

        Serial.print("[");
        Serial.print(backendProtocolLabel());
        Serial.print("] Command result response body=");
        Serial.println(body);

        http.end();

        return status_code >= 200 && status_code < 300;
    }

}
