#pragma once

#include "../core/Types.h"
#include "../utils/InlineHook.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>

struct lua_State;

namespace LuaEngine {

struct LuaHookEntry {
    uint32_t HookId = 0;
    uint32_t PreId = 0;
    uint32_t PostId = 0;
    int PreCallbackRef = LUA_NOREF;
    int PostCallbackRef = LUA_NOREF;
    std::string FunctionName;
    bool IsCustomEvent = false;
    bool IsLoadMapHook = false;

    bool IsInitGameStateHook = false;

    bool IsBeginPlayEventHook = false;

    bool LoadMapNotify = false;

    bool AuthorityNotify = false;
};

struct PendingNameWatch {
    uint64_t FirstSeenTick = 0;
    bool Reported = false;
};

struct BeginPlayRideAlong {

    void* World = nullptr;

    void* Authority = nullptr;

    bool IsGameModeBeginningPlay = false;

    bool MatchedByAuthorityField = false;
    bool HasLoadMapEntry = false;
    bool HasGameModeEntry = false;
};

struct WorldNotice {
    void* World = nullptr;
    std::string WorldName;
    bool Recorded = false;
};

struct LuaTimerEntry {
    uint32_t TimerId = 0;
    int CallbackRef = LUA_NOREF;
    std::chrono::steady_clock::time_point NextTriggerTime;
    int64_t IntervalMs = 0;
    bool IsLoop = false;
    bool IsFrameBased = false;
    int64_t DelayFrames = 0;
    int64_t FramesRemaining = 0;
    bool Paused = false;
    bool Cancelled = false;
};

class LuaHookManager {
public:
    static LuaHookManager& Get();

    bool Initialize(lua_State* L);
    void Shutdown();

    uint32_t RegisterHook(const std::string& functionName, int preCallbackRef, int postCallbackRef = -1);
    bool UnregisterHook(uint32_t hookId);
    bool UnregisterHookByName(const std::string& functionName);

    void RegisterHookCompat(const std::string& functionName, int preCallbackRef, int postCallbackRef,
                            uint32_t* outPreId, uint32_t* outPostId, bool bLoadMapHook = false,
                            bool bInitGameStateHook = false);
    bool UnregisterHookById(uint32_t id);

    void RegisterCustomEvent(const std::string& eventName, int preCallbackRef, int postCallbackRef);
    bool UnregisterCustomEvent(const std::string& eventName, int preCallbackRef, int postCallbackRef);
    void ClearAllCustomEvents();

    void RegisterKeyBind(const std::string& keyName, int callbackRef);
    void RegisterKeyBindWithModifiers(const std::string& keyName, const std::vector<std::string>& modifierKeys, int callbackRef);
    bool IsKeyBindRegistered(const std::string& keyName);
    void OnKeyPress(const std::string& keyName);

    using FrameSink = void (*)(void* user);
    void SetFrameSink(FrameSink sink, void* user);

    using CppProcessEventSink = void (*)(void* object, void* function, void* params);
    void SetCppProcessEventSink(CppProcessEventSink sink);

    using CppScriptFunctionSink = void (*)(void* context, void* frame, void* result, bool pre);
    void SetCppScriptFunctionSink(CppScriptFunctionSink sink);

    using CppKeyBindCallback = void (*)(void* user);
    uint32_t RegisterCppKeyBind(const std::string& keyName, CppKeyBindCallback callback, void* user);
    bool UnregisterCppKeyBind(uint32_t id);

    void RegisterNewObjectListener(const std::string& className, int callbackRef);
    void UnregisterNewObjectListener(const std::string& className, int callbackRef);
    void CheckNewObjects();

    void RegisterConsoleCommandHandler(const std::string& commandName, int callbackRef);

    void RegisterGlobalConsoleHandler(int callbackRef, const std::string& commandName = std::string());
    bool ExecuteConsoleCommand(const std::string& commandStr);

    void RegisterExecLifecycleHook(const std::string& fnName, int preRef, int postRef,
                                   uint32_t* outPreId, uint32_t* outPostId);

    uint32_t RegisterBeginPlayHook(int preRef, int postRef);
    uint32_t RegisterEndPlayHook(int preRef, int postRef);
    uint32_t RegisterLoadMapHook(int preRef, int postRef);

    uint32_t RegisterInitGameStateHook(int preRef, int postRef);

    uint32_t MakeActionHandle();
    uint32_t ExecuteWithDelay(int64_t delayMs, int callbackRef);
    uint32_t ExecuteWithDelayUsingHandle(uint64_t userHandle, int64_t delayMs, int callbackRef, bool retriggerable);
    uint32_t LoopAsync(int64_t intervalMs, int callbackRef);
    uint32_t ExecuteAfterFrames(int64_t frames, int callbackRef);
    uint32_t LoopAfterFrames(int64_t frames, int callbackRef);
    bool CancelTimer(uint32_t timerId);
    bool PauseTimer(uint32_t timerId);
    bool UnpauseTimer(uint32_t timerId);
    bool ResetTimer(uint32_t timerId);
    bool SetTimerDelay(uint32_t timerId, int64_t newDelayMs);
    bool IsValidTimerHandle(uint32_t timerId);
    bool IsTimerActive(uint32_t timerId);
    bool IsTimerPaused(uint32_t timerId);
    int64_t GetTimerTimeRemaining(uint32_t timerId);
    int64_t GetTimerTimeElapsed(uint32_t timerId);
    int64_t GetTimerRate(uint32_t timerId);
    int CancelAllTimers();
    void ProcessTimers();

    void EnqueueGameThreadTask(int callbackRef);
    void ProcessGameThreadTasks();

    void ClearAllHooks();
    void ClearAllTimers();

