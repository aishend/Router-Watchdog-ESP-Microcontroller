#include "BackendClient.h"

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#include "../config/AppConfig.h"

namespace {

String buildHeartbeatJson(const NetworkStatus &status, uint8_t failures)
{
    String json;
    json.reserve(220);

    json += "{\"deviceId\":\"";
    json += AppConfig::DEVICE_ID;
    json += "\",\"wifiConnected\":";
    json += status.wifi_connected ? "true" : "false";
    json += ",\"internetConnected\":";
    json += status.internet_connected ? "true" : "false";
    json += ",\"ip\":\"";
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

}

namespace BackendClient {

void begin()
{
    Serial.println("[BACKEND] Client initialized");
}

bool sendHeartbeat(const NetworkStatus &status, uint8_t failures)
{
    if (!status.wifi_connected) {
        Serial.println("[BACKEND] Heartbeat skipped, Wi-Fi not connected");
        return false;
    }

    Serial.println("[BACKEND] Sending heartbeat");
    Serial.print("[HTTPS] POST ");
    Serial.println(AppConfig::BACKEND_URL);

    BearSSL::WiFiClientSecure client;
    client.setInsecure();
    client.setBufferSizes(512, 512);

    HTTPClient http;
    http.setTimeout(AppConfig::BACKEND_TIMEOUT_MS);

    if (!http.begin(client, AppConfig::BACKEND_URL)) {
        Serial.println("[HTTPS] begin() failed");
        return false;
    }

    http.addHeader("Content-Type", "application/json");

    int status_code = http.POST(buildHeartbeatJson(status, failures));

    if (status_code <= 0) {
        Serial.print("[HTTPS] POST failed: ");
        Serial.println(http.errorToString(status_code));
        http.end();
        return false;
    }

    Serial.print("[HTTPS] Response status=");
    Serial.println(status_code);

    http.end();
    return status_code >= 200 && status_code < 300;
}

}
