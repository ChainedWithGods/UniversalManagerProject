#include "ConsoleInternal.h"
#include "../lua/LuaHookManager.h"
#include "../lua/LuaManager.h"
#include "../core/BlueprintDiscovery.h"
#include "../core/RuntimeConfig.h"
#include "SeamlessTravelGuard.h"

extern HMODULE g_Module;

namespace ConsoleEnabler {

UObject *g_CachedConsoleObject = nullptr;
int32_t g_ConsoleStateOffset = -2;
int32_t g_NameTypingIndex = -2;
int32_t g_NameOpenIndex = -2;
void *g_FakeGotoStateFunction = nullptr;
bool g_TriedResolveFakeGotoStateFunction = false;
bool g_LoggedViewportConsoleFailureDetails = false;
bool g_LoggedConsoleStateFailureDetails = false;
bool g_LoggedFakeGotoStateFailureDetails = false;
bool g_LoggedOpenWithoutStateValidation = false;
UObject *g_CachedCheatManagerOwner = nullptr;
UClass *g_CachedConsoleClass = nullptr;
UClass *g_CachedGameEngineClass = nullptr;
UClass *g_CachedEngineClass = nullptr;
UClass *g_CachedGameViewportClientClass = nullptr;
UClass *g_CachedLocalPlayerClass = nullptr;
UClass *g_CachedGameplayStaticsClass = nullptr;

bool SetConsoleTargetPlayer(const EngineState &state, UObject *consoleObject) {
  if (!consoleObject) return false;

  UClass *consoleClass = EngineDiscovery::GetObjectClass(state, consoleObject);
  if (!consoleClass) return false;

  const int32_t targetPlayerOffset = FindPropertyOffset(state, (UStruct *)consoleClass, "ConsoleTargetPlayer");
  if (targetPlayerOffset < 0) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] ConsoleTargetPlayer property not found on UConsole.");
    return false;
  }

  void **targetPlayerPtr = (void **)((uint8_t *)consoleObject + targetPlayerOffset);
  UObject *existingTargetPlayer = targetPlayerPtr ? reinterpret_cast<UObject *>(*targetPlayerPtr) : nullptr;

  UObject *localPlayer = FindFirstLocalPlayer(state);
  if (!localPlayer) {
    if (!existingTargetPlayer) {
      EngineDiscovery::WriteLogLine("[ConsoleEnabler] No live LocalPlayer found for ConsoleTargetPlayer.");
    }
    return existingTargetPlayer != nullptr;
  }

  if (existingTargetPlayer == localPlayer) {
    return true;
  }

  if (!targetPlayerPtr ||
      !SafeMem_Write(targetPlayerPtr, &localPlayer, sizeof(localPlayer))) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] Failed to set ConsoleTargetPlayer.");
    return false;
  }

  char buffer[512] = {};
  snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] ConsoleTargetPlayer set to %s",
           EngineDiscovery::BuildFullName(state, localPlayer).c_str());
  EngineDiscovery::WriteLogLine(buffer);
  return true;
}

UClass *FindUniqueCheatManagerSubclass(const EngineState &state, UClass *baseClass) {
  if (!baseClass) return nullptr;

  UClass *match = nullptr;
  const int32_t count = EngineDiscovery::GetObjectCount(state);
  for (int32_t i = 0; i < count; ++i) {
    UObject *object = EngineDiscovery::GetObjectByIndex(state, i);
    if (!object || !IsClassObject(state, object)) continue;
    UClass *candidate = (UClass *)object;
    if (candidate == baseClass) continue;
    if (!EngineDiscovery::IsStructA(state, (UStruct *)candidate, (UStruct *)baseClass)) continue;
    if (state.UClass_ClassFlagsOff >= 0) {
      uint32_t classFlags = 0;
      if (SafeMem_Read((uint8_t *)candidate + state.UClass_ClassFlagsOff, &classFlags, 4) &&
          (classFlags & 0x1u) != 0) {
        continue;
      }
    }
    if (match) {
      EngineDiscovery::WriteLogLine(
          "[ConsoleEnabler] Several CheatManager subclasses are loaded; keeping the base class.");
      return nullptr;
    }
    match = candidate;
  }

  if (match) {
    char buffer[512] = {};
    snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] Found CheatManager subclass: %s",
             EngineDiscovery::BuildFullName(state, (UObject *)match).c_str());
    EngineDiscovery::WriteLogLine(buffer);
  }
  return match;
}

UClass *ResolveCheatManagerClass(const EngineState &state, UObject *playerController) {
  UClass *baseClass = FindClassByNameExact(state, "CheatManager");

  UClass *pcClass = EngineDiscovery::GetObjectClass(state, playerController);
  if (pcClass) {
    const int32_t cheatClassOffset = FindPropertyOffset(state, (UStruct *)pcClass, "CheatClass");
    if (cheatClassOffset >= 0) {
      void *declaredClass = *(void **)((uint8_t *)playerController + cheatClassOffset);
      if (declaredClass && declaredClass != (void *)baseClass && IsClassObject(state, (UObject *)declaredClass)) {
        char buffer[512] = {};
        snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] PlayerController CheatClass: %s",
                 EngineDiscovery::BuildFullName(state, (UObject *)declaredClass).c_str());
        EngineDiscovery::WriteLogLine(buffer);
        return (UClass *)declaredClass;
      }
    }
  }

  UClass *subclass = FindUniqueCheatManagerSubclass(state, baseClass);
  if (subclass) return subclass;
  return baseClass;
}

