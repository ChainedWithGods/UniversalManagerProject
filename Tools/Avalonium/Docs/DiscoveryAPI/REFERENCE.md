# DiscoveryAPI: Reference

Namespace: `EngineDiscovery` unless noted otherwise. All types live in
`Runtime/core/Types.h`. All functions take `EngineState&` explicitly; the
runtime passes its global `g_State`.

---

## 1. `Types.h`

### 1.1 Fundamental types

| Type | Definition | Notes |
| :--- | :--- | :--- |
| `UObject`, `UClass`, `UStruct`, `UField`, `UEnum`, `UFunction`, `FProperty`, `UWorld`, `AActor`, `ULevel` | `typedef void` | opaque engine objects |
| `TArray<T>` | `{ T* Data; int32 ArrayNum; int32 ArrayMax; }` | engine array; `AllocatorInstance` not modelled |
| `FName` | `{ uint32 ComparisonIndex; uint32 Number; }` | in-memory form; layout discovery may widen it |
| `FString` | `{ wchar_t* Data; int32 ArrayNum; int32 ArrayMax; }` + `ToStdString()` | UTF-8 conversion (trims trailing NULs) |

### 1.2 Kismet VM frame

`FFrame` offsets follow `Public/UObject/Stack.h` (identical in all supported
trees). `FOutParmRec` matches the engine declaration exactly.

```cpp
struct FOutParmRec {
    void* Property;            // FProperty*
    uint8_t* PropAddr;         // caller storage
    FOutParmRec* NextOutParm;  // null terminates
};
```

| Constant | Value | Meaning |
| :--- | :--- | :--- |
| `kCpfParm` | `0x80` | `CPF_Parm` |
| `kCpfOutParm` | `0x100` | `CPF_OutParm` |
| `kCpfReturnParm` | `0x400` | `CPF_ReturnParm` |
| `kFuncNative` | `0x400` | `FUNC_Native` |
| `FFrame_NodeOff` | `0x10` | `FFrame::Node` |
| `FFrame_ObjectOff` | `0x18` | `FFrame::Object` |
| `FFrame_CodeOff` | `0x20` | `FFrame::Code` (bytecode cursor) |
| `FFrame_LocalsOff` | `0x28` | `FFrame::Locals` (params + locals buffer) |
| `FFrame_OutParmsOff` | `0x78` | declared `FOutParmRec*`, validated at runtime |
| `FFrame_ScanFirstOff` / `FFrame_ScanLastOff` | `0x10` / `0xF8` | search window for a build with a wider flow stack |

Out-parameters are **not** read from `Locals`. Epic stores the caller's
destination in `FOutParmRec::PropAddr`, and that's the only place worth reading,
`Locals` gives plausible-looking garbage. Readers resolve the list against the
executing function's parameters (`LuaHookManager::ResolveOutParmAddress`).

### 1.3 Function pointer typedefs

| Typedef | Signature |
| :--- | :--- |
| `FName_ToString_t` | `void __fastcall (const FName*, FString& out)` |
| `FName_AppendString_t` | `void __fastcall (const FName*, FString& out)` |
| `UObject_GetFullName_t` | `FString __fastcall (const UObject*, const UObject* stopOuter, uint32 flags)` |
| `UObject_GetName_t` | `FString __fastcall (const UObject*)` |
| `UObject_GetPathName_t` | `FString __fastcall (const UObject*, const UObject* stopOuter)` |
| `UObject_IsA_t` | `bool __fastcall (const UObject*, const UClass*)` |
| `StaticFindObject_t` | `UObject* __fastcall (UClass*, UObject* package, const wchar_t* name, bool exactClass)` |
| `StaticLoadObject_t` | `UObject* __fastcall (UClass*, UObject* parent, const wchar_t* name, const wchar_t* filename, uint32 flags, void* sandbox, bool bAllowReconciliation)` |
| `StaticConstructObject_Internal_t` | `UObject* __fastcall (const void* Params)` |

### 1.4 Interop structs (packed, 1-byte alignment)

**`PropertyInfoEx`** (1568 bytes): returned by `GetClassProperties`:

| Offset | Field |
| :--- | :--- |
| 0 | `char Name[256]` |
| 256 | `char TypeName[256]` |
| 512 | `char InnerType[256]` |
| 768 | `char KeyType[256]` |
| 1024 | `char ValueType[256]` |
| 1280 | `char ClassName[256]` |
| 1536… | `int32 Offset; int32 Size; int32 ElementSize; int32 ArrayDim;` |
| 1552… | `uint64 PropertyFlags` |
| 1560… | `bool bIsArray,bIsMap,bIsSet,bIsStruct,bIsEnum,bIsObject,bIsBool,bIsString` |

> `InnerType`/`KeyType`/`ValueType` are part of the ABI; the export leaves them empty.
> The Lua/Blueprint layers are where container detail lives.

**`ClassInfoEx`** (1044 bytes): element of the `DUMP` shared memory:

| Offset | Field |
| :--- | :--- |
| 0 | `char Name[256]` |
| 256 | `char FullPath[512]` |
| 768 | `char SuperClassName[256]` |
| 1024 | `void* Address` |
| 1032 | `int32 PropertiesCount` |
| 1036 | `int32 FunctionsCount` |
| 1040 | `int32 Size` |

