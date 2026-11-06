# LuaAPI: Mod Packaging, Shared Tree & Builtin Mods

Everything a mod author needs beyond the API surface itself. Loader behavior is documented in
[README.md §2](README.md#2-mod-loading).

---

## 1. Directory layout

```
<Avalonium.dll dir>/Mods/Avalonium/            default root (LuaModsPath overrides)
├── mods.json                                  preferred mod list (order honored)
├── mods.txt                                   legacy list (name:1)
├── MyMod/
│   ├── scripts/
│   │   ├── main.lua                           entry point
│   │   └── config.lua                         required as "config"
│   └── assets/…
├── shared/
│   ├── exports/Name.lua                       LoadExport("Name") target
│   ├── Name.lua                               LoadExport fallback
│   └── types/<GameName>/AvaloniumTypes.lua    auto-generated definitions
├── Cache/                                     reserved (never loaded as a mod)
└── standalone.lua                             top-level script, auto-loaded
```

A mod is identified by its folder name (entry script under `scripts/` → parent folder name;
standalone `.lua` → file name); that name is used for logs and `OnUnload` attribution.

---

## 2. `mods.json`

```json
{
  "mods": [
    { "name": "CoreMod",   "enabled": true  },
    { "name": "OptionalMod","enabled": false }
  ]
}
```

* The parser is deliberately lenient: it locates `"mods"`, then reads `{…}` objects, taking
  `name` and `enabled` keys; `enabled` accepts `true/false/1/0`; unknown keys are skipped.
  I keep the parser lenient, not psychic. A malformed file falls back to auto-discovery, and
  one typo can quietly change the whole load set, no error, no warning, just a different list.
* Order is preserved and honored, mods load top to bottom.
* Listing a mod that is not on disk logs a `missing` warning (not fatal).
* Folders not mentioned are **auto-enabled** after the listed ones
  (`[Lua] Found unlisted mod '%s' on disk (auto-enabled)`), except `shared` and `Cache`.

### `mods.txt` (legacy)

```
; comment or # comment
CoreMod:1
OptionalMod:0
```

Used only when `mods.json` is missing/empty. Parse failures fall back to auto-discovery.

---

## 3. Entry-point execution

For every active script the loader:

1. Creates a fresh environment table with `__index = _G` and installs the mod's `require`
   resolver + root path.
2. `luaL_loadfile` → environment as first upvalue → `lua_pcall`.
3. On success logs `[Lua] Loaded mod: <name> (<file>)` and appends to the loaded list.
4. On compile/run failure logs the error; other mods still load. We let the other mods keep
   loading; one broken mod doesn't take the rest down, but the log gets loud for a reason,
   "still loading" is not "everything is fine".

Implications:

* **Globals are per-mod**: assigning `Foo = 1` in one mod is invisible to others; read/write
  shared data through `ModRef:GetSharedVariable` / `ModRef:SetSharedVariable` (or
  `GetModRef():...`) or return values from `require`d modules. There are no bare
  `GetSharedVariable`/`SetSharedVariable` globals.
* `require("config")` from `scripts/main.lua` resolves to `scripts/config.lua`, `scripts/config/init.lua`
  or `scripts/config/main.lua` (also `Scripts/`, and mod-root variants). The fallback to the
  standard Lua path only happens when nothing local matches.
* Modules are cached per **resolved file**, so two mods can each have their own `config.lua`.

---

## 4. Shared modules and `LoadExport`

`LoadExport("Name")` / `loadexport` searches the mods root, in order:

```
shared/exports/Name.lua
shared/Name.lua
exports/Name.lua
Name.lua
```

The first existing file is compiled and its return value handed back (nil + log line when
nothing is found). This is the modern replacement for copy-pasting helper modules between
mods.

`shared/` is also reachable through normal `require` fallback if it is on `package.path`.

---

## 5. Type definitions (`AvaloniumTypes.lua`)

* Generated automatically before mod loading when
  `<mods>/shared/types/<GameName>/AvaloniumTypes.lua` does not exist and the object index is
  built.
* Format: **EmmyLua** annotations for the detected game's classes/functions.
* Manual regeneration: `GenerateLuaTypes([dir])` (defaults to
  `Mods/shared/types/<GameName>`); logs
  `[LuaTypeGen] Successfully generated EmmyLua type definitions to: <path> (<n> classes)`.
* `DumpAllObjects([path])` (default `Avalonium_ObjectDump.txt`) dumps every object as
  `[index] [0xaddr] FullName (Class: X)`, logging `[ObjectDumper] …`.

---

## 6. Builtin mods

Registered unconditionally by `LuaManager::Initialize` via
`LuaEngine::BuiltinMods::RegisterAll(L)` ([`BuiltinMods/`](../../Runtime/lua/BuiltinMods)).

### UniversalHUD

```
ToggleHUD()  HideAllHUD()  ShowAllHUD()  IsHUDHidden()  RefreshHUDCache()
```

Controls the in-game HUD visibility, with a cached HUD state that can be refreshed after
level transitions.

### UniversalOverlay

```
ToggleMetrics() ToggleMetricsHUD()
ShowMetrics()   ShowMetricsHUD()
HideMetrics()   HideMetricsHUD()
IsMetricsVisible() IsMetricsHUDVisible()
Notify(msg[, type[, duration]])        -- overlay notification
Toast(msg[, type[, duration]])         -- toast notification
```

Registration log lines:

```
[UniversalHUD] Registered: ToggleHUD, HideAllHUD, ShowAllHUD, IsHUDHidden, RefreshHUDCache
[UniversalOverlay] Registered built-in mod globals: ToggleMetrics, ShowMetrics, HideMetrics, IsMetricsVisible, Notify, Toast
```

The overlay's richer API (`Overlay.Notify`, windows, anchors, …) is documented in
[OverlayAPI](../OverlayAPI/README.md).

---

## 7. Hot reload & cleanup

* **Alt+PageDown** or `ReloadMods()` / `HotReloadMods()` (IPC `LUA RELOAD`):
  1. `LuaHookManager::ClearAllHooks()`
  2. `LuaHookManager::ClearAllTimers()`
  3. `m_LoadedMods.clear()`
  4. Full discovery/load pass again.
* Reflected UFunction hooks and timers do not survive a reload; re-register them from the
  entry script. Console-exec lifecycle callbacks and custom-event registrations are stored
  separately and **do** survive `ClearAllHooks`.
* `ModRef.OnUnload = function() … end` stores a cleanup callback per detected mod name, but
  the runtime does not currently invoke it on reload or uninstall. We treat that as decoration,
  not teardown, critical cleanup goes somewhere the reload actually reaches.

---

## 8. Load log reference

| Log line | Meaning |
| --- | --- |
| `[Lua] Loaded mod list from mods.json (N entries)` | list parsed |
| `[Lua] Loaded mod list from mods.txt (legacy format, N entries)` | legacy list parsed |
| `[Lua] Mod 'X' is disabled in mods.json (skipped)` | disabled entry |
| `[Lua] Mod 'X' is enabled in mods.json, but folder was not found on disk (skipped)` | missing folder |
| `[Lua] Mod 'X' folder exists, but neither 'scripts/main.lua' nor 'main.lua' was found (skipped)` | no entry script |
| `[Lua] Found unlisted mod 'X' on disk (auto-enabled)` | auto-discovery |
| `[Lua] Found N active mod(s) in <dir>` / `… (M disabled in mods.json)` | summary |
| `[Lua] Info: All N detected mod(s) are currently disabled …` | everything off |
| `[Lua] Loaded mod: X (main.lua)` | success |
| `[Lua] Error running mod <file>: <err>` / `[Lua] Failed to compile mod <file>: <err>` | failure |
| `[Lua] Mods directory does not exist: <dir>` | bad `LuaModsPath` |
| `[Lua] === Hot-reloading all active mods ===` | reload start |

---

## 9. Authoring tips

* Keep entry work short on load; do engine-touching work inside `ExecuteInGameThread` or a
  hook/timer callback, where engine thread state exists.
* Wrap risky reads with `Memory.IsValidPtr` / `SafeMem`-backed accessors.
* Prefer `require` over globals for module state; use shared variables for cross-mod state.
* Feature-detect with `RuntimeVersion.CompatMinor` and `RuntimeVersion.CompatMajor` rather
  than assuming a surface level.
* Register `ModRef.OnUnload` for anything that must be undone (patched CVars, spawned actors,
  file handles).
* Return early from pre-hooks (truthy) only when you truly want to cancel the engine call.
  We return early only to cancel. A stray `true` in a pre-hook silently eats the engine call,
  and then a feature mysteriously stops working.

---

## Cross-references

* [README.md](README.md): loader pipeline.
* [GLOBALS.md](GLOBALS.md): `File`, `JSON`, `ReloadMods`, `GetModDirectory`.
* [COMPAT.md](COMPAT.md): `Mod`/`ModRef`, `LoadExport`, hot-reload aliases.
