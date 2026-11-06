#include "EngineDiscovery.h"
#include "EncryptedCache.h"
#include "RuntimeConfig.h"
#include "../utils/SafeMemory.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <psapi.h>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

struct EnumWindowsData {
  DWORD processId;
  HWND windowHandle;
};

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
  EnumWindowsData *data = (EnumWindowsData *)lParam;
  DWORD processId = 0;
  GetWindowThreadProcessId(hwnd, &processId);
  if (data->processId != processId)
    return TRUE;
  if (GetWindow(hwnd, GW_OWNER) != (HWND)0 || !IsWindowVisible(hwnd))
    return TRUE;
  data->windowHandle = hwnd;
  return FALSE;
}

namespace EngineDiscovery {

std::string GetMainWindowTitle() {
  EnumWindowsData data = {GetCurrentProcessId(), NULL};
  EnumWindows(EnumWindowsProc, (LPARAM)&data);
  if (data.windowHandle) {
    wchar_t title[512] = {};
    GetWindowTextW(data.windowHandle, title, sizeof(title) / sizeof(wchar_t) - 1);
    std::wstring clean;
    for (wchar_t c : std::wstring(title)) {
      if (wcschr(L"\\/:*?\"<>|", c) == nullptr)
        clean += c;
    }
    const size_t first = clean.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return {};
    const size_t last = clean.find_last_not_of(L" \t\r\n");
    const std::wstring trimmed = clean.substr(first, last - first + 1);

    const int n = WideCharToMultiByte(CP_UTF8, 0, trimmed.c_str(), (int)trimmed.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string result(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, trimmed.c_str(), (int)trimmed.size(), result.data(), n, nullptr, nullptr);
    return result;
  }
  return {};
}

bool FindGameWindow(EngineState &state) {
  EnumWindowsData data = {GetCurrentProcessId(), NULL};
  EnumWindows(EnumWindowsProc, (LPARAM)&data);
  if (data.windowHandle) {
    state.GameWindow = data.windowHandle;
    char buf[256];
    snprintf(buf, sizeof(buf), "[Discovery] Found game window: %p", state.GameWindow);
    WriteLogLine(buf);
    return true;
  }
  return false;
}

}

static void LogCoreClassMap(const EngineState &state) {
  Log("[Discovery] Core class map:");

  auto logEntry = [](const char *label, const void *value) {
    if (value)
      Log("[Discovery]   %-8s = %p", label, value);
    else
      Log("[Discovery]   %-8s = missing", label);
  };

  logEntry("Class", state.UClassClass);
  logEntry("Object", state.UObjectClass);
  logEntry("Actor", state.AActorClass);
  logEntry("Pawn", state.APawnClass);
  logEntry("Enum", state.UEnumClass);
  logEntry("Function", state.UFunctionClass);
  logEntry("Package", state.UPackageClass);
  logEntry("Struct", state.UStructClass);
  logEntry("World", state.UWorldClass);
}

namespace EngineDiscovery {

bool Initialize(EngineState &state, bool skipConsoleManager) {

  SetLogSection("discovery");

  std::string gameName = GetMainWindowTitle();
  if (gameName.empty()) {
    auto mainMod = PatternScan::GetMainModule();
    gameName = PatternScan::ExtractGameName(mainMod.name);
  }
  strncpy(state.GameName, gameName.c_str(), sizeof(state.GameName) - 1);
  state.GameName[sizeof(state.GameName) - 1] = '\0';
  Log("[Discovery] Game: %s", state.GameName);
  SetLogFact("Game", state.GameName);

  const auto &runtimeConfig = RuntimeConfig::Get();
  HMODULE hModule = GetModuleHandleA("Avalonium.dll");

  if (runtimeConfig.EnableDiscoveryCache && EncryptedCache::TryLoadSnapshot(state, hModule)) {
    char engine[32] = {};
    snprintf(engine, sizeof(engine), "UE%d.%d", state.UEMajor, state.UEMinor);
    Log("[Discovery] Detected %s (from cache)", engine);
    const bool hadConsoleManager = (state.IConsoleManager != nullptr);
    const bool hadKismetVM = (state.ProcessInternal != nullptr && state.ProcessLocalScriptFunction != nullptr);
    BuildFNameCache(state);
    FindEngineFunctions(state, skipConsoleManager);
    FindCoreClasses(state);
    BuildObjectIndex(state);
    RebuildFunctionCache(state);
    FindStaticConstructObject(state);
    if (state.bCoreClassesFound) {
      DiscoverWorldLayout(state);
    }
    if (state.bCoreClassesFound && state.UStruct_ChildrenOff >= 0) {
      FindProcessEvent(state);
    }
    if (!state.ProcessInternal) {
      FindProcessInternal(state);
    }
    if (!state.ProcessLocalScriptFunction) {
      FindProcessLocalScriptFunction(state);
    }
    FindGEngine(state);
    state.bInitialized = true;

    if ((!hadConsoleManager && state.IConsoleManager) ||
        (!hadKismetVM && state.ProcessInternal && state.ProcessLocalScriptFunction)) {
      Log("[Cache] Snapshot updated with newly resolved targets - refreshing snapshot...");
      EncryptedCache::SaveSnapshot(state, hModule);
    }
    SetLogFact("Engine build", engine);
    SetLogFact("Objects", std::to_string(GetObjectCount(state)));
    SetLogFact("Discovery source", "restored from cache");
    Log("[Discovery] Version: UE%d.%d", state.UEMajor, state.UEMinor);
    Log("[Discovery] GUObjectArray: %p", state.GUObjectArray);
    Log("[Discovery] GNames: %p", state.GNames);
    Log("[Discovery] GWorld: %p", state.GWorld);
    Log("[Discovery] GEngine: %p", state.GEngine);
    Log("[Discovery] Objects: %d", GetObjectCount(state));
    if (state.bCoreClassesFound) {
      LogCoreClassMap(state);
    }
    if (state.ProcessInternal) {
      Log("[Discovery] Kismet VM: ProcessInternal=%p, ProcessLocalScriptFunction=%p",
          state.ProcessInternal, state.ProcessLocalScriptFunction);
    } else {
      Log("[Discovery] Kismet VM: ProcessInternal unresolved");
    }
    return true;
  }

  if (!DetectVersion(state)) {
    snprintf(state.LastError, sizeof(state.LastError),
             "Failed to detect UE version");
    return false;
  }

  if (state.UEMajor == 4 && state.UEMinor < 23) {
    WriteLogLineEx(LogLevel::Error,
                   "[Avalonium] UNSUPPORTED ENGINE VERSION: Unreal "
                   "Engine%d.%d",
                   state.UEMajor, state.UEMinor);
    WriteLogLineEx(LogLevel::Error,
                   "[Avalonium] This game uses UE%d.%d; Avalonium requires "
                   "Unreal Engine 4.23 or newer (4.23-4.27, 5.0-5.8).",
                   state.UEMajor, state.UEMinor);
    WriteLogLineEx(LogLevel::Error,
                   "[Avalonium] Aborting to prevent undefined behavior or "
                   "crashes.");
    snprintf(state.LastError, sizeof(state.LastError),
             "Unsupported Unreal Engine version %d.%d (minimum: UE4.23)",
             state.UEMajor, state.UEMinor);
    state.bUnsupportedVersion = true;
    return false;
  }

  if (!FindGlobals(state)) {
    snprintf(state.LastError, sizeof(state.LastError),
             "Failed to find engine globals");
    return false;
  }

  if (!DiscoverOffsets(state)) {
    snprintf(state.LastError, sizeof(state.LastError),
             "Failed to discover memory layout");
    return false;
  }

  BuildFNameCache(state);

  FindEngineFunctions(state, skipConsoleManager);
  FindCoreClasses(state);

#pragma region debug-point backrooms-init-crash-post-core
  Log("[Discovery][Debug] Post-core init: entering retry DiscoverUStructLayout "
      "(ChildrenOff=0x%X ChildPropsOff=0x%X CoreClasses=%d)",
      state.UStruct_ChildrenOff, state.UStruct_ChildPropsOff,
      state.bCoreClassesFound ? 1 : 0);
#pragma endregion
  if ((state.UStruct_ChildrenOff < 0 || state.UStruct_ChildPropsOff < 0) &&
      state.bCoreClassesFound) {
    DiscoverUStructLayout(state);
  }
#pragma region debug-point backrooms-init-crash-post-core
  Log("[Discovery][Debug] Post-core init: after DiscoverUStructLayout "
      "(ChildrenOff=0x%X ChildPropsOff=0x%X SuperOff=0x%X UFieldNextOff=0x%X)",
      state.UStruct_ChildrenOff, state.UStruct_ChildPropsOff,
      state.UStruct_SuperOff, state.UField_NextOff);
#pragma endregion

#pragma region debug-point backrooms-init-crash-post-core
  Log("[Discovery][Debug] Post-core init: entering ApplyFFieldLayoutFixups "
      "(bUseFProperty=%d FFieldNameOff=0x%X AActorClass=%p)",
      state.bUseFProperty ? 1 : 0, state.FField_NameOff, state.AActorClass);
#pragma endregion
  ApplyFFieldLayoutFixups(state);
#pragma region debug-point backrooms-init-crash-post-core
  Log("[Discovery][Debug] Post-core init: after ApplyFFieldLayoutFixups "
      "(bUseFProperty=%d ChildPropsOff=0x%X FFieldNameOff=0x%X FFieldNextOff=0x%X)",
      state.bUseFProperty ? 1 : 0, state.UStruct_ChildPropsOff,
      state.FField_NameOff, state.FField_NextOff);
#pragma endregion
#pragma region debug-point backrooms-init-crash-post-core
  Log("[Discovery][Debug] Post-core init: entering "
      "DiscoverFPropertyOffsetInternal (bUseFProperty=%d ChildPropsOff=0x%X)",
      state.bUseFProperty ? 1 : 0, state.UStruct_ChildPropsOff);
#pragma endregion
  DiscoverFPropertyOffsetInternal(state);
#pragma region debug-point backrooms-init-crash-post-core
  Log("[Discovery][Debug] Post-core init: after DiscoverFPropertyOffsetInternal "
      "(FPropertyOffsetOff=0x%X UPropertyOffsetOff=0x%X)",
      state.FProperty_OffsetOff, state.UProperty_OffsetOff);
#pragma endregion

  ClearInheritanceCache();

  state.bInitialized = true;

  if (state.bCoreClassesFound) {
#pragma region debug-point backrooms-init-crash-post-core
    Log("[Discovery][Debug] Post-core init: entering DiscoverWorldLayout "
        "(GWorld=%p GWorldPtr=%p)", state.GWorld, state.GWorldPtr);
#pragma endregion
    DiscoverWorldLayout(state);
#pragma region debug-point backrooms-init-crash-post-core
    Log("[Discovery][Debug] Post-core init: after DiscoverWorldLayout "
        "(WorldLevelOff=0x%X LevelActorsOff=0x%X GWorld=%p)",
        state.UWorld_PersistentLevelOff, state.ULevel_ActorsOff, state.GWorld);
#pragma endregion
    Log("[Discovery] WorldLevelOff: 0x%X, LevelActorsOff: 0x%X",
        state.UWorld_PersistentLevelOff, state.ULevel_ActorsOff);
  }

  if (state.bCoreClassesFound && state.UStruct_ChildrenOff >= 0) {
#pragma region debug-point backrooms-init-crash-post-core
    Log("[Discovery][Debug] Post-core init: entering FindProcessEvent "
        "(ChildrenOff=0x%X FunctionFlagsOff=0x%X)",
        state.UStruct_ChildrenOff, state.UFunction_FlagsOff);
#pragma endregion
    if (FindProcessEvent(state)) {
      state.bFunctionsFound =
          true;
    } else {
      Log("[Discovery] ProcessEvent unresolved: VTable[%d], FunctionFlagsOff: "
          "0x%X",
          state.ProcessEvent_VTableIdx, state.UFunction_FlagsOff);
    }
#pragma region debug-point backrooms-init-crash-post-core
    Log("[Discovery][Debug] Post-core init: after FindProcessEvent "
        "(ProcessEventIdx=%d FunctionsFound=%d FunctionFlagsOff=0x%X)",
        state.ProcessEvent_VTableIdx, state.bFunctionsFound ? 1 : 0,
        state.UFunction_FlagsOff);
#pragma endregion
  }

#pragma region debug-point backrooms-init-crash-post-core
  Log("[Discovery][Debug] Post-core init: entering BuildObjectIndex");
#pragma endregion
  BuildObjectIndex(state);
#pragma region debug-point backrooms-init-crash-post-core
  Log("[Discovery][Debug] Post-core init: after BuildObjectIndex "
      "(IndexBuilt=%d NameCount=%zu ClassCount=%zu SortedNames=%zu)",
      state.bIndexBuilt ? 1 : 0, state.NameToFirstObject.size(),
      state.ClassToInstances.size(), state.SortedNames.size());
#pragma endregion

  RebuildFunctionCache(state);
  Log("[Discovery] Reflection caches: %zu UFunctions (%zu by full key, "
      "%zu by name only)",
      state.LiveUFunctions.size(), state.FunctionByFullKey.size(),
      state.FunctionByNameOnly.size());

  FindStaticConstructObject(state);

  FindProcessInternal(state);
  FindProcessLocalScriptFunction(state);

  FindGEngine(state);

  char engine[32] = {};
  snprintf(engine, sizeof(engine), "UE%d.%d", state.UEMajor, state.UEMinor);
  SetLogFact("Engine build", engine);
  SetLogFact("Objects", std::to_string(GetObjectCount(state)));
  Log("[Discovery] Version: UE%d.%d", state.UEMajor, state.UEMinor);
  Log("[Discovery] GUObjectArray: %p", state.GUObjectArray);
  Log("[Discovery] GNames: %p", state.GNames);
  Log("[Discovery] GWorld: %p", state.GWorld);
  Log("[Discovery] GEngine: %p", state.GEngine);
  Log("[Discovery] GMalloc: %p (allocator=%s, Malloc slot=%d)", state.GMalloc,
      state.GMallocName[0] ? state.GMallocName : "unknown",
      state.GMallocMallocSlot);
  Log("[Discovery] GNatives: %p", state.GNatives);
  Log("[Discovery] GUObjectHashTables: %p", state.GUObjectHashTables);
  Log("[Discovery] Objects: %d", GetObjectCount(state));
  Log("[Discovery] FName::ToString: %s",
      state.FNameToString ? "found" : "manual");
  if (state.bCoreClassesFound) {
    LogCoreClassMap(state);
  } else {
    Log("[Discovery] Core class map: unavailable");
  }
  if (state.ProcessEvent_VTableIdx >= 0) {
    Log("[Discovery] VTable: ProcessEvent[%d], FunctionFlags=0x%X",
        state.ProcessEvent_VTableIdx, state.UFunction_FlagsOff);
  } else {
    Log("[Discovery] VTable: ProcessEvent unresolved");
  }
  Log("[Discovery] SuperOff: 0x%X, ChildrenOff: 0x%X, UFieldNextOff: 0x%X",
      state.UStruct_SuperOff, state.UStruct_ChildrenOff, state.UField_NextOff);
  Log("[Discovery] ChildPropsOff: 0x%X, FFieldNameOff: 0x%X, FFieldNextOff: "
      "0x%X",
      state.UStruct_ChildPropsOff, state.FField_NameOff, state.FField_NextOff);
  if (state.ProcessInternal) {
    Log("[Discovery] Kismet VM: ProcessInternal=%p, ProcessLocalScriptFunction=%p",
        state.ProcessInternal, state.ProcessLocalScriptFunction);
  } else {
    Log("[Discovery] Kismet VM: ProcessInternal unresolved");
  }

  if (runtimeConfig.EnableDiscoveryCache) {
    EncryptedCache::SaveSnapshot(state, hModule);
  }

  return true;
}

}
