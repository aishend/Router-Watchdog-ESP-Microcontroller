#include "FirmwareUpdater.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <Updater.h>
#include <WiFiClientSecureBearSSL.h>
#include <bearssl/bearssl_hash.h>
#include <time.h>

#include "../config/AppConfig.h"
#include "../drivers/Relay.h"
#include "../mqtt/MqttClient.h"
#include "../network/NetworkManager.h"
#include "../operations/OperationCoordinator.h"
#include "../watchdog/RouterWatchdog.h"

namespace
{
    FirmwareUpdateRequest pendingRequest;
    bool updating = false;
    bool acceptedPublished = false;

    void publishFailure(const FirmwareUpdateRequest &request, const String &error)
    {
        Serial.printf("[OTA] WARN Update failed: %s\n", error.c_str());
        MqttClient::publishFirmwareStatus(request.version, "FAILED", error);
    }

    bool validHexDigest(const String &expected, const unsigned char actual[32])
    {
        char encoded[65];
        for (size_t i = 0; i < 32; i++)
        {
            snprintf(encoded + i * 2, 3, "%02x", actual[i]);
        }
        encoded[64] = '\0';

        String normalized = expected;
        normalized.toLowerCase();
        return normalized == encoded;
    }

    bool downloadAndInstall(const FirmwareUpdateRequest &request, String &error)
    {
        BearSSL::X509List trustAnchor(AppConfig::MQTT_CA_CERT);
        BearSSL::WiFiClientSecure client;
        client.setTrustAnchors(&trustAnchor);

        HTTPClient http;
        if (!http.begin(client, request.url))
        {
            error = "HTTPS_BEGIN_FAILED";
            return false;
        }

        const int responseCode = http.GET();
        const int contentLength = http.getSize();
        if (responseCode != HTTP_CODE_OK || contentLength <= 0)
        {
            error = String("HTTP_") + responseCode;
            http.end();
            return false;
        }

        if (static_cast<uint32_t>(contentLength) > ESP.getFreeSketchSpace())
        {
            error = "INSUFFICIENT_FLASH_SPACE";
            http.end();
            return false;
        }

        if (!Update.begin(contentLength))
        {
            error = "UPDATE_BEGIN_FAILED";
            http.end();
            return false;
        }

        br_sha256_context hashContext;
        br_sha256_init(&hashContext);
        WiFiClient *stream = http.getStreamPtr();
        uint8_t buffer[1024];
        size_t written = 0;

        while (http.connected() && written < static_cast<size_t>(contentLength))
        {
            const size_t available = stream->available();
            if (available == 0)
            {
                delay(1);
                continue;
            }

            const size_t chunkSize = min(available, sizeof(buffer));
            const int read = stream->readBytes(buffer, chunkSize);
            if (read <= 0 || Update.write(buffer, read) != static_cast<size_t>(read))
            {
                error = "FLASH_WRITE_FAILED";
                http.end();
                return false;
            }

            br_sha256_update(&hashContext, buffer, read);
            written += read;
            yield();
        }

        unsigned char digest[32];
        br_sha256_out(&hashContext, digest);
        http.end();

        if (written != static_cast<size_t>(contentLength))
        {
            error = "INCOMPLETE_DOWNLOAD";
            return false;
        }

        if (!validHexDigest(request.sha256, digest))
        {
            error = "SHA256_MISMATCH";
            return false;
        }

        if (!Update.end(true))
        {
            error = String("UPDATE_END_FAILED_") + Update.getError();
            return false;
        }

        return true;
    }
}

namespace FirmwareUpdater
{
    void begin()
    {
        pendingRequest = FirmwareUpdateRequest();
        updating = false;
        acceptedPublished = false;
        Serial.println("[OTA] Firmware updater initialized");
    }

    void requestUpdate(const FirmwareUpdateRequest &request)
    {
        if (!request.pending) return;
        pendingRequest = request;
        acceptedPublished = false;
        Serial.printf("[OTA] Desired version received: %s\n", request.version.c_str());
    }

    void tick(unsigned long now)
    {
        (void)now;
        if (!pendingRequest.pending || updating) return;

        if (pendingRequest.version == AppConfig::FIRMWARE_VERSION)
        {
            pendingRequest = FirmwareUpdateRequest();
            return;
        }

        if (!pendingRequest.url.startsWith("https://"))
        {
            FirmwareUpdateRequest request = pendingRequest;
            pendingRequest = FirmwareUpdateRequest();
            publishFailure(request, "HTTPS_REQUIRED");
            return;
        }

        if (strlen(AppConfig::MQTT_CA_CERT) == 0 || time(nullptr) < AppConfig::TLS_MIN_VALID_EPOCH)
        {
            return;
        }

        if (!acceptedPublished)
        {
            MqttClient::publishFirmwareStatus(pendingRequest.version, "ACCEPTED");
            acceptedPublished = true;
        }

        if (!OperationCoordinator::isIdle() || !RouterWatchdog::canStartFirmwareUpdate() ||
            Relay::isActive() || !NetworkManager::internetIsKnownAvailable())
        {
            return;
        }

        if (!OperationCoordinator::tryStart(DestructiveOperation::FirmwareUpdate)) return;

        FirmwareUpdateRequest request = pendingRequest;
        pendingRequest = FirmwareUpdateRequest();
        updating = true;
        MqttClient::publishFirmwareStatus(request.version, "DOWNLOADING");

        String error;
        if (!downloadAndInstall(request, error))
        {
            publishFailure(request, error);
            updating = false;
            OperationCoordinator::finish(DestructiveOperation::FirmwareUpdate);
            if (Update.isRunning())
            {
                Serial.println("[OTA] Restarting to discard incomplete or unverified update state");
                delay(100);
                ESP.restart();
            }
            return;
        }

        Serial.println("[OTA] Firmware verified; restarting");
        ESP.restart();
    }

    bool isUpdating() { return updating; }
}
