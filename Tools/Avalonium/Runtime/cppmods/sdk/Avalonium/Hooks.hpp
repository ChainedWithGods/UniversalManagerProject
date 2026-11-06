#pragma once

#include "Api.hpp"
#include "Unreal.hpp"
#include "Value.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace RC::Hook
{
    using Unreal::Value;

    struct Context
    {
        Unreal::UObject* Object = nullptr;
        Unreal::UFunction* Function = nullptr;
        void* Frame = nullptr;
        void* Result = nullptr;

        template <class T>
        T GetArg(std::string_view parameterName) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->get_function_argument || !Function) return T{};

            AvaloniumValue raw{};
            if (!api->get_function_argument(Unreal::UObject::AddressOf(Function), Frame,
                                            std::string(parameterName).c_str(), &raw))
                return T{};
            const Value value = Value::FromRaw(raw);
            return Convert<T>(value);
        }

        template <class T>
        bool SetArg(std::string_view parameterName, const T& newValue) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->set_function_argument || !Function) return false;
            const Value value = Avalonium::ToValue(newValue);
            return api->set_function_argument(Unreal::UObject::AddressOf(Function), Frame,
                                              std::string(parameterName).c_str(), &value.Raw());
        }

        template <class T>
        T GetResult() const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->get_hook_result || !Function || !Result) return T{};

            AvaloniumValue raw{};
            if (!api->get_hook_result(Unreal::UObject::AddressOf(Function), Result, &raw)) return T{};
            const Value value = Value::FromRaw(raw);
            return Convert<T>(value);
        }

      private:
        template <class T>
        static T Convert(const Value& value)
        {
            if constexpr (std::is_pointer_v<T>)
            {
                using Pointee = std::remove_pointer_t<T>;
                if constexpr (std::is_same_v<Pointee, void> || std::is_base_of_v<Unreal::UObject, Pointee>)
                    return static_cast<T>(value.AsObject());
                else
                    return T{};
            }
            else
            {
                return value.As<T>();
            }
        }
    };

    using Callback = AvaloniumHookCallback;

    inline uint32_t Register(std::string_view className, std::string_view functionName,
                             Callback pre = nullptr, Callback post = nullptr, void* userData = nullptr)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->register_hook) return 0;
        return api->register_hook(std::string(className).c_str(), std::string(functionName).c_str(),
                                  pre, post, userData);
    }

    inline uint32_t RegisterNative(std::string_view className, std::string_view functionName,
                                   Callback pre = nullptr, Callback post = nullptr, void* userData = nullptr)
    {
        return Register(className, functionName, pre, post, userData);
    }

    inline uint32_t RegisterFunction(Unreal::UFunction* function, Callback pre = nullptr,
                                     Callback post = nullptr, void* userData = nullptr)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->register_hook_function || !function) return 0;
        return api->register_hook_function(Unreal::UObject::AddressOf(function), pre, post,
                                           userData);
    }

    inline bool Unregister(uint32_t hookId)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        return api && api->unregister_hook && api->unregister_hook(hookId);
    }
}
