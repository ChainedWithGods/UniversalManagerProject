# Avalonium API Documentation

Avalonium is a runtime modding framework for Unreal Engine 4.23 – 5.8 games.
It injects into a shipping game process, discovers the engine's own internals
(object array, name pool, reflection, globals, VM dispatch, allocator and
console manager), and exposes them through these surfaces. The game hands over exactly nothing, so I take it anyway.

| Surface | Consumed by | Entry point |
| :--- | :--- | :--- |
| C exports (`Avalonium.dll`) | external tools (Universal Manager, injectors) | `LoadLibrary` + `GetProcAddress` |
| TCP/IP IPC server | external tools | `127.0.0.1:<ServerPort>` (default `20030`) |
| Lua scripting API | mods in `Mods/Avalonium` | `Mods/Avalonium/<mod>/scripts/main.lua` |
| Native C++ mods | DLLs in `Mods/Avalonium` | `Mods/Avalonium/<mod>/dlls/main.dll` exporting `AvaloniumModAbiVersion` and `start_mod` |
| ImGui overlay | the player | `Insert` / `F5` in game |
| SDK / USMap / Reflection generators | mod developers | IPC, Lua, or exports |
| Crypto utilities (AES key finder / pak verifier) | pak tooling | `AutoDiscoverAESKey`, JSON output |
| Structured session log | operators and support | `Avalonium.log` (+ rotated copies) |
| AggressiveDog crash watchdog | operators and support | [CrashHandler/AggressiveDog.exe](../../Runtime/AggressiveDog/README.md) |

This documentation set is organised the same way the runtime source tree is:

| API folder | Runtime source | What it documents |
| :--- | :--- | :--- |
| [RuntimeAPI](../RuntimeAPI/README.md) | `Runtime/api`, `Runtime/core/dllmain.cpp`, `Runtime/AggressiveDog`, `Runtime/proxy` | DLL exports, IPC command protocol, `Avalonium.ini`, crash watchdog, dwmapi proxy bootstrap |
| [DiscoveryAPI](../DiscoveryAPI/README.md) | `Runtime/core` | Engine version detection, pattern scanning, globals/offsets/reflection discovery, allocator identification, object index, blueprint & asset-registry discovery, encrypted discovery cache, structured logging |
| [ConsoleAPI](../ConsoleAPI/README.md) | `Runtime/console` | In-game Unreal console enabler, command unlocking, dumps |
| [CryptoAPI](../CryptoAPI/README.md) | `Runtime/crypto` | AES-256 key discovery from live memory, pak/UTOC inspection and key verification |
| [LuaAPI](../LuaAPI/README.md) | `Runtime/lua` | Complete Lua modding surface: globals, UObject handles, hooks, timers, keybinds, compatibility layer, built-in mods |
| [CppAPI](../CppAPI/README.md) | `Runtime/cppmods` | Native C++ mod loader, C ABI table, reflected values and calls, object construction, UMG widget kit and delegates, typed containers, native and interpreted UFunction hooks, native non-UFUNCTION resolution and calls, key binds, SDK headers |
| [OverlayAPI](../OverlayAPI/README.md) | `Runtime/overlay` | ImGui overlay (Globals, World Explorer, Live Editor, Global Search), Lua overlay API |
| [SafeMemAPI](../SafeMemAPI/README.md) | `Runtime/utils` | Fault-free memory reads/writes, page-table oracle, inline hook engine |
| [SdkAPI](../SdkAPI/README.md) | `Runtime/sdk` | C++ SDK generator, ReflectionDB/diff, validation and libclang pipeline |
| [UsmapAPI](../UsmapAPI/README.md) | `Runtime/usmap` | `.usmap` mapping file generator and format |

---

## 1. Architecture at a glance

```text
 game folder (Binaries\Win64)
 ├── <Game>-Win64-Shipping.exe
 ├── dwmapi.dll              <- proxy: forwards real dwmapi + LoadLibrary(Avalonium.dll)
 ├── Avalonium.dll            <- the runtime (this repo's main target)
 ├── Avalonium.ini            <- runtime configuration
 ├── Avalonium.pdb            <- optional symbols (better debugging)
 └── Mods\Avalonium\          <- Lua mods + mods.json
```

Startup flow (see [RuntimeAPI](../RuntimeAPI/README.md) for details):

