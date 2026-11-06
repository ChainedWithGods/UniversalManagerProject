#pragma once

#include "ModApi.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string_view>

namespace Avalonium
{
    namespace Detail
    {
        inline const AvaloniumModApi* ResolveApi()
        {
            HMODULE module = GetModuleHandleA("Avalonium.dll");
            if (!module)
            {
                OutputDebugStringA("[Avalonium] C++ mod API unavailable: Avalonium.dll is not loaded\n");
                return nullptr;
            }

            using GetApiFn = const AvaloniumModApi* (*)(uint32_t);
            auto getApi = reinterpret_cast<GetApiFn>(GetProcAddress(module, "AvaloniumGetModApi"));
            if (!getApi)
            {
                OutputDebugStringA("[Avalonium] C++ mod API unavailable: AvaloniumGetModApi is not exported\n");
                return nullptr;
            }

            const AvaloniumModApi* api = getApi(AVALONIUM_MOD_API_VERSION);
            if (!api)
            {
                OutputDebugStringA("[Avalonium] C++ mod API unavailable: the runtime does not answer this API version\n");
                return nullptr;
            }

            if (api->api_version < AVALONIUM_MOD_API_MIN_VERSION ||
                api->struct_size < sizeof(AvaloniumModApi))
            {
                OutputDebugStringA("[Avalonium] C++ mod API unavailable: the runtime's table is older or smaller than this SDK\n");
                return nullptr;
            }

            return api;
        }

        inline const AvaloniumModApi* Api()
        {
            static const AvaloniumModApi* api = ResolveApi();
            return api;
        }
    }

    inline bool Available()
    {
        return Detail::Api() != nullptr;
    }

    inline std::string_view RuntimeVersion()
    {
        const AvaloniumModApi* api = Detail::Api();
        return (api && api->get_runtime_version) ? std::string_view(api->get_runtime_version()) : std::string_view();
    }

    inline int32_t EngineMajor()
    {
        const AvaloniumModApi* api = Detail::Api();
        return (api && api->get_engine_major) ? api->get_engine_major() : 0;
    }

    inline int32_t EngineMinor()
    {
        const AvaloniumModApi* api = Detail::Api();
        return (api && api->get_engine_minor) ? api->get_engine_minor() : 0;
    }

    inline std::string_view GameName()
    {
        const AvaloniumModApi* api = Detail::Api();
        const char* name = (api && api->get_game_name) ? api->get_game_name() : nullptr;
        return name ? std::string_view(name) : std::string_view();
    }
}
