# SdkAPI: Reference

Complete reference for `SDKGenerator` and `SDKValidation`, their data structures, constants
and output formats.

---

# Part 1: Generator

## 1.1 Public API

```cpp
// Runtime/sdk/SDKGenerator.h
namespace SDKGenerator {
    bool GenerateSDK(const EngineState& state, const std::string& outputDir);
    bool GenerateReflectionKit(const EngineState& state, const std::string& outputRootDir);
    bool GenerateReflectionDiff(const EngineState& state, const std::string& outputDir,
                                const std::string& previousSnapshotPath,
                                const std::string& currentSnapshotPath);
    void SdkLog(const char* fmt, ...);
}
```

| Function | Returns | Notes |
| --- | --- | --- |
| `GenerateSDK` | `false` on output-dir conversion failure or a failed `Basic.h` write (which aborts); other per-file write failures are counted, not fatal. Output-directory creation errors are not checked | Runs under `SdkFastMemScope` (fast memory mode) |
| `GenerateReflectionKit` | result of `GenerateSDK(state, root + "\\SDK")` | Creates `root` first |
| `GenerateReflectionDiff` | `false` if any path is empty or a snapshot fails to load | Loads both snapshots into id→entry maps, then `WriteReflectionDiffFromSnapshots(..., hasPrevious=true)` |
| `SdkLog` | N/A | `[SDK]`-tagged lines via the discovery logger |

`GenerateSDK` logs a final summary: package/enum/struct/class/function counts, metadata symbol
counts, reflection record counts, and `Total Files: N (failed: M)`.

## 1.2 Internal data structures (`SDKGenerator_Impl.h`)

### `SDKProperty`

| Group | Fields |
| --- | --- |
| Types | `Type`, `Name`, `RawType`, `InnerType`, `KeyType`, `ValueType`, `StructType`, `EnumType`, `ObjectClassType` |
| Layout | `Offset`, `Size`, `ElementSize`, `ArrayDim`, `Flags`, `FlagsText`, `SignatureHash`, `Fingerprint` |
| Kind flags | `IsArray`, `IsMap`, `IsSet`, `IsStruct`, `IsEnum`, `IsObject`, `IsBitfield`, `IsBool`, `IsString`, `IsNumeric`, `IsInt`, `IsFloat`, `IsDelegate`, `IsInterface` |
| Bitfields | `BitIndex` (0xFF = none), `BitCount`, `ByteMask`, `FieldMask`, `FieldSize` (`FBoolProperty::FieldSize`: 1/2/4/8 bytes, the width the bit is declared in) |
| Parameter role | `IsReturnParam`, `IsOutParam`, `IsReferenceParam`, `IsConstParam` |

### `SDKFunction`

`Name`, `RawName`, `FullPath`, `Params` (`vector<SDKProperty>`), `ReturnType`, `ReturnName`,
`FlagsText`, `FunctionFlags`, `ParameterSize`, `SignatureHash`, `Fingerprint`.

### `MetadataEntry` (→ `SDKMetadata.json` symbols)

`Kind`, `PackageName`, `Name`, `CppName`, `FullPath`, `SuperName`, `SignatureHash`, `Size`,
`SuperSize`, `OwnSize`, `PropertyCount`, `FunctionCount`, `ValueCount`, `FunctionFlags`.

### `PackageSummary`

`Enums`, `Structs`, `Classes`, `Functions` (per package).

### Reflection records

| Record | Key fields |
| --- | --- |
| `ReflectionEnumRecord` | `Id`, `PackageName`, `Name`, `CppName`, `FullPath`, `SignatureHash`, `Values` (name, int64) |
| `ReflectionTypeRecord` | `Id`, `Kind`, `PackageName`, `Name`, `CppName`, `FullPath`, `SuperName`, `SuperCppName`, `SignatureHash`, `Size`, `SuperSize`, `OwnSize`, `PropertyCount`, `FunctionCount` |
| `ReflectionFunctionRecord` | `Id`, `OwnerTypeId`, `OwnerKind`, `OwnerPackageName`, `OwnerName`, `OwnerCppName`, `Name`, `FullPath`, `ReturnType`, `ReturnName`, `FunctionFlags`, `ParameterSize`, `SignatureHash`, `Params` |
| `ReflectionPropertyRecord` | `Id`, `OwnerId`, `OwnerKind`, owner identity, `Name`, `CppType`, full type set, `FlagsText`, `Role`, `SignatureHash`, layout + kind flags, parameter-role flags |
| `ReflectionSnapshotEntry` | `Kind`, `Id`, `SignatureHash`, `OwnerCppName`, `Name`, `Extra` |

