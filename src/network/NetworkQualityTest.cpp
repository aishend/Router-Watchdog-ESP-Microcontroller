#include "NetworkQualityTest.h"

#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <time.h>

#include "../config/AppConfig.h"
#include "../mqtt/MqttClient.h"
#include "../operations/OperationCoordinator.h"
#include "NetworkManager.h"

namespace
{
    class PatternStream : public Stream
    {
    public:
        explicit PatternStream(size_t size) : remaining(size) {}
        int available() override { return remaining; }
        int read() override { if (!remaining) return -1; remaining--; return 0xA5; }
        int peek() override { return remaining ? 0xA5 : -1; }
        size_t write(uint8_t) override { return 0; }
    private:
        size_t remaining;
    };

    unsigned long nextAutomaticTestMs = 0;
    bool testRequested = false;

    unsigned long scheduledInterval()
    {
        return AppConfig::SPEEDTEST_INTERVAL_MS +
               (ESP.getChipId() % (AppConfig::SPEEDTEST_JITTER_MS + 1));
    }

    bool measureDownload(BearSSL::WiFiClientSecure &client, uint32_t &latency, uint32_t &kbps)
    {
        HTTPClient http;
        if (!http.begin(client, AppConfig::SPEEDTEST_DOWNLOAD_URL)) return false;
        const unsigned long requestStarted = millis();
        if (http.GET() != HTTP_CODE_OK) { http.end(); return false; }
        latency = millis() - requestStarted;

        WiFiClient *stream = http.getStreamPtr();
        uint8_t buffer[1024];
        size_t received = 0;
        const unsigned long transferStarted = millis();
        while (http.connected() && received < AppConfig::SPEEDTEST_DOWNLOAD_BYTES)
        {
            const size_t available = stream->available();
            if (!available) { delay(1); continue; }
            const int count = stream->readBytes(buffer, min(available, sizeof(buffer)));
            if (count <= 0) break;
            received += count;
            yield();
        }
        const unsigned long elapsed = max(1UL, millis() - transferStarted);
        kbps = static_cast<uint32_t>((received * 8UL) / elapsed);
        http.end();
        return received > 0;
    }

    bool measureUpload(BearSSL::WiFiClientSecure &client, uint32_t &kbps)
    {
        HTTPClient http;
        if (!http.begin(client, AppConfig::SPEEDTEST_UPLOAD_URL)) return false;
        PatternStream payload(AppConfig::SPEEDTEST_UPLOAD_BYTES);
        const unsigned long started = millis();
        http.addHeader("Content-Type", "application/octet-stream");
        const int response = http.sendRequest("POST", &payload, AppConfig::SPEEDTEST_UPLOAD_BYTES);
        const unsigned long elapsed = max(1UL, millis() - started);
        http.end();
        if (response < 200 || response >= 300) return false;
        kbps = static_cast<uint32_t>((AppConfig::SPEEDTEST_UPLOAD_BYTES * 8UL) / elapsed);
        return true;
    }

    void runTest(unsigned long now)
    {
        BearSSL::X509List ca(AppConfig::MQTT_CA_CERT);
        BearSSL::WiFiClientSecure client;
        client.setTrustAnchors(&ca);
        uint32_t latency = 0, download = 0, upload = 0;
        const bool downloadOk = measureDownload(client, latency, download);
        client.stop();
        const bool uploadOk = measureUpload(client, upload);

        JsonDocument doc;
        doc["latencyMs"] = latency;
        doc["downloadKbps"] = download;
        doc["uploadKbps"] = upload;
        doc["rssi"] = WiFi.RSSI();
        doc["success"] = downloadOk && uploadOk;
        String json;
        serializeJson(doc, json);
        MqttClient::publishNetworkTestResult(json);
        OperationCoordinator::finish(DestructiveOperation::NetworkTest);
        nextAutomaticTestMs = now + scheduledInterval();
    }
}

namespace NetworkQualityTest
{
    void begin() { nextAutomaticTestMs = millis() + scheduledInterval(); }
    void startReservedTest() { testRequested = true; }

    void tick(unsigned long now)
    {
        const bool automaticDue = static_cast<long>(now - nextAutomaticTestMs) >= 0;
        if (!testRequested && automaticDue && OperationCoordinator::isIdle() &&
            NetworkManager::internetIsKnownAvailable())
        {
            if (OperationCoordinator::tryStart(DestructiveOperation::NetworkTest)) testRequested = true;
        }
        if (!testRequested || !OperationCoordinator::isActive(DestructiveOperation::NetworkTest)) return;
        if (strlen(AppConfig::SPEEDTEST_DOWNLOAD_URL) == 0 || strlen(AppConfig::SPEEDTEST_UPLOAD_URL) == 0 ||
            strlen(AppConfig::MQTT_CA_CERT) == 0 || time(nullptr) < AppConfig::TLS_MIN_VALID_EPOCH)
        {
            testRequested = false;
            OperationCoordinator::finish(DestructiveOperation::NetworkTest);
            nextAutomaticTestMs = now + scheduledInterval();
            return;
        }
        testRequested = false;
        runTest(now);
    }
}
