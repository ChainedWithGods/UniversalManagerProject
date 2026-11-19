#pragma once

#include "Api.hpp"
#include "Unreal.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace RC::Images
{

    inline Unreal::UObject* Load(std::string_view path)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->load_image || path.empty()) return nullptr;
        return static_cast<Unreal::UObject*>(api->load_image(std::string(path).c_str()));
    }

    inline Unreal::UObject* LoadFromMemory(const void* bytes, int32_t size)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->load_image_from_memory || !bytes || size <= 0) return nullptr;
        return static_cast<Unreal::UObject*>(api->load_image_from_memory(bytes, size));
    }

    inline void Release(Unreal::UObject* texture)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->release_asset || !texture) return;
        api->release_asset(Unreal::UObject::AddressOf(texture));
    }
}