void AttachCheatManager(const EngineState &state) {
  UObject *playerController = FindPlayerController(state);
  if (!playerController) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] PlayerController not found, cannot attach CheatManager.");
    return;
  }

  UClass *pcClass = EngineDiscovery::GetObjectClass(state, playerController);
  if (!pcClass) return;

  const int32_t cheatManagerOffset = FindPropertyOffset(state, (UStruct *)pcClass, "CheatManager");
  if (cheatManagerOffset < 0) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] CheatManager property not found on PlayerController.");
    return;
  }

  void **cheatManagerPtr = (void **)((uint8_t *)playerController + cheatManagerOffset);
  UObject *existingCheatManager = cheatManagerPtr ? reinterpret_cast<UObject *>(*cheatManagerPtr) : nullptr;

  if (existingCheatManager) {
    if (g_CachedCheatManagerOwner != playerController) {
      g_CachedCheatManagerOwner = playerController;
      char buffer[512] = {};
      snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] Adopted existing CheatManager on %s",
               EngineDiscovery::BuildFullName(state, playerController).c_str());
      EngineDiscovery::WriteLogLine(buffer);
    }
    return;
  }

  UClass *cheatManagerClass = ResolveCheatManagerClass(state, playerController);
  if (!cheatManagerClass) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] CheatManager class not found.");
    return;
  }

  UObject *createdCheatManager = nullptr;

  if (state.StaticConstructObject) {
    createdCheatManager = TrySEHConstruct(state, cheatManagerClass, playerController);
    if (createdCheatManager) {
      EngineDiscovery::WriteLogLine("[ConsoleEnabler] Successfully spawned CheatManager via Direct Native Instantiation.");
    }
  }

  if (!createdCheatManager) {
    const ConstructHelperInfo constructHelper = FindConstructHelper(state);
    if (constructHelper.Function && constructHelper.OwnerClass) {
      UObject *helperCDO = FindFirstObjectOfClass(state, constructHelper.OwnerClass, true);
      if (helperCDO) {
        FSpawnObjectParams params = {};
        params.ObjectClass = cheatManagerClass;
        params.Outer = playerController;

        if (EngineDiscovery::CallProcessEvent(state, helperCDO, constructHelper.Function, &params) && params.ReturnValue) {
          createdCheatManager = params.ReturnValue;
          EngineDiscovery::WriteLogLine("[ConsoleEnabler] Successfully spawned CheatManager via construct helper.");
        }
      }
    }
  }

  if (!createdCheatManager) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] Failed to create CheatManager.");
    return;
  }

  *cheatManagerPtr = createdCheatManager;
  g_CachedCheatManagerOwner = playerController;

  char buffer[512] = {};
  snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] Created and attached CheatManager to %s",
           EngineDiscovery::BuildFullName(state, playerController).c_str());
  EngineDiscovery::WriteLogLine(buffer);

  UClass *cmClass = EngineDiscovery::GetObjectClass(state, createdCheatManager);
  if (cmClass) {
    void *initFunc = FindFunctionByNameLoose(state, (UStruct *)cmClass, "InitCheatManager");
    if (initFunc) {
      EngineDiscovery::CallProcessEvent(state, createdCheatManager, initFunc, nullptr);
      EngineDiscovery::WriteLogLine("[ConsoleEnabler] InitCheatManager called.");
    }
  }
}

void ReattachCheatManagerIfNeeded(const EngineState &state) {
  SeamlessTravelGuard::Install(g_State);

  UObject *playerController = FindPlayerController(state);
  if (!playerController) return;

  if (playerController == g_CachedCheatManagerOwner) return;

  EngineDiscovery::BeginExclusiveLogSection("level_transition");
  EngineDiscovery::WriteLogLine("[ConsoleEnabler] PlayerController changed (level transition?), re-attaching CheatManager.");
  AttachCheatManager(state);
  EngineDiscovery::EndLogSection("level_transition");
}

bool WriteConsoleKeySlot(FKeyEntry *keyArray, int32_t slot, const FName &keyName, bool initializeSlot) {
  if (!keyArray || slot < 0) return false;

  FKeyEntry *entry = keyArray + slot;
  if (initializeSlot) {

    memset(entry, 0, sizeof(*entry));
  }
  entry->KeyName = keyName;
  return true;
}

