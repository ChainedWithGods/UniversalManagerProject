# OverlayAPI: In-Game GUI (Dear ImGui)

The overlay subsystem hooks the game's graphics backend and renders a Dear ImGui interface:
the **Avalonium menu** (Globals / World Explorer & Inspector / Live Editor / Universal Search)
and a **Lua overlay surface** (toasts, stats HUD, canvas drawing, custom mod windows).

| Layer | Source | Purpose |
| --- | --- | --- |
| Bootstrap & lifecycle | [`OverlayDragMe.cpp`](../../Runtime/overlay/OverlayDragMe.cpp), [`OverlayDragMe.h`](../../Runtime/overlay/OverlayDragMe.h) | backend selection, MinHook init, detach |
| Menu & tabs | [`OverlayMenu.cpp`](../../Runtime/overlay/OverlayMenu.cpp), [`OverlayGlobalsView.cpp`](../../Runtime/overlay/OverlayGlobalsView.cpp) | window shell, tab routing, Globals dashboard |
| World Explorer / Inspector | [`OverlayWorldExplorer.cpp`](../../Runtime/overlay/OverlayWorldExplorer.cpp), [`OverlayInspector.cpp`](../../Runtime/overlay/OverlayInspector.cpp) | actor/object browsing, property tables |
| Live Editor | [`OverlayLiveEditor.cpp`](../../Runtime/overlay/OverlayLiveEditor.cpp), [`OverlayLiveEditor.h`](../../Runtime/overlay/OverlayLiveEditor.h) | property/function/component editing and watches |
| Universal Search | [`OverlayGlobalSearch.cpp`](../../Runtime/overlay/OverlayGlobalSearch.cpp), [`OverlayGlobalSearch.h`](../../Runtime/overlay/OverlayGlobalSearch.h) | cross-scope property/function/object search |
| Lua surface | [`LuaOverlay.cpp`](../../Runtime/overlay/LuaOverlay.cpp), [`LuaOverlay.h`](../../Runtime/overlay/LuaOverlay.h) | `Overlay` table, toasts, stats, draw commands, mod windows |
| Shared internals | [`OverlayInternal.h`](../../Runtime/overlay/OverlayInternal.h), [`OverlayHelpers.cpp`](../../Runtime/overlay/OverlayHelpers.cpp) | state structs, formatters, fonts, shared panel helpers |

The rendering itself comes from the vendored
[`Helpers/Universal-Dear-ImGui-Hook`](../../Helpers/Universal-Dear-ImGui-Hook) (DX11/DX12
ImGui hook with MinHook).

---

## 1. Lifecycle

```
DllMain(DLL_PROCESS_ATTACH)
  └─ OverlayIntegration::SetMainModule(hModule)

dllmain InitThread (after EngineDiscovery::Initialize succeeds)
  ├─ OverlayIntegration::Start()        (InGameGUI || EnableLuaMods)
  └─ … overlay init thread:
       globals::openMenuKey  = VK_INSERT
       globals::uninjectKey  = VK_DELETE
       globals::preferredBackend = ParsePreferredBackend([Features] GraphicsBackend)
       globals::SetDebugLogging([Features] VerboseLog)
       MH_Initialize()
       TryInitializeLoadedBackends()      // DX12 then DX11 (order depends on loaded modules)
         └─ on failure: InstallLoadLibraryHooks() to catch a late backend load
       mousehooks::Init()
```

The overlay is never bootstrapped before engine discovery. It starts when the
menu is enabled **or** when Lua mods are enabled (so the Lua overlay surface
can render).

Shutdown: `OverlayIntegration::Stop(processExiting)` → `ReleaseActiveBackend()`
(releases the ImGui hook; skipped when the process is exiting), `mousehooks::Remove()`,
then `MH_DisableHook/RemoveHook(MH_ALL_HOOKS)` and `MH_Uninitialize()`.

