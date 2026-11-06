# OverlayAPI: Reference

Complete reference for the overlay subsystem: lifecycle API, manager state, Lua `Overlay`
table semantics and draw-command structures.

---

## 1. `OverlayIntegration`: bootstrap API

```cpp
// Runtime/overlay/OverlayDragMe.h
namespace OverlayIntegration {
void SetMainModule(HMODULE module);
bool Start();
void Stop(bool processExiting = false);
}
void Uninject();   // global (same TU)
```

| Function | Behavior |
| --- | --- |
| `SetMainModule(hModule)` | Stores the Avalonium module handle used for detach (`globals::mainModule`) |
| `Start()` | Idempotent (`g_Started` CAS). Spawns `OverlayInitThread`; returns `true` if already started. Fails (returns `false`) only when the thread cannot be created |
| `Stop(processExiting)` | Sets `g_StopRequested`, releases the active backend (skipped when `processExiting=true`, because the game has already released its D3D device/swapchain), removes mouse hooks, disables/removes all MinHook hooks and uninitializes MinHook. Safe to call twice |
| `Uninject()` | Guards with `g_UninjectInProgress`; spawns `UninjectThread` which gracefully detaches and calls `FreeLibraryAndExitThread(globals::mainModule, 0)` |

`Start()` is called both from startup and on demand, `ShowNotification`, `SetStat` and
`SetStatsVisible(true)` call it so that a toast/stat set before the overlay bootstrapped still
brings the overlay up.

### Bootstrap thread (`OverlayInitThread`)

1. Abort if `g_StopRequested`.
2. `globals::openMenuKey = VK_INSERT`, `globals::uninjectKey = VK_DELETE`,
   `globals::preferredBackend = ParsePreferredBackend([Features] GraphicsBackend)`,
   `globals::SetDebugLogging([Features] VerboseLog)`, `menu::isOpen = false`.
3. `MH_Initialize()` (accepts `MH_ERROR_ALREADY_INITIALIZED`).
4. `TryInitializeLoadedBackends()`; if no backend initialized → `InstallLoadLibraryHooks()`.
5. `mousehooks::Init()`.
6. Log `Overlay bootstrap finished. Active backend=<None|DX11|DX12>.`

### Backend parsing

`ParsePreferredBackend` accepts (case-insensitive): `dx12`/`d3d12` → DX12,
`dx11`/`d3d11` → DX11, anything else → `None` (auto).

Auto order: `d3d12.dll` loaded → DX12 first; otherwise DX11 first. Each attempt polls
`IsInitialized` up to 600 × 25 ms, with `ShouldFallbackImmediately()` breaking out early for
incompatible swapchains. Late `LoadLibrary*` of the other backend is ignored once one is
active (`Ignoring late load of X because Y is already active.`).

### Detach sequence (`PrepareForGracefulDetach`)

```
menu::isOpen = false
ImGui::GetIO().MouseDrawCursor = false
ReleaseCapture(); ClipCursor(nullptr)
PostMessage(mainWindow, WM_MOUSEMOVE, 0, 0)
PostMessage(mainWindow, WM_SETCURSOR, mainWindow, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE))
Sleep(150)
```

---

## 2. Menu state

```cpp
namespace menu {
bool isOpen = false;
void PrepareContext();               // configure fonts for the current ImGui context
void RememberStateForTransition();   // remember open state across a backend transition
void SetRestoreDelayFrames(UINT);    // frames to wait before restoring
void RestoreStateAfterTransition();
void OnFrame();                      // MouseDrawCursor = isOpen
void RenderOverlay();                // → LuaOverlayManager::Render()
void Init();                         // window + tab dispatch
}
```

`Init()` honors `RuntimeConfig::Get().EnableInGameGui`; applies `StyleColorsDark()` once per
ImGui context, fixed font scale, window size 980×640 (min 900×580), position (40,40),
`ImGuiWindowFlags_NoCollapse`, title **"Avalonium"**.

