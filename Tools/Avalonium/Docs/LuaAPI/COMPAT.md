# LuaAPI: Compatibility Surface (v3.5)

The compatibility layer lets scripts written for other Unreal Lua runtimes (UE4SS)
run unmodified against Avalonium; we aim for source compatibility, not bug-for-bug cosplay. If
a script from another runtime works, good, reproducing its bugs was never the job. It is
registered in three batches from
`LuaCompatApi.cpp` (batch 1 + wrappers), `LuaCompatParity.cpp` (parity globals) and
`LuaCompatSurface.cpp` (surface closure), plus the wrapper work in
`LuaCompatParity.cpp` and `LuaUObject.cpp`.

**Surface version: `COMPAT_API_VERSION_MAJOR = 3`, `COMPAT_API_VERSION_MINOR = 5`**

Registration logs (useful for verifying the running build):

```
[Lua] Compat surface v3.5: wrappers=on, datatable-mutation=on, weak-objects=on
[Lua] Compat surface v3.5: string-api=on, output-device=on, reflection-members=on
```

---

## 1. Version tables

### `UnrealVersion`: detected engine

| Member | Kind |
| --- | --- |
| `Major`, `Minor` | numbers read from discovery (`g_State.UEMajor/UEMinor`) |
| `GetMajor()`, `GetMinor()` | functions |
| `IsEqual`, `IsAtLeast`, `IsAtMost`, `IsBelow`, `IsAbove` | `(major, minor)` comparisons |

### `RuntimeVersion`: this runtime

```lua
RuntimeVersion = {
  Major = 3, Minor = 0, Hotfix = 1,
  GetVersion = function() return 3, 0, 1 end,
  CompatMajor = 3, CompatMinor = 5,
}
```

The same table is also published as the global **`Avalonium`**, so scripts can
either feature-detect (`if RuntimeVersion.CompatMinor >= 5`) or type-check the host.
We feature-detect on `CompatMinor`. The top-level `Major` is the runtime's own version and
tells a mod absolutely nothing about the compat surface.

---

## 2. Thread introspection

| Global | Returns |
| --- | --- |
| `GetCurrentThreadId` | runtime thread id (number) |
| `GetMainModThreadId` | mod-loading thread id |
| `GetGameThreadId` | game thread id |
| `GetAsyncThreadId` | async thread id |
| `IsInGameThread` / `IsInMainModThread` / `IsInAsyncThread` | booleans |
| `EngineTickAvailable` / `ProcessEventAvailable` | both `true` (both execution paths exist) |
| `EGameThreadMethod` | `{ EngineTick = 0, ProcessEvent = 1 }` |
| `MainThread`, `HookThread`, `AsyncThread` | real Lua threads (coroutine handles) usable as execution contexts |
| `ThreadId` | constructor for a thread-id handle |
| `ThreadIdUserdata` / `threadid` | the thread-id type record (metatable; compare with `getmetatable()` on a live handle) |

Thread-id handles: `ToString()`, `get()`, `GetId()`, `type()` → `"ThreadIdUserdata"`,
`IsValid()`; `==` compares ids.

Both execution paths exist, and we don't treat them as interchangeable. Run mod code on the
wrong thread and the engine state corrupts in ways that look like random crashes, because
that's exactly what they are.

`__OriginalReturnValue` is a global slot holding the engine's return value in post hooks
(initialized to `nil`; read defensively).

---

## 3. Name, text and string types

### `FName`

Final `FName` global is the surface constructor:

```lua
FName("SomeName")            -- default flag FNAME_Add (creates through the engine writer)
FName("SomeName", FNAME_Find)-- pure lookup, no creation
FName(index, _, number)      -- from a comparison index (+ optional number)
```

We use `FNAME_Find` when a lookup is all that's wanted. The default `FNAME_Add` doesn't just
find a name; it creates one, and enough of those pollute Epic's pool and slow every later
lookup.

`EFindName = { FNAME_Find = 0, FNAME_Add = 1, FNAME_Replace_Not_Safe_For_Threading = 2 }`.
`NAME_None` is a pre-made handle for index 0.

Handle members: `ToString()`, `get()`, `GetComparisonIndex()`, `GetNumber()`, `Equals(x)`,
`IsValid()`, `type()` → `"FName"`; `__tostring` renders `"Name"` or `"Name_<number>"`;
`__concat` joins like text; `__eq` compares comparison indices.
`FNameUserdata` / `fname` are the type records.

### `FText`

`FText(text)` value wrapper with `ToString()`, `get()`, `type()` → `"FText"`.
Text properties read from engine memory (including hook parameter views) push a reference
wrapper that resolves the live `FString` payload; both flavors answer `ToString()`.

### `FString` / `FUtf8String` / `FAnsiString`

