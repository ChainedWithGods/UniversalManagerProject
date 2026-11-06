# LuaModding.md: Avalonium Lua API (complete, source-verified)

Avalonium embeds **Lua 5.4.6** (statically linked, `Helpers/lua-5.4.6`) inside a shipping
Unreal Engine game and registers a scripting surface on one `lua_State`. This document is
the **complete, authoritative** list of that surface: every global, table, value type,
container, UObject member and hook that the runtime actually registers.

It is derived directly from the runtime source under `Runtime/lua/` (and the overlay
bindings in `Runtime/overlay/LuaOverlay.cpp`), which is the only source of truth. Where an
older document said something the code does not do, this document follows the code.

Engine support: **UE 4.23 – 4.27 and 5.0 – 5.8**. Nothing in the surface is hardcoded per
game or per engine version; versions, offsets and flags are read out of the running process.

Source files that register the surface:

| File | Registers |
| :--- | :--- |
| `LuaManager.cpp` | core globals, `File`, `JSON`, `ue`, `Memory` |
| `LuaTypes.cpp` | `FVector`/`FRotator`/`FTransform`/`FLinearColor`/`FSoftObjectPath`/`FDelegate`, `Math`, `ModRef`, `__SharedModuleRoot`, `UEHelpers` preload |
| `LuaUObject.cpp` | UObject metatable, reflected calls, `SpawnActor`, `StaticClass`/`StaticStruct`/`StaticEnum`, `NewObject`/`ConstructObject`/`DuplicateObject`, `LoadAsset`/`StaticLoadObject`, `GetBlueprintPipeline`, `GetObjectShape` |
| `LuaHookManager.cpp` | hooks, custom events, keybinds, console, timers, threads, type generation; calls the three compat registrars |
| `LuaCompatApi.cpp` | compat batch 1: `UnrealVersion`, `FName`/`FText`/`FString`, `Key`, `ModifierKey`, flags, `PropertyTypes`, thread ids, `RuntimeVersion` |
| `LuaCompatParity.cpp` | compat parity: overloads, `LoadExport`, mod restart/uninstall, directory tree, custom properties, wildcard console, lifecycle Pre/Post, dump/generate family, `FQuat`/`FVector2D`/`FVector4`/`FWeakObjectPtr`, `FProperty` wrapper |
| `LuaCompatSurface.cpp` | compat surface: `Log`/`Print`, `DerefToInt32`, `EGameThreadMethod`, `__OriginalReturnValue`, string members, `Mod`/`ModRef` handle, `Avalonium`/`UE4SS`, thread handles, UObject surface members |
| `BuildinMods/UniversalHUD.cpp`, `BuiltinMods/UniversalOverlay.cpp` | builtin mod globals |
| `../overlay/LuaOverlay.cpp` | the `Overlay` table |

**Compat surface version:** `COMPAT_API_VERSION_MAJOR = 3`, `COMPAT_API_VERSION_MINOR = 5`
(published as `RuntimeVersion.CompatMajor`/`CompatMinor`).

---

## 0. Registration order (matters!)

`LuaManager::RegisterBindings()` runs, in this exact order:

```
1. LuaEngine::RegisterLuaTypes          (value types, Math)
2. LuaEngine::RegisterLuaUObject        (UObject metatable, object-creation globals)
3. LuaEngine::RegisterLuaHookBindings   (hooks + compat batch1 + compat parity + compat surface)
4. RegisterGlobalHelpers                (core globals, File, JSON)  <-- overwrites some names
5. RegisterEngineGlobals                (FindFirstOf/FindAllOf/StaticFindObject/StaticConstructObject, UObjectGlobals)
6. RegisterUETable                      (the `ue` table)
7. RegisterMemoryBindings               (Memory table + ue memory names)
8. LuaEngine::RegisterCompatibilityLayer(__SharedModuleRoot, ModRef, GetModRef, UEHelpers preload)
9. Overlay::RegisterLuaOverlayBindings  (the `Overlay` table)
```

Because step 4 runs **after** step 3, the compatibility `FindObject`/`FindObjects`
overloads registered in `LuaCompatParity.cpp` are **overwritten** by the simple finders in
`LuaManager.cpp`. The live `FindObject`/`FindObjects` are therefore the simple by-name
finders documented in §3, not the multi-overload scanners. Likewise `ModRef` is
reassigned in step 8 (see §11). We let the later write win, registration order is destiny, so
the compatibility overloads are dead code in the live surface, not broken, just buried by the
registrar that ran after them.

---

## 1. Enablement, config, files

`.ini` keys (read from `Avalonium.ini`):

```ini
[Features]
EnableLuaMods = true     ; aliases: LuaMods, Lua. Default: true when absent.
LuaModsPath   =          ; default <dir of Avalonium.dll>\Mods\Avalonium
```

Default mods directory: **`<Avalonium.dll dir>\Mods\Avalonium`**.

Files the Lua system reads/writes:

| Path | Purpose |
| :--- | :--- |
| `<mods>/mods.json` / `<mods>/mods.txt` | mod list |
| `<mods>/shared/types/<GameName>/AvaloniumTypes.lua` | auto-generated EmmyLua type annotations |
| `<mods>/shared/` | shared modules; never loaded as a mod |
| `<mods>/Cache/` | reserved; never loaded as a mod |
| `<mods>/<Mod>/scripts/main.lua` | entry script |

IPC commands share the same state: `LUA STATUS`, `LUA EVAL <code>`, `LUA RUN <file>`,
`LUA LOAD`, `LUA DOFILE`, `LUA RELOAD`, `LUA RESET`.

---

## 2. Mod packaging & loading

### 2.1 List file

`mods.json` is preferred and order is honored:

```json
{ "mods": [ { "enabled": true, "name": "MyMod" }, { "enabled": false, "name": "Other" } ] }
```

The parser scans for `"mods"`, then reads each `{ … }`; it takes `name` and `enabled`
(`true`/`false`/`1`/`0`); unknown keys are skipped. `mods.txt` (legacy, `name:1` /
`name:true` per line, `;`/`#` comments) is used only when `mods.json` is absent or parses
empty. When neither exists, every folder on disk is auto-enabled alphabetically.

Rules: disabled → skipped; enabled but folder missing → skipped with a log; folder without
`scripts/main.lua`/`main.lua` → skipped; unlisted folders and top-level `.lua` files are
auto-loaded after the listed ones; `shared` and `Cache` are never mods.

### 2.2 Entry point

| Layout | Entry script |
| :--- | :--- |
| `Mods/Avalonium/MyMod/scripts/main.lua` | preferred |
| `Mods/Avalonium/MyMod/main.lua` | accepted |
| `Mods/Avalonium/foo.lua` | standalone (mod name = file name) |

Each mod runs in a **fresh environment table** whose metatable is `__index = _G` (isolated
globals), with a per-mod `require` resolver. A mod is identified by its folder name (or file
name for a standalone script); that name is used for logs and `OnUnload` attribution.

### 2.3 `require`

Search order for `require("name")` (dots become `/`), always local first:

```
<modRoot>/scripts/<name>.lua
<modRoot>/scripts/<name>/init.lua
<modRoot>/scripts/<name>/main.lua
<modRoot>/Scripts/<name>.lua          (+ /init.lua, /main.lua)
<modRoot>/<name>.lua
<modRoot>/<name>/init.lua
<modRoot>/<name>/main.lua
```

If nothing local matches, the standard Lua search path answers. Modules are cached per
**resolved file path** (`package.loaded["@<path>"]`), so two mods may each own a `config.lua`.
`package.preload["UEHelpers"]` and `package.preload["UEHelpers.UEHelpers"]` are set by the
compatibility layer (§11.7).

### 2.4 Reload

`ReloadMods()` / `HotReloadMods()` (IPC `LUA RELOAD`, hotkey **Alt+PageDown**): clears all
hooks, clears all timers, clears the loaded list, re-runs the full discovery pass. Timers and
hooks never survive a reload.

