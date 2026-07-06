#pragma once

#include <Arduino.h>
#include <IPAddress.h>

struct DiscoveredClient
{
    IPAddress ip;
    String mac;
};