    bool IsHookInstalled() const { return m_HookInstalled; }
    void PumpFallbackHeartbeat();

    void UpdateFrameTiming();
    double GetLastFrameTimeMs() const { return m_LastFrameTimeMs; }
    double GetFPS() const { return (m_SmoothedFPS > 0.0) ? m_SmoothedFPS : ((m_LastFrameTimeMs > 0.001) ? (1000.0 / m_LastFrameTimeMs) : 60.0); }
    double GetDeltaSeconds() const { return m_LastFrameTimeMs / 1000.0; }

    int64_t GetCurrentThreadId();
    int64_t GetMainModThreadId() const;
    int64_t GetGameThreadId() const;
    int64_t GetAsyncThreadId() const;
    bool IsInGameThread() const;
    bool IsInMainModThread() const;
    bool IsInAsyncThread() const;

    uint64_t GetFrameCount() const { return m_FrameCounter.load(std::memory_order_relaxed); }

private:
    LuaHookManager();
    ~LuaHookManager();

    static void __fastcall Hooked_ProcessEvent(void* object, void* ufunction, void* params);

    static void __fastcall Hooked_ProcessEvent_Actor(void* object, void* ufunction, void* params);
    static void __fastcall Hooked_ProcessLocalScriptFunction(void* context, void* frame, void* result);

    void DispatchScriptFunctionEntries(void* context, void* ufunction, void* frame);

    lua_State* LiveStateUnderLock() const;

    bool MayRunModCodeOnThisThread() const;

    lua_State* m_L = nullptr;
    bool m_HookInstalled = false;
    void* m_OriginalProcessEvent = nullptr;

    void* m_OriginalActorProcessEvent = nullptr;
    bool m_IsVTableHook = false;
    void* m_HookedTargetAddress = nullptr;

    bool m_ScriptHookInstalled = false;
    void* m_OriginalProcessLocalScriptFunction = nullptr;
    void* m_HookedProcessLocalScriptFunctionAddress = nullptr;

    Avalonium::InlineHook m_ProcessEventPatch;
    Avalonium::InlineHook m_ScriptPatch;

    uint32_t m_NextHookId = 1;

    uint32_t m_NextTimerId = 1;
    int m_LastObjectCount = 0;

    std::thread m_TimerThread;
    bool m_TimerThreadRunning = false;

    std::chrono::high_resolution_clock::time_point m_LastFrameTimestamp{};
    double m_LastFrameTimeMs = 16.666;
    double m_SmoothedFrameTimeMs = 16.666;
    double m_SmoothedFPS = 60.0;
    std::atomic<uint64_t> m_FrameCounter{ 0 };

    std::thread::id m_MainModThreadId{};
    std::thread::id m_AsyncThreadId{};
    std::thread::id m_GameThreadId{};
    bool m_GameThreadIdCaptured = false;

    std::unordered_map<void*, std::vector<LuaHookEntry>> m_FunctionHooks;
    std::unordered_map<std::string, std::vector<LuaHookEntry>> m_PendingNameHooks;
    std::unordered_map<std::string, std::vector<LuaHookEntry>> m_CustomEvents;

    std::unordered_set<int32_t> m_KnownNonEventNameIndexes;
    std::unordered_map<std::string, std::vector<int>> m_KeyBinds;
    struct CppKeyBind
    {
        uint32_t Id = 0;
        CppKeyBindCallback Callback = nullptr;
        void* User = nullptr;
    };
    std::unordered_map<std::string, std::vector<CppKeyBind>> m_CppKeyBinds;
    uint32_t m_NextCppKeyBindId = 1;
    std::atomic<FrameSink> m_FrameSink{nullptr};
    std::atomic<void*> m_FrameSinkUser{nullptr};
    std::atomic<CppProcessEventSink> m_CppProcessEventSink{nullptr};
    std::atomic<CppScriptFunctionSink> m_CppScriptFunctionSink{nullptr};
    std::unordered_map<std::string, std::vector<int>> m_NewObjectListeners;
    std::unordered_map<std::string, std::vector<int>> m_ConsoleCommandHandlers;

    std::unordered_map<uint32_t, WorldNotice> m_WorldNotices;

    void* m_BeginPlayAnchorFunction = nullptr;

    std::unordered_map<std::string, PendingNameWatch> m_PendingNameWatches;

    struct GlobalConsoleHandler {
        int CallbackRef = LUA_NOREF;
        std::string CommandName;
    };
    std::vector<GlobalConsoleHandler> m_GlobalConsoleHandlers;

    std::unordered_map<std::string, std::vector<LuaHookEntry>> m_ExecLifecycleCallbacks;
    bool FireExecLifecycleCallbacks(bool post, const std::string& commandStr);

    bool DispatchConsoleCommand(const std::string& commandStr, const std::string& baseCmd,
                                const std::string& args, const std::string& lowerBase);
    std::vector<int> m_GameThreadQueue;
    std::vector<LuaTimerEntry> m_Timers;
    void ResolvePendingHooks();

    BeginPlayRideAlong PrepareBeginPlayRideAlong(void* object, std::vector<LuaHookEntry>& entries);

    bool ClaimBeginPlayEntry(const LuaHookEntry& entry, void* object);

    struct BeginPlayAnswer {
        void* Object = nullptr;
        uint64_t EngineCall = 0;
    };
    std::unordered_map<uint32_t, BeginPlayAnswer> m_BeginPlayAnswers;

    std::atomic<bool> m_HasActiveHooks{ false };

    std::recursive_mutex m_HookMutex;
    std::mutex m_KeyBindMutex;
    std::mutex m_TimerMutex;
    std::mutex m_TaskMutex;
};

void RegisterLuaHookBindings(lua_State* L);
int ResolveVirtualKeyCode(const std::string& keyName);
}
