#pragma once

#include "Api.hpp"

#include <cstdint>

namespace RC
{
    using GameThreadTask = AvaloniumTaskCallback;

    inline bool IsInGameThread()
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        return api && api->is_in_game_thread && api->is_in_game_thread();
    }

    inline bool RunOnGameThread(GameThreadTask callback, void* userData = nullptr)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->run_on_game_thread || !callback) return false;
        return api->run_on_game_thread(callback, userData);
    }

    inline double GetDeltaSeconds()
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        return (api && api->get_delta_seconds) ? api->get_delta_seconds() : 0.0;
    }

    inline uint64_t GetFrameCount()
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        return (api && api->get_frame_count) ? api->get_frame_count() : 0;
    }
}
