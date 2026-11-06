# UsmapAPI: `.usmap` Generator

The UsmapAPI subsystem writes the engine's reflection data into the **`.usmap` mapping file**
format used by the UE modding ecosystem (UnrealMappingsDump, FModel, UAssetAPI, …). A `.usmap`
contains the name table, enums, structs/classes, property schemas and, in Avalonium's output,
an extended CEXT block with property offsets, package paths, default objects, interfaces,
function signatures and bytecode.

Source: [`Runtime/usmap/USMapGenerator.{h,cpp}`](../../Runtime/usmap/USMapGenerator.h).

---

## 1. Entry points

```cpp
bool GenerateUSMap(const EngineState& state, const std::string& outputPath);
void USMapDiscovery::DiscoverPropertySubTypeOffsets(EngineState& state);
```

| Trigger | Form |
| --- | --- |
| IPC | `USMAP [path]`, see [RuntimeAPI/IPC.md](../RuntimeAPI/IPC.md) |
| Lua (compat) | `DumpUSMAP()` / `DumpJMAP()`, see [LuaAPI/COMPAT.md](../LuaAPI/COMPAT.md) |

Output path defaults to the usual dump folder for the detected game (derived from the log
directory + sanitized game name); any explicit path is honored.

Logging is tagged `[USMap]` and starts with:

```
[USMap] ====== Starting .usmap Generation ======
[USMap] Output: <path>
```

Failure modes: engine not initialized (`Error: Engine not initialized`), path conversion
failure, `CreateFileW` failure, short write (`[USMap] ERROR: Ugh, damn it, Avalonium failed
to write the file for you: …`).

---

## 2. Offset auto-discovery

Before serializing, `GenerateUSMap` runs three discovery passes and logs every result:

| Pass | What it finds |
| --- | --- |
| `DiscoverUEnumNamesOffset` | `UEnum::Names` layout and offset (`0x40..0xD0` step 8); supports both the UE5 split layout (tagged names + values pointers) and the classic `TArray<TPair<FName,int64>>` |
| `DiscoverPropertySubTypeOffsets` | `FProperty` base offsets and every property subtype's key pointer (`FArrayProperty::Inner`, `FStructProperty::Struct`, `FObjectProperty::PropertyClass`, `FEnumProperty::UnderlyingProp`/`Enum`, `FMapProperty` key/value, `FSetProperty::ElementProp`, `FOptionalProperty::ValueProperty`, `FByteProperty::Enum`, `FClassProperty::MetaClass`, bool masks…) |
| `DiscoverPropertiesSizeOffset` | delegates to `EngineDiscovery::ResolveUStructPropertiesSizeOffset` (the `FColor`/`FGuid` confirmation and member-order fallback) |

`UStruct::PropertiesSize` is primarily resolved during startup discovery, at the end of
`ApplyFFieldLayoutFixups`, and `DiscoverPropertySubTypeOffsets` probes it again at usmap
time (confirming against `FColor` = 4 and `FGuid` = 16 where found) while re-verifying and
repairing `FProperty::Offset_Internal`. If the size still cannot be resolved, generation
continues with size `0`; `DiscoverPropertiesSizeOffset` remains as the entry point that
calls the resolver.

The discovered offsets are printed in an “I discovered some offsets for you” block, so a
generation log doubles as an offset report.

---

## 3. Generation phases

```
Phase 1  Serialize enums        (name idx, value count, int64 values + name idx each)
Phase 2  Collect structs        (Class, ScriptStruct, AnimBlueprintGeneratedStruct)
Phase 3  Serialize structs      (properties, flags, offsets, functions, bytecode, CDO,
                                interfaces)
Phase 4  Assemble payload       (names, enums, structs, CEXT extensions)
Phase 5  Write file             (magic/version/header + payload)
```

Serialization runs with `SafeMem_SetFastMode(1)` for the whole walk.

### Per-struct extras

For every struct/class the generator also collects:

* **PPTH**: package path index lists (enums, structs).
* **EATR**: enum flags and per-struct property-flag arrays.
* **MOFF**: property size, min alignment and per-property offset/element size/bitfield masks.
* **FUNC**: function name, flags, parameter count/size, return offset, RPC ids and full
  parameter property types.
* **VMBC**: per-struct script bytecode and per-function native RVA + bytecode.
* **DFLT**: class default object name + flags.
* **VMTB**: implemented interfaces (name + pointer offset).

These live in a CEXT extension block appended after the classic data, so standard `.usmap`
consumers can ignore it while Avalonium-aware tools read the richer data.

Full byte-level layout: [FORMAT.md](FORMAT.md).

---

## 4. EPropertyType mapping

`EPropertyType` mirrors the standard `.usmap` enum (0…36). `PropertyNameToUsmapType` maps live
`FProperty` class names to it, with the same normalizations other dumpers use:

| Live name | Mapped to |
| --- | --- |
| `ClassProperty`, `ClassPtrProperty` | `ObjectProperty` |
| `ObjectPtrProperty` | `ObjectProperty` |
| `SoftClassProperty` | `SoftObjectProperty` |
| `MulticastSparseDelegateProperty`, `MulticastInlineDelegateProperty` | `MulticastDelegateProperty` |
| `AssetClassProperty` | `AssetObjectProperty` |
| `F*`/`U*` prefixed names | the suffix looked up (`FStructProperty` → `StructProperty`) |
| `_<digits>` suffix | stripped before lookup |
| Unknown/Verse types | `Unknown` (0xFF) + a warning log line |

`ByteProperty` properties that actually reference a `UEnum` are emitted as
`EnumProperty(underlying ByteProperty, enum name)`: the "fake enum" upgrade.

---

## 5. Output

A single file; the caller decides the name. The IPC default is
`<logdir>\<GameName>\USMAP\<sanitized Game>.usmap` (no UE version in the name), and the Lua
`DumpUSMAP()` default is `Avalonium.usmap`:

```
USMAP_MAGIC = 0x30C4 (uint16 LE)
version     = 4 (ExplicitEnumValues, uint8)
package-version int32 = 0
compression = 0x00 (none)
payload uncompressed size (uint32) ×2
payload bytes
```

Success log:

```
[USMap] ====== YAY, Be happy; usmap written successfully! ======
[USMap] File: <path> (<bytes> bytes)
[USMap] Names: N, Enums: E, Structs: S
```

---

## 6. Source map

| File | Contents |
| --- | --- |
| `USMapGenerator.h` | `EPropertyType`, `PropertyNameToUsmapType`, `USMapWriter`, `USMapNameTable`, `USMapDiscovery` |
| `USMapGenerator.cpp` | offset discovery, enum/struct collection, property serialization, CEXT extensions, file assembly |

---

## Cross-references

* [FORMAT.md](FORMAT.md): the complete `.usmap` byte format including CEXT.
* [SdkAPI](../SdkAPI/README.md): the C++ SDK generator (`GENSDK`).
* [RuntimeAPI/IPC.md](../RuntimeAPI/IPC.md): `USMAP` command.
* [LuaAPI/COMPAT.md](../LuaAPI/COMPAT.md): `DumpUSMAP`/`DumpJMAP` globals.
