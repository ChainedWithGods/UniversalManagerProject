# RuntimeAPI: Exported C Functions

All symbols below are `extern "C" __declspec(dllexport)` from `Avalonium.dll`
(header: `Runtime/api/UEInternalAPI.h`, implementation:
`Runtime/api/UEInternalAPI.cpp`), except the C++ mod API entries, which are
declared in `Runtime/cppmods/sdk/Avalonium/ModApi.h` and
`Runtime/cppmods/sdk/Avalonium/SdkApi.h` and implemented in
`Runtime/cppmods/CppModApi.cpp`. The release build's export table contains
exactly this set; there are no ordinal-only exports.

Get them with `LoadLibrary` + `GetProcAddress` (names are undecorated).

```cpp
typedef bool         (*Initialize_t)();
typedef const char*  (*GetString_t)();
typedef void*        (*GetPtr_t)();
typedef int          (*GetObjects_t)(void** out, int maxCount);
```

---

## Lifecycle

### `bool Initialize()`

Runs engine discovery if it has not run yet.

* Idempotent: returns `true` immediately when `g_State.bInitialized`.
* Guarded by an internal mutex; safe to call from one thread while other
  threads call lookup functions.
* Loads `Avalonium.ini` through `RuntimeConfig::Load(g_Module)` and passes
  `skipConsoleManager = !EnableUnrealConsole` to discovery.
* Returns `false` on failure; the reason is in `GetLastErrorMsg()`.

> `Initialize` performs discovery only. The DllMain init thread owns overlay
> startup, IPC start, Lua startup and console enabling. An external caller
> normally only needs `Initialize` for the reflection surface.

### `void Shutdown()`

Resets the entire shared state: `g_State = EngineState()`. Does **not** stop
the IPC server, unhook the overlay, or close Lua. Call it only when the caller
owns the runtime's lifetime.

### `const char* GetLastErrorMsg()`

Returns `g_State.LastError`, a 1024-byte buffer describing the last discovery
failure (e.g. `"Failed to find engine globals"`,
`"Unsupported Unreal Engine version 4.22 (minimum: UE4.23)"`). Never null;
empty when there is no error.

### `const char* GetStatus()`

Returns a multi-line status block from a shared 2048-byte buffer:

```text
Game: <GameName>
Initialized: YES|NO
Version: UE<major>.<minor>
GUObjectArray: <ptr>
GNames: <ptr>
GWorld: <ptr>
GMalloc: <ptr>
GNatives: <ptr>
GUObjectHashTables: <ptr>
GEngine: <ptr>
ProcessEvent: VTable[<idx>]
Class: <ptr>
Object: <ptr>
Actor: <ptr>
Pawn: <ptr>
Enum: <ptr>
Function: <ptr>
Package: <ptr>
Struct: <ptr>
World: <ptr>
Objects: <count>
Globals: YES|NO
Functions: YES|NO
CoreClasses: YES|NO
Offsets: YES|NO
SuperOff: 0x<hex>
ChildrenOff: 0x<hex>
UFieldNextOff: 0x<hex>
ChildPropsOff: 0x<hex>
FFieldNameOff: 0x<hex>
FFieldNextOff: 0x<hex>
```

**Pointers carry no `0x` prefix**: MSVC `%p` prints bare uppercase-padded hex
(`0000029316B92140`). The four `*Off` fields are formatted with `0x%X` and *do*
carry the prefix. Live example (UE4.27):

```text
Game: ExampleGame
Initialized: YES
Version: UE4.27
GUObjectArray: 00007FF6E52AC668
GNames: 00007FF6E5270340
GWorld: 000002933A3D5160
GMalloc: 0000029314B46DC0
GNatives: 00007FF6E5388EC8
GUObjectHashTables: 00007FF6E53F0498
GEngine: 000002932428E020
ProcessEvent: VTable[68]
Objects: 74045
Globals: YES
Functions: YES
CoreClasses: YES
Offsets: YES
SuperOff: 0x40
ChildrenOff: 0x48
UFieldNextOff: 0x28
ChildPropsOff: 0x50
FFieldNameOff: 0x28
FFieldNextOff: 0x20
```

If `UClass` was not resolved, `Class:` falls back to `UObject`.

### `const char* GetDetectedVersion()`

Returns `"UE<major>.<minor>"` from a 64-byte shared buffer (`"UE5.3"`,
`"UE4.27"`). Returns `"UE0.0"` before discovery.

---

## Discovered globals