**`EnumInfoEx`**: `char Name[256]`, `char FullPath[512]`, `int32 NumValues`.

### 1.5 Constants

| Constant | Value |
| :--- | :--- |
| `PIPE_NAME` | `\\.\pipe\Avalonium` |
| `IOCP_PORT` | `20030` |
| `MAX_STRING_RESULT` | `4096` |
| `MAX_OBJECT_COUNT` | `100000` |

### 1.6 `EngineState`

The single discovery result object. Fields grouped by purpose; all offsets are
byte offsets or `-1` when unresolved.

**Identification / progress**

| Field | Meaning |
| :--- | :--- |
| `int UEMajor, UEMinor` | detected engine version |
| `char GameName[256]` | window title or executable name, sanitized for paths |
| `HWND GameWindow` | main game window |
| `void* OriginalWndProc` | saved WndProc when hooked |
| `bool bInitialized` | top-level success flag |
| `bGlobalsFound`, `bFunctionsFound`, `bCoreClassesFound`, `bOffsetsDiscovered`, `bFNameLayoutDiscovered`, `bIndexBuilt` | per-stage flags |
| `bUnsupportedVersion` | engine < 4.23; aborts retries |
| `char LastError[1024]` | human-readable failure reason |

**Globals**

| Field | Meaning |
| :--- | :--- |
| `GUObjectArray` | `FUObjectArray` instance |
| `GNames` | name pool |
| `GWorld` / `GWorldPtr` | world instance / module slot holding it |
| `GMalloc` / `GMallocLocation` / `GMallocName[32]` / `GMallocMallocSlot` | allocator instance, module-resident location, descriptive name, measured `Malloc` vtable slot (`-1` unmeasured) |
| `GNatives`, `GUObjectHashTables`, `GEngine` | engine globals |
| `IConsoleManager`, `IConsoleManager_TMapOff` | console manager + TMap offset |

**Core class objects**

`UObjectClass`, `UClassClass`, `UStructClass`, `AActorClass`, `APawnClass`,
`UEnumClass`, `UFunctionClass`, `UPackageClass`, `UWorldClass`.

**UObject layout**

| Field | Default | Meaning |
| :--- | :--- | :--- |
| `UObject_VTableOff` | `0x00` | vtable |
| `UObject_FlagsOff` | `0x08` | `EObjectFlags` |
| `UObject_IndexOff` | `0x0C` | internal index |
| `UObject_ClassOff` | `0x10` | `UClass*` |
| `UObject_NameOff` | `0x18` | `FName` |
| `UObject_OuterOff` | `0x20` | outer pointer |