| Config key | Effect |
| --- | --- |
| `[Features] InGameGUI` (default false) | master switch for the menu window and hotkeys; Lua mods still start the overlay bootstrap |
| `[Features] GraphicsBackend` = `auto` / `dx12` / `dx11` | preferred backend; `auto` probes loaded modules |
| `[Features] VerboseLog` | overlay debug logging |

---

## 2. Backend selection

* `auto`: if `d3d12.dll` is already loaded → try **DX12 → DX11**; otherwise → **DX11 → DX12**.
* A preferred backend (`dx12`/`dx11`) is tried alone and honored; a late-loaded module of the
  other type is ignored.
* Each attempt waits up to **600 × 25 ms = 15 s** for initialization, with immediate-fallback
  detection (`ShouldFallbackImmediately`) when a D3D11 swapchain is paired with the DX12 hook
  (or vice versa). Only DX11 and DX12 backends exist; a Vulkan/OpenGL-only game never
  initializes an overlay backend.
* If no backend is loaded yet, `LoadLibraryA/W/ExA/ExW` are hooked in `kernel32.dll` and the
  overlay initializes when `d3d12.dll` / `d3d11.dll` loads later.

Log tags (written to the Avalonium log):

```
[Avalonium][Overlay] Starting overlay bootstrap.
[Avalonium][Overlay] Attempting DX12 hook. | DX11 hook.
[Avalonium][Overlay] DX12 initialized. Overlay hidden by default; press Insert or F5 to open.
[Avalonium][Overlay] Non-DX12 swapchain detected; triggering immediate fallback to DX11.
[Avalonium][Overlay] Detected runtime load of DX11.
[Avalonium][Overlay] Overlay bootstrap finished. Active backend=DX12.
[Avalonium][Overlay] Stopping overlay.
```

---

## 3. Input & hotkeys

| Key | Action |
| --- | --- |
| **Insert** | toggle menu (default `openMenuKey`) |
| **F5** | toggle menu (always checked) |
| `Overlay.SetMenuKey("X")` | rebind `openMenuKey` to another key |
| **Delete** | detach/uninject, **only while the menu is open** (`menu::isOpen && key pressed`) |

Detach sequence (`PrepareForGracefulDetach` → `UninjectThread`):
close menu → `MouseDrawCursor = false` → `ReleaseCapture()` → `ClipCursor(nullptr)` →
post `WM_MOUSEMOVE`/`WM_SETCURSOR` to the game window → `OverlayIntegration::Stop()` →
`FreeLibraryAndExitThread(globals::mainModule)`.

`Uninject()` is idempotent (an in-progress flag guards a second call).

---

## 4. The Avalonium menu

Top bar (via `DrawCompactTopBar`) switches between four views (`RuntimeMainView`):

| Tab | View enum | Contents |
| --- | --- | --- |
| **Globals** | `RuntimeMainView_Globals` | runtime health dashboard |
| **Explorer / Inspector** | `RuntimeMainView_WorldExplorer` | two-pane actor list + inspector |
| **Live Editor** | `RuntimeMainView_LiveEditor` | property/function/component editing, watches |
| **Universal Search** | `RuntimeMainView_GlobalSearch` | cross-scope search |

### 4.1 Globals tab

Six metric blocks: **UE Version**, **Renderer**, **VTable** (ProcessEvent slot), **Object
Count**, **Discovery** state, **Pointers** (`n/6 resolved`). The pointer table shows
`GUObjectArray`, `GNames`, `GWorld`, `GMalloc`, `GNatives`, `GUObjectHashTables` with per-row
**Copy** buttons and a **Copy all** button that puts a `key=value` block on the clipboard.
Unresolved pointers render `<not found>` and disable copying.

### 4.2 World Explorer / Inspector

* Source modes: **Persistent Level**, **All Actors**, **All Objects** (`WorldExplorerSourceMode`).
* Entries carry `Name`, `ClassName`, `SearchText`, flags (`kWorldExplorerFlag_IsActor`,
  `kWorldExplorerFlag_InPersistentLevel`).
