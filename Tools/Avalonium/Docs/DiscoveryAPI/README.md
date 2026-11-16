# DiscoveryAPI: `Runtime/core`

The discovery layer turns a running, stripped UE game process into a usable
reflection database. It is the first subsystem to run and every other subsystem
(console, Lua, overlay, SDK/USMap) consumes its result: the global
`EngineState g_State`.

Source files (`Runtime/core`):

| File | Responsibility |
| :--- | :--- |
| `Types.h` | `EngineState`, `FName`/`FString`/`TArray`, `FFrame` layout, `PropertyInfoEx`/`ClassInfoEx`/`EnumInfoEx` |
| `EngineDiscovery.h/.cpp` | the `Initialize()` orchestration; public façade for the whole layer |
| `EngineDetection.h/.cpp` | engine version detection, global lookups (`GUObjectArray`, `GNames`, `GWorld`, `GMalloc`, `GNatives`, `GUObjectHashTables`, `GEngine`), allocator identification and Malloc-slot measurement, FName-pool validation |
| `AllocatorSlotScan.h/.cpp` | call-site argument-shape analysis that separates `FMalloc::Malloc` from `Free`/`Realloc` slots |
| `PatternScanner.h/.cpp` | AOB scanning, export/string-reference resolution, module enumeration |
| `OffsetDiscovery.h/.cpp` | UObject/UStruct/FProperty layout discovery |
| `ObjectArray.h/.cpp` | `FUObjectArray` layout, chunked object iteration, chunk-size discovery |
| `NameResolution.h/.cpp` | FName layout discovery, decoding, name cache, path/full names |
| `ObjectUtils.h/.cpp` | safe object accessors, property type identity, alignment heuristics |
| `FunctionFinder.h/.cpp` | function/global addresses: `FName::ToString`, `GetFullName`, `StaticConstructObject`, `ProcessEvent`, `ProcessInternal`, `ProcessLocalScriptFunction`, core classes |
| `WorldDiscovery.h/.cpp` | live `UWorld`, persistent level, sublevels, world actors |
| `ObjectIndex.h/.cpp` | name/class indexes, fast lookup, function caches, live lookups |
| `BlueprintDiscovery.h/.cpp` | reflected member/frame access, FName creation through the engine, asset registry binding, asset loading, blueprint class discovery |
| `EncryptedCache.h/.cpp` | encrypted discovery snapshot save/restore (format v5) |
| `LogSession.h/.cpp` | structured session report: sections, severity, facts, summary, rotation |
| `RuntimeConfig.h/.cpp` | INI configuration (see [RuntimeAPI/CONFIG.md](../RuntimeAPI/CONFIG.md)) |
| `Logger.h/.cpp` | timestamped raw log sink |

Detailed per-function reference: [REFERENCE.md](REFERENCE.md).
`dllmain.cpp` (lifecycle) is documented in
[RuntimeAPI](../RuntimeAPI/README.md).

---

## Design principles

1. **Nothing is version-tabled.** Every offset, layout and function address is
   recovered from the running process. Supported range is UE 4.23–4.27 and
   5.0–5.8; older 4.x aborts with `bUnsupportedVersion = true`. I didn't build a version table.
   They're a lazy crutch that turns to shit on the first patch, and I'm not maintaining one
   for every build Epic spits out. Epic can call that paranoid; the process is the source of
   truth.
2. **The engine's own laws are the authority.** ProcessEvent's vtable slot is
   resolved by matching the two implementations the engine declares
   (`UObject`/`AActor`) plus the `FUNC_Native` test, not by a pattern's score.
   FFrame offsets follow `Stack.h`, property flags follow `ObjectMacros.h`.
   I don't care how high a pattern scores; if it points at the wrong function, it's still
   bullshit.
3. **Fan-out with fallbacks.** Layout discovery probes candidates, validates
   them against live classes, and records which layout won
   (`bUseFProperty`, `bUseCasePreservingName`, `bUseOutlineNumber`, ...).
4. **Fault-free reads.** Every speculative read goes through `SafeMemory`
   (see [SafeMemAPI](../SafeMemAPI/README.md)); an unnecessary fault costs an
   exception round trip. We treat faults as expensive: Microsoft's exception path is slow as
   hell, and an unnecessary fault buys a round trip.