**UStruct / UField layout** (discovered; `-1` = unknown)`UStruct_SuperOff`, `UStruct_ChildrenOff`, `UStruct_ChildPropsOff`,
`UStruct_PropertiesSizeOff`, `UStruct_MinAlignmentOff`, `UField_NextOff`.UStruct_PropertiesSizeOff` is resolved at the end of `ApplyFFieldLayoutFixups`, so it is
normally known once startup discovery finishes. It is confirmed against `FColor` = 4 and
`FGuid` = 16 where both structs are found, and otherwise derived from the member order in
`Public/UObject/Class.h`: `PropertiesSize` follows the pointer member before it
(`ChildProperties` from UE 4.25 onwards, `Children` on UE 4.23–4.24), so it sits one
pointer-width after that member. Both paths agree on UE 4.27 and UE 5.8, where they report
the same offset. `UStructPropertiesSizeOffset(state)` is the one reader of
that field: it returns the discovered offset and falls back to the member order, so no caller
keeps its own copy of the fallback expression.

`UStruct_MinAlignmentOff` is resolved right after it (`ResolveUStructMinAlignmentOffset`).
`MinAlignment` is the member `Class.h` declares immediately after `PropertiesSize`, `int32`
through UE 5.5 and `int16` from UE 5.6, both starting at the same offset, so the probe reads
a 16-bit word at `PropertiesSize + 4` and accepts it when the value is 1/2/4/8/16/32. It does
**not** require `PropertiesSize % alignment == 0`: `PropertiesSize` is the raw end of the last
property and need not be a multiple. There is no candidate walk; the result is `-1` only when
`PropertiesSize` itself is unknown, and consumers of the alignment stay silent then.

**FField / property layout**

`FField_ClassOff`, `FFieldClass_NameOff`, `FField_NameOff`, `FField_NextOff`,
`FProperty_OffsetOff` (default `0x4C`), `UProperty_OffsetOff` (default `0x44`),
`FProperty_ArrayDimOff`, `FProperty_ElementSizeOff`,
`FProperty_PropertyFlagsOff`, then per-container/type members:
`FArrayProperty_InnerOff`, `FStructProperty_StructOff`,
`FObjectProperty_ClassOff`, `FEnumProperty_UnderlyingOff`,
`FEnumProperty_EnumOff`, `FMapProperty_KeyPropOff`,
`FMapProperty_ValuePropOff`, `FSetProperty_ElementPropOff`,
`FOptionalProperty_ValuePropOff`, `FByteProperty_EnumOff`,
`FBoolProperty_BaseOff`, `FClassProperty_MetaClassOff`.

**Enum / class / function metadata**

`UEnum_NamesOff`, `UEnum_EnumFlagsOff`, `UClass_ClassFlagsOff`,
`UScriptStruct_StructFlagsOff`, `UStruct_ScriptOff`,
`UClass_ClassDefaultObjectOff`, `UClass_InterfacesOff`,
`UFunction_FuncOff`, `UFunction_NumParmsOff`, `UFunction_ParmsSizeOff`,
`UFunction_ReturnValueOffsetOff`, `UFunction_RPCIdOff`,
`UFunction_RPCResponseIdOff`, `UFunction_FlagsOff`.

**FName layout**

`FName_Size` (8), `FName_CompIdxOff` (0), `FName_NumberOff` (4),
`bUseCasePreservingName`, `bUseOutlineNumber`, `bUseFProperty`,
`bUseMaskForFieldOwner`, `FName_ChunksStart` (0x10), `FName_HeaderOff` (0),
`FName_Stride` (2), `FName_LenShift` (6), `FName_LenMask` (0x3FF).
`FName_IsIndirect` selects the block-array indirection used by UE5 pools, and
`FName_BlockOffsetBits` is the block-offset width used to split indexes.
`bIsNewUE5Enum` distinguishes the 5.x enum representation.

**World**

`UWorld_PersistentLevelOff`, `ULevel_ActorsOff`.

**ProcessEvent**

| Field | Meaning |
| :--- | :--- |
| `ProcessEvent_VTableIdx` | resolved vtable slot |
| `ProcessEvent_UObjectImpl` / `ProcessEvent_AActorImpl` | the two implementations named by law |
| `ProcessInternal`, `ProcessLocalScriptFunction` | VM entries |

**Function pointers**

`FNameToString`, `GetFullName`, `StaticConstructObject`,
`bStaticConstructTakesParams` (layout of the constructor call).

**Object array layout**

`FUObjectItem_Size` (0x18), `FUObjectItem_ObjectOff` (0),
`TUObjectArray_ObjectsOff`, `TUObjectArray_NumElementsOff`,
`TUObjectArray_MaxElementsOff`, `UObjectArray_NumElements`,
`UObjectArray_NumChunks`, `UObjectArray_Chunks`,
`UObjectArray_ItemSize` (24), `UObjectArray_NumElementsPerChunk`
(measured; 0 = use source constant 64·1024).

**Indexes / caches**

| Field | Meaning |
| :--- | :--- |
| `NameToFirstObject` | FName comparison index → first object |
| `NameToObjects` | index → all objects |
| `ClassToInstances` | class → instances (startup snapshot) |
| `FNameCache`, `FNameCacheArray`, `FNameThreadArenas`, `StringToFNameIndex` | name decode caches |
| `SortedNames` | `(name, object)` pairs used by prefix search |
| `LiveLookupGeneration`, `LiveLookupCacheStamp`, `LiveClassLookups` | per-frame live lookup memo |
| `FunctionByFullKey`, `FunctionByNameOnly`, `LiveUFunctions`, `ClassByName` | reflection caches |
| `CachedPlayerControllerClass`, `FunctionCacheLastCount` | helpers for `UObj` layer |
| `IndexMutex`, `ReflectionCacheMutex` | locks (shared_ptr so copies share) |

---

## 2. `Logger.h`

```cpp
namespace EngineDiscovery {
void SetLogPath(const std::string& path);
std::string GetLogPath();
bool IsVerboseLoggingEnabled();
void SetRuntimeVerboseLogging(bool enabled);
void ResetSdkLogTimeBaseline();
std::chrono::milliseconds GetSdkLogElapsed();
void WriteSdkLogLine(const char* line);   // [SDK][+ms] prefix
void WriteLogLine(const char* line);      // [+ms] prefix
}
void Log(const char* fmt, ...);           // printf-style → WriteLogLine
```

* Timestamps are milliseconds since process start.
* `VerboseLog`/`verbose_log` gate `[Debug]` output via
  `IsVerboseLoggingEnabled()`; SDK generation uses its own baseline/timing
  (`ResetSdkLogTimeBaseline`, `GetSdkLogElapsed`, `WriteSdkLogLine`).

---

## 3. `PatternScanner.h`: `namespace PatternScan`

```cpp
uintptr_t FindPattern(uintptr_t start, size_t size, const char* pattern);
uintptr_t FindPatternInModule(HMODULE module, const char* pattern);
uintptr_t FindPatternInExecutableSections(HMODULE module, const char* pattern);
void*     FindExport(HMODULE module, const char* symbolName);
uintptr_t FindStringRef(HMODULE module, const char* str);
uintptr_t FindWStringRef(HMODULE module, const wchar_t* str);
uintptr_t ResolveRelative(uintptr_t instructionAddr, int32_t offset, int instructionSize);