```text
dwmapi.dll (proxy)
   └─ LoadLibrary(Avalonium.dll)
        └─ DllMain(DLL_PROCESS_ATTACH)
             ├─ OverlayIntegration::SetMainModule()
             ├─ AggressiveDog::LaunchIfEnabled()       [watchdog]
             └─ CreateThread(InitThread)
                  ├─ BeginLogSession()                  [LogSession]
                  ├─ RuntimeConfig::Load(Avalonium.ini)
                  ├─ optional InitDelay
                  ├─ EngineDiscovery::Initialize()      [DiscoveryAPI]
                  │    └─ g_State (EngineState)
                  ├─ IOCPServer::Start(port)            [RuntimeAPI]
                  ├─ game window + WndProc hook         [ConsoleAPI]
                  ├─ AES key scan (config-gated)        [CryptoAPI]
                  ├─ console enable (sync + retries)    [ConsoleAPI]
                  ├─ overlay bootstrap                  [OverlayAPI]
                  ├─ LuaManager::Initialize()           [LuaAPI, when EnableLuaMods]
                  ├─ CppModManager::Initialize()        [CppAPI, when EnableCppMods]
                  │    └─ LoadModsFromDirectory()
                  │         ├─ installs ProcessEvent hook when no other owner exists
                  │         └─ loads native mods before Lua scripts
                  ├─ LuaManager::LoadModsFromDirectory() [after native mods]
                  └─ discovery cache save
             (on DLL_PROCESS_DETACH) Lua shutdown, IPC stop, WndProc removal,
             process-exit-aware overlay stop, state reset, EndLogSession
```

All subsystems share the global `EngineState g_State` object.
The struct is declared in `Runtime/core/Types.h`, the single instance is
defined in `Runtime/api/UEInternalAPI.cpp`, and consumers declare it `extern`.
Discovery fills it; every other subsystem borrows it read-only. We keep one struct, one source of truth, no shadow copies drifting out of sync like some
bullshit.

## 2. Build targets

From `Runtime/CMakeLists.txt` (CMake ≥ 3.20, C++23, MSVC):

| Target | Output | Purpose |
| :--- | :--- | :--- |
| `Avalonium` | `Avalonium.dll` + `Avalonium.pdb` | the runtime; post-build copies into `Runtime/DLL/` and deploys the PDB to `CrashHandler/` |
| `AggressiveDog` | `CrashHandler/AggressiveDog.exe` | external crash watchdog (MSVC only); a build dependency of `Avalonium` |
| `dwmapi_proxy` | `dwmapi.dll` | proxy bootstrap DLL (MSVC + MASM only) |

Third-party dependencies live in `../Helpers`: `minhook`,
`Universal-Dear-ImGui-Hook` (ImGui + DX11/DX12 hooks), and `lua-5.4.6`.
The build links `kernel32 user32 gdi32 dwmapi psapi ws2_32 bcrypt
shlwapi dxgi d3d11 d3d12` and uses a static CRT.

## 3. Deployment quick start

1. Copy `dwmapi.dll`, `Avalonium.dll`, `Avalonium.ini` and `Avalonium.pdb`
   next to the game's shipping executable (`<Game>\Binaries\Win64\`).
2. Copy the `CrashHandler\` folder (watchdog executable + PDB) when
   `[AggressiveDog] Enabled=1`.
3. Copy `Mods/Avalonium` there if you want Lua or native C++ mods. A native mod is a
   folder with `dlls/main.dll` (or `dlls/<ModName>.dll`) exporting
   `AvaloniumModAbiVersion`, `start_mod` and `uninstall_mod`.
4. Leave `LuaModsPath=` empty so mods load from the local `Mods\Avalonium`.
5. Configure a startup delay (`InitDelaySeconds=10`) for games that must finish
   loading before hooks are safe. I added delay init because hooking a game mid-load is a great way to make the runtime fall or eat a crash for no reason.
6. Launch the game normally.

Full file-by-file installation notes also live in
[Runtime/DLL/Readme.md](../../Runtime/DLL/Readme.md).

## 4. Documentation conventions

* **C++ signatures** are copied verbatim from the headers; namespaces are
  included where they matter (`EngineDiscovery::`, `Crypto::`, `SDKGenerator::`,
  `LuaEngine::`).
* **C exports** are `extern "C" __declspec(dllexport)` with plain C names.
* **IPC commands** are uppercase and case-insensitive at the dispatch level
  (`NormalizeCommandName` strips whitespace/BOM and uppercases the verb);
  responses end in `\n`, and a command is answered on a single connection.
* **Lua functions** are case-sensitive as written. Many also exist in
  `snake_case` or under aliases; aliases are listed explicitly.
* Every API carries its source file, so symbol → file lookups stay traceable
  (`Runtime/core/EngineDiscovery.cpp`, etc.) instead of buried under some vague "core"
  bullshit. Saves us from grepping the whole tree every time.
* "Thread-safety" notes are explicit wherever a function allocates a shared
  buffer, mutates shared state, or must run on the game thread.

## 5. Version coverage

* Supported engines: **UE 4.23 – 4.27 and 5.0 – 5.8**.
* Anything older than 4.23 is refused at startup (`bUnsupportedVersion`),
  logged, and initialization aborts to avoid undefined behaviour. I refuse unsupported engines; guessing through one is how a runtime gets a crash with no
  useful stack.
* Nothing in the discovery layer is version-tabled: every offset, layout and
  function is recovered from the running process itself (see
  [DiscoveryAPI/README.md](../DiscoveryAPI/README.md#design-principles)).
</content>
