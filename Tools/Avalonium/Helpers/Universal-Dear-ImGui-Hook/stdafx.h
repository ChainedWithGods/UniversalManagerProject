#pragma once

#ifdef UNIVERSAL_MANAGER_BUILD
#include "../minhook/include/MinHook.h"
#else
#pragma comment(lib, "minhook/lib/libMinHook.x64.lib")
#include "minhook/include/MinHook.h"
#endif

#ifndef UNIVERSAL_MANAGER_BUILD
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3d12.lib")
#endif

#include <windows.h>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <cstdarg>
#include <map>
#include <unordered_map>
#include <string>
#include <Psapi.h>

#include <dxgi.h>
#include <dxgi1_4.h>
#include <d3d11.h>
#include <d3d12.h>

#include <wrl/client.h>

#if defined _M_X64
typedef uint64_t uintx_t;
#elif defined _M_IX86
typedef uint32_t uintx_t;
#endif

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_dx12.h"

#include "namespaces.h"

#ifdef UNIVERSAL_MANAGER_BUILD
namespace EngineDiscovery {
    std::string GetLogPath();
    bool IsVerboseLoggingEnabled();
    void WriteLogLine(const char* line);
}
#include "../../Runtime/core/RuntimeConfig.h"
#endif

inline bool WasToggleHotkeyPressed(int vk)
{
    if (vk <= 0)
        return false;

    static bool s_KeyWasDown[256] = {};
    if (vk < 256)
    {
        bool isDown = (GetAsyncKeyState(vk) & 0x8000) != 0;
        bool wasDown = s_KeyWasDown[vk];
        s_KeyWasDown[vk] = isDown;
        return isDown && !wasDown;
    }
    return (GetAsyncKeyState(vk) & 1) != 0;
}

inline bool WasUninjectHotkeyPressed()
{
    if (globals::uninjectKey <= 0)
        return false;

    // Detach is only allowed while the overlay is intentionally open.
    return menu::isOpen && WasToggleHotkeyPressed(globals::uninjectKey);
}

inline bool IsMenuToggleRequested()
{

    if (!RuntimeConfig::Get().EnableInGameGui)
        return false;

    if (WasToggleHotkeyPressed(VK_INSERT))
        return true;
    if (WasToggleHotkeyPressed(VK_F5))
        return true;
    if (globals::openMenuKey > 0 && globals::openMenuKey != VK_INSERT && globals::openMenuKey != VK_F5)
    {
        if (WasToggleHotkeyPressed(globals::openMenuKey))
            return true;
    }
    return false;
}


inline void DebugLog(const char* fmt, ...)
{
    if (!globals::enableDebugLog)
        return;

    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

#ifdef UNIVERSAL_MANAGER_BUILD
    EngineDiscovery::WriteLogLine(buf);
#else
    OutputDebugStringA(buf);
#endif
}

inline void ErrorLog(const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

#ifdef UNIVERSAL_MANAGER_BUILD
    EngineDiscovery::WriteLogLine(buf);
#else
    OutputDebugStringA(buf);
#endif
}



