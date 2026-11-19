# Installation

Avalonium ships two ways. The only difference is who installs it for you.

## Path 1: Universal Manager (recommended)

UE4SS and Avalonium are compatible on this path. Avalonium loads through Universal Manager, which injects the runtime into the running game. No `dwmapi.dll` is placed next to the game executable, so UE4SS keeps its own proxy.

1. Install it and open Universal Manager. Avalonium is already included, so there is nothing else to download.
2. Create a profile for the game: Game Presets → Create New Profile, or let Game Scan find the installed game and pick it.
3. Press Launch Game, wait until the game reaches the main menu, then press Inject Avalonium in the Avalonium Runtime Engine tab.

## Path 2: Avalonium only (manual)

Avalonium loads through a `dwmapi.dll` proxy, so it takes over the same file UE4SS uses. If the game uses UE4SS, Avalonium's `dwmapi.dll` overwrites the UE4SS one and UE4SS stops working.

1. Open `<Game>\Binaries\Win64`, the folder with the shipping executable.
2. Copy `Avalonium.dll`, `dwmapi.dll`, `Avalonium.ini`, `Helpers` and `Mods` into it. Merge with existing folders if asked. `dwmapi.dll` must sit next to the game executable.
3. In `Avalonium.ini`, enable the features you need, for example `InGameGUI` or `LuaMods`, or the in-game menu. Native C++ mods are enabled by default (`EnableCppMods=true`).

Runtime docs: [COMMANDS.md](../RuntimeAPI/COMMANDS.md), [CONFIG.md](../RuntimeAPI/CONFIG.md), [EXPORTS.md](../RuntimeAPI/EXPORTS.md), [IPC.md](../RuntimeAPI/IPC.md), [PROXY.md](../RuntimeAPI/PROXY.md), [README.md](../RuntimeAPI/README.md).

## Next steps

1. [Confirm the runtime is running](VerifyRuntime.md)
2. [Generate the game SDK](SDKGeneration.md)
3. [Build your first C++ mod](FirstCppMod.md)
4. [Troubleshooting](Troubleshooting.md)

Writing Lua instead of C++? Start with [Lua Mods: Start Here](LuaMods.md).

## Deeper reference

[TUTORIAL](../CppAPI/TUTORIAL.md) for game-specific patterns,
[EXAMPLES](../CppAPI/EXAMPLES.md) for complete mods,
[SDK_MODS](../CppAPI/SDK_MODS.md) for typed game access,
[SPAWNING](../CppAPI/SPAWNING.md) for spawn workflows,
[CppAPI](../CppAPI/README.md) for the full C++ API reference, and
[LuaAPI](../LuaAPI/README.md) for the Lua scripting surface.