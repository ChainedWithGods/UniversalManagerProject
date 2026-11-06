#pragma once

#include "Api.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace RC::Native
{

    inline void* Module(std::string_view name = {})
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->get_module_handle) return nullptr;
        const std::string text(name);
        return api->get_module_handle(text.empty() ? nullptr : text.c_str());
    }

    inline void* FindExport(std::string_view symbol, void* module = nullptr)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->find_export || symbol.empty()) return nullptr;
        return api->find_export(module, std::string(symbol).c_str());
    }

    inline uint64_t FindPattern(std::string_view pattern, void* module = nullptr,
                                bool executableOnly = true)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->find_pattern || pattern.empty()) return 0;
        return api->find_pattern(module, std::string(pattern).c_str(), executableOnly);
    }

    inline uint64_t ResolveRelative(uint64_t instruction, int32_t displacementOffset,
                                    int32_t instructionLength)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->resolve_relative) return 0;
        return api->resolve_relative(instruction, displacementOffset, instructionLength);
    }

    inline void* GetVtable(void* object)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->get_vtable) return nullptr;
        return api->get_vtable(object);
    }

    inline void* GetVirtualFunction(void* object, int32_t slot)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->get_virtual_function) return nullptr;
        return api->get_virtual_function(object, slot);
    }

    template <typename Ret = void, typename... Args>
    inline Ret Call(uint64_t address, Args... args)
    {
        if (!address)
        {
            if constexpr (std::is_void_v<Ret>) return;
            else if constexpr (std::is_default_constructible_v<Ret>) return Ret();
        }
        using Fn = Ret(__fastcall*)(Args...);
        return reinterpret_cast<Fn>(static_cast<uintptr_t>(address))(args...);
    }

    template <typename Ret = void, typename... Args>
    inline Ret CallVirtual(void* object, int32_t slot, Args... args)
    {
        void* function = GetVirtualFunction(object, slot);
        if (!function)
        {
            if constexpr (std::is_void_v<Ret>) return;
            else if constexpr (std::is_default_constructible_v<Ret>) return Ret();
        }
        using Fn = Ret(__fastcall*)(void*, Args...);
        return reinterpret_cast<Fn>(function)(object, args...);
    }
}
