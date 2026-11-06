# Part III: Event-driven modding

Polling is a last resort. Avalonium gives you the engine's own events.

## 19. Function hooks

### 19.1 The shape

```text
local preId, postId = RegisterHook("FunctionName", preCallback [, postCallback])
```

* `FunctionName` is the UFunction's **short name**.
* `preCallback(self, p1, p2, ...)` runs **before** the engine executes it.
* `postCallback(self, p1, p2, ...)` runs **after**.
* Returns `(preId, postId)` for later removal (`postId` is `0` if you passed no post callback).

```lua
local preId, postId = RegisterHook("ReceiveBeginPlay", function(self)
    print("about to begin play:", self:GetFullName())
end, function(self)
    print("began play:", self:GetFullName())
end)
```

### 19.2 Arguments are live proxies

Each parameter arrives as a proxy object:

```lua
RegisterHook("ApplyDamage", function(self, DamageAmount)
    local amount = DamageAmount:get()
    if type(amount) == "number" and amount > 100 then
        DamageAmount:set(100)        -- clamp incoming damage
        print("damage clamped to 100")
    end
end)
```

Out-params work the same way, which is how you make an engine function produce a different
result.

### 19.3 Cancelling the engine call

Return **truthy** from the pre-hook to prevent the engine call:

```lua
RegisterHook("SomeDangerousFunction", function(self)
    print("blocked for", self:GetName())
    return true        -- cancel
end)
```

Use this sparingly; cancelling a function can leave the engine in a state it did not expect.
We cancel only when we mean it: cancelling isn't a pause button, the callers keep running,
and the bookkeeping the skipped function owed never happens.

### 19.4 The engine's return value

In a post hook, `__OriginalReturnValue` holds what the engine returned:

```lua
RegisterHook("GetDamage", nil, function(self)
    print("engine returned:", __OriginalReturnValue)
end)
```

It is initialised to `nil`, so read it defensively. We track the call ourselves when `nil` is a valid return; otherwise a post hook can't tell
"returned nil" from "never ran".

### 19.5 The proxy's own API

Hook parameter proxies expose:

```
:get()  :Get()          -- read the value
:set(v) :Set(v)         -- write it back
:Cancel()               -- cancel the engine call from inside the callback
:IsCancelled()
:GetSelf()              -- the object the function was dispatched on
:GetFunction()          -- the UFunction
:GetReturnValue()  :SetReturnValue(v)
:Dump()                 -- log the params
```

### 19.6 Unregistering

```lua
UnregisterHook(preId)                        -- by id
UnregisterHook("FunctionName")               -- both ids for that name
UnregisterHook("FunctionName", preId, postId)-- explicit pair
Unhook(preId)                                -- alias
```

Removal is worth doing for expensive hooks that only matter for a while, but remember the
reload wipes everything anyway.

### 19.7 Which functions can be hooked

Avalonium hooks two things:

| Patch | Covers |
| :--- | :--- |
| `ProcessEvent` | native and Blueprint-dispatched functions |
| `ProcessLocalScriptFunction` | interpreted (bytecode) UFunctions, which the VM calls directly |

So both native engine functions and Blueprint script functions are reachable. If you hook a
name the process does not have yet, the registration is remembered and applied when the name
appears, which is exactly what you want for a class that loads with a later level.

### 19.8 Practical hook patterns

**Log every actor that begins play and matches a class:**

```lua
RegisterHook("ReceiveBeginPlay", function(self)
    local cls = self:GetClass()
    if cls and cls:GetName():find("Enemy") then
        print("enemy spawned:", self:GetFullName())
    end
end)
```

**Watch an actor move** (post hook reads the state after the call):

```lua
RegisterHook("K2_SetActorLocation", nil, function(self)
    print("actor moved to", tostring(self:GetLocation()))
end)
```

**Neutralise a single function's effect without touching its code:**

```lua
RegisterHook("Explode", function(self)
    print("explosion suppressed on", self:GetName())
    return true
end)
```

## 20. Lifecycle hooks

These are the engine's coarse events. Register them with a callback; they return an id.

