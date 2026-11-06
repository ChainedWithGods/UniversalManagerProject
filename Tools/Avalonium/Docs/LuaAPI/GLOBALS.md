# LuaAPI: Globals Reference

Every global registered by `LuaManager` and what it does. Hook/timer globals are in
[HOOKS.md](HOOKS.md); compatibility globals in [COMPAT.md](COMPAT.md); the `Overlay` table in
[OverlayAPI](../OverlayAPI/README.md).

---

## 1. Logging

| Name | Signature | Behavior |
| --- | --- | --- |
| `print` | `print(...)` | Concatenates arguments (tab-separated, Lua formatting), prefixes `[Lua] `, appends `\n`, writes to the Avalonium log **and** `OutputDebugStringA`. Returns nothing. |
| `log` | `log(...)` | Same as `print` (alias registered separately; identically implemented). |

Non-string values print as their Lua representation: integers/numbers plain, booleans
`true`/`false`, nil `nil`, light userdata `0x…`, other types by Lua type name. `print` every frame and Microsoft's `OutputDebugStringA` plus the log get hammered on every
tick. There's no rate limit hiding in there, that's a performance bug wearing a debug hat.

---

## 2. Player & world accessors

| Global | Signature | Returns |
| --- | --- | --- |
| `GetPlayer` | `() -> UObject|nil` | Local player pawn (falls back through controller → pawn/character; skips `Default__` CDOs) |
| `GetPlayerPawn` | `()` | Alias of `GetPlayer` |
| `GetPlayerCharacter` | `()` | Alias of `GetPlayer` |
| `GetPlayerController` | `() -> UObject|nil` | Local `PlayerController` |
| `GetFirstPlayerController` | `()` | Alias |
| `GetPlayerCameraManager` | `() -> UObject|nil` | Camera manager |
| `GetCameraManager` | `()` | Alias |
| `GetLocalPlayer` | `() -> UObject|nil` | `ULocalPlayer` |
| `GetGameInstance` | `() -> UObject|nil` | Game instance |
| `GetGameMode` | `() -> UObject|nil` | `AGameModeBase` |
| `GetGameState` | `() -> UObject|nil` | `AGameStateBase` |
| `GetWorld` | `() -> UObject|nil` | Live world, validated against the discovered world class |
| `GetEngine` | `() -> UObject|nil` | `GEngine` (index search fallback) |

`GetPlayerController`/camera/etc. resolve via the console subsystem's verified finders, not
name guessing. We filter `Default__` CDOs for a reason: a CDO is a template, not a live player. Write to one
and the game happily ignores it while the mod author wonders why nothing happened.

---

## 3. Actor & object queries

| Global | Signature | Notes |
| --- | --- | --- |
| `GetAllActorsOfClass` | `(className|class, [maxCount]) -> table` | World actors matching the class (name accepted with or without `_C`). `"Actor"`/`"AActor"` = all actors. |
| `GetAllActors` | `(…)` | Alias of `GetAllActorsOfClass` |
| `GetWorldActors` | `() -> table` | All actors in the live world (up to 10 000 buffered) |
| `GetDistance` | `(a, b) -> number` | Distance between two UObjects (uses `K2_GetActorLocation`; UE5 double vectors handled) or two vector values |
| `FindFirstOf` | `(className) -> UObject|nil` | Special-cases `PlayerController`/`Character`/`Pawn`, then world actors (subclass-name match, 32 levels), then live object index, then full object-array scan |
| `FindAllOf` | `(className) -> table` | World actors matching class (generic actors when class unknown); falls back to live-object index when the world has none |
| `FindObject` | see below | Compatibility overloads (registered last, supersedes the simple finder) |
| `FindObjects` | see below | Compatibility overloads |
| `StaticFindObject` | `(path) -> UObject|nil` or `(class, outer, name, [exact]) -> UObject|nil` | Path form resolves `"/Script/Engine.Default__GameplayStatics"`-style paths |
| `StaticConstructObject` | `(class, [outer]) -> UObject|nil` | Spawn through the engine, SEH-construction fallback |
| `GetObjectCount` | `() -> integer` | `GUObjectArray` count |
| `GetObjectByIndex` | `(i) -> UObject|nil` | Index into the object array |
| `GetObjectName` | `(obj) -> string` | Short FName |
| `GetObjectFullName` | `(obj) -> string` | Full path name |
| `GetObjectClass` | `(obj) -> UClass|nil` | Class of any object |
| `GetSuperClass` | `(struct) -> UStruct|nil` | Super struct |
| `InheritsFrom` | `(objOrClass, baseName) -> boolean` | Walks up to 40 super levels |
| `GetClassProperties` | `(objOrClass) -> table` | Array of `{name, type, offset, size, is_array, is_struct, is_enum, is_map, is_bool, class_name}` (max 512) |
| `CallFunction` | `(obj, funcName, ...) -> results|boolean` | Resolves the UFunction and calls through ProcessEvent (`LuaEngine::CallUFunctionDynamic`) |
| `ExportProperty` | `(obj, propName) -> value` | Read a single reflected property by name |
| `ImportProperty` | `(obj, propName, value) -> boolean` | Write a single reflected property by name |

