# RuntimeAPI: Configuration Reference

Two files control the runtime:

| File | Loaded by | Purpose |
| :--- | :--- | :--- |
| `Avalonium.ini` | `RuntimeConfig::Load()`, next to `Avalonium.dll` | every feature toggle |
| `injector_config.json` | `Logger.cpp`, next to `Avalonium.dll` | log path + verbose flag only |

`RuntimeConfig::Load()` runs once per process (result cached). On every load it
reads the current file, keeps the values of schema keys it finds, fills missing
keys with defaults, and **rewrites `Avalonium.ini` from the schema** with the
commented header. Keys, comments and sections outside the schema are therefore
dropped; the `[AggressiveDog]` section is preserved (values or defaults) and
appended after the main keys.

Parsing details:

* Keys may live at the root (flat file, as shipped) or in a `[Features]` /
  `[Init]` section: both are read. `[Features]`/`[Init]` keys take priority
  over root-level duplicates.
* Booleans accept `1/true/yes/on` and `0/false/no/off` (case-insensitive);
  anything else keeps the default.
* Integers accept `0x`/`0` prefixes (`std::stoul(..., 0)`).
* Strings are read with `GetPrivateProfileStringA` (2048-byte cap) or the flat
  fallback reader; surrounding whitespace is trimmed.

---

## Keys

### `InGameGUI` (bool, default `false`)

Enables the ImGui overlay. Aliases: `IsInGameGUI`.
When `false` but `EnableLuaMods=true`, the overlay bootstrap still starts so the
Lua overlay API (toasts/stats/custom windows) can render when a backend
initializes, the menu window itself is only drawn by `menu::Init()` when this
flag is true.

### `UnrealEngineConsole` (bool, default `false`)

Enables the in-game Unreal console. Aliases: `IsUnrealEngineConsole`,
`UEConsole`. The init thread attempts console creation synchronously; if the
window/viewport is not ready, `InputAndConsoleThread` retries for up to
200 attempts (≈20 s). When `false`, discovery is told to skip the
`IConsoleManager` search (`skipConsoleManager = true`) and the WndProc hook's
console keys become no-ops.

### `SkipPatternScan` (bool, default `false`)

Skips the default `GUObjectArray` AOB pattern scan and goes straight to the
obfuscation scan. Aliases: `SkipGUObjectArrayPatternScan`, `UseObfuscationScan`.
Useful for heavily obfuscated/protected games where the AOB scan always times
out (~5 s).

### `EnableDiscoveryCache` (bool, default `false`)

