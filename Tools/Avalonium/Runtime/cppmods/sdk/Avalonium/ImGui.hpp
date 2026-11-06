#pragma once

#include "Api.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace RC::ImGui
{
    using RenderCallback = AvaloniumImGuiCallback;
    using AllocFunc = AvaloniumImGuiAlloc;
    using FreeFunc = AvaloniumImGuiFree;

    inline void* Context()
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        return (api && api->get_imgui_context) ? api->get_imgui_context() : nullptr;
    }

    inline bool Available()
    {
        return Context() != nullptr;
    }

    inline bool GetAllocatorFunctions(AllocFunc* alloc, FreeFunc* free, void** userData)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->get_imgui_allocator_functions) return false;
        api->get_imgui_allocator_functions(alloc, free, userData);
        return true;
    }

    inline uint32_t RegisterRender(std::string_view name, RenderCallback callback,
                                   void* userData = nullptr)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->register_imgui_render || !callback) return 0;
        return api->register_imgui_render(std::string(name).c_str(), callback, userData);
    }

    inline bool UnregisterRender(uint32_t id)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        return api && api->unregister_imgui_render && api->unregister_imgui_render(id);
    }

    inline bool SetInputActive(uint32_t id, bool active)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        return api && api->set_imgui_input_active && api->set_imgui_input_active(id, active);
    }
}