`FindFirstOf` falls through special cases, world actors, the live object index and finally a
full object-array scan. We keep that finder chain ordered for a reason. "Simplify" it and the full object-array scan
lands on every call, congratulations, the fast path is gone.

Raw engine pointers are available only as `ue.get_globals()`; there is no top-level
`GetGlobals` global.

### 3.1 `FindObject` (compatibility overloads, final registration)

Both call shapes are recognized by argument type, not position:

```
FindObject(objectName, [excludeFlags], [includeFlags])            -- name form
FindObject(classObj|className, outerObj, objectName, [exactClass]) -- class/outer form
```

* Names may be plain text or an FName handle.
* Class may be a class object or a class name (case-insensitive).
* `exactClass` false (default) accepts subclass instances (`IsA` semantics).
* A path-shaped name (`/Script/Engine.Actor`) additionally constrains the full object path.
* Only `PendingKill`-like and root-set approximations of the internal flag masks are
  supported (`1<<29` ≈ pending kill, `1<<30` root set, `1<<25` native); unknown bits
  degrade to "any non-zero flags".
* Returns the first match or `nil`.

### 3.2 `FindObjects(maxCount, objectName, [mustHaveFlags], [mustNotFlags]) -> table`

`maxCount` optional (0/nil = default limit). Returns an array of matches. Name matches
accept full-name suffix matches (`…Package.Object`) as well as short names.

### 3.3 Simple finders that remain available

`FindClass(name) -> UClass|nil` (cached class lookup, `_C` not required) and the
`UObjectGlobals` table:

```lua
UObjectGlobals = { FindFirstOf, FindAllOf, StaticFindObject, FindObjects, FindClass }
```

---

## 4. Camera, screen & trace helpers

| Global | Signature | Returns | Notes |
| --- | --- | --- | --- |
| `GetCameraInfo` | `() -> {Location, Rotation, FOV}` | Camera transform + FOV | Uses `PlayerCameraManager->GetCameraCachePOV`; `Rotation` has `Pitch`/`Yaw`/`Roll` and `GetForwardVector()` |
| `GetMousePosition` | `() -> x, y` | Screen-space mouse coords | From `PlayerController->GetMousePosition` |
| `ProjectWorldToScreen` | `(worldPos, [bPlayerViewportRelative]) -> x, y, ok` | Screen projection | Uses `PlayerController->ProjectWorldLocationToScreen` |
| `DeprojectScreenToWorld` | `(screenX, screenY) -> worldPos, worldDir, ok` | Ray from screen | Uses `PlayerController->DeprojectScreenPositionToWorld` |
| `GetViewportSize` | `() -> width, height` | Active viewport resolution | From ImGui `DisplaySize` (matches the swapchain) |

## 5. Actor queries & manipulation

| Global | Signature | Returns | Notes |
| --- | --- | --- | --- |
| `GetActorBounds` | `(actor) -> origin, extent` | Bounding-box origin + extent | `UKismetSystemLibrary::GetActorBounds` |
| `GetActorLocation` | `(actor) -> FVector\|nil` | Actor world location | `K2_GetActorLocation` |
| `SetActorLocation` | `(actor, location, [bSweep], [bTeleport]) -> bool` | Teleport success | `K2_SetActorLocation`; defaults `bSweep=false`, `bTeleport=true` (instant) |
| `LineTraceSingle` | `(start, end, [traceChannel], [bTraceComplex], [actorsToIgnore], [drawDebugType], [bIgnoreSelf]) -> hitResult, success` | First blocking hit | `UKismetSystemLibrary::LineTraceSingle`; default channel `0` (`TraceTypeQuery1` / Visibility); `bIgnoreSelf` defaults `true` |