Enables the encrypted discovery cache (`EncryptedCache::TryLoadSnapshot` /
`SaveSnapshot`). Aliases: `DiscoveryCache`, `UseDiscoveryCache`,
`EnableSnapshotCache`. Format version 5 validates the executable's PE timestamp
and image size, the allocator's descriptive name, the measured Malloc slot (with
one 64-byte allocation probe) and the `IConsoleManager` global's sparse-map
header before restoring
(see [DiscoveryAPI](../DiscoveryAPI/REFERENCE.md#14-encryptedcacheh--namespace-encryptedcache)).

### `VerboseLog` (bool, default `false`)

Enables verbose internal tracing (`[Discovery][Debug]` probes, layout
heuristics, retry/fallback messages). Aliases: `EnableVerboseLog`,
`VerboseLogging`, `EnableVerboseLogging`. Applied immediately via
`EngineDiscovery::SetRuntimeVerboseLogging`.

### `GraphicsBackend` (string, default `auto`)

Overlay rendering backend: `auto`, `dx12`/`d3d12`, `dx11`/`d3d11`
(case-insensitive). Aliases: `PreferredBackend`, `Backend`, `Renderer`.
`auto` prefers the backend whose DLL is already loaded, then falls back to the
other; a late `LoadLibrary` of the other backend is detected via
`LoadLibraryA/W/ExA/ExW` hooks.

### `DumpAllDeveloperCommands` (bool, default `false`)

Dumps developer/console data on console enable: `ExecCommands.txt`,
`SpawnableClasses.txt`, `AllConsoleCommands.json`, `CVARs.ini`,
`CVarReference.txt`, `UnlockedCommands.txt`, and copies of loaded DLLs. Aliases:
`DumpAllDeveloperCommand`, `DumpAllCommands`.
See [ConsoleAPI](../ConsoleAPI/README.md#dumps).

### `SDKValidation` (bool, default `false`)

Runs live-memory SDK validation after startup and after every SDK dump (same
path merges requests). Aliases: `EnableSDKValidation`, `RunSDKValidation`.
Requires `SDKPath`.

### `SDKPath` (string, default `""`)

Folder containing `SDKMetadata.json` for startup validation. Aliases:
`ValidationSDKPath`. When empty, validation requests are skipped with a log
line.

### `GenerateBuildSystem` (bool, default `false`)

After dumping the SDK, launches the build-system generator:
`python "<BuildScriptPath>" "<sdkDir>" --game-name "<Game>"`. Aliases:
`RunGenerateBuild`.

### `BuildScriptPath` (string, default `""`)

Path to `GenerateBuild.py`. Aliases: `GenerateBuildScriptPath`.
When empty, the loader auto-discovers a `Helpers` folder up to 6 levels above
the DLL and uses `<Helpers>\GenerateBuild.py` if present. The resolved value is
logged as `(auto)` or `(set)`.

### `SDKLibclangGeneration` (bool, default `false`)

Runs the libclang AST generation + verification pipeline after SDK dumping.
Aliases: `EnableSDKLibclangGeneration`, `RunSDKLibclangGeneration`.

### `LibclangPath` (string, default `""`)

Full path to `libclang.dll`. Aliases: `LibClangPath`, `SDKLibclangPath`.
Empty = auto-discover Visual Studio LLVM or a libclang package at runtime.

### `EnableLuaMods` (bool, default `true`)

Enables the Lua runtime and mod loader. Aliases: `LuaMods`, `Lua`.
Note: when the key is absent from an existing INI the default is **true**
(the default-file schema writes `false`, so a generated file starts with Lua
disabled only if never edited).

### `LuaModsPath` (string, default `""`)

Custom mods directory. Aliases: `ModsPath`, `LuaPath`.
Empty = `<directory of Avalonium.dll>\Mods\Avalonium`.

> Universal Manager profiles set this to the profile folder. For a manual
> install leave it empty so the local `Mods\Avalonium` is used.

### `EnableCppMods` (bool, default `true`)

Enables the native C++ mod loader. Aliases: `CppMods`, `NativeMods`.
A mod is a folder with `dlls\main.dll` (or `dlls\<ModName>.dll`) exporting
`AvaloniumModAbiVersion`, `start_mod` and `uninstall_mod`. The engine's
ProcessEvent hook is installed only when at least one native mod is loaded.
See [CppAPI](../CppAPI/README.md).

### `CppModsPath` (string, default `""`)

Custom native mods directory. Aliases: `CppModPath`, `NativeModsPath`, and the
shared `ModsPath` alias.
Empty = `<directory of Avalonium.dll>\Mods\Avalonium`.

### `AutoDiscoverAESKey` (bool, default `true`)

Runs `Crypto::AESKeyFinder::Scan()` at startup on the init thread (blocking;
uses the main executable path). Aliases: `FindAESKey`, `AutoAESKey`,
`DiscoverAESKey`. `ScanAsync()` also exists but `dllmain.cpp` uses the
synchronous entry point.

The scan also writes a container report (`<Executable>_AESKey.json`
plus a CWD copy) for encrypted and unencrypted games: verified pak/IoStore
indexes, per-container directory inventories, census totals and verification
failures.

### `ServerPort` (uint, default `20030`)

IPC server port. Aliases: `IPCServerPort`, `IOCPPort`, `Port`.
`0` falls back to `20030` at the call sites.

### `InitDelayMinutes` / `InitDelaySeconds` (uint, default `0`)

Initialization delay. Aliases: `InitDelayMins`/`InitDelayMin`/`InitDelayMinute`
and `InitDelaySecs`/`InitDelaySec`/`InitDelaySecond`.
The total is clamped to **3600 seconds** (1 hour); any excess is redistributed
into the minute/second fields.

### `EnableWndProcHook`: not an INI key

The WndProc hook is hardcoded enabled in `RuntimeConfig.cpp`; there is no
reader, alias or schema entry for it. The console keys and Lua hotkeys always
have a WndProc hook to arrive through.

---

## `[AggressiveDog]`: crash watchdog

`AggressiveDog::LaunchIfEnabled` reads this section from the same
`Avalonium.ini` in `DllMain`. When `Enabled=1`, it launches
`CrashHandler\AggressiveDog.exe` (PID/TID, PDB, INI, report directory and
runtime-log path are passed on the command line).

| Key | Default | Meaning |
| :--- | :--- | :--- |
| `Enabled` | `false` in generated INIs | Start the watchdog when the DLL loads |
| `LogDirectory` | `CrashHandler/Crashes` | Report/minidump/status directory; relative paths resolve beside `Avalonium.dll` |
| `DumpRegisters` | `true` | Include faulting-thread registers in text reports |
| `CatchFastFail` | `true` | Capture Windows fast-fail events as well as fatal exceptions |
| `MaxStackDepth` | `64` | Faulting-thread frame cap (clamped to 1–256) |
| `OtherThreadStackDepth` | `8` | Frames per other thread (0–64; 0 disables other-thread stacks) |
| `WriteMiniDump` | `true` | Write a `.dmp` beside each crash text report |

The watchdog attaches as a debugger, writes `AggressiveDog_*.txt` reports (and
optional `.dmp` files), `AggressiveDog_exit_*.txt` for nonzero exits without a
captured exception, and `AggressiveDog-status.log`. The launcher writes
`AggressiveDog-launch.log` beside `Avalonium.dll`. A normal exit with code `0`
does not produce a crash report.

---

## Example (default schema as generated)

```ini
;Avalonium runtime configuration

InGameGUI=false
;Enable/Disable InGame Overlay

UnrealEngineConsole=false
;Disable/Enable Unreal Engine Console

SkipPatternScan=false
;Skip the default GUObjectArray AOB pattern scan and go straight to the obfuscation scan.
;Useful for heavily obfuscated / protected games where the pattern scan always times out (~5s).

EnableDiscoveryCache=false
;Enable encrypted discovery cache system to accelerate initialization time.

VerboseLog=false
;Enable verbose internal-trace logging (Discovery [Debug] probes, layout heuristics, retry/fallback messages).
;When false the log is filtered down to high-level lifecycle and discovery results.

GraphicsBackend=auto
;Rendering backend for the overlay: auto, dx12, dx11

DumpAllDeveloperCommands=false
;Dump all developer commands and modules.

SDKValidation=false
;Run the advanced SDK validation (startup + post-dump requests for the same path are merged)

SDKPath=
;Optional folder containing SDKMetadata.json for startup validation.

GenerateBuildSystem=false
;Automatically run GenerateBuild.py after dumping the SDK

BuildScriptPath=
;Path to GenerateBuild.py. Leave empty to auto-discover from the bundled Helpers folder.

SDKLibclangGeneration=false
;Run the libclang AST generation + verification pipeline after SDK dumping.

LibclangPath=
;Full path to libclang.dll. Leave empty to auto-discover Visual Studio LLVM or libclang package.

EnableLuaMods=false
;Enable Lua scripting and mod loader runtime.

LuaModsPath=
;Custom directory for Lua mods. Leave empty for default 'Mods/Avalonium' folder next to Avalonium.dll.

EnableCppMods=true
;Enable native C++ mods. A mod is a folder with dlls/main.dll or dlls/<ModName>.dll exporting AvaloniumModAbiVersion, start_mod and uninstall_mod.

CppModsPath=
;Custom directory for native C++ mods. Leave empty for default 'Mods/Avalonium' folder next to Avalonium.dll.

AutoDiscoverAESKey=false
;Auto-discover and verify AES-256 encryption key from live memory at startup.

ServerPort=20030
;IPC / IOCP server port for tool communication.
;Used to enable external tools to communicate with Avalonium (Universal Manager, uses 20030 by default).

InitDelayMinutes=0
;Set Initialization Delay in minutes

InitDelaySeconds=0
;Set Initialization Delay in seconds

[AggressiveDog]
Enabled=0
LogDirectory=CrashHandler/Crashes
DumpRegisters=1
CatchFastFail=1
MaxStackDepth=256
OtherThreadStackDepth=8
WriteMiniDump=0
```

> The values above are the *generated-file* defaults. When a key is missing
> entirely at runtime, `EnableLuaMods` and `AutoDiscoverAESKey` default to
> **true**; the generated file writes `false` for both, so a freshly generated
> INI starts with them disabled. `EnableCppMods` defaults to **true** in both
> the read path and the generated file. `EnableWndProcHook` is always enabled
> and has no INI key.

## `injector_config.json`

```json
{
    "log_path": "E:UniversalManager\\Tools\\Avalonium\\log\\Avalonium.log",
}
```

* If the file is missing or has no `log_path`, the log is written next to the
  DLL as `Avalonium.log`.

## Derived folders

Given the log path `<logdir>\Avalonium.log` and sanitized `GameName`
(`/ \ : * ? " < > |` → `_`):

| Output | Path |
| :--- | :--- |
| Game root | `<logdir>\<GameName>` |
| Console dumps | `<logdir>\<GameName>\Console\` |
| USMap (IPC default) | `<logdir>\<GameName>\USMAP\<GameName>.usmap` |
| SDK (IPC default) | `<logdir>\<GameName>\SDK\` |
| Reflection kit | `<root>\SDK`, `<root>\Reflection` |

## See also

* [RuntimeAPI/README.md](README.md): lifecycle that consumes this config.
* [ConsoleAPI](../ConsoleAPI/README.md): `DumpAllDeveloperCommands` outputs.
* [CryptoAPI](../CryptoAPI/README.md): `AutoDiscoverAESKey` behaviour.
* [SdkAPI](../SdkAPI/README.md): validation/build/libclang options.
</content>