### 2.5 Type definitions

`AvaloniumTypes.lua` (EmmyLua) is generated automatically before mod loading when it does not
exist and the object index is built. Manual: `GenerateLuaTypes([dir])`.

---

## 3. Core globals

Every name below is registered as a **global** (unless marked "table member"). Names are
case-sensitive; aliases are listed explicitly.

### 3.1 Logging

| Global | Behaviour |
| :--- | :--- |
| `print(...)` | Writes `"[Lua] "` + args (tab-separated; strings/ints/floats/booleans/`0x…` light-userdata/`nil`/type-name) to the Avalonium log, filed under the `mods` section. Returns nothing. |
| `log(...)` | Same implementation as `print`. |
| `Log(...)` | Compat log form (from `LuaCompatSurface.cpp`); same sink, `[Lua] ` prefix. |
| `Print(...)` | Bound to the existing `print` if it is a function, else to the compat `Log` implementation. |

### 3.2 Player / world accessors

```
GetPlayer               GetPlayerPawn           GetPlayerCharacter
GetPlayerController     GetFirstPlayerController
GetPlayerCameraManager  GetCameraManager
GetLocalPlayer          GetGameInstance
GetGameMode             GetGameState
GetWorld                GetEngine
```

`GetPlayer` walks the local player controller's pawn properties (`AcknowledgedPawn`, `Pawn`,
`Character`, `TargetView`, `ControlledPawn`), filtering `Default__` CDOs, then falls back to
world actors. `GetPlayerPawn`/`GetPlayerCharacter` and `GetFirstPlayerController`/
`GetCameraManager` are aliases of `GetPlayer`/`GetPlayerController`/`GetPlayerCameraManager`.

### 3.3 Actor & object queries

```
GetAllActorsOfClass     GetAllActors            GetWorldActors
GetDistance
FindFirstOf             FindAllOf
FindObject              FindObjects             FindClass
StaticFindObject        StaticConstructObject
NewObject               ConstructObject         DuplicateObject
SpawnActor
StaticClass             StaticStruct            StaticEnum
LoadAsset               StaticLoadObject
GetObjectCount          GetObjectByIndex
GetObjectName           GetObjectFullName       GetObjectClass
GetSuperClass           InheritsFrom            GetClassProperties
CallFunction            ExportProperty          ImportProperty
GetBlueprintPipeline    GetObjectShape
```

* `GetAllActorsOfClass(className|class[, maxCount])` / `GetAllActors`, world actors of the
  class (name with/without `_C`); `"Actor"`/`"AActor"` = all.
* `GetWorldActors()`: all actors in the live world.
* `GetDistance(a, b)`: distance between two objects or two vectors.
* `FindFirstOf(className)` / `FindAllOf(className)`: class special-cases
  (`PlayerController`/`Character`/`Pawn`), then world actors, then the object index.
* **`FindObject(name)`**: the live implementation resolves a **single name** via the object
  index (`FindObjectByNameFast`), then a prefix search; extra arguments are ignored.
* **`FindObjects(name)`**: returns an array of objects whose short name matches (exact first,
  then prefix). (The compatibility overloads exist in source but are overwritten, see §0.)
* `FindClass(name)`: cached class lookup; `_C` not required.
* `StaticFindObject(path)` **or** `StaticFindObject(class|nil, outer, name[, exactClass])`.
  The string form resolves a path (`"/Script/Engine.Default__GameplayStatics"`); the second
  form bounds the search by class/outer with optional exact-class matching.
* `StaticConstructObject(class[, outer])` / `NewObject` / `ConstructObject`, construct via
  the engine, with an SEH fallback. Outer defaults to the live world.
* `DuplicateObject(source[, outer])`: construct a new instance of the source's class.
* `SpawnActor(world|class, …)`: see §4.1 (also a UObject member).
* `StaticClass(name)`: resolve a `UClass` by name.
* `StaticStruct(name)` / `StaticEnum(name)`: resolve a `UScriptStruct` / `UEnum` by name
  (index fast path, then full array scan).
* `LoadAsset(path)` / `StaticLoadObject(path)`: strip a leading type token
  (`Blueprint'/Game/Foo.Foo_C'` → `/Game/Foo.Foo_C`), load through the engine else find an
  already-loaded object.
* `GetObjectCount()` / `GetObjectByIndex(i)`: `GUObjectArray`.
* `GetObjectName(obj)` / `GetObjectFullName(obj)` / `GetObjectClass(obj)`.
* `GetSuperClass(struct)` / `InheritsFrom(objOrClass, baseName)`.
* `GetClassProperties(objOrClass)`: array of property descriptors.
* `CallFunction(obj, funcName, ...)`: resolve and dispatch a UFunction via ProcessEvent.
* `ExportProperty(obj, propName)`: read a single reflected property by name.
* `ImportProperty(obj, propName, value)`: write a single reflected property by name.
* `ue.get_globals()`: table `{ guobjectarray, gnames, gworld, gengine, gmalloc, gnatives }`;
  there is no top-level `GetGlobals` global.
* `GetBlueprintPipeline()`: reports the asset-registry binding discovery resolved:
  `assetPipelineReady`, `assetPipelineDetail`, `assetDataStruct`, `assetDataSize`,
  `assetDataFields[]` (`Name`/`Type`/`Offset`/`Size`), `assetRegistryFunctions`
  (`GetAsset`, `GetAssetRegistry`, `GetClass`, `IsValid`, `CreateAssetData` as booleans),
  `assetRegistry`, `assetDataOffset`, `getAssetFrameSize`, `objectPathOffset`,
  `packageNameOffset`, `assetNameOffset`, `canCreateNames`, `nameWidth`,
  `nameNumberOffset`, `memberChain` (`objectOuter`, `uFieldNext`, `structSuper`,
  `structChildren`, `childProperties`, `fieldName`, `fieldNext`), `blueprintGeneratedClass`,
  `blueprintActorClassCount`.
* `GetObjectShape(path)`: diagnostics for one object: `path`, `resolved`, `name`,
  `className`, `fullName`, `structFamilyByIdentity`, `isClassObject`, `structFamily`,
  `structClassName`, `classSuperChain[]`, `ownerIsReceiver`, and the enumerated member list.

### 3.4 `UObjectGlobals` table

```lua
UObjectGlobals = { FindFirstOf, FindAllOf, StaticFindObject, FindObjects, FindClass }
```

### 3.5 Kismet libraries

```
GetGameplayStatics            GetKismetSystemLibrary      GetKismetMathLibrary
GetKismetStringLibrary        GetKismetArrayLibrary       GetKismetTextLibrary
GetKismetGuidLibrary          GetKismetRenderingLibrary
```

Each returns the library CDO, e.g. `GetKismetMathLibrary():Abs(-3)`.

### 3.6 Timing, memory, GC

```
GetFrameTime   GetFPS   GetDeltaSeconds   GetMemoryUsage
ForceGarbageCollection   CollectGarbage
```

### 3.7 CVars

| Global | Behaviour |
| :--- | :--- |
| `GetCVar(name)` | Reads an `IConsoleManager` int CVar; falls back to `1` for a registered flag CVar. Returns `nil` when console is disabled. |
| `SetCVar(name, value)` | Executes `<name> <value>` through the console; returns boolean. |
| `FindCVars()` | First 10 000 registered CVar names. Returns `{}` when console is disabled. |
| `RegisterConsoleCommandHandler(cmd, fn)` | Registers `fn(command, args)` callback for console command `cmd`. Aliases: `RegisterConsoleCommand`, `RegisterCommand`. |
| `ExecuteConsoleCommand(cmd)` | Executes an arbitrary console command string. Alias: `ExecuteCommand`. |

