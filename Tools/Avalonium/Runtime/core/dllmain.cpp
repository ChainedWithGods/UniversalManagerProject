#include "../api/UEInternalAPI.h"
#include "EngineDiscovery.h"
#include "ConsoleEnabler.h"
#include "RuntimeConfig.h"
#include "../api/IOCPServer.h"
#include "../overlay/OverlayDragMe.h"
#include "../sdk/SDKValidation.h"
#include "../lua/LuaManager.h"
#include "../cppmods/CppModManager.h"
#include "../crypto/AESKeyFinder.h"
#include "../AggressiveDog/Launcher.h"
#include <cstdio>
#include <chrono>
#include <thread>

using Avalonium::CppMods::CppModManager;

extern EngineState g_State;

static IOCPServer* g_IOCPServer = nullptr;
static HANDLE g_InitThread = nullptr;
static HANDLE g_ConsoleThread = nullptr;
static volatile bool g_Initialized = false;
static bool g_ConsoleDone = false;
HMODULE g_Module = nullptr;

static void LogMsg(const char* msg) {
    EngineDiscovery::WriteLogLine(msg);
}

UE_API void SetLogPath(const char* path) {
    if (path) {
        EngineDiscovery::SetLogPath(path);
    }
}

static DWORD WINAPI InputAndConsoleThread(LPVOID ) {
    EngineDiscovery::SetLogThreadName("input");

    const RuntimeFeatureConfig& cfg = RuntimeConfig::Load(g_Module);
    bool prewarmed    = false;
    bool hookDone     = !cfg.EnableWndProcHook || (g_State.OriginalWndProc != nullptr);
    bool consoleDone  = !cfg.EnableUnrealConsole || g_ConsoleDone;

    constexpr int kMaxConsoleAttempts = 1800;

    for (int attempt = 1; attempt <= kMaxConsoleAttempts; ++attempt) {
        if (!g_State.bInitialized) {
            Sleep(50);
            continue;
        }

        if (!hookDone) {
            if (!g_State.GameWindow) {
                EngineDiscovery::FindGameWindow(g_State);
            }
            if (g_State.GameWindow) {
                ConsoleEnabler::InstallWndProcHook(g_State);
                hookDone = (g_State.OriginalWndProc != nullptr);
            }
        }

        if (!consoleDone) {

            if (!prewarmed) {
                ConsoleEnabler::Prewarm(g_State);
                prewarmed = true;
            }

            if (cfg.EnableUnrealConsole) {
                if (ConsoleEnabler::EnableInGameConsole(g_State)) {
                    char msg[128] = {};
                    snprintf(msg, sizeof(msg), "[ConsoleEnabler] Console enabled on attempt %d\n", attempt);
                    LogMsg(msg);
                    consoleDone = true;
                    g_ConsoleDone = true;
                    EngineDiscovery::EndLogSection("console");
                } else if (attempt == 1 || (attempt % 100) == 0) {
                    char msg[160] = {};
                    snprintf(msg, sizeof(msg), "[Avalonium] Console not ready yet, retrying (attempt %d/%d)\n", attempt, kMaxConsoleAttempts);
                    LogMsg(msg);
                }
            } else {

                consoleDone = true;
            }
        }

        if (hookDone && consoleDone) {
            if (!cfg.EnableUnrealConsole) {
                ConsoleEnabler::DisableInGameConsole(g_State);
            }
            return 0;
        }

        Sleep(100);
    }

    if (!hookDone) {
        LogMsg("[Avalonium] WndProc hook timed out — mod hotkeys will not work.\n");
    }
    if (cfg.EnableUnrealConsole && !consoleDone) {
        LogMsg("[Avalonium] Console enable timed out.\n");
    }
    return 1;
}

