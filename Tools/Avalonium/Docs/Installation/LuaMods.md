# Lua Mods: Start Here

Lua mods are `.lua` scripts loaded by the running runtime. No SDK, no C++ compiler
and no rebuild: edit the file, reload, and the change is live. They reach the
engine through the same reflection the rest of the docs describe.

## What you need

- The runtime is injected and initialized: [Confirm the Runtime Is Running](VerifyRuntime.md).
- `EnableLuaMods=true` in `Avalonium.ini`. The shipped ini has it off.
- The Lua mods folder. With `LuaModsPath` empty it is `Mods\Avalonium` next to
  `Avalonium.dll`. Universal Manager sets the path to the profile's Lua mods
  folder for you.

## A mod in two minutes

Layout:

```text
Mods\Avalonium\HelloMod\scripts\main.lua
```

```lua
print("Hello from HelloMod!")

local player = GetPlayer()
if player and player:IsValid() then
    print("Player:", player:GetFullName())
else
    print("No player yet - are you at the main menu?")
end
```

Load it: drop the folder in and press **Alt+PageDown** in game. The reload scans
the directory, so a new script needs no list entry and no restart. You can also
restart the game.

Read the log (`Avalonium.log`):

```text
[Lua] Loaded mod: HelloMod (main.lua)
[Lua] Hello from HelloMod!
[Lua] Player: BP_PlayerCharacter_C /Game/.../BP_PlayerCharacter.BP_PlayerCharacter_C
```

## The mod list

- `mods.json` in the mods folder is the preferred list and its order is honored:
  `{ "mods": [ { "enabled": true, "name": "MyMod" } ] }`.
- Without a list file, every mod folder on disk is auto-enabled.
- `mods.txt` is the legacy list, one `name:1` per line.
- `shared` and `Cache` are reserved and never loaded as mods.

Universal Manager manages the same list for installed profiles.

## Reloading

- **Alt+PageDown** reloads every Lua mod.
- IPC also has `LUA RELOAD`, `LUA RUN <file>` and `LUA EVAL <code>`; see
  [IPC.md](../RuntimeAPI/IPC.md).

A reload clears reflected hooks and timers, then rescans the folder. Dropped-in
scripts are picked up without touching the list.

## Where to go next

The full handbook lives in [LuaAPI](../LuaAPI/README.md). The shortest path
through it:

| Document | Contents |
| :--- | :--- |
| [GettingStartedLuaMods.md](../LuaAPI/GettingStartedLuaMods.md) | the eight-part course, start here |
| [Part1-Foundations.md](../LuaAPI/Part1-Foundations.md) | first mod, the mental model, Lua essentials |
| [Part2-RuntimeAPI.md](../LuaAPI/Part2-RuntimeAPI.md) | finding objects, properties, calls, spawning |
| [Part3-EventDriven.md](../LuaAPI/Part3-EventDriven.md) | hooks, keybinds, timers, overlay |
| [LuaModding.md](../LuaAPI/LuaModding.md) | the complete Lua API surface |
| [MODS.md](../LuaAPI/MODS.md) | mod layout, `mods.json`, `require` |
| [HOOKS.md](../LuaAPI/HOOKS.md) | the hook reference |
| [UOBJECT.md](../LuaAPI/UOBJECT.md) | the UObject reference |

Writing a compiled mod instead? [Build Your First C++ Mod](FirstCppMod.md) is the
C++ route.