5. **Failures are recorded, not hidden.** Unresolved structures set
   `LastError` and log once.

## Initialization pipeline

`EngineDiscovery::Initialize(g_State, skipConsoleManager)`:

```text
 1. Game name: main window title, else executable name
 2. DetectVersion: UE major.minor (reject < 4.23)
 3. Cache path: if EnableDiscoveryCache and snapshot validates:
                         restore offsets/globals → rebuild caches → done
 4. FindGlobals: GUObjectArray, GNames, GWorld, GMalloc, GNatives,
                       GUObjectHashTables (pattern scan or obfuscation scan);
                       the FName cache decodes as soon as GNames validates,
                       under the remaining searches
 4a. Identify GMalloc: GetDescriptiveName, measure the Malloc vtable slot from
                       call-site argument shapes (parallel section scan),
                       prove it with one allocation
 5. DiscoverOffsets: UObject, UStruct/UClass/UFunction, FProperty/FName layout
 6. BuildFNameCache: ran inside FindGlobals (step 4); the per-chunk text index
                       is filled by the walk workers
 7. FindEngineFunctions: FName::ToString, GetFullName,
                       IConsoleManager (unless skipped)
 8. FindCoreClasses: UObject/UClass/UStruct/AActor/APawn/UEnum/UFunction/
                       UPackage/UWorld class objects
 9. Layout fixups: re-run UStruct layout if needed, FField fixups,
                       FProperty offset probing, drop stale inheritance cache
10. DiscoverWorldLayout: UWorld::PersistentLevel, ULevel::Actors, live world
11. FindProcessEvent: vtable slot via engine-law resolution
12. BuildObjectIndex: Name→object(s), class→instances, function caches
13. RebuildFunctionCache / DeltaUpdateFunctionCache
14. FindStaticConstructObject (only when a mod runtime is enabled),
                       FindProcessInternal, FindProcessLocalScriptFunction
15. FindGEngine
16. (optional) save discovery snapshot
```

The cached path (step 3) checks magic and structure size, compares the
executable's PE timestamp and image size, re-verifies the allocator identity
and Malloc slot (one allocation probe) and the `IConsoleManager` pointer
global's sparse map, then re-runs the in-memory stages: FName cache, engine
functions, core classes, object index, world, ProcessEvent,
`StaticConstructObject` (when a mod runtime is enabled), `ProcessInternal`,
`ProcessLocalScriptFunction` and `GEngine`. It may re-save the snapshot when it
newly resolves `IConsoleManager`.

`GUObjectHashTables` is searched main module first and stops at the first
Strict four-map chain whose sampled keys are all live UObjects; that proof
settles the table, so the remaining modules are not scanned. The search line
reports its wall time, the megabytes it read and the key-proof time.

### StaticConstructObject_Internal

The construct entry search runs in cost order: exported symbol first, then the
parameter-struct law, then the reflected native-caller traversal, the
`NewObject` diagnostic string and the RF-mask population scan as fallbacks.

The parameter-struct law is the primary route. The entry takes
`FStaticConstructObjectParameters` by const reference, so its body loads the
structure members off RCX, and every build from UE 4.23 to 5.8 opens with the
class-flag test for `CLASS_Native | CLASS_Intrinsic` (mask `0x10000080`,
`UObjectGlobals.cpp`). Candidate starts come from the module's own direct-call
targets, because `NewObject<T>` inlines into hundreds of engine functions and
calls the entry directly. Padding before a function cannot be trusted for
start detection, and the `NewObject` diagnostic string is compiled out of
shipping builds, so neither is used as evidence. A candidate is accepted with
at least three distinct parameter reads, the mask inside its own body, at
least sixteen direct callers and a 4x lead over any runner-up. Measured
winners: 775 callers on 4.26, 546 on 4.27, 1317 on 5.4, 1484 on 5.8.

The search itself only runs at startup when a mod runtime is enabled
(`EnableLuaMods` or `EnableCppMods`). Otherwise discovery defers it, and the
console resolves the entry on demand only after its construct helper has
failed (`EnsureStaticConstructObject`).


