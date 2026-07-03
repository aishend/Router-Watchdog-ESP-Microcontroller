#ifndef ROUTER_WATCHDOG_H
#define ROUTER_WATCHDOG_H

namespace RouterWatchdog {

void begin();
void tick(unsigned long now);

}

#endif
