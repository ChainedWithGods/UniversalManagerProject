#include "stdafx.h"
#ifdef UNIVERSAL_MANAGER_BUILD
#include "overlay/LuaOverlay.h"
#include "cppmods/CppModImGui.h"
#endif

using SetCursorPos_t = BOOL (WINAPI*)(int,int);
static SetCursorPos_t oSetCursorPos = nullptr;

static bool ShouldBlockCursorHooks()
{
    if (ImGui::GetCurrentContext() != nullptr)
    {
        if (menu::isOpen)
            return true;
#ifdef UNIVERSAL_MANAGER_BUILD
        if (LuaEngine::Overlay::LuaOverlayManager::Get().HasVisibleCustomWindows())
            return true;
        if (Avalonium::CppMods::ImGuiRender::AnyInputActive())
            return true;
#endif
    }

#ifdef UNIVERSAL_MANAGER_BUILD
    return runtimeinput::ShouldReleaseMouseCapture();
#else
    return false;
#endif
}

BOOL WINAPI hookSetCursorPos(int x,int y) {
    return ShouldBlockCursorHooks() ? TRUE : oSetCursorPos(x,y);
}

using ClipCursor_t = BOOL (WINAPI*)(const RECT*);
static ClipCursor_t oClipCursor = nullptr;
BOOL WINAPI hookClipCursor(const RECT* rect) {
    // A null rect releases the clip, including the one the overlay issues when
    // it frees the cursor. Blocking that call would leave an old game clip in
    // place for as long as the menu is open.
    if (rect == nullptr)
        return oClipCursor(nullptr);
    return ShouldBlockCursorHooks() ? TRUE : oClipCursor(rect);
}


namespace mousehooks {
    void Init() {
        HMODULE user32 = GetModuleHandleA("user32.dll");
        if (!user32)
            return;

        bool createdAny = false;
        if (auto addr = GetProcAddress(user32, "SetCursorPos")) {
            if (MH_CreateHook(addr, reinterpret_cast<LPVOID>(hookSetCursorPos), reinterpret_cast<LPVOID*>(&oSetCursorPos)) == MH_OK) {
                createdAny = true;
                DebugLog("[mousehooks] Hooked SetCursorPos@%p\n", addr);
            }
        }

        if (auto addr = GetProcAddress(user32, "ClipCursor")) {
            if (MH_CreateHook(addr, reinterpret_cast<LPVOID>(hookClipCursor), reinterpret_cast<LPVOID*>(&oClipCursor)) == MH_OK) {
                createdAny = true;
                DebugLog("[mousehooks] Hooked ClipCursor@%p\n", addr);
            }
        }

        if (createdAny) {
            MH_EnableHook(MH_ALL_HOOKS);
        }
    }

    void Remove() {
        HMODULE user32 = GetModuleHandleA("user32.dll");
        if (!user32)
            return;

        if (auto addr = GetProcAddress(user32, "SetCursorPos")) {
            MH_DisableHook(addr);
            MH_RemoveHook(addr);
        }

        if (auto addr = GetProcAddress(user32, "ClipCursor")) {
            MH_DisableHook(addr);
            MH_RemoveHook(addr);
        }
    }
}
