#include "Logger.h"

#include <stdarg.h>
#include <stdio.h>

#include "../config/AppConfig.h"

namespace
{
    constexpr size_t LOG_BUFFER_SIZE = 192;

    void writeLine(const char *tag, const char *level, const char *message)
    {
        Serial.print("[");
        Serial.print(tag);
        Serial.print("]");

        if (level != nullptr && level[0] != '\0')
        {
            Serial.print(" ");
            Serial.print(level);
        }

        Serial.print(" ");
        Serial.println(message);
    }

    void writeFormatted(const char *tag, const char *level, const char *format, va_list args)
    {
        char message[LOG_BUFFER_SIZE];
        vsnprintf(message, sizeof(message), format, args);
        writeLine(tag, level, message);
    }
}

namespace Log
{
    void blank()
    {
        Serial.println();
    }

    void info(const char *tag, const char *message)
    {
        writeLine(tag, "", message);
    }

    void warn(const char *tag, const char *message)
    {
        writeLine(tag, "WARN", message);
    }

    void error(const char *tag, const char *message)
    {
        writeLine(tag, "ERROR", message);
    }

    void infof(const char *tag, const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        writeFormatted(tag, "", format, args);
        va_end(args);
    }

    void warnf(const char *tag, const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        writeFormatted(tag, "WARN", format, args);
        va_end(args);
    }

    void errorf(const char *tag, const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        writeFormatted(tag, "ERROR", format, args);
        va_end(args);
    }

    void debugf(const char *tag, const char *format, ...)
    {
        if (!AppConfig::DEBUG_LOGS_ENABLED)
        {
            return;
        }

        va_list args;
        va_start(args, format);
        writeFormatted(tag, "DEBUG", format, args);
        va_end(args);
    }
}
