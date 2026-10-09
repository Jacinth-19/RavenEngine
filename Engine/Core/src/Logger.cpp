#include "Raven/Core/Logger.h"

#include <cstdio>

namespace Raven::Log
{
    namespace
    {
        LogLevel g_level = LogLevel::Info;

        void write(LogLevel level, const char* tag, const std::string& message)
        {
            if (level < g_level)
            {
                return;
            }
            std::fprintf(stderr, "[%s] %s\n", tag, message.c_str());
        }
    }

    void setLevel(LogLevel level) { g_level = level; }
    LogLevel level() { return g_level; }

    void debug(const std::string& message)   { write(LogLevel::Debug,   "DEBUG", message); }
    void info(const std::string& message)    { write(LogLevel::Info,    "INFO",  message); }
    void warning(const std::string& message) { write(LogLevel::Warning, "WARN",  message); }
    void error(const std::string& message)   { write(LogLevel::Error,   "ERROR", message); }
}
