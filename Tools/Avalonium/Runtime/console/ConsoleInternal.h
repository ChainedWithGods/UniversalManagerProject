#pragma once

#include "../core/ConsoleEnabler.h"
#include "../utils/SafeMemory.h"
#include "../core/EngineDiscovery.h"
#include "../core/RuntimeConfig.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <cmath>
#include <filesystem>
#include <psapi.h>
#include <windows.h>
#include <mutex>
#include <unordered_map>

#include "../api/UEInternalAPI.h"

extern EngineState g_State;

void GetConsoleDumpDirectory(const EngineState &state, char* outBuffer, size_t bufferSize);

namespace ConsoleEnabler {

    extern UObject *g_CachedConsoleObject;
    extern int32_t g_ConsoleStateOffset;
    extern int32_t g_NameTypingIndex;
    extern int32_t g_NameOpenIndex;
    extern void *g_FakeGotoStateFunction;
    extern bool g_TriedResolveFakeGotoStateFunction;
    extern bool g_LoggedViewportConsoleFailureDetails;
    extern bool g_LoggedConsoleStateFailureDetails;
    extern bool g_LoggedFakeGotoStateFailureDetails;
    extern bool g_LoggedOpenWithoutStateValidation;
    extern UObject *g_CachedCheatManagerOwner;
    extern UClass *g_CachedConsoleClass;
    extern UClass *g_CachedGameEngineClass;
    extern UClass *g_CachedEngineClass;
    extern UClass *g_CachedGameViewportClientClass;
    extern UClass *g_CachedLocalPlayerClass;
    extern UClass *g_CachedGameplayStaticsClass;

    struct FSpawnObjectParams {
        UClass *ObjectClass;
        UObject *Outer;
        UObject *ReturnValue;
    };

    struct FFakeGotoStateParams {
        FName NextStateName;
    };

    struct FKeyEntry {
        FName KeyName;
        void *KeyDetails[2];
    };

    struct SpawnableClassEntry {
        std::string shortName;
        std::string fullName;
    };

    struct ConstructHelperInfo {
        void *Function = nullptr;
        UClass *OwnerClass = nullptr;
        const char *Name = nullptr;
    };

    struct PropertyLookupResult {
        void *Property = nullptr;
        int32_t Offset = -1;
        bool UsedFPropertyLayout = false;
        std::string TypeName;
        std::string InnerTypeName;
        std::string StructTypeName;
    };

    struct FConsoleMapElement {
        void *Value;
    };

    struct ExecDumpContext {
        const EngineState *state;
        std::map<std::string, std::vector<std::string>> classToCommands;
        std::map<std::string, std::string> classToParent;
        int totalCount = 0;
    };

    struct SpawnableDumpContext {
        const EngineState *state = nullptr;
        UClass *actorClass = nullptr;
        UClass *componentClass = nullptr;
        UClass *widgetClass = nullptr;
        std::vector<SpawnableClassEntry> actors;
        std::vector<SpawnableClassEntry> components;
        std::vector<SpawnableClassEntry> widgets;
        std::vector<SpawnableClassEntry> others;
        int totalCount = 0;
    };

    UObject *TrySEHConstruct(const EngineState &state, UClass *cls, UObject *outer, FName name = {}, uint32_t setFlags = 0) noexcept;
    UObject *TrySEHConstruct(StaticConstructObject_Internal_t fn, void *params) noexcept;
    bool StartsWith(const std::string &text, const char *prefix);
    bool EqualsInsensitiveAscii(const std::string &text, const char *rhs);
    bool ContainsInsensitiveAscii(const std::string &text, const char *needle);
    bool IsDefaultObjectName(const std::string &name);
    bool IsClassObject(const EngineState &state, UObject *object);
    bool IsObjectA(const EngineState &state, UObject *object, UClass *baseClass);
    UObject *FindOwningClassObject(const EngineState &state, UObject *object);
    UClass *FindClassByNameExact(const EngineState &state, const char *className);
    UObject *FindObjectByNameExact(const EngineState &state, const char *objectName);
    UObject *FindFirstObjectOfClass(const EngineState &state, UClass *baseClass, bool wantDefaultObject);
    UObject *FindExistingConsoleObject(const EngineState &state, UClass *consoleClass,
                                       UObject *expectedOuter);
    bool IsUsableConsoleObject(const EngineState &state, UObject *consoleObject, UClass *consoleClass);
    void *ReadPtrAt(void *base, int32_t offset);
    void *GetPropertyOwner(const EngineState &state, void *prop, bool useFPropertyLayout);
    std::string GetUPropertyTypeName(const EngineState &state, void *prop);
    void *GuessArrayInnerProperty(const EngineState &state, void *prop, bool useFPropertyLayout);
    PropertyLookupResult FindPropertyWithLayout(const EngineState &state, UStruct *cls, const std::string &propName, bool useFPropertyLayout);
    int32_t FindPropertyOffset(const EngineState &state, UStruct *cls, const std::string &propName);
    PropertyLookupResult FindPropertyInfo(const EngineState &state, UStruct *cls, const std::string &propName);
    void LogPropertyPreview(const EngineState &state, UStruct *cls, const char *header, bool useFPropertyLayout);
    void LogFunctionPreview(const EngineState &state, UStruct *cls, const char *header);
    void LogFunctionKeywordMatches(const EngineState &state, UStruct *cls, const char *header);
    void LogConsoleClassDiagnostics(const EngineState &state, UObject *consoleObject, const char *reason, bool includeFunctionDetails);
    bool EnsureFakeGotoStateResolved(const EngineState &state, UObject *consoleObject);
    void *FindFunctionByNameLoose(const EngineState &state, UStruct *cls, const char *funcName);
    int32_t FindViewportConsoleOffset(const EngineState &state, UObject *gameViewport);
    int32_t ResolveCachedFNameIndex(const EngineState &state, const char *name);
    UObject *FindFirstLocalPlayer(const EngineState &state);
    UObject *FindPlayerController(const EngineState &state);
    UObject *FindLiveEngineObject(const EngineState &state, bool logSelection = true);
    UObject *ResolveGameViewport(const EngineState &state, UObject *engineObject);
    ConstructHelperInfo FindConstructHelper(const EngineState &state);

    int32_t ScanForConsoleStateOffset(const EngineState &state, UObject *consoleObject);
    int32_t ResolveConsoleStateOffset(const EngineState &state, UObject *consoleObject);
    int32_t ResolveNamedStateIndex(const EngineState &state, int32_t &cache, const char *name);
    bool IsConsoleStateActive(const EngineState &state, UObject *consoleObject);
    bool CallFakeGotoState(const EngineState &state, UObject *consoleObject, const char *stateName);
    bool TryOpenConsoleInternal(const EngineState &state);
    bool ShouldReleaseMouseCaptureForState(const EngineState &state);
    bool SetConsoleTargetPlayer(const EngineState &state, UObject *consoleObject);
    void AttachCheatManager(const EngineState &state);
    void ReattachCheatManagerIfNeeded(const EngineState &state);
    void RemapConsoleKeys(const EngineState &state);
    bool WriteConsoleKeySlot(FKeyEntry *keyArray, int32_t slot, const FName &keyName, bool initializeSlot);
    bool IsLikelyValidConsoleKeyArray(const TArray<FKeyEntry> &keyArray);
    void LogConsoleKeySlots(const EngineState &state, const TArray<FKeyEntry> &keyArray, const char *prefix);

    void DumpLoadedModules(const EngineState &state);
    void DumpExecCommands(const EngineState &state);
    void DumpSpawnableClasses(const EngineState &state);
    void UnlockConsoleCommands(const EngineState &state);

}