struct ModuleInfo { HMODULE handle; uintptr_t base; size_t size; std::string name; };
std::vector<ModuleInfo> GetLoadedModules();
ModuleInfo GetModuleByName(const char* name);
ModuleInfo GetMainModule();
std::string ExtractGameName(const std::string& moduleName);
```

| Function | Semantics |
| :--- | :--- |
| `FindPattern` | IDA-style pattern (`"48 8B ?? E8"`) over a raw range; returns absolute address or 0 |
| `FindPatternInModule` | scans all committed readable regions of the module |
| `FindPatternInExecutableSections` | scans only executable sections (slower but fewer false positives) |
| `FindExport` | `GetProcAddress` wrapper; null-safe |
| `FindStringRef` / `FindWStringRef` | finds a `lea`/reference to a string in the module's code |
| `ResolveRelative` | resolves a rel32 displacement to an absolute address |
| `GetLoadedModules` | snapshot via `EnumProcessModules` (fallback to `K32EnumProcessModules`), with base/size/name |
| `GetModuleByName` | case-insensitive name lookup (`"d3d12.dll"`) |
| `GetMainModule` | the executable (not Avalonium) |
| `ExtractGameName` | strips `-Win64-Shipping`, `.exe`, etc. for folder naming |

---

## 4. `EngineDetection.h`

```cpp
bool DetectVersion(EngineState& state);
bool FindGUObjectArray(EngineState& state);
bool FindGNames(EngineState& state);
bool FindGWorld(EngineState& state);
bool FindGMalloc(EngineState& state);
bool IsValidMallocInstance(void* instance);
bool GetAllocatorDescriptiveName(void* instance, char (&outName)[64], int& outSlot);
void* ProbeAllocatorMalloc(void* instance, int32_t slot, size_t bytes);
bool FindGNatives(EngineState& state);
bool FindGUObjectHashTables(EngineState& state);
bool IsValidFNamePool(EngineState& state, void* candidate);
bool FindGlobals(EngineState& state);
bool FindGEngine(EngineState& state);   // declared in FunctionFinder.h
```

| Function | Semantics |
| :--- | :--- |
| `DetectVersion` | reads version evidence (RTTI/sections/strings) to set `UEMajor/UEMinor` |
| `FindGUObjectArray` | AOB pattern first unless `SkipPatternScan`; when that comes back empty, I fall back to the obfuscation scan, which does the dirty work; validates the candidate against live objects |
| `FindGNames` | name-pool candidates validated by `IsValidFNamePool` |
| `FindGWorld` | finds the global slot (`GWorldPtr`) and the current world |
| `FindGMalloc` | resolves the instance + module location + `GetDescriptiveName` (`GMallocName`) and measures the `Malloc` vtable slot from engine call sites |
| `IsValidMallocInstance` | canonical pointer, readable vtable, first four slots canonical code, not inside a module, has a descriptive-name getter |
| `GetAllocatorDescriptiveName` | reads the literal returned by the allocator's name getter; reports the slot |
| `ProbeAllocatorMalloc` | calls one measured Malloc slot for a small block and accepts it only if the returned block is canonical, mapped and writable; the probe block is deliberately leaked (one per session) |
| `FindGNatives` / `FindGUObjectHashTables` | global table addresses |
| `FindGlobals` | orchestrates all of the above; sets `bGlobalsFound` |

### 4.1 Malloc-slot measurement (`AllocatorSlotScan.h`)

`FMalloc::Malloc`'s vtable index is not assumed. `FindGMalloc` locates
`mov reg,[rip+GMalloc]` call sites and runs `AnalyzeAllocatorCallSite` on each:
the code is followed through the null-check branch to its first indirect call,
and the argument shape identifies the slot. `FMalloc::Malloc` passes a byte
count in `rdx` and a constant alignment in `r8` while leaving `r9` alone;
`Realloc`/`TryRealloc` fill `r9`, and `Free` passes only a pointer. The vote
therefore separates `Malloc` from `Free` without depending on the `FExec` base
class's virtual count (two through UE 5.3, more from UE 5.4). The chosen slot is
then proven by `ProbeAllocatorMalloc`; if the probe fails, I keep engine allocation
disabled rather than call some random slot and pray it's `Malloc`.

---

## 5. `EngineDiscovery.h`: façade

### Window / lifecycle

```cpp
std::string GetMainWindowTitle();
bool FindGameWindow(EngineState& state);       // sets GameWindow, resizes checks
bool Initialize(EngineState& state, bool skipConsoleManager = false);
```

### Globals / offsets / functions

```cpp
bool DetectVersion(EngineState&);
bool FindGlobals(EngineState&);
bool FindGUObjectArray(EngineState&); bool FindGNames(EngineState&);
bool FindGWorld(EngineState&);        bool FindGMalloc(EngineState&);
bool FindGNatives(EngineState&);      bool FindGUObjectHashTables(EngineState&);
bool DiscoverOffsets(EngineState&);
bool DiscoverUObjectLayout(EngineState&);
bool DiscoverUStructLayout(EngineState&);
bool FindEngineFunctions(EngineState&);
bool FindFNameToString(EngineState&);
bool FindGetFullName(EngineState&);
bool FindStaticConstructObject(EngineState&);
bool FindConsoleManager(EngineState&);
bool FindCoreClasses(EngineState&);
bool FindProcessEvent(EngineState&);
bool FindProcessInternal(EngineState&);
bool FindProcessLocalScriptFunction(EngineState&);
```

### Index / lookups / dispatch

```cpp
void BuildObjectIndex(EngineState&);
UObject* FindObjectByNameFast(EngineState&, const std::string&);
std::vector<UObject*> FindObjectsByNameFast(EngineState&, const std::string&);
std::vector<UObject*> FindObjectsByClassFast(EngineState&, UClass*);
std::vector<UObject*> FindObjectsByPrefix(EngineState&, const std::string&);
std::string GetCachedFName(EngineState&, int32_t nameIdx);
int32_t ResolveFNameIndex(EngineState&, const std::string& name);
void* FindFunctionByName(const EngineState&, UStruct* cls, const char* funcName);
bool CallProcessEvent(const EngineState&, void* object, void* ufunction, void* params);
bool GenerateUSMap(const EngineState&, const std::string& outputPath);
```

`CallProcessEvent` is SEH-wrapped: I catch, log and return `false` on a fault inside the
engine call, so one mod's half-baked argument list can't drag the whole game down with it.
While the loader dispatches, a thread-local marker makes
`IsLoaderDispatchedCall()` true so hooks can distinguish the loader's own calls
from engine calls.

---

## 6. `OffsetDiscovery.h`

```cpp
bool DiscoverUObjectFlagsOffset(EngineState&);
bool DiscoverUObjectLayout(EngineState&);
bool DiscoverUStructLayout(EngineState&);
bool DiscoverOffsets(EngineState&);
```

* `DiscoverUObjectLayout` probes vtable/class/name/outer positions against live
  objects, cross-checking `GetObjectNameStr` results.
* `DiscoverUStructLayout` probes super/children/child-props/properties-size and
  `UField::Next`.
* `DiscoverOffsets` runs the full sequence, including FProperty offsets, FName
  layout and container member offsets; sets `bOffsetsDiscovered`.

---

## 7. `ObjectArray.h`

```cpp
bool DiscoverTUObjectArrayLayout(EngineState&);
int32_t GetElementsPerChunk(const EngineState&);
int32_t GetObjectCount(const EngineState&);
UObject* GetObjectByIndex(const EngineState&, int32_t index);
typedef bool (*ObjectIteratorCallback)(UObject* object, int32_t index, void* userdata);
int32_t ForEachObject(const EngineState&, ObjectIteratorCallback callback, void* userdata);
```

* Supports both chunked `TUObjectArray` (`Objects`/`NumElements`/`MaxElements`
  with `UObjectArray_Chunks`) and the flat layout, plus `FUObjectItem::Object`
  offset/stride.
* `GetElementsPerChunk` returns the measured `NumElementsPerChunk` or the
  engine's `64 * 1024` constant when unmeasured.
* `ForEachObject` stops early when the callback returns `false`; returns the
  number of visited objects.

---

## 8. `NameResolution.h`

```cpp
void BuildFNameCache(EngineState&);
std::string DecodeFNameEntryString(const EngineState&, uint8_t* data, size_t offset, uint16_t meta);
std::string FNameToStr(const EngineState&, const FName&);
std::string GetObjectNameStr(const EngineState&, UObject*);
std::string BuildPathName(const EngineState&, UObject*);
std::string BuildFullName(const EngineState&, UObject*);
bool DiscoverFNameLayout(EngineState&);
```

* `BuildPathName`: outer chain, outermost first, joined with `.` between a
  link and its outer and `:` for subobjects of a top-level object.
* `BuildFullName`: `ClassName Path`, the engine's own full-name form.
* `DiscoverFNameLayout` detects width (4/8/12), case-preserving blocks,
  outline numbers, header/length bit layout (`FName_Stride`, `FName_LenShift`,
  `FName_LenMask`), and sets `bFNameLayoutDiscovered`. I make it probe,
  because the FName layout shifts every few builds and Epic won't tell
  anyone shit.

---

## 9. `ObjectUtils.h`

```cpp
UClass*  GetObjectClass(const EngineState&, UObject*);
UObject* GetObjectOuter(const EngineState&, UObject*);
UStruct* GetSuperStruct(const EngineState&, UStruct*);

