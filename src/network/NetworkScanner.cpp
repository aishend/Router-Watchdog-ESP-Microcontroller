#include "NetworkScanner.h"

#include <ESP8266WiFi.h>

#include "../config/AppConfig.h"
#include "../logging/Logger.h"

extern "C"
{
#include <lwip/etharp.h>
#include <lwip/ip4_addr.h>
#include <lwip/netif.h>
}

namespace
{
    uint32_t ipToUint32(IPAddress ip)
    {
        return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | ((uint32_t)ip[2] << 8) | ((uint32_t)ip[3]);
    }

    IPAddress uint32ToIp(uint32_t value)
    {
        return IPAddress(
            (value >> 24) & 0xFF,
            (value >> 16) & 0xFF,
            (value >> 8) & 0xFF,
            value & 0xFF);
    }
}

uint8_t NetworkScanner::scan(DiscoveredClient *clients, uint8_t maxClients)
{
    if (WiFi.status() != WL_CONNECTED || maxClients == 0)
    {
        return 0;
    }

    IPAddress localIp = WiFi.localIP();
    IPAddress subnetMask = WiFi.subnetMask();

    uint32_t local = ipToUint32(localIp);
    uint32_t mask = ipToUint32(subnetMask);
    uint32_t network = local & mask;
    uint32_t broadcast = network | ~mask;

    if (broadcast <= network + 1)
    {
        Log::warn("NETWORK_SCANNER", "Scan skipped, subnet has no usable host range");
        return 0;
    }

    uint32_t firstHost = network + 1;
    uint32_t lastHost = broadcast - 1;
    uint16_t hostCount = 0;

    uint8_t count = 0;

    Log::infof(
        "NETWORK_SCANNER",
        "Starting ARP scan | IP=%s | Mask=%s | Network=%s | Broadcast=%s | Max clients=%u",
        localIp.toString().c_str(),
        subnetMask.toString().c_str(),
        uint32ToIp(network).toString().c_str(),
        uint32ToIp(broadcast).toString().c_str(),
        maxClients);

    for (uint8_t round = 1; round <= 3; round++)
    {
        Log::debugf("NETWORK_SCANNER", "ARP round %u", round);

        hostCount = 0;

        for (uint32_t current = firstHost; current <= lastHost && hostCount < AppConfig::NETWORK_SCAN_MAX_HOSTS; current++, hostCount++)
        {
            IPAddress targetIp = uint32ToIp(current);

            if (!isUsableTarget(targetIp))
            {
                continue;
            }

            ip4_addr_t lwipIp;
            IP4_ADDR(
                &lwipIp,
                targetIp[0],
                targetIp[1],
                targetIp[2],
                targetIp[3]);

            if (netif_default != nullptr)
            {
                etharp_request(netif_default, &lwipIp);
            }

            delay(5);
            yield();
        }

        delay(1000);
        yield();
    }

    delay(3000);
    yield();

    hostCount = 0;

    for (uint32_t current = firstHost; current <= lastHost && hostCount < AppConfig::NETWORK_SCAN_MAX_HOSTS; current++, hostCount++)
    {
        if (count >= maxClients)
        {
            break;
        }

        IPAddress targetIp = uint32ToIp(current);

        if (!isUsableTarget(targetIp))
        {
            continue;
        }

        String mac;

        if (!resolveMac(targetIp, mac))
        {
            yield();
            continue;
        }

        clients[count].ip = targetIp;
        clients[count].mac = mac;
        count++;

        Log::debugf("NETWORK_SCANNER", "Found %s %s", targetIp.toString().c_str(), mac.c_str());

        yield();
    }

    Log::infof("NETWORK_SCANNER", "Scan finished. clients=%u", count);

    return count;
}

bool NetworkScanner::isUsableTarget(IPAddress ip) const
{
    if (ip == WiFi.localIP())
    {
        return false;
    }

    if (ip == WiFi.gatewayIP())
    {
        return false;
    }

    return true;
}

bool NetworkScanner::resolveMac(IPAddress ip, String &mac)
{
    if (netif_default == nullptr)
    {
        return false;
    }

    ip4_addr_t lwipIp;
    IP4_ADDR(
        &lwipIp,
        ip[0],
        ip[1],
        ip[2],
        ip[3]);

    struct eth_addr *ethAddress = nullptr;
    const ip4_addr_t *resolvedIp = nullptr;

    int result = etharp_find_addr(
        netif_default,
        &lwipIp,
        &ethAddress,
        &resolvedIp);

    if (result < 0 || ethAddress == nullptr)
    {
        return false;
    }

    mac = formatMac(ethAddress->addr);
    return true;
}

String NetworkScanner::formatMac(const uint8_t *macBytes) const
{
    char buffer[18];

    snprintf(
        buffer,
        sizeof(buffer),
        "%02X:%02X:%02X:%02X:%02X:%02X",
        macBytes[0],
        macBytes[1],
        macBytes[2],
        macBytes[3],
        macBytes[4],
        macBytes[5]);

    return String(buffer);
}
