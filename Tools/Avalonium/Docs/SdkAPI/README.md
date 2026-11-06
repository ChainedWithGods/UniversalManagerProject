# SdkAPI: C++ SDK Generator, Reflection Kit, Diff & Validation

The SdkAPI subsystem turns the live engine's reflection data (classes, structs, enums,
properties, functions) into a compilable **C++ SDK** plus machine-readable **reflection
databases**, and can validate a previously generated SDK against live memory.

The generated SDK carries no engine addresses. Its runtime layer resolves
`AvaloniumGetSdkApi` in Avalonium.dll, so object enumeration, class and function lookup,
name rendering and ProcessEvent follow the runtime's live discovery. The typed class and
struct layouts still describe the build the SDK was generated from; regeneration is needed
when that build's reflection or layouts change, not when only module addresses move.

| Component | Source | Role |
| --- | --- | --- |
| `SDKGenerator` | [`SDKGenerator.{h,cpp}`](../../Runtime/sdk/SDKGenerator.h), `SDKGenerator_*.{h,cpp}` | Generates the SDK, ReflectionKit, diff, metadata |
| `SDKValidation` | [`SDKValidation.{h,cpp}`](../../Runtime/sdk/SDKValidation.h) | Live-memory validation + libclang AST generation/verification |

Namespace `SDKGenerator` / `SDKValidation`.

---

## 1. Entry points

### In-process API

```cpp
namespace SDKGenerator {
    bool GenerateSDK(const EngineState& state, const std::string& outputDir);
    bool GenerateReflectionKit(const EngineState& state, const std::string& outputRootDir);
    bool GenerateReflectionDiff(const EngineState& state, const std::string& outputDir,
                                const std::string& previousSnapshotPath,
                                const std::string& currentSnapshotPath);
    void SdkLog(const char* fmt, ...);
}

namespace SDKValidation {
    void StartSDKValidationThread(const EngineState& state, const std::string& sdkPath);
    void StartSDKGenerationThread(const std::string& sdkPath, const std::string& gameName,
                                  const std::string& libclangPath);
}
```

### External triggers

| Path | Command / Call |
| --- | --- |
| IPC | `GENSDK [path]`, `GENDIFF [dir] [previous] [current]`, see [RuntimeAPI/IPC.md](../RuntimeAPI/IPC.md) |
| Lua | `GenerateSDK()`, `GenerateUHTCompatibleHeaders()` (compat parity), see [LuaAPI/COMPAT.md](../LuaAPI/COMPAT.md) |
| Startup | `[Features] SDKPath` + `SDKValidation = true` validates the given folder |

