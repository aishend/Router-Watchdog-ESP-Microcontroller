#include "NetworkManager.h"

#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#include "../config/AppConfig.h"

namespace
{

    bool canConnectToInternet()
    {
        WiFiClient client;
        client.setTimeout(AppConfig::INTERNET_TEST_TIMEOUT_MS);

        bool connected = client.connect(
            AppConfig::INTERNET_TEST_HOST,
            AppConfig::INTERNET_TEST_PORT);

        client.stop();
        return connected;
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
