#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "NetworkStatus.h"

namespace NetworkManager
{

    void begin();
    void tick();
    void requestStatusCheck();
    bool takeStatusResult(NetworkStatus &status);
    bool internetIsKnownAvailable();

}

#endif