Output location: the path given to the command, or derived from the log directory and the
sanitized game name (see [RuntimeAPI/IPC.md](../RuntimeAPI/IPC.md#generator-commands)).

---

## 2. `GenerateSDK` pipeline

```
GenerateSDK(state, outputDir)                       [SdkFastMemScope → SafeMem fast mode]
  ├─ DiscoverUEnumNamesOffset(state)                (if not already known)
  ├─ USMapDiscovery::DiscoverPropertySubTypeOffsets(state) (probe/repair property offsets)
  ├─ Walk GUObjectArray once:
  │    enums → enums[]                              (values via ReadEnumValues)
  │    ScriptStructs → structs[]                    (CollectProperties)
  │    Classes → classes[]                          (CollectProperties + CollectFunctions)
  │    functions → funcs[]                          (ReadFunctionParams)
  ├─ Name/type resolution with caches:
  │    package name collision resolution, CPP keyword/Windows-reserved handling
  │    ("<pkg>_N" suffixes), type dependency tracking
  ├─ Emit C++:
  │    Basic.h                                      (core type declarations + runtime backend)
  │    UtfN.hpp, UnrealContainers.hpp               (support headers)
  │    PropertyFixup.hpp                           (after property collection: live FText/TLazy sizes)
  │    <Pkg>_structs.h / <Pkg>_classes.h            (type definitions)
  │    <Pkg>_parameters.h                           (parameter structs)
  │    <Pkg>_functions.cpp                          (function implementations)
  │    Assertions.inl                               (per-type macro, AVALONIUM_ASSERT_LAYOUT)
  │    SDK.h                                        (umbrella header)
  ├─ WriteMetadataManifest → SDKMetadata.json
  ├─ Reflection output (sibling "Reflection" dir):
  │    ReflectionDB.json
  │    Runtime/ReflectionHelpers.h/.cpp
  │    Inheritance.json, PackageGraph.json
  │    Diff/ReflectionDiff.json, Diff/ReflectionSnapshot.tsv
  └─ Post-steps (config-gated):
       GenerateBuildSystem → python GenerateBuild.py <sdkDir> --game-name <Game>
       SDKLibclangGeneration → StartSDKGenerationThread(...)
       SDKValidation → StartSDKValidationThread(...)
```

Generation runs under `SafeMem_SetFastMode(1)` (`SdkFastMemScope`), a single object-array pass
with caches.

### Output file inventory

| File | Contents |
| --- | --- |
| `Basic.h` | Core type declarations, the `AvaloniumGetSdkApi` backend client, `InSDKUtils`, reload-safe lookup helpers, and the `AVALONIUM_ASSERT_LAYOUT` switch |
| `UtfN.hpp`, `UnrealContainers.hpp`, `PropertyFixup.hpp` | Support headers included by `Basic.h` (UTF-N conversion, container equivalents with iterators, fixed-size enum templates) |
| `<Package>_structs.h` | `UScriptStruct` definitions (includes deps + `Basic.h`) |
| `<Package>_classes.h` | `UClass` definitions (includes `Basic.h`, own/dep structs, dep classes); `StaticClass()`/`GetDefaultObj()` are inline here |
| `<Package>_parameters.h` | Parameter structs for functions with parameters |
| `<Package>_functions.cpp` | Function implementations (`#include "SDK.h"` + package params) |
| `Assertions.inl` | One `AVALONIUM_ASSERTS_<Type>` macro per type (size, alignment, member offsets); included by `Basic.h`, expanded beside each definition when `CHECK_SDK_LAYOUT` is defined |
| `SDK.h` | Umbrella include list |
| `SDKMetadata.json` | Package summaries, dependencies and per-symbol metadata (§3) |
| `Reflection/ReflectionDB.json` | Enums, types, functions, properties |
| `Reflection/Runtime/ReflectionHelpers.h/.cpp` | C++ runtime helpers for the DB |
| `Reflection/Inheritance.json`, `PackageGraph.json` | Graphs |
| `Reflection/Diff/ReflectionDiff.json` | Diff against the previous snapshot |
| `Reflection/Diff/ReflectionSnapshot.tsv` | This run's snapshot (kind/id/hash/owner/name/extra) |
| `SDKValidation.log` | Validation report (validation runs only) |
| `SDKAstGeneration.log`, `SDKAstModel.json` | libclang runs only |

`GenerateReflectionKit(state, root)` creates `<root>` and calls
`GenerateSDK(state, root + "\\SDK")`, giving the canonical layout
`<root>/SDK/...` + `<root>/Reflection/...`.

> `ResolveReflectionOutputDir` places `Reflection/` as a **sibling of the SDK output dir**
> (the parent directory plus `\Reflection`). When dumping straight into `<root>/SDK`,
> reflection lands in `<root>/Reflection` as intended.

---

## 3. `SDKMetadata.json`

```json
{
  "generator": "Avalonium SDK",
  "format_version": 1,
  "game": "MyGame",
  "engine": "UE5.3",
  "object_count": 421337,
  "package_collision_count": 3,
  "total_files": 812,
  "failed_files": 0,
  "packages": [
    { "name": "Engine", "enums": 0, "structs": 321, "classes": 210,
      "functions": 0, "dependencies": ["CoreUObject"] }
  ],
  "symbols": [
    { "kind": "class", "package": "Engine", "name": "Actor", "cpp_name": "AActor",
      "full_path": "/Script/Engine.Actor", "super": "AActor", "hash": "0x…",
      "size": 0, "super_size": 0, "own_size": 0,
      "property_count": 0, "function_count": 0, "value_count": 0,
      "function_flags": 0 }
  ]
}
```

Symbols are sorted by (kind, package, full path, C++ name). `kind` values include `class`,
`struct`, `enum`, `function`. This is the file `SDKValidation` consumes.

---

## 4. Configuration

| INI key | Effect |
| --- | --- |
| `GenerateBuildSystem` (bool) | Run the build-system generator after dumping |
| `BuildScriptPath` | Path to `GenerateBuild.py` (empty = bundled Helpers discover) |
| `SDKLibclangGeneration` (bool) | Run the libclang AST generation + verification pipeline |
| `LibclangPath` | `libclang.dll` path (empty = auto-discover VS LLVM) |
| `SDKValidation` (bool) | Validate `SDKPath` at startup; post-dump validation receives the dump's own output directory |
| `SDKPath` | Folder containing `SDKMetadata.json` for startup validation |

Details: [RuntimeAPI/CONFIG.md](../RuntimeAPI/CONFIG.md).

---

## 5. Build-system generation

When `GenerateBuildSystem` is on and a script path is known:

```
python "<BuildScriptPath>" "<sdkOutputDir>" --game-name "<GameName>"
```

launched with `CREATE_NO_WINDOW`; success logged as `Build system generation launched for
game: <Game>`, failure as a warning with `GetLastError()`.

## 6. libclang generation & validation

`StartSDKGenerationThread(sdkPath, gameName, libclangPath)` (see
[REFERENCE.md](REFERENCE.md#part-2--libclang-ast-generation)):
loads `libclang.dll` (auto-discovery: VS Community/Professional/Enterprise/BuildTools LLVM
folders, `LLVM\bin`), builds a **unity translation unit** from all generated sources, parses
it, writes `SDKAstModel.json` and `SDKAstGeneration.log`, and reports errors. The raw SDK is
left untouched when the AST contains errors (`no files were changed`).

`StartSDKValidationThread(state, sdkPath)` walks live objects, matches them against
`SDKMetadata.json` by full path and reports PASS / WARN / FAIL with a verdict; new live
symbols increment a `NEW` count (the log gets an aggregate line, not one line per symbol), see
[REFERENCE.md](REFERENCE.md#part-3--live-memory-validation).

---

## 7. Threading & logging

* `GenerateSDK` is synchronous; keep it off the game thread when calling from tools.
* Validation and libclang generation run on their own threads. Validation deduplicates
  requests by normalized path (one in-flight or one completed per session); libclang
  generation spawns a new thread per request with no path deduplication.
* `SdkLog` writes `[SDK]` lines through the discovery logger with a time baseline reset at
  the start of each dump (`ResetSdkLogTimeBaseline`).
* Generation errors per file are counted and reported in the final summary
  (`Total Files: N (failed: M)`).

---

## 8. Source map

| File | Contents |
| --- | --- |
| `SDKGenerator.h` | public API |
| `SDKGenerator.cpp` | orchestration, Basic.h, per-package emission, SDK.h, manifest, post-steps, ReflectionKit, Diff entry |
| `SDKGenerator_Impl.h` | all record types, CPF_*/FUNC_* constants, caches, internal declarations |
| `SDKGenerator_Common.cpp` | naming, collisions, hashing (FNV-1a), `WriteFile`, cached getters, fingerprints |
| `SDKGenerator_TypeSystem.cpp` | C++ type mapping + dependency resolution |
| `SDKGenerator_Containers.cpp` | `TArray`/`TMap`/`TSet`/delegates rendering |
| `SDKGenerator_Bitfields.cpp` | bitfield rendering (storage width from `FBoolProperty::FieldSize`) |
| `SDKGenerator_LayoutRules.cpp` | bitfield width/bit placement, enum underlying types and fixed-size wrappers, array-dim repair, struct alignment, padding/member line formatting |
| `SDKGenerator_Cycles.cpp` | dependency cycle handling / typedef ordering |
| `SDKGenerator_Assertions.cpp` | `CHECK_SDK_LAYOUT` assertion generation |
| `SDKGenerator_CodeEmit.cpp` | field/function code emission |
| `SDKGenerator_Predefined.cpp` | predefined core class declarations/implementations |
| `SDKGenerator_Reflection.cpp` | ReflectionDB, runtime helpers, graphs, snapshots, diff |
| `SDKValidation.cpp` | validation thread, metadata loading, libclang pipeline |

---

## 9. SDK layout notes

The generated SDK keeps the following layout choices, without changing the file
naming or the package split:

| Import | What it does |
| --- | --- |
| Runtime backend | `Basic.h` resolves `AvaloniumGetSdkApi` in Avalonium.dll and calls the runtime for object enumeration, object identity, class and function lookup, default objects, FName text and ProcessEvent. The runtime measures every address and layout on the build the mod is loaded into, so the generated SDK carries no RVAs and no object-array or name-pool layout constants |
| `InSDKUtils` | `GetImageBase()` and `GetVirtualFunction<FuncType>(instance, index)` for mods that need their own native signatures; the SDK's own lookups do not use baked addresses |
| Reload-safe UFunction lookup | Every function body keeps a `SDKHelpers::ReloadableFuncInfo` and calls `GetCachedFunction`/`SetCachedFunction`: the cached `UFunction*` is revalidated against the object's index, name and outer name before use, and a lookup that finds nothing is **not** cached, so it is retried on the next call instead of staying dead for the session |
| Validated class lookup + inline `StaticClass()` | `StaticClass()` and `GetDefaultObj()` live in the class definition (`StaticClassInfo`, `GetStaticClass`), so a header alone is usable, no `_functions.cpp` has to be compiled for the reflection lookups, and the cached class is revalidated the same way |
| Per-type assertion macros | `Assertions.inl` defines `AVALONIUM_ASSERTS_<Type>`; every class and struct definition ends with `AVALONIUM_ASSERT_LAYOUT(<Type>);`. With `CHECK_SDK_LAYOUT` defined, a failure names the type and points at that type's own header instead of a line in one flat file, and one type can be checked on its own. The file is only read when the switch is on, so a normal build does not parse it |
| Struct size from `UStruct::GetStructureSize()` | A struct is embedded by value and used as an array element, so its `sizeof` is `Align(PropertiesSize, MinAlignment)` (`CoreUObject/Public/UObject/Class.h`); the body is padded explicitly to that size. A **base** whose packed body ends below its storage size is packed only when `alignas(MinAlignment)` restores the reported size (`FRootMotionSource` reports `0xA0`, its body ends at `0x98`, and its five derived structs place their first own member at `0x98`). The live `MinAlignment` decides that. A struct keeps its live storage size when the alignment cannot restore it (a raw `0x2C` body already 4-aligned has `0x30` storage), and its tail members are logged as layout candidates. Structs whose live `MinAlignment` is 1 are packed **without changing their size**, so compiler padding cannot round odd property-bag / BP layouts. Every other struct stays unpacked: an `alignas` on a struct otherwise overrides `#pragma pack(1)` in every class that embeds it by value. On 5.8 the instance-data structs are 16-aligned and pack with `alignas` (body `0x168`, `sizeof` `0x170`), so their derived members are recovered without changing the embedded stride |
| Classes emitted under `#pragma pack(1)`, to the packed body end | `UStruct::Link` starts a derived class's properties at the super's **raw** `PropertiesSize` (`CoreUObject/Private/UObject/Class.cpp`, identical from 4.23 to 5.8) while `GetStructureSize()` rounds it. A native base can even report the rounded size while its derived class starts earlier: `UPointLightComponent` reports `0x360`, but `USpotLightComponent::InnerConeAngle` lives at `0x358`, inside the base's alignment tail. The emitter therefore derives each class's **packed body end** from the earliest first own member of a direct subclass (capped by the class's own size), pads the packed body to that end, and adds `alignas(MinAlignment)` when the end is below the live size **and** the alignment restores that size; otherwise the packed body end is the emitted `sizeof` (classes are never embedded). A packed derived class starts at the base's packed body end even when the base is aligned (`#pragma pack(1)` + `alignas`: verified with clang-cl and MSVC), so inherited members keep their live offsets. Own members are filtered against that packed base end, not the live `PropertiesSize`. The asserted class size is the emitted C++ `sizeof`: the packed body rounded by the strongest alignment in the base chain or in an `alignas` struct member it embeds by value, which is the raw size when no alignment is in effect and the aligned size for a Blueprint subclass of an aligned native base. A class with no alignment in its chain keeps its raw `PropertiesSize`, so its size assertion uses the raw size while the offsets stay identical |
| Non-reflected engine members | `UEnum::Names`, `UDataTable::RowMap` and `ULevel::Actors` are plain C++ members with no `UPROPERTY`, so the reflection chain cannot see them. They are added from the engine's own declarations, anchored to live or reflected data rather than a game offset: `Names` follows the 16-byte `FString CppType` on a `0x30` `UField`, a `TArray<TPair<FName,int64>>` (0x10) in the classic `0x60` layout, and `FNameData` (two tagged pointers + count, 0x18) in the 5.8 `0x70` layout, emitted as `SDK_EnumNameData` in `Basic.h`; `RowMap` follows the reflected `RowStruct` pointer; `Actors` is anchored backwards from the reflected `OwningWorld`, the source sequence between them is `ActorsForGC` (0x20 total) through 5.7, plus `NumActorsMarkedForPendingUnloadByGC` and its padding (0x28) in 5.8+, so 4.x gets 0x98 and 5.8 gets 0xA0 without reading `UClass::Interfaces` (which has a different offset on 5.8). Injected members get a header member and an assertion but **no reflection record**; a candidate that does not fit the live class layout is skipped with a log line instead of being emitted wrong |
| Enum width from the engine's own storage | `FEnumProperty::ElementSize` is the width the engine stores an enum with, and it is authoritative: `ENiagaraRibbonUVDistributionMode` has four values but occupies four bytes, so a `uint8_t` declaration (what the value range suggests) moved every member holding one. Enum emission is therefore deferred until every property has been read, and the width is `max(value range, widest observed ElementSize)`. UHT's `<Enum>_MAX` entry is metadata, not a C++ enumerator, `SoundWaveLoadingBehavior.h` ends `ESoundWaveLoadingBehavior` at 255 in a `uint8` enum while the UEnum reports `_MAX = 256`, so it is kept out of the width, and left out of the declaration (as `constexpr int32_t <Enum>_MAX`) when it cannot be written in that type |
| Narrowed `EnumProperty` | An `EnumProperty` whose storage is narrower than the enum's declared type goes through the same fixed-size wrapper `ByteProperty` already used (`T1ByteEnum<…>`), so the member keeps its one byte |
| Function locals in the parameter structs | A Blueprint `UFunction` carries its local temporaries (`CallFunc_*_ReturnValue`, …) as properties of the function, and they occupy `ParmsSize`. They are emitted as named members of the `<Class>_<Function>_Params` struct, exact bytes instead of one anonymous pad, while staying out of the callable signature and the argument marshalling, which use only the `CPF_Parm` properties. This keeps the parameter block byte-exact |
| Parameter structs asserted and packed | Every `<Class>_<Function>_Params` declares `AVALONIUM_ASSERT_LAYOUT` and asserts its `ParmsSize` plus every parameter offset, these are the buffers handed to `ProcessEvent`, so their layout is as load-bearing as a class's. They are emitted under `#pragma pack(1)` for the same reason classes are: without it the compiler rounds `sizeof` past `ParmsSize` |
| Container headers | `UnrealContainers.hpp` (iterators over `TArray`/`TSet`/`TMap`) and `UtfN.hpp` (full Utf8/Utf16/Utf32 conversion) updated for the current engine layouts; `PropertyFixup.hpp` stays Avalonium's |

### Layout model

Everything above comes from two functions that are identical in every engine tree this
project covers (4.23 … 5.8):

* `UStruct::GetStructureSize()` → `Align(PropertiesSize, MinAlignment)`, the size a struct
  is stored with, so it is the struct's `sizeof`.
* `UStruct::Link()` → a derived type's properties start at the super's `PropertiesSize`,
  with each property aligned by its own alignment. For a **native class** the reported
  `PropertiesSize` can be above the body end the next derived class actually starts from:
  `UPointLightComponent` reports `0x360`, while `USpotLightComponent` places
  `InnerConeAngle` at `0x358`. The emitter therefore reads each class's packed body end
  from the earliest first own member of its direct subclasses (capped by the class's own
  size) instead of assuming it equals the reported size.

Assertion policy that follows from it:

* a **struct** asserts its size (the aligned `GetStructureSize()`) and every member offset; a
  struct base that needs the packed form carries the body-end/`alignas` comment the same way
  a class does;
* a **class** asserts its **emitted** `sizeof`, the packed body end rounded by the
  strongest alignment in its base chain or in an `alignas` struct member it embeds by value,
  and every member offset. With no alignment in the chain that is the raw `PropertiesSize`;
  a Blueprint subclass of an aligned native base asserts the aligned size the engine
  allocates;
* alignment itself is not asserted: the SDK reproduces sizes and offsets byte for byte, but
  `alignof` is the compiler's (a packed class is alignment-free unless a native base needs
  `alignas` to restore its reported size);
* **nothing is ever suppressed.** Every class and struct carries its size and all member
  offsets, so a layout that does not match the engine fails the build instead of silently
  producing a wrong offset in a mod. The run logs a warning
  (`[SDK] Layout: N type(s) do not match the engine byte-for-byte`) when the emitted offsets
  disagree with the live ones, but the assertions are still written, the warning says where
  to look, it does not hide the failure. The comparison skips **bitfields**: several of them
  legitimately share one offset (their containing word).

Struct bases use the same rule: `FRootMotionSource` reports `0xA0`, its body ends at `0x98`,
and its five derived structs place one recovered member at `0x98`. The three non-reflected
members are injected from engine anchors (`UEnum::Names` `0x40`, `UDataTable::RowMap` `0x30`,
`ULevel::Actors` `0x98` in the classic layout, `0xA0` on 5.8).

UE 5.8 has its own shape: `UEnum::Names` is `FNameData` (0x18) at `0x40` in a `0x70` class,
and `ULevel::Actors` is `0xA0` (`OwningWorld` `0xC8` minus `0x28`, because
`NumActorsMarkedForPendingUnloadByGC` follows `ActorsForGC`). The three instance-data struct
bases `FStaticMeshComponentInstanceData` (0x170→0x168), `FISMComponentDescriptor`
(0x2B0→0x2A8) and `FRigUnit_HierarchyAddControlElement` (0xA0→0x98) are 16-aligned: they pack
to the child-proven body end with `alignas` and assert that packed `sizeof`.

UE 5.8 has a different wrapper ABI. The generator reads the live sizes instead of using
version constants:

* `FText` from any collected `TextProperty::ElementSize` (0x10 in 5.8, 0x18 in 4.x); the size
  is recorded while reading properties, and `PropertyFixup.hpp` is written after collection;
* `TLazyObjectPtr` from the collected `LazyObjectProperty` (0x18 in 5.8, 0x1C in 4.x);
* `Utf8StrProperty` → `FUtf8String` (0x10), using the `UC::FUtf8String` the container header
  already defines;
* `OptionalProperty` → `SDK_OptionalBytes<N>`, where reflection's `ElementSize` is the
  wrapper's total size (UE5's `FOptionalProperty`);
* the predefined `UObject`, `UField`, `UStruct`, `UClass` and `UFunction` declarations place
  their known members at the live offsets from `EngineState` and pad to that class's packed
  body end, so no fixed head or tail pads remain and a build with a different member order
  changes only the generated padding;
* structs with `MinAlignment == 1` are packed, so odd-sized property-bag / Blueprint data
  structs keep their engine `sizeof` and offsets.

`UStruct::MinAlignment` is read as a 16-bit word at `PropertiesSize + 4` (`int32` through
5.5, `int16` from 5.6) and accepted when it is 1/2/4/8/16/32; `PropertiesSize` is the raw end
of the last property and is not required to be a multiple of the alignment. Class size
assertions fold the strongest `alignas` struct member into the emitted `sizeof`, so a class
embedding one rounds its size while its member offsets stay live (`UPCGManagedISMComponent`
0x360 vs live 0x358).

Known limitations:

| Item | Behavior |
| --- | --- |
| `UEnum` layouts outside 0x60/0x70 | The classic and 5.8 layouts are handled; a build whose `UEnum` size is neither leaves the member list empty rather than guessing |
| `UClass::Interfaces` on 5.8 | The usmap `VMTB` extension reads `UClass::Interfaces`; its offset differs on 5.8, and `ULevel::Actors` does not depend on it |
| Per-type layout assertions | `CHECK_SDK_LAYOUT` is one global switch; a single type can be checked by invoking its `AVALONIUM_ASSERTS_<Type>` macro directly |

Verification: the emitted SDK is compiled with `CHECK_SDK_LAYOUT` (clang-cl and MSVC, C++20).
Every class, struct and parameter struct asserts its size and member offsets, so a layout
that disagrees with the engine fails that type's build.

---

## 10. Generated SDK runtime

`Basic.h` resolves `AvaloniumGetSdkApi` from Avalonium.dll on first use and keeps the
returned table for the life of the process. The table is a separate, append-only contract
from the native mod API:

```c
#define AVALONIUM_SDK_API_VERSION 1u

typedef struct AvaloniumSdkApi
{
    uint32_t struct_size;
    uint32_t api_version;

    int32_t (*get_engine_major)(void);
    int32_t (*get_engine_minor)(void);

    int32_t (*get_object_count)(void);
    void* (*get_object_by_index)(int32_t index);
    int32_t (*get_object_index)(void* object);
    void* (*get_object_class)(void* object);
    void* (*get_object_outer)(void* object);
    void* (*get_super_struct)(void* object);
    const char* (*get_object_name)(void* object);
    const char* (*get_object_full_name)(void* object);
    const char* (*get_fname_string)(int32_t comparison_index, int32_t number);
    bool (*is_a)(void* object, const char* class_name);

    void* (*find_object)(const char* path);
    void* (*find_class)(const char* name);
    void* (*get_class_default_object)(void* uclass);
    void* (*find_function)(void* uclass, const char* function_name);

    bool (*process_event)(void* object, void* function, void* params);
} AvaloniumSdkApi;

AVALONIUM_SDK_API_ENTRY const AvaloniumSdkApi* AvaloniumGetSdkApi(uint32_t requested_version);
```

| Field | Result |
| --- | --- |
| `get_engine_major`, `get_engine_minor` | Engine version the runtime detected. |
| `get_object_count`, `get_object_by_index` | The live object array. An index outside the array returns null. |
| `get_object_index` | The object's `InternalIndex`, or `-1` for an unreadable or null object. |
| `get_object_class`, `get_object_outer`, `get_super_struct` | Class, outer and super pointers from runtime reflection. |
| `get_object_name`, `get_object_full_name` | Names rendered by the runtime; the pointer is thread-local and valid until the next name call on that thread. |
| `get_fname_string` | Text for an `FName` index and number, including the engine's `_N` suffix when `Number` is non-zero. |
| `is_a` | Name-based class-chain test. |
| `find_object`, `find_class`, `get_class_default_object` | Path, short-name and `_C` aware lookups plus the class default object. |
| `find_function` | A function on the class or its super chain, with the declaring class checked against the chain. |
| `process_event` | Calls the engine's ProcessEvent for the supplied parameter block. |

Lookups and name reads are read-only. `process_event` is a game-thread operation; the
runtime refuses it from other threads and logs one warning per call site. A generated
function called from the game thread satisfies this rule.

The table is versioned independently from `AvaloniumModApi`. A generated SDK built
against a newer table than the runtime answers reports the backend as unavailable rather
than reading past `struct_size`. The `AvaloniumSdkApi` definition is embedded in
`Basic.h` under the `AVALONIUM_SDK_API_TYPES_DEFINED` guard, so the generated SDK stays
self-contained and a mod that also includes the public `Avalonium/SdkApi.h` does not get a
duplicate definition.

The predefined `UObject`, `UField`, `UStruct`, `UClass` and `UFunction` declarations use
the same live data: member offsets come from `EngineState`, `PropertiesSize` and
`MinAlignment` come from the build, and the tail pad reaches the class's packed body end.
No fixed head or tail pads remain in those declarations, so a build whose member order or
alignment differs changes only the generated padding.

---

## Cross-references

* [REFERENCE.md](REFERENCE.md): structures, constants, JSON schemas, validation algorithm.
* [RuntimeAPI/IPC.md](../RuntimeAPI/IPC.md): `GENSDK`/`GENDIFF` commands.
* [RuntimeAPI/CONFIG.md](../RuntimeAPI/CONFIG.md): SDK config keys.
* [UsmapAPI](../UsmapAPI/README.md): the sibling `.usmap` generator.
* [LuaAPI/COMPAT.md](../LuaAPI/COMPAT.md): `GenerateSDK`/`GenerateUHTCompatibleHeaders`.