static DWORD WINAPI InitThread(LPVOID ) {
    EngineDiscovery::BeginLogSession();
    EngineDiscovery::SetLogThreadName("init");
    EngineDiscovery::SetLogSection("runtime");

    const RuntimeFeatureConfig& runtimeConfig = RuntimeConfig::Load(g_Module);

    const auto initStartTime = std::chrono::steady_clock::now();
    LogMsg("[Avalonium] Init thread started\n");

    const unsigned int delayMinutes = runtimeConfig.InitDelayMinutes;
    const unsigned int delaySeconds = runtimeConfig.InitDelaySeconds;
    if (delayMinutes > 0 || delaySeconds > 0) {
        char msg[160] = {};
        snprintf(msg, sizeof(msg),
                 "[Avalonium] InitDelay active: sleeping %um %us before initialization\n",
                 delayMinutes, delaySeconds);
        LogMsg(msg);

        unsigned int remaining = delayMinutes * 60u + delaySeconds;
        while (remaining > 0) {
            Sleep(1000);
            --remaining;
        }
        LogMsg("[Avalonium] InitDelay elapsed, starting initialization\n");
    }

    if (!runtimeConfig.EnableInGameGui) {
        LogMsg("[Avalonium] In-game GUI disabled by config\n");
    }

    const bool needConsoleManager =
        runtimeConfig.DumpAllDeveloperCommands || runtimeConfig.EnableLuaMods;
    bool ok = EngineDiscovery::Initialize(g_State, !needConsoleManager);

    if (ok) {
        char fact[160] = {};
        snprintf(fact, sizeof(fact), "succeeded in %lld ms",
                 static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - initStartTime).count()));
        EngineDiscovery::SetLogFact("Initialisation", fact);
        LogMsg("[Avalonium] Initialization successful\n");
        {
            const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now() - initStartTime)
                                       .count();
            char timingMsg[128] = {};
            snprintf(timingMsg, sizeof(timingMsg),
                     "[Avalonium] Initialization took %lldms\n",
                     static_cast<long long>(elapsedMs));
            LogMsg(timingMsg);
        }

        if (runtimeConfig.EnableSDKValidation) {
            const std::string& sdkPath = runtimeConfig.SDKPath;
            if (sdkPath.empty()) {
                Log("[Avalonium] SDKValidation=true but no SDKPath is set in ini - skipping validation\n");
            } else {
                Log("[Avalonium] SDK validation enabled (Path: %s)\n", sdkPath.c_str());
                SDKValidation::StartSDKValidationThread(g_State, sdkPath);
            }
        }

        const int serverPort = runtimeConfig.ServerPort > 0 ? static_cast<int>(runtimeConfig.ServerPort) : 20030;
        g_IOCPServer = new IOCPServer();
        if (g_IOCPServer->Start(serverPort)) {
            char msg[128] = {};
            snprintf(msg, sizeof(msg), "[Avalonium] IOCP server started on port %d\n", serverPort);
            LogMsg(msg);
        } else {
            LogMsg("[Avalonium] Failed to start IOCP server\n");
        }

        if (runtimeConfig.EnableWndProcHook) {
            EngineDiscovery::FindGameWindow(g_State);
            if (g_State.GameWindow) {
                ConsoleEnabler::InstallWndProcHook(g_State);
            }
        }

        if (runtimeConfig.AutoDiscoverAESKey) {
            Crypto::AESKeyFinder::Scan("");
        }

        bool consoleDone = !runtimeConfig.EnableUnrealConsole || g_ConsoleDone;
        if (runtimeConfig.EnableUnrealConsole && !consoleDone) {
            ConsoleEnabler::Prewarm(g_State);
            if (ConsoleEnabler::EnableInGameConsole(g_State)) {
                LogMsg("[ConsoleEnabler] Console enabled on attempt 1\n");
                EngineDiscovery::EndLogSection("console");
                consoleDone = true;
                g_ConsoleDone = true;
            }
        } else if (!runtimeConfig.EnableUnrealConsole) {
            LogMsg("[Avalonium] Unreal Engine console disabled by config\n");
        }

        const bool hookDone = !runtimeConfig.EnableWndProcHook || (g_State.OriginalWndProc != nullptr);
        if (!hookDone || !consoleDone) {
            g_ConsoleThread = CreateThread(nullptr, 0, InputAndConsoleThread, nullptr, 0, nullptr);
        }

        if (runtimeConfig.EnableInGameGui || runtimeConfig.EnableLuaMods ||
            runtimeConfig.EnableCppMods) {
            if (!runtimeConfig.EnableInGameGui && !runtimeConfig.EnableLuaMods) {
                LogMsg("[Overlay] Starting overlay bootstrap for native C++ mods\n");
            }
            OverlayIntegration::Start();
        }

        bool luaReady = false;
        if (runtimeConfig.EnableLuaMods) {
            EngineDiscovery::SetLogSection("lua");
            luaReady = LuaManager::Get().Initialize(g_Module);
        }

        if (runtimeConfig.EnableCppMods) {
            EngineDiscovery::SetLogSection("cppmods");
            if (CppModManager::Get().Initialize(g_Module)) {
                CppModManager::Get().LoadModsFromDirectory(runtimeConfig.CppModsPath);
            }
        }

        if (luaReady) {
            EngineDiscovery::SetLogSection("lua");
            std::string modsPath = runtimeConfig.LuaModsPath.empty()
                ? LuaManager::Get().GetModsDirectory()
                : runtimeConfig.LuaModsPath;
            LuaManager::Get().LoadModsFromDirectory(modsPath);
        }
    } else {
        char msg[1024];
        snprintf(msg, sizeof(msg), "[Avalonium] Initialization failed: %s\n", GetLastErrorMsg());
        EngineDiscovery::SetLogResult("FAILED", g_State.LastError);
        LogMsg(msg);

        if (!g_State.bUnsupportedVersion) {
            Sleep(5000);
            ok = EngineDiscovery::Initialize(g_State, !needConsoleManager);
            if (ok) {
                EngineDiscovery::SetLogResult("SUCCESS", "recovered on the retry attempt");
                LogMsg("[Avalonium] Retry successful\n");

                LogMsg("[Avalonium] Initialization successful\n");
                const int retryPort = runtimeConfig.ServerPort > 0 ? static_cast<int>(runtimeConfig.ServerPort) : 20030;
                g_IOCPServer = new IOCPServer();
                if (g_IOCPServer->Start(retryPort)) {
                    char retryMsg[128] = {};
                    snprintf(retryMsg, sizeof(retryMsg), "[Avalonium] IOCP server started on port %d\n", retryPort);
                    LogMsg(retryMsg);
                } else {
                    LogMsg("[Avalonium] Failed to start IOCP server\n");
                }

                if (runtimeConfig.EnableInGameGui || runtimeConfig.EnableLuaMods ||
                    runtimeConfig.EnableCppMods) {
                    if (!runtimeConfig.EnableInGameGui && !runtimeConfig.EnableLuaMods) {
                        LogMsg("[Overlay] Starting overlay bootstrap for native C++ mods\n");
                    }
                    OverlayIntegration::Start();
                }

                bool luaReady = false;
                if (runtimeConfig.EnableLuaMods) {
                    EngineDiscovery::SetLogSection("lua");
                    luaReady = LuaManager::Get().Initialize(g_Module);
                }

                if (runtimeConfig.EnableCppMods) {
                    EngineDiscovery::SetLogSection("cppmods");
                    if (CppModManager::Get().Initialize(g_Module)) {
                        CppModManager::Get().LoadModsFromDirectory(runtimeConfig.CppModsPath);
                    }
                }

                if (luaReady) {
                    EngineDiscovery::SetLogSection("lua");
                    std::string modsPath = runtimeConfig.LuaModsPath.empty()
                        ? LuaManager::Get().GetModsDirectory()
                        : runtimeConfig.LuaModsPath;
                    LuaManager::Get().LoadModsFromDirectory(modsPath);
                }
            }
        }
    }

    g_Initialized = true;
    return ok ? 0 : 1;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        g_Module = hModule;
        OverlayIntegration::SetMainModule(hModule);
        DisableThreadLibraryCalls(hModule);

        AggressiveDog::LaunchIfEnabled(hModule);

        g_InitThread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        break;

    case DLL_PROCESS_DETACH: {

        const bool processExiting = (lpReserved != nullptr);

        CppModManager::Get().Shutdown(processExiting);
        EngineDiscovery::EndLogSection("cppmods");

        LuaManager::Get().Shutdown();
        EngineDiscovery::EndLogSection("lua");

        if (g_IOCPServer) {
            g_IOCPServer->Stop();
            delete g_IOCPServer;
            g_IOCPServer = nullptr;
        }

        ConsoleEnabler::RemoveWndProcHook(g_State);
        OverlayIntegration::Stop(processExiting);
        Shutdown();

        if (g_InitThread) {
            WaitForSingleObject(g_InitThread, 3000);
            CloseHandle(g_InitThread);
            g_InitThread = nullptr;
        }

        if (g_ConsoleThread) {
            WaitForSingleObject(g_ConsoleThread, 3000);
            CloseHandle(g_ConsoleThread);
            g_ConsoleThread = nullptr;
        }

        EngineDiscovery::EndLogSession("process detached");
        break;
    }
    }
    return TRUE;
}