bool IsLikelyValidConsoleKeyArray(const TArray<FKeyEntry> &keyArray) {
  if (keyArray.ArrayNum < 0 || keyArray.ArrayMax < 0) return false;
  if (keyArray.ArrayNum > keyArray.ArrayMax) return false;
  if (keyArray.ArrayMax > 64) return false;

  if (keyArray.ArrayMax == 0) {
    return keyArray.Data == nullptr;
  }

  if (!keyArray.Data) return false;

  return SafeMem_IsReadable(keyArray.Data, sizeof(FKeyEntry) * keyArray.ArrayMax) != 0;
}

void LogConsoleKeySlots(const EngineState &state, const TArray<FKeyEntry> &keyArray, const char *prefix) {
  if (!keyArray.Data || keyArray.ArrayNum <= 0) {
    EngineDiscovery::WriteLogLine(prefix);
    return;
  }

  const int32_t toLog = keyArray.ArrayNum < 4 ? keyArray.ArrayNum : 4;
  for (int32_t i = 0; i < toLog; ++i) {
    const std::string keyName = EngineDiscovery::FNameToStr(state, keyArray.Data[i].KeyName);
    char buffer[256] = {};
    snprintf(buffer, sizeof(buffer), "%s[%d]: %s", prefix, i, keyName.empty() ? "<empty>" : keyName.c_str());
    EngineDiscovery::WriteLogLine(buffer);
  }
}

UObject *SpawnInputSettingsInstance(const EngineState &state, UClass *inputSettingsClass, UObject *outer) {
  if (state.StaticConstructObject) {
    if (UObject *created = TrySEHConstruct(state, inputSettingsClass, outer, {}, kRfTransient)) {
      return created;
    }
  }

  const ConstructHelperInfo constructHelper = FindConstructHelper(state);
  if (constructHelper.Function && constructHelper.OwnerClass) {
    UObject *helperCDO = FindFirstObjectOfClass(state, constructHelper.OwnerClass, true);
    if (helperCDO) {
      FSpawnObjectParams params = {};
      params.ObjectClass = inputSettingsClass;
      params.Outer = outer;
      if (EngineDiscovery::CallProcessEvent(state, helperCDO, constructHelper.Function, &params) && params.ReturnValue) {
        return params.ReturnValue;
      }
    }
  }

  return nullptr;
}

UObject *EnsureInputSettingsDefaultObject(const EngineState &state, UClass *inputSettingsClass) {
  UObject *cdo = EngineDiscovery::FindClassDefaultObject(
      const_cast<EngineState &>(state), inputSettingsClass);
  if (cdo) {
    if (!FindObjectByNameExact(state, "Default__InputSettings")) {
      EngineDiscovery::WriteLogLine("[ConsoleEnabler] InputSettings CDO resolved through its class; the name index held no Default__InputSettings entry.");
    }
    return cdo;
  }

  UObject *classOuter = EngineDiscovery::GetObjectOuter(state, (UObject *)inputSettingsClass);
  (void)SpawnInputSettingsInstance(state, inputSettingsClass, classOuter);

  cdo = EngineDiscovery::FindClassDefaultObject(
      const_cast<EngineState &>(state), inputSettingsClass);
  if (!cdo) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] InputSettings CDO could not be created; console keys unchanged.");
    return nullptr;
  }

  EngineDiscovery::WriteLogLine("[ConsoleEnabler] InputSettings had no CDO in this build; created it on demand.");
  return cdo;
}

