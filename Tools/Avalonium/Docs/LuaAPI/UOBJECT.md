# LuaAPI: UObject, Value Types & Containers

How Lua sees engine objects. Implemented in [`LuaUObject.cpp`](../../Runtime/lua/LuaUObject.cpp),
[`LuaTypes.cpp`](../../Runtime/lua/LuaTypes.cpp), with wrapper extensions from
`LuaCompatApi.cpp` / `LuaCompatParity.cpp` / `LuaCompatSurface.cpp`.

---

## 1. The UObject wrapper

Any UObject pushed to Lua (from `GetPlayer()`, `FindObject`, hook parameters, …) is a userdata
carrying its `UObject*` with a metatable whose `__index` / `__newindex` resolve members in
this order:

1. **Compatibility surface members** (`Compat_Surface_Index`, `Mod` handles, string members,
   `GetRegion`-style helpers).
2. **Named engine members** (the tables below, case-insensitive).
3. **Reflected property lookup**: any property name resolves live through `FProperty`
   (`FindPropertyOnClassEx`, superclass-walking).
4. **Reflected function lookup**: any UFUNCTION short name (with `K2_` fallbacks) becomes a
   callable.

We resolve named members first, reflected properties second, functions last. Reflection is the
fallback, not the opening move, the order matters when a property and a function share a name.

Property reads return *live values*: vectors, rotators, transforms, strings, containers and
struct values read straight out of engine storage. Property writes go through
`SetPropertyValueEx`. Hook parameter views (`PushPropertyView`) accept writes too, so a
pre-hook can modify parameters in place (out-params included).

### 1.1 Identity

| Member | Kind | Returns |
| --- | --- | --- |
| `GetName` / `GetFName` | method | short name (string / FName handle) |
| `GetFullName` / `GetPathName` | method | full path |
| `GetAddress` | method | raw pointer (light userdata) |
| `GetAddressString` | method | `"0x…"` |
| `GetClass` | method | `UClass` |
| `GetOuter` / `GetPackage` / `GetWorld` | method | outer chain |
| `GetSuper` / `GetSuperClass` | method | super struct/class |
| `GetCDO` / `GetClassDefaultObject` | method | class default object |
| `IsValid` | method | live-object check |
| `type` | method | type name string |
| `__tostring` | metamethod | `"ClassName 0x…"` style description |

We treat `IsValid` as a live check, not a lifetime guarantee. The object can die right after it
returns true; the next read is then a fault waiting for a handler.

### 1.2 Reflection

| Member | Kind | Behavior |
| --- | --- | --- |
| `GetProperty(name)` | method | property value (supports `"A.B.C"` nested paths) |
| `FindProperty(term)` | method | first matching property **name** (case-insensitive substring), or `nil`; it does not return a descriptor |
| `GetPropertyByName` / `SetPropertyByName` | method | read/write by name (compat batch 1) |
| `GetPropertyValue` / `SetPropertyValue` | method | value read/write |
| `ExportProperty(name)` | method | read a single property by name (returns typed value) |
| `ImportProperty(name, value)` | method | write a single property by name (returns `true` on success) |
| `GetProperties` / `DumpProperties` | method | enumerate properties (max 512 in `DumpProperties`) |
| `GetFunctions` / `DumpFunctions` | method | enumerate functions |
| `ForEachProperty(fn)` | method | `fn(propertyWrapper)`, super-inclusive |
| `ForEachFunction(fn)` | method | `fn(functionWrapper)`, super-inclusive |
| `GetFunctionFlags` / `SetFunctionFlags` | method | UFunction flags |
| `Call(name, ...)` | method | invoke a UFunction on the object |
| `Reflection` | method | reflection descriptor table |
| `HasFunction(name)` | method | boolean |
| `IsA(className)` / `IsChildOf(class)` | method | inheritance tests |
| `IsClass` / `IsAnyClass` | method | class-ness |
| `Cast(class)` | method | cast (nil on failure) |
| `GetChildClasses` | method | derived classes |
| `GetComponent(name)` | method | named component |
| `GetInterfaceClass` | (property wrapper) | interface class of an FInterfaceProperty |

