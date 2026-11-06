# LuaAPI: Lua Scripting & Mod Loader

Avalonium embeds **Lua 5.4.6** (statically linked, [`Helpers/lua-5.4.6`](../../Helpers/lua-5.4.6))
and registers a large scripting surface: engine access, UObject reflection, memory I/O,
hooking, timers, keybinds, console commands and compatibility shims for the UE4SS scripting
community. Mods are ordinary Lua files loaded from disk; no recompilation is required. No SDK,
no rebuild, no header dump: I keep the whole surface reflection-driven so a `.lua` file can
walk into a shipping build and start talking to the engine.

| Document | Contents |
| --- | --- |
| [GLOBALS.md](GLOBALS.md) | Core global functions: actors, objects, CVars, File/JSON, `Memory`, `ue` |
| [UOBJECT.md](UOBJECT.md) | UObject wrapper members, property get/set, function calls, containers, value types |
| [HOOKS.md](HOOKS.md) | `RegisterHook`, custom events, keybinds, lifecycle hooks, timers, threading |
| [COMPAT.md](COMPAT.md) | UE4SS-compatible surface (`RuntimeVersion` v3.5), parity globals, aliases |
| [MODS.md](MODS.md) | Mod layout, `mods.json`/`mods.txt`, shared variables, type generation, builtin mods |

Source: [`Runtime/lua/`](../../Runtime/lua/), `LuaManager`, `LuaUObject`, `LuaTypes`,
`LuaHookManager`, `LuaCompatApi`, `LuaCompatParity`, `LuaCompatSurface`, `LuaTypeGenerator`,
`BuiltinMods/`.

---

## 1. Enablement & initialization

```ini
[Features]
EnableLuaMods = true     ; default: true when the key is absent; aliases LuaMods, Lua
LuaModsPath   =          ; default: <dir of Avalonium.dll>/Mods/Avalonium
```

| Step | Where |
| --- | --- |
| `LuaManager::Initialize(hModule)` | `dllmain.cpp` on the init thread, after engine discovery |
| `RegisterBindings()` | `LuaManager.cpp`, types, UObject, hooks, globals, `ue`, `Memory`, compat layers, overlay bindings |
| `LoadModsFromDirectory(...)` | `LuaManager.cpp`, discovery, list parsing, sandboxed execution |
| `LuaHookManager::Initialize(L)` | installs the ProcessEvent / ProcessLocalScriptFunction hooks |

Shutdown order takes Lua first: hot-reload/cancel state is torn down before the overlay and
the rest of the runtime (see [RuntimeAPI/README.md](../RuntimeAPI/README.md)). We tear Lua down first on shutdown. Do it the other way round and a timer fires into a dead
Lua state, that's not a shutdown, that's a crash with extra steps.

### IPC access

The TCP server exposes Lua commands (`LUA STATUS`, `LUA EVAL <code>`, `LUA RUN <file>`,
`LUA LOAD`, `LUA DOFILE`, `LUA RELOAD`, `LUA RESET`), see
[RuntimeAPI/IPC.md](../RuntimeAPI/IPC.md). Scripts executed this way share the global state.

---

## 2. Mod loading

Default directory: **`<Avalonium.dll dir>\Mods\Avalonium`** (override with `LuaModsPath`).
If the directory does not exist the loader logs and stops.

### 2.1 List file

`mods.json` is preferred (order is honored):

```json
{ "mods": [ { "enabled": true, "name": "MyMod" }, { "enabled": false, "name": "Other" } ] }
```

Legacy `mods.txt` (one `name:1` / `name:true` per line; `;`/`#` comments) is used when
`mods.json` is absent or parses empty. When neither exists, every mod folder on disk is
auto-enabled (alphabetical order).

Rules applied for a listed mod (`mods.json` order):

* Disabled → skipped with a log line.
* Enabled but folder missing → `missingMods` log, skipped.
* Folder present without `scripts/main.lua` or `main.lua` → skipped with a log.
* Unlisted folders and top-level `.lua` files found on disk are auto-loaded after the listed
  ones. `shared` and `Cache` directories are never treated as mods.

