#pragma once

#include <Arduino.h>
#include <IPAddress.h>

#include "DiscoveredClient.h"

class NetworkScanner
{
public:
    static constexpr uint8_t MAX_CLIENTS = 64;

    uint8_t scan(DiscoveredClient *clients, uint8_t maxClients);

private:
    bool isUsableTarget(IPAddress ip) const;
    bool resolveMac(IPAddress ip, String &mac);
    String formatMac(const uint8_t *macBytes) const;
};