`hitResult` is a reflection struct with fields like `Location`, `ImpactPoint`, `Normal`, `ImpactNormal`, `Distance`, `Time`, `bBlockingHit`, `Actor` (weak ptr wrapper), `Component`, `BoneName`, `FaceIndex`, `TraceStart`, `TraceEnd`.

---

## 6. Convenience helpers

| Global | Signature | Returns | Notes |
| --- | --- | --- | --- |
| `SpawnActorByClass` | `(classNameOrClass, [location], [rotation], [bDeferConstruction]) -> UObject|nil` | The spawned actor | Convenience wrapper around `UWorld::SpawnActor` / `UGameplayStatics::SpawnActor`; accepts a class name (with or without `_C`) or a class object. `location` is `{X,Y,Z}` (default 0,0,0), `rotation` is `{Pitch,Yaw,Roll}` (default 0,0,0). Returns `nil` and logs a reason when the spawn fails (no live world, class not found, engine call refused). |
| `GetAllPawns` | `() -> table` | Array of pawn UObjects | Iterates the live world's `PawnIterator` and any `APlayerController->GetPawn` instances; falls back to the live object index when the iterator is empty. |
| `WithWorld` | `(callback) -> nil` | N/A | Schedules `callback(liveWorld)` to run on the engine game thread. The callback is invoked asynchronously through the hook manager's game-thread queue; it may be called on a later frame, never synchronously. Use it from timer callbacks, async hooks, or any context where the calling thread does not have a live world. |
| `WhenClassSpawns` | `(className, callback, [intervalMs]) -> timerId` | Timer handle (integer) | Polls for an instance of `className` every `intervalMs` milliseconds (default 200). As soon as one is found the callback is called with the actor and the polling timer is auto-cancelled. The handle can be passed to `CancelTimer` from [HOOKS.md](HOOKS.md) to abort early. |

### 6.1 `Director` table: time dilation, bullet time, freeze

The `Director` table is a global convenience surface for the most common
world-time manipulations. It writes through the live world's
`WorldSettings->TimeDilation` and tracks the prior value so it can be
restored on demand.

| Member | Signature | Behavior |
| --- | --- | --- |
| `Director.Set` | `(scale) -> boolean` | `true` if the world accepted the write | Sets the world time dilation to `scale` without clamping; `0` is a full freeze. |
| `Director.Get` | `() -> number` | Current `TimeDilation` | `1.0` when no live world is present. |
| `Director.SlowFor` | `(ms, [scale=0.15]) -> boolean` | `true` if the schedule was accepted | Sets dilation to `scale` for `ms` milliseconds, then restores the previous value. The restore is scheduled through the hook manager's delay timer. |
| `Director.ToggleBulletTime` | `([slowScale=0.25]) -> boolean` | `true` on a successful state change | Toggles between `slowScale` and the last non-bullet-time dilation. |
| `Director.ToggleFreeze` | `() -> boolean` | `true` on a successful state change | Toggles between `0.0001` and the last non-frozen dilation. |

All `Director` members are no-ops when no live world is present; `Get` reports
`1.0` and the setters return `false`.

### 6.2 `Record` / `Watched`: lightweight session telemetry

A small in-memory ring buffer (5000 entries). `Record` appends a tagged
entry; `Watched` tests a name against substring patterns; `GetRecords`
returns the buffer.

| Global | Signature | Behavior |
| --- | --- | --- |
| `Record` | `(kind, [text]) -> nil` | Appends `{t = GetTickCount64()/1000.0, kind, text}` to the ring buffer. When the buffer hits 5000, the oldest entry is dropped. |
| `Watched` | `(name) -> string\|nil` | Returns the first pattern in the watch list that is a substring of `name`, or `nil` if no pattern matches. |
| `SetWatchPatterns` | `(table) -> nil` | Replaces the watch list with the strings in `table` (1-indexed). |
| `GetRecords` | `() -> table` | Array of `{t, kind, text}` for every entry currently in the buffer. |

---

## 7. Kismet libraries

