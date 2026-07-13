#ifndef FIRMWARE_UPDATE_REQUEST_H
#define FIRMWARE_UPDATE_REQUEST_H

#include <Arduino.h>

struct FirmwareUpdateRequest
{
    bool pending = false;
    String version;
    String url;
    String sha256;
};

#endif