void RemapConsoleKeys(const EngineState &state) {
  UClass *inputSettingsClass = FindClassByNameExact(state, "InputSettings");
  if (!inputSettingsClass) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] InputSettings class not found; console keys unchanged.");
    return;
  }

  UObject *inputSettings = EnsureInputSettingsDefaultObject(state, inputSettingsClass);
  if (!inputSettings) return;

  static PropertyLookupResult consoleKeysProperty = {};
  static bool consoleKeysCached = false;
  if (!consoleKeysCached) {
    consoleKeysProperty = FindPropertyInfo(state, (UStruct *)inputSettingsClass, "ConsoleKeys");
    consoleKeysCached = true;
  }
  if (consoleKeysProperty.Offset < 0) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] ConsoleKeys property not found on InputSettings.");
    return;
  }

  if (!consoleKeysProperty.TypeName.empty() && consoleKeysProperty.TypeName != "ArrayProperty") {
    char buffer[256] = {};
    snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] ConsoleKeys resolved to unexpected property type '%s'; skipping remap.", consoleKeysProperty.TypeName.c_str());
    EngineDiscovery::WriteLogLine(buffer);
    return;
  }

  if (!consoleKeysProperty.InnerTypeName.empty() && consoleKeysProperty.InnerTypeName != "StructProperty") {
    char buffer[256] = {};
    snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] ConsoleKeys inner property type is '%s' instead of StructProperty; skipping remap.", consoleKeysProperty.InnerTypeName.c_str());
    EngineDiscovery::WriteLogLine(buffer);
    return;
  }

  if (!consoleKeysProperty.StructTypeName.empty() && consoleKeysProperty.StructTypeName != "Key") {
    char buffer[256] = {};
    snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] ConsoleKeys struct type resolved to '%s' instead of Key; skipping remap.", consoleKeysProperty.StructTypeName.c_str());
    EngineDiscovery::WriteLogLine(buffer);
    return;
  }

  auto *consoleKeys = (TArray<FKeyEntry> *)((uint8_t *)inputSettings + consoleKeysProperty.Offset);
  if (!consoleKeys) return;

  if (!IsLikelyValidConsoleKeyArray(*consoleKeys)) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] ConsoleKeys layout/header looks invalid for this engine build; skipping remap.");
    return;
  }

  if (!consoleKeys->Data || consoleKeys->ArrayMax <= 0) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] ConsoleKeys has no writable storage; skipping remap.");
    return;
  }

  const int32_t tildeIndex = ResolveCachedFNameIndex(state, "Tilde");
  const int32_t f10Index = ResolveCachedFNameIndex(state, "F10");
  if (tildeIndex < 0 && f10Index < 0) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] Failed to resolve FName indices for console keys.");
    return;
  }

  bool changed = false;
  if (consoleKeys->ArrayMax == 1) {
    FName singleKey = {};
    singleKey.ComparisonIndex = (f10Index >= 0) ? f10Index : tildeIndex;
    singleKey.Number = 0;
    changed |= WriteConsoleKeySlot(consoleKeys->Data, 0, singleKey, consoleKeys->ArrayNum <= 0);
    if (consoleKeys->ArrayNum < 1) {
      consoleKeys->ArrayNum = 1;
    }
  } else {
    if (tildeIndex >= 0) {
      FName tildeKey = {};
      tildeKey.ComparisonIndex = tildeIndex;
      tildeKey.Number = 0;
      changed |= WriteConsoleKeySlot(consoleKeys->Data, 0, tildeKey, consoleKeys->ArrayNum <= 0);
      if (consoleKeys->ArrayNum < 1) {
        consoleKeys->ArrayNum = 1;
      }
    }

    if (f10Index >= 0 && consoleKeys->ArrayMax >= 2) {
      FName f10Key = {};
      f10Key.ComparisonIndex = f10Index;
      f10Key.Number = 0;
      changed |= WriteConsoleKeySlot(consoleKeys->Data, 1, f10Key, consoleKeys->ArrayNum <= 1);
      if (consoleKeys->ArrayNum < 2) {
        consoleKeys->ArrayNum = 2;
      }
    }
  }

  if (!changed) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] ConsoleKeys remap made no changes.");
    return;
  }

  EngineDiscovery::WriteLogLine("[ConsoleEnabler] Console hotkeys mapped: Tilde and F10.");
}

void DisableInGameConsole(const EngineState &state) {

  UObject *engineObject = FindLiveEngineObject(state, false);
  if (engineObject) {
    UObject *gameViewport = ResolveGameViewport(state, engineObject);
    if (gameViewport) {
      int32_t viewportConsoleOffset = FindViewportConsoleOffset(state, gameViewport);
      if (viewportConsoleOffset >= 0) {
        void **viewportConsolePtr = reinterpret_cast<void **>((uint8_t *)gameViewport + viewportConsoleOffset);
        if (viewportConsolePtr && *viewportConsolePtr) {
          *viewportConsolePtr = nullptr;
          EngineDiscovery::WriteLogLine("[ConsoleEnabler] Detached ViewportConsole from GameViewportClient (UnrealEngineConsole=false).");
        }
      }
    }
  }
}

int32_t ScanForConsoleStateOffset(const EngineState &state, UObject *consoleObject) {
  (void)state;
  (void)consoleObject;

  return -1;
}

int32_t ResolveConsoleStateOffset(const EngineState &state, UObject *consoleObject) {
  if (g_ConsoleStateOffset != -2) {
    return g_ConsoleStateOffset;
  }

  if (!consoleObject) {
    g_ConsoleStateOffset = -1;
    return g_ConsoleStateOffset;
  }

  UClass *consoleClass = EngineDiscovery::GetObjectClass(state, consoleObject);
  if (!consoleClass) {
    g_ConsoleStateOffset = -1;
    return g_ConsoleStateOffset;
  }

  PropertyLookupResult consoleStateProperty = FindPropertyInfo(state, (UStruct *)consoleClass, "ConsoleState");
  if (consoleStateProperty.Offset >= 0) {
    g_ConsoleStateOffset = consoleStateProperty.Offset;
  } else {

    PropertyLookupResult historyBufferProp = FindPropertyInfo(state, (UStruct *)consoleClass, "HistoryBuffer");
    if (historyBufferProp.Offset >= 0) {
      g_ConsoleStateOffset = historyBufferProp.Offset + 112;
    } else {

      PropertyLookupResult whiteTextureProp = FindPropertyInfo(state, (UStruct *)consoleClass, "DefaultTexture_White");
      if (whiteTextureProp.Offset >= 0) {
        g_ConsoleStateOffset = whiteTextureProp.Offset + 144;
      }
    }
  }

  char buffer[160] = {};
  snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] ConsoleState offset resolved to %d", g_ConsoleStateOffset);
  EngineDiscovery::WriteLogLine(buffer);

  if (g_ConsoleStateOffset < 0 && !g_LoggedConsoleStateFailureDetails) {
    g_LoggedConsoleStateFailureDetails = true;
    LogConsoleClassDiagnostics(state, consoleObject,
        "ConsoleState property and layout anchors not found", false);
  }

  return g_ConsoleStateOffset;
}