Tab dispatch by `gActiveMainView`:

| View | Draw call |
| --- | --- |
| `RuntimeMainView_LiveEditor` | `LiveEditor::DrawLiveEditorTab()` |
| `RuntimeMainView_GlobalSearch` | `GlobalSearch::DrawGlobalSearchTab()` |
| `RuntimeMainView_WorldExplorer` | `DrawWorldExplorerInspectorTab()` |
| `RuntimeMainView_Globals` (default) | `DrawGlobalsTab()` |

---

## 3. `LuaOverlayManager`

```cpp
namespace LuaEngine { namespace Overlay {
class LuaOverlayManager {
public:
    static LuaOverlayManager& Get();
    void Render();
    void ShowNotification(const std::string& title, const std::string& message = "",
                          float duration = 3.5f, const std::string& type = "info");
    void ClearNotifications();
    void SetStat(const std::string& key, const std::string& value, uint32_t color = 0);
    void RemoveStat(const std::string& key);
    void ClearStats();
    void SetStatsVisible(bool visible);
    bool IsStatsVisible() const;
    void SetLiveMetricsEnabled(bool enabled);
    bool IsLiveMetricsEnabled() const;
    void SetStatsAnchor(const std::string& anchor);
    void SetStatsPosition(float x, float y);
    void AddDrawText(...); void AddDrawRect(...); void AddDrawLine(...);
    void AddDrawCircle(...); void AddDrawProgressBar(...);
    void RegisterDrawCallback(int callbackRef);
    void UnregisterDrawCallback(int callbackRef);
    void RegisterWindow(const std::string& title, int callbackRef);
    void UnregisterWindow(const std::string& title);
    void ClearAll(lua_State* L);
    void SetLuaState(lua_State* L);
};
}}
```

All state mutations take `m_OverlayMutex` (recursive).

### Notification semantics

`ShowNotification(title, message, duration, type)`:

* `type` is lowercased; unknown types fall back to `Info`; known: `info`, `success`, `ok`,
  `warning`, `warn`, `error`, `err`.
* When the **title** is empty and the message is not, the message becomes the
  title. Otherwise the title is derived from the type
  (`Success`/`Warning`/`Error`/`Notification`; `info` yields `Notification`, not
  `Info`).
* Duration auto-converts: a value `>= 50` is treated as milliseconds (`2500 → 2.5 s`);
  values `<= 0.2` fall back to 3.5 s.
* New toasts start with `SlideOffset = 60` (slide-in animation).
* Logs `[Overlay] Toast (<type>): <title>, <message>`.
* Calls `OverlayIntegration::Start()` so the overlay exists to display it.

### Stats HUD

* Built-in live metrics (`m_LiveMetricsEnabled`, default true): **FPS** (color-coded
  ≥55 green / ≥28 yellow / else red) and **Frame Time** are read every frame; **Memory**
  (working set / total physical), **Objects** (object-array count), **CPU** (system-wide),
  **GPU** (3D engine utilization of the render adapter) and **GPU Temp** (green <70 C /
  yellow <85 C / else red) are cached and refreshed at most every 250 ms.
* CPU, GPU and GPU Temp come from Windows itself: `GetSystemTimes`, the `GPU Engine` PDH
  counters and `D3DKMT_ADAPTER_PERFDATA`. That covers NVIDIA, AMD and Intel through their
  WDDM drivers. No vendor SDK, no kernel driver: when a driver does not report a value,
  the row is simply absent.
* Custom stats (`SetStat`) keep insertion order (`m_NextStatOrder`) and an optional color
  (`0` = default green).
* Layout: fixed width 240 px, line height 20 px, padding 12 px, title "Avalonium Overlay".
* Anchors: `topleft`/`top_left`, `topright` (default), `bottomleft`/`bottom_left`,
  `bottomright`/`bottom_right`; margin 24 px. `SetStatsPosition(x, y)` overrides the anchor
  (any `SetStatsAnchor` call resets the custom position to `-1`).