> **Console availability:** All CVar and console-command operations require the Unreal Engine
> console manager to be discovered at startup. When `EnableConsole = false` (or the console is
> disabled in `Avalonium.ini`), `IConsoleManager` is not resolved: `GetCVar` returns `nil` and
> `FindCVars` returns an empty table `{}`. `SetCVar` and `ExecuteConsoleCommand` route via
> `KismetSystemLibrary::ExecuteConsoleCommand` if available, but direct console variables cannot
> be queried. Mods should check `if #FindCVars() > 0 then` or verify `GetCVar(...) ~= nil`
> before relying on CVar state. We make a disabled console return `nil` and empty tables, not
> throw, a mod that skips the check does nothing, which is worse than a crash.

### 3.8 Mod management

```
GetModDirectory   GetModsDirectory   ReloadMods   HotReloadMods
OtherLog, Mod, ModRef, GetModRef        (see §11)
```

### 3.8.1 Convenience helpers (registered in `RegisterEngineGlobals`)

These wrappers bake common recipes into a single call. They are top-level
globals, not members of `ue`.

| Global | Signature | Behaviour |
| :--- | :--- | :--- |
| `SpawnActorByClass` | `(className\|class, [loc], [rot], [bDeferConstruction]) -> UObject\|nil` | Spawns an actor of the given class through `UWorld::SpawnActor` / `UGameplayStatics::SpawnActor`. `loc` is `{X,Y,Z}` (default `0,0,0`), `rot` is `{Pitch,Yaw,Roll}` (default `0,0,0`). Class name accepts `_C` suffix or not. Returns `nil` and logs the reason on failure. |
| `GetAllPawns` | `() -> table` | Walks the live world's `PawnIterator` plus every `PlayerController->GetPawn`; falls back to the live object index. |
| `WithWorld` | `(callback) -> nil` | Schedules `callback(liveWorld)` to run on the engine game thread via the hook manager's game-thread queue. The callback is invoked asynchronously, never synchronously. |
| `WhenClassSpawns` | `(className, callback, [intervalMs=200]) -> timerId` | Polls every `intervalMs` until an instance of `className` exists, then fires `callback(actor)` once and auto-cancels. The returned handle can be passed to `CancelTimer` for early abort. |

### 3.8.2 `Director` table: time dilation, bullet time, freeze

Writes through `WorldSettings->TimeDilation` on the live world; tracks the
prior value so it can be restored on demand.

| Member | Signature | Behaviour |
| :--- | :--- | :--- |
| `Director.Set(scale)` | `(scale) -> boolean` | `true` if the world accepted the write. Sets world time dilation (clamped to `[0, 10]`); `0` = full freeze. |
| `Director.Get()` | `() -> number` | The current `TimeDilation`. `1.0` when no live world is present. |
| `Director.SlowFor(ms, scale)` | `(ms, [scale=0.15]) -> boolean` | `true` if the schedule was accepted. Sets dilation to `scale` for `ms` milliseconds, then restores the previous value. |
| `Director.ToggleBulletTime(slowScale)` | `([slowScale=0.25]) -> boolean` | Toggles between `slowScale` and the last non-bullet-time dilation. |
| `Director.ToggleFreeze()` | `() -> boolean` | Toggles between `0.0001` and the last non-frozen dilation. |

### 3.8.3 `Record` / `Watched`: session telemetry ring buffer

A 5000-entry ring buffer of `{t, kind, text}` records. `Record` appends,
`Watched` tests a name against substring patterns, `SetWatchPatterns` sets
the patterns, `GetRecords` returns the buffer.

| Global | Signature | Behaviour |
| :--- | :--- | :--- |
| `Record` | `(kind, [text]) -> nil` | Appends `{t = GetTickCount64()/1000.0, kind, text}` to the ring buffer. When the buffer hits 5000, the oldest entry is dropped. |
| `Watched` | `(name) -> string\|nil` | Returns the first pattern that is a substring of `name`, or `nil`. |
| `SetWatchPatterns` | `(table) -> nil` | Replaces the watch list with the strings in `table` (1-indexed). |
| `GetRecords` | `() -> table` | Array of `{t, kind, text}` for every entry currently in the buffer. |

### 3.9 `File` table

| Member | Behaviour |
| :--- | :--- |
| `File.Read(path)` | whole file (string) or `nil` |
| `File.Write(path, data)` | overwrite → boolean |
| `File.Append(path, data)` | append → boolean |
| `File.Exists(path)` | boolean |
| `File.List(path)` | directory entries |
| `File.CreateDir(path)` / `File.MakeDir(path)` | create directory → boolean |
| `File.Delete(path)` | delete file → boolean |

### 3.10 `JSON` table

| Member | Behaviour |
| :--- | :--- |
| `JSON.Parse(str)` / `JSON.decode(str)` | decode to Lua value (or `nil`) |
| `JSON.Stringify(value)` / `JSON.encode(value)` | encode (ordered keys) |

### 3.11 `Memory` table

Canonical PascalCase readers/writers. Readers are `(base [, offset])`; writers are
`(base, value [, offset])`. `base` accepts a light-userdata address, an integer, or a
property wrapper.

| Readers | Writers |
| :--- | :--- |
| `ReadInt8` / `ReadByte` | `WriteInt8` / `WriteByte` |
| `ReadInt16` | `WriteInt16` |
| `ReadInt32` | `WriteInt32` |
| `ReadInt64` | `WriteInt64` |
| `ReadUInt8` | `WriteUInt8` |
| `ReadUInt16` | `WriteUInt16` |
| `ReadUInt32` | `WriteUInt32` |
| `ReadUInt64` | `WriteUInt64` |
| `ReadFloat` | `WriteFloat` |
| `ReadDouble` | `WriteDouble` |
| `ReadPointer` | `WritePointer` |
| `ReadBool(base[, offset[, mask=1]])` | `WriteBool(base, value[, offset[, mask=1]])` |
| `ReadString(base[, offset[, maxLen=256]])` (cap 4096) | N/A |
| `ReadFString` | N/A |
| `ReadFName` | N/A |
| `IsValidPtr` | N/A |

Snake_case spellings exist on **both** `Memory` and `ue`:

```
read_i8 read_i16 read_i32 read_i64 read_u8 read_u16 read_u32 read_u64
read_float read_double read_ptr read_bool read_str read_fstring read_fname is_valid_ptr
write_i8 write_i16 write_i32 write_i64 write_u8 write_u16 write_u32 write_u64
write_float write_double write_ptr write_bool
```

### 3.12 `Math` table

```lua
-- Math table members:
-- Math.Lerp(a, b, t)
-- Math.Clamp(x, min, max)
-- Math.VInterpTo(current, target, deltaTime, interpSpeed)
-- Math.RInterpTo(current, target, deltaTime, interpSpeed)
-- Math.FindLookAtRotation(startVec, targetVec)
```

### 3.13 The `ue` table (snake_case)

| Category | Members |
| :--- | :--- |
| Accessors | `get_player get_player_controller get_camera_manager get_local_player get_game_instance get_game_mode get_game_state get_all_actors get_distance get_world get_engine get_object_count get_object get_world_actors` |
| Object queries | `find_object find_objects find_class get_name get_full_name get_class get_super_class inherits_from get_properties call` |
| Info | `get_version` (`"UE4.27"` / `"UE5.3"`), `get_game_name`, `get_globals` |
| Kismet | `get_gameplay_statics get_kismet_system_library get_kismet_math_library get_kismet_string_library get_kismet_array_library` |
| Timing/GC | `get_frame_time` + `GetFrameTime`, `get_fps` + `GetFPS`, `get_delta_seconds` + `GetDeltaSeconds`, `get_memory_usage` + `GetMemoryUsage`, `force_garbage_collection` / `ForceGarbageCollection` / `collect_garbage` / `CollectGarbage` |
| Memory | all snake_case names from §3.11 |