void* GetChildrenHead(const EngineState&, UStruct*);
void* GetFieldNext(const EngineState&, void* field);
std::string GetFieldName(const EngineState&, void* field);
void* GetChildPropsHead(const EngineState&, UStruct*);
void* GetFFieldNext(const EngineState&, void* ffield);
std::string GetFFieldName(const EngineState&, void* ffield);
std::string GetFFieldTypeName(const EngineState&, void* ffield);

void RecordPropertyTypeClass(const std::string& typeName, void* classInstance);
void* GetRecordedPropertyTypeClass(const std::string& typeName);
int32_t GetPropertyAlignment(const EngineState&, void* property, int32_t valueSize);
void ApplyFFieldLayoutFixups(EngineState&);
void DiscoverFPropertyOffsetInternal(EngineState&);

int32_t UStructPropertiesSizeOffset(const EngineState&);
bool ResolveUStructMinAlignmentOffset(EngineState&);
```

* Both member chains are walked: `FField` (`UStruct::ChildProperties`, 4.25+)
  and classic `UField` (`UStruct::Children`, pre-4.25). `GetChildPropsHead`
  falls back automatically.
* `GetFFieldTypeName` names the property's type from its `FFieldClass`.
* **Property type identity cache**: `RecordPropertyTypeClass` is filled as a
  side effect of the reflection walk; the Lua compatibility layer reads it back
  (`PropertyTypes`).
* **Alignment heuristic**: `GetPropertyAlignment` derives the tightest
  alignment consistent with every placement of that property class in the live
  graph (offset and element size are always multiples of it); falls back to the
  provided `valueSize` when no placement exists. Results cached per class/struct.
* `ApplyFFieldLayoutFixups` corrects a mis-detected `FField` layout using
  cross-checks against live fields; `DiscoverFPropertyOffsetInternal` probes
  the property offset field if it is still unknown. It also resolves
  `UStruct_PropertiesSizeOffset` as its last step, once `bUseFProperty` and
  `UStruct_ChildPropsOff` are both settled.
* `ResolveUStructPropertiesSizeOffset` finds `UStruct::PropertiesSize`,
  confirming against `FColor` and `FGuid` when those can be found and falling back to the
  declared member order otherwise. Idempotent: an offset restored from the encrypted cache is
  left alone.
* `UStructPropertiesSizeOffset` is the shared accessor for that offset (discovered value, else
  `ChildProperties`/`Children` + one pointer); every size read, SDK generation, live SDK
  validation, goes through it.
* `ResolveUStructMinAlignmentOffset` reads `UStruct::MinAlignment` at `PropertiesSize + 4`.
  Both are called at the end of `ApplyFFieldLayoutFixups`, MinAlignment last because it reads
  `PropertiesSize`.

---

## 10. `WorldDiscovery.h`

```cpp
bool DiscoverWorldLayout(EngineState&);
UObject* GetLiveWorld(EngineState&);
UObject* GetPersistentLevel(const EngineState&);
int32_t GetWorldActors(const EngineState&, void** outActors, int32_t maxCount);
```

* `GetLiveWorld` reads the discovered slot first (it follows level
  transitions) but only accepts it when the object's class chain contains the
  resolved world class; otherwise it falls back to the world instance discovery
  recovered. I keep the class check because level transitions don't give a shit about cached pointers.
* `GetPersistentLevel` reads `UWorld::PersistentLevel`.
* `GetWorldActors` enumerates `ULevel::Actors` (the `TArray`) up to `maxCount`.

---

## 11. `FunctionFinder.h`

### Engine-law resolution

```cpp
bool ResolveProcessEventImplementations(EngineState&, int slotIdx,
                                        void** uobjectImpl, void** aactorImpl, bool* distinct);
