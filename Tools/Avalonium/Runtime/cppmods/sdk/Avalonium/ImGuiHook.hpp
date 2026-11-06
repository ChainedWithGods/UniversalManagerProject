#pragma once

#include "ImGui.hpp"

#include <imgui.h>

namespace RC::ImGui
{

    inline bool BeginFrame()
    {
        auto* context = static_cast<ImGuiContext*>(Context());
        if (!context) return false;
        ::ImGui::SetCurrentContext(context);

        AllocFunc alloc = nullptr;
        FreeFunc free = nullptr;
        void* userData = nullptr;
        if (GetAllocatorFunctions(&alloc, &free, &userData) && alloc && free)
        {
            ::ImGui::SetAllocatorFunctions(reinterpret_cast<ImGuiMemAllocFunc>(alloc),
                                         reinterpret_cast<ImGuiMemFreeFunc>(free), userData);
        }
        return true;
    }
}
