# LuaAPI: Hooks, Events, Keybinds & Timers

Everything dispatched by `LuaHookManager`
([`LuaHookManager.h`](../../Runtime/lua/LuaHookManager.h) /
[`.cpp`](../../Runtime/lua/LuaHookManager.cpp)).

---

## 1. Function hooks

### Registration

```lua
local preId, postId = RegisterHook("FunctionName", preCallback, [postCallback])
local preId, postId = Hook("FunctionName", preCallback, [postCallback])   -- alias
UnregisterHook(preId)                       -- numeric id form
UnregisterHook("FunctionName")              -- by name (both ids)
UnregisterHook("FunctionName", preId, postId) -- explicit pair
Unhook(...)                                 -- aliases of UnregisterHook
```

`RegisterHook` returns the **pair of registry ids** (`preId`, `postId`) that must be handed
back when unregistering; `postId` is `0` when no post callback was supplied.

### Dispatch & callback convention

```
pre(self, Param1, Param2, …)   -- before the engine executes the function
post(self, Param1, Param2, …)  -- after; return value via __OriginalReturnValue
```

* `self` is the object the function was dispatched on.
* Parameters are **live property views** (`PushPropertyView`): reading fetches engine storage,
  writing writes back. Out-parameters included, so a pre-hook can rewrite arguments.