Progress flags on `EngineState`: `bGlobalsFound`, `bFunctionsFound`,
`bCoreClassesFound`, `bOffsetsDiscovered`, `bFNameLayoutDiscovered`,
`bIndexBuilt`, `bInitialized`.

## Public façade (`EngineDiscovery.h`)

The header re-exports the whole layer. Most-used entry points:

```cpp
// lifecycle
bool Initialize(EngineState& state, bool skipConsoleManager = false);

// globals / layout
bool DetectVersion(EngineState&);
bool FindGlobals(EngineState&);
bool DiscoverOffsets(EngineState&);
bool FindEngineFunctions(EngineState&);

// name resolution
std::string GetObjectNameStr(const EngineState&, UObject*);
std::string BuildPathName(const EngineState&, UObject*);
std::string BuildFullName(const EngineState&, UObject*);
int32_t ResolveFNameIndex(EngineState&, const std::string& name);

// index lookups
UObject* FindObjectByNameFast(EngineState&, const std::string&);
std::vector<UObject*> FindObjectsByNameFast(EngineState&, const std::string&);
std::vector<UObject*> FindObjectsByClassFast(EngineState&, UClass*);
std::vector<UObject*> FindObjectsByPrefix(EngineState&, const std::string&);
UObject* FindObjectByPath(EngineState&, const std::string&);
UObject* ResolveObjectPath(EngineState&, const std::string&,
                           UClass* requiredClass = nullptr,
                           bool exactClass = false,
                           UObject* withinOuter = nullptr);

// dispatch
void* FindFunctionByName(const EngineState&, UStruct*, const char*);
bool  CallProcessEvent(const EngineState&, void* object, void* ufunction, void* params);

// world
UObject* GetLiveWorld(EngineState&);
UObject* GetPersistentLevel(const EngineState&);
int32_t  GetWorldActors(const EngineState&, void** out, int32_t max);

// introspection / generation
int32_t GetObjectCount(const EngineState&);
UObject* GetObjectByIndex(const EngineState&, int32_t);
bool GenerateUSMap(const EngineState&, const std::string& outputPath);
```

## Threading and locking

* Discovery runs on the init thread; consumers start only after it completes.
* `state.IndexMutex` (`std::recursive_mutex`, shared_ptr) guards the object
  indexes (`NameToFirstObject`, `ClassToInstances`, `FunctionByFullKey`, ...).
  Helpers such as `GetAllActors` take it.
* `state.ReflectionCacheMutex` guards reflection/type caches.
* `EncryptedCache` and the generators run under the RuntimeAPI mutex, so two
  IPC clients cannot generate concurrently.
* Live lookups (`FindLiveObjectsByClass`) are memoized per frame under
  `LiveLookupGeneration` / `LiveLookupCacheStamp`, advanced by the overlay
  pump.

## Structured session log

Discovery participates in `LogSession` (`Runtime/core/LogSession.h`). The
session opens a banner with host OS, CPU brand/topology/cache/features, GPU,
RAM and binary inventory, then groups lines under lifecycle sections
(`config`, `runtime`, `discovery`, `crypto`, `console`, `overlay`, `lua`,
`level_transition`). Subsystems contribute facts and an overall result, and
`EndLogSession` prints the summary. Severity filtering supports
Trace/Debug/Info/Warning/Error/Fatal, and I rotate the log through five files so a crash
doesn't take the evidence down with it.

## Failure handling

* `Initialize` returns `false` with a human reason in `state.LastError`
  (also readable via `GetLastErrorMsg()`).
* Unsupported versions set `state.bUnsupportedVersion` and do not retry.
* Individual discovery steps return `false` and leave their state flags
  unset. I skip dependent steps rather than guess; guessing is how a runtime ends up crashing
  in ways nobody can explain.

## See also

* [REFERENCE.md](REFERENCE.md): every function, struct and `EngineState` field.
* [RuntimeAPI/CONFIG.md](../RuntimeAPI/CONFIG.md): `SkipPatternScan`,
  `EnableDiscoveryCache`, `VerboseLog`.
* [LuaAPI](../LuaAPI/README.md): scripting surface built on this layer.
* [SafeMemAPI](../SafeMemAPI/README.md): the read/write primitives used here.
</content>
