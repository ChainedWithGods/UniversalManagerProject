#include "OverlayDragMe.h"
#include "SystemMetrics.h"

#include "../../Helpers/Universal-Dear-ImGui-Hook/stdafx.h"
#include "../core/EngineDiscovery.h"
#include "../core/RuntimeConfig.h"
#include "../core/Types.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>
#include <string>
#include <vector>

extern EngineState g_State;

namespace {
std::atomic<bool> g_Started{ false };
std::atomic<bool> g_StopRequested{ false };
std::atomic<bool> g_MinhookInitialized{ false };
std::atomic<bool> g_UninjectInProgress{ false };
std::mutex g_LifecycleMutex;

using IsInitFn = bool(*)();
using LoadLibraryA_t = HMODULE(WINAPI*)(LPCSTR);
using LoadLibraryW_t = HMODULE(WINAPI*)(LPCWSTR);
using LoadLibraryExA_t = HMODULE(WINAPI*)(LPCSTR, HANDLE, DWORD);
using LoadLibraryExW_t = HMODULE(WINAPI*)(LPCWSTR, HANDLE, DWORD);

static LoadLibraryA_t g_OriginalLoadLibraryA = nullptr;
static LoadLibraryW_t g_OriginalLoadLibraryW = nullptr;
static LoadLibraryExA_t g_OriginalLoadLibraryExA = nullptr;
static LoadLibraryExW_t g_OriginalLoadLibraryExW = nullptr;

static void OverlayLog(const char* message)
{

    EngineDiscovery::WriteLogLine(message);
}

static void OverlayLogf(const char* format, ...)
{
    char buffer[512] = {};
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    OverlayLog(buffer);
}

static const char* BackendToString(globals::Backend backend)
{
    switch (backend)
    {
    case globals::Backend::DX11:   return "DX11";
    case globals::Backend::DX12:   return "DX12";
    default:                       return "None";
    }
}

static bool WaitForInitialization(IsInitFn fn, int attempts = 600, int sleepMs = 25, bool checkEarlyFallback = false, bool isDx11 = false)
{
    for (int i = 0; i < attempts && !g_StopRequested.load(); ++i)
    {
        if (fn && fn())
            return true;
        if (checkEarlyFallback && d3d12hook::ShouldFallbackImmediately())
            return false;
        if (isDx11 && hooks_dx11::ShouldFallbackImmediately())
            return false;
        Sleep(sleepMs);
    }
    return false;
}

static void LogBackendInitialized(const char* backendName)
{
    if (RuntimeConfig::Get().EnableInGameGui)
        OverlayLogf("[Avalonium][Overlay] %s initialized. Overlay hidden by default; press Insert or F5 to open.\n", backendName);
    else
        OverlayLogf("[Avalonium][Overlay] %s initialized.\n", backendName);
}

static bool TryInitBackend(globals::Backend backend)
{
    if (g_StopRequested.load())
        return false;

    switch (backend)
    {
    case globals::Backend::DX12:
        if (GetModuleHandleA("d3d12.dll") || GetModuleHandleA("dxgi.dll"))
        {
            OverlayLogf("[Avalonium][Overlay] Attempting %s hook.\n", BackendToString(backend));
            d3d12hook::PrepareForInit();
            hooks::Init();
            if (WaitForInitialization(d3d12hook::IsInitialized, 600, 25, true))
            {
                globals::activeBackend = globals::Backend::DX12;
                menu::isOpen = false;
                LogBackendInitialized("DX12");
                return true;
            }

            if (d3d12hook::ShouldFallbackImmediately())
            {
                OverlayLog("[Avalonium][Overlay] Non-DX12 swapchain detected; triggering immediate fallback to DX11.\n");
            }
            else
            {
                OverlayLog("[Avalonium][Overlay] DX12 did not initialize in time, trying fallback to DX11.\n");
            }

            d3d12hook::release();
        }
        break;

    case globals::Backend::DX11:
        if (GetModuleHandleA("d3d11.dll") || GetModuleHandleA("dxgi.dll"))
        {
            OverlayLogf("[Avalonium][Overlay] Attempting %s hook.\n", BackendToString(backend));
            hooks_dx11::PrepareForInit();
            hooks_dx11::Init();
            if (WaitForInitialization(hooks_dx11::IsInitialized, 600, 25, false, true))
            {
                globals::activeBackend = globals::Backend::DX11;
                menu::isOpen = false;
                LogBackendInitialized("DX11");
                return true;
            }

            if (hooks_dx11::ShouldFallbackImmediately())
            {
                OverlayLog("[Avalonium][Overlay] Non-DX11 swapchain detected; triggering immediate fallback to DX12.\n");
            }
            else
            {
                OverlayLog("[Avalonium][Overlay] DX11 did not initialize in time, trying fallback to DX12.\n");
            }

            hooks_dx11::release();
        }
        break;

    default:
        break;
    }

    return false;
}

static globals::Backend ParsePreferredBackend(const std::string& name)
{
    std::string s = name;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (s == "dx12" || s == "d3d12") return globals::Backend::DX12;
    if (s == "dx11" || s == "d3d11") return globals::Backend::DX11;
    return globals::Backend::None;
}

static bool TryInitializeLoadedBackends()
{
    if (globals::preferredBackend != globals::Backend::None)
        return TryInitBackend(globals::preferredBackend);

    std::vector<globals::Backend> order;
    if (GetModuleHandleA("d3d12.dll"))
    {
        order = {
            globals::Backend::DX12,
            globals::Backend::DX11,
        };
    }
    else
    {
        order = {
            globals::Backend::DX11,
            globals::Backend::DX12,
        };
    }

    for (globals::Backend backend : order)
    {
        if (TryInitBackend(backend))
            return true;
    }

    OverlayLog("[Avalonium][Overlay] No loaded rendering backend initialized yet; waiting for runtime module load.\n");
    return false;
}

static void ReleaseActiveBackend()
{
    switch (globals::activeBackend)
    {
    case globals::Backend::DX11:
        hooks_dx11::release();
        break;
    case globals::Backend::DX12:
        d3d12hook::release();
        break;
    default:
        break;
    }

    globals::activeBackend = globals::Backend::None;
}

static void InitForModule(const char* moduleName)
{
    if (!moduleName || g_StopRequested.load())
        return;

    const char* baseName = strrchr(moduleName, '\\');
    baseName = baseName ? baseName + 1 : moduleName;

    globals::Backend detected = globals::Backend::None;
    if (_stricmp(baseName, "d3d12.dll") == 0)
        detected = globals::Backend::DX12;
    else if (_stricmp(baseName, "d3d11.dll") == 0)
        detected = globals::Backend::DX11;
    else
        return;

    if (globals::preferredBackend != globals::Backend::None && detected != globals::preferredBackend)
        return;

    if (globals::activeBackend != globals::Backend::None)
    {
        OverlayLogf("[Avalonium][Overlay] Ignoring late load of %s because %s is already active.\n",
            BackendToString(detected), BackendToString(globals::activeBackend));
        return;
    }

    OverlayLogf("[Avalonium][Overlay] Detected runtime load of %s.\n", BackendToString(detected));
    TryInitBackend(detected);
}

static HMODULE WINAPI HookedLoadLibraryA(LPCSTR libraryName)
{
    HMODULE module = g_OriginalLoadLibraryA ? g_OriginalLoadLibraryA(libraryName) : LoadLibraryA(libraryName);
    if (module)
        InitForModule(libraryName);
    return module;
}

static HMODULE WINAPI HookedLoadLibraryW(LPCWSTR libraryName)
{
    HMODULE module = g_OriginalLoadLibraryW ? g_OriginalLoadLibraryW(libraryName) : LoadLibraryW(libraryName);
    if (module && libraryName)
    {
        char name[MAX_PATH] = {};
        WideCharToMultiByte(CP_ACP, 0, libraryName, -1, name, MAX_PATH, nullptr, nullptr);
        InitForModule(name);
    }
    return module;
}

static HMODULE WINAPI HookedLoadLibraryExA(LPCSTR libraryName, HANDLE fileHandle, DWORD flags)
{
    HMODULE module = g_OriginalLoadLibraryExA ? g_OriginalLoadLibraryExA(libraryName, fileHandle, flags) : LoadLibraryExA(libraryName, fileHandle, flags);
    if (module)
        InitForModule(libraryName);
    return module;
}

static HMODULE WINAPI HookedLoadLibraryExW(LPCWSTR libraryName, HANDLE fileHandle, DWORD flags)
{
    HMODULE module = g_OriginalLoadLibraryExW ? g_OriginalLoadLibraryExW(libraryName, fileHandle, flags) : LoadLibraryExW(libraryName, fileHandle, flags);
    if (module && libraryName)
    {
        char name[MAX_PATH] = {};
        WideCharToMultiByte(CP_ACP, 0, libraryName, -1, name, MAX_PATH, nullptr, nullptr);
        InitForModule(name);
    }
    return module;
}

static void TryInstallLoadLibraryHook(HMODULE module, const char* exportName, void* hook, void** original)
{
    if (!module)
        return;

    if (void* address = GetProcAddress(module, exportName))
    {
        const MH_STATUS createStatus = MH_CreateHook(address, hook, original);
        if (createStatus == MH_OK || createStatus == MH_ERROR_ALREADY_CREATED)
        {
            const MH_STATUS enableStatus = MH_EnableHook(address);
            if (enableStatus == MH_OK || enableStatus == MH_ERROR_ENABLED)
                OverlayLogf("[Avalonium][Overlay] Hooked %s at %p.\n", exportName, address);
        }
    }
}

static void InstallLoadLibraryHooks()
{
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (!kernel32)
        return;

    TryInstallLoadLibraryHook(kernel32, "LoadLibraryA", reinterpret_cast<void*>(&HookedLoadLibraryA), reinterpret_cast<void**>(&g_OriginalLoadLibraryA));
    TryInstallLoadLibraryHook(kernel32, "LoadLibraryW", reinterpret_cast<void*>(&HookedLoadLibraryW), reinterpret_cast<void**>(&g_OriginalLoadLibraryW));
    TryInstallLoadLibraryHook(kernel32, "LoadLibraryExA", reinterpret_cast<void*>(&HookedLoadLibraryExA), reinterpret_cast<void**>(&g_OriginalLoadLibraryExA));
    TryInstallLoadLibraryHook(kernel32, "LoadLibraryExW", reinterpret_cast<void*>(&HookedLoadLibraryExW), reinterpret_cast<void**>(&g_OriginalLoadLibraryExW));
}

static DWORD WINAPI OverlayInitThread(LPVOID)
{
    EngineDiscovery::SetLogThreadName("overlay");
    EngineDiscovery::BeginExclusiveLogSection("overlay");

    std::lock_guard<std::mutex> lock(g_LifecycleMutex);

    if (g_StopRequested.load()) {
        EngineDiscovery::EndLogSection("overlay");
        return 1;
    }

    globals::openMenuKey = VK_INSERT;
    globals::uninjectKey = VK_DELETE;
    globals::preferredBackend = ParsePreferredBackend(RuntimeConfig::Get().GraphicsBackend);
    globals::SetDebugLogging(RuntimeConfig::Get().VerboseLog);
    menu::isOpen = false;

    OverlayLog("[Avalonium][Overlay] Starting overlay bootstrap.\n");

    const MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
    {
        g_Started = false;
        OverlayLog("[Avalonium][Overlay] MinHook initialization failed.\n");
        EngineDiscovery::EndLogSection("overlay");
        return 1;
    }

    g_MinhookInitialized = true;

    const bool backendFound = TryInitializeLoadedBackends();
    if (!backendFound)
    {
        InstallLoadLibraryHooks();
    }
    mousehooks::Init();

    SystemMetrics::Initialize();

    OverlayLogf("[Avalonium][Overlay] Overlay bootstrap finished. Active backend=%s.\n",
        BackendToString(globals::activeBackend));

    EngineDiscovery::EndLogSection("overlay");
    return 0;
}

static void PrepareForGracefulDetach()
{
    OverlayLog("[Avalonium][Overlay] Manual detach requested; closing menu and restoring input state.\n");

    menu::isOpen = false;
    if (ImGui::GetCurrentContext())
        ImGui::GetIO().MouseDrawCursor = false;

    ReleaseCapture();
    ClipCursor(nullptr);

    if (globals::mainWindow && IsWindow(globals::mainWindow))
    {
        PostMessage(globals::mainWindow, WM_MOUSEMOVE, 0, 0);
        PostMessage(globals::mainWindow, WM_SETCURSOR, reinterpret_cast<WPARAM>(globals::mainWindow), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    }

    Sleep(150);
}

static DWORD WINAPI UninjectThread(LPVOID)
{
    EngineDiscovery::SetLogThreadName("uninject");
    EngineDiscovery::SetLogSection("overlay");

    PrepareForGracefulDetach();
    OverlayIntegration::Stop();
    if (globals::mainModule)
        FreeLibraryAndExitThread(globals::mainModule, 0);
    return 0;
}

}

void Uninject()
{
    if (!globals::mainModule)
        return;

    bool expected = false;
    if (!g_UninjectInProgress.compare_exchange_strong(expected, true))
        return;

    HANDLE thread = CreateThread(nullptr, 0, UninjectThread, nullptr, 0, nullptr);
    if (thread)
    {
        CloseHandle(thread);
    }
    else
    {
        g_UninjectInProgress = false;
    }
}

namespace OverlayIntegration {
void SetMainModule(HMODULE module)
{
    globals::mainModule = module;
}

bool Start()
{
    bool expected = false;
    if (!g_Started.compare_exchange_strong(expected, true))
        return true;

    g_StopRequested = false;
    globals::openMenuKey = VK_INSERT;

    HANDLE thread = CreateThread(nullptr, 0, OverlayInitThread, nullptr, 0, nullptr);
    if (!thread)
    {
        g_Started = false;
        OverlayLog("[Avalonium][Overlay] Failed to create overlay init thread.\n");
        return false;
    }

    WaitForSingleObject(thread, 2000);
    CloseHandle(thread);
    return true;
}

void Stop(bool processExiting)
{
    std::lock_guard<std::mutex> lock(g_LifecycleMutex);
    g_StopRequested = true;

    if (!g_Started.load() && !g_MinhookInitialized.load())
        return;

    OverlayLog("[Avalonium][Overlay] Stopping overlay.\n");

    if (!processExiting)
        ReleaseActiveBackend();
    else
        globals::activeBackend = globals::Backend::None;

    mousehooks::Remove();

    if (g_MinhookInitialized.exchange(false))
    {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_RemoveHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }

    g_OriginalLoadLibraryA = nullptr;
    g_OriginalLoadLibraryW = nullptr;
    g_OriginalLoadLibraryExA = nullptr;
    g_OriginalLoadLibraryExW = nullptr;
    g_Started = false;
}
}
