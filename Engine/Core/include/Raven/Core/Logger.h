#pragma once

#include <string>

namespace Raven
{
    enum class LogLevel
    {
        Debug = 0,
        Info,
        Warning,
        Error,
        Off
    };

    // Minimal process-wide logger. Messages at or above the current level
    // are written to stderr. Replace the sink later without touching callers.
    namespace Log
    {
        void setLevel(LogLevel level);
        LogLevel level();

        void debug(const std::string& message);
        void info(const std::string& message);
        void warning(const std::string& message);
        void error(const std::string& message);
    }
}