I treat `FindProperty` as a substring search, not an exact lookup. Ask for `"Health"` and
`"MaxHealth"` may come back first; the winner is whatever the class layout happens to produce.

Flags: `HasAllFlags`, `HasAnyFlags`, `HasAnyInternalFlags` (compat batch 1; values see
`EObjectFlags` / `EInternalObjectFlags` in [COMPAT.md](COMPAT.md)).

### 1.3 Calling reflected functions

* `obj:SomeFunction(args…)`: resolved by name, then `K2_SomeFunction`; called through
  ProcessEvent with parameter marshalling (`CallUFunctionDynamic`).
* `obj:Call(name, args…)` / `obj:CallFunction(name, args…)`, dynamic form; parameters may be
  passed positionally or as a named table depending on the parameter shapes.
* `GetKismetLibrary():Function(args)` for static libraries (CDO calls).
* Return values are marshalled back, including struct/container returns.

### 1.4 Actor / component members

| Group | Members |
| --- | --- |
| Transform | `GetTransform`, `SetTransform`, `GetLocation`, `SetLocation`, `GetRotation`, `SetRotation`, `GetScale`, `SetScale`, `GetScale3D`, `K2_GetActorLocation`, `K2_GetActorRotation`, `K2_GetActorScale3D`, `K2_GetActorTransform`, `K2_SetActorLocation`, `K2_SetActorRotation`, `K2_SetActorScale3D`, `K2_SetActorTransform` |
| Movement | `Teleport`, `TeleportTo`, `K2_TeleportTo`, `AddActorWorldOffset`, `AddActorWorldRotation`, `AddActorLocalOffset`, `AddActorLocalRotation`, `GetVelocity`, `GetActorForwardVector`, `GetForwardVector`, `GetActorRightVector`, `GetRightVector`, `GetActorUpVector`, `GetUpVector` |
| Bounds / state | `GetActorBounds`, `GetBounds`, `IsHidden`, `SetActorHiddenInGame`, `SetActorEnableCollision`, `GetAttachedActors`, `GetDistanceTo` |
| Attach | `AttachToActor`, `AttachToComponent`, `DetachFromActor` |
| Components | `GetComponents`, `GetComponentsByClass`, `GetRootComponent` |
| Lifetime | `DestroyActor` / `K2_DestroyActor`, `SpawnActor` (on a world/class) |
| Level | `GetLevel`, owning `ULevel` via the outer-chain walk |
| Misc | `ProcessConsoleExec` (compat lifecycle), `IsMappedToObject`, `IsMappedToProperty` |

All engine access is SEH-guarded and degrades to `nil`/`false` when an offset is unavailable
on the current engine version.

### 1.5 DataTable members

| Member | Behavior |
| --- | --- |
| `GetRowMap` | row map data |
| `GetAllRows` | all rows as a table |
| `GetRowStruct` | row `UScriptStruct` |
| `GetRowNames` | row names |
| `FindRow(name)` | single row (or `Get`) |
| `AddRow(name, table)` | insert/update row |
| `RemoveRow(name)` | delete row |
| `EmptyTable()` | clear all rows |
| `ExportAsJSON([pretty])` | serialize all rows to a JSON string |
| `__len` | row count |

Row values are pushed as tables of reflected fields (`CompatPushRowTable`), using
`CompatRowMapOffset`/`CompatRowStruct`/`CompatRowMapStride`: reflection first, then layout
probing (stride 16 or 24 bytes).

### 1.6 UEnum members

| Member | Behavior |
| --- | --- |
| `GetNameByValue(v)` | enum-entry name |
| `ForEachName(fn)` | iterate entries |
| `GetEnumNameByIndex(i)` | entry by index (`GetNameByIndex` is not registered) |
| `EditNameAt(i, name)` | best-effort mutation |
| `EditValueAt(i, value)` | best-effort mutation |
| `InsertIntoNames(name, value)` | insert |
| `RemoveFromNamesAt(i)` | remove |

