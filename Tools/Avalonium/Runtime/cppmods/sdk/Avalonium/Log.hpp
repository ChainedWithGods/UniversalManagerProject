#pragma once

#include "Api.hpp"

#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace Avalonium
{
    enum class LogLevel : int32_t
    {
        Verbose = AVALONIUM_LOG_DEBUG,
        Debug = AVALONIUM_LOG_DEBUG,
        Info = AVALONIUM_LOG_INFO,
        Warning = AVALONIUM_LOG_WARNING,
        Error = AVALONIUM_LOG_ERROR
    };

    inline void LogMessage(LogLevel level, std::string_view message)
    {
        const AvaloniumModApi* api = Detail::Api();
        if (api && api->log_message)
        {
            api->log_message(static_cast<int32_t>(level), std::string(message).c_str());
            return;
        }

        OutputDebugStringA("[Avalonium] ");
        OutputDebugStringA(std::string(message).c_str());
        OutputDebugStringA("\n");
    }

    template <class... Args>
    void Log(LogLevel level, std::format_string<Args...> fmt, Args&&... args)
    {
        LogMessage(level, std::format(fmt, std::forward<Args>(args)...));
    }

    template <class... Args>
    void LogInfo(std::format_string<Args...> fmt, Args&&... args)
    {
        Log(LogLevel::Info, fmt, std::forward<Args>(args)...);
    }

    template <class... Args>
    void LogWarning(std::format_string<Args...> fmt, Args&&... args)
    {
        Log(LogLevel::Warning, fmt, std::forward<Args>(args)...);
    }

    template <class... Args>
    void LogError(std::format_string<Args...> fmt, Args&&... args)
    {
        Log(LogLevel::Error, fmt, std::forward<Args>(args)...);
    }
}

namespace RC::Output
{
    enum class LogLevel
    {
        Verbose = AVALONIUM_LOG_DEBUG,
        Debug = AVALONIUM_LOG_DEBUG,
        Info = AVALONIUM_LOG_INFO,
        Warning = AVALONIUM_LOG_WARNING,
        Error = AVALONIUM_LOG_ERROR
    };

    template <LogLevel Level = LogLevel::Info, class... Args>
    void send(std::format_string<Args...> fmt, Args&&... args)
    {
        Avalonium::Log(static_cast<Avalonium::LogLevel>(Level), fmt, std::forward<Args>(args)...);
    }
}

namespace RC
{
    using LogLevel = Output::LogLevel;
}
