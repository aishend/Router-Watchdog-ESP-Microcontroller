#ifndef MQTT_TOPICS_H
#define MQTT_TOPICS_H

#include <Arduino.h>

namespace MqttTopics
{
    String build(const char *suffix);
}

#endif