| Function | Returns |
| :--- | :--- |
| `void* GetGUObjectArray()` | `FUObjectArray` instance |
| `void* GetFNamePool()` | name pool (`GNames`) |
| `void* GetGWorld()` | live `UWorld*` value (`GWorld`); the pointer-to-global storage is kept internally as `GWorldPtr` |
| `void* GetGMalloc()` | `FMalloc` instance |
| `void* GetGNatives()` | native function table |
| `void* GetGUObjectHashTables()` | object hash tables (`GUObjectHashTables`) |

These are raw process addresses; they are `nullptr` before initialization or
when discovery could not resolve them.

---

## Logging

### `void SetLogPath(const char* path)`

Redirects the runtime log to `path`. Equivalent to
`EngineDiscovery::SetLogPath`. Passing `nullptr` is ignored.

---

## Object enumeration

All enumeration functions require `g_State.bInitialized`, a non-null output
array and `maxCount > 0`; they return `0` otherwise.

### `int GetAllUClasses(void** outClasses, int maxCount)`

Writes up to `maxCount` `UClass` objects (modern engines: instances whose class
is `UClass`; legacy fallback: objects whose metaclass is themselves).

**Objects carrying `RF_ClassDefaultObject` or `RF_ArchetypeObject` are excluded.**
"What is this object's class" cannot tell a class from the class-default object
of `UClass`: a CDO is an *instance* of its own class, so both answer `UClass`.
Left unchecked, the CDO of `UClass` is admitted and appears as a phantom root,
named `Default__Class`, with an empty superclass and no members, because it is not
a class at all. The engine separates the two by flag rather than by class
(`UClass::CreateDefaultObject` allocates a CDO with
`RF_Public|RF_ClassDefaultObject|RF_ArchetypeObject`, and tests for a CDO with
`HasAnyFlags(RF_ClassDefaultObject)`), so this does too. Measured: 2460 entries
before, 2459 after, on UE 4.27.

If `UObject_FlagsOff` was never resolved the exclusion cannot be evaluated and
every class is returned, the flags are not assumed to be zero.

### `int GetAllActors(void** outActors, int maxCount)`

Writes up to `maxCount` live actors. Uses the class→instances index when built,
otherwise a full object-array walk; a class is accepted when it is `AActor` or
derives from it.

### `int GetWorldActors(void** outActors, int maxCount)`

Writes actors of the **current** persistent level (delegates to
`EngineDiscovery::GetWorldActors`). Prefer this over `GetAllActors` when you
want only the loaded world's actors.

### `int GetAllEnums(void** outEnums, int maxCount)`

Writes `UEnum` objects (class name `Enum` / `UserDefinedEnum` fallback).

### `int GetAllFunctions(void** outFunctions, int maxCount)`

Writes `UFunction` objects (class name `Function` fallback).

### `int GetObjectCount()`

Number of live objects in the object array (chunked walk).

---

## Name helpers

### `const char* GetObjectName(void* obj)`
### `const char* GetObjectFullName(void* obj)`
### `const char* GetUClassName(void* uclass)`

Return pointers into a shared 4096-byte static buffer (`g_StringBuf`),
overwritten by each call. `GetUClassName` is `GetObjectName` on a class
pointer. Return `""` for null objects or before initialization.

`GetObjectFullName` returns the engine's full-name form:
`ClassName Path` (e.g. `BP_Player_C /Game/Mods/ModActor.ModActor_C`).

### `int GetObjectNameSafe(void* obj, char* outBuf, int bufSize)`
### `int GetObjectFullNameSafe(void* obj, char* outBuf, int bufSize)`

Copy the same names into a caller buffer, always NUL-terminated, and return the
number of characters written (truncated to `bufSize - 1`). Return `0` when
`obj`, `outBuf` or `bufSize` is invalid, or before initialization.

**Use the safe variants from any multi-threaded context.**

---

## Class / inheritance helpers

### `bool InheritsFrom(void* uclass, const char* baseClassName)`

Walks the super chain (max depth 50) comparing object names to
`baseClassName`. Returns `false` when the super offset (`UStruct_SuperOff`) is
unresolved, for null arguments, or before initialization.

### `void* GetSuperClass(void* uclass)`

Returns the direct super struct/class pointer, or `nullptr`.

### `const char* GetInheritanceChain(void* uclass)`

Returns `"Child -> Parent -> GrandParent -> ..."` (max depth 50) from a shared
4096-byte buffer. Empty string for null input.

### `int GetClassProperties(void* uclass, PropertyInfoEx* outProps, int maxCount)`

Fills caller storage with reflected properties and returns the number of
entries written.

Two implementations are chosen by discovered layout:

* modern (`UStruct_ChildPropsOff >= 0` + `FField_NameOff >= 0`): walks the
  `FField` chain and **climbs the super chain**, so inherited properties are
  included; for each property it reads `Offset`, `Name`, `TypeName` and derives
  the `bIs*` flags; `ClassName` is the owner class segment.
