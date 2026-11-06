# ConsoleAPI: `Runtime/console`

Turns a shipping UE game's disabled Unreal console back on, maps it to safe
hotkeys, unlocks developer/cheat commands, and dumps console-related metadata
for tools. Epic ships the console unwired and the commands people actually want sit behind
`ECVF_Cheat`; we drag that shit back online. It also keeps seamless travel sane
when a console command replaces the local player's controller.

Source files:

| File | Responsibility |
| :--- | :--- |
| `ConsoleInternal.h` | shared internal declarations: helper functions, cached state, dump contexts, `GetConsoleDumpDirectory` |
| `ConsoleHelpers.cpp` | generic helpers: UE-version-agnostic reflection/property/function lookup, object construction, viewport/player resolution |
| `ConsoleEnabler.cpp` | console creation/attachment, key remapping, WndProc input, lazy cheat manager |
| `ConsoleDumpers.cpp` | command unlocking plus `ExecCommands.txt`, `SpawnableClasses.txt`, `AllConsoleCommands.json`, `UnlockedCommands.txt`, `LoadedDLLs/` |
| `ConsoleVariables.cpp` | CVar values and the console reference: `CVARs.ini`, `CVarReference.txt` |
| `SeamlessTravelGuard.cpp` | participation patch for seamless travel: restores the real PlayerController when a console command left it detached |

Enabled/disabled by `UnrealEngineConsole` in `Avalonium.ini`
(see [RuntimeAPI/CONFIG.md](../RuntimeAPI/CONFIG.md)).

---

## What it does

```text
UnlockConsoleCommands      unlock ECVF_Cheat-bit commands in IConsoleManager
        │
EnableInGameConsole        locate live GameEngine → GameViewport →
        │                  ConsoleClass → create/reuse UConsole →
        │                  write GameViewport::ViewportConsole → set
        │                  ConsoleTargetPlayer → remap keys → attach CheatManager
        │
TryOpenConsole             FakeGotoState("Typing"|"Open") on the console object
        │                  (fallback when the engine stripped the state machine:
        │                   Tilde/F10 are forwarded to the engine's own handler)
        │
InstallWndProcHook         WM_KEYDOWN interception:
                           • Tilde / F10 / OEM_8 / scan code 41 → open console
                           • Alt+PageDown → hot-reload Lua mods
                           • every key → Lua keybind dispatch
```

The console object is cached in `ConsoleEnabler::g_CachedConsoleObject`;
`ReattachCheatManagerIfNeeded` re-attaches when a level transition replaces the
PlayerController, and refreshes the travel guard for PlayerController classes
that loaded since.

## Seamless travel safety

Some engine commands replace the local player's controller. The debug camera is
the loud one: the original PlayerController keeps its `PlayerState`, loses its
`Player`, and stays in the world. `APlayerController::ShouldParticipateInSeamlessTravel`
returns true for it, so travel carries it into the next world, where games that
spawn a pawn assuming `Player != nullptr` fall over. `SeamlessTravelGuard`
removes that state before the engine decides what to keep.

* Installs with the console (`EnableInGameConsole`, once) and refreshes on every
  console open through `ReattachCheatManagerIfNeeded`, so PlayerController
  classes loaded after startup get patched too.
* Finds the participation and `SwitchController` slots from the live class
  layout, not from fixed indices, and patches only PlayerController class
  vtables that still hold the stock constant-true stub. Game overrides stay.
  The participation virtual itself is UE5.1+; 4.23 through 5.0 have no such
  filter, so the guard logs that and patches nothing there.
* At travel it reads the world from the controller's Outer chain, switches the
  single local player back to the detached game controller when there is
  exactly one, and lets the drained substitute drop out of the keep set. The
  engine then spawns the destination PlayerController and the real PlayerState
  rides along.
* I keep it conservative: one local player and one detached controller, or it
  does nothing. If discovery fails it logs once and leaves the game alone.

Log lines use `[TravelGuard]` and land in LEVEL TRANSITION.

## Console creation strategy

`EnableInGameConsole` (require `bInitialized` + `UnrealEngineConsole=true`):

1. `UnlockConsoleCommands` walks `IConsoleManager`'s command map and clears the
   `ECVF_Cheat` bit so the commands are accepted.
2. Finds a live `GameEngine` object and its `GameViewport`.
3. Resolves `UConsole` class from the engine's own `ConsoleClass` property or by
   name (`"Console"`).
4. Reads `GameViewport::ViewportConsole`:
   * existing usable console → reuse;
   * stale/invalid pointer → log and replace.
5. Otherwise creates one, in order:
   * a reflected **construct helper** (`FindConstructHelper` + a spawn-params
     frame), the engine's own factory;
   * an already existing console owned by the viewport;
   * `StaticConstructObject_Internal` via `TrySEHConstruct`, both the
     params-frame and direct-call ABI variants.
6. Validates the object (class + outer == viewport) before attaching it to
   `ViewportConsole`. We're not attaching a random UObject that merely looks like a console,
   that's how the runtime ends up crashing deep inside the engine with no useful stack.