int32_t ResolveNamedStateIndex(const EngineState &state, int32_t &cache, const char *name) {
  if (cache != -2) {
    return cache;
  }

  cache = ResolveCachedFNameIndex(state, name);
  return cache;
}

bool IsConsoleStateActive(const EngineState &state, UObject *consoleObject) {
  if (!state.bInitialized || !consoleObject) return false;

  const int32_t consoleStateOffset = ResolveConsoleStateOffset(state, consoleObject);
  if (consoleStateOffset < 0) return false;

  FName consoleState = {};
  if (!SafeMem_Read((uint8_t *)consoleObject + consoleStateOffset, &consoleState, sizeof(consoleState))) {
    return false;
  }

  const int32_t typingIndex = ResolveNamedStateIndex(state, g_NameTypingIndex, "Typing");
  const int32_t openIndex = ResolveNamedStateIndex(state, g_NameOpenIndex, "Open");
  if ((typingIndex >= 0 && consoleState.ComparisonIndex == static_cast<decltype(consoleState.ComparisonIndex)>(typingIndex)) ||
      (openIndex >= 0 && consoleState.ComparisonIndex == static_cast<decltype(consoleState.ComparisonIndex)>(openIndex))) {
    return true;
  }

  const std::string stateName = EngineDiscovery::FNameToStr(state, consoleState);
  return stateName == "Typing" || stateName == "Open";
}

bool CallFakeGotoState(const EngineState &state, UObject *consoleObject, const char *stateName) {
  if (!consoleObject || !stateName || !*stateName) return false;
  if (!EnsureFakeGotoStateResolved(state, consoleObject)) return false;

  const int32_t stateIndex = ResolveCachedFNameIndex(state, stateName);
  if (stateIndex < 0) {
    return false;
  }

  FFakeGotoStateParams params = {};
  params.NextStateName.ComparisonIndex = static_cast<decltype(params.NextStateName.ComparisonIndex)>(stateIndex);
  params.NextStateName.Number = 0;
  return EngineDiscovery::CallProcessEvent(state, consoleObject, g_FakeGotoStateFunction, &params);
}

bool TryOpenConsoleInternal(const EngineState &state) {
  if (!state.bInitialized) return false;

  const RuntimeFeatureConfig &cfg = RuntimeConfig::Load(g_Module);
  if (!cfg.EnableUnrealConsole) {
    return false;
  }

  if (!g_CachedConsoleObject && !EnableInGameConsole(state)) {
    return false;
  }
  if (!g_CachedConsoleObject) return false;

  ReattachCheatManagerIfNeeded(state);

  const bool hasFakeGotoState =
      EnsureFakeGotoStateResolved(state, g_CachedConsoleObject);
  if (!hasFakeGotoState) {

    return false;
  }

  if (IsConsoleStateActive(state, g_CachedConsoleObject)) {
    return true;
  }

  UObject *engineObject = FindLiveEngineObject(state, false);
  if (engineObject) {
    UObject *gameViewport = ResolveGameViewport(state, engineObject);
    if (gameViewport) {
      const int32_t viewportConsoleOffset = FindViewportConsoleOffset(state, gameViewport);
      if (viewportConsoleOffset >= 0) {
        void **viewportConsolePtr = (void **)((uint8_t *)gameViewport + viewportConsoleOffset);
        UObject *currentViewportConsole = viewportConsolePtr ? reinterpret_cast<UObject *>(*viewportConsolePtr) : nullptr;

        UClass *cachedConsoleClass = EngineDiscovery::GetObjectClass(state, g_CachedConsoleObject);
        if (currentViewportConsole && cachedConsoleClass &&
            IsUsableConsoleObject(state, currentViewportConsole, cachedConsoleClass) &&
            EngineDiscovery::GetObjectOuter(state, currentViewportConsole) == gameViewport) {
          if (currentViewportConsole != g_CachedConsoleObject) {
            g_CachedConsoleObject = currentViewportConsole;
            EngineDiscovery::WriteLogLine("[ConsoleEnabler] Hotkey open switched to the current viewport console.");
          }
        } else if (viewportConsolePtr && currentViewportConsole != g_CachedConsoleObject) {
          if (!cachedConsoleClass ||
              EngineDiscovery::GetObjectOuter(state, g_CachedConsoleObject) != gameViewport ||
              !SafeMem_Write(viewportConsolePtr, &g_CachedConsoleObject,
                             sizeof(g_CachedConsoleObject))) {
            EngineDiscovery::WriteLogLine("[ConsoleEnabler] Cached console is not safe to reattach to this viewport.");
            return false;
          }

          char buffer[512] = {};
          snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] Reattached cached console to %s for hotkey open.",
                   EngineDiscovery::BuildFullName(state, gameViewport).c_str());
          EngineDiscovery::WriteLogLine(buffer);
        }
      }
    }
  }

  SetConsoleTargetPlayer(state, g_CachedConsoleObject);

  const bool canValidateState = ResolveConsoleStateOffset(state, g_CachedConsoleObject) >= 0;

  if (hasFakeGotoState &&
      CallFakeGotoState(state, g_CachedConsoleObject, "Typing")) {
    if (!canValidateState) {
      if (!g_LoggedOpenWithoutStateValidation) {
        g_LoggedOpenWithoutStateValidation = true;
        EngineDiscovery::WriteLogLine("[ConsoleEnabler] Opened console via FakeGotoState(Typing) without ConsoleState validation.");
      }
      return true;
    }
    if (IsConsoleStateActive(state, g_CachedConsoleObject)) {
      EngineDiscovery::WriteLogLine("[ConsoleEnabler] Opened console via FakeGotoState(Typing).");
      return true;
    }
  }

  if (hasFakeGotoState &&
      CallFakeGotoState(state, g_CachedConsoleObject, "Open")) {
    if (!canValidateState) {
      if (!g_LoggedOpenWithoutStateValidation) {
        g_LoggedOpenWithoutStateValidation = true;
        EngineDiscovery::WriteLogLine("[ConsoleEnabler] Opened console via FakeGotoState(Open) without ConsoleState validation.");
      }
      return true;
    }
    if (IsConsoleStateActive(state, g_CachedConsoleObject)) {
      EngineDiscovery::WriteLogLine("[ConsoleEnabler] Opened console via FakeGotoState(Open).");
      return true;
    }
  }

  (void)canValidateState;
  return false;
}

