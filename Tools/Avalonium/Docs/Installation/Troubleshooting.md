# Troubleshooting

Check the log first: `Avalonium.log` beside `Avalonium.dll`, or the log folder of
the Universal Manager profile. The proxy's `dwmapi_proxy.log` tails the same
output.

## Runtime does not start

| Symptom | Check |
| :--- | :--- |
| No `[Avalonium] Initialization successful` line | `Avalonium.dll` and `dwmapi.dll` must sit beside the shipping executable (`Binaries\Win64`), not beside the launcher |
| The game starts but nothing loads | another proxy already owns `dwmapi.dll` (UE4SS, for example); only one can sit in that folder |
| The log is missing or empty | the log folder is not writable (the profile folder, or the runtime folder for a manual install) |
| Initialization aborts with an unsupported version | the runtime supports UE 4.23 through 5.8; older engines are refused on purpose |

Universal Manager path: press Inject Avalonium only after the game has reached
the main menu, and check the Avalonium Runtime Engine tab for its status.

## Control port

| Symptom | Check |
| :--- | :--- |
| `ERROR: Connection refused. is the DLL injected? Maybe the game is not running?` | the runtime is not in the process, or `ServerPort` in `Avalonium.ini` changed |
| `INIT` answers `ERROR: <reason>` | discovery failed; the log line above explains why |
| Timeouts on long commands | raise the timeout: `iocp_client.py -t 600 ...`, `SDKOrchestrator.py --timeout 600` |
| Port is taken | another process owns 20030; change `ServerPort` and use the matching port in scripts |

## SDK generation

| Symptom | Check |
| :--- | :--- |
| Nothing is written | the game must be running with the runtime initialized; run [the runtime check](VerifyRuntime.md) |
| `failed: N` above the summary | the log names the files that failed; free space and permissions are the usual causes |
| No `Reflection` folder | it is written beside the SDK folder, not inside it |
| Wrappers report missing functions at runtime | the SDK is from another game build; regenerate after the patch |
| Generation takes very long on a big game | expected; raise the socket timeout and let it finish |

## C++ mods

| Symptom | Check |
| :--- | :--- |
| No `[CppMod]` lines at all | `EnableCppMods=true` and the runtime loaded |
| `[CppMod] Mods directory does not exist` | `CppModsPath` points at a missing folder; leave it empty for `Mods\Avalonium` |
| Mod folder is ignored | it needs `dlls\main.dll`, `dlls\<FolderName>.dll`, or `<FolderName>.dll` in the folder itself |
| `is disabled by enabled.txt` | remove the file, or make it empty, `1`, `true`, `yes` or `on` |
| `reports mod ABI N; this runtime accepts ABI M` | the mod was built against different headers; rebuild it against this runtime's SDK |
| `has no start_mod/uninstall_mod exports` | the DLL is not a mod or was built without the exports |
| Loaded, but the menu or key does nothing | the mod was built against a newer API version than this runtime. `AvaloniumGetModApi` returns null, so every API pointer is null; the mod's debug output says `the runtime does not answer this API version`. Update the runtime or rebuild the mod against its headers |
| A copied DLL did not change anything | the game was running and the old file was locked; close the game and copy again |
| The game crashes while loading the mod | look for `[CppMod]` lines in the log, and place `Avalonium.pdb` beside the DLL for readable stacks |

Key binds run on the input thread. Engine work belongs in `on_update` or a
`RC::RunOnGameThread` task; a hotkey that opens a window but touches engine
objects directly is a crash waiting to happen.

## Overlay and input

| Symptom | Check |
| :--- | :--- |
| A C++ mod's window never appears | the overlay bootstrap starts with `EnableCppMods`; look for `[Overlay]` lines in the log |
| Overlay works in one game and not another | `GraphicsBackend=auto` covers D3D11 and D3D12; other renderers are unsupported |
| The mouse stays with the game while a window is open | the mod did not call `RC::ImGui::SetInputActive(id, true)` for its render callback |
| The game stops receiving input after closing a mod window | the mod did not set input active back to `false` when the window hid |

## Before reporting a problem

- Are the two startup lines in the log present?
- Does `python iocp_client.py VERSION` answer?
- Does the log contain `[CppMod]` lines, and what do they say?
- Was the game restarted after the last DLL copy?

The API reference ([CppAPI](../CppAPI/README.md)) and the runtime documents
([RuntimeAPI](../RuntimeAPI/README.md)) cover the behavior behind each line.