* `SetStatsVisible(true)` also calls `OverlayIntegration::Start()`.

### Render order (`Render`)

1. Draw callbacks (`m_DrawCallbackRefs`).
2. `RenderCanvas`: copies and **clears** `m_FrameDrawCmds`, draws them. The
   `m_PersistentDrawCmds` list is stored/cleared by the manager but is never drawn.
3. `RenderStatsHUD`.
4. `RenderToasts`: card list top-right, slide-in, progress bar for remaining time.
5. `RenderCustomWindows`: ImGui windows (default 360×280) invoking the registered Lua
   callback inside `LuaManager::GetLuaMutex()` + overlay mutex; a Lua error renders inline
   (`Lua Window Error: …`).

---

## 4. Lua `Overlay` table

Every member is registered through a thunk that drops a leading `Overlay` table argument, so
`Overlay.Notify(...)`, `Overlay:Notify(...)` and `Overlay["Notify"](...)` are equivalent.

### 4.1 Colors

`ParseColor(L, index, default)` accepts:

| Form | Example | Result |
| --- | --- | --- |
| integer | `0xFF2ECC71` | `IM_COL32` packed value used as-is |
| `#RRGGBB` / `#RRGGBBAA` string | `"#2ECC71"` | parsed as hex; `RRGGBB` gains `FF` alpha |
| table `{r, g, b[, a]}` | `{0.18, 0.8, 0.44}` | floats ≤ 1 are scaled by 255; alpha defaults to 1 |
| omitted/nil | N/A | the function's default |

### 4.2 `Notify`

```
Overlay.Notify(title)
Overlay.Notify(title, message)
Overlay.Notify(title, type)              -- "success"/"warning"/"error"/"info"/"ok"/…
Overlay.Notify(title, durationSeconds)   -- number
Overlay.Notify(title, message, duration)
Overlay.Notify(title, duration, type)
Overlay.Notify(title, message, duration, type)
```

Default duration 3.5 s. Type strings that are recognized as notification types are treated as
types; anything else is a message.

### 4.3 `Toast`

```
Overlay.Toast(message)
Overlay.Toast(message, durationSeconds)
Overlay.Toast(message, type)
Overlay.Toast(message, duration, type)
```

Default duration 3.0 s. Shows with the message as the title.

### 4.4 Stats

| Call | Behavior |
| --- | --- |
| `Overlay.SetStat(key, value[, color])` | Set/update; `value` accepts string, integer, number (`%.2f`), boolean, or any value via `tostring` |
| `Overlay.RemoveStat(key)` | Remove one |
| `Overlay.ClearStats()` | Remove all custom stats |
| `Overlay.ShowStats([bool])` | Show/hide (default show) |
| `Overlay.IsStatsVisible()` | boolean |
| `Overlay.SetStatsAnchor(anchor)` | `"TopRight"` (default), `"TopLeft"`, `"BottomLeft"`, `"BottomRight"` (underscore variants accepted) |
| `Overlay.SetStatsPosition(x, y)` | Custom pixel position |

### 4.5 Canvas draw calls

Draw calls are **per-frame** (cleared after rendering), so call them every frame, from a
`RegisterDrawCallback` or a repeating timer.

| Call | Signature | Defaults |
| --- | --- | --- |
| `DrawText` | `(text, x, y[, color[, fontSize]])` | color `0xFFFFFFFF`, size 0 (font default). The `fontSize` argument is stored but not applied by the renderer |
| `DrawRect` | `(x, y, w, h[, color[, filled[, thickness[, rounding]]]])` | white, outline, 1 px, rounding 0 |
| `DrawLine` | `(x1, y1, x2, y2[, color[, thickness]])` | white, 1 px |
| `DrawCircle` | `(x, y, radius[, color[, filled[, thickness]]])` | white, outline, 1 px |
| `DrawProgressBar` | `(x, y, w, h, fraction[, text[, barColor[, bgColor]]])` | bar `0xFF2ECC71`, bg `0xC81E2028` (`IM_COL32(30,32,40,200)`); fraction clamped 0..1 |

