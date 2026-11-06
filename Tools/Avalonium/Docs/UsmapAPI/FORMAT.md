# UsmapAPI: `.usmap` Format Reference

Byte-level layout written by `GenerateUSMap`. All integers are **little-endian** unless stated
otherwise. Strings are `uint16 length` + raw bytes (no NUL terminator), the "LongFName" style
of the standard format.

---

## 1. File header

| Offset | Type | Value | Notes |
| --- | --- | --- | --- |
| 0x00 | `uint16` | `0x30C4` | `USMAP_MAGIC` |
| 0x02 | `uint8` | `4` | `EUsmapVersion::ExplicitEnumValues` |
| 0x03 | `int32` | `0` | package version (unused by this writer) |
| 0x07 | `uint8` | `0` | compression (0 = none) |
| 0x08 | `uint32` | `N` | uncompressed size of the payload |
| 0x0C | `uint32` | `N` | compressed size (equal, no compression) |
| 0x10 | bytes | payload | length `N` |

`EUsmapVersion`: `Initial 0`, `PackageVersioning 1`, `LongFName 2`, `LargeEnums 3`,
`ExplicitEnumValues 4`.

---

## 2. Payload layout (classic section)

Written in this order:

```
usmap.NameTable
usmap.Enums
usmap.Structs
CEXT extension block
```

### 2.1 Name table

```
uint32 count
count × string        (uint16 length + bytes)
```

Names are deduplicated through `USMapNameTable::AddName`, which returns the first index.

### 2.2 Enums

```
uint32 enumCount
enumCount × EnumEntry
```

`EnumEntry`:

| Field | Type | Notes |
| --- | --- | --- |
| `name` | `int32` | name-table index |
| `valueCount` | `uint16` | number of values |
| `valueCount` × value | `{ int64 value; int32 nameIdx; }` | explicit numeric value + name |

Enum flags are not part of this classic block; they are mirrored in `CEXT/EATR`.

### 2.3 Structs / classes

```
uint32 structCount
structCount × StructEntry
```

`StructEntry`:

| Field | Type | Notes |
| --- | --- | --- |
| `structNameIdx` | `int32` | name-table index of the struct/class name |
| `superNameIdx` | `int32` | name-table index of the super name |
| `totalPropCount` | `uint16` | sum of `arrayDim` over serializable properties |
| `serializedPropCount` | `uint16` | number of property records following |
| property records | ×`serializedPropCount` | see below |

Property record:

| Field | Type | Notes |
| --- | --- | --- |
| `schemaIdx` | `uint16` | sequential index within the struct (0-based) |
| `arrayDim` | `uint8` | static array dimension |
| `name` | `int32` | name-table index |
| `type` | `uint8` + payload | `EPropertyType` and its inline data (see §3) |

---

## 3. Property serialization (`SerializePropertyType`)

Common format: `uint8 EPropertyType` followed by type-specific data. All names below are
`int32` name-table indices unless stated.

| Type | Extra payload |
| --- | --- |
| `EnumProperty` | nested `EPropertyType` of the underlying property, then `int32 enumNameIdx` (empty name → index of `""`) |
| `StructProperty` | `int32 structNameIdx` |
| `ObjectProperty`, `WeakObjectProperty`, `LazyObjectProperty`, `AssetObjectProperty`, `SoftObjectProperty`, `InterfaceProperty` | none |
| `ArrayProperty` | nested property type of `Inner` (falls back to `IntProperty` + warning when null) |
| `SetProperty` | nested property type of `ElementProp` |
| `MapProperty` | nested key type, then nested value type |
| `OptionalProperty` | nested value property type |
| all scalar types | none |

"Fake enum" upgrade: a `ByteProperty` whose `FByteProperty::Enum` points at a `UEnum`/
`UserDefinedEnum` object is emitted as:

```
uint8 EnumProperty
uint8 ByteProperty          (the underlying type)
int32 enumNameIdx
```

Unmappable live type names emit `uint8 0xFF` (`Unknown`) and log
`[USMap] WARNING: Unknown property type '<name>' (prop=<ptr>)`.

### `EPropertyType` values

| Value | Name | | Value | Name |
| --- | --- | --- | --- | --- |
| 0 | `ByteProperty` | | 19 | `UInt32Property` |
| 1 | `BoolProperty` | | 20 | `UInt16Property` |
| 2 | `IntProperty` | | 21 | `Int64Property` |
| 3 | `FloatProperty` | | 22 | `Int16Property` |
| 4 | `ObjectProperty` | | 23 | `Int8Property` |
| 5 | `NameProperty` | | 24 | `MapProperty` |
| 6 | `DelegateProperty` | | 25 | `SetProperty` |
| 7 | `DoubleProperty` | | 26 | `EnumProperty` |
| 8 | `ArrayProperty` | | 27 | `FieldPathProperty` |
| 9 | `StructProperty` | | 28 | `OptionalProperty` |
| 10 | `StrProperty` | | 29 | `Utf8StrProperty` |
| 11 | `TextProperty` | | 30 | `AnsiStrProperty` |
| 12 | `InterfaceProperty` | | 31 | `ClassProperty` |
| 13 | `MulticastDelegateProperty` | | 32 | `MulticastInlineDelegateProperty` |
| 14 | `WeakObjectProperty` | | 33 | `SoftClassProperty` |
| 15 | `LazyObjectProperty` | | 34 | `VerseStringProperty` |
| 16 | `AssetObjectProperty` | | 35 | `VerseDynamicProperty` |
| 17 | `SoftObjectProperty` | | 36 | `VerseFunctionProperty` |
| 18 | `UInt64Property` | | 0xFF | `Unknown` |

