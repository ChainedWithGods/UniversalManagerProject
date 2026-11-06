# ConsoleAPI: Reference

Headers: `Runtime/console/ConsoleInternal.h`,
`Runtime/console/SeamlessTravelGuard.h`.
Namespaces: `ConsoleEnabler`, `runtimeinput`, `SeamlessTravelGuard`; free
function `GetConsoleDumpDirectory`.

---

## 1. Public API (`namespace ConsoleEnabler`)

### `bool EnableInGameConsole(const EngineState& state)`

Creates or finds the `UConsole` object, attaches it to
`GameViewport::ViewportConsole`, sets `ConsoleTargetPlayer`, and remaps
`ConsoleKeys`. It also installs the seamless travel guard once (section 3).
`CheatManager` attachment happens lazily when the console opens
(or during enable only when the WndProc hook is disabled).

Returns `false` when: state is not initialized; `UnrealEngineConsole=false`; no
live GameEngine/GameViewport; no `Console` class; no `ViewportConsole` offset;
console creation fails; or the created object fails validation (wrong class or
outer). `UnlockConsoleCommands` runs before the console-class/viewport checks,
so a later failure can still have unlocked cheat bits and written console dumps.
I don't roll back on `false`; it's not a rollback.

On success caches the object in `g_CachedConsoleObject` and returns `true`.

### `void DisableInGameConsole(const EngineState& state)`

Detaches Avalonium's console from `GameViewport::ViewportConsole` and restores
the console state, without touching `InputSettings.ConsoleKeys` (see the
implementation comment: a guessed `FKey` layout can corrupt the engine's
allocator). Safe to call repeatedly. I write nothing to `InputSettings` on the exit path,
because a wrong `FKey` layout corrupts the allocator just as easily on the way out as on the
way in.

### `bool TryOpenConsole(const EngineState& state)`