Enum mutation works against verified name arrays (`EEnumFlags`-aware); it degrades to
no-ops when the arrays cannot be verified.

### 1.7 Invalid objects

`CreateInvalidObject()` returns a neutral handle whose methods resolve (returning neutral
values) and whose reads return `nil`, useful when a mod wants a "safe" placeholder that will
not error on use. Backed by the `INVALID_OBJECT_MT` metatable.

---

## 2. Value types

Constructors are globals; values are userdata with `__index`/`__newindex` (fields are
live-read from the underlying struct where they came from engine storage).

| Global | Aliases | Fields / members |
| --- | --- | --- |
| `FVector` | `Vector` | `X Y Z`, `Length`, `Size`, `LengthSquared`, `SizeSquared`, `Normalize`, `Dot`, `Cross`, `Distance`, `ToTable`; operators `+ - * /` unary-minus, `==` |
| `FRotator` | `Rotator` | `Pitch Yaw Roll`, `ToTable`, `Normalize`, `Clamp`, arithmetic `+ -` |
| `FTransform` | `Transform` | `Location`, `Rotation`, `Scale3D`, `Translation`, `Scale`, `ToTable` |
| `FLinearColor` | `FColor` (constructor alias) | `R G B A`, arithmetic `+ - *` |
| `FQuat` | N/A | `X Y Z W`, `Rotator`, `ToRotator`, `type` (compat parity) |
| `FVector2D` | N/A | `X Y` |
| `FVector4` | N/A | `X Y Z W` |
| `FSoftObjectPath` | `SoftObjectPath`, `TSoftObjectPtr` | `AssetPathName`, `GetAssetPathName`, `GetAssetPath`, `GetSubPathString`, `GetSubPathName`, `GetWeakPtr`, `GetWeakObjectPtr`, `GetObjectID`, `GetTagAtLastTest`, `Load`, `LoadSynchronous`, `ResolveObject`, `IsNull`, `IsValid`, `ToString`, `==` |
| `FDelegate` | `Delegate`, `MulticastDelegate`, `FMulticastScriptDelegate` | `Bind`, `Unbind`, `Add`, `Remove`, `RemoveAll`, `Clear`, `Broadcast`/`ProcessDelegate` (also `__call`), `IsBound`, `GetBindings`, `Bindings`, `__len`, `ToString` |
| `FWeakObjectPtr` | `WeakObjectPtr` | `Get()` (also `__call`), `IsValid`, `type`, `__tostring` (`FWeakObjectPtr(0x…) valid/stale`); constructor accepts a UObject, raw pointer, another weak handle or `{Object=…}` |

Vectors/rotators/colors/transforms also push as tables when required (`.ToTable`), and
`LuaEngine::GetVector/GetRotator/GetColor` accept either form as arguments.

### 2.1 Math table

```lua
Math = { Lerp, Clamp, VInterpTo, RInterpTo, FindLookAtRotation }
```

* `Lerp(a, b, t)`, `Clamp(x, min, max)`.
* `VInterpTo(current, target, deltaTime, interpSpeed)`, `RInterpTo(...)` rotator form.
* `FindLookAtRotation(start, target)`.

---

## 3. Containers

Containers are **live views** over engine storage (they hold the array/map/set address and
inner property descriptor). Mutating methods are intentionally refused on `TMap`/`TSet`
(`TMap_RefuseMutation` / `TSet_RefuseMutation`). `TArray` exposes read access and `Empty()`;
There's no push API, and that's not an oversight. We don't hand Lua the ability to resize
memory the engine still owns; allocator corruption is the alternative, and a missing
convenience is the cheaper outcome.

### `TArray`

| Member | Behavior |
| --- | --- |
| `Get(i)` / `[i]` (1-based) | element view/value |
| `Num` / `Count` / `__len` | element count |
| `GetArrayNum`, `GetArrayMax`, `GetArrayAddress`, `GetArrayDataAddress` | raw introspection |
| `Empty` | zeroes the logical element count without freeing the allocation |
| `ForEach(fn)` | iterate live elements |
| `IsValid`, `type`, `ToString` | helpers |