`RegisterDrawCallback(fn)` returns the registry ref and runs `fn()` every frame.

### 4.6 Mod windows

```lua
Overlay.RegisterWindow("My Window", function()
    Overlay.Text("Hello from Lua")
    if Overlay.Button("Click me") then Overlay.Notify("Clicked", "success") end
    Overlay.SameLine()
    local v, changed = Overlay.Checkbox("Enable", true)
    local s, sChanged = Overlay.SliderFloat("Speed", 1.0, 0.0, 10.0)
    local i, iChanged = Overlay.SliderInt("Count", 3, 0, 10)
    Overlay.Separator()
    Overlay.TextColored("Colored", 1.0, 0.4, 0.2)
end)
```

Widgets:

| Widget | Returns |
| --- | --- |
| `Text(str)` | N/A |
| `TextColored(str, r, g, b[, a])` |, (values > 1 are treated as 0–255) |
| `Button(label)` | `clicked` |
| `Checkbox(label, value)` | `value, changed` |
| `SliderFloat(label, value, min, max)` | `value, changed` |
| `SliderInt(label, value, min, max)` | `value, changed` |
| `InputText(label, text[, maxLen])` | `newText, changed` |
| `InputFloat(label, value[, step[, stepFast]])` | `value, changed` |
| `InputInt(label, value[, step[, stepFast]])` | `value, changed` |
| `Combo(label, currentIndex, itemsTable)` | `newIndex, changed` |
| `Selectable(label, selected)` | `selected, clicked` |
| `RadioButton(label, active)` | `clicked` |
| `CollapsingHeader(label[, defaultOpen])` | `isOpen` |
| `TreeNode(label)` | `isOpen` (call `TreePop()` when done) |
| `TreePop()` | N/A |
| `BeginChild(id[, width[, height]])` | `isOpen` (call `EndChild()`) |
| `EndChild()` | N/A |
| `Indent([width])` / `Unindent([width])` | N/A |
| `Spacing()` | N/A |
| `Dummy(width, height)` | N/A |
| `Separator()` / `SameLine()` | N/A |

Input & feedback:

| Call | Returns |
| --- | --- |
| `Tooltip(text)` |, (shows on hover of previous item) |
| `IsItemHovered()` | `boolean` |
| `IsItemClicked([button])` | `boolean` (button: 0=left, 1=right, 2=middle) |
| `ProgressBar(fraction[, width[, height[, overlayText]]])` | N/A |

Tables:

| Call | Returns |
| --- | --- |
| `BeginTable(id, columnCount[, flags])` | `boolean` (call `EndTable()`) |
| `EndTable()` | N/A |
| `TableNextRow([flags[, minHeight]])` | N/A |
| `TableNextColumn()` | `boolean` |
| `TableSetupColumn(label[, flags[, initWidth[, minWidth]]])` | N/A |

Style:

| Call | Notes |
| --- | --- |
| `PushStyleColor(idx, color)` | color accepts hex/int/`{r,g,b,a}` |
| `PopStyleColor([count])` | default count 1 |
| `PushStyleVar(idx, value)` | scalar or `{x, y}` table for vec2 vars |
| `PopStyleVar([count])` | default count 1 |

Window callbacks run on the render thread under the Lua mutex; errors render inline.

### 4.7 Menu control

| Call | Behavior |
| --- | --- |
| `Overlay.IsOpen()` | whether the Avalonium menu (or, when custom windows exist, any window) is open |
| `Overlay.Open()` | open the menu (only if `InGameGUI` enabled); with registered windows, opens the window and works even when `InGameGUI` is disabled |
| `Overlay.Close()` | close |
| `Overlay.Toggle([title])` | with no argument, toggles the menu; a string argument targets a custom window by title |
| `Overlay.ToggleMenu([bool])` | explicit menu setter (the documented boolean form of Toggle) |
| `Overlay.SetMenuKey("F8")` | rebind the menu hotkey (Insert/F5 always work); returns boolean |
| `Overlay.GetViewportSize()` | returns the current viewport width and height |

