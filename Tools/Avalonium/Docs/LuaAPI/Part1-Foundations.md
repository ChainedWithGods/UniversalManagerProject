# Part I: Foundations

## 1. What Avalonium is (and why that matters to you)

Avalonium is a runtime modding framework for Unreal Engine games, covering **UE 4.23 through
UE 5.8**. It injects into a shipping game, finds the engine's own internals in memory, the
object array, the name pool, reflection, the globals, the script VM's dispatch, and exposes
all of it to Lua.

The consequence for you, as a mod author, is the important part:

- **There is no SDK to compile.** No headers, no `.lib`, no C++ project. You ship `.lua`.
- **There is nothing to update when the game patches.** Avalonium re-discovers offsets at
  startup. The same mod keeps working across engine versions in the supported range. I didn't
  want a per-version build matrix, so we ship one `.lua` folder and accept the rediscovery
  cost at startup instead of a rewrite.
- **You are not limited to a curated API.** Anything the game reflects is reachable. If the
  game has a property, you can read it. If it has a function, you can call it. If it has a
  class, you can spawn it. If it does not reflect something, the raw memory API is still there.

Practically, modding with Avalonium feels like scripting the game from the inside, because that
is exactly what it is: a Lua state living in the game's process, with hooks on the engine's own
event dispatch.

### What you can build

A short, non-exhaustive list, every item of which is demonstrated later in this handbook:

| Capability | Where it is shown |
| :--- | :--- |
| Read and write any reflected property | §10 |
| Call any reflected function, with out-params | §11 |
| Spawn, attach, teleport and destroy actors | §12, §35 |
| Read and rewrite DataTables at runtime | §28, §35 |
| Hook any function before/after it runs and rewrite its arguments | §19 |
| React to actors beginning/ending play, and to level loads | §20 |
| Add keybinds | §22 |
| Schedule delays and loops | §23 |
| Draw HUD text, bars, crosshairs and custom ImGui windows | §25 |
| Read raw memory, walk pointers, patch bytes | §16 |
| Toggle the game's own CVars | §17 |
| Read/write files and parse JSON | §18 |
| Talk to other mods through a shared store | §14.5, §36 |
| Generate EmmyLua type stubs for autocomplete | §34 |
| Ask the runtime to explain its own discovery | §33, §34 |

## 2. Prerequisites and a sanity check

You need:

1. A supported game with Avalonium installed:
   - `<Game>\Binaries\Win64\<Game>-Win64-Shipping.exe`
   - `dwmapi.dll` (the proxy that loads the runtime)
   - `Avalonium.dll`, `Avalonium.ini`
   - `Mods\Avalonium\` next to them
2. A text editor. That is the entire toolchain.
3. The log file, `Avalonium.log`, open in a second window or tailed.

### Verify the runtime is alive

Launch the game and look for these blocks in the log. You do not need to understand all of it
yet, you are checking that (a) discovery succeeded and (b) Lua came up.

```
[Avalonium] Init thread started
[Discovery] Version: UE4.27
[Discovery] Objects: 44571
[Avalonium] Initialization successful
[Lua] Registering Lua bindings...
[Lua] Compat surface v3.5: wrappers=on, datatable-mutation=on, weak-objects=on
[Lua] Lua runtime initialized successfully
[Lua] Found N active mod(s) in <path>
```

If you see `Initialization successful` and `Lua runtime initialized successfully`, you are ready.

### Find your mods folder programmatically

Never guess the path. Ask the runtime:

```lua
print(GetModsDirectory())
```

If the folder is wrong, the log says so exactly:

```
[Lua] Mods directory does not exist: C:\...\Mods\Avalonium
```

The default is `<folder containing Avalonium.dll>\Mods\Avalonium`, and it can be overridden in
`Avalonium.ini`. We don't put mods next to the EXE by default; they sit under the DLL directory.
`LuaModsPath` is the only override, so guessing paths is a waste of time.

```ini
[Features]
EnableLuaMods = true
LuaModsPath   =
```

## 3. The log is your console

There is no attached debugger. Your instruments are:

| Instrument | How |
| :--- | :--- |
| `print(...)` / `log(...)` | a line in `Avalonium.log`, prefixed `[Lua]`, filed under the `mods` section |
| `Log(...)` | the compatibility spelling of the same sink |
| The in-game console | `ExecuteConsoleCommand("stat fps")` and friends |
| `Overlay` / `Notify` / `Toast` | what the *player* sees |
| `DumpAllObjects()` | a file listing every live object |
| `GetObjectShape(path)` | why the runtime classified an object the way it did |

`print` renders each argument the way Lua does, tab-separated:

```lua
print("string", 42, 3.5, true, nil, SomeObject)
-- [Lua] string	42	3.5	true	nil	userdata
```

Use it constantly. Every example in this handbook assumes you check the log after reloading.
We check the log after every reload. A silent reload is not a successful reload, and the log
line is the only proof the entry script actually ran.

## 4. The anatomy of a mod

```
Mods/Avalonium/
├── mods.json                     <- list: order + enabled flags
├── HelloMod/
│   ├── scripts/
│   │   ├── main.lua              <- entry point (required)
│   │   └── config.lua            <- optional module, via require("config")
│   └── assets/                   <- anything you want
├── shared/
│   ├── UEHelpers/UEHelpers.lua   <- shared helper module
│   ├── exports/Shared.lua        <- reachable via LoadExport("Shared")
│   └── types/<Game>/AvaloniumTypes.lua  <- generated EmmyLua stubs
├── Cache/                        <- reserved, never loaded
└── Loose.lua                     <- a standalone top-level script
```

### Entry-point rules

| Layout | Entry script | Notes |
| :--- | :--- | :--- |
| `MyMod/scripts/main.lua` | preferred | the modern layout |
| `MyMod/main.lua` | accepted | older layout |
| `Loose.lua` | standalone | mod name is the file name |

`shared` and `Cache` are **never** loaded as mods, no matter what the list says.

### `mods.json`: order and enablement

```json
{
  "mods": [
    { "enabled": true,  "name": "AvalonToolkit" },
    { "enabled": false, "name": "Whatever" },
    { "enabled": true,  "name": "HelloMod" }
  ]
}
```

* Mods load **top to bottom**; later mods can observe earlier mods' effects.
* Anything on disk but not listed is **auto-enabled** and loaded after the listed entries,
  alphabetically. A brand-new mod therefore works with zero configuration.
* `enabled` accepts `true`/`false`/`1`/`0`.
* Legacy `mods.txt` (`Name:1` per line, `;` comments) still works when `mods.json` is missing.

### The per-mod sandbox

Every mod is evaluated in a **fresh environment** whose metatable falls back to the real
globals (`__index = _G`). Practical consequences:

* `MyThing = 1` in your mod is invisible to other mods. Good, no collisions.
* You still *read* every global normally.
* To share state, use shared variables (§11.4) or `require`d modules.
* To intentionally publish something to everyone, write to the real table:
  `_G.MyGlobal = value` (used in §36).

### Reloading

| Method | Effect |
| :--- | :--- |
| **Alt+PageDown** | hot-reload all mods |
| `ReloadMods()` / `HotReloadMods()` | same, from Lua |
| IPC `LUA RELOAD` | same, from a tool |
| Restart the game | the slow but always-correct option |

A reload **clears every hook and every timer**, then re-runs every mod's entry script. That is
the single most important operational fact in this handbook, so it gets its own rule:

> **Rule:** a mod must be able to (re-)establish its entire world from its entry script, every
> single load. Never assume a hook or timer survived.

The corollary is `Mod.OnUnload`, which is where you undo side effects (§11.5).

## 5. Hello, mod: in two minutes

### Step 1: create the files

```
Mods/Avalonium/HelloMod/scripts/main.lua
```

```lua
-- HelloMod/scripts/main.lua
print("Hello from HelloMod!")

local player = GetPlayer()
if player and player:IsValid() then
    print("Player:", player:GetFullName())
    print("Class: ", player:GetClass():GetName())
else
    print("No player yet - are you at the main menu?")
end
```

### Step 2: load it

Either set `"enabled": true` for `HelloMod` in `mods.json` and restart, **or** just reload with
**Alt+PageDown** (auto-discovery will find it).

### Step 3: read the log

```
[Lua] Loaded mod: HelloMod (main.lua)
[Lua] Hello from HelloMod!
[Lua] Player: BP_PlayerCharacter_C /Game/Game/Blueprints/BP_PlayerCharacter.BP_PlayerCharacter_C
[Lua] Class:  BP_PlayerCharacter_C
```

### Step 4: make it do something

Add a keybind and a notification, then reload and press **F6** in game:

```lua
RegisterKeyBind(Key.F6, function()
    local player = GetPlayer()
    if player and player:IsValid() then
        Notify("Player is " .. player:GetName())
    end
end)
```

You have now used: a finder, a UObject method, a keybind, and the overlay. Everything else is
more of the same, with better aim.

## 6. The mental model that unlocks everything

If you remember five ideas, you can figure out the rest from `DumpProperties()` alone.

### 6.1 Everything is a UObject handle

`GetPlayer()` returns a handle. So does `FindFirstOf("Pawn")`, `GetWorld()`, an actor you found
in a table, a class object, a component, a function object. They all share one vocabulary:

```lua
local obj = GetPlayer()

