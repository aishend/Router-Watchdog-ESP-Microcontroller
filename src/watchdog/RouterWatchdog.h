#ifndef ROUTER_WATCHDOG_H
#define ROUTER_WATCHDOG_H

namespace RouterWatchdog
{
    void begin();
    void tick(unsigned long now);
    bool canRequestRouterReboot();
    bool canStartFirmwareUpdate();
    bool requestRouterReboot();
    bool isRecovering();
    bool isCooldownActive();
}

#endif