---

## 5. Draw command structures

```cpp
enum class NotificationType : uint8_t { Info, Success, Warning, Error };
enum class StatsAnchor : uint8_t { TopLeft, TopRight, BottomLeft, BottomRight };

struct ToastNotification {
    uint32_t Id; std::string Title; std::string Message;
    float Duration = 3.0f; float Elapsed = 0.0f;
    NotificationType Type = NotificationType::Info;
    float SlideOffset = 50.0f;
};

struct StatItem { std::string Key; std::string Value; uint32_t Color = 0; int Order = 0; };

enum class DrawCmdType : uint8_t { Text, Rect, RectFilled, Line, Circle, CircleFilled, ProgressBar };

struct DrawCommand {
    DrawCmdType Type;
    float X1, Y1, X2, Y2;
    float Radius; float Thickness = 1.0f; float Rounding = 0.0f; float Fraction = 0.0f;
    uint32_t Color = 0xFFFFFFFF; uint32_t BgColor = 0xFF222222;
    std::string Text; float FontSize = 0.0f;
};

struct CustomLuaWindow { std::string Title; int CallbackRef = -1; bool IsOpen = true; };
```

Manager state: `m_Toasts`, `m_Stats` (unordered_map + order counter), `m_PersistentDrawCmds`,
`m_FrameDrawCmds`, `m_DrawCallbackRefs`, `m_CustomWindows`, `m_StatsVisible` (false),
`m_LiveMetricsEnabled` (true), `m_StatsAnchor` (TopRight), `m_CustomStatsX/Y` (-1).

---

## 6. World Explorer / Inspector internals

`OverlayInternal.h`:

| Item | Meaning |
| --- | --- |
| `kWorldExplorerFlag_IsActor` (`1<<0`) | entry is an actor |
| `kWorldExplorerFlag_InPersistentLevel` (`1<<1`) | entry lives in the persistent level |
| `WorldExplorerEntry` | `Object`, `Class`, `Name`, `ClassName`, `SearchText`, `Flags` |
| `WorldExplorerSourceMode` | `PersistentLevel`, `AllActors`, `AllObjects` |
| `RuntimeMainView` | `Globals`, `WorldExplorer`, `LiveEditor`, `GlobalSearch` |
| `WorldInspectorSnapshot` | resolved identity strings for the selected object |
| `WorldExplorerState` | entries, visible indices, property cache, filters, selection, `MaxResults` (1024), ceilings, `SourceMode` (AllActors), refresh flags/status |
| `kMinWorldExplorerResults` | 128 (measured minimum) |
| `kMaxInspectorProperties` | 1024 |

Helper groups: UI scaffolding (`BeginSectionPanel`, `DrawPanelHeading`, `DrawMetricBlock`,
`DrawInspectorInfoRow`, `DrawMutedText`), formatting (`FormatPointer`, `FormatOffset`,
`FormatCountValue`, `SanitizeUiText`, `ContainsCaseInsensitive`, `ToLowerCopy`), discovery
bridging (`GetLiveWorldPointer`, `GetLivePersistentLevelPointer`, `IsActorClass`,
`ResolveInheritanceChain`, `ResolveCachedClassName`), explorer operations (ceiling caches,
refresh queue, filter rebuild, selection restore) and panel drawing.

## 7. Live Editor internals

`OverlayLiveEditor.h`:

