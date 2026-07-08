#include "MqttTopics.h"

#include "../config/AppConfig.h"

namespace MqttTopics
{
    String build(const char *suffix)
    {
        String value = String(AppConfig::MQTT_TOPIC_PREFIX);
        value += "/";
        value += AppConfig::DEVICE_ID;
        value += "/";
        value += suffix;
        return value;
    }
}