* Filtering, sorting, result ceilings (`MaxResults`, default 1024, minimum 128 measured
  entries) and truncation reporting are handled by the shared helpers in
  `OverlayInternal.h`.
* The inspector pane shows the selected object's identity, class, outer, inheritance chain and
  a property table (up to `kMaxInspectorProperties = 1024` properties).

### 4.3 Live Editor

* Tabs: **Properties**, **Function Caller**, **Watches**, **Components**, plus breadcrumbs.
* `LiveEditorState` holds the current object/class, filters, inherited-member toggles, live
  auto-refresh interval (default 0.1 s), editing buffers and a navigation history.
* Watch items support live value display, editing, history and logging to file. Persistence
  (`SaveWatchesToDisk` / `LoadWatchesFromDisk`) stores the watch definition (object path,
  property, type, flags) but not history, invocation counters or last values.
* Function caller builds parameter buffers per reflected parameter (in/out/return), reports
  `LastResult`, success flag and execution time.

### 4.4 Universal Search

* Scopes: Present Level, All Actors, All Objects, All Objects + CDOs, Selected Hierarchy.
* Matches properties, functions and objects; optional value matching, regex, address search
  and class-hierarchy inclusion; type categories (numbers, booleans, objects, strings, structs,
  functions).
* Results carry the live value, offset and a jump-to-object action; editing of a result row is
  supported inline.

### 4.5 Lua overlay rendering

`menu::RenderOverlay()` calls `LuaOverlayManager::Render()` every frame, which draws (in
order): draw callbacks → frame draw commands → stats HUD → toasts → custom mod windows.
`m_PersistentDrawCmds` is stored and cleared but is not drawn, so the documented
"persistent" canvas layer does not currently render.

---

## 5. Lua surface (summary)

Registered as the global `Overlay` table. All members accept both `.` and `:` call styles
(the runtime drops a leading `Overlay` receiver automatically).

| Group | Members |
| --- | --- |
| Notifications | `Notify`, `Toast`, `ClearNotifications` |
| Stats HUD | `SetStat`, `RemoveStat`, `ClearStats`, `ShowStats`, `IsStatsVisible`, `SetStatsAnchor`, `SetStatsPosition` |
| Canvas | `DrawText`, `DrawRect`, `DrawLine`, `DrawCircle`, `DrawProgressBar`, `RegisterDrawCallback` |
| Windows | `RegisterWindow` + ImGui widgets `Text`, `TextColored`, `Button`, `Checkbox`, `SliderFloat`, `SliderInt`, `Separator`, `SameLine` |
| Menu | `IsOpen`, `Open`, `Close`, `Toggle`, `SetMenuKey` |

Builtin-mod aliases (`Notify`, `Toast`, `ToggleMetrics`, …) are documented in
[LuaAPI/MODS.md](../LuaAPI/MODS.md#6-builtin-mods).

Detailed structures, colors, anchors and per-function semantics:
[REFERENCE.md](REFERENCE.md).

---

## 6. Threading

* Rendering runs on the backend's present thread; Lua callbacks registered as draw callbacks or
  windows run there too (inside the overlay mutex).
* `LuaOverlayManager` state is guarded by `std::recursive_mutex m_OverlayMutex`.
* Mods registering draw callbacks should keep them cheap, they execute every frame.
* Notifications and stats set from other threads are safe (mutex-guarded) but are consumed by
  the renderer.

---

## Cross-references

* [REFERENCE.md](REFERENCE.md): every struct, member and draw command.
* [LuaAPI/MODS.md](../LuaAPI/MODS.md): builtin mod globals (`UniversalHUD`, `UniversalOverlay`).
* [RuntimeAPI/CONFIG.md](../RuntimeAPI/CONFIG.md): `InGameGUI`, `GraphicsBackend`, `VerboseLog`.
* [RuntimeAPI/README.md](../RuntimeAPI/README.md): startup/shutdown ordering.
* [DiscoveryAPI/README.md](../DiscoveryAPI/README.md): the object index powering the explorer.