`ue.get_version` and `ue.get_game_name` are the **only** spellings of engine version and game
name; they are table members, not top-level globals.

`ue.get_globals()` returns a key-value table of discovered engine root pointers:
`guobjectarray`, `gnames`, `gworld`, `gengine`, `gmalloc`, `gnatives`. Each non-null
pointer is wrapped as an opaque userdata object. To inspect available roots, iterate with
`pairs(ue.get_globals())`.

---

## 4. The UObject wrapper

Any `UObject*` pushed to Lua is userdata with a metatable whose resolution order is:
(1) UObject surface members (§4.2) and the named members below; (2) reflected property lookup
(superclass-walking); (3) reflected function lookup (`K2_` fallback); (4) injected custom
properties (§11.10); else an invalid-object sink.

> **Invalid-object sink & method lookups:** Because missing members resolve to an
> `Avalonium.InvalidObject` userdata rather than `nil`, chained lookups (e.g. `obj.A.B`)
> never throw nil-dereference errors. Calling an invalid object or an unresolved method safely
> returns `nil` (`__call` sink). To check whether a reflected property or function actually exists
> on an object, test `:IsValid()` on the resolved member (e.g. `if obj.Method and obj.Method:IsValid() then`),
> as `obj.Method ~= nil` will be true even for non-existent members. I treat `:IsValid()` as
> the only real check: nil-checks are useless for reflection, and the sink is truthy even when
> the member is garbage.

Metamethods: `__index`, `__newindex`, `__tostring` (`[UObject: Class] Name (0x…)`), `__eq`,
`__len` (data-table row count, else 0), and `__call` (only UFunction objects are callable;
calling a UFunction without a bound context requires the context object as first argument).

### 4.1 Named members

Identity / lifetime:

```
GetAddress  GetAddressString  GetName  GetFullName  GetPathName  GetFName
GetClass    GetOuter  GetPackage  IsA  IsValid  get  Get  Cast  SpawnActor
```

Reflection:

```
HasAllFlags  HasAnyFlags  HasAnyInternalFlags  IsClass  IsAnyClass  Reflection
GetPropertyByName  SetPropertyByName
GetProperty | GetPropertyValue        SetProperty | SetPropertyValue
ExportProperty  ImportProperty
GetProperties  FindProperty  GetFunctions  HasFunction
Call | CallFunction  GetComponent  GetComponents  GetComponentsByClass
GetCDO | GetClassDefaultObject  GetSuperClass | GetSuper  GetChildClasses  IsChildOf
GetWorld  DumpProperties  DumpFunctions
```

Actors / transforms:

```
GetLocation | K2_GetActorLocation          SetLocation | SetActorLocation | K2_SetActorLocation
GetRotation | K2_GetActorRotation          SetRotation | SetActorRotation | K2_SetActorRotation
GetScale | GetActorScale3D | K2_GetActorScale3D   SetScale | SetActorScale3D | K2_SetActorScale3D
GetTransform | GetActorTransform | K2_GetActorTransform
SetTransform | SetActorTransform | K2_SetActorTransform
Teleport | TeleportTo | K2_TeleportTo      GetVelocity  GetDistanceTo
GetForwardVector | GetActorForwardVector   GetRightVector | GetActorRightVector
GetUpVector | GetActorUpVector
AddActorWorldOffset  AddActorLocalOffset   AddActorWorldRotation  AddActorLocalRotation
GetRootComponent  AttachToActor  AttachToComponent  DetachFromActor  GetAttachedActors
GetActorBounds | GetBounds
DestroyActor | K2_DestroyActor  SetActorHiddenInGame  IsHidden  SetActorEnableCollision
```

DataTable / UEnum / UStruct (see §4.3):

```
GetRowNames  FindRow  ForEachRow  GetRowMap  GetAllRows  GetRowStruct  AddRow  RemoveRow  EmptyTable
GetNameByValue  ForEachName  GetEnumNameByIndex
GetFunctionFlags  SetFunctionFlags  ForEachProperty  ForEachFunction  GetLevel
EditNameAt  EditValueAt  InsertIntoNames  RemoveFromNamesAt
```

### 4.2 UObject surface members (`LuaCompatSurface.cpp`)

```
type   GetSuperStruct   GetOutermost   IsMappedToObject   IsMappedToProperty   ProcessConsoleExec
```

`type()` returns `"UObject"`. `ProcessConsoleExec(cmd)` invokes a same-named UFunction on the
object, else hands the command to the engine console; returns boolean.

### 4.3 Container member tables

Row values are pushed as tables of reflected fields. Enum mutation is best-effort.

| Object | Members |
| :--- | :--- |
| UDataTable | `GetRowNames`, `FindRow` (or `Get`), `ForEachRow`, `GetRowMap`, `GetAllRows`, `GetRowStruct`, `AddRow`, `RemoveRow`, `EmptyTable`, `__len` |
| UEnum | `GetNameByValue(v)`, `ForEachName(fn)`, `GetEnumNameByIndex(i)`, `EditNameAt(i,name)`, `EditValueAt(i,value)`, `InsertIntoNames(name,value)`, `RemoveFromNamesAt(i)` |

### 4.4 Calling reflected functions

* `obj:SomeFunction(args…)`: resolved by short name, then `K2_SomeFunction`, dispatched via
  ProcessEvent with parameter marshalling.
* `obj:Call(name, args…)` / `obj:CallFunction(name, args…)`.
* `lib:Function(args)` on Kismet CDOs.
* Return values (including structs/containers) are marshalled back.

---

## 5. Value types

Constructors are **globals**; instances are userdata with live `__index`/`__newindex`.
`Vector`/`Rotator`/`Transform`/`FColor` aliases exist.

| Global (aliases) | Members |
| :--- | :--- |
| `FVector` (`Vector`) | `X Y Z` (aliases `Pitch Yaw Roll`), `Length`, `Size`, `Length2D`, `Size2D`, `LengthSquared`, `SizeSquared`, `Distance`, `Normalize`, `Dot`, `Cross`, `ToRotator`, `Rotation`, `ToTable`, `Clone`; operators `+ - * /`, unary `-`, `==` |
| `FRotator` (`Rotator`) | `Pitch Yaw Roll`, `Vector`, `GetForwardVector`/`ForwardVector`, `GetRightVector`/`RightVector`, `GetUpVector`/`UpVector`, `Normalize`, `Clamp`, `ToTable`, `Clone`; `+ -` |
| `FTransform` (`Transform`) | `Translation`/`Location`, `Rotation`, `Scale3D`/`Scale`, `TransformPosition`, `TransformLocation`, `ToTable`, `Clone` |
| `FLinearColor` (`FColor`) | `R G B A`, `ToHex`/`Hex`, `ToTable`, `Clone`; `+ - *` |
| `FQuat` | `X Y Z W`, `ToRotator`/`Rotator`, `type` |
| `FVector2D` | `X Y`, `Length`/`Size`, `type` |
| `FVector4` | `X Y Z W`, `type` |
| `FSoftObjectPath` (`SoftObjectPath`, `TSoftObjectPtr`) | `GetAssetPath`, `GetAssetPathName`, `ToString`, `GetSubPathString`, `GetSubPathName`, `GetObjectID`, `type`, `Get`, `ResolveObject`, `Load`, `LoadSynchronous`, `IsValid`, `IsNull`, `IsEmpty`, `GetWeakPtr`, `GetWeakObjectPtr`, `GetTagAtLastTest`, `AssetPathName`, `AssetPath` |
| `FDelegate` (`Delegate`, `MulticastDelegate`, `FMulticastScriptDelegate`) | `Add`, `Bind`, `Remove`, `Unbind`, `Clear`, `RemoveAll`, `Broadcast`, `ProcessDelegate`, `Num`, `Count`, `IsBound`, `GetBindings`, `Bindings`, `IsValid`; `__call`, `__len` |