Constructors for engine string values. `FString(x)` accepts strings/numbers and produces a
string-like handle carrying the members in [UOBJECT.md §4](UOBJECT.md#4-string-member-surface)
(`Len`, `Find`, `StartsWith`, `GetFName`, …).

### `FPackageName`

| Member | Behavior |
| --- | --- |
| `IsShortPackageName(name)` | short vs long package name |
| `IsValidLongPackageName(name)` | long-name validation |

---

## 4. Constant tables

### `Key`

Every virtual-key name as a numeric code, plus convenience aliases:
`Key.F9`, `Key.SPACE`, `Key.OEM_THREE` (not `OEM_3`), mouse buttons, media/browser keys and
IME keys, plus `Key.LEFT_CLICK` / `RIGHT_CLICK` aliases (compat table in `LuaCompatApi.cpp`).
The hook manager merges its CamelCase table (`Key.LeftShift`, `Key.SpaceBar`,
`Key.PageDown`, gamepad names, …) into the same global, so both spellings resolve.
Arguments to key APIs accept `Key.*` codes or the equivalent names, both file under the
same canonical bind name.

### `ModifierKey`

```
SHIFT CONTROL ALT  LEFT_SHIFT RIGHT_SHIFT  LEFT_CONTROL RIGHT_CONTROL  LEFT_ALT RIGHT_ALT
```

### `EObjectFlags` (version-aware)

```
RF_NoFlags RF_Public RF_Standalone RF_MarkAsNative RF_Transactional RF_ClassDefaultObject
RF_ArchetypeObject RF_Transient RF_MarkAsRootSet RF_TagGarbageTemp RF_NeedInitialization
RF_NeedLoad RF_NeedPostLoad RF_NeedPostLoadSubobjects RF_NewerVersionExists RF_BeginDestroyed
RF_FinishDestroyed RF_BeingRegenerated RF_DefaultSubObject RF_WasLoaded RF_TextExportTransient
RF_LoadCompleted RF_InheritableComponentTemplate RF_DuplicateTransient RF_StrongRefOnFrame
RF_NonPIEDuplicateTransient RF_WillBeLoaded RF_HasExternalPackage
```

Family-specific spellings: `RF_KeepForCooker` (all) / `RF_GeneratePublicHash` (5.6+),
`RF_Dynamic` (pre-5.6) / `RF_ImmutableDefaultObject` (5.6+), `RF_MirroredGarbage` (UE5),
`RF_MigratingAsset` + `RF_HasDynamicImports` (5.8+), `RF_AllocatedInSharedPage` (UE5),
and `RF_AllFlags` (`0x1FFFFFFF` pre-UE5, `0xFFFFFFFF` on UE5).

### `EInternalObjectFlags`

```
ReachableInCluster (1<<23) ClusterRoot (1<<24) Native (1<<25) Async (1<<26)
Unreachable (1<<28) RootSet (1<<30)
Garbage / PendingKill (1<<29)
```

UE5 adds `AsyncLoadingPhase1 (1<<22)` / `AsyncLoadingPhase2 (1<<27)`; both families carry
`AsyncLoading (1<<27)`. Also `GarbageCollectionKeepFlags` and `AllFlags (0x7F800000)`.

### `PropertyTypes`

A registry of property type records, each with `Name`, `Size`, and lazily-resolved
`FFieldClassPointer` (the type's class-like identity: `UClass*` on legacy engines,
`FFieldClass*` on modern ones; `0` until observed). `StaticPointer` exists for shape
compatibility and is `0`.

```
ObjectProperty(8) ObjectPtrProperty(8) Int8Property(1) Int16Property(2) IntProperty(4)
Int64Property(8) ByteProperty(1) UInt16Property(2) UInt32Property(4) UInt64Property(8)
NameProperty(8) FloatProperty(4) BoolProperty(1) ArrayProperty(16) SetProperty(16)
MapProperty(32) StructProperty(0) ClassProperty(8) WeakObjectProperty(8) EnumProperty(1)
TextProperty(8) StrProperty(16) Utf8StrProperty(16) AnsiStrProperty(16)
InterfaceProperty(8) DelegateProperty(32) MulticastDelegateProperty(16)
MulticastInlineDelegateProperty(16) MulticastSparseDelegateProperty(8) SoftObjectProperty(16)
SoftClassProperty(16) DoubleProperty(8)
```

---

## 5. `Mod` / `ModRef`

`Mod` and `ModRef` are the same callable handle (a function with a metatable), representing
the currently executing mod:

| Member | Behavior |
| --- | --- |
| `ModRef:SetSharedVariable(name, value)` | Game-wide named store (`_SharedVariables`), readable by every mod; there is no bare `SetSharedVariable` global |
| `ModRef:GetSharedVariable(name)` | Read the store; there is no bare `GetSharedVariable` global |
| `ModRef.OnUnload = function()` | Stores a cleanup callback (`_ModUnloadCallbacks`, keyed by detected mod name, falling back to `__main__`). The runtime does not currently invoke it on reload/uninstall |
| `ModRef.type()` | `"ModRef"` |
| `ModRef.Name = value` | Assigning any other key falls through to the callable itself, so `local m = ModRef; m.Name = v` export patterns keep working |

Detected mod name comes from the running Lua call stack (`DetectCurrentModName`), a mod's
`OnUnload` is attributed to the mod that assigned it, not to another mod's file.

---

## 6. Logging and memory helpers

| Global | Behavior |
| --- | --- |
| `Log(...)` | Runtime log (compat form; also used by internal surface code) |
| `Print(...)` | Bound to `print` when available, else `Log` |
| `DerefToInt32(ptr)` / `dereftoint32` | Raw 32-bit read at an address |
| `CreateInvalidObject()` | Neutral object handle (methods resolve, reads return neutral values) |

---

## 7. Parity globals

### Finders & modules

| Global | Behavior |
| --- | --- |
| `FindObject` / `FindObjects` | Overloads documented in [GLOBALS.md §3.1](GLOBALS.md#31-findobject-compatibility-overloads-final-registration), these are the final registrations |
| `LoadExport(name)` / `loadexport` | Loads `shared/exports/<name>.lua`, then `shared/<name>.lua`, then `exports/<name>.lua`, then `<name>.lua` from the mods root; returns the loaded value |
| `IterateGameDirectories()` | Iterates game directories |
| `CreateLogicModsDirectory()` | Creates the LogicMods directory |
| `RestartMod(name|nil)` / `RestartCurrentMod` | Ignores the supplied name and reloads all mods |
| `UninstallMod(name|nil)` / `UninstallCurrentMod` | Sets `"enabled": false` for the mod in `mods.json`, then reloads; it does not delete files |

`RestartMod` has a name argument and ignores it. We pass a mod name expecting a single
restart and the whole load set comes back instead.

### Custom properties & console

| Global | Behavior |
| --- | --- |
| `RegisterCustomProperty({ Name=…, Type=…, BelongsToClass=… })` | Inject a synthetic property readable/writable through the UObject index handler (registry consulted after reflected lookup fails); the implementation takes one table, not positional arguments |
| `RegisterConsoleCommandGlobalHandler([name], callback)` | Named handler, or wildcard when no name/empty name (every command reaches it; `true` blocks execution) |
| `RegisterKeyBindAsync` | Transparent alias of `RegisterKeyBind` (input dispatch is already asynchronous) |
| `OutputDeviceRef` | The output-device handle (`Log`/`Warn`/`IsValid`) console handlers are handed, published as a global so a script can reach a working device before any handler has run |

### Lifecycle pre/post aliases

```
RegisterBeginPlayPreHook/PostHook      RegisterEndPlayPreHook/PostHook
RegisterLoadMapPreHook/PostHook        RegisterInitGameStatePreHook/PostHook
RegisterProcessConsoleExecPreHook/PostHook
RegisterCallFunctionByNameWithArgumentsPreHook/PostHook
RegisterULocalPlayerExecPreHook/PostHook
```

The last three answer from the runtime's own console-command dispatch: they fire
when a command runs through `ExecuteConsoleCommand`, because the engine members
they name have no script binding in any supported version (4.23 through 5.8) and
cannot be reached through reflection. Argument shapes follow the reference
runtime's native callbacks -- `(context, cmd, params, outputDevice, executor)`,
`(context, cmd, outputDevice, executor, bForce)`, `(context, world, cmd,
outputDevice)` -- with object handles where that runtime passes remote
parameters. A pre callback returning `true` claims the command.

### Dump & generation

```
GenerateSDK()  GenerateUHTCompatibleHeaders()  DumpUSMAP()  DumpJMAP()
DumpAllActors()  DumpStaticMeshes()
```

These call into the SDK/USMap generators (see [SdkAPI](../SdkAPI/README.md),
[UsmapAPI](../UsmapAPI/README.md)). `DumpAllObjects` (non-compat) writes the object dump.

### Value wrappers added by parity

```
FQuat  FVector2D  FVector4  FWeakObjectPtr / WeakObjectPtr
```

See [UOBJECT.md §2](UOBJECT.md#2-value-types). `FWeakObjectPtr` members: callable `Get()`,
`IsValid()`, `type`, `__tostring` (`valid`/`stale`); constructor accepts a UObject, raw
pointer, existing weak handle or `{Object=…}`.

### Wrapper batch (on the UObject index handler)

* `HasAllFlags` / `HasAnyFlags` / `HasAnyInternalFlags`, `IsClass`,
  `GetPropertyByName` / `SetPropertyByName`, `Reflection`.
* `UStruct:ForEachProperty` / `ForEachFunction` (super-inclusive), `GetFunctionFlags` /
  `SetFunctionFlags`.
* `UEnum`: `GetNameByValue`, `ForEachName`, `GetEnumNameByIndex` (`GetNameByIndex` is not
  registered), `EditNameAt`, `EditValueAt`, `InsertIntoNames`, `RemoveFromNamesAt`.
* `UDataTable`: `GetRowMap`, `GetAllRows`, `GetRowStruct`, `AddRow`, `RemoveRow`,
  `EmptyTable`, `ExportAsJSON`, `__len`.
* `GetLevel` via outer-chain walk.
* `FProperty` wrapper (see [UOBJECT.md §5](UOBJECT.md#5-property-wrappers-fproperty)).

---

## 8. Runtime-built UMG mods

Mods that build their whole interface as UMG widgets at run time -- no assets, no
Blueprint classes; `StaticConstructObject` of `/Script/UMG` classes, assembled
through `WidgetTree`/`RootWidget`, shown with `AddToViewport` -- run unmodified.
Validated against a cross-runtime mod (TrueFPS v1.1.0) that creates its
~60-widget settings menu this way:

```lua
local umg = StaticFindObject("/Script/UMG.UserWidget")
local w = StaticConstructObject(umg, pc)
local tree = StaticConstructObject(StaticFindObject("/Script/UMG.WidgetTree"), w)
w.WidgetTree = tree
tree.RootWidget = StaticConstructObject(StaticFindObject("/Script/UMG.Overlay"), tree)
w:SetOwningPlayer(pc)
w:AddToViewport(1000)
```

What the surface provides for this pattern:

- `StaticConstructObject` creates the object through the engine's own reflected
  `GameplayStatics::SpawnObject`, so a class with `ClassWithin` rules and native
  constructors initialises exactly as the engine would construct it.
- Method calls and property writes go through reflection. An `FText` argument is
  built by the engine itself (`KismetTextLibrary::Conv_StringToText`, present
  from 4.23 through 5.8), so widget text properties hold real engine text; enum
  arguments are written at the backing property's width; a value that yields no
  text is answered with a valid empty text, never a zeroed slot.
- Slate construction is deferred to the engine: `AddToViewport`/`TakeWidget`
  build the widget tree inside the engine's own code path, on the game thread,
  through the same `ExecuteInGameThread`/poll-loop pattern the mods already use.
- The menu lifecycle of this class of mods is covered by the surface already
  documented above: per-mod `require`, `Key`/`ModifierKey`, `RegisterKeyBind`,
  `ExecuteInGameThread`, `LoopAsync`/`ExecuteWithDelay`, `RegisterHook`,
  `RegisterConsoleCommandHandler`.

Interface-building helper code stays mod-side: anything beyond the engine's own
classes is Lua, exactly as it would be on the reference runtime.

---

## 9. Compatibility batch history

| Batch | CompatMinor | Contents |
| --- | --- | --- |
| 1 | 1 | Constant tables (`Key`, flags, `PropertyTypes`, `EFindName`), `UnrealVersion`, `FName`/`FText`/`FString`, thread introspection, `RuntimeVersion` |
| 2 | 2 | Method-bearing wrappers: `FWeakObjectPtr`, `FProperty`, `UEnum`/`UDataTable` mutation, `ForEachProperty/Function`, `GetLevel` |
| 3 | 3 | Surface closure: `Mod`/`ModRef`, `Log`/`Print`, `DerefToInt32`, `EGameThreadMethod`, `__OriginalReturnValue`, `MainThread`/`HookThread`/`AsyncThread`, string members |
| Parity | 5 | FindObject/FindObjects overloads, `LoadExport`, mod restart/uninstall, directory helpers, custom properties, wildcard console handler, lifecycle pre/post hooks, dump/generation family, `FQuat`/`FVector2D`/`FVector4`, output-device global (`OutputDeviceRef`), return-value-first marshaling, numeric enum returns, table-filled out parameters |

A mod can require a minimum surface with:

```lua
assert(RuntimeVersion.CompatMinor >= 5, "Avalonium compat surface too old")
```

---

## Cross-references

* [README.md](README.md) · [GLOBALS.md](GLOBALS.md) · [UOBJECT.md](UOBJECT.md) · [HOOKS.md](HOOKS.md) · [MODS.md](MODS.md)
* [SdkAPI](../SdkAPI/README.md) · [UsmapAPI](../UsmapAPI/README.md): generation globals.
* [OverlayAPI](../OverlayAPI/README.md): `Overlay` notifications surface.
