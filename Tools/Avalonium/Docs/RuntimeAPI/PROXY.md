# RuntimeAPI: Proxy Bootstrap (`dwmapi.dll`)

Avalonium can be injceted without an external injector by shipping a **proxy
`dwmapi.dll`** next to the game executable. Windows resolves `dwmapi.dll` from
the application directory first, so the game loads the proxy, the proxy loads
the real system DLL and forwards every call, then loads `Avalonium.dll` on a
background thread.

Source: `Runtime/proxy/dllmain_proxy.cpp`, `dwmapi.asm`, `dwmapi.def`,
`proxy_stub.cpp`. CMake target `dwmapi_proxy` (MSVC + MASM only), output
`dwmapi.dll`.

---

## `dllmain_proxy.cpp` behaviour

On `DLL_PROCESS_ATTACH`:

1. `DisableThreadLibraryCalls`.
2. Opens `dwmapi_proxy.log` next to itself (`_wfsopen(L"w+t", _SH_DENYNO)`).
3. Loads `%SystemRoot%\System32\dwmapi.dll` with `GetSystemDirectoryW` +
   `LoadLibraryW`.
4. `setup_functions()` resolves all **113** real exports into the `mProcs[113]`
   table (ordinals 103–198 are resolved by `MAKEINTRESOURCEA`, named exports by
   name). A count of resolved entries is logged.
5. Starts `ManagerLoaderThread`, which tries to load, in order:
   * `Avalonium.dll`
   * `Avalonium-Proxied.dll`

   from its own directory with `LOAD_WITH_ALTERED_SEARCH_PATH`, logging the
   attempt/outcome.
6. Starts `LogTailThread`, which tails `<proxy directory>\Avalonium.log` into
   `dwmapi_proxy.log`: so a single file shows both proxy and runtime output.
   The tail path is fixed; an `injector_config.json` that moves the runtime log
   elsewhere is not consulted, so the proxy tail stays empty in that setup.

On `DLL_PROCESS_DETACH`: stop tail thread (≤2 s), `FreeLibrary` the real
dwmapi, log and close.

### Why it is needed

* No external injector: normal game launch loads it.
* `LOAD_WITH_ALTERED_SEARCH_PATH` makes `Avalonium.dll`'s own dependencies
  resolve from the game folder first.
* If `Avalonium.dll` is missing, the proxy still works as a complete dwmapi
  forwarder, the game runs normally and `dwmapi_proxy.log` says what happened.

### Log

`dwmapi_proxy.log` next to the proxy contains timestamped lines for: attach
(pid + path), system DLL load result, resolved-export count, each
`Trying to load ...` / `Loaded ... successfully (handle=...)` attempt, and every
line tailed from `Avalonium.log`.

---

## Export forwarding

`dwmapi.def` exports **113 functions**:

* ordinals `@100`–`@212`;
* 60+ named exports (`DwmExtendFrameIntoClientArea`, `DwmGetWindowAttribute`,
  `DwmSetWindowAttribute`, `DwmRegisterThumbnail`, `DwmFlush`, ...);
* unnamed ordinals exposed as `ordinal103`, `ordinal104`, ... to keep the JDK
  ordinal layout intact.

`dwmapi.asm` implements each as a tiny MASM jump:

```asm
f0 proc
    jmp mProcs[8*0]
f0 endp
```

`mProcs` is defined in `dllmain_proxy.cpp` and filled from the real DLL at
attach. A call that arrives before `setup_functions()` (or when the real DLL
failed to load) jumps to null; the proxy logs the failure so this is visible.

### Adding a new export

1. Add the slot to `mProcs` and resolve it in `setup_functions()`.
2. Add a `jmp` stub in `dwmapi.asm`.
3. Add the line to `dwmapi.def` with the next ordinal.

---

## Generic proxy stubs (`proxy_stub.cpp`)

The same file also contains an older, simpler proxy implementation that
supports several host DLLs through compile definitions:

| Macro | Forwards |
| :--- | :--- |
| `PROXY_DWMAPI` | the 18 most common `Dwm*` APIs via `GetProcAddress` thunks |
| `PROXY_DINPUT8` | `DirectInput8Create`, `DllCanUnloadNow`, `DllGetClassObject`, ... |
| `PROXY_DXGI` | `CreateDXGIFactory*`, `DXGIDeclareAdapterRemovalSupport`, ... |
| `PROXY_D3D12` | `D3D12CreateDevice`, `D3D12GetDebugInterface`, ... |
| `PROXY_D3D11` | `D3D11CreateDevice*`, `D3DPerformance_*`, ... |
| `PROXY_D3D9` | `Direct3DCreate9*`, `D3D9GetSWInfo`, ... |

It resolves the host function lazily per call, returns
`HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND)` when missing, and spawns
`ManagerLoaderThread` to load `Avalonium.dll`/`Avalonium-Proxied.dll`. This
stub is kept as a template for other proxy targets; the shipped target uses the
`dwmapi.def`/`.asm` pair because it must preserve the exact export/ordinal
layout of the system DLL.

---

## Version resource

The proxy target compiles `Runtime/proxy/proxy_version.rc` (definitions in
`proxy_version.h`). It sets the file properties of the shipped `dwmapi.dll` to
v`1.0.0.0`, product `dwmapi+Avalonium`, description `UE4.23-UE5.8`, company
`Universal Manager project`: so the binary is identifiable as Avalonium's
loader rather than a system component.

---

## Uninstall / disable

* Delete or rename `dwmapi.dll` next to the game → no injection, game works
  normally.
* Delete `dwmapi.dll`, `Avalonium.dll`, `Avalonium.ini`, `Avalonium.pdb`
  and `Mods/` for a full removal.
* `Avalonium.log` and `dwmapi_proxy.log` are diagnostics only and can be
  deleted at any time.

## See also

* [RuntimeAPI/README.md](README.md#lifecycle): what happens after the proxy
  loads the runtime DLL.
* `Runtime/DLL/Readme.md`: end-user installation guide.
</content>