`Math` is documented in §3.12. Vectors/rotators/colors/transforms also push as tables and the
engine getters accept either form.

---

## 6. Containers (`LuaTypes.cpp`)

Containers are **live views** over engine storage. `TMap`/`TSet` refuse mutation
(`TMap_RefuseMutation` / `TSet_RefuseMutation`): their `Add`/`Remove` are present but refused,
because engine hashing must not be bypassed. We don't clear the backing array; that doesn't
empty the container, it corrupts it. The mutation refusal is the feature, not a missing method.

### `TArray`

Numeric key `[i]` (1-based) returns an element; `__len` and `Num`/`Count`/`GetArrayNum`
report the count. Members:

```
ForEach  Num  Count  GetArrayNum  IsValid  Empty
type  GetArrayAddress  GetArrayDataAddress  GetArrayMax
```

`ForEach(fn)` calls `fn(index, elementView)`; returning `false` stops the loop. Elements are
property views (usable with `:get()`/`:set()`).

### `TMap`

```
ForEach  Num  Count  Find  Contains  ContainsKey  Add  Remove  Empty  type  IsValid
```

`Find(key)`/`Contains(key)`/`ContainsKey(key)`; `ForEach(fn)` calls `fn(key, value)`.

### `TSet`

```
ForEach  Num  Count  Contains  Add  Remove  Empty  type  IsValid
```

### Delegates

Live multicast/single delegates can be `Add`/`Bind`ed with Lua functions, `Broadcast`ed
(also `__call`), queried (`IsBound`, `GetBindings`/`Bindings`), and cleared
(`Remove`, `RemoveAll`, `Clear`, `Unbind`). `__len` = binding count.

---

## 7. Strings

Engine string values are native Lua strings with the string namespace extended. Members work
as both `("abc"):Len()` and `string.Len("abc")`:

```
ToString  Len  Length  IsEmpty  Find  StartsWith  EndsWith  ToUpper  ToLower
Append  Concat  Clear  Empty  IsValid  Clone  GetFName  GetName  GetFullName  type
```

`Clear`/`Empty` answer the empty string (Lua strings are values, not buffers).

---

## 8. Hooks, events, keybinds, timers (`LuaHookManager.cpp`)

### 8.1 Function hooks

```lua
local preId, postId = RegisterHook("FunctionName", pre, post)   -- Hook alias
UnregisterHook(preId)
-- Also accepted: UnregisterHook("Name") or UnregisterHook("Name", preId, postId)
-- Unhook(...) is an alias
```

* `RegisterHook` returns the pair `(preId, postId)`; `postId` is `0` when no post callback.
* Callback convention: `pre(self, Param1, …)` before the engine call, `post(self, …)` after.
  Parameters are **live property views** (reads fetch engine storage, writes write back;
  out-params included). A pre-hook returning truthy cancels the engine call.
* The engine return value in a post hook is exposed in the global `__OriginalReturnValue`.
* Installed with two inline hooks: on `ProcessEvent` (native/Blueprint dispatch) and on
  `ProcessLocalScriptFunction` (interpreted bytecode). A hook for a name the process does not
  yet hold is stored and applied when the name appears. We leave a typo'd name in the pending
  list forever with one log line; nothing retries the spelling, and the list is patient, not
  clever.

Related:

```
RegisterCustomEvent(name, pre[, post]) | HookCustomEvent
UnregisterCustomEvent(name[, preId][, postId]) | UnhookCustomEvent
IsKeyBindRegistered(key)
NotifyOnNewObject(className, callback) | OnNewObject
RegisterConsoleCommandHandler(name, cb) | RegisterConsoleCommand | RegisterCommand
RegisterConsoleCommandGlobalHandler([name], cb)
ExecuteConsoleCommand(cmd) | ExecuteCommand
```

### 8.2 Lifecycle hooks

```
RegisterBeginPlayHook   | OnBeginPlay
RegisterEndPlayHook     | OnEndPlay
RegisterLoadMapHook     | OnLoadMap
RegisterInitGameStateHook | OnInitGameState
```

Registered `(preCallback[, postCallback])`, returning a numeric id; the callback receives the
relevant object as `self`.

Compat pre/post variants (from `LuaCompatParity.cpp`):

```
RegisterBeginPlayPreHook/PostHook   RegisterEndPlayPreHook/PostHook
RegisterLoadMapPreHook/PostHook     RegisterInitGameStatePreHook/PostHook
RegisterProcessConsoleExecPreHook/PostHook
RegisterCallFunctionByNameWithArgumentsPreHook/PostHook
RegisterULocalPlayerExecPreHook/PostHook
```

### 8.3 Keybinds

```lua
RegisterKeyBind("F9", callback)
RegisterKeyBind(Key.F9, callback)
RegisterKeyBind("S", {ModifierKey.CONTROL}, callback)
RegisterKeyBind({Key.S, ModifierKey.SHIFT}, callback)   -- first value = key
-- Aliases: RegisterKeybind, BindKey, RegisterKeyBindAsync
```

Names resolve case/space/underscore/dash-insensitively: `f1..f24`, `a..z`, `0..9`, `space`,
`enter`/`return`, `tab`, `escape`/`esc`, `backspace`, `shift`/`ctrl`/`control`/`alt`,
arrows (`up`/`down`/`left`/`right`), `insert`/`del`, `home`/`end`, `pageup`/`pgup`,
`pagedown`/`pgdn`, `capslock`, `numlock`, `scrolllock`, `pause`, `printscreen`, numpad
(`numpadadd`, `numpadsubtract`, `numpadmultiply`, `numpaddivide`, `numpaddecimal`, `numpadN`),
OEM punctuation (`minus`/`dash`, `plus`/`equals`, `bracketleft`, `bracketright`, `semicolon`,
`quote`/`apostrophe`, `comma`, `period`/`dot`, `slash`, `backslash`, `tilde`), and mouse
(`lbutton`/`mouse1`/`leftclick`, `rbutton`/`mouse2`/`rightclick`, `mbutton`/`mouse3`, `xbutton1`/
`mouse4`, `xbutton2`/`mouse5`). Numeric arguments are virtual-key codes, normalized to the
same canonical name.

Key state: `IsKeyDown(key)` / `IsKeyPressed` / `IsKeyHeld` (via `GetAsyncKeyState`).
Input dispatch is asynchronous, so `RegisterKeyBindAsync` is a transparent alias.

### 8.4 Timers & delayed actions

| Global (aliases) | Behaviour |
| :--- | :--- |
| `ExecuteInGameThread(fn)` (`RunOnGameThread`, `NextTick`) | enqueue for next game-thread pump |
| `ExecuteWithDelay(delayMs, fn)` / `ExecuteWithDelay(handle, delayMs, fn)` (`SetTimeout`) | one-shot; returns/uses a handle |
| `ExecuteInGameThreadWithDelay(handle, delayMs, fn)` | delay-style, caller handle |
| `RetriggerableExecuteInGameThreadWithDelay(handle, delayMs, fn)` | retriggerable |
| `ExecuteInGameThreadAfterFrames(frames, fn)` / `…AfterFrames_DelayStyle` | frame-based one-shot |
| `LoopAsync(intervalMs, fn)` (`SetInterval`, `SetTimer`) | repeating |
| `LoopInGameThreadAfterFrames(frames, fn)` | repeating frame-based |
| `LoopInGameThreadWithDelay(ms, fn)` | repeating delay-style |
| `MakeActionHandle()` | reserve a unique handle |
| `CancelTimer(handle)` (`ClearTimer`, `CancelDelayedAction`) | cancel |
| `PauseDelayedAction` / `UnpauseDelayedAction` | pause / resume |
| `ResetDelayedActionTimer` / `SetDelayedActionTimer(handle, ms)` | reset / change delay |
| `IsValidDelayedActionHandle` / `IsDelayedActionActive` / `IsDelayedActionPaused` | queries |
| `GetDelayedActionTimeRemaining` / `GetDelayedActionTimeElapsed` / `GetDelayedActionRate` | timing |
| `ClearAllDelayedActions()` | cancel everything (also run by mod reload) |
| `ExecuteAsync(fn)` | run once on the async pump thread |

