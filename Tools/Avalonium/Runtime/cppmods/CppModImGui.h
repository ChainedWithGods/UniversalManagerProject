#pragma once

#include "sdk/Avalonium/ModApi.h"

#include <cstdint>

namespace Avalonium::CppMods::ImGuiRender
{
    uint32_t Register(const char* name, AvaloniumImGuiCallback callback, void* userData);
    bool Unregister(uint32_t id);
    void UnregisterOwner(void* owner);

    bool SetInputActive(uint32_t id, bool active);
    bool AnyInputActive();

    void RenderAll();

    bool WaitForIdle(uint32_t milliseconds);

    void Shutdown();
}