### 2.2 Entry point

| Layout | Entry script |
| --- | --- |
| `Mods/Avalonium/MyMod/scripts/main.lua` | preferred |
| `Mods/Avalonium/MyMod/main.lua` | accepted |
| `Mods/Avalonium/foo.lua` | standalone script (no mod identity directory) |

Each mod is loaded with a **fresh environment table** whose metatable `__index = _G`
(isolated globals) and a per-mod `require` resolver. Load errors are reported in the log.
One broken mod doesn't get to take the rest down. We log the failure and keep loading; a bad
script is a bad script, not a reason to kill every other mod in the folder.

### 2.3 `require` inside a mod

The mod's own tree is searched first, permanently (also from hook/timer callbacks):

```
<modRoot>/scripts/<name>.lua
<modRoot>/scripts/<name>/init.lua
<modRoot>/scripts/<name>/main.lua
<modRoot>/Scripts/...            (same three suffixes)
<modRoot>/<name>.lua
<modRoot>/<name>/init.lua
<modRoot>/<name>/main.lua
```

Dots in the module name become `/`. Modules are cached in `package.loaded` under the
**resolved file path** (`@<path>`), so two mods requiring `config` do not share one module.
If nothing in the mod tree matches, the standard Lua search path answers.

### 2.4 Reloading

* `ReloadMods()` / `HotReloadMods()` globals (also IPC `LUA RELOAD`, and the **Alt+PageDown**
  hotkey). A reload clears reflected hooks and timers, clears `m_LoadedMods`, then re-runs the
  whole discovery pass. Console-exec lifecycle callbacks and custom-event registrations are
  kept.
* No mod-list change is needed for new scripts dropped into the folder.

---

## 3. Threading model

| Thread | What runs there |
| --- | --- |
| Game thread | Hook callbacks (ProcessEvent / script-function dispatch), keybind callbacks, timers processed from the game-thread pump, `ExecuteInGameThread` tasks |
| Timer thread | OS-level scheduling of `ExecuteWithDelay` / `LoopAsync` (callbacks still run on the game thread) |
| Init/async thread | Mod loading pass, `ExecuteAsync` bodies |
| Caller thread | `LuaManager::ExecuteString` / `ExecuteFile` under the Lua mutex |

* One `lua_State`, one mutex: `LuaManager::GetLuaMutex()` (recursive) guards entry. We run one `lua_State`, one thread queue, one chance to fuck the frame. A callback that blocks
the game thread stalls the whole game, that's not a style note, it's the architecture.
* A mod-loading pass owns the state; `m_LoadingMods` is set **before** taking the mutex so an
  engine thread that arrives mid-load answers immediately instead of stalling the game thread
  (see `LockLuaForHandOff`).
* Mod code that must run where the engine's per-thread state exists is only dispatched on the
  game thread (`MayRunModCodeOnThisThread`); use the thread helpers
  (`IsInGameThread`, `ExecuteInGameThread`, …) to respect that.
* `ForEachUObject` / `ForEachUObjectFlat` and other large scans use **thread-local result
  buffers** so the game thread and async callers do not fight over one array.

---

## 4. Files the Lua system reads/writes

| Path | Purpose |
| --- | --- |
| `<mods>/mods.json`, `<mods>/mods.txt` | mod list |
| `<mods>/shared/types/<GameName>/AvaloniumTypes.lua` | auto-generated engine type annotations |
| `<mods>/shared/` | shared modules/mods; skipped as a mod, usable via `require` search paths |
| `<mods>/Cache/` | reserved; skipped as a mod |
| `<mods>/<Mod>/scripts/main.lua` | entry script |

`AvaloniumTypes.lua` is generated by `LuaTypeGenerator::GenerateLuaTypes` the first time the
loader runs with no such file (requires a built object index).

---

## 5. Global surface overview

Registered by `LuaManager::RegisterBindings()` in this order:

```
LuaEngine::RegisterLuaTypes             → value/container types, Math
LuaEngine::RegisterLuaUObject           → UObject metatable, enums, datatables, actors,
                                          vectors, transforms, delegates, FWeakObjectPtr
LuaEngine::RegisterLuaHookBindings      → hooks, events, keybinds, console, timers,
                                          thread helpers, type generation
LuaManager::RegisterGlobalHelpers       → print/log, players, Kismet libs, CVars,
                                          File, JSON, finders
LuaManager::RegisterEngineGlobals       → FindFirstOf / FindAllOf / StaticFindObject /
                                          StaticConstructObject / UObjectGlobals
LuaManager::RegisterUETable             → snake_case `ue` table
LuaManager::RegisterMemoryBindings      → `Memory` table (PascalCase + snake_case)
LuaEngine::RegisterCompatibilityLayer   → ModRef / UEHelpers
(end of RegisterLuaHookBindings)        → compat API batches 1–3 + Parity + Surface
LuaEngine::Overlay::RegisterLuaOverlayBindings → `Overlay` table (see OverlayAPI)
```

Every name is listed in the documents below; the [GLOBALS.md](GLOBALS.md#full-global-index)
index is the authoritative flat list. The index is the flat truth. If a name isn't in it, the name doesn't exist, I don't care what
a stale forum post swears.

---

## 6. Source map

| File | Contents |
| --- | --- |
| [`LuaManager.h`](../../Runtime/lua/LuaManager.h) | manager API, per-global handlers |
| [`LuaManager.cpp`](../../Runtime/lua/LuaManager.cpp) | registration, mod loading, `require`, core globals, `ue`/`Memory` |
| [`LuaTypes.h`](../../Runtime/lua/LuaTypes.h) | value/container data structs, push/get helpers |
| [`LuaTypes.cpp`](../../Runtime/lua/LuaTypes.cpp) | vectors, rotators, transforms, arrays, maps, sets, delegates, math |
| [`LuaUObject.h`](../../Runtime/lua/LuaUObject.h) | `FoundPropertyEx`, property view/value helpers, DataTable helpers |
| [`LuaUObject.cpp`](../../Runtime/lua/LuaUObject.cpp) | UObject metatable and reflection implementation |
| [`LuaHookManager.h`](../../Runtime/lua/LuaHookManager.h) | hook/timer/keybind manager, god-mode guards |
| [`LuaHookManager.cpp`](../../Runtime/lua/LuaHookManager.cpp) | ProcessEvent trampoline, dispatch, timer thread, hook bindings |
| [`LuaCompatApi.h`](../../Runtime/lua/LuaCompatApi.h) | compat surface v3.5 declarations |
| [`LuaCompatApi.cpp`](../../Runtime/lua/LuaCompatApi.cpp) | `Key`, flags, `PropertyTypes`, `FName`/`FText`/`FString`, threads |
| [`LuaCompatParity.cpp`](../../Runtime/lua/LuaCompatParity.cpp) | parity globals, weak ptr, `FQuat`/`FVector2D`/`FVector4`, lifecycle |
| [`LuaCompatSurface.cpp`](../../Runtime/lua/LuaCompatSurface.cpp) | `Mod`/`ModRef`, logging handles, string members |
| [`LuaTypeGenerator.h/cpp`](../../Runtime/lua/LuaTypeGenerator.h) | `GenerateLuaTypes`, `DumpAllObjects`, `ForEachUObject` |
| [`BuiltinMods/`](../../Runtime/lua/BuiltinMods) | `UniversalHUD`, `UniversalOverlay` builtin globals |

---

## Cross-references

* [GLOBALS.md](GLOBALS.md) · [UOBJECT.md](UOBJECT.md) · [HOOKS.md](HOOKS.md) ·
  [COMPAT.md](COMPAT.md) · [MODS.md](MODS.md)
* [RuntimeAPI/IPC.md](../RuntimeAPI/IPC.md): `LUA` IPC commands.
* [OverlayAPI/README.md](../OverlayAPI/README.md): `Overlay` Lua table.
* [Architecture.md](../Architecture/Architecture.md): documentation index and runtime architecture.
