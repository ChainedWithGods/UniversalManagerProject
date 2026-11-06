#pragma once

#include "Log.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace RC
{

    class CppUserModBase
    {
      public:
        std::string ModName{};
        std::string ModVersion{};
        std::string ModDescription{};
        std::string ModAuthors{};
        std::string ModIntendedSDKVersion{};

        CppUserModBase() = default;
        virtual ~CppUserModBase() = default;

        CppUserModBase(const CppUserModBase&) = delete;
        CppUserModBase& operator=(const CppUserModBase&) = delete;

        virtual void on_program_start() {}
        virtual void on_unreal_init() {}
        virtual void on_ui_init() {}
        virtual void on_update() {}
        virtual void on_cpp_mods_loaded() {}
        virtual void on_unload() {}

        std::string GetModPath() const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            const char* path = (api && api->get_mod_directory) ? api->get_mod_directory() : nullptr;
            return path ? path : "";
        }

        void LogInfo(std::string_view message) const { Avalonium::LogMessage(Avalonium::LogLevel::Info, message); }
        void LogWarning(std::string_view message) const { Avalonium::LogMessage(Avalonium::LogLevel::Warning, message); }
        void LogError(std::string_view message) const { Avalonium::LogMessage(Avalonium::LogLevel::Error, message); }

        template <class... Args>
        void Log(std::format_string<Args...> fmt, Args&&... args) const
        {
            Avalonium::LogMessage(Avalonium::LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
        }
    };
}