int  FindDirectBranchTo(void* from, void* to, int window = 0x400);
int  FindFuncNativeFlagsOffset(void* fn, int window = 0x800);

struct ProcessEventSlotLaw {
  int slot; void* uobjectImpl; void* aactorImpl;
  int overrideBranchOffset; int flagsOffset;
  int objectReads, objectAgree, actorReads, actorAgree;
};
bool FindProcessEventSlotByEngineLaw(EngineState&, int slotLo, int slotHi, ProcessEventSlotLaw* out);

void ClearInheritanceCache();
```

* A slot is only accepted when **both** declared implementations exist, the
  AActor override branches into the UObject implementation (direct `E8/E9
  rel32`), and both test `FUNC_Native` at the same `UFunction` flags offset.
  Population agreement with the object array ranks ties.

### Others

```cpp
bool IsStructA(const EngineState&, UStruct* type, UStruct* want);
bool FindFNameToString(EngineState&);
bool FindGetFullName(EngineState&);
bool FindGEngine(EngineState&);
bool FindStaticConstructObject(EngineState&);
bool FindConsoleManagerHeuristic(EngineState&);
bool FindConsoleManager(EngineState&);
bool FindEngineFunctions(EngineState&, bool skipConsoleManager);
bool FindCoreClasses(EngineState&);
bool FindProcessEvent(EngineState&);
bool FindProcessInternal(EngineState&);
bool FindProcessLocalScriptFunction(EngineState&);
void* FindFunctionByName(const EngineState&, UStruct* cls, const char* funcName);
bool CallProcessEvent(const EngineState&, void* object, void* ufunction, void* params);
bool IsLoaderDispatchedCall();
```

* `IsStructA` is an identity-cached "derives from" check used throughout.
* `FindProcessEvent` chooses the vtable slot: engine-law resolution first, then
  a vtable fallback; records both implementations.
* `FindProcessInternal`/`FindProcessLocalScriptFunction` read the VM entries
  from live reflection (every bytecode `UFunction::Func` holds
  `&UObject::ProcessInternal`; its local branch calls
  `ProcessLocalScriptFunction`).
* `FindConsoleManager` locates `IConsoleManager` by heuristic + validation;
  `skipConsoleManager` avoids the cost when the console is disabled.
* `FindFunctionByName` resolves by full key first, then name-only, then a class
  walk; functionality used by IPC/Lua `CallFunction`.

---

## 12. `ObjectIndex.h`

```cpp
void BuildObjectIndex(EngineState&);
UObject* FindObjectByNameFast(EngineState&, const std::string&);
std::vector<UObject*> FindObjectsByNameFast(EngineState&, const std::string&);
std::vector<UObject*> FindObjectsByClassFast(EngineState&, UClass*);
std::vector<UObject*> FindLiveObjectsByClass(EngineState&, UClass*);
void AdvanceLiveLookupGeneration(EngineState&);
std::vector<UObject*> FindObjectsByPrefix(EngineState&, const std::string&);