### Layout rules (`SDKGenerator_LayoutRules.h`)

Pure functions (no Windows, no live memory) that decide and format layout facts, so the
same rule is used by every writer and can be exercised on its own:

| Function | Rule |
| --- | --- |
| `BitfieldUnderlyingType(FieldSize)` | a bitfield's member and padding are declared in the word `FBoolProperty::FieldSize` names (`uint8_t`/`uint16_t`/`uint32_t`/`uint64_t`); a byte-declared member for a 4-byte word would move every member after it |
| `BitfieldBitIndex(byteOffset, byteMask)` | where the bit sits inside its word: `byteOffset * 8` plus its position in `ByteMask`. A one-byte word reports a zero byte offset, so byte-declared bitfields are unchanged |
| `EnumUnderlyingBytes/TypeName(min, max)` | the enum's underlying type from its value range (`UEnum::UnderlyingType` exists from UE 5.8+; the name array supplies the same answer otherwise) |
| `FixedSizeEnumWrapper(enum, enumBytes, memberBytes)` | a property narrower than its enum is declared through `T1ByteEnum`/`T2ByteEnum`/`T4ByteEnum`/`T8ByteEnum`; the template + aliases ship in `PropertyFixup.hpp` (`GetFixedSizeEnumTemplateContent`) |
| `SanitizeArrayDim(asInt32, asByte)` | a build that stores `ArrayDim` in one byte without clearing the word reads as `0xFFFFFF01`; such a read is repaired to the low byte |
| `IsPlausibleStructAlignment(v)` | 1/2/4/8/16/32 (measured distribution over 15206 live structs) |
| `BytePaddingLine` / `BitPaddingLine` / `BitfieldMemberLine` / `MemberComment` | the exact emitted member text, including the decoded flag list |

### Emitted member text

```
    struct FActorTickFunction PrimaryActorTick; // 0x28(0x30) flags=0x10000000010001 (Edit|Public) hash=0x8B284893AE264B5
    uint8_t bHidden : 1;     // 0x58(0x0001)(BitIndex: 0x5, PropSize: 0x0001 flags=0x48001200000035 (Net|Public))
    uint8_t BitPad_5B_4 : 4; // 0x5b(0x0001)(Fixing Bit-Field Size For New Byte)
    uint8_t Pad_114[0x4];    // 0x114(0x4)(Fixing Size After Last Property)
```

`flags=` keeps the raw `EPropertyFlags` value it always carried and now also names the bits
that are set (`DescribePropertyFlags`). Every
`EPropertyFlags` bit is decoded; the same text is what `ReflectionDB.json`'s `flags_text`
holds, so the two cannot drift.