Timers are driven by a dedicated timer thread that enqueues callbacks onto the game-thread
queue (`ProcessTimers` / `ProcessGameThreadTasks`).

### 8.5 Hook parameter proxies

Hook callbacks (and out-parm lists) receive parameter-proxy objects whose `__index`/`__newindex`
resolve:

```
Get  get  Set  set  Cancel  IsCancelled  GetSelf  GetFunction  GetReturnValue  SetReturnValue  Dump
```

### 8.6 Utility generation

```
GenerateLuaTypes([dir])           DumpAllObjects([path])
ForEachUObject(callback)          -- callback(UObject, chunkIndex, indexWithinChunk)
ForEachUObjectFlat(callback)      -- callback(UObject)
```

---

## 9. Threading model

| Thread | Runs |
| :--- | :--- |
| Game thread | hook callbacks, keybind callbacks, timers (drained from the game-thread pump), `ExecuteInGameThread` |
| Timer thread | OS scheduling of delays/loops (callbacks still land on the game thread) |
| Init/async thread | mod-loading pass, `ExecuteAsync` bodies |
| Caller thread | `LuaManager::ExecuteString`/`ExecuteFile` under the Lua mutex (IPC) |

One `lua_State`, one recursive mutex. During a mod-loading pass `m_LoadingMods` is set before
the mutex is taken, so an engine thread arriving mid-load answers immediately instead of
stalling. `ForEachUObject*` and large scans use thread-local buffers. We use the mutex to
prevent corruption, not bad manners: a mod callback that blocks the game thread still stalls
the whole game, the lock won't save the frame rate.

---

## 10. Compatibility surface: batch 1 & parity

### 10.1 Version tables

`UnrealVersion` (detected engine): `Major`, `Minor`, `GetMajor()`, `GetMinor()`, `IsEqual`,
`IsAtLeast`, `IsAtMost`, `IsBelow`, `IsAbove` (all `(major, minor)` comparisons; accept method
form too).

`RuntimeVersion`:

```lua
RuntimeVersion = { Major = 3, Minor = 0, Hotfix = 1,
                   GetVersion = function() return 3, 0, 1 end,
                   CompatMajor = 3, CompatMinor = 5 }
```

`Avalonium` and `UE4SS` are aliases of the same table (published by `RegisterVersionSurface`).

### 10.2 Thread introspection

```
GetCurrentThreadId  GetMainModThreadId  GetGameThreadId  GetAsyncThreadId
IsInGameThread  IsInMainModThread  IsInAsyncThread
EngineTickAvailable  ProcessEventAvailable       -- both true
EGameThreadMethod = { EngineTick = 0, ProcessEvent = 1 }
MainThread  HookThread  AsyncThread              -- real Lua threads
__OriginalReturnValue                            -- global slot, init nil
```

`ThreadId` is a constructor for a thread-id handle; `ThreadIdUserdata`/`threadid` is the type
record. Handle members: `ToString()`, `get()`, `GetId()`, `type()` → `"ThreadIdUserdata"`,
`IsValid()`; also `__eq`, `__tostring`.

### 10.3 Names, text, strings

**`FName`**: final `FName` is the surface constructor (`Surf_FName`):
`FName("Name")` (default flag `FNAME_Add`), `FName("Name", FNAME_Find)` (pure lookup),
`FName(index, _, number)`. Members: `ToString()`, `get()`, `GetComparisonIndex()`,
`GetNumber()`, `Equals(x)`, `type()` → `"FName"`, `IsValid()`; `__tostring`
(`Name` or `Name_<n>`), `__eq`, `__concat`. `FNameUserdata`/`fname` are the type records.
`NAME_None` is a handle for index 0.

**`FText`**: `FText(text)` with `ToString()`, `get()`, `type()` → `"FText"`. Text properties
read from engine memory push a reference wrapper resolving the live payload.

**`FString` / `FUtf8String` / `FAnsiString`**: constructors returning Lua strings.

**`FPackageName`**: `IsShortPackageName(name)`, `IsValidLongPackageName(name)`.

### 10.4 Constant tables

**`EFindName`** = `{ FNAME_Find = 0, FNAME_Add = 1, FNAME_Replace_Not_Safe_For_Threading = 2 }`.

**`Key`**: every Windows virtual-key name → code (mouse, media, browser, IME, OEM, `F1..F24`,
letters, digits, numpad, arrows, etc.), plus aliases `LEFT_CLICK`, `RIGHT_CLICK`.

**`ModifierKey`** = `SHIFT CONTROL ALT LEFT_SHIFT RIGHT_SHIFT LEFT_CONTROL RIGHT_CONTROL
LEFT_ALT RIGHT_ALT` (all mapped to the generic `VK_SHIFT`/`VK_CONTROL`/`VK_MENU`).

**`EObjectFlags`** (base bits, version-aware additions):

```
RF_NoFlags RF_Public RF_Standalone RF_MarkAsNative RF_Transactional
RF_ClassDefaultObject RF_ArchetypeObject RF_Transient RF_MarkAsRootSet
RF_TagGarbageTemp RF_NeedInitialization RF_NeedLoad RF_NeedPostLoad
RF_NeedPostLoadSubobjects RF_NewerVersionExists RF_BeginDestroyed RF_FinishDestroyed
RF_BeingRegenerated RF_DefaultSubObject RF_WasLoaded RF_TextExportTransient
RF_LoadCompleted RF_InheritableComponentTemplate RF_DuplicateTransient
RF_StrongRefOnFrame RF_NonPIEDuplicateTransient RF_WillBeLoaded RF_HasExternalPackage
```
Plus `RF_KeepForCooker` (0x800) always; `RF_GeneratePublicHash` (5.6+);
`RF_ImmutableDefaultObject` (5.6+) / `RF_Dynamic` (pre-5.6) at 0x4000000;
`RF_MirroredGarbage` + `RF_AllocatedInSharedPage` (UE5); `RF_MigratingAsset` +
`RF_HasDynamicImports` (5.8+); `RF_AllFlags` = `0x1FFFFFFF` pre-UE5 / `0xFFFFFFFF` on UE5.

**`EInternalObjectFlags`**:

```
ReachableInCluster (1<<23)  ClusterRoot (1<<24)  Native (1<<25)  Async (1<<26)
Unreachable (1<<28)  RootSet (1<<30)
Garbage / PendingKill (1<<29)
```
UE5 adds `AsyncLoadingPhase1 (1<<22)` and `AsyncLoadingPhase2 (1<<27)`; both families carry
`AsyncLoading (1<<27)`. Also `GarbageCollectionKeepFlags = (1<<25)|(1<<26)|(1<<27)` and
`AllFlags = 0x7F800000`.

**`PropertyTypes`**: a registry keyed by type name (and index). Each record has `Name`,
`Size`, a lazily-resolved `FFieldClassPointer` (class-like identity; `0` until observed), and
`StaticPointer` (always `0`). Sizes:

```
ObjectProperty 8, ObjectPtrProperty 8, Int8Property 1, Int16Property 2, IntProperty 4,
Int64Property 8, ByteProperty 1, UInt16Property 2, UInt32Property 4, UInt64Property 8,
NameProperty 8, FloatProperty 4, BoolProperty 1, ArrayProperty 16, SetProperty 16,
MapProperty 32, StructProperty 0, ClassProperty 8, WeakObjectProperty 8, EnumProperty 1,
TextProperty 8, StrProperty 16, Utf8StrProperty 16, AnsiStrProperty 16, InterfaceProperty 8,
DelegateProperty 32, MulticastDelegateProperty 16, MulticastInlineDelegateProperty 16,
MulticastSparseDelegateProperty 8, SoftObjectProperty 16, SoftClassProperty 16,
DoubleProperty 8
```