* legacy: walks the `UField` chain of the requested class only; inherited
  properties are **not** included.

`PropertyInfoEx` (packed, 1568 bytes) layout:

| Field | Type | Notes |
| :--- | :--- | :--- |
| `Name[256]` | char | property name |
| `TypeName[256]` | char | e.g. `"IntProperty"` |
| `InnerType[256]`, `KeyType[256]`, `ValueType[256]` | char | reserved by the export (filled by richer APIs) |
| `ClassName[256]` | char | owning class segment |
| `Offset` | int32 | byte offset in the object |
| `Size`, `ElementSize`, `ArrayDim` | int32 | element metrics |
| `PropertyFlags` | uint64 | engine `EPropertyFlags` |
| `bIsArray/bIsMap/bIsSet/bIsStruct/bIsEnum/bIsObject/bIsBool/bIsString` | bool | type classification |

---

## Bulk class dump

### `const char* DumpAllClassesSharedMem(int* outTotalSize)`

Creates a named shared-memory section containing a snapshot of all UClasses for
**out-of-process** consumers.

* Name: `AvaloniumDump_<pid>` (returned pointer, 256-byte static buffer).
* Size: `4 + classCount * sizeof(ClassInfoEx)` =
  `4 + count * 1044` bytes; written to `*outTotalSize`.
* Layout: `int32 count` followed by packed `ClassInfoEx[count]`.
* `ClassInfoEx`: `Name[256]`, `FullPath[512]`, `SuperClassName[256]`,
  `Address` (8), `PropertiesCount`, `FunctionsCount`, `Size` (3×4).
* The file-mapping handle is intentionally kept for the process lifetime so a
  reader can open the section by name at any time; the export unmaps only its
  own view.

Returns `""` and `*outTotalSize = 0` when the runtime is not initialized or the
mapping cannot be created. Consume it with `OpenFileMappingA(FILE_MAP_READ, ...)`.

---

## Generators

All four require an initialized state, non-empty paths, and serialize on the
runtime mutex.

### `bool GenerateUSMapFile(const char* outputPath)`

Generates a `.usmap` mapping at `outputPath`
(see [UsmapAPI](../UsmapAPI/README.md)).

### `bool GenerateSDKFile(const char* outputPath)`

Generates the full C++ SDK into the `outputPath` directory
(see [SdkAPI](../SdkAPI/README.md)).

### `bool GenerateReflectionKit(const char* outputPath)`

Generates SDK + ReflectionDB bundle. Writes the SDK under
`<outputPath>\SDK` and the reflection database under
`<outputPath>\Reflection`.

### `bool GenerateReflectionDiffFile(const char* outputPath, const char* previousSnapshotPath, const char* currentSnapshotPath)`

Compares two reflection snapshots and writes `ReflectionDiff.json` into
`outputPath`. Returns `false` when any path is empty or a snapshot cannot be
loaded.

---

## C++ mod API

### `const AvaloniumModApi* AvaloniumGetModApi(uint32_t requested_version)`

Returns the runtime's versioned function table for native C++ mods. Returns null
when `requested_version` is below `AVALONIUM_MOD_API_MIN_VERSION` or above
`AVALONIUM_MOD_API_VERSION`. The table is owned by `Avalonium.dll` and lives for
the life of the process.

See [CppAPI](../CppAPI/README.md) for the table contents, lifecycle and mod layout.

### `const AvaloniumSdkApi* AvaloniumGetSdkApi(uint32_t requested_version)`

Returns the runtime table used by generated SDKs. It answers
`AVALONIUM_SDK_API_VERSION` only and returns null for every other version. The
table carries object enumeration and identity, class and function lookup, FName
text, class default objects and ProcessEvent, so a generated SDK carries no
engine addresses. The table is owned by `Avalonium.dll` and lives for the life
of the process.

See [SdkAPI](../SdkAPI/README.md#10-generated-sdk-runtime) for the table fields
and threading rules.

---

## Version history / compatibility

* The export table contains 34 functions. Additions are strictly additive; the
  order and set above is verified against the built release DLL's export table.
* `DumpAllClassesSharedMem` consumers should validate the `count` field and read
  exactly `4 + count * 1044` bytes.
* Functions returning `int` counts return `0` (not negative) on any validation
  failure.

## See also

* [IPC.md](IPC.md): same capabilities over TCP for out-of-process tools.
* [DiscoveryAPI](../DiscoveryAPI/README.md): semantics behind every pointer.
* [CONFIG.md](CONFIG.md): `Avalonium.ini` reference.
</content>
