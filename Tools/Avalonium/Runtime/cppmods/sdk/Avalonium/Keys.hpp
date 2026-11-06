#pragma once

#include "Api.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace RC::Keys
{
    using Callback = AvaloniumKeyCallback;

    inline uint32_t Register(std::string_view keyName, Callback callback, void* userData = nullptr)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->register_key_bind || !callback) return 0;
        return api->register_key_bind(std::string(keyName).c_str(), callback, userData);
    }

    inline bool Unregister(uint32_t keyId)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        return api && api->unregister_key_bind && api->unregister_key_bind(keyId);
    }
}