| Type | Purpose |
| --- | --- |
| `LivePropType` | `Unknown, Bool, Byte, Int8, Int16, Int32, Int64, UInt16, UInt32, UInt64, Float, Double, Str, Name, Text, Object, Class, Struct, Enum, Array, Map, Set` |
| `LivePropertyInfo` | name, type, owner class, offset, element size, field pointer, inner/struct/enum props, bitfield masks, nested `InnerProperties`, expansion state |
| `LiveFunctionParam` | name, type, offset, size, flags, in/out/return classification, input buffers (`InputBuffer[256]`, bool/int/float values) |
| `LiveFunctionInfo` | function pointer, names, flags, `ParmsSize`, `ReturnValueOffset`, params, last result, success/time |
| `LiveWatchItem` | watched object/property, current+last value, edit buffer, enabled/logging/history flags, invocation count for function watches |
| `LiveActorComponentEntry` | component pointer/name/class, root flag |
| `LiveEditorState gLiveEditor` | current object/class, filters, inherited toggles, auto-refresh (0.1 s), editing buffers, navigation history, status |

Namespace `LiveEditor`: `Initialize`, `OnSelectObject`, `NavigateHistory`, `AddObjectToHistory`,
`ClassifyPropertyType`, `GetPropertyTypeColor`, `EnumerateClassProperties`,
`EnumerateStructProperties`, `EnumerateClassFunctions`, `GetEnumNamesAndValues`,
`RefreshProperties/Functions/Components`, `ReadLivePropertyValueStr`,
`WriteLivePropertyValue`, `ExecuteLiveFunction`, watch management
(`AddPropertyToWatches`, `AddFunctionToWatches`, `RemoveWatch`, `ClearAllWatches`,
`UpdateWatches`, `SaveWatchesToDisk`, `LoadWatchesFromDisk`) and tab drawing
(`DrawLiveEditorTab`, `DrawLivePropertiesTab`, `DrawLiveFunctionCallerTab`,
`DrawLiveWatchesTab`, `DrawLiveComponentsTab`, `DrawNavigationBreadcrumbs`).

## 8. Universal Search internals

`OverlayGlobalSearch.h`:

| Type | Values |
| --- | --- |
| `GlobalSearchScope` | `PresentLevel`, `AllActors`, `AllObjects`, `AllObjectsAndCDOs`, `SelectedHierarchy` |
| `GlobalSearchResultKind` | `Property`, `Function`, `Object` |
| `GlobalSearchTypeCategory` | `All`, `Numbers`, `Booleans`, `Objects`, `Strings`, `Structs`, `Functions` |
| `GlobalSearchResult` | kind, object/class pointers, names, path, property info + live value, function info, match label, offset |
| `GlobalSearchState gGlobalSearch` | query, last executed query, scope, match toggles (properties/functions/objects/values), regex/address/hierarchy flags, category, results, limit (`EnableLimit=false`, `CustomLimitValue=500` by default; the search uses a 1,000,000 cap), totals, truncation, timing, editing row, status message |

Namespace `GlobalSearch`: `Initialize`, `ExecuteSearch`, `ClearResults`, `RefreshLiveValues`,
`DrawGlobalSearchTab`.

---

## 9. Shared UI helpers (`OverlayHelpers.cpp`)

Formatters (`FormatPointer` → `<not found>` for null, `FormatOffset`, `FormatCountValue`,
`SanitizeUiText`, `ContainsCaseInsensitive`, `ToLowerCopy`), label getters
(`GetDetectedVersionLabel`, `GetRendererBackendLabel`, `GetProcessEventLabel`), live world
accessors, panel scaffolding and `DrawCompactTopBar` (the tab bar used by `menu::Init`).

---

## Cross-references

* [README.md](README.md): overview, lifecycle, hotkeys.
* [LuaAPI/MODS.md](../LuaAPI/MODS.md): builtin mod aliases.
* [DiscoveryAPI/REFERENCE.md](../DiscoveryAPI/REFERENCE.md): object index/`PropertyInfoEx`
  used by the explorer and inspector.
* [RuntimeAPI/CONFIG.md](../RuntimeAPI/CONFIG.md): overlay config keys.
