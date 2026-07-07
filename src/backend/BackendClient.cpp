#include "BackendClient.h"

#include <ArduinoJson.h>
#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecureBearSSL.h>

#include "../commands/Command.h"
#include "../config/AppConfig.h"
#include "../logging/Logger.h"

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

    String buildNetworkClientsReportJson(const DiscoveredClient *clients, uint8_t count)
    {
        JsonDocument doc;

        doc["watchdogDeviceId"] = AppConfig::DEVICE_ID;

        JsonArray clientsArray = doc["clients"].to<JsonArray>();

        for (uint8_t i = 0; i < count; i++)
        {
            JsonObject client = clientsArray.add<JsonObject>();
            client["ip"] = clients[i].ip.toString();
            client["mac"] = clients[i].mac;
            client["hostname"] = nullptr;
            client["vendor"] = nullptr;
        }

        String json;
        serializeJson(doc, json);

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
            Log::errorf("BACKEND", "Failed to parse response JSON: %s", error.c_str());
            return response;
        }

        JsonVariant command = doc["command"];

        if (command.isNull())
        {
            return response;
        }

        const char *commandId = command["id"] | "";
        const char *commandType = command["type"] | "NONE";

        if (commandId[0] == '\0')
        {
            Log::warn("BACKEND", "Command ignored, missing id");
            return response;
        }

        response.command.has_command = true;
        response.command.id = commandId;
        response.command.type = parseCommand(String(commandType));

        Log::infof("BACKEND", "Command received id=%s type=%s", response.command.id.c_str(), commandType);

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

    bool beginBackendRequest(HTTPClient &http,
                             WiFiClient &plainClient,
                             BearSSL::WiFiClientSecure &secureClient,
                             const String &url)
    {
        if (!AppConfig::BACKEND_SECURE)
        {
            return http.begin(plainClient, url);
        }

        if (AppConfig::BACKEND_INSECURE_TLS)
        {
            secureClient.setInsecure();
        }

        secureClient.setBufferSizes(512, 512);
        return http.begin(secureClient, url);
    }

    int postJson(const String &url, const String &payload, const char *label, String *responseBody = nullptr)
    {
        Log::infof("BACKEND", "Sending %s via %s POST %s", label, backendProtocolLabel(), url.c_str());

        WiFiClient plainClient;
        BearSSL::WiFiClientSecure secureClient;

        HTTPClient http;
        http.setTimeout(AppConfig::BACKEND_TIMEOUT_MS);

        if (!beginBackendRequest(http, plainClient, secureClient, url))
        {
            Log::errorf("BACKEND", "%s begin() failed", backendProtocolLabel());
            return -1;
        }

        http.addHeader("Content-Type", "application/json");

        int statusCode = http.POST(payload);

        if (statusCode <= 0)
        {
            Log::errorf(
                "BACKEND",
                "%s POST %s failed: %s",
                backendProtocolLabel(),
                label,
                http.errorToString(statusCode).c_str());
            http.end();
            return statusCode;
        }

        Log::infof("BACKEND", "%s %s response status=%d", backendProtocolLabel(), label, statusCode);

        String body = http.getString();

        Log::debugf("BACKEND", "%s response body=%s", label, body.c_str());

        if (responseBody != nullptr)
        {
            *responseBody = body;
        }

        http.end();
        return statusCode;
    }

    String buildUrlForPath(const char *path)
    {
        String url = String(AppConfig::BACKEND_URL);

        int apiIndex = url.indexOf("/api/v1/");

        if (apiIndex >= 0)
        {
            url = url.substring(0, apiIndex);
        }
        else
        {
            url.replace("/heartbeat", "");
        }

        url += path;
        return url;
    }
}

namespace BackendClient
{

    void begin()
    {
        Log::info("BACKEND", "Client initialized");
        Log::infof("BACKEND", "Transport=%s", backendProtocolLabel());

        if (AppConfig::BACKEND_SECURE && AppConfig::BACKEND_INSECURE_TLS)
        {
            Log::warn("BACKEND", "TLS certificate validation disabled");
        }
    }

    HeartbeatResponse sendHeartbeat(const NetworkStatus &status, uint8_t failures)
    {
        HeartbeatResponse response;

        if (!status.wifi_connected)
        {
            Log::info("BACKEND", "Heartbeat skipped, Wi-Fi not connected");
            return response;
        }

        String url = String(AppConfig::BACKEND_URL);
        String body;
        int statusCode = postJson(url, buildHeartbeatJson(status, failures), "heartbeat", &body);

        if (statusCode <= 0)
        {
            return response;
        }

        response = parseHeartbeatResponse(statusCode, body);

        if (!response.accepted)
        {
            Log::warn("BACKEND", "Heartbeat rejected by backend");
        }

        return response;
    }

    bool sendCommandResult(const CommandResult &result)
    {
        if (result.command_id.length() == 0)
        {
            Log::warn("BACKEND", "Command result skipped, missing command id");
            return false;
        }

        String url = buildUrlForPath("/api/v1/command-results");

        int statusCode = postJson(url, buildCommandResultJson(result), "command result");

        return statusCode >= 200 && statusCode < 300;
    }

    bool sendNetworkClientsReport(const DiscoveredClient *clients, uint8_t count)
    {
        if (count == 0)
        {
            Log::info("BACKEND", "Network clients report skipped, no clients found");
            return false;
        }

        String url = buildUrlForPath("/api/v1/network-clients/report");

        String payload = buildNetworkClientsReportJson(clients, count);

        Log::debugf("BACKEND", "Network clients payload=%s", payload.c_str());

        int statusCode = postJson(url, payload, "network clients");

        return statusCode >= 200 && statusCode < 300;
    }

}