UObject* FindObjectByPath(EngineState&, const std::string& query);
std::string StripTypeTokenWrapper(const std::string& query);
UObject* ResolveObjectPath(EngineState&, const std::string& query,
                           UClass* requiredClass = nullptr,
                           bool exactClass = false,
                           UObject* withinOuter = nullptr);

std::string GetCachedFName(EngineState&, int32_t nameIdx);
int32_t ResolveFNameIndex(EngineState&, const std::string& name);

void RebuildFunctionCache(EngineState&);
int32_t DeltaUpdateFunctionCache(EngineState&);
UFunction* FindFunctionCached(EngineState&, const std::string& lookup);
UClass* FindClassCached(EngineState&, const std::string& className);
void InvalidateObjectFromCache(EngineState&, UObject*);
```

* `FindLiveObjectsByClass` reads the object array **as it is now** and is
  memoized per frame (`AdvanceLiveLookupGeneration`); newly created objects are
  visible immediately, unlike the startup `ClassToInstances` snapshot. We keep no stale cache
  when the game spawns something mid-frame, the array is read as it is now.
* `ResolveObjectPath` parses engine path forms: `"/Script/Mod.Default__Thing"`,
  `"/Game/X.Y"`, `"SomePackage.SomeObject"`, renderer full names
  (`"ClassName Path"`), `"Blueprint'/Game/Foo.Foo_C'"` wrappers via
  `StripTypeTokenWrapper`, and `withinOuter` for engine-style outer lookups.
* Function caches: `RebuildFunctionCache` (full rebuild) and
  `DeltaUpdateFunctionCache` (incremental; returns number added). Lookup
  accepts `"Class:Function"` and bare names.

---

## 13. `BlueprintDiscovery.h`

Everything a Blueprint/asset mod needs, resolved from the process (no bullshit, no version
tables).

### Reflected members / names / frames

```cpp
struct ReflectedMember {
    void* Property; std::string Name, TypeName;
    int32_t Offset, ElementSize, ArrayDim; uint64_t Flags;
    void* Struct;    // FStructProperty::Struct
};
bool GetReflectedMembers(const EngineState&, UStruct* owner, std::vector<ReflectedMember>& out);

struct FNameBytes { uint8_t Bytes[16]; int32_t Size; };
bool FindOrAddFName(EngineState&, const std::string& text, FNameBytes& out, std::string* detail = nullptr);
int32_t FindOrAddFNameIndex(EngineState&, const std::string& text, std::string* detail = nullptr);
bool WriteFNameBytes(EngineState&, uint32_t index, uint32_t number, void* dest, int32_t width);
bool CanCreateFNames(EngineState&);

int32_t GetFunctionFrameSize(const EngineState&, void* ufunction);
int32_t GetFunctionReturnValueOffset(const EngineState&, void* ufunction);
bool GetFunctionParameter(EngineState&, const char* className, const char* functionName,
                          const char* parameterName, ReflectedMember& out);

struct ReflectedArgument { const char* Parameter; const void* Value; int32_t Size; };
bool CallStaticFunctionByName(EngineState&, const char* className, const char* functionName,
                              const ReflectedArgument* args, int32_t argCount,
                              void* result, int32_t resultSize);
UObject* SpawnObjectThroughEngine(EngineState&, UClass* objectClass, UObject* outer);
```

* Names are created **through the engine's own name-table writer**, because
  the pool's probe bookkeeping/allocation cursor/lock are private, an entry
  written externally is not reachable by the engine's lookup. `CanCreateFNames`
  reports whether that writer was found; when false only existing names resolve.
* `FNameBytes` is filled in the build's own layout (width discovered at
  startup), including the display index on builds that carry one.
* Function frames follow `UFunction::InitializeDerivedMembers` (largest
  `CPF_Parm` offset+size), deliberately **not** `PropertiesSize`.
* `CallStaticFunctionByName` calls a reflected static function on the class
  default object, filling arguments **by parameter name**; unspecified
  parameters keep the zero the frame was built with.

### Asset registry binding

```cpp
struct AssetDataField { std::string Name, TypeName; void* Property; int32_t Offset, Size; };
struct AssetRegistryBinding {
    bool bReady; std::string Detail;
    UObject *HelpersClass, *HelpersCDO, *Registry;
    UStruct *AssetDataStruct; std::string AssetDataStructName;
    int32_t AssetDataSize, AssetDataOffset, GetAssetFrameSize;
    void *FnGetAsset, *FnGetAssetRegistry, *FnGetClass, *FnIsValid, *FnCreateAssetData;
    std::vector<AssetDataField> Fields;
    int32_t OffObjectPath, OffPackageName, OffPackagePath, OffAssetName,
            OffAssetClass, OffAssetClassPath;
};
const AssetRegistryBinding& GetAssetRegistryBinding(EngineState&);
bool BuildAssetData(EngineState&, const AssetRegistryBinding&, const std::string& packagePath,
                    const std::string& assetName, uint8_t* buffer, int32_t size,
                    std::string* detail = nullptr);
