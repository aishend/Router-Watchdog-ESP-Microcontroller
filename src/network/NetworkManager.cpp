#include "NetworkManager.h"

#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#include "../config/AppConfig.h"

namespace
{
    enum class ProbeState
    {
        Idle,
        Testing,
        Complete,
    };

    ProbeState probeState = ProbeState::Idle;
    NetworkStatus probeResult;
    size_t nextHostIndex = 0;
    size_t attemptsRemaining = 0;
    bool latestInternetAvailable = false;

    void completeProbe(bool internetConnected)
    {
        probeResult.internet_connected = internetConnected;
        latestInternetAvailable = internetConnected;
        probeState = ProbeState::Complete;
    }

    void testOneHost()
    {
        const char *host = AppConfig::INTERNET_TEST_HOSTS[nextHostIndex];
        nextHostIndex = (nextHostIndex + 1) % AppConfig::INTERNET_TEST_HOST_COUNT;

        WiFiClient client;
        client.setTimeout(AppConfig::INTERNET_TEST_TIMEOUT_MS);
        const bool connected = client.connect(host, AppConfig::INTERNET_TEST_PORT);
        client.stop();

        Serial.printf("[NETWORK] Internet test %s via %s:%u\n",
                      connected ? "OK" : "failed", host, AppConfig::INTERNET_TEST_PORT);

        if (connected)
        {
            completeProbe(true);
            return;
        }

        attemptsRemaining--;
        if (attemptsRemaining == 0)
        {
            completeProbe(false);
        }
    }
}

namespace NetworkManager
{
    void begin()
    {
        Serial.println("[NETWORK] Initialization started");
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.begin(AppConfig::WIFI_SSID, AppConfig::WIFI_PASSWORD);
        Serial.println("[NETWORK] Wi-Fi connection requested");
    }

    void requestStatusCheck()
    {
        if (probeState != ProbeState::Idle)
        {
            return;
        }

        probeResult = NetworkStatus();
        if (WiFi.status() != WL_CONNECTED)
        {
            latestInternetAvailable = false;
            probeState = ProbeState::Complete;
            return;
        }

        probeResult.wifi_connected = true;
        probeResult.ip_address = WiFi.localIP();
        probeResult.gateway_address = WiFi.gatewayIP();
        attemptsRemaining = AppConfig::INTERNET_TEST_HOST_COUNT;
        probeState = ProbeState::Testing;
    }

    void tick()
    {
        if (probeState == ProbeState::Testing)
        {
            testOneHost();
        }
    }

    bool takeStatusResult(NetworkStatus &status)
    {
        if (probeState != ProbeState::Complete)
        {
            return false;
        }

        status = probeResult;
        probeState = ProbeState::Idle;
        return true;
    }

    bool internetIsKnownAvailable()
    {
        return WiFi.status() == WL_CONNECTED && latestInternetAvailable;
    }
}
