#include "NetworkManager.h"

#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#include "../config/AppConfig.h"

namespace
{

    bool canConnectToInternet()
    {
        for (size_t i = 0; i < AppConfig::INTERNET_TEST_HOST_COUNT; i++)
        {
            const char *host = AppConfig::INTERNET_TEST_HOSTS[i];
            WiFiClient client;
            client.setTimeout(AppConfig::INTERNET_TEST_TIMEOUT_MS);

            bool connected = client.connect(host, AppConfig::INTERNET_TEST_PORT);
            client.stop();

            if (connected)
            {
                Serial.printf("[NETWORK] Internet test OK via %s:%u\n", host, AppConfig::INTERNET_TEST_PORT);
                return true;
            }

            Serial.printf("[NETWORK] Internet test failed via %s:%u\n", host, AppConfig::INTERNET_TEST_PORT);
        }

        return false;
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

    NetworkStatus checkStatus()
    {
        NetworkStatus status;

        if (WiFi.status() == WL_CONNECTED)
        {
            status.wifi_connected = true;
            status.ip_address = WiFi.localIP();
            status.gateway_address = WiFi.gatewayIP();
            status.internet_connected = canConnectToInternet();
        }

        return status;
    }

}