Opens the cached console through `TryOpenConsoleInternal`:
`FakeGotoState("Typing")` → validate → `FakeGotoState("Open")` → validate.
Returns `false` when the console is disabled, not creatable, or when
`FakeGotoState` was stripped (the caller then forwards Tilde/F10 to the
engine's WndProc).

### `void InstallWndProcHook(EngineState& state)`

Subclasses the game window (`SetWindowLongPtr(GWLP_WNDPROC)`) and stores the
original in `state.OriginalWndProc`. No-op when already installed. The hook
(`GameWndProc`):

* dispatches `WM_KEYDOWN`/`WM_SYSKEYDOWN` to Lua keybinds when Lua mods are on;
* handles `Alt+PageDown` → `LuaManager::HotReloadMods()`, swallowing the key;
* handles console keys `VK_OEM_3` (Tilde), `VK_F10`, `VK_OEM_8`, scan code 41:
  tries `TryOpenConsole`, otherwise forwards to the original WndProc (so the
  engine's own console, if present, can open) and swallows the key when the
  console is disabled;
* forwards everything else to the original WndProc.

### `void RemoveWndProcHook(EngineState& state)`

Restores `OriginalWndProc`, clears it, logs.

### `void Prewarm(const EngineState& state)`

Cheap warm-up executed before the first enable attempt: resolves the construct
helper so later property lookups hit warm caches. It does not touch
`Default__InputSettings`; the `InputSettings` CDO is resolved or created by
`RemapConsoleKeys`.

---

## 2. `namespace runtimeinput`

```cpp
bool IsConsoleEnabled();            // RuntimeConfig.EnableUnrealConsole
bool ShouldReleaseMouseCapture();   // console state is Typing/Open
bool TryOpenConsole();              // wrapped ConsoleEnabler::TryOpenConsole(g_State)
```

---

## 3. Seamless travel safety (`SeamlessTravelGuard.h`)

Engine commands can replace the local player's controller. The debug camera is
the loud one: the original PlayerController keeps its `PlayerState`, loses its
`Player`, and stays in the world. `APlayerController::ShouldParticipateInSeamlessTravel`
returns true for it, so travel carries it into the next world and games that
spawn a pawn assuming `Player != nullptr` crash. This module removes that state
before the engine decides what to keep.

### `bool Install(EngineState& state)`

Called by `EnableInGameConsole` once, inside the CONSOLE ENABLER log section,
and again by `ReattachCheatManagerIfNeeded` on every console open so classes
loaded after startup are covered. Idempotent: a slot that no longer holds the
stock stub is left alone.

Discovery reads the live class layout, no fixed indices:

* `Player`, `PlayerState`, `PlayerController` and `World::AuthorityGameMode`
  come from reflection.
* Participation slot: scan the `Controller` and `PlayerController` CDO vtables
  for the pair "reads `PlayerState` at its reflected offset and returns
  `setne al`" (the base, `cmp` or `mov`/`test` form) and "`mov al,1; ret`" (the
  override). The virtual itself exists from UE5.1 (`Controller.h`); 4.23
  through 5.0 have no participation filter to patch, so `Install` logs that
  and leaves the game alone instead of reporting a discovery failure.
* `SwitchController` slot: scan the `LocalPlayer` CDO vtable for the function
  that reads `UPlayer::PlayerController` and clears the old controller's
  `Player`.

Patching walks every class deriving from `PlayerController` and writes the hook
only where the slot still points at the stock constant-true stub. A game that
overrides the virtual keeps its own logic.

At travel the hook:

1. Reads the controller's world from its Outer chain, `Controller -> Level ->
   World`, then that world's GameMode and `PlayerControllerClass`. The GWorld
   slot is not the reference here, it can point at a world that fails validation
   mid-transition.
2. If the controller lost its `Player` but still has a `PlayerState`, is an
   instance of `PlayerControllerClass`, and the single local player sits on some
   other controller, it switches the player back through
   `UPlayer::SwitchController`, the engine's own switch, not a hand-rolled
   pointer write.
3. If the controller holds the player but is not an instance of
   `PlayerControllerClass`, it looks for the one detached controller of that
   class and switches the player to it.
4. Returns `Player != nullptr`, so the drained substitute is not carried into
   the next world. The engine then replaces the real controller normally and the
   real `PlayerState` rides along.

I keep the repair conservative: exactly one local player and exactly one
detached controller, otherwise the guard stays out of the way. Detached
controllers come from `GetWorldActors`, the same validated enumeration the rest
of the runtime uses. If a class, offset or slot cannot be identified, `Install`
logs once and leaves the game untouched.

Log lines use the `[TravelGuard]` tag and are attributed to LEVEL TRANSITION.

---

## 4. Dump directory

### `void GetConsoleDumpDirectory(const EngineState& state, char* outBuffer, size_t bufferSize)`

Computes `<logdir>\<GameName>\Console\` (separators sanitized in the game
name), creates it, and writes a NUL-terminated path. No-op when `outBuffer` is
null or `bufferSize == 0`.

---

## 5. Dumpers (`ConsoleDumpers.cpp`)

### `void DumpLoadedModules(const EngineState& state)`

Requires `DumpAllDeveloperCommands`. Runs as one of the three dump worker
threads, enumerates modules (`EnumProcessModules`), skips `.exe`, copies each
DLL into
`<Console>\LoadedDLLs\` with `overwrite_existing`, but only when source and
destination last-write times differ (`IsModuleBackupCurrent`). Logs
`[Dumper] Found N loaded modules...`, `Copied X, skipped Y unchanged module(s).`
We timestamp-check it so it doesn't copy the same DLLs every run like an idiot.

### `void DumpExecCommands(const EngineState& state)`

Requires `DumpAllDeveloperCommands`. Walks the object array for `UFUNCTION(Exec)`
entries (`OnProcessExecObject`), groups them by declaring class, and writes:

```text
=== Avalonium - Exec Commands ===
Game: <GameName>
Engine: <major>.<minor>
Exec functions: <n> in <classes> classes

Exec functions are UFUNCTION(Exec) declarations recovered from reflection, ...
Class: <Class> (<ParentClass>, <n> commands)
  - <ReturnType if any> <Name>(<ParamType> <ParamName>, ...)  [<FunctionFlags>]
```

Every command prints its C++ signature instead of a stripped type word:

* Parameter types come from the property itself: `FString`, `int32`, `bool`,
  `FVector` (through `FStructProperty::Struct`), enums (`FEnumProperty` /
  `FByteProperty`), `TSubclassOf<AActor>`, object/weak/soft pointers, and
  `TArray` / `TSet` / `TMap` / `TOptional` with their real element types.
  Member prefixes follow the engine convention: `F` for structs, `A`/`U` for
  class references (actor-derived classes get `A`).
* `out`, `const` and `&` come from `CPF_OutParm`, `CPF_ConstParm` and
  `CPF_ReferenceParm`; the return type comes from the `CPF_ReturnParm` property.
* The bracket list is the function's own `EFunctionFlags` (`Native`,
  `BlueprintCallable`, `BlueprintPure`, `Static`, `Const`, `Net`, `Reliable`,
  `Server`, `Client`, `Multicast`, `AuthorityOnly`, `Cosmetic`, `EditorOnly`).
  `Exec` itself is not repeated on every line.
* Class lines name the declaring class, its direct parent from
  `UStruct::SuperStruct` (omitted for `Object`), and the command count. Classes
  and commands are sorted; duplicate signatures are removed.

file: `<Console>\ExecCommands.txt`. The header states the routing rule: a
declared function runs while an object of its class is part of the local
player's exec chain (player input, controller, pawn, HUD, cheat manager, game
mode, game state, camera manager, game instance, viewport) or the engine's
global exec handlers. When zero functions are found, a file left by an earlier
run is removed instead of being left behind as if it were current.

### `void DumpSpawnableClasses(const EngineState& state)`

Requires `DumpAllDeveloperCommands`. Classifies every loaded class as
actor/component/widget/other (`SpawnableDumpContext`) and writes
`<Console>\SpawnableClasses.txt` with the game name and each bucket's entries
(short + full names).

### `void UnlockConsoleCommands(const EngineState& state)`

The main console unlock pass, called by `EnableInGameConsole`:

* Walks `IConsoleManager`'s command hash map (validating array header/storage
  through `SafeMem`; max 200 000 entries, stride discovered), reads each
  command's name string and its console object's `GetHelp` (vtable slot 1,
  SEH-guarded).
* Clears `ECVF_Cheat` bits so developer commands become available. Help and
  flags are resolved from the engine layout, not fixed slots: UE5.8 makes help
  text virtual (`UE_WITH_CVAR_HELP_TEXT`), so `GetFlags`/`SetFlags` sit at 4/5;
  a 5.8 build that compiles help text out puts them at 1/2; pre-5.8 uses 3/4.
  Slot 1 is probed (a help string is readable, a flags enum is not), and
  `SetFlags` takes the new flag value. Only `ECVF_Cheat` is cleared.
* When `DumpAllDeveloperCommands` is on, writes:
  * `<Console>\AllConsoleCommands.json`: game, `totalCommands`, and a
    `commands` array with name + help text;
  * `<Console>\UnlockedCommands.txt`: names and count of unlocked commands.
* Runs the fast-memory scope (`SafeMem_SetFastMode(1)`) for speed and restores
  it at the end.
* Reads the CVar values through `DumpConsoleVariables` **before** clearing any
  cheat bits, so the reference still shows which variables were cheats.
* Finally runs `DumpExecCommands`, `DumpSpawnableClasses` and
  `DumpLoadedModules` on three worker threads and **joins all three**
  (`LaunchDumpsAsync`), so the unlock call blocks until the dumps finish.

Logs `[ConsoleEnabler] Processed IConsoleManager. Unlocked N ECVF_Cheat
commands (M elements threw exceptions!).`

### `void DumpConsoleVariables(const EngineState& state, void* dataPtr, uint32_t* allocationFlags, int32_t arrayNum, int32_t stride)` (`ConsoleVariables.cpp`)

Requires `DumpAllDeveloperCommands`. Walks the same `IConsoleManager` map once
and collects, for every console object, its name, variable/command kind, type,
flags, current value and the engine's help text; then sorts by name and writes
two files:

* `<Console>\CVARs.ini`: sorted `name=value` lines holding the current value of
  every console variable. A console command runs code instead of holding a
  value, so commands are a commented block at the end of the file and each
  line carries the command's help text.
* `<Console>\CVarReference.txt`: every object as
  `name | type | flags | value | help`, with the commands and their help text
  in their own section. Type is `bool`/`int`/`float`/`string`; a string value
  is quoted so an empty one is visible. Flags carry the engine's console
  variable flags (`Cheat`, `ReadOnly`, `Scalability`, ...) plus the source that
  set the value (`SetBy:ConsoleVariablesIni`, `SetBy:DeviceProfile`,
  `SetBy:Code`, ...).

`IConsoleObject::AsVariable` separates variables from commands (the base
returns null, every `FConsoleVariableBase` returns itself), and values come
from the engine's own typed getters: `GetBool`, `GetInt`, `GetFloat`, and
`GetString` for string or custom variables. Their slots follow the
`IConsoleVariable` declaration order in `HAL/IConsoleManager.h`, counted from
the probed `SetFlags` slot: 4.23 has no bool variable type, 4.24 to 5.3 share
one layout, and 5.4 (`GetParentObject`), 5.5 (`GetDetailedHelp`), 5.6
(shadow/deprecated methods), 5.7 (`Unset`) and 5.8 (help text made virtual,
either layout) each add virtuals. Getters are never found by probing slots,
because a wrong slot can call `Set`. A raised read is logged with the variable
name and skipped; eight faults stop the pass, so one broken object cannot cost
the rest of the file. An unknown engine version leaves the files without
values.

---

## 6. Internal helper reference (`ConsoleInternal.h`)

These are available to all console sources and follow the module's
"validate, probe, log" style, I don't do blind pointer arithmetic or "it's probably
fine" bullshit. Grouped by purpose:

### Object/class lookup

| Function | Semantics |
| :--- | :--- |
| `TrySEHConstruct(state, cls, outer, name, setFlags)` | `StaticConstructObject_Internal` through the params-frame or direct-call ABI under SEH |
| `TrySEHConstruct(fn, params)` | raw constructor call under SEH |
| `StartsWith`, `EqualsInsensitiveAscii`, `ContainsInsensitiveAscii` | string helpers |
| `IsDefaultObjectName`, `IsClassObject`, `IsObjectA` | shape tests (`Default__` prefix, class object, derives-from) |
| `FindOwningClassObject` | class that owns an object |
| `FindClassByNameExact`, `FindObjectByNameExact` | exact-name lookups |
| `FindFirstObjectOfClass(state, base, wantDefaultObject)` | first instance/CDO of a class |
| `FindExistingConsoleObject` | console already owned by a viewport |
| `IsUsableConsoleObject` | class + readability validation |
| `ReadPtrAt(base, offset)` | pointer read |
| `GetPropertyOwner`, `GetUPropertyTypeName`, `GuessArrayInnerProperty` | property inspection |
| `FindPropertyWithLayout`, `FindPropertyOffset`, `FindPropertyInfo` | property resolution with FProperty/UProperty fallback; `PropertyLookupResult` carries `Offset`, `TypeName`, `InnerTypeName`, `StructTypeName` |
| `FindClassByNameExact` etc. | name lookups used by all flows |

### Diagnostics

`LogPropertyPreview`, `LogFunctionPreview`, `LogFunctionKeywordMatches`,
`LogConsoleClassDiagnostics`: log class contents/keyword matches for a class
when a lookup fails; guarded by one-shot flags
(`g_LoggedViewportConsoleFailureDetails`, `g_LoggedConsoleStateFailureDetails`,
`g_LoggedFakeGotoStateFailureDetails`, `g_LoggedOpenWithoutStateValidation`).

### Console state machine

| Function | Semantics |
| :--- | :--- |
| `EnsureFakeGotoStateResolved` | one-shot resolution of the reflected state-driver function (`g_FakeGotoStateFunction`) |
| `FindFunctionByNameLoose` | tolerant function lookup |
| `FindViewportConsoleOffset` | `GameViewportClient::ViewportConsole` offset (reflection/probe) |
| `ResolveCachedFNameIndex` | cached FName index for a literal |
| `FindFirstLocalPlayer`, `FindPlayerController` | live-player lookups |
| `FindLiveEngineObject` | live GameEngine (UGameEngine preferred) |
| `ResolveGameViewport` | viewport from the engine |
| `FindConstructHelper` | engine factory function (`ConstructHelperInfo`: function, owner class, name) |
| `ScanForConsoleStateOffset`, `ResolveConsoleStateOffset` | resolve/validate `ConsoleState` (see README) |
| `ResolveNamedStateIndex` | cached FName index for `Typing`/`Open` |
| `IsConsoleStateActive` | true when `ConsoleState` FName is Typing/Open |
| `CallFakeGotoState` | dispatches `FakeGotoState` with an `FName` parameter |
| `TryOpenConsoleInternal` | the open sequence described in the README |
| `ShouldReleaseMouseCaptureForState` | maps to `IsConsoleStateActive` |
| `SetConsoleTargetPlayer` | writes `ConsoleTargetPlayer` to the first LocalPlayer |
| `AttachCheatManager`, `ReattachCheatManagerIfNeeded` | create/adopt `CheatManager` on the PlayerController, re-check after level transitions, and refresh the travel guard |
| `RemapConsoleKeys` | resolves/creates the `InputSettings` CDO, then rewrites `ConsoleKeys`, slot 0 = Tilde, slot 1 = F10 when `ArrayMax >= 2`; when `ArrayMax == 1`, slot 0 = F10 (Tilde fallback) |
| `WriteConsoleKeySlot` | initializes raw (past-`Num`) slots before naming them; constructed slots keep their engine-owned `FKey` details |
| `IsLikelyValidConsoleKeyArray` | header validation (`0 ≤ Num ≤ Max ≤ 64`, readable storage) |
| `LogConsoleKeySlots` | debug log of the first 4 slots |

### Cached state (extern)

`g_CachedConsoleObject`, `g_ConsoleStateOffset` (init `-2` = unresolved),
`g_NameTypingIndex`, `g_NameOpenIndex`, `g_FakeGotoStateFunction`,
`g_TriedResolveFakeGotoStateFunction`, `g_CachedCheatManagerOwner`, and the
one-shot log flags listed above. Cached classes: `g_CachedConsoleClass`,
`g_CachedGameEngineClass`, `g_CachedEngineClass`,
`g_CachedGameViewportClientClass`, `g_CachedLocalPlayerClass`,
`g_CachedGameplayStaticsClass`.

### Structs

```cpp
struct FSpawnObjectParams { UClass* ObjectClass; UObject* Outer; UObject* ReturnValue; };
struct FFakeGotoStateParams { FName NextStateName; };
struct FKeyEntry { FName KeyName; void* KeyDetails[2]; };
struct SpawnableClassEntry { std::string shortName, fullName; };
struct ConstructHelperInfo { void* Function; UClass* OwnerClass; const char* Name; };
struct PropertyLookupResult {
    void* Property; int32_t Offset; bool UsedFPropertyLayout;
    std::string TypeName, InnerTypeName, StructTypeName;
};
struct FConsoleMapElement { void* Value; };
struct ExecDumpContext { const EngineState* state; std::map<std::string, std::vector<std::string>> classToCommands; int totalCount; };
struct SpawnableDumpContext { /* state, actor/component/widget classes, 4 result vectors, totalCount */ };
```

---

## 7. Callback conventions used by the console integration

* **Lua console command handlers** are invoked as
  `(outputDevice, commandName, fullCommandLine, argsString)`; returning a truthy
  value cancels the engine execution. `outputDevice:Log(...)` writes to the
  Avalonium log. Return `nil` or `false` when the engine's own command should run; a truthy
  value eats it, and then the command does jack shit.
* `runtimeinput::ShouldReleaseMouseCapture()` is polled by the overlay input
  layer so the mouse is released while the console is open.

## See also

* [README.md](README.md): behaviour overview and output formats.
* [LuaAPI/HOOKS.md](../LuaAPI/HOOKS.md): console command APIs from Lua.
* [DiscoveryAPI/REFERENCE.md](../DiscoveryAPI/REFERENCE.md): property/function
  lookups and `CallProcessEvent` used here.
</content>