7. `SetConsoleTargetPlayer` writes `ConsoleTargetPlayer` to the first
   `LocalPlayer`.
8. `RemapConsoleKeys` resolves the `InputSettings` **class**, finds its CDO and
   constructs one when the cooked build never created `Default__InputSettings`,
   then writes `ConsoleKeys`: slot 0 = `Tilde`, slot 1 = `F10` when a second
   slot exists (single-slot arrays get `F10`). Raw slots are initialized first;
   constructed slots keep their engine-owned `FKey` details.
9. `CheatManager` attachment is lazy, `TryOpenConsoleInternal` calls
   `ReattachCheatManagerIfNeeded`. During enable it is only attached when the
   WndProc hook is disabled (never with the hardcoded default).

Every step logs a `[ConsoleEnabler]` line; failures leave the console simply
unavailable rather than crashing.

## Opening the console

`TryOpenConsole(state)`:

1. Ensures the console object exists.
2. Resolves `FakeGotoState` (a reflected function that drives `UConsole`'s
   state machine). If the shipping build stripped it, returns `false` and the
   WndProc handler forwards Tilde/F10 to the engine's own WndProc.
3. Calls `FakeGotoState("Typing")`, then `"Open"`; validates via the
   `ConsoleState` FName (`Typing`/`Open`).
4. `ConsoleState` offset resolution: direct reflection, else
   `HistoryBuffer + 112`, else `DefaultTexture_White + 144`, else fail with
   class diagnostics.

`runtimeinput::` helpers wrap this for the overlay/input layer:
`IsConsoleEnabled()`, `ShouldReleaseMouseCapture()` (true while the console
state is active), `TryOpenConsole()`.

## Hotkeys

| Key | Action |
| :--- | :--- |
| `` ` `` (Tilde), `F10`, `OEM_8`, scan code 41 | open the console (when enabled) |
| `Alt`+`PageDown` | hot-reload Lua mods (`LuaManager::HotReloadMods`) |
| any key | dispatched to Lua `RegisterKeyBind` handlers |

When the console is disabled, console keys are swallowed (`return 0`) instead of
reaching the engine. I swallow them so hitting Tilde with the feature off doesn't open the
engine's own console by accident.

## Dumps

`GetConsoleDumpDirectory` → `<logdir>\<GameName>\Console\` (sanitized game
name). `DumpAllDeveloperCommands` in `Avalonium.ini` gates the dumps. The three
file dumps run on worker threads that `LaunchDumpsAsync` joins, so
`UnlockConsoleCommands` returns only after they finish. I join all three worker threads, no
fire-and-forget bullshit; when the call returns, the files are on disk.

| Output | Producer | Contents |
| :--- | :--- | :--- |
| `ExecCommands.txt` | `DumpExecCommands` | game + engine version, totals; commands grouped by declaring class with parent class and count, each as a C++ signature with real types and function flags |
| `SpawnableClasses.txt` | `DumpSpawnableClasses` | actors / components / widgets / others with short + full names |
| `AllConsoleCommands.json` | `UnlockConsoleCommands` | `{ "game", "totalCommands", "commands": [...] }` with help text read through each console object's own `GetHelp` |
| `CVARs.ini` | `DumpConsoleVariables` | sorted `name=value` lines holding the current value of every console variable; console commands are a commented block where each line carries its help text (commands hold no value) |
| `CVarReference.txt` | `DumpConsoleVariables` | sorted reference of every console object: `name | type | flags | value | help`, with commands and their help text in their own section |
| `UnlockedCommands.txt` | `UnlockConsoleCommands` | names of commands whose `ECVF_Cheat` bit was cleared |
| `LoadedDLLs/` | `DumpLoadedModules` | copies of every loaded module (`.exe` skipped), incremental (unchanged files skipped by timestamp) |

## Enabling/disabling

* Enable: `UnrealEngineConsole=true` in `Avalonium.ini`, then press Tilde/F10.
  The travel guard installs with the console and stays off when the feature is
  disabled.
* The init thread prewarms and attempts console creation immediately; if the
  game window/viewport or console is not ready, `InputAndConsoleThread` retries
  for up to 200 attempts (≈20 s after discovery).
* `DisableInGameConsole` detaches Avalonium's console object from
  `ViewportConsole` without touching `InputSettings` (see the comment in the
  implementation); used when the config disables the console. I don't touch `InputSettings` on
  disable; writing a guessed `FKey` layout is a great way to corrupt the engine's allocator.

## See also

* [REFERENCE.md](REFERENCE.md): every public function and helper.
* [RuntimeAPI/CONFIG.md](../RuntimeAPI/CONFIG.md): `UnrealEngineConsole`,
  `DumpAllDeveloperCommands`.
* [LuaAPI/HOOKS.md](../LuaAPI/HOOKS.md): `ExecuteConsoleCommand`,
  `RegisterConsoleCommandHandler`.
</content>
