#include <Arduino.h>

#include "backend/BackendClient.h"
#include "config/AppConfig.h"
#include "drivers/Relay.h"
#include "logging/Logger.h"
#include "network/NetworkManager.h"
#include "network/NetworkScanner.h"
#include "watchdog/RouterWatchdog.h"
#include "commands/CommandManager.h"

namespace
{

    NetworkScanner networkScanner;
    unsigned long lastNetworkScanAt = 0;

    bool startupGracePeriodElapsed(unsigned long now)
    {
        return now >= AppConfig::STARTUP_GRACE_PERIOD_MS;
    }

    void tickNetworkScanner(unsigned long now)
    {
        if (!AppConfig::ARP_SCAN_ENABLED)
        {
            return;
        }

        if (!startupGracePeriodElapsed(now))
        {
            return;
        }

        if (lastNetworkScanAt != 0 &&
            now - lastNetworkScanAt < AppConfig::NETWORK_SCAN_INTERVAL_MS)
        {
            return;
        }

        lastNetworkScanAt = now;

        DiscoveredClient clients[NetworkScanner::MAX_CLIENTS];
        uint8_t clientCount = networkScanner.scan(clients, NetworkScanner::MAX_CLIENTS);

        if (clientCount == 0)
        {
            return;
        }

        BackendClient::sendNetworkClientsReport(clients, clientCount);
    }

}

void setup()
{
    Serial.begin(AppConfig::SERIAL_BAUD);
    Log::blank();
    Log::info("BOOT", "Router watchdog starting");

    if (AppConfig::hasPlaceholderSecrets())
    {
        Log::warn("BOOT", "placeholder config detected, check AppSecrets.h");
    }

    Relay::begin();
    NetworkManager::begin();
    BackendClient::begin();
    RouterWatchdog::begin();
    CommandManager::begin();

    Log::infof("BOOT", "Waiting for Wi-Fi startup grace period (%lu ms)", AppConfig::STARTUP_GRACE_PERIOD_MS);
    Log::infof("BOOT", "ARP scan=%s", AppConfig::ARP_SCAN_ENABLED ? "enabled" : "disabled");
}

void loop()
{
    unsigned long now = millis();

    if (startupGracePeriodElapsed(now))
    {
        RouterWatchdog::tick(now);
        CommandManager::tick(now);
        tickNetworkScanner(now);
    }

    delay(10);
}