bool ShouldReleaseMouseCaptureForState(const EngineState &state) {
  return IsConsoleStateActive(state, g_CachedConsoleObject);
}

bool EnableInGameConsole(const EngineState &state) {
  if (!state.bInitialized) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] Engine not fully initialized.");
    return false;
  }

  const RuntimeFeatureConfig &cfg = RuntimeConfig::Load(g_Module);
  if (!cfg.EnableUnrealConsole) {
    return false;
  }

  UObject *engineObject = FindLiveEngineObject(state, true);
  if (!engineObject) {
    return false;
  }

  UObject *gameViewport = ResolveGameViewport(state, engineObject);
  if (!gameViewport) {
    return false;
  }

  EngineDiscovery::BeginExclusiveLogSection("console");

  static bool travelGuardInstalled = false;
  if (!travelGuardInstalled) {
    travelGuardInstalled = SeamlessTravelGuard::Install(g_State);
  }

  UnlockConsoleCommands(state);

  UClass *engineClass = EngineDiscovery::GetObjectClass(state, engineObject);
  UClass *consoleClass = nullptr;
  if (engineClass) {
    const int32_t consoleClassOffset = FindPropertyOffset(state, (UStruct *)engineClass, "ConsoleClass");
    if (consoleClassOffset >= 0) {
      consoleClass = (UClass *)SafeMem_ReadPtr((uint8_t *)engineObject + consoleClassOffset);
    }
  }

  if (!consoleClass) {
    consoleClass = FindClassByNameExact(state, "Console");
  }
  if (!consoleClass) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] Failed to resolve Console class.");
    EngineDiscovery::EndLogSection("console");
    return false;
  }

  const int32_t viewportConsoleOffset = FindViewportConsoleOffset(state, gameViewport);
  if (viewportConsoleOffset < 0) {
    EngineDiscovery::EndLogSection("console");
    return false;
  }

  UObject *existingConsole = (UObject *)SafeMem_ReadPtr((uint8_t *)gameViewport + viewportConsoleOffset);
  if (existingConsole && !IsUsableConsoleObject(state, existingConsole, consoleClass)) {
    char buffer[512] = {};
    const std::string existingName = EngineDiscovery::BuildFullName(state, existingConsole);
    const std::string existingClass = EngineDiscovery::GetObjectNameStr(state, EngineDiscovery::GetObjectClass(state, existingConsole));
    snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] Ignoring invalid ViewportConsole pointer: class=%s name=%s",
             existingClass.empty() ? "<unknown>" : existingClass.c_str(),
             existingName.empty() ? "<unknown>" : existingName.c_str());
    EngineDiscovery::WriteLogLine(buffer);
    existingConsole = nullptr;
  } else if (existingConsole) {
    char buffer[512] = {};
    snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] Viewport already has a console: %s",
             EngineDiscovery::BuildFullName(state, existingConsole).c_str());
    EngineDiscovery::WriteLogLine(buffer);
  }

  UObject *createdConsole = existingConsole;
  if (!createdConsole) {
    createdConsole = FindExistingConsoleObject(state, consoleClass, gameViewport);
    if (createdConsole) {
      EngineDiscovery::WriteLogLine("[ConsoleEnabler] Reusing existing console instance owned by viewport.");
    }

    if (!createdConsole && state.StaticConstructObject) {
      createdConsole = TrySEHConstruct(state, consoleClass, gameViewport);
      if (createdConsole) {
        EngineDiscovery::WriteLogLine("[ConsoleEnabler] Successfully spawned Console via Direct Native Instantiation (StaticConstructObject).");
      }
    }

    if (!createdConsole) {
      const ConstructHelperInfo constructHelper = FindConstructHelper(state);
      if (constructHelper.Function && constructHelper.OwnerClass) {
        char buffer[512] = {};
        snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] Using construct helper: %s on %s",
                 constructHelper.Name,
                 EngineDiscovery::BuildFullName(state, (UObject *)constructHelper.OwnerClass).c_str());
        EngineDiscovery::WriteLogLine(buffer);

        UObject *helperCDO = FindFirstObjectOfClass(state, constructHelper.OwnerClass, true);
        if (helperCDO) {
          FSpawnObjectParams params = {};
          params.ObjectClass = consoleClass;
          params.Outer = gameViewport;

          if (EngineDiscovery::CallProcessEvent(state, helperCDO, constructHelper.Function, &params) && params.ReturnValue) {
            createdConsole = params.ReturnValue;
            EngineDiscovery::WriteLogLine("[ConsoleEnabler] Successfully spawned Console via construct helper.");
          } else if (state.UEMajor >= 5 && state.UEMinor >= 8) {
            EngineDiscovery::WriteLogLine("[Intelligence] UE5.8+ detected: Construct helper restricted Outer.");
          } else {
            EngineDiscovery::WriteLogLine("[Intelligence] Construct helper unfulfilled for Viewport Outer.");
          }
        } else {
          EngineDiscovery::WriteLogLine("[ConsoleEnabler] Failed to find helper class default object.");
        }
      }
    }
  }

  if (!createdConsole) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] Failed to create or locate a console object.");
    EngineDiscovery::EndLogSection("console");
    return false;
  }

  if (!IsUsableConsoleObject(state, createdConsole, consoleClass) ||
      EngineDiscovery::GetObjectOuter(state, createdConsole) != gameViewport) {
    char buffer[512] = {};
    snprintf(buffer, sizeof(buffer),
             "[ConsoleEnabler] Refusing to attach invalid console object %p (outer=%p, expected viewport=%p).",
             createdConsole, EngineDiscovery::GetObjectOuter(state, createdConsole),
             gameViewport);
    EngineDiscovery::WriteLogLine(buffer);
    EngineDiscovery::EndLogSection("console");
    return false;
  }

  if (!existingConsole) {
    void **viewportConsolePtr =
        (void **)((uint8_t *)gameViewport + viewportConsoleOffset);
    if (!viewportConsolePtr ||
        !SafeMem_Write(viewportConsolePtr, &createdConsole, sizeof(createdConsole))) {
      EngineDiscovery::WriteLogLine("[ConsoleEnabler] Failed to attach Console to ViewportConsole.");
      EngineDiscovery::EndLogSection("console");
      return false;
    }

    char buffer[512] = {};
    snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] Console attached to %s", EngineDiscovery::BuildFullName(state, gameViewport).c_str());
    EngineDiscovery::WriteLogLine(buffer);
  }

  SetConsoleTargetPlayer(state, createdConsole);

  if (!cfg.EnableWndProcHook) {
    AttachCheatManager(state);
  }
  RemapConsoleKeys(state);
  ResolveConsoleStateOffset(state, createdConsole);

  g_CachedConsoleObject = createdConsole;
  return true;
}