| Global | Returns |
| --- | --- |
| `GetGameplayStatics` | `UGameplayStatics` CDO |
| `GetKismetSystemLibrary` | `UKismetSystemLibrary` CDO |
| `GetKismetMathLibrary` | `UKismetMathLibrary` CDO |
| `GetKismetStringLibrary` | `UKismetStringLibrary` CDO |
| `GetKismetArrayLibrary` | `UKismetArrayLibrary` CDO |
| `GetKismetTextLibrary` | `UKismetTextLibrary` CDO |
| `GetKismetGuidLibrary` | `UKismetGuidLibrary` CDO |
| `GetKismetRenderingLibrary` | `UKismetRenderingLibrary` CDO |

The returned objects support normal function calls, e.g.
`GetKismetMathLibrary():Abs(-3)`.

---

## 8. Timing, memory, GC

| Global | Returns |
| --- | --- |
| `GetFrameTime` | Frame time in ms (ImGui delta when available, else hook-manager timing, fallback 16.666) |
| `GetFPS` | Smoothed FPS (ImGui when available, else hook manager) |
| `GetDeltaSeconds` | Frame delta in seconds |
| `GetMemoryUsage` | Process memory usage |
| `ForceGarbageCollection` / `CollectGarbage` | Force a GC pass |

---

## 9. CVars

| Global | Signature | Behavior |
| --- | --- | --- |
| `GetCVar` | `(name) -> integer|nil` | Reads via `IConsoleManager` TMap walk; tries multiple vtable slots; falls back to `1` for a registered flag CVar |
| `SetCVar` | `(name, value) -> boolean` | Executes `<name> <value>` through the console |
| `FindCVars` | `() -> table` | First 10 000 registered CVar names |

`IConsoleManager` and its TMap offset are discovered at startup
(`IConsoleManager_TMapOff`, defaults 0x08; stride 0x20 on UE5, 0x18 on UE4).

---

## 10. `File` table

| Member | Signature | Behavior |
| --- | --- | --- |
| `Read` | `(path) -> string|nil` | Whole file |
| `Write` | `(path, data) -> boolean` | Overwrite |
| `Append` | `(path, data) -> boolean` | Append |
| `Exists` | `(path) -> boolean` | N/A |
| `List` | `(path) -> table` | Directory entries |
| `CreateDir` / `MakeDir` | `(path) -> boolean` | Create directory (both names) |
| `Delete` | `(path) -> boolean` | Delete file |

Paths are forwarded to the Windows filesystem; relative paths resolve against the game's
Relative paths resolve against the game's working directory, not the mod folder. I build every
path from `GetModsDirectory()` because that mistake bites every mod author exactly once.

## 11. `JSON` table

| Member | Signature | Behavior |
| --- | --- | --- |
| `Parse` / `decode` | `(str) -> value|nil` | Decode |
| `Stringify` / `encode` | `(value) -> string` | Encode (ordered keys, Lua-table aware) |

## 12. Mod management

| Global | Signature | Behavior |
| --- | --- | --- |
| `GetModDirectory` / `GetModsDirectory` | `() -> string` | Active mods root |
| `ReloadMods` / `HotReloadMods` | `()` | Full reload: clears reflected hooks and timers, re-scans the folder (same as Alt+PageDown). Console-exec lifecycle callbacks and custom-event registrations are **not** cleared |
| `GetLoadedMods` | N/A | C++ accessor only (reported by IPC `LUA STATUS`), not a Lua global |

## 13. `Memory` table (and `ue` snake_case equivalents)

PascalCase (canonical) on `Memory`; snake_case spellings exist on **both** `Memory` and `ue`.

| Read | Write | Type |
| --- | --- | --- |
| `ReadInt8` / `ReadByte` | `WriteInt8` / `WriteByte` | int8 |
| `ReadInt16` | `WriteInt16` | int16 |
| `ReadInt32` | `WriteInt32` | int32 |
| `ReadInt64` | `WriteInt64` | int64 |
| `ReadUInt8` | `WriteUInt8` | uint8 |
| `ReadUInt16` | `WriteUInt16` | uint16 |
| `ReadUInt32` | `WriteUInt32` | uint32 |
| `ReadUInt64` | `WriteUInt64` | uint64 |
| `ReadFloat` | `WriteFloat` | float |
| `ReadDouble` | `WriteDouble` | double |
| `ReadPointer` | `WritePointer` | pointer |
| `ReadBool` | `WriteBool` | bool (byte) |
| `ReadString` | N/A | NUL-terminated ANSI, optional length |
| `ReadFString` | N/A | UE `FString` (UTF-16/ANSI aware) |
| `ReadFName` | N/A | FName resolved through the name table |
| `IsValidPtr` | N/A | Safe pointer/Mapped check |