* A pre-hook returning a **truthy** value cancels the engine call (return value is the hook's).
* The post callback sees the engine's return value in the global `__OriginalReturnValue`.
* Hooks are installed on the function by name: `FunctionName` is normally the UFunction short
  name. Map-load spellings (`LoadMap`, `Engine:LoadMap`, `UEngine:LoadMap`,
  `/Script/Engine.Engine:LoadMap`) are remapped to the level-load post event when no such
  UFunction exists; custom events are matched case-insensitively.

### Installation internals

Two `Avalonium::InlineHook` patches (not MinHook, see
[SafeMemAPI/REFERENCE.md §InlineHook](../SafeMemAPI/REFERENCE.md#part-2--inline-hooking)):

| Patch | Covers |
| --- | --- |
| `ProcessEvent` | Native/Blueprint-dispatched functions; slot resolved by `FunctionFinder`'s ProcessEvent law (UObject impl + AActor override, vtable fallback mode) |
| `ProcessLocalScriptFunction` | Interpreted (bytecode) UFunctions, the VM calls nested script functions directly, so the ProcessEvent trampoline never sees them |

`DispatchScriptFunctionEntries` answers entries registered on interpreted functions. Both
routes feed `PrepareBeginPlayRideAlong`, so begin-play-riding entries (LoadMap, InitGameState)
behave identically whichever route carried the event.

If the ProcessEvent slot cannot be resolved, the manager keeps a fallback heartbeat and a
**deferred name** mechanism: registering a hook for a name the process does not yet hold
stores it in `m_PendingNameHooks`; when the name appears it is applied, and a name that never
appears is logged once (`ResolvePendingHooks`, `m_PendingNameWatches`). We leave a typo'd name
in the pending list forever with exactly one log line; nothing retries the spelling, one line
in the log is the whole alarm.

### Related helpers

| Global | Behavior |
| --- | --- |
| `RegisterCustomEvent(name, pre, [post])` / `HookCustomEvent` | Subscribe to a custom event dispatched by UFunction short name (case-insensitive); returns ids |
| `UnregisterCustomEvent(name, [preId], [postId])` / `UnhookCustomEvent` | Remove |
| `IsKeyBindRegistered(key)` | Whether a bind exists (accepts a `Key` code or name) |
| `NotifyOnNewObject(className, callback)` / `OnNewObject` | Called for newly created objects of that class (class-prefix match) |
| `RegisterConsoleCommandHandler(name, callback)` / `RegisterConsoleCommand` / `RegisterCommand` | Exact-name console handler; once registered it consumes that command unconditionally, regardless of the callback's return value |
| `RegisterConsoleCommandGlobalHandler([name], callback)` | Wildcard form (empty/no name) or named form; returning `true` blocks the engine exec path |
| `ExecuteConsoleCommand(cmd)` / `ExecuteCommand` | Run a console command string; returns boolean |

We let a named console handler own its command outright. Return `false` and it still doesn't go
back to the engine, that one surprises everyone who thought they had a veto.

`LuaHookManager::ClearAllHooks()` is invoked by `ReloadMods()`: reflected hooks do not
survive a reload unless re-registered by the entry scripts. Console-exec lifecycle callbacks
and custom-event registrations live in separate maps and are not cleared.

---

## 2. Engine lifecycle hooks

All are registered with `(preCallback, [postCallback])` and return a numeric id. Callbacks
receive the relevant object (`self`).

| Global | Alias | Answered when |
| --- | --- | --- |
| `RegisterBeginPlayHook` | `OnBeginPlay` | An actor begins play, rides on `AActor::ReceiveBeginPlay` plus per-class override functions, answered **once per object per engine call** (`ClaimBeginPlayEntry`) |
| `RegisterEndPlayHook` | `OnEndPlay` | An actor ends play |
| `RegisterLoadMapHook` | `OnLoadMap` | A level world comes up, answered **once per world** (address + name identity, `WorldNotice`); callback context is the world |
| `RegisterInitGameStateHook` | `OnInitGameState` | A world's gameplay authority is up, callback is handed the **game mode actor** of the world. The engine cannot hook `AGameModeBase::InitGameState` (native virtual, and not reflected in 4.23–5.8), so the signal rides the level's begin-play pass and is answered once per world |

This is a ride-along signal, not a real `InitGameState` hook. It means the world is up, not
that the engine's virtual just ran, Epic left the virtual unreflected, and the name is doing
more work than the hook.

Compatibility pre/post variants (from `LuaCompatParity.cpp`):

```
RegisterBeginPlayPreHook / RegisterBeginPlayPostHook
RegisterEndPlayPreHook   / RegisterEndPlayPostHook
RegisterLoadMapPreHook   / RegisterLoadMapPostHook
RegisterInitGameStatePreHook / RegisterInitGameStatePostHook
RegisterProcessConsoleExecPreHook / RegisterProcessConsoleExecPostHook
RegisterCallFunctionByNameWithArgumentsPreHook / …PostHook
RegisterULocalPlayerExecPreHook / RegisterULocalPlayerExecPostHook
```

The last three fire from the runtime's own console-command dispatch: they answer
when a command runs through `ExecuteConsoleCommand`, because `ProcessConsoleExec`,
`CallFunctionByNameWithArguments` and `ULocalPlayer::Exec` have no script binding
in any supported engine version, so reflection can never reach them. Argument
shapes: `(context, cmd, params, outputDevice, executor)`,
`(context, cmd, outputDevice, executor, bForce)`, `(context, world, cmd,
outputDevice)`; a pre callback returning `true` claims the command.

---

## 3. Keybinds

```lua
RegisterKeyBind("F9", callback)                              -- key name
RegisterKeyBind(Key.F9, callback)                            -- Key table virtual-key code
RegisterKeyBind("S", {ModifierKey.CONTROL}, callback)        -- modifiers (table)
RegisterKeyBind({Key.S, ModifierKey.SHIFT}, callback)        -- table form (first = key)
-- RegisterKeybind / BindKey / RegisterKeyBindAsync are aliases of the above.
```

* Names resolve through `ResolveVirtualKeyCode` (case/space/underscore/dash-insensitive):
  `f1…f24`, `a…z`, `0…9`, `space`, `enter`, `tab`, `escape`, `shift/ctrl/alt`, arrows,
  `pageup/pagedown`, numpad keys, OEM punctuation (`minus`, `plus`, `bracketleft`, `quote`,
  `tilde`, …), mouse buttons (`lbutton`, `rbutton`, `mbutton`, `xbutton1/2`, or `mouse1..5`).
* Numeric arguments are treated as virtual-key codes and normalized through
  `VkToCanonicalName`: so `Key.F9` and `"F9"` file under the same bind.
* Modifiers come from `ModifierKey.*`; a modifier table may contain names or codes.
* Dispatch happens on key press from the console WndProc; **every** key press is also
  forwarded to Lua keybind dispatch, independent of the console hotkeys
  (Tilde/F10 open console; Alt+PageDown hot-reloads mods).

`IsKeyDown(key)` / `IsKeyPressed` / `IsKeyHeld` query live key state (`GetAsyncKeyState`).
`IsKeyBindRegistered(key)` accepts the same two spellings.

---

## 4. Timers & delayed actions

| Global | Form | Behavior |
| --- | --- | --- |
| `ExecuteInGameThread(fn)` | `NextTick`, `RunOnGameThread` | Enqueue for the next game-thread pump |
| `ExecuteWithDelay(delayMs, fn)` | `SetTimeout` | Returns a timer handle |
| `ExecuteWithDelay(userHandle, delayMs, fn)` | UE `Delay`-style | Uses a caller-owned handle |
| `ExecuteInGameThreadWithDelay(userHandle, delayMs, fn)` | N/A | Same, compat spelling |
| `RetriggerableExecuteInGameThreadWithDelay(userHandle, delayMs, fn)` | N/A | Retriggerable |
| `ExecuteInGameThreadAfterFrames(frames, fn)` | `ExecuteInGameThreadAfterFrames_DelayStyle` (delay-style alias) | Frame-based, one-shot |
| `LoopAsync(intervalMs, fn)` | `SetInterval`, `SetTimer` | Repeating |
| `LoopInGameThreadAfterFrames(frames, fn)` | N/A | Repeating frame-based |
| `LoopInGameThreadWithDelay(ms, fn)` | N/A | Repeating delay-style |
| `MakeActionHandle()` | N/A | Reserve a unique handle (shared counter with timer ids) |
| `CancelTimer(handle)` | `ClearTimer`, `CancelDelayedAction` | Cancel |
| `PauseDelayedAction` / `UnpauseDelayedAction` | N/A | Pause/resume |
| `ResetDelayedActionTimer` / `SetDelayedActionTimer(handle, ms)` | N/A | Reset / change delay |
| `IsValidDelayedActionHandle` / `IsDelayedActionActive` / `IsDelayedActionPaused` | N/A | Queries |
| `GetDelayedActionTimeRemaining` / `GetDelayedActionTimeElapsed` / `GetDelayedActionRate` | N/A | Timing |
| `ClearAllDelayedActions()` | N/A | Cancel everything (called by mod reload) |
| `ExecuteAsync(fn)` | N/A | Run once on the runtime's async pump thread, not the game thread |

Timers are driven by a dedicated timer thread (`LuaHookManager::m_TimerThread`) that enqueues
callbacks to the game-thread queue; `ProcessTimers`/`ProcessGameThreadTasks` run them where
the engine state exists. Handles are numbered from one counter
(`m_NextTimerId`/`MakeActionHandle`) so a stale handle can never name an unrelated live timer.
We number handles from one counter, so a cancelled handle comes back invalid instead of
quietly cancelling some other mod's timer. That's the whole point; without it, stale handles
are a loaded gun.

---

## 5. Utility generation globals

| Global | Behavior |
| --- | --- |
| `GenerateLuaTypes([dir])` | Generate `AvaloniumTypes.lua` for the detected game |
| `DumpAllObjects([path])` | Dump the live object array |
| `ForEachUObject(callback)` | Iterate the object array with the compatibility convention: `callback(UObject, chunkIndex, indexWithinChunk)` |
| `ForEachUObjectFlat(callback)` | Flat iteration: `callback(UObject)` |

---

## 6. Failure behavior

* A hook callback raising a Lua error is logged; the engine call proceeds. We keep hook errors
  non-fatal, but that's not a license to be sloppy, a broken hook can spam the log for hours
  while the game limps along, and silence is not the same as success.
* Hook dispatch only runs mod code on threads where the engine's per-thread state exists
  (`MayRunModCodeOnThisThread`); off-thread calls are marshalled to the game thread.

---

## Cross-references

* [README.md](README.md): mod loading, threading.
* [UOBJECT.md](UOBJECT.md): hook parameter property views.
* [GLOBALS.md](GLOBALS.md): `ExecuteConsoleCommand`.
* [COMPAT.md](COMPAT.md): `RuntimeVersion`, `EGameThreadMethod`, thread introspection.
* [SafeMemAPI/REFERENCE.md](../SafeMemAPI/REFERENCE.md): the InlineHook used for both patches.