`UStruct::PropertiesSize` (a struct's size, a superclass's size, a `UFunction`'s `ParmsSize`)
is read through `ReadStructPropertySize`, which uses the discovered
`UStruct_PropertiesSizeOff` and falls back to the declared member order, the hand-rolled
`ChildProperties + 8` expression is gone from the generator and from live SDK validation.

### Working types

| Type | Purpose |
| --- | --- |
| `PackageStreams` | Four `ostringstream`s (`structs`, `classes`, `functions`, `parameters`) + `has*` flags |
| `PendingTypeDef` | `cppName`, `text`, dependency set (topological ordering) |
| `EnumInfo` | `name`, `packageName`, `values` |
| `GenerationCaches` | Object/field/package name caches, type-name and flags-text caches; `Clear()` |
| `TypeDependencyContext` | Current state, collision maps, self package, dependency set; `Reset`/`Finish` |

## 1.3 Flag constants

### Property flags (`CPF_*`, `uint64`)

```
Edit 0x1          ConstParm 0x2        BlueprintVisible 0x4   ExportObject 0x8
BlueprintReadOnly 0x10  Net 0x20       EditFixedSize 0x40     Parm 0x80
OutParm 0x100     ZeroConstructor 0x200  ReturnParm 0x400     DisableEditOnTemplate 0x800
NonNullable 0x1000  Transient 0x2000   Config 0x4000          RequiredParm 0x8000
DisableEditOnInstance 0x10000  EditConst 0x20000  GlobalConfig 0x40000  InstancedReference 0x80000
DuplicateTransient 0x200000  SaveGame 0x1000000  NoClear 0x2000000  ReferenceParm 0x8000000
BlueprintAssignable 0x10000000  Deprecated 0x20000000  IsPlainOldData 0x40000000  RepSkip 0x80000000
RepNotify 0x100000000  Interp 0x200000000  NonTransactional 0x400000000  EditorOnly 0x800000000
NoDestructor 0x1000000000  AutoWeak 0x4000000000  ContainsInstancedReference 0x8000000000
AssetRegistrySearchable 0x10000000000  SimpleDisplay 0x20000000000  AdvancedDisplay 0x40000000000
Protected 0x80000000000  BlueprintCallable 0x100000000000  BlueprintAuthorityOnly 0x200000000000
TextExportTransient 0x400000000000  NonPIEDuplicateTransient 0x800000000000
ExposeOnSpawn 0x1000000000000  PersistentInstance 0x2000000000000  UObjectWrapper 0x4000000000000
HasGetValueTypeHash 0x8000000000000  NativeAccessSpecifierPublic/Protected/Private
SkipSerialization 0x80000000000000
```

### Function flags (`FUNC_*`, `uint32`)

```
Final 0x1  RequiredAPI 0x2  BlueprintAuthorityOnly 0x4  BlueprintCosmetic 0x8
Net 0x40   NetReliable 0x80  Native 0x400  Event 0x800  Static 0x2000
NetMulticast 0x4000  MulticastDelegate 0x10000  Public 0x20000  Private 0x40000
Protected 0x80000  Delegate 0x100000  NetServer 0x200000  HasOutParms 0x400000
HasDefaults 0x800000  NetClient 0x1000000  BlueprintCallable 0x4000000
BlueprintEvent 0x8000000  BlueprintPure 0x10000000  Const 0x40000000
```

(These match the bit meanings decoded by the IPC `FUNCS` command, see
[RuntimeAPI/IPC.md](../RuntimeAPI/IPC.md).)

## 1.4 Naming & collisions

* `CleanClassName` sanitizes engine names for C++ (`A`/`U`/`F` prefixes applied per kind).
* `IsCppKeyword` / `IsReservedWindowsName` guard names; a reserved package gets `_pkg`.
* **Package collisions**: `ResolvePackageName` strips `/Script/`/`/Game/`, cleans the name and
  appends `_<n>` for the n-th distinct raw package mapping to the same clean name
  (`collisionCounter`/`rawToUnique`), e.g. `Engine`, `Engine_1`.
* `MakeUniqueName(wanted, used)` ensures unique identifiers within a scope.
* `GetUniqueTypeCppName` does the same for type names.

## 1.5 Hashing

FNV-1a (`HashBytesFNV1a` with basis `0x811C9DC5`-style continuation and prime `1099511628211`):

* `HashStringFNV1a` feeds the UTF-8 bytes plus an `0xFF` separator.
* `HashUInt64FNV1a` feeds an 8-byte little-endian value.
* `FormatHashHex` renders `0x…` uppercase.
* `BuildPropertyFingerprint` / `BuildFunctionFingerprint` combine field-level hashes into the
  `SignatureHash` seen in JSON and snapshots.

## 1.6 Reflection outputs

`ResolveReflectionOutputDir(sdkOutputDir)`:

```
<parent of sdkOutputDir>\Reflection        // trailing separators trimmed; "Reflection" if empty
```

| File | Writer | Contents |
| --- | --- | --- |
| `ReflectionDB.json` | `WriteReflectionDatabase` | `generator`/`format_version`/`game`/`engine`/`object_count`/`package_collision_count`/`output_dir`/`indexes`, `packages[]` (summaries + dependencies), `enums[]`, `types[]`, `functions[]`, `properties[]` (sorted by owner C++ name, role, offset, name) |
| `Runtime/ReflectionHelpers.h`, `Runtime/ReflectionHelpers.cpp` | `WriteReflectionRuntimeHelpers` | C++ helpers to navigate the DB |
| `Inheritance.json` | `WriteReflectionGraphs` | `{types:[{id,kind,package,cpp_name,super_cpp_name}]}` |
| `PackageGraph.json` | `WriteReflectionGraphs` | `{packages:[{name,dependency_count,dependencies[]}]}` |
| `Diff/ReflectionDiff.json` | `WriteReflectionDiff` / `FromSnapshots` | Changed/added/removed records with risk + reason, `has_previous_snapshot` |
| `Diff/ReflectionSnapshot.tsv` | `WriteReflectionDiff` | Header `# kind\tid\thash\towner\tname\textra` then one line per entry |

Snapshot entries are serialized tab-separated; `ParseSnapshotExtra` reads `key=value` pairs
from the `Extra` column; `GetSnapshotField` reads one key.

Diff classification helpers: `DetermineDiffRisk(change, before, after)` and
`DetermineDiffReason(change, before, after)`.

### `GenerateReflectionDiff(state, outputDir, previousSnapshotPath, currentSnapshotPath)`

Standalone diff between two arbitrary `ReflectionSnapshot.tsv` files (no live walk):

1. `LoadSnapshotMap` both files (fails with a log line when a file cannot be read).
2. Create `outputDir`.
3. `WriteReflectionDiffFromSnapshots(outputDir, state, previousById, currentById, true)`.

## 1.7 `SDKMetadata.json` schema (exact)

| Field | Type | Notes |
| --- | --- | --- |
| `generator` | string | `"Avalonium SDK"` |
| `format_version` | int | `1` |
| `game` | string | `state.GameName` (JSON-escaped) |
| `engine` | string | `"UE<Major>.<Minor>"` |
| `object_count` | int | live object count |
| `package_collision_count` | int | package name collisions resolved |
| `total_files` | int | written file count (includes the manifest) |
| `failed_files` | int | write failures |
| `packages[]` | array | `name`, `enums`, `structs`, `classes`, `functions`, `dependencies[]` |
| `symbols[]` | array | `kind`, `package`, `name`, `cpp_name`, `full_path`, `super`, `hash`, `size`, `super_size`, `own_size`, `property_count`, `function_count`, `value_count`, `function_flags` |

Symbols are sorted by kind, package, full path, then C++ name.

## 1.8 `GenerateSDK` step detail

1. `SdkFastMemScope` + `ResetSdkLogTimeBaseline` + start logs.
2. Convert/validate the output directory (UTF-16). An existing output directory is
   **removed recursively and recreated** (`fs::remove_all`) before generation.
3. `USMapDiscovery::DiscoverPropertySubTypeOffsets` probes and repairs property subtype
   offsets before the object walk.
4. `DiscoverUEnumNamesOffset` (when `UEnum_NamesOff < 0`): probes `Enum`/`UserDefinedEnum`
   objects, scanning offsets `0x40..0xD0` step 8 for either the new UE5 tagged-array layout
   (`bIsNewUE5Enum = true`) or the legacy `TArray<TPair<FName, int64>>` layout; validates the
   first few entries resolve through `FNameToStr`. Returns either layout + offset.
4. Single object-array pass to collect enums/structs/classes/functions.
5. Emit `Basic.h` (abort on write failure: `[SDK] ERROR: Failed to write Basic.h, aborting.`).
6. Emit per-package files with include dependency lists.
7. `Assertions.inl` and `SDK.h`. Each entry asserts the type's emitted size and the offset of
   every named member (bitfields excluded); alignment is not asserted. `SDK.h` includes the
   file only under `#ifdef CHECK_SDK_LAYOUT`, so a normal mod build does not parse it.
8. Parallel asynchronous file writes (`std::async`), one future per file; failures counted.
9. `WriteMetadataManifest` → `SDKMetadata.json`.
10. Reflection DB/helpers/graphs/diff.
11. Optional: build system (`python <script> <dir> --game-name <Game>`, `CREATE_NO_WINDOW`),
    libclang generation, live validation.

## 1.9 Post-dump hooks

```cpp
if (runtimeConfig.GenerateBuildSystem && !runtimeConfig.BuildScriptPath.empty()) { … }
if (runtimeConfig.EnableSDKLibclangGeneration) SDKValidation::StartSDKGenerationThread(outputDir, state.GameName, runtimeConfig.LibclangPath);
if (runtimeConfig.EnableSDKValidation)         SDKValidation::StartSDKValidationThread(state, outputDir);
```

Note: the code uses `EnableSDKLibclangGeneration`/`EnableSDKValidation`; the INI spellings in
`RuntimeConfig` are `SDKLibclangGeneration`/`SDKValidation` (with aliases): see
[RuntimeAPI/CONFIG.md](../RuntimeAPI/CONFIG.md).

---

# Part 2: libclang AST generation

`SDKValidation::StartSDKGenerationThread(sdkPath, gameName, libclangPath)` spawns
`SDKGenerationThreadProc`.

## 2.1 Setup

1. Validate the SDK directory (else `Verdict: SETUP FAILED (no SDK files were changed).`).
2. Load `libclang.dll` (`NativeLibclang::Load`): when `libclangPath` is empty it
   auto-discovers:
   * Visual Studio **18** and **2022**, under both `%ProgramFiles%` and
     `%ProgramFiles(x86)%`, in `{Community,Professional,Enterprise,BuildTools}\VC\Tools\Llvm\x64\bin\libclang.dll`
   * `%ProgramFiles%\LLVM\bin\libclang.dll`
   * the `LIBCLANG_PATH` and `CLANG_LIBRARY_PATH` environment variables
   Requires every C API entry point (else `libclang.dll is missing a required C API entry point.`).
3. Discover include directories (`DiscoverIncludeDirectories`):
   * every directory in the `INCLUDE` environment variable;
   * the newest `VC\Tools\MSVC\<version>\include`;
   * the newest Windows 10 SDK under `%ProgramFiles(x86)%\Windows Kits\10\Include` that has
     `ucrt`, `shared` and `um`, adding `ucrt`, `shared`, `um`, `winrt`, `cppwinrt`.
   Duplicates removed case-insensitively.
4. Parse arguments: `-x c++ -std=c++20 -fms-extensions -fms-compatibility -fsyntax-only -I <sdkRoot> -I <each include>`.
5. Require `SDK.h`.
6. Collect all `.cpp`/`.cxx`/`.cc` recursively (skipping temp `.native-libclang-*` files) and
   write a temporary **unity source** `AvaloniumSdkAst_*` that includes `SDK.h` + every source.
7. Create a clang index, parse the unity TU, count AST cursors, collect diagnostics
   (errors = severity 3/4), delete the temporary source.

## 2.2 Outcomes

| Condition | Log file `SDKAstGeneration.log` verdict |
| --- | --- |
| SDK dir/libclang/SDK.h/unity failure | `SETUP FAILED (no SDK files were changed).` |
| Any libclang errors | `FAILED (raw SDK contains libclang errors; no files were changed).` |
| Clean parse | `PASSED (native libclang parsed and verified the generated SDK).` |

On success it also writes `SDKAstModel.json`:

```json
{ "format": 2, "backend": "native-libclang", "translation_units": [ … ] }
```

`SDKAstGeneration.log` lines include: SDK path, unity source count, toolchain include count,
the translation unit's relative path `<native-libclang-unity>`, parsed file/cursor counts,
errors, and the verdict.

---

# Part 3: Live-memory validation

`SDKValidation::StartSDKValidationThread(state, sdkPath)`.

## 3.1 Request deduplication

Normalized paths are tracked in `g_ValidationPathsInFlight` / `g_ValidationPathsCompleted`
(mutex `g_ValidationStateMutex`), so the same path is validated at most once per session and
duplicate requests are skipped with a log line.

## 3.2 Algorithm (`ValidationThreadProc`)

1. `LoadSDKMetadata(sdkPath)` → symbol map keyed by `full_path` (`SDKMetadata.json`).
   Empty → write `SDKValidation.log` with "Failed to load SDKMetadata.json or no symbols found.
   Validation aborted."
2. Iterate live objects in `GUObjectArray`:
   * Classify via the meta-class chain (walk up to 10 super levels looking for
     `Class`/`ScriptStruct`/`Enum`).
   * Skip non-structural objects, `Default__*`, `SKEL_*`, `REINST_*`, empty/failed names.
   * Missing symbol → `AddNew` (new in live memory); this increments `NewCount` and produces
     only the aggregate `NEW` count in the report.
3. For matched class/struct symbols:
   * `Size` vs live `UStruct::PropertiesSize` (`UStruct_PropertiesSizeOff`, with the super's
     size read through `UStructPropertiesSizeOffset`), mismatch warning.
   * `PropertyCount` vs live count (FProperty chain via `FProperty_OffsetOff`, legacy UProperty
     chain via `UProperty_OffsetOff`), counting only properties at/after the super size.
4. Enum symbols are matched but carry no extra checks.
5. Symbols in the metadata not seen live (functions excluded) → `FAIL` (`NOT FOUND in live
   memory (removed or renamed)`).
6. Write `SDKValidation.log`:

```
=== Avalonium SDK Validation ===
SDK Path : <path>
Symbols  : <n>

<pass/warn/fail lines>

Summary: <P> PASS | <W> WARN | <F> FAIL
[INFO] <N> structural types in live memory are not in SDK -> re-dump recommended   (if N > 0)
Verdict: SDK matches live memory perfectly.
   -- or --
Verdict: SDK partially stale. Re-dump recommended.
```

The "partially stale" verdict is chosen when `WarnCount > 0 || FailCount > 0 ||
NewCount > 50`.

---

# Part 4: Log tags

| Tag | Source |
| --- | --- |
| `[SDK] ====== Starting Full C++ SDK Generation ======` / `…generated successfully!` | generator |
| `[SDK] Output Folder: …` | generator |
| `[SDK] ERROR: Failed to write Basic.h, aborting.` | generator |
| `[SDK] ERROR: Failed to write file: <name>` / `…SDKMetadata.json!` | generator |
| `[SDK] ReflectionDB written to: …` / `Reflection runtime helpers written to: …` / `Reflection graph files written to: …` / `Reflection diff files written to: …` | reflection |
| `[SDK] WARNING: Failed to write …` | reflection (non-fatal) |
| `[SDK] Build system generation launched for game: …` | build system |
| `[SDK] Native libclang AST generation started/finished/failed …` | libclang |
| `[SDKValidation] Looking for SDKMetadata.json at: …` | validation |
| `[SDKValidation] Loaded N symbols from SDK metadata. Validating against live memory...` | validation |
| `[SDKValidation] Validation complete. Wrote <path>\SDKValidation.log` | validation |
| `[SDKValidation] Validation already running/completed for … - skipping duplicate request.` | dedup |

---

## Cross-references

* [README.md](README.md): pipeline overview and config.
* [RuntimeAPI/IPC.md](../RuntimeAPI/IPC.md): `GENSDK` / `GENDIFF` wire formats.
* [RuntimeAPI/CONFIG.md](../RuntimeAPI/CONFIG.md): `SDK*` keys.
* [UsmapAPI/FORMAT.md](../UsmapAPI/FORMAT.md): `.usmap` alternative.
* [LuaAPI/COMPAT.md](../LuaAPI/COMPAT.md): Lua generation globals.