Snake_case aliases:

```
read_i8 read_i16 read_i32 read_i64
read_u8 read_u16 read_u32 read_u64
read_float read_double read_ptr read_bool
read_str read_fstring read_fname is_valid_ptr
write_i8 write_i16 write_i32 write_i64
write_u8 write_u16 write_u32 write_u64
write_float write_double write_ptr write_bool
```

Argument convention: `(address_or_object, value…)`. Addresses accept light userdata,
FProperty wrappers or numbers. `IsValidPtr` uses the safe-memory oracle. We treat `IsValidPtr` as a probe, not a lock. The engine can free the object between the check
and the read; call it a guarantee and it's a race with a countdown.

---

## 14. `ue` table

Registered in two passes: `RegisterUETable` (snake_case) and `RegisterMemoryBindings`
(adds the memory aliases plus PascalCase timing/GC names).

| Category | Members |
| --- | --- |
| Accessors | `get_player`, `get_player_controller`, `get_camera_manager`, `get_local_player`, `get_game_instance`, `get_game_mode`, `get_game_state`, `get_all_actors`, `get_distance`, `get_world`, `get_engine`, `get_object_count`, `get_object`, `get_world_actors` |
| Object queries | `find_object`, `find_objects`, `find_class`, `get_name`, `get_full_name`, `get_class`, `get_super_class`, `inherits_from`, `get_properties`, `call` |
| Runtime info | `get_version` (`"UE4.27"` / `"UE5.3"`), `get_game_name`, `get_globals` |
| Kismet | `get_gameplay_statics`, `get_kismet_system_library`, `get_kismet_math_library`, `get_kismet_string_library`, `get_kismet_array_library` |
| Timing/GC | `get_frame_time` + `GetFrameTime`, `get_fps` + `GetFPS`, `get_delta_seconds` + `GetDeltaSeconds`, `get_memory_usage` + `GetMemoryUsage`, `force_garbage_collection` / `ForceGarbageCollection` / `collect_garbage` / `CollectGarbage` |
| Memory | all snake_case names from §10 |

---

## Full global index

Non-hook, non-compat globals (hook names in [HOOKS.md](HOOKS.md), compat names in
[COMPAT.md](COMPAT.md)):

```
print log
GetPlayer GetPlayerPawn GetPlayerCharacter GetPlayerController GetFirstPlayerController
GetPlayerCameraManager GetCameraManager GetLocalPlayer GetGameInstance GetGameMode
GetGameState GetPlayerState GetWorld GetEngine
GetComponentByClass GetAllComponents
GetCameraInfo GetMousePosition ProjectWorldToScreen DeprojectScreenToWorld GetViewportSize
GetActorBounds GetActorLocation SetActorLocation LineTraceSingle
GetAllActorsOfClass GetAllActors GetWorldActors GetDistance
FindFirstOf FindAllOf FindObject FindObjects FindClass StaticFindObject StaticConstructObject
StaticClass StaticStruct StaticEnum LoadAsset StaticLoadObject SpawnActor NewObject
ConstructObject DuplicateObject GetModRef
GetObjectCount GetObjectByIndex GetObjectName GetObjectFullName GetObjectClass GetSuperClass
InheritsFrom GetClassProperties CallFunction
GetGameplayStatics GetKismetSystemLibrary GetKismetMathLibrary GetKismetStringLibrary
GetKismetArrayLibrary GetKismetTextLibrary GetKismetGuidLibrary GetKismetRenderingLibrary
GetFrameTime GetFPS GetDeltaSeconds GetMemoryUsage ForceGarbageCollection CollectGarbage
GetCVar SetCVar FindCVars
GetModDirectory GetModsDirectory ReloadMods HotReloadMods
SpawnActorByClass GetAllPawns WithWorld WhenClassSpawns
Director Record Watched SetWatchPatterns GetRecords
File JSON Memory ue UObjectGlobals
```

`ue.get_version` and `ue.get_game_name` are the only spellings of engine version and game
name; they are table members, not top-level globals. Mod enumeration lives on the compat
surface (`Mod` table).
