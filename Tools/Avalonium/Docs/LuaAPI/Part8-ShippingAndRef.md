# Part IX: Shipping a mod

A mod that works on your machine is a script. A mod other people can install is a product.
This part is short and mostly about discipline.

## 52. The folder layout to ship

```
MyMod/
    scripts/
        main.lua              -- entry point, nothing else may be required to load
        config.lua
        ui.lua
        cheats.lua
    README.md                 -- what it does, how to install, keybinds, known issues
    CHANGELOG.md              -- dated entries, newest first
    LICENSE                   -- pick one; MIT is the friendly default
    settings.example.json     -- optional: show users the shape of the settings file
```

Notes:

* **The folder name is the mod name.** Renaming the folder breaks `mods.json` entries,
  `ModRef("Name")` lookups, and settings paths (`GetModDirectory() .. "/MyMod/..."`). Pick it
  once.
* **Never ship a populated `mods.json`.** We let users merge the entry into their own list; a
  shipped list keeps overwriting theirs every time a new version is copied.
* **Do not ship generated state**: `session.json`, `settings.json`, log files, or a generated
  `AvaloniumTypes.lua`. They are per-user.
* **Do ship `README.md`** with the keybinds. A mod with a hidden hotkey is a mod nobody uses.

## 53. A README template

```markdown
# MyMod

One sentence: what it does.

## Requirements
- Avalonium runtime (compat surface 3.5 or newer)
- Game: <name> <build/version tested against>

## Install
1. Copy the `MyMod` folder into your mods directory.
2. Add `{ "enabled": true, "name": "MyMod" }` to `mods.json`.
3. Reload mods (hotkey: Alt+PageDown).

## Keybinds
| Key | Action |
| :--- | :--- |
| F7 | open the palette |
| Ctrl+S | save settings |

## Settings
Written to `MyMod/settings.json`. Delete the file to reset to defaults.

## Known issues
- <honest list>

## Changelog
See CHANGELOG.md.
```

## 54. Versioning and compatibility

Version the mod, and gate on the runtime, not on hope:

```lua
local MOD_VERSION = "1.2.0"
local REQUIRED_COMPAT = 5          -- RuntimeVersion.CompatMinor

if RuntimeVersion.CompatMinor < REQUIRED_COMPAT then
    print(("MyMod %s requires compat surface 3.%d or newer; this runtime is 3.%d. Not loading.")
        :format(MOD_VERSION, REQUIRED_COMPAT, RuntimeVersion.CompatMinor))
    return
end

print(("MyMod %s loading (engine %s)"):format(MOD_VERSION, UnrealVersion:GetMajor() .. "." .. UnrealVersion:GetMinor()))
```

`return` at the top level of `main.lua` stops the chunk cleanly, the runtime logs the mod as
loaded and nothing else happens. We use `return` for refusal; an `error` gets logged as a failure and looks like a bug, even
when the check was intentional.

For game-specific property names, detect rather than assume:

```lua
local function FirstExisting(object, names)
    for _, name in ipairs(names) do
        local value = object[name]
        if value ~= nil then return name, value end
    end
    return nil
end

local name, value = FirstExisting(GetPlayer(), { "CustomTimeDilation", "TimeDilation", "ActorTimeDilation" })
print("speed property is", name, value)
```

## 55. The pre-release checklist

We run this before every release. Two minutes here beats a support thread, and every
unchecked box becomes someone else's bug report.

**Load**

* [ ] Fresh install, fresh `settings.json`: does it write defaults without erroring?
* [ ] Corrupt `settings.json` (paste garbage into it): does it still load?
* [ ] Old `settings.json` missing a key: does `DeepFill` handle it?

**Reload**

* [ ] Load → unload → load: no error, no duplicate handlers, no doubled timers.
* [ ] Load → load without unloading (hotkey): still one of everything.
* [ ] After unload: have time dilation, CVars and speeds been restored?

**Threads**

* [ ] Every engine call is on the game thread (or wrapped in `ExecuteInGameThread`).
* [ ] Nothing is read from engine memory on the async thread.

**Performance**

* [ ] No full-world scan in a draw callback.
* [ ] No per-frame `print`.
* [ ] Timer intervals are the largest that still feel responsive.

**Behaviour**

* [ ] No player exists yet (load before spawning in): does the mod survive?
* [ ] Level change: does it re-resolve objects instead of holding dead handles?
* [ ] Every destructive feature is off by default.
* [ ] Keybinds do not collide with the game's own keys.

## 56. Distributing

Whatever the channel (a mod site, a forum thread, a zip in a chat), ship the same four things:

1. **A zip whose root is the mod folder**: so extracting into the mods directory is the entire
   install.
2. **A checksum or a version number** in the file name: `MyMod-1.2.0.zip`.
3. **The tested game build** in the README. "Works on the current patch" ages badly. I name the tested build; "latest" is a moving target that turns a known issue into a
   mystery.
4. **A way to disable everything.** One key that turns off every mutation, documented in the
   README. Support requests drop sharply when the first question has an answer.

## 57. Working with other mods

