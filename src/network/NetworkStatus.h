#ifndef NETWORK_STATUS_H
#define NETWORK_STATUS_H

#include <IPAddress.h>

struct NetworkStatus {
    bool wifi_connected = false;
    bool internet_connected = false;
    IPAddress ip_address = IPAddress(0, 0, 0, 0);
    IPAddress gateway_address = IPAddress(0, 0, 0, 0);
};

#endif