obj:GetName()            -- short name          -> "BP_PlayerCharacter_C"
obj:GetFullName()        -- full path
obj:GetPathName()        -- path form
obj:GetAddressString()   -- "0x7FF7..."
obj:GetClass()           -- the UClass
obj:GetOuter()           -- containing object
obj:GetPackage()         -- containing package
obj:GetWorld()           -- owning world
obj:GetSuperClass()      -- parent class
obj:IsA("Pawn")          -- inheritance test
obj:IsValid()            -- liveness test
obj.Cast(classObj)       -- cast, nil on failure
obj:GetCDO()             -- class default object
obj:GetComponent("Mesh") -- a named component
```

### 6.2 Property access is live, not a copy

```lua
-- If the game exposes a property through reflection, reads and writes are live.
local before = player.CustomTimeDilation   -- reads the engine's storage
player.CustomTimeDilation = before + 0.2   -- writes the engine's storage
print(player.CustomTimeDilation)           -- reads it back
```

There is no "apply" step. The value you read came out of the game; the value you wrote went
straight in. This works for primitives, structs (as value objects), strings, names, and
through the container views arrays, maps and sets.

If a name is not a property, the runtime next tries it as a **function**, then as a **custom
property** (§31), then answers an invalid object.

### 6.3 Invalid is not nil

A miss returns a **neutral handle**, not `nil`. It answers every call with a neutral value, so
one missed lookup does not halt your mod.

```lua
local ghost = FindFirstOf("NoSuchClass")
print(ghost)              -- [UObject: ...] ... (0x0000000000000000) or similar
print(ghost:IsValid())    -- false
print(ghost.CustomTimeDilation) -- nil, no error
```

**Always** gate on validity:

```lua
if obj and obj:IsValid() then
    print(obj:GetName())
end
```

We gate on `:IsValid()`. `nil` checks don't catch misses here, and `obj.Method ~= nil` stays
true even for a garbage member.

### 6.4 The game thread rule

Mod **loading happens off the game thread**. Engine-touching work belongs on the game thread.

```lua
-- At load time, this is the safe wrapper:
ExecuteInGameThread(function()
    local world = GetWorld()
    if world and world:IsValid() then
        print("world:", world:GetName())
    end
end)
```

Hook callbacks, keybind callbacks and timer callbacks **already run on the game thread**, so
you do not need to wrap those. Use the thread helpers to check where you are:

```lua
print(IsInGameThread(), IsInAsyncThread(), IsInMainModThread())
print(GetGameThreadId(), GetCurrentThreadId())
```

### 6.5 Find → inspect → write

The loop you will repeat a thousand times:

```lua
-- 1. find
local pc = GetPlayerController()

-- 2. inspect (what is on this thing?)
if pc and pc:IsValid() then
    for _, prop in ipairs(pc:GetProperties() or {}) do
        print(prop.name, prop.type, prop.offset)
    end
end

-- 3. write
pc.bShowMouseCursor = true
```

`GetProperties()` is your map of any object. You rarely need documentation for a specific game
once you can dump its members.

## 7. Lua 5.4 that matters to modders

Avalonium embeds **Lua 5.4.6** with its standard library. Most of it behaves as you expect;
these are the points that affect mod code.

### 7.1 Multiple returns are real

```lua
local preId, postId = RegisterHook("SomeFunction", pre, post)
local lo, hi = GetKismetMathLibrary():MinOfIntArray(...)
```

### 7.2 Integers vs floats

Lua 5.4 distinguishes 64-bit integers from doubles. Property writes pick their conversion from
the property's own type, so `obj.SomeInt = 3.0` stores `3`. When reading, an integer property
gives you an integer.

```lua
print(math.type(1))     -- integer
print(math.type(1.0))   -- float
```

### 7.3 Standard library availability

`io`, `os`, `string`, `table`, `math`, `utf8` and `debug` basics are present. The shipped
`BPModLoader` uses `io.open`, `io.lines`, `loadfile` and `load`, so file work is available
even beyond the `File`/`JSON` helpers. Prefer `File`/`JSON` for portability and clearer logs.

### 7.4 `require` is mod-scoped

```lua
local config = require("config")
```

Search order (local first, always):

```
<mod>/scripts/config.lua      <mod>/scripts/config/init.lua     <mod>/scripts/config/main.lua
<mod>/Scripts/...             <mod>/config.lua                  <mod>/config/init.lua
<mod>/config/main.lua
```

Then the standard path. Modules are cached per **resolved file**, so your `config.lua` and
another mod's `config.lua` are different modules. Dots become slashes: `require("lib.math")`
looks for `lib/math.lua`.

### 7.5 Errors are reported, not fatal

A hook callback that raises is logged and the engine call continues. A mod that fails to
compile is logged and skipped while the others still load. We wrap risky calls in `pcall`. Not fatal is not the same as fine: a callback that throws
every frame can spam the log forever while the game limps along:

```lua
local ok, err = pcall(function()
    local player = GetPlayer()
    return player:SetLocation(0, 0, 400)
end)
if not ok then print("failed:", err) end
```

### 7.6 `#` and `ipairs` on engine containers

Provider tables (like `GetAllActorsOfClass`) are ordinary Lua arrays: `#arr` and `ipairs` work.
Live engine arrays are `TArray` views with `:Num()`/`:Count()` and `:ForEach()`, plus a `__len`
metamethod so `#arr` also works (§14.1).
