#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

namespace Log
{
    void blank();
    void info(const char *tag, const char *message);
    void warn(const char *tag, const char *message);
    void error(const char *tag, const char *message);
    void infof(const char *tag, const char *format, ...);
    void warnf(const char *tag, const char *format, ...);
    void errorf(const char *tag, const char *format, ...);
    void debugf(const char *tag, const char *format, ...);
}

#endif