std::string DescribeAssetRegistryBinding(const AssetRegistryBinding&);
UObject* LoadAssetByPath(EngineState&, const std::string& objectPath, std::string* detail = nullptr);
```

* `UAssetRegistryHelpers::GetAsset` takes the engine's reflected `FAssetData`;
  the binding reads that structure from the function's own parameter, so the
  buffer size is always the engine's expectation.
* `BuildAssetData` writes every path member the build has
  (`ObjectPath` up to 5.1, `PackageName`/`AssetName` from 5.1 on), which lets
  one path serve both eras.
* `LoadAssetByPath` accepts `"/Game/Mods/X/ModActor.ModActor"` or the short
  form and loads packages the game never referenced.

### Blueprint classes

```cpp
UObject* FindClassDefaultObject(EngineState&, UClass* cls);
bool IsBlueprintClass(const EngineState&, UClass* cls);
UClass* GetBlueprintGeneratedClassObject(EngineState&);
std::vector<UClass*> FindBlueprintClasses(EngineState&, const std::string& superClassName);
```

* `FindClassDefaultObject` resolves by name (`Default__<Class>`), then the
  discovered `UClass::ClassDefaultObject` offset, then the class's instance
  list.
* `IsBlueprintClass` tests the class's own metaclass chain, so it holds for
  every Blueprint flavour without naming any.
* `FindBlueprintClasses` filters by super chain; empty `superClassName` drops
  the restriction.

---

## 14. `EncryptedCache.h`: `namespace EncryptedCache`

```cpp
std::wstring GetCacheFilePathW(HMODULE hModule, const char* gameName);
bool TryLoadSnapshot(EngineState& state, HMODULE hModule);
bool SaveSnapshot(const EngineState& state, HMODULE hModule);
```

Snapshot format (`SnapshotHeader` + `SnapshotData`, packed):

| Field group | Contents |
| :--- | :--- |
| Header | magic `UDCACHE1`, version `5`, struct size, PE timestamp/checksum/image size, game name, UE version |
| Globals | RVAs for `GUObjectArray`, `GNames`, `GWorldPtr`, the `GMalloc` pointer global, `GMallocName`, `GMallocMallocSlot`, `GNatives`, `GUObjectHashTables`, the `IConsoleManager` pointer global, `FName::ToString`, `StaticConstructObject` |
| Layout | every UObject/UStruct/FField/FProperty/FName/World/UFunction offset |

* The snapshot stores **module-relative RVAs**, so it survives ASLR.
* `TryLoadSnapshot` checks magic and `StructSize`, then compares the PE
  timestamp and image size. It does not compare the stored format version or
  `PECheckSum`, and it restores `GameName` rather than validating it; a
  mismatch falls back to full discovery. We treat the PE identity as the real gate, version
  and checksum fields are wishful thinking with better formatting.
* `GMallocName` re-verifies the cached allocator instance through
  `GetAllocatorDescriptiveName`; the cached `GMallocMallocSlot` is reused only
  after `ProbeAllocatorMalloc` returns a writable 64-byte block. A failure
  forces allocator rediscovery.
* `IConsoleManagerLocation_RVA` is the **pointer global**, not the heap
  instance. Restore re-reads it and accepts it only when the cached TMap offset
  still describes a live sparse map (`Data != null`, `0 < Num <= Max`), which
  avoids a BSS sweep on a cache hit.
* The file is encrypted on disk; the path derives from the DLL location + game
  name. Saving happens at the end of `Initialize` and when a restored session
  newly resolves `IConsoleManager`.

---

## 15. `RuntimeConfig` (summary)

See [RuntimeAPI/CONFIG.md](../RuntimeAPI/CONFIG.md) for the full key reference.

```cpp
struct RuntimeFeatureConfig { /* every INI key */ };
namespace RuntimeConfig {
const RuntimeFeatureConfig& Load(HMODULE module);  // once, cached
const RuntimeFeatureConfig& Get();                 // Load(nullptr) if needed
}
```

---

## See also

* [DiscoveryAPI/README.md](README.md): pipeline overview.
* [RuntimeAPI/EXPORTS.md](../RuntimeAPI/EXPORTS.md): the public C surface over
  this layer.
* [LuaAPI/UOBJECT.md](../LuaAPI/UOBJECT.md): how reflection is exposed to Lua.
* [SafeMemAPI/REFERENCE.md](../SafeMemAPI/REFERENCE.md): memory primitives.
</content>