```lua
RegisterBeginPlayHook(function(actor)   -- OnBeginPlay
    if actor and actor:IsValid() then
        print("begin play:", actor:GetFullName())
    end
end)

RegisterEndPlayHook(function(actor)     -- OnEndPlay
    print("end play:", actor:GetFullName())
end)

RegisterLoadMapHook(function(world)     -- OnLoadMap
    print("level up:", world:GetFullName())
end)

RegisterInitGameStateHook(function(gameMode)  -- OnInitGameState
    print("game state ready in world with", gameMode:GetName())
end)
```

Notes that matter:

* Aliases: `OnBeginPlay`, `OnEndPlay`, `OnLoadMap`, `OnInitGameState`.
* `RegisterBeginPlayHook` is answered **once per object per engine call**, so you will not get
  duplicate callbacks for the same actor from one begin-play pass.
* `RegisterLoadMapHook` is answered **once per world** (identity is address + name), and the
  callback context is the world.
* `RegisterInitGameStateHook` hands you the **game mode actor** of the world, because
  `AGameModeBase::InitGameState` is a native virtual that is not reflected on the supported
  engine range; the signal rides the level's begin-play pass and is answered once per world.
* All four have pre/post compatibility variants:

```lua
RegisterBeginPlayPreHook(fn)   RegisterBeginPlayPostHook(fn)
RegisterEndPlayPreHook(fn)     RegisterEndPlayPostHook(fn)
RegisterLoadMapPreHook(fn)     RegisterLoadMapPostHook(fn)
RegisterInitGameStatePreHook(fn) RegisterInitGameStatePostHook(fn)
```

### 20.1 A "wait for the world" helper

Every mod that touches the world needs this. Write it once:

```lua
local worldWaiters = {}

local function WhenWorldReady(fn)
    local world = GetWorld()
    if world and world:IsValid() then
        ExecuteInGameThread(fn)
        return
    end
    worldWaiters[#worldWaiters + 1] = fn
end

RegisterLoadMapHook(function(world)
    if not (world and world:IsValid()) then return end
    local held = worldWaiters
    worldWaiters = {}
    for _, fn in ipairs(held) do pcall(fn, world) end
end)
```

## 21. Custom events

Custom events are the runtime's own message bus, keyed by UFunction short name. Both Lua mods
and Blueprint mods use them.

```lua
RegisterCustomEvent("MyEvent", function(ctx, Arg1)
    print("fired from", ctx:get():GetFullName(), "with", Arg1:get())
end)

RegisterCustomEvent("WithPost", pre, post)   -- optional post callback
```

Removing:

```lua
UnregisterCustomEvent("MyEvent")
UnregisterCustomEvent("MyEvent", preId, postId)
```

Aliases: `HookCustomEvent`, `UnhookCustomEvent`.

The shipped `GenericFunctions` mod is a worked example: it registers
`PrintToModLoader` and `ConstructPersistentObject`, and Blueprint mods dispatch them. The
patterns to copy from it:

* type-check your parameters (`Arg:get():type()`), and raise a clear error on a mismatch;
* use `ctx:get()` to find out *who* called you (its class full name carries the mod's package);
* use an out-param proxy with `:set(...)` to return a value.

I set the out-param proxy; it's not a return value, and returning from Lua does nothing.

```lua
RegisterCustomEvent("GiveItem", function(ctx, ItemName, OutOk)
    local name = ItemName:get()
    if name:type() ~= "FString" then
        error("GiveItem expects an FString item name")
    end
    OutOk:set(true)
    print("giving", name:ToString(), "to", ctx:get():GetFullName())
end)
```

(Blueprint mods and how they call these are documented in
[AvaloniumBP.md](../BPMods/AvaloniumBP.md#6-calling-back-into-the-loader-bpml-custom-events).)

## 22. Keybinds and input

### 22.1 Registering

```lua
RegisterKeyBind(Key.F6, function() print("F6") end)
RegisterKeyBind("F7", function() print("F7 by name") end)
RegisterKeyBind(Key.S, { "LeftControl" }, function() print("Ctrl+S") end)
RegisterKeyBind("S", { "LeftShift" }, function() print("Shift+S") end)
RegisterKeyBind({ Key.S, "LeftControl" }, function() print("table form") end)
```

Aliases: `RegisterKeybind`, `BindKey`, `RegisterKeyBindAsync` (input dispatch is already
asynchronous, so this is a transparent alias).

### 22.2 Key names

Names resolve case-, space-, underscore- and dash-insensitively. Supported families:

* functions `f1`…`f24`; letters `a`…`z`; digits `0`…`9`
* `space`/`spacebar`, `enter`/`return`, `tab`, `escape`/`esc`, `backspace`/`back`/`bksp`
* `shift`/`lshift`/`rshift`, `ctrl`/`control`/`lctrl`/`rctrl`, `alt`/`lalt`/`ralt`
* `insert`/`ins`, `delete`/`del`, `home`, `end`, `pageup`/`pgup`, `pagedown`/`pgdn`
* `capslock`/`caps`, `numlock`, `scrolllock`, `pause`, `printscreen`/`prtscn`/`snapshot`
* arrows `up`/`down`/`left`/`right`
* numpad `numpad0`…`numpad9`, `numpadadd`, `numpadsubtract`, `numpadmultiply`,
  `numpaddivide`, `numpaddecimal`
* OEM punctuation `minus`/`dash`, `plus`/`equals`, `bracketleft`, `bracketright`, `semicolon`,
  `quote`/`apostrophe`, `comma`, `period`/`dot`, `slash`, `backslash`, `tilde`
* mouse `lbutton`/`mouse1`/`leftclick`, `rbutton`/`mouse2`/`rightclick`, `mbutton`/`mouse3`,
  `xbutton1`/`mouse4`, `xbutton2`/`mouse5`

The `Key` table carries **virtual-key code constants:**

```lua
Key.W, Key.A, Key.S, Key.D
Key.SpaceBar, Key.Enter, Key.Tab, Key.Escape, Key.BackSpace
Key.LeftShift, Key.RightShift, Key.LeftControl, Key.RightControl, Key.LeftAlt, Key.RightAlt
Key.F1 … Key.F24
Key.Zero … Key.Nine
Key.NumPadZero … Key.NumPadNine, Key.Multiply, Key.Add, Key.Subtract, Key.Decimal, Key.Divide
Key.Insert, Key.Delete, Key.Home, Key.End, Key.PageUp, Key.PageDown
Key.Up, Key.Down, Key.Left, Key.Right
Key.LeftMouseButton, Key.RightMouseButton, Key.MiddleMouseButton
Key.LeftBracket, Key.RightBracket, Key.Semicolon, Key.Quote, Key.Comma, Key.Period
Key.Slash, Key.Backslash, Key.Tilde, Key.Equals, Key.Minus
Key.CapsLock, Key.NumLock, Key.ScrollLock, Key.Pause
```

Use them with `IsKeyDown`, `RegisterKeyBind`, and anywhere else a key code is expected:

```lua
if IsKeyDown(Key.LeftControl) and IsKeyDown(Key.F1) then … end
RegisterKeyBind(Key.F9, function() ToggleMyMod() end)
```

### 22.3 Querying state

```lua
print(IsKeyDown(Key.W))      -- held right now
print(IsKeyPressed("W"))     -- alias
print(IsKeyHeld(Key.W))      -- alias
print(IsKeyBindRegistered(Key.F6))
```

### 22.4 A held-key loop (movement, flight, etc.)

```lua
-- A simple "noclip-ish" up/down while keys are held, evaluated every 16 ms.
LoopAsync(16, function()
    local player = GetPlayer()
    if not (player and player:IsValid()) then return end

    local loc = player:GetLocation()
    local speed = 8
    local moved = false

    if IsKeyDown(Key.SpaceBar) then loc.Z = loc.Z + speed; moved = true end
    if IsKeyDown(Key.LeftControl) then loc.Z = loc.Z - speed; moved = true end

    if moved then player:SetLocation(loc) end
end)
```

## 23. Timers and scheduling

```lua
-- once
local h = ExecuteWithDelay(2000, function() print("2s later") end)
CancelTimer(h)                       -- ClearTimer / CancelDelayedAction

-- repeating
local loop = LoopAsync(1000, function() print("tick") end)

-- next game-thread pump
ExecuteInGameThread(fn)              -- RunOnGameThread / NextTick

-- frame-based
ExecuteInGameThreadAfterFrames(30, function() print("frame 30") end)
LoopInGameThreadAfterFrames(10, function() print("every 10 frames") end)
LoopInGameThreadWithDelay(500, fn)   -- delay-style repeating
```

### 23.1 Handles you own (UE `Delay`-style)

```lua
local handle = MakeActionHandle()

ExecuteInGameThreadWithDelay(handle, 500, function() print("once") end)
RetriggerableExecuteInGameThreadWithDelay(handle, 500, function() print("retriggered") end)
-- calling it again restarts the same timer instead of stacking a new one
```

### 23.2 Inspecting and controlling

```lua
print(IsValidDelayedActionHandle(handle))
print(IsDelayedActionActive(handle))
print(IsDelayedActionPaused(handle))
print(GetDelayedActionTimeRemaining(handle))
print(GetDelayedActionTimeElapsed(handle))
print(GetDelayedActionRate(handle))

PauseDelayedAction(handle)
UnpauseDelayedAction(handle)
ResetDelayedActionTimer(handle)
SetDelayedActionTimer(handle, 250)
ClearAllDelayedActions()
```

### 23.3 Timers and reloads

`ReloadMods()` calls `ClearAllDelayedActions()`. This is a feature: it means a reload cannot
leave a stale loop mutating a world that no longer exists. It also means you must re-create
your loops in the entry script every load. We cancel repeating timers when the world goes away. The reload wipes them, but the gap
between levels doesn't.

### 23.4 A debounce helper

```lua
local function Debounce(ms, fn)
    local handle
    return function(...)
        if handle then CancelTimer(handle) end
        local args = { ... }
        handle = ExecuteWithDelay(ms, function()
            handle = nil
            fn(table.unpack(args))
        end)
    end
end

local Save = Debounce(500, function() print("saving (once, after quiet)") end)
```

## 24. Threads, async and the game-thread rule

### 24.1 The threads

| Thread | What runs there |
| :--- | :--- |
| **Game thread** | hook callbacks, keybind callbacks, timers, `ExecuteInGameThread` tasks |
| **Timer thread** | schedules delays/loops (their callbacks still land on the game thread) |
| **Init/async thread** | mod loading, `ExecuteAsync` bodies |
| **Caller thread** | IPC evaluations, under the Lua mutex |

### 24.2 Asking where you are

```lua
print(GetCurrentThreadId(), GetGameThreadId(), GetMainModThreadId(), GetAsyncThreadId())
print(IsInGameThread(), IsInMainModThread(), IsInAsyncThread())
print(EngineTickAvailable, ProcessEventAvailable)   -- both true
print(EGameThreadMethod.EngineTick, EGameThreadMethod.ProcessEvent)
```

### 24.3 Running work where it belongs

```lua
-- one-off background work (do not touch engine state here)
ExecuteAsync(function()
    local data = File.Read(GetModsDirectory() .. "/MyMod/big.json")
    local parsed = JSON.Parse(data)
    ExecuteInGameThread(function()
        -- now safe to touch the world with `parsed`
    end)
end)
```

That "read on the async thread, apply on the game thread" pattern is the professional shape for
any heavy mod work: file parsing, big JSON, long computations. We keep engine state on the game thread. Touch it from `ExecuteAsync` and it's not a race
that can be won, it's a crash with scheduling luck.

### 24.4 Thread handles

`MainThread`, `HookThread` and `AsyncThread` are real Lua threads you can store things against.
`ThreadId(id)` wraps a numeric id; handles compare with `__eq` and render via `ToString()`.

```lua
local tid = ThreadId(GetGameThreadId())
print(tid:type())        -- "ThreadIdUserdata"
print(tid:ToString(), tid:GetId())
```

## 25. The Overlay: notifications, stats, canvas, windows

The overlay is what makes a mod *feel* finished. Everything here is on the in-game ImGui
overlay, opened with Insert/F5.

### 25.1 Notifications and toasts

```lua
Notify("Mod loaded")                        -- notification
Toast("Saved!", "success", 3.0)             -- message, type, seconds
```

Accepted type strings: `info`, `success`, `warning`, `error`, `ok`, `warn`, `err`.

```lua
Notify("Careful!", "warning", 5.0)
Toast("Something broke", "error", 4.0)
```

### 25.2 Built-in mod globals

```lua
ToggleHUD()  HideAllHUD()  ShowAllHUD()  IsHUDHidden()  RefreshHUDCache()

ToggleMetrics()  ShowMetrics()  HideMetrics()  IsMetricsVisible()
```

### 25.3 The `Overlay` table

The `Overlay` table is the whole in-game UI: toasts, a stats HUD, a 2D canvas, and custom ImGui
windows. Four things about it are easy to get wrong, so they are stated up front.

1. **Widgets return `(value, changed)`** value first, then the bool.
2. **Colors are not `FLinearColor`.** A color argument accepts a packed integer, a hex string,
   or an **array** table. An `FLinearColor` userdata is not recognised and falls back to the
   default color. See below, the conversions are not the ones you would guess:

   ```lua
   Overlay.SetStat("bulbul", 42, "#FF5555")        -- hex, 6 digits (alpha forced opaque)
   Overlay.SetStat("Water",   42, "#C00080FF")      -- hex, 8 digits = AARRGGBB (alpha FIRST)
   Overlay.SetStat("Focus",  42, 0xFF20B2FF)       -- packed integer, also AARRGGBB
   Overlay.DrawLine(0, 0, 10, 10, {1, 0, 0, 1})    -- array table: {r, g, b, a}

   -- FLinearColor is userdata, so it must be converted by hand:
   local function Color(c)
       return { c.R, c.G, c.B, c.A }                -- array form, 0..1 floats
   end
   Overlay.DrawLine(0, 0, 10, 10, Color(FLinearColor(1, 0, 0, 1)))
   ```

   Two traps live here. `c:ToTable()` returns a table with **named** keys (`R`, `G`, `B`, `A`),
   but the parser reads **array slots** `1`–`4`, so the result is transparent black. And
   `c:ToHex()` emits `#RRGGBBAA` while the parser expects `AARRGGBB`, the alpha and the red
   channel end up swapped. Convert explicitly, or write hex by hand.
3. **Windows are gated by the global menu.** `Overlay.RegisterWindow` registers a panel; its
   body only runs while the menu is open. `Overlay.Open/Close/Toggle/IsOpen` control that one
   global menu, not an individual window. Each window still has its own ImGui close box.
4. **`Notify` takes a title first**; `Toast` takes a message first.

```lua
-- notifications ------------------------------------------------------------
Overlay.Notify("Title")                      -- title only
Overlay.Notify("Title", "body text")         -- title + message
Overlay.Notify("Title", "body text", 5.0)    -- + seconds
Overlay.Notify("Title", "body text", 5.0, "success")
Overlay.Toast("message")                     -- message only
Overlay.Toast("message", "warning")          -- accepts a type in slot 2
Overlay.Toast("message", 2.5, "error")
Overlay.ClearNotifications()

-- stats HUD ---------------------------------------------------------------
Overlay.SetStat("Speed", 471)                -- value is stringified for you
Overlay.SetStat("Health", 3.5, "#FF5555")    -- optional color
Overlay.RemoveStat("Speed")
Overlay.ClearStats()
Overlay.ShowStats(true)
print(Overlay.IsStatsVisible())
Overlay.SetStatsAnchor("top_left")           -- "top_left" | "bottom_left" | "bottom_right" | anything else = top-right
Overlay.SetStatsPosition(10, 10)             -- overrides the anchor with an absolute position

-- immediate-mode drawing (call inside a draw callback) --------------------
-- Every trailing argument below is optional:
--   DrawText(text, x, y, color, fontSize)
--   DrawRect(x, y, w, h, color, filled, thickness, rounding)
--   DrawLine(x1, y1, x2, y2, color, thickness)
--   DrawCircle(x, y, radius, color, filled, thickness)
--   DrawProgressBar(x, y, w, h, fraction, text, barColor, backgroundColor)
local x, y, w, h, radius, fraction = 100, 100, 200, 12, 24, 0.66
local x1, y1, x2, y2 = 0, 0, 100, 100
Overlay.DrawText("label", x, y)
Overlay.DrawRect(x, y, w, h)
Overlay.DrawLine(x1, y1, x2, y2)
Overlay.DrawCircle(x, y, radius)
Overlay.DrawProgressBar(x, y, w, h, fraction)

-- a per-frame canvas callback; returns its registry id
local ref = Overlay.RegisterDrawCallback(function()
    Overlay.DrawLine(x1, y1, x2, y2, {1, 0, 0, 1}, 2)
end)

-- a custom window ---------------------------------------------------------
local enabled, height, count = false, 500, 3
Overlay.RegisterWindow("My Window", function()
    Overlay.Text("Hello from my mod")
    if Overlay.Button("Click me") then Notify("clicked") end

    local value, changed = Overlay.Checkbox("Enabled", enabled)   -- value FIRST
    if changed then enabled = value end

    local newHeight, moved = Overlay.SliderFloat("Height", height, 0, 1000)
    height = newHeight
    if moved then print("height", height) end

    local newCount = Overlay.SliderInt("Count", count, 0, 10)     -- value first here too
    count = newCount

    Overlay.Separator()
    Overlay.SameLine()

    -- dropdown from a table
    local items = {"Low", "Medium", "High", "Ultra"}
    local qualityIdx = 2
    local newIdx, qChanged = Overlay.Combo("Quality", qualityIdx, items)
    if qChanged then qualityIdx = newIdx end

    -- text input
    local name = "Player"
    local newName, nChanged = Overlay.InputText("Name", name, 64)
    if nChanged then name = newName end

    -- numeric inputs
    local speed = 1.5
    local newSpeed, sChanged = Overlay.InputFloat("Speed", speed, 0.1, 1.0)
    if sChanged then speed = newSpeed end

    -- selectable row (great for lists)
    local selectedRow = 1
    for i = 1, 5 do
        local sel, clicked = Overlay.Selectable("Item " .. i, selectedRow == i)
        if clicked then selectedRow = i end
    end

    -- collapsing section
    if Overlay.CollapsingHeader("Advanced settings", false) then
        Overlay.Indent()
        if Overlay.RadioButton("Option A", true) then end
        Overlay.Unindent()
    end

    -- tables
    if Overlay.BeginTable("MyTable", 3) then
        Overlay.TableSetupColumn("Name")
        Overlay.TableSetupColumn("Value")
        Overlay.TableSetupColumn("Action")
        Overlay.TableNextRow()
        Overlay.TableNextColumn(); Overlay.Text("Health")
        Overlay.TableNextColumn(); Overlay.Text("100")
        Overlay.TableNextColumn(); if Overlay.Button("Heal") then end
        Overlay.EndTable()
    end

    -- style tweak
    Overlay.PushStyleColor(21, {1, 0.3, 0.3, 1})  -- ImGuiCol_Button = 21
    if Overlay.Button("Danger") then end
    Overlay.PopStyleColor()
end)

-- the global menu ---------------------------------------------------------
Overlay.SetMenuKey(Key.F8)        -- the key that opens the menu (accepts a Key or a string)
Overlay.Open()                    -- open the menu
Overlay.Close()
Overlay.Toggle()                  -- flip
Overlay.Toggle(true)              -- or force a state
print(Overlay.IsOpen())
```

> Every `Overlay` member is callable with either `.` or `:`, the binding strips a leading
> table argument when it is the `Overlay` table itself, so `Overlay:SetStat("k", "v")` and
> `Overlay.SetStat("k", "v")` both work.

> Exact member list: [LuaModding.md §13](LuaModding.md#13-the-overlay-table). A drawing mistake
> shows up immediately on screen and never crashes the game, so the canvas is a safe place to
> experiment. We convert colors by hand. The trap here isn't a crash; it's silent wrong colors and
> `(value, changed)` returns in the order nobody expects.

### 25.4 A stats panel that shows your own numbers

```lua
LoopAsync(250, function()
    local player = GetPlayer()
    if not (player and player:IsValid()) then return end
    local loc = player:GetLocation()
    Overlay.SetStat("X", math.floor(loc.X))
    Overlay.SetStat("Y", math.floor(loc.Y))
    Overlay.SetStat("Z", math.floor(loc.Z))
    Overlay.SetStat("FPS", math.floor(GetFPS()))
end)
```

## 26. Built-in mod globals

Registered unconditionally, before any mod runs:

| Group | Globals |
| :--- | :--- |
| HUD | `ToggleHUD`, `HideAllHUD`, `ShowAllHUD`, `IsHUDHidden`, `RefreshHUDCache` |
| Metrics | `ToggleMetrics`/`ToggleMetricsHUD`, `ShowMetrics`/`ShowMetricsHUD`, `HideMetrics`/`HideMetricsHUD`, `IsMetricsVisible`/`IsMetricsHUDVisible` |
| Overlay | `Notify`, `Toast` |

`RefreshHUDCache()` is worth calling after a level transition if you toggled HUD widgets and the
game rebuilt them.