Several mods in one process is the normal case, so:

* **Namespace your globals.** Each mod already has its own environment, but shared variables are
  global by name, prefix them: `MyMod.state`, not `state`.
* **Never assume you own a property.** If two mods write `CustomTimeDilation`, the last writer per
  frame wins and the result is arrhythmic. Publish a service and let one mod own the value.
* **Expect other windows in the menu.** The in-game menu is shared: registering a window adds a
  panel next to everyone else's. Keep yours compact and titled with your mod's name.
* **Do not unregister someone else's hook.** `UnregisterHook(id)` takes an id you received; both
  the name form and the id form can take down another mod's patch if you pass the wrong thing.
  We unregister by id; a name is a shared bucket, not a handle.
* **Be a good consumer.** If another mod provides a service, prefer it over re-implementing the
  scan (the ModKit pattern in Project 7 exists exactly for this).

---

# Part X: Reference appendices

## Appendix A: The API surface, by category

Every global the runtime registers, grouped by what you use it for. See
[LuaModding.md §14](LuaModding.md#14-complete-a--z-index-globals--top-level-values) for the
flat A–Z list with per-name detail.

**Accessors**
`GetPlayer` `GetPlayerPawn` `GetPlayerCharacter` `GetPlayerController` `GetFirstPlayerController`
`GetPlayerCameraManager` `GetCameraManager` `GetLocalPlayer` `GetGameInstance` `GetGameMode`
`GetGameState` `GetWorld` `GetEngine` `GetPlayerState`

**Camera & viewport**
`GetCameraInfo` `ProjectWorldToScreen` `DeprojectScreenToWorld` `GetMousePosition` `GetViewportSize`

**Components & actors**
`GetComponentByClass` `GetAllComponents` `GetActorBounds` `GetActorLocation` `SetActorLocation`

**Queries**
`FindFirstOf` `FindAllOf` `FindObject` `FindObjects` `FindClass` `StaticFindObject` `StaticClass`
`StaticStruct` `StaticEnum` `GetAllActorsOfClass` `GetAllActors` `GetWorldActors` `GetDistance`
`GetObjectCount` `GetObjectByIndex` `GetObjectName` `GetObjectFullName` `GetObjectClass`
`GetSuperClass` `InheritsFrom` `GetClassProperties` `ue.get_globals`

**Construction**
`NewObject` `StaticConstructObject` `ConstructObject` `DuplicateObject` `SpawnActor` `LoadAsset`
`StaticLoadObject` `CallFunction` `ExportProperty` `ImportProperty`

**Diagnostics**
`GetBlueprintPipeline` `GetObjectShape` `DumpAllObjects` `ForEachUObject` `ForEachUObjectFlat`
`GenerateLuaTypes`

**Kismet**
`GetGameplayStatics` `GetKismetSystemLibrary` `GetKismetMathLibrary` `GetKismetStringLibrary`
`GetKismetArrayLibrary` `GetKismetTextLibrary` `GetKismetGuidLibrary` `GetKismetRenderingLibrary`
`UObjectGlobals` `Math`

**Collision & traces**
`LineTraceSingle`

**Hooks**
`RegisterHook` `Hook` `UnregisterHook` `Unhook` `RegisterCustomEvent` `HookCustomEvent`
`UnregisterCustomEvent` `UnhookCustomEvent` `NotifyOnNewObject` `OnNewObject` `RegisterBeginPlayHook`
`OnBeginPlay` `RegisterEndPlayHook` `OnEndPlay` `RegisterLoadMapHook` `OnLoadMap`
`RegisterInitGameStateHook` `OnInitGameState` and the `PreHook`/`PostHook` variants

**Input**
`RegisterKeyBind` `RegisterKeybind` `BindKey` `RegisterKeyBindAsync` `IsKeyBindRegistered`
`IsKeyDown` `IsKeyPressed` `IsKeyHeld` `Key`

**Time**
`ExecuteInGameThread` `RunOnGameThread` `NextTick` `ExecuteWithDelay` `SetTimeout`
`ExecuteInGameThreadWithDelay` `RetriggerableExecuteInGameThreadWithDelay`
`ExecuteInGameThreadAfterFrames` `LoopAsync` `SetInterval` `SetTimer` `LoopInGameThreadAfterFrames`
`LoopInGameThreadWithDelay` `MakeActionHandle` `CancelTimer` `ClearTimer` `CancelDelayedAction`
`PauseDelayedAction` `UnpauseDelayedAction` `ResetDelayedActionTimer` `SetDelayedActionTimer`
`IsValidDelayedActionHandle` `IsDelayedActionActive` `IsDelayedActionPaused`
`GetDelayedActionTimeRemaining` `GetDelayedActionTimeElapsed` `GetDelayedActionRate`
`ClearAllDelayedActions` `ExecuteAsync`

**Console**
`RegisterConsoleCommandHandler` `RegisterConsoleCommand` `RegisterCommand`
`RegisterConsoleCommandGlobalHandler` `ExecuteConsoleCommand` `ExecuteCommand` `GetCVar` `SetCVar`
`FindCVars`

**Overlay & HUD**
`Overlay` `Notify` `Toast` `ToggleHUD` `HideAllHUD` `ShowAllHUD` `IsHUDHidden` `RefreshHUDCache`
`ToggleMetrics` `ToggleMetricsHUD` `ShowMetrics` `ShowMetricsHUD` `HideMetrics` `HideMetricsHUD`
`IsMetricsVisible` `IsMetricsHUDVisible`

**Files & data**
`File` `JSON` `GetModDirectory` `GetModsDirectory` `RegisterCustomProperty` `IterateGameDirectories`
`CreateLogicModsDirectory`

**Memory**
`Memory` (and the snake_case mirrors on `ue`)

**Mods**
`Mod` `ModRef` `GetModRef` `_SharedVariables` `_ModRegistry` `_ModUnloadCallbacks` `ReloadMods`
`HotReloadMods` `RestartMod` `RestartCurrentMod` `UninstallMod` `UninstallCurrentMod` `LoadExport`
`loadexport`

**Runtime info**
`UnrealVersion` `RuntimeVersion` `UE4SS` `Avalonium` `EObjectFlags` `EInternalObjectFlags`
`EFindName` `PropertyTypes` `NAME_None` `GetFrameTime` `GetFPS` `GetDeltaSeconds`
`GetMemoryUsage` `ForceGarbageCollection` `CollectGarbage` `GetGameThreadId` `GetAsyncThreadId`
`GetMainModThreadId` `GetCurrentThreadId` `IsInGameThread` `IsInAsyncThread` `IsInMainModThread`
`EngineTickAvailable` `ProcessEventAvailable` `MainThread` `AsyncThread` `ThreadId` `threadid`
`HookThread` `CreateInvalidObject`

**Values**
`FVector` `Vector` `FRotator` `Rotator` `FTransform` `Transform` `FQuat` `FVector2D` `FVector4`
`FLinearColor` `FColor` `FString` `FAnsiString` `FUtf8String` `FName` `FText` `FSoftObjectPath`
`TSoftObjectPtr` `FWeakObjectPtr` `FDelegate` `FMulticastScriptDelegate` `Delegate`
`MulticastDelegate` `FNameUserdata` `ThreadIdUserdata` `FPackageName` `DerefToInt32`
`dereftoint32`

**Logging**
`print` `log` `Log` `Print` `OtherLog`

## Appendix B: Glossary

| Term | Meaning |
| :--- | :--- |
| **Avalonium** | this runtime; the Lua API is its scripting surface |
| **BPML** | Blueprint Mod Loader, the Blueprint-side mod pipeline (`AvaloniumBP.md`) |
| **CDO** | Class Default Object; the `Default__X` instance a class is built from. Never treat one as a live actor |
| **`_C` suffix** | Blueprint-generated *class* (spawnable), as opposed to the Blueprint asset |
| **compat surface** | the v3.5 API layer that keeps scripts written for other Unreal Lua runtimes working |
| **delayed action** | a queued one-shot or repeating callback owned by a handle |
| **game thread** | the thread that owns engine state; where hook, keybind, timer and draw callbacks run |
| **hook** | a pre/post interception of a named UFunction via `ProcessEvent` |
| **invalid object** | a handle whose target is gone; methods resolve but reads return neutral values |
| **lifecycle hook** | a hook on `BeginPlay`, `EndPlay`, `LoadMap` or `InitGameState` |
| **live proxy** | a hook parameter that reads and writes the engine's own argument storage |
| **mods root** | the folder containing every mod; what `GetModDirectory()` returns |
| **out-param** | a function argument the engine writes back; exposed as a live proxy |
| **`ProcessEvent`** | the engine's virtual dispatch entry point the runtime hooks |
| **shared variable** | a process-wide value other mods can read |
| **UObject wrapper** | the userdata type backing every object handle |

We never write to a CDO; it changes the template every future spawn copies from, a
game-wide bug with no source mod in the log.

## Appendix C: Where to go next

| Document | Contents |
| :--- | :--- |
| [LuaModding.md](LuaModding.md) | the complete, source-verified Lua API surface |
| [AvaloniumBP.md](../BPMods/AvaloniumBP.md) | Blueprint / LogicMods modding |
| [README.md](README.md) | the API folder index |
| [UOBJECT.md](UOBJECT.md) | UObject reference |
| [HOOKS.md](HOOKS.md) | hook reference |
| [MODS.md](MODS.md) | mod packaging reference |
| [Architecture.md](../Architecture/Architecture.md) | how the runtime itself is built |

The shipped mods in the mods directory are also worth reading: `AvaloniumAPIReporter`
(surface verification), `BPModLoader` (Blueprint loading), `UniversalHUDToggle` and
`UniversalOverlay` (the built-in HUD and overlay features), and `GenericFunctions`
(Blueprint helper library).

---

*Written against the same source as [LuaModding.md](LuaModding.md). Where this guide and the
binary disagree, the binary is right, and the `AvaloniumAPIReporter` mod exists to tell you
when that has happened.*