`Add`, `PushElement`, `RemoveAt`, `Remove`, `Clear`, `Contains` and `ToTable` are **not**
registered for `TArray`.

### `TMap`

| Member | Behavior |
| --- | --- |
| `Find(key)` / `Contains(key)` / `ContainsKey(key)` | lookup |
| `ForEach(fn)` | `fn(key, value)` over live pairs |
| `Num` / `Count` / `__len` | size |
| `type`, `ToString` | helpers |

### `TSet`

| Member | Behavior |
| --- | --- |
| `Contains(v)` | membership |
| `ForEach(fn)` | iterate |
| `Num` / `Count` / `__len`, `type`, `ToString` | helpers |

### Delegates

Live multicast/single-cast delegates can be `Bind`/`Add`ed with Lua functions, `Broadcast`ed,
queried (`IsBound`, `GetBindings`) and cleared.

---

## 4. String member surface

Engine string values (`FString`/`FName`/`FText` payloads pushed as string-like handles)
support, in addition to Lua's own string methods:

```
ToString Len Length IsEmpty Find StartsWith EndsWith ToUpper ToLower Append Concat
Clear Empty IsValid Clone GetFName GetName GetFullName type
```

These work in both call shapes: `("abc"):Len()` and `string.Len("abc")`. The members are
installed into the live string metatable's `__index` at registration.

`FName`, `FText`, `FString`, `FUtf8String`, `FAnsiString` constructors and their full member
sets are documented in [COMPAT.md](COMPAT.md#3-name-text-and-string-types).

---

## 5. Property wrappers (`FProperty`)

`GetProperty`, `ForEachProperty` and compatibility pipelines push **property wrappers**,
userdata around a raw `FProperty/UProperty` address with an owner. Members (case-insensitive):

| Group | Members |
| --- | --- |
| Identity | `GetName`, `Name`, `GetFullName`, `GetPathName`, `GetOuter`, `GetOutermost`, `GetClass`, `GetPropertyClass`, `type` |
| Addressing | `GetOffset`, `Offset`, `GetOffset_Internal`, `Offset_Internal`, `GetElementSize`, `ElementSize`, `GetArrayDim`, `ArrayDim`, `GetSize`, `Size`, `GetByteOffset`, `ByteOffset`, `GetByteMask`, `ByteMask`, `GetFieldMask`, `FieldMask`, `GetFieldSize`, `FieldSize`, `ContainerPtrToValuePtr`, `PropertyAddress` |
| Types | `GetInner`, `GetStruct`, `GetEnum`, `GetInterfaceClass`, `GetOwner`, `IsArrayLike`, `IsOutParam`, `IsShown` |
| Values | `GetValue`, `SetValue`, `Get(address)`, `ImportText` |
| Flags | `PropertyFlags` |

`DescribePropertyEx` makes property semantics identical everywhere a property pointer is read, hook parameters, container elements, `__index`/`__newindex`, compat iterators.

---

## 6. Compatibility wrapper batch (summary)

Registered by `LuaCompatApi.cpp` / `LuaCompatParity.cpp` and reachable from the UObject index
handler:

* `FWeakObjectPtr` / `WeakObjectPtr` constructor + methods.
* `FProperty` wrapper (above).
* `UEnum`, `UDataTable`, `GetLevel` members.
* `UStruct` super-inclusive `ForEachProperty` / `ForEachFunction`.
* `HasAllFlags` / `HasAnyFlags` / `HasAnyInternalFlags` / `IsClass` / `GetPropertyByName` /
  `SetPropertyByName` / `Reflection`.

Full compatibility list: [COMPAT.md](COMPAT.md).

---

## Cross-references

* [GLOBALS.md](GLOBALS.md): finders, memory, CVars.
* [HOOKS.md](HOOKS.md): hook parameter views (same property machinery).
* [COMPAT.md](COMPAT.md): `EObjectFlags`, `PropertyTypes`, `FName`/`FText`.
* [DiscoveryAPI/REFERENCE.md](../DiscoveryAPI/REFERENCE.md#9-objectutilsh): property identity
  and alignment heuristics used underneath.
