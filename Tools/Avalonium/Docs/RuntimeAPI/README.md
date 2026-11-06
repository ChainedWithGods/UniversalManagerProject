# RuntimeAPI: `Runtime/api`, `Runtime/core/dllmain.cpp`, `Runtime/AggressiveDog`, `Runtime/proxy`

The RuntimeAPI is everything an external process can use to *talk to* Avalonium:

1. **C exports**: `Avalonium.dll` exports 34 flat C functions for tools that
   load the DLL directly, including the versioned C++ mod API and generated SDK
   runtime tables (see [EXPORTS.md](EXPORTS.md)).
2. **IPC server**: a loopback TCP command server (`IOCPServer`) started after
   discovery, speaking a line-based text protocol on port `20030`
   (see [IPC.md](IPC.md)).
3. **Configuration**: `Avalonium.ini` drives every subsystem, including the
   crash watchdog (see [CONFIG.md](CONFIG.md)).
4. **Bootstrap**: `dwmapi.dll` proxy loading and the `DllMain` startup thread
   (see [PROXY.md](PROXY.md) and [Lifecycle](#lifecycle) below).
5. **Diagnostics**: the structured session log and the external AggressiveDog
   watchdog (see [Logging](#logging) and [CONFIG.md](CONFIG.md#aggressivedog--crash-watchdog)).

---

## Lifecycle

### In-process bootstrap (`Runtime/core/dllmain.cpp`)

```text
DllMain(DLL_PROCESS_ATTACH)
 ├─ g_Module = hModule
 ├─ OverlayIntegration::SetMainModule(hModule)
 ├─ DisableThreadLibraryCalls(hModule)
 ├─ AggressiveDog::LaunchIfEnabled(hModule)     // [AggressiveDog] Enabled=1
 └─ CreateThread(InitThread)
```

`InitThread` then, in order:

1. Opens the structured log session (`BeginLogSession`, section `runtime`).
2. Loads `RuntimeConfig::Load(hModule)` (reads/regenerates `Avalonium.ini` next
   to the DLL).
3. Applies `InitDelayMinutes` / `InitDelaySeconds`.
4. Calls `EngineDiscovery::Initialize(g_State, skipConsoleManager)`. If this
   fails, it sleeps 5 s and retries once (unless the engine version is
   unsupported).
5. On success:
   * starts `SDKValidation::StartSDKValidationThread` when `SDKValidation=true`
     and `SDKPath` is set;
   * starts `IOCPServer` on `ServerPort`;
   * finds the game window and installs the WndProc hook
     (`EnableWndProcHook`, hardcoded enabled);
   * runs the **synchronous** `Crypto::AESKeyFinder::Scan()` when
     `AutoDiscoverAESKey=true`;
   * prewarms and enables the in-game console synchronously when
     `UnrealEngineConsole=true`;
   * creates `InputAndConsoleThread` only when the hook or console was not
     completed synchronously (up to 200 retries, ≈20 s);
   * starts the overlay bootstrap when `InGameGUI=true` **or**
     `EnableLuaMods=true`;
   * initializes `LuaManager` and loads mods when `EnableLuaMods=true`.

The encrypted discovery cache is saved/restored inside
`EngineDiscovery::Initialize`, not in `dllmain.cpp`.

A second thread (`InputAndConsoleThread`) retries the game-window lookup,
installs the `WndProc` hook and enables the in-game console for up to
200 attempts (≈20 s).

### Shutdown (`DLL_PROCESS_DETACH`)

```text
LuaManager::Get().Shutdown()
EndLogSection("lua")
g_IOCPServer->Stop(); delete
ConsoleEnabler::RemoveWndProcHook(g_State)
OverlayIntegration::Stop(processExiting)   // skips D3D release when the process exits
Shutdown()                                 // resets g_State
WaitForSingleObject(initThread, 3000)
WaitForSingleObject(consoleThread, 3000)
EndLogSession("process detached")
```

---

## The exported C surface

All exports are declared in `Runtime/api/UEInternalAPI.h` behind
`AVALONIUM_EXPORTS` (so consumers can also include the header with
`dllimport`). The built DLL exports exactly the symbols in
[EXPORTS.md](EXPORTS.md), the set is verified from the release build's export
table.

Typical consumer flow:

```cpp
HMODULE h = LoadLibraryW(L"Avalonium.dll");
auto Initialize = (bool(*)())GetProcAddress(h, "Initialize");
auto GetStatus  = (const char*(*)())GetProcAddress(h, "GetStatus");
auto GetObjects = (int(*)(void**, int))GetProcAddress(h, "GetAllActors");

Initialize();
printf("%s\n", GetStatus());

std::vector<void*> actors(20000);
int n = GetObjects(actors.data(), (int)actors.size());
```

Notes that apply to the whole export surface:

* `Initialize()` is idempotent (`g_State.bInitialized` short-circuit) and
  mutex-protected. Calling it when `DllMain` already initialized the runtime is
  safe and instant.
* Pointer getters (`GetGUObjectArray`, `GetFNamePool`, `GetGWorld`, `GetGMalloc`,
  `GetGNatives`, `GetGUObjectHashTables`) return addresses **inside the target
  process**. They are only meaningful to code already running in that process
  (an injected companion DLL), not to an out-of-process tool.
* The `const char*` getters (`GetObjectName`, `GetObjectFullName`,
  `GetInheritanceChain`, `GetStatus`, `GetDetectedVersion`, `GetLastErrorMsg`)
  return pointers into static scratch buffers. The buffer is overwritten by the
  next call on any thread, copy the string immediately, or use the safe
  variants.
* `Get*Safe` variants copy into a caller-supplied buffer, NUL-terminate, and
  return the copied length.
* Enumeration functions never allocate: they write up to `maxCount` pointers and
  return the number written.

---

## IPC server (`Runtime/api/IOCPServer.cpp`)

The server is a minimal blocking `select()` loop, deliberately one connection at
a time:

| Property | Value |
| :--- | :--- |
| Bind address | `INADDR_LOOPBACK` (127.0.0.1) only |
| Socket | TCP, `SO_REUSEADDR`, `TCP_NODELAY` |
| Buffers | 1 MiB send/recv |
| Timeouts | 10 s recv / 10 s send |
| Receive buffer | 64 KiB per request |
| Framing | one command per connection; response written, socket closed |
| Port | `ServerPort` from `Avalonium.ini`, default `20030` |

There is no authentication or encryption: the port is loopback-only by design.

See [IPC.md](IPC.md) for the complete command reference with request/response
examples, including the `DUMP` shared-memory protocol.

---

## `EngineState`: the shared runtime state

`EngineState` (`Runtime/core/Types.h`) is the struct; the single `g_State`
instance is defined in `Runtime/api/UEInternalAPI.cpp` and consumed through
`extern` declarations. It is the source of truth created by discovery and read
by every other subsystem. Highlights relevant to RuntimeAPI consumers:

| Field | Meaning |
| :--- | :--- |
| `UEMajor` / `UEMinor` | detected engine version, e.g. `5 / 3` |
| `GameName[256]` | executable/game name used for output folders |
| `GUObjectArray`, `GNames`, `GWorld`, `GMalloc`, `GNatives`, `GEngine`, `IConsoleManager` | discovered engine globals |
| `GMallocLocation` / `GMallocName` / `GMallocMallocSlot` | allocator global address, descriptive name and measured Malloc vtable slot |
| `IConsoleManagerLocation` | module address of the pointer global that holds the console manager |
| `GWorldPtr` | module address of the world pointer (re-read for the live world) |
| `ProcessEvent_VTableIdx` | resolved `UObject::ProcessEvent` vtable slot, `-1` if unknown |
| `bInitialized`, `bGlobalsFound`, `bFunctionsFound`, `bCoreClassesFound`, `bOffsetsDiscovered`, `bIndexBuilt` | discovery progress flags |
| `bUnsupportedVersion` | set when UE < 4.23 is detected |
| `LastError[1024]` | human-readable reason for the last discovery failure |
| `IndexMutex`, `ReflectionCacheMutex` | locks guarding the object index / reflection caches |

The full field reference lives in
[DiscoveryAPI/REFERENCE.md](../DiscoveryAPI/REFERENCE.md#16-enginestate).

---

## Configuration surface

`RuntimeConfig` (`Runtime/core/RuntimeConfig.cpp`) loads `Avalonium.ini` **next
to `Avalonium.dll`**, accepts alias spellings, `[Features]`/`[Init]` sections or
flat keys, and rewrites the file from its schema on every load: known keys keep
their values, missing keys get defaults, and keys/comments outside the schema
are dropped. `EnableWndProcHook` is hardcoded to `true` and is not an INI key.

A second file, `injector_config.json`, is read by the logger only:

```json
{
  "log_path": "E:\\...\\Avalonium.log",
}
```

When `log_path` is absent the log defaults to `Avalonium.log` next to the DLL.

Full key reference: [CONFIG.md](CONFIG.md).

---

## Logging

`LogSession` (`Runtime/core/LogSession.cpp`) owns the structured session report:
80-column banner, lifecycle sections (`config`, `runtime`, `discovery`, `crypto`,
`console`, `overlay`, `lua`, `level_transition`), severity filtering, thread
labels, session facts and a closing summary. It rotates the log through five
files (`.1.log` … `.5.log`) and mirrors every line to
`OutputDebugStringA`. `EngineDiscovery::WriteLogLine` (`Runtime/core/Logger.cpp`)
is the raw sink used by subsystems; `Log(fmt, ...)` is its `printf`-style
variant. Both honour `VerboseLog` / `verbose_log`.

---

## Source map

| File | Role |
| :--- | :--- |
| `Runtime/api/UEInternalAPI.h/.cpp` | exported C API + `g_State` definition |
| `Runtime/api/IOCPServer.h/.cpp` | TCP command server and command dispatch |
| `Runtime/core/dllmain.cpp` | `DllMain`, init/console threads, `SetLogPath`, shutdown order |
| `Runtime/core/RuntimeConfig.h/.cpp` | INI schema, loading, aliasing, regeneration |
| `Runtime/core/LogSession.h/.cpp` | structured session report, sections, facts, rotation |
| `Runtime/AggressiveDog/Launcher.cpp` | watchdog launch from `DllMain` |
| `Runtime/AggressiveDog/AggressiveDog.cpp` | external watchdog process |
| `Runtime/proxy/dllmain_proxy.cpp` | dwmapi proxy that loads `Avalonium.dll` |
| `Runtime/proxy/dwmapi.asm`, `dwmapi.def` | 113 export forwarders for the proxy |
| `Runtime/proxy/proxy_stub.cpp` | generic proxy stub (dwmapi/dinput8/dxgi/d3d12/d3d11/d3d9) |
| `Runtime/utils/version.rc` | `Avalonium.dll` Windows version resource (1.0.0.0, "Avalonium") |
| `Runtime/proxy/proxy_version.h/.rc` | `dwmapi.dll` version resource ("dwmapi+Avalonium", "Avalonium loader") |

* [EXPORTS.md](EXPORTS.md): every exported function with signature and semantics.
* [IPC.md](IPC.md): every IPC command, response format and examples.
* [CONFIG.md](CONFIG.md): `Avalonium.ini` and `injector_config.json` reference.
* [PROXY.md](PROXY.md): proxy bootstrap details and export forwarding.
</content>
