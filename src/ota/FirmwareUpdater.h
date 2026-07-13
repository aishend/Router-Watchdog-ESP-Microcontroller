#ifndef FIRMWARE_UPDATER_H
#define FIRMWARE_UPDATER_H

#include "FirmwareUpdateRequest.h"

namespace FirmwareUpdater
{
    void begin();
    void requestUpdate(const FirmwareUpdateRequest &request);
    void tick(unsigned long now);
    bool isUpdating();
}

#endif