---

## 4. CEXT extension block

Appended directly after the classic structs section. Marker names are written as `uint32`
little-endian ASCII (they read as the tag text). Every section except `CEXT` has the shape:

```
uint32 MARKER
uint32 byteSize            (size of everything after this field)
uint8  0                   (reserved/search-flag, always 0)
... section body ...
```

`CEXT` itself has **no** `byteSize` field: marker, one reserved byte, then `uint32 revision = 7`.

| Marker | ASCII | Body |
| --- | --- | --- |
| `0x54584543` | `CEXT` | `uint8 0` + `uint32 revision = 7`, announces the extension block |
| `0x48545050` | `PPTH` | `uint32 enumCount` + `enumCount × int32 nameIdx`; then `uint32 structCount` + `structCount × int32 nameIdx` (package paths) |
| `0x52544145` | `EATR` | `uint32 enumCount` + `uint32 flags` each; `uint32 structCount`; per struct: `uint8 kind`, `int32 flags`, `uint32 propCount`, `propCount × uint64 property flags` |
| `0x46464F4D` | `MOFF` | `uint32 structCount`; per struct: `int32 propSize`, `int32 minAlignment`, `uint32 propCount`, per property: `int32 offset`, `int32 elemSize`, `uint8 byteOffset`, `uint8 byteMask`, `uint8 fieldMask` |
| `0x434E5546` | `FUNC` | `uint32 structCount`; per struct: `uint32 fnCount`; per function: `int32 nameIdx`, `uint32 flags`, `uint8 numParms`, `uint16 parmsSize`, `uint16 returnValueOffset`, `uint16 rpcId`, `uint16 rpcResponseId`, `uint32 paramCount`, per param: `int32 nameIdx`, `uint64 flags`, property type serialization |
| `0x43424D56` | `VMBC` | `uint32 structCount`; per struct: `uint32 structBytecodeSize` + bytes, `uint32 funcCount`, per function: `uint64 nativeRVA`, `uint32 bytecodeSize` + bytes |
| `0x544C4644` | `DFLT` | `uint32 structCount`; per **serialized struct** (classes and non-classes): `int32 cdoNameIdx`, `uint32 cdoFlags` (missing CDO → `-1`/`0`; flags default to `0x10` only when a CDO exists with flags `0`) |
| `0x42544D56` | `VMTB` | `uint32 structCount`; per **serialized struct**: `uint32 interfaceCount`; per interface: `int32 ifaceNameIdx`, `int32 ptrOffset` |

Notes:

* `EATR`/`MOFF`/`FUNC`/`VMBC`/`DFLT`/`VMTB` counts are per-struct/class in the same order as
  the classic structs section, so consumers pair them positionally.
* `MOFF.minAlignment` is read from `UStruct::PropertiesSize + 4`.
* Bool bitfield masks come from `FBoolProperty` base offsets +1/+2/+3 (`byteOffset`, `byteMask`,
  `fieldMask`); zero for non-bools.
* `VMBC` struct bytecode is the raw `UStruct::Script` array (element count sanity 1..500000);
  function bytecode carries the function's native RVA.
* The extension log line:
  `[USMap] CEXT extensions: PPTH=%u, EATR=%u, MOFF=%u, FUNC=%u, VMBC=%u, DFLT=%u, VMTB=%u bytes`.

Standard `.usmap` readers stop after the classic sections and ignore everything past them; the
sections are only reachable by scanning for their markers, which is why the `CEXT` revision word
is present.

---

## 5. Writer API (C++)

```cpp
class USMapWriter {
    std::vector<uint8_t> buffer;
    void WriteByte(uint8_t);
    void WriteInt16(int16_t);  void WriteUInt16(uint16_t);
    void WriteInt32(int32_t);  void WriteUInt32(uint32_t);
    void WriteInt64(int64_t);  void WriteUInt64(uint64_t);
    void WriteString(const std::string&);   // uint16 length + bytes
    void WriteBytes(const void*, size_t);
    bool SaveToFile(const std::string& path, void (*logFunc)(const char*, ...));
};

class USMapNameTable {
    int32_t AddName(const std::string&);    // returns existing or new index
    uint32_t GetCount() const;
    void Serialize(USMapWriter&) const;     // uint32 count + strings
    const std::vector<std::string>& GetNames() const;
};
```

`SaveToFile` converts the path to UTF-16 (UTF-8 first, ANSI fallback), creates parent
directories, opens with `CreateFileW(CREATE_ALWAYS, FILE_SHARE_READ)` and writes the whole
buffer; failures are logged with the Win32 error code.

---

## 6. `.jmap` note

`DumpJMAP()` is exposed as a Lua compat alias, but it is **not** a JSON format: it calls
`GenerateUSMap` and writes the identical binary `.usmap` serializer bytes to a `.jmap`
filename. See [LuaAPI/COMPAT.md](../LuaAPI/COMPAT.md).

---

## Cross-references

* [README.md](README.md): generation pipeline and triggers.
* [SdkAPI/REFERENCE.md](../SdkAPI/REFERENCE.md): reflection DB with the same information in
  JSON form.
* [RuntimeAPI/IPC.md](../RuntimeAPI/IPC.md): `USMAP` command.