bool TryOpenConsole(const EngineState &state) {
  return TryOpenConsoleInternal(state);
}

static std::string GetKeyNameFromVK(WPARAM vk) {
  if (vk >= VK_F1 && vk <= VK_F24) {
    return "F" + std::to_string(vk - VK_F1 + 1);
  }
  if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
    return "NumPad" + std::to_string(vk - VK_NUMPAD0);
  }
  if (vk >= 'A' && vk <= 'Z') {
    return std::string(1, static_cast<char>(vk));
  }
  if (vk >= '0' && vk <= '9') {
    return std::string(1, static_cast<char>(vk));
  }
  switch (vk) {
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "PageUp";
    case VK_NEXT: return "PageDown";
    case VK_SPACE: return "Space";
    case VK_RETURN: return "Enter";
    case VK_TAB: return "Tab";
    case VK_ESCAPE: return "Escape";
    case VK_BACK: return "Backspace";
    case VK_CAPITAL: return "CapsLock";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_OEM_3: return "Tilde";
    case VK_OEM_MINUS: return "Minus";
    case VK_OEM_PLUS: return "Plus";
    case VK_OEM_4: return "BracketLeft";
    case VK_OEM_6: return "BracketRight";
    case VK_OEM_1: return "Semicolon";
    case VK_OEM_7: return "Quote";
    case VK_OEM_COMMA: return "Comma";
    case VK_OEM_PERIOD: return "Period";
    case VK_OEM_2: return "Slash";
    case VK_OEM_5: return "Backslash";
    case VK_MULTIPLY: return "NumPadMultiply";
    case VK_ADD: return "NumPadAdd";
    case VK_SUBTRACT: return "NumPadSubtract";
    case VK_DECIMAL: return "NumPadDecimal";
    case VK_DIVIDE: return "NumPadDivide";
    case VK_PAUSE: return "Pause";
    case VK_SNAPSHOT: return "PrintScreen";
    case VK_SCROLL: return "ScrollLock";
    default: break;
  }
  return "";
}

