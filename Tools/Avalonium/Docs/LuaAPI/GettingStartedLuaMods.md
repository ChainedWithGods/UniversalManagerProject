# Getting Started with Lua Mods for Avalonium

Welcome. This handbook teaches you to write runtime mods for Unreal Engine games running under
**Avalonium** (UE 4.23 through UE 5.8). No SDK, no C++, no rebuilds, just `.lua` files and
the engine's own reflection. I don't recompile to change a number. If the workflow demands a rebuild for that, the
workflow is wrong.

## How this guide is organised

The handbook is split into eight focused parts so each file stays short enough to actually read.
Start at the top; the later parts assume the earlier ones.

| # | File | What it covers |
| :--- | :--- | :--- |
| 1 | [Part1-Foundations.md](Part1-Foundations.md) | What Avalonium is, prerequisites, the anatomy of a mod, your first mod in two minutes, the mental model that unlocks everything, Lua 5.4 essentials |
| 2 | [Part2-RuntimeAPI.md](Part2-RuntimeAPI.md) | Finding things, reflection, properties, function calls, spawning, value types, containers, names/strings, memory, CVars, files/JSON |
| 3 | [Part3-EventDriven.md](Part3-EventDriven.md) | Function hooks, lifecycle hooks, custom events, keybinds, timers, threads, the overlay, built-in globals |
| 4 | [Part4-Advanced.md](Part4-Advanced.md) | FProperty wrappers, DataTables, enums, structs, custom properties, the compatibility layer, the blueprint pipeline, diagnostics |
| 5 | [Part5-Recipes.md](Part5-Recipes.md) | 25 copy-paste recipes plus a second 25-recipe set with full engine-aware fallbacks |
| 6 | [Part6-Projects.md](Part6-Projects.md) | Six complete mod projects (PlayerTweaks, FlightRecorder, DirectorMode, LiveBalance, Hooked, ModKit) |
| 7 | [Part7-DoingItWell.md](Part7-DoingItWell.md) | Performance, threading, reload correctness, the debugging playbook, and the cookbook of value types, keys, overlay, timers, hooks |
| 8 | [Part8-ShippingAndRef.md](Part8-ShippingAndRef.md) | Shipping a mod, the pre-release checklist, full API surface index, glossary, and pointers to other docs |

## The shortest possible tour

If you have ten minutes and a working game, read these in order:

1. **§1–2** in [Part 1](Part1-Foundations.md), what Avalonium is, what a mod folder looks like.
2. **§5** in [Part 1](Part1-Foundations.md), the "Hello, mod" two-minute example.
3. **§8.1** in [Part 2](Part2-RuntimeAPI.md), the player and world accessors.
4. **§9–11** in [Part 2](Part2-RuntimeAPI.md), the find → inspect → write loop.
5. **§19–22** in [Part 3](Part3-EventDriven.md), hooks and keybinds.
6. **§25** in [Part 3](Part3-EventDriven.md), the overlay (notifications, stats, drawing).
7. Then pick a project from [Part 6](Part6-Projects.md) that matches what you want to build.
   We start from a project that already runs. The engine doesn't give a shit how clever the
   folder layout looks; it cares whether the code reaches the right object.

## Companion documents

| Document | Contents |
| :--- | :--- |
| [LuaModding.md](LuaModding.md) | the complete, source-verified Lua API surface |
| [AvaloniumBP.md](../BPMods/AvaloniumBP.md) | Blueprint / LogicMods modding |
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
