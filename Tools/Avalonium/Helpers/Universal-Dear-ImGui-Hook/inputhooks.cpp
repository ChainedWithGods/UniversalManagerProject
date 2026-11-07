#include "stdafx.h"
#ifdef UNIVERSAL_MANAGER_BUILD
#include "overlay/LuaOverlay.h"
#include "cppmods/CppModImGui.h"
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace inputhook {
    // Maps each hooked window handle to its original WndProc.
    // Using a map instead of a single static variable ensures that:
    //   - Each window correctly forwards messages to its own original WndProc.
    //   - Remove() restores every hooked window individually.
    //   - No window is left pointing to unloaded DLL code after removal.
    static std::map<HWND, WNDPROC> sOriginalWndProcs;
#ifdef UNIVERSAL_MANAGER_BUILD
    static std::map<HWND, WPARAM> sSuppressedConsoleHotkeys;
#endif

    static bool IsMouseMessage(UINT uMsg)
    {
        switch (uMsg)
        {
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_XBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_SETCURSOR:
            return true;
        default:
            return false;
        }
    }

    static bool IsKeyboardMessage(UINT uMsg)
    {
        switch (uMsg)
        {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_IME_CHAR:
            return true;
        default:
            return false;
        }
    }

    static bool IsInitialKeyDown(UINT uMsg, LPARAM lParam)
    {
        if (uMsg != WM_KEYDOWN && uMsg != WM_SYSKEYDOWN)
            return false;

        return (lParam & 0x40000000) == 0;
    }

    static bool IsConsoleHotkey(WPARAM wParam, LPARAM lParam)
    {
        if (wParam == VK_F10)
            return true;

        const UINT scanCode = (static_cast<UINT>(lParam) >> 16) & 0xFF;
        if (scanCode == 41)
            return true;

        return wParam == VK_OEM_3 || wParam == VK_OEM_8;
    }

    static bool IsConsoleOpenHotkey(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        return IsInitialKeyDown(uMsg, lParam) && IsConsoleHotkey(wParam, lParam);
    }

    void Init(HWND hWindow)
    {
        // Skip if this window is already hooked
        if (sOriginalWndProcs.count(hWindow)) {
            return;
        }

        DebugLog("[inputhook] Initializing input hook for window %p\n", hWindow);


        // Store window globally for later use during release
        globals::mainWindow = hWindow;

        WNDPROC original = (WNDPROC)SetWindowLongPtr(hWindow, GWLP_WNDPROC, (LONG_PTR)WndProc);
        if (!original) {
            DebugLog("[inputhook] Failed to set WndProc for window %p: %d\n", hWindow, GetLastError());
        }
        else {
            sOriginalWndProcs[hWindow] = original;
            DebugLog("[inputhook] WndProc hook set for window %p. Original WndProc=%p\n", hWindow, original);
        }
    }

    void Remove(HWND hWindow)
    {
        auto it = sOriginalWndProcs.find(hWindow);
        if (it == sOriginalWndProcs.end()) {
            DebugLog("[inputhook] WndProc hook for window %p was already removed or never set.\n", hWindow);
            return;
        }

        DebugLog("[inputhook] Removing input hook for window %p\n", hWindow);
        SetLastError(ERROR_SUCCESS);
        if (SetWindowLongPtr(hWindow, GWLP_WNDPROC, (LONG_PTR)it->second) == 0) {
            const DWORD error = GetLastError();
            if (error != ERROR_SUCCESS && error != ERROR_INVALID_WINDOW_HANDLE)
                DebugLog("[inputhook] Failed to restore WndProc for window %p: %lu\n", hWindow, error);
        }
        else {
            DebugLog("[inputhook] WndProc restored to %p for window %p\n", it->second, hWindow);
        }

        sOriginalWndProcs.erase(it);
#ifdef UNIVERSAL_MANAGER_BUILD
        sSuppressedConsoleHotkeys.erase(hWindow);
#endif

        // Clear the global handle only if it matched this window
        if (globals::mainWindow == hWindow)
            globals::mainWindow = nullptr;
    }

    LRESULT APIENTRY WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        auto it = sOriginalWndProcs.find(hwnd);
        WNDPROC original = (it != sOriginalWndProcs.end()) ? it->second : nullptr;

#ifdef UNIVERSAL_MANAGER_BUILD
        if ((uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP) && IsConsoleHotkey(wParam, lParam))
        {
            auto suppressed = sSuppressedConsoleHotkeys.find(hwnd);
            if (suppressed != sSuppressedConsoleHotkeys.end() && suppressed->second == wParam)
            {
                sSuppressedConsoleHotkeys.erase(suppressed);
                return TRUE;
            }
        }

        if (!menu::isOpen && IsConsoleOpenHotkey(uMsg, wParam, lParam))
        {
            if (runtimeinput::IsConsoleEnabled())
            {
                if (runtimeinput::TryOpenConsole())
                {
                    sSuppressedConsoleHotkeys[hwnd] = wParam;
                    return TRUE;
                }
            }
            else
            {
                // Console is disabled - do NOT run console logic.
                // Pass to original WndProc so Lua keybinds (if registered) still receive it in GameWndProc,
                // which will swallow the key from native Unreal Engine.
                if (original)
                    return CallWindowProc(original, hwnd, uMsg, wParam, lParam);
                return TRUE;
            }
        }
#endif

        const bool hasImGuiContext = ImGui::GetCurrentContext() != nullptr;
        bool hasActiveGui = menu::isOpen;
#ifdef UNIVERSAL_MANAGER_BUILD
        hasActiveGui = hasActiveGui || LuaEngine::Overlay::LuaOverlayManager::Get().HasVisibleCustomWindows();
        hasActiveGui = hasActiveGui || Avalonium::CppMods::ImGuiRender::AnyInputActive();
#endif
        if (hasActiveGui && hasImGuiContext)
        {
            ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam);
            ImGuiIO& io = ImGui::GetIO();

            // Never swallow function keys (F1-F24) or hotkeys when not typing into an ImGui text input box,
            // so GameWndProc and Lua RegisterKeyBind always receive the key events
            if (IsKeyboardMessage(uMsg))
            {
                if ((wParam >= VK_F1 && wParam <= VK_F24) || !io.WantTextInput)
                {
                    if (original)
                        return CallWindowProc(original, hwnd, uMsg, wParam, lParam);
                    return 0;
                }
                if (io.WantCaptureKeyboard)
                    return TRUE;
            }

            if (IsMouseMessage(uMsg))
            {
                return TRUE;
            }
        }

        if (original)
            return CallWindowProc(original, hwnd, uMsg, wParam, lParam);
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}