static std::atomic<bool> g_KeyStates[256] = {};

bool IsVirtualKeyHeldWndProc(int vk) {
    if (vk < 0 || vk >= 256) return false;
    return g_KeyStates[vk].load(std::memory_order_relaxed);
}

static LRESULT CALLBACK GameWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) {
      if (wParam < 256) {
          g_KeyStates[wParam].store(true, std::memory_order_relaxed);
          if (wParam == VK_SHIFT) {
              if (GetKeyState(VK_LSHIFT) & 0x8000) g_KeyStates[VK_LSHIFT].store(true, std::memory_order_relaxed);
              if (GetKeyState(VK_RSHIFT) & 0x8000) g_KeyStates[VK_RSHIFT].store(true, std::memory_order_relaxed);
          }
      }
      const RuntimeFeatureConfig& cfg = RuntimeConfig::Load(g_Module);
      std::string keyName = GetKeyNameFromVK(wParam);
      if (!keyName.empty() && cfg.EnableLuaMods) {

        LuaEngine::LuaHookManager::Get().OnKeyPress(keyName);
      }

      if (wParam == VK_NEXT && (GetKeyState(VK_MENU) & 0x8000)) {
        if (cfg.EnableLuaMods && LuaManager::Get().IsInitialized()) {
          LuaManager::Get().HotReloadMods();
          return 0;
        }
      }

      const UINT scanCode = (static_cast<UINT>(lParam) >> 16) & 0xFF;
      if (wParam == VK_OEM_3  || wParam == VK_F10 || wParam == VK_OEM_8 || scanCode == 41) {
        if (cfg.EnableUnrealConsole) {
          if (ConsoleEnabler::TryOpenConsole(g_State)) {
            return 0;
          }

          if (wParam == VK_OEM_3 || wParam == VK_F10 || scanCode == 41) {
            return CallWindowProc((WNDPROC)g_State.OriginalWndProc, hwnd, uMsg,
                                  wParam, lParam);
          }
          return 0;
        } else {

          return 0;
        }
      }
  } else if (uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP) {
      if (wParam < 256) {
          g_KeyStates[wParam].store(false, std::memory_order_relaxed);
          if (wParam == VK_SHIFT) {
              g_KeyStates[VK_LSHIFT].store(false, std::memory_order_relaxed);
              g_KeyStates[VK_RSHIFT].store(false, std::memory_order_relaxed);
          }
      }
  }
  return CallWindowProc((WNDPROC)g_State.OriginalWndProc, hwnd, uMsg, wParam, lParam);
}

void InstallWndProcHook(EngineState &state) {
  if (!state.GameWindow || state.OriginalWndProc) return;
  state.OriginalWndProc = (void *)SetWindowLongPtr(state.GameWindow, GWLP_WNDPROC, (LONG_PTR)GameWndProc);
  if (state.OriginalWndProc) {
    EngineDiscovery::WriteLogLine("[Input] Installed WndProc hook.");
  } else {
    EngineDiscovery::WriteLogLine("[Input] Failed to install WndProc hook.");
  }
}

void RemoveWndProcHook(EngineState &state) {
  if (state.GameWindow && state.OriginalWndProc) {
    SetWindowLongPtr(state.GameWindow, GWLP_WNDPROC, (LONG_PTR)state.OriginalWndProc);
    state.OriginalWndProc = nullptr;
    EngineDiscovery::WriteLogLine("[Input] Removed WndProc hook.");
  }
}

void Prewarm(const EngineState &state) {
  const RuntimeFeatureConfig &cfg = RuntimeConfig::Load(g_Module);
  if (!cfg.EnableUnrealConsole) return;

  FindConstructHelper(state);
}

}

namespace runtimeinput {

bool IsConsoleEnabled() {
  const RuntimeFeatureConfig &cfg = RuntimeConfig::Load(g_Module);
  return cfg.EnableUnrealConsole;
}

bool ShouldReleaseMouseCapture() {
  const RuntimeFeatureConfig &cfg = RuntimeConfig::Load(g_Module);
  if (!cfg.EnableUnrealConsole) return false;
  return ConsoleEnabler::ShouldReleaseMouseCaptureForState(::g_State);
}

bool TryOpenConsole() {
  const RuntimeFeatureConfig &cfg = RuntimeConfig::Load(g_Module);
  if (!cfg.EnableUnrealConsole) return false;
  return ConsoleEnabler::TryOpenConsole(::g_State);
}

}