### 10.5 Flag helpers on UObject

`HasAllFlags(mask)`, `HasAnyFlags(mask)` (external flags), `HasAnyInternalFlags(mask)`
(approximated from the object's own flags where a mirror exists; else false), `IsClass`,
`GetPropertyByName`, `SetPropertyByName`, `Reflection`.

`Reflection` returns a table keyed by property name (`{ Type, Name }`) plus a
`GetProperty(name)` that returns an `FProperty` wrapper.

### 10.6 `FProperty` wrapper

Members:

```
GetFName  GetName  GetFullName  GetClass  GetStruct  GetInner  GetEnum
GetPropertyClass  GetInterfaceClass
GetOffset_Internal | GetOffset  GetArrayDim  GetSize | GetElementSize
GetFieldSize  GetByteOffset  GetByteMask  GetFieldMask  ContainerPtrToValuePtr
GetValue | get   SetValue | set   ImportText   IsA  IsValid  GetOwner  type
```

Plain metadata keys (same handle, both call styles):

```
Type/TypeName  Name  Offset_Internal/Offset  ElementSize/Size  ArrayDim  PropertyFlags
IsOutParam  IsArrayLike  HasValue  PropertyAddress  ByteOffset  ByteMask  FieldMask  FieldSize
```

Instance-bound struct wrappers expose the struct's own members directly for read and write.

### 10.7 Value wrappers from parity

`FQuat`, `FVector2D`, `FVector4` (see §5); `FWeakObjectPtr` / `WeakObjectPtr`, callable
`Get()`, `IsValid()`, `type`, `__tostring` (`FWeakObjectPtr(0x…) valid/stale`); constructor
accepts a UObject, raw pointer, another weak handle, or `{Object=…}`.

### 10.8 Parity globals

```
CreateInvalidObject            -- neutral handle (see §10.9)
LoadExport(name) | loadexport  -- shared/exports/<n>.lua, shared/<n>.lua, exports/<n>.lua, <n>.lua
RestartMod(name|nil) | RestartCurrentMod
UninstallMod(name|nil) | UninstallCurrentMod
IterateGameDirectories()       -- game directory tree (see §10.10)
CreateLogicModsDirectory()
RegisterCustomProperty(spec)   -- see §10.11
RegisterConsoleCommandGlobalHandler([name], cb)
GenerateSDK  GenerateUHTCompatibleHeaders  DumpUSMAP  DumpJMAP
DumpAllActors  DumpStaticMeshes
```

### 10.9 Invalid object

`CreateInvalidObject()` returns a handle whose members resolve and whose reads return neutral
values. Unresolved method calls and direct invocations (`__call`) safely return `nil`.
Members: `IsValid` (returns `false`), `IsShown`, `get`/`Get`, `GetAddress`, `GetAddressString`,
`GetFullName`, `GetPathName`, `GetName`, `GetFName`, `GetClass`, `GetOuter`, `GetWorld`,
`GetOutermost`, `type`.

### 10.10 Directory tree

`IterateGameDirectories()` returns a tree of directory tables. Each node has `__name`,
`__absolute_path`, a `__dirs` array of child tables, and named child fields; child-directory
fields are the plain-named tables. A node's `__files` (lazily built, cached) is an array of
file entries, each with `__name` and `__absolute_path`. The project directory answers to
`"Game"` as well as its own name.

### 10.11 Custom properties

`RegisterCustomProperty{ Name, Type (string or {Name=…}), BelongsToClass (object or class
name), OffsetInternal, ArrayDim, PropertyFlags }` injects a synthetic property resolved after
reflected lookup fails, readable/writable through the UObject index handler.

### 10.12 Output device

Command handlers may be handed an `FOutputDevice` handle. Members: `Log(...)`, `Warn(...)`,
`IsValid()`, `type()` → `"FOutputDevice"`, `__tostring` → `"[FOutputDevice]"`; unknown keys
answer a nil-returning no-op.

---

## 11. Mod handles & shared state

### 11.1 Mod reference

`ModRef` (and `GetModRef`, which is the same value after registration) is a **callable**
handle. `ModRef("Name")` returns that mod's handle table carrying `SetSharedVariable` /
`GetSharedVariable`; `ModRef()` returns a fresh handle for the caller. `Mod` is the callable
mod-reference function carrying a metatable whose `__index` resolves:

```
SetSharedVariable  GetSharedVariable  OnUnload  type   -- type() -> "ModRef"
```

Any other key falls through to the callable itself, so `local m = ModRef; m.Name = v`
patterns keep working. `Mod.OnUnload = function() … end` stores a cleanup callback
attributed to the calling mod (fallback `__main__`) in `_ModUnloadCallbacks`; the runtime
does not currently invoke it on reload/uninstall.

### 11.2 Shared variables

`SetSharedVariable(name, value)` / `GetSharedVariable(name)` back onto the game-wide
`_SharedVariables` table, readable by every mod. (Both the `Mod` metatable methods and the
`ModRef(name)` handle table install these; the argument order shifts by one for the metatable
method form.)

### 11.3 Internal globals (present, not for general use)

```
_SharedVariables      _ModUnloadCallbacks     _ModRegistry
__SharedModuleRoot    __OriginalReturnValue   __SharedHelpersMerged
```

`__SharedModuleRoot` is `<mods>/shared` (forward-slashed).

---

## 12. Built-in mods

Registered unconditionally by `LuaManager::Initialize`.

**UniversalHUD** (`lua_register`):

```
ToggleHUD  HideAllHUD  ShowAllHUD  IsHUDHidden  RefreshHUDCache
```

**UniversalOverlay** (globals):

```
ToggleMetrics | ToggleMetricsHUD      ShowMetrics | ShowMetricsHUD
HideMetrics | HideMetricsHUD          IsMetricsVisible | IsMetricsHUDVisible
Notify(msg[, type[, duration]])       Toast(...)
```

---

## 13. The `Overlay` table (`Runtime/overlay/LuaOverlay.cpp`)

```
Notify  Toast  ClearNotifications
SetStat  RemoveStat  ClearStats  ShowStats  IsStatsVisible  SetStatsAnchor  SetStatsPosition
DrawText  DrawRect  DrawLine  DrawCircle  DrawProgressBar  RegisterDrawCallback
RegisterWindow
Text  TextColored  Button  Checkbox  SliderFloat  SliderInt  Separator  SameLine
InputText  InputFloat  InputInt  Combo  Selectable  RadioButton
CollapsingHeader  TreeNode  TreePop  BeginChild  EndChild
Indent  Unindent  Spacing  Dummy
Tooltip  IsItemHovered  IsItemClicked  ProgressBar
BeginTable  EndTable  TableNextRow  TableNextColumn  TableSetupColumn
PushStyleColor  PopStyleColor  PushStyleVar  PopStyleVar
IsOpen  Open  Close  Toggle  SetMenuKey
```

---

## 14. Complete A → Z index (globals & top-level values)

```
Avalonium  AsyncThread  BindKey  CallFunction  CancelDelayedAction  CancelTimer
ClearAllDelayedActions  ClearTimer  CollectGarbage  ConstructObject  CreateInvalidObject
CreateLogicModsDirectory  Delegate  DerefToInt32  dereftoint32  DumpAllActors
DumpAllObjects  DumpJMAP  DumpStaticMeshes  DumpUSMAP  DuplicateObject  EFindName
EGameThreadMethod  EInternalObjectFlags  EObjectFlags  EngineTickAvailable  ExecuteAsync
ExecuteCommand  ExecuteConsoleCommand  ExecuteInGameThread  ExecuteInGameThreadAfterFrames
ExecuteInGameThreadAfterFrames_DelayStyle  ExecuteInGameThreadWithDelay  ExecuteWithDelay
FAnsiString  FColor  FDelegate  FLinearColor  FMulticastScriptDelegate  FName  FNameUserdata
FQuat  FRotator  FSoftObjectPath  FString  FText  FTransform  FUtf8String  FVector  FVector2D
FVector4  FWeakObjectPtr  File  FindAllOf  FindCVars  FindClass  FindFirstOf  FindObject  FindObjects
ForEachUObject  ForEachUObjectFlat  ForceGarbageCollection  FPackageName
GenerateLuaTypes  GenerateSDK  GenerateUHTCompatibleHeaders  GetAllActors
GetAllActorsOfClass  GetAsyncThreadId  GetBlueprintPipeline  GetCVar  GetCameraManager
GetClassProperties  GetCurrentThreadId  GetDelayedActionRate  GetDelayedActionTimeElapsed
GetDelayedActionTimeRemaining  GetDeltaSeconds  GetDistance  GetEngine  GetFPS
GetFirstPlayerController  GetFrameTime  GetGameInstance  GetGameMode  GetGameState
GetGameThreadId  GetGameplayStatics  GetKismetArrayLibrary  GetKismetGuidLibrary
GetKismetMathLibrary  GetKismetRenderingLibrary  GetKismetStringLibrary
GetKismetSystemLibrary  GetKismetTextLibrary  GetLocalPlayer  GetMainModThreadId
GetMemoryUsage  GetModDirectory  GetModRef  GetModsDirectory  GetObjectByIndex
GetObjectClass  GetObjectCount  GetObjectFullName  GetObjectName  GetObjectShape
GetPlayer  GetPlayerCameraManager  GetPlayerCharacter  GetPlayerController  GetPlayerPawn
GetSuperClass  GetWorld  GetWorldActors  HideAllHUD  HideMetrics  HideMetricsHUD  Hook
HookCustomEvent  HookThread  HotReloadMods  InheritsFrom  IsDelayedActionActive
IsDelayedActionPaused  IsHUDHidden  IsInAsyncThread  IsInGameThread  IsInMainModThread
IsKeyBindRegistered  IsKeyDown  IsKeyHeld  IsKeyPressed  IsMetricsHUDVisible
IsMetricsVisible  IsValidDelayedActionHandle  IterateGameDirectories  JSON  Key  LoadAsset
LoadExport  loadexport  log  Log  LoopAsync  LoopInGameThreadAfterFrames
LoopInGameThreadWithDelay  MainThread  MakeActionHandle  Math  Memory  Mod  ModifierKey
ModRef  MulticastDelegate  NAME_None  NewObject  NextTick  Notify  NotifyOnNewObject  OnBeginPlay  OnEndPlay
OnInitGameState  OnLoadMap  OnNewObject  Overlay  PauseDelayedAction  print  Print
ProcessEventAvailable  PropertyTypes  RefreshHUDCache  RegisterBeginPlayHook
RegisterBeginPlayPostHook  RegisterBeginPlayPreHook  RegisterCallFunctionByNameWithArgumentsPreHook
RegisterCallFunctionByNameWithArgumentsPostHook  RegisterCommand  RegisterConsoleCommand
RegisterConsoleCommandGlobalHandler  RegisterConsoleCommandHandler  RegisterCustomEvent
RegisterCustomProperty  RegisterEndPlayHook  RegisterEndPlayPostHook  RegisterEndPlayPreHook
RegisterHook  RegisterInitGameStateHook  RegisterInitGameStatePostHook
RegisterInitGameStatePreHook  RegisterKeyBind  RegisterKeyBindAsync  RegisterKeybind
RegisterLoadMapHook  RegisterLoadMapPostHook  RegisterLoadMapPreHook
RegisterProcessConsoleExecPreHook  RegisterProcessConsoleExecPostHook
RegisterULocalPlayerExecPreHook  RegisterULocalPlayerExecPostHook  ReloadMods
ResetDelayedActionTimer  RestartCurrentMod  RestartMod  RetriggerableExecuteInGameThreadWithDelay
Rotator  RunOnGameThread  RuntimeVersion  SetCVar  SetDelayedActionTimer  SetInterval
SetTimeout  SetTimer  ShowAllHUD  ShowMetrics  ShowMetricsHUD  SoftObjectPath  SpawnActor
StaticClass  StaticConstructObject  StaticEnum  StaticFindObject  StaticLoadObject
StaticStruct  TSoftObjectPtr  ThreadId  ThreadIdUserdata  threadid  Toast  ToggleHUD
ToggleMetrics  ToggleMetricsHUD  Transform  UE4SS  UObjectGlobals  Unhook  UnhookCustomEvent
UninstallCurrentMod  UninstallMod  UnpauseDelayedAction  UnrealVersion  UnregisterCustomEvent
UnregisterHook  Vector  WeakObjectPtr  fname  ue
```

### 14.1 Complete UObject member index

```
AddActorLocalOffset  AddActorLocalRotation  AddActorWorldOffset  AddActorWorldRotation
AddRow  AttachToActor  AttachToComponent  Call  CallFunction  Cast
DestroyActor  DetachFromActor  DumpFunctions  DumpProperties  EditNameAt  EditValueAt
EmptyTable  ExportAsJSON  FindProperty  FindRow  ForEachFunction  ForEachName  ForEachProperty  ForEachRow
Get  GetActorBounds  GetActorForwardVector  GetActorRightVector  GetActorScale3D
GetActorTransform  GetActorUpVector  GetAllRows  GetAddress  GetAddressString  GetAttachedActors
GetBounds  GetCDO  GetChildClasses  GetClass  GetClassDefaultObject  GetComponent
GetComponents  GetComponentsByClass  GetDistanceTo  GetEnumNameByIndex  GetFName
GetForwardVector  GetFullName  GetFunctionFlags  GetFunctions  GetLevel  GetLocation
GetName  GetOutermost  GetOuter  GetPackage  GetPathName  GetProperties  GetProperty
GetPropertyByName  GetPropertyValue  GetRightVector  GetRootComponent  GetRotation  GetRowMap
GetRowNames  GetRowStruct  GetScale  GetSuper  GetSuperClass  GetSuperStruct  GetTransform
GetUpVector  GetVelocity  GetWorld  HasAllFlags  HasAnyFlags  HasAnyInternalFlags
HasFunction  InsertIntoNames  IsA  IsAnyClass  IsChildOf  IsClass  IsHidden
IsMappedToObject  IsMappedToProperty  K2_DestroyActor  K2_GetActorLocation
K2_GetActorRotation  K2_GetActorScale3D  K2_GetActorTransform  K2_SetActorLocation
K2_SetActorRotation  K2_SetActorScale3D  K2_SetActorTransform  K2_TeleportTo
ProcessConsoleExec  Reflection  RemoveFromNamesAt  RemoveRow  SetActorEnableCollision
SetActorHiddenInGame  SetActorLocation  SetActorRotation  SetActorScale3D  SetActorTransform
SetFunctionFlags  SetLocation  SetProperty  SetPropertyByName  SetPropertyValue  SetRotation
SetScale  SetTransform  SpawnActor  Teleport  TeleportTo  get  set  type
```

---

## 15. Authoring notes

* Keep entry work short; do engine-touching work inside `ExecuteInGameThread`, a hook, or a
  timer callback (where engine per-thread state exists).
* Guard risky reads with `Memory.IsValidPtr`.
* Feature-detect with `RuntimeVersion.CompatMinor` / `CompatMajor`.
* `Mod.OnUnload` (or `ModRef.OnUnload`) is stored but not invoked by the runtime; undo work
  explicitly in your own reload path.
* Return truthy from a pre-hook only to cancel the engine call.
* Globals are per-mod; use shared variables for cross-mod state.
