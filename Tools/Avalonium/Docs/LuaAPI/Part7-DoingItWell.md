# Part VII: Doing this well

The projects in [Part 6](Part6-Projects.md) work. This part is about making them *last*:
performant, correct across a reload, safe on the wrong thread, and debuggable at 2 a.m.

## 35. Performance: the four budgets

A mod has exactly four resources. Almost every complaint about a "laggy mod" is one of them.

| Budget | Cost centre | The rule |
| :--- | :--- | :--- |
| **Frame time** | draw callbacks, tick loops | do nothing per frame you can do per second |
| **Object walks** | `ForEachUObjectFlat`, `FindAllOf`, `GetAllActorsOfClass` | walk once, cache the result |
| **Log volume** | `print` from hooks and per-frame code | log state changes, not states |
| **Timers** | `LoopAsync` at small intervals | one 250 ms loop beats four 60 ms loops |

### 35.1 The per-frame trap

The single most common performance mistake is doing work in a draw callback that does not need
to run every frame.

```lua
-- BAD: a full world scan, 60 times a second.
Overlay.RegisterDrawCallback(function()
    local enemies = GetAllActorsOfClass("BP_Enemy_C")     -- allocates an array every frame
    Overlay.Text("enemies: " .. #enemies)
end)

-- GOOD: scan on a slow clock, draw the cached answer every frame.
local enemyCount = 0
LoopAsync(500, function()
    enemyCount = #GetAllActorsOfClass("BP_Enemy_C")
end)

Overlay.RegisterDrawCallback(function()
    Overlay.Text("enemies: " .. enemyCount)                -- a number, not a scan
end)
```

We don't scan in a draw callback; the BAD version allocates a fresh array every frame, so the
GC gets a vote in the frame budget.

### 35.2 The scan-once-and-filter pattern

`ForEachUObjectFlat` over a large game touches tens of thousands of objects. When you need a
subset, collect the subset once and keep it, then refresh on an event.

```lua
local cache = { enemies = {}, actorsByName = {} }

local function RebuildCache()
    cache.enemies, cache.actorsByName = {}, {}
    for _, actor in ipairs(GetWorldActors()) do
        if not actor:IsValid() then goto continue end
        cache.actorsByName[actor:GetName()] = actor
        if actor:GetClass() and actor:GetClass():GetName():find("Enemy") then
            cache.enemies[#cache.enemies + 1] = actor
        end
        ::continue::
    end
end

-- Refresh when the world changes, not on a timer.
RegisterLoadMapHook(RebuildCache)
RebuildCache()
```

> If your Lua version predates `goto`, use `if actor:IsValid() then ... end` nesting. Lua 5.4 is
> what Avalonium embeds, so `goto` and `::labels::` are available.

### 35.3 Logging is not free

Every `print` hits a C++ logging path, and a hook that fires per damage event can call it
tens of thousands of times a second.

```lua
-- BAD: one line per hit.
RegisterHook("TakeDamage", function(self, amount)
    print("damage", tostring(amount:get()))
end)

-- GOOD: accumulate, report on a slow clock.
local damageEvents, damageTotal = 0, 0

RegisterHook("TakeDamage", function(self, amount)
    damageEvents = damageEvents + 1
    local value = amount:get()
    if type(value) == "number" then damageTotal = damageTotal + value end
end)

LoopAsync(5000, function()
    if damageEvents == 0 then return end
    print(("damage: %d events, %.1f total, %.2f avg"):format(
        damageEvents, damageTotal, damageTotal / damageEvents))
    damageEvents, damageTotal = 0, 0
end)
```

### 35.4 Budgeting a tick bus

When several mods share a frame callback, you can measure each one and drop the expensive ones.

```lua
local function Budgeted(limitMs, name, fn)
    local total, calls = 0, 0
    return function(...)
        local started = os.clock()
        local ok, err = pcall(fn, ...)
        local cost = (os.clock() - started) * 1000

        calls = calls + 1
        total = total + cost

        if not ok then print(("%s failed: %s"):format(name, tostring(err))); return end

        if calls >= 600 then                  -- roughly ten seconds of frames
            local average = total / calls
            if average > limitMs then
                print(("%s: %.2fms/frame average exceeds %.2fms budget"):format(name, average, limitMs))
            end
            total, calls = 0, 0
        end
    end
end

kit.OnTick("expensive", Budgeted(0.5, "expensive", function()
    -- per-frame work with a visible cost report
end))
```

### 35.5 What the runtime already does for you

* Hooks are installed on `ProcessEvent` and `ProcessLocalScriptFunction`, so Blueprint and
  native dispatch are both covered without extra cost to you. We filter inside the hook; coverage is automatic, filtering is not, and a hook on a hot
  function pays Lua dispatch for every call.
* Timers run on a dedicated thread and enqueue onto the game-thread pump, a 16 ms loop does
  not spin a busy thread.
* Mod loading is guarded by `m_LoadingMods` so an engine thread arriving mid-load answers
  immediately instead of blocking.
* `GenerateLuaTypes` runs once, not per frame.

---

## 36. Threading without tears

### 36.1 The one question

Before any engine call, ask: **am I on the game thread?**

```lua
print(IsInGameThread(), IsInAsyncThread(), GetCurrentThreadId(), GetGameThreadId())
```

| Where your code runs | On the game thread? |
| :--- | :--- |
| Mod `main.lua` at load time | yes during the loading pass, but the world may not exist yet |
| `ExecuteInGameThread(fn)` body | yes |
| hook callbacks (pre/post) | yes |
| keybind callbacks | yes |
| timer callbacks (`LoopAsync`, `ExecuteWithDelay`) | yes |
| overlay draw callbacks, window callbacks | yes |
| `ExecuteAsync(fn)` body | **no** |
| code run from the IPC server (`ExecuteString`) | **no** |

### 36.2 The universal safe wrapper

When you cannot prove which thread you are on, or when a function is reachable from both, hop
first:

```lua
local function Safe(fn)
    if IsInGameThread() then
        return fn()
    end
    ExecuteInGameThread(fn)
end
```

### 36.3 Reading engine state off-thread

Reading a reflected property is a memory read. It is *usually* fine, but on a multi-threaded
game a property can be mid-write. If you are on the async thread and you need a value, do not
read it there. We hop to the game thread for this: a torn read isn't a crash, it's a plausible garbage
value that sends the next branch down the wrong path:

```lua
-- BAD: reading engine memory from the async thread on a schedule.
ExecuteAsync(function()
    local player = GetPlayer()            -- engine access off-thread
    print(player.CustomTimeDilation)
end)

-- GOOD: the async body only hands work to the game thread.
ExecuteAsync(function()
    ExecuteInGameThread(function()
        local player = GetPlayer()
        if player and player:IsValid() then print(player.CustomTimeDilation) end
    end)
end)
```

### 36.4 Delayed work is game-thread work

`ExecuteWithDelay` and `LoopAsync` are safe from anywhere because they only *enqueue*:

```lua
-- Called from wherever; the body runs on the game thread.
ExecuteWithDelay(1500, function()
    local player = GetPlayer()            -- safe
end)
```

---

## 37. Correctness across reloads

**Reload wipes hooks and timers.** That is not a bug; it is the contract. Build for it. We restore engine values on unload; the reload contract covers hooks and timers, not values
a mod changed.

### 37.1 The checklist for every mod

1. `Mod.OnUnload` cancels what you started (`CancelTimer`, `CancelAllDelayedActions`).
2. `Mod.OnUnload` restores anything you changed (time dilation, CVars, speeds).
3. `Mod.OnUnload` saves persistent state.
4. Nothing assumes a hook from a previous load still exists.
5. Shared variables you published are cleared if the next load cannot rebuild them.
6. Global names you created are either re-created at load or explicitly nil'd.

### 37.2 A concrete pattern

```lua
local handles = {}
local saved = { speed = nil, dilation = nil }

local function ApplySpeed()
    local player = GetPlayer()
    if not (player and player:IsValid()) then return end
    if saved.speed == nil then saved.speed = player.CustomTimeDilation end
    player.CustomTimeDilation = 1.4
end

local function ApplyTimeDilation()
    local world = GetWorld()
    if not (world and world:IsValid()) then return end
    local settings = world.WorldSettings
    if not (settings and settings:IsValid()) then return end
    if saved.dilation == nil then saved.dilation = settings.TimeDilation end
    settings.TimeDilation = 0.5
end

local function RestoreEngineValues()
    local player = GetPlayer()
    if player and player:IsValid() and saved.speed then
        player.CustomTimeDilation = saved.speed
    end
    local world = GetWorld()
    if world and world:IsValid() and saved.dilation then
        local settings = world.WorldSettings
        if settings and settings:IsValid() then settings.TimeDilation = saved.dilation end
    end
end

local function PersistState()
    local dir = GetModDirectory() .. "/MyMod"
    File.CreateDir(dir)
    File.Write(dir .. "/settings.json", JSON.Stringify({ saved = saved }))
end

local function Install()
    handles.speed = LoopAsync(500, ApplySpeed)
    handles.tick  = LoopAsync(1000, ApplyTimeDilation)
end

local function Teardown()
    for name, handle in pairs(handles) do
        CancelTimer(handle)
        handles[name] = nil
    end
    RestoreEngineValues()
    PersistState()
end

Install()
Mod.OnUnload = Teardown
```

### 37.3 Why `RestartCurrentMod` helps

While developing, you can iterate without leaving the game:

```lua
RegisterKeyBind({Key.R, "LeftControl"}, function()
    Notify("reloading...", "info")
    ExecuteWithDelay(150, function() RestartCurrentMod() end)
end)
```

The small delay matters: let the notification and any queued work drain *before* the reload
clears the timer queue.

---

## 38. The debugging playbook

### 38.1 Symptom → cause

| Symptom | Most likely cause | First move |
| :--- | :--- | :--- |
| `attempt to index a nil value` at `player.CustomTimeDilation` | the player does not exist yet | guard with `player and player:IsValid()` |
| property read returns `nil` | wrong name, or the member is not reflected | `obj:DumpProperties()`, then check the spelling |
| property write appears to do nothing | the game recomputes it every frame | write it in a loop, or hook the function that computes it |
| `Invalid object` | you held a handle across a level change | re-resolve the object; do not cache actors across maps |
| a hook never fires | the function name does not exist, or fires on a different self | install a logging hook, then `obj:DumpFunctions()` |
| `require("mymodule")` fails | file is not under `scripts/` with that exact name | check §2.3's search order |
| mod does not load at all | folder name mismatch in `mods.json`, or no `scripts/main.lua` | check the log for the discovery pass |
| `Key.X` is `nil` | the key table spells it differently (`OEM_THREE`, not `OEM_3`) | print the table keys |
| everything stops after a reload | your mod re-ran but its hooks are gone | re-install on load, never rely on persistence |
| two mods fight over a value | both write the same property | one mod should own it; expose a service instead (§Project 7) |

### 38.2 The five commands that answer most questions

```lua
-- 1. What is this object, really?
obj:DumpProperties()
obj:DumpFunctions()

-- 2. What class does it derive from, and what shape does the runtime see?
print(obj:GetClass():GetName())
print(GetObjectShape(obj:GetFullName()))

-- 3. What exists matching a name?
for _, o in ipairs(FindObjects("Sword")) do print(o:GetFullName()) end

-- 4. Can the runtime resolve assets and names at all?
print(GetBlueprintPipeline().assetPipelineReady)

-- 5. Do I have the API I think I have?
for _, n in ipairs({ "RegisterHook", "Overlay", "Memory", "ModRef" }) do
    print(n, _G[n] ~= nil)
end
```

### 38.3 Reading the log

The log is the primary interface between your mod and reality. `print`, `log` and `Log` all land
in the same sink with a `[Lua] ` prefix. Because mods are named, prefix your own lines:

```lua
local function MyLog(fmt, ...)
    print(("[MyMod] " .. fmt):format(...))
end

MyLog("loaded, %d features", 7)
```

When a log line is noisy, remember the object's address is in `__tostring`:

```lua
print("player handle:", tostring(GetPlayer()))   -- [UObject: Class] Name (0x0000...)
```

### 38.4 Confirming you are looking at the right object

Two objects can share a short name. Always log the **full** name when it matters:

```lua
local a = FindFirstOf("Character")
print(a:GetName(), a:GetFullName(), a:GetAddressString())
```

### 38.5 When a value refuses to stick

Three causes, in order of likelihood:

1. **The property is not what you think.** Check its type:
   `print(tostring(obj:GetProperty("CustomTimeDilation")))` gives name, class and offset.
2. **The game overwrites it each frame** from a script or a timer. Hook the writer and inspect,
   or re-apply on your own timer.
3. **You are writing to a copy, not the storage.** Struct members returned by value are copies;
   write through the property wrapper or through the owning object.

I check them in order: wrong descriptor, overwritten every frame, copy semantics. Working
backwards wastes hours, and the answer is usually the first one.

---

# Part VIII: Cookbook

Everything you will reach for, in tables. Keep this part open in a second window while you
work; it is the page you will come back to.

## 39. Value types at a glance

```text
FVector(x, y, z)              Vector(...) also works
FRotator(pitch, yaw, roll)    Rotator(...)
FTransform                    Transform(...)
FQuat(x, y, z, w)
FLinearColor(r, g, b, a)      0..1 floats
FColor(r, g, b, a)            0..255 bytes
FVector2D   FVector4   FVector4d
FString   FAnsiString   FUtf8String
FName(name [, EFindName.FNAME_Find | FNAME_Add | FNAME_Replace])
FText
FSoftObjectPath   TSoftObjectPtr
FWeakObjectPtr(obj)
FDelegate   FMulticastScriptDelegate   Delegate   MulticastDelegate
FNameUserdata / ThreadIdUserdata
```

Reading a value costs nothing and returns plain numbers where the type is arithmetic:

```lua
local v = FVector(1, 2, 3)
print(v.X, v.Y, v.Z)     -- 1, 2, 3
```

## 40. Keys you can bind

`RegisterKeyBind` accepts the `Key` table, a string, or a raw virtual-key number, and normalises
case, spaces, underscores and dashes.

### 40.1 The `Key` table (constants)

These are the exact field names in the `Key` table. Anything not here is `nil`:

| Group | `Key.*` constants |
| :--- | :--- |
| Function | `F1` … `F24` |
| Letters | `A` … `Z` |
| Digits | `Zero` … `Nine` |
| Editing | `BackSpace`, `Tab`, `Enter`, `Escape`, `Insert`, `Delete` |
| Arrows | `Left`, `Up`, `Right`, `Down` |
| Blocks | `PageUp`, `PageDown`, `End`, `Home` |
| Locks / system | `CapsLock`, `NumLock`, `ScrollLock`, `Pause` |
| Numpad | `NumPadZero` … `NumPadNine`, `Multiply`, `Add`, `Subtract`, `Decimal`, `Divide` |
| OEM | `LeftBracket`, `RightBracket`, `Semicolon`, `Quote`, `Comma`, `Period`, `Slash`, `Backslash`, `Tilde`, `Equals`, `Minus` |
| Mouse | `LeftMouseButton`, `RightMouseButton`, `MiddleMouseButton`, `ThumbMouseButton`, `ThumbMouseButton2` |
| Modifiers | `Shift`, `LeftShift`, `RightShift`, `Control`, `LeftControl`, `RightControl`, `Alt`, `LeftAlt`, `RightAlt` |
| Gamepad | `Gamepad_Left2D`, `Gamepad_LeftX`, `Gamepad_LeftY`, `Gamepad_Right2D`, `Gamepad_RightX`, `Gamepad_RightY`, `Gamepad_LeftTriggerAxis`, `Gamepad_RightTriggerAxis`, `Gamepad_LeftThumbstick`, `Gamepad_RightThumbstick`, `Gamepad_Special_Left`, `Gamepad_Special_Right`, `Gamepad_FaceButton_Bottom`, `Gamepad_FaceButton_Right`, `Gamepad_FaceButton_Left`, `Gamepad_FaceButton_Top`, `Gamepad_LeftShoulder`, `Gamepad_RightShoulder`, `Gamepad_LeftTrigger`, `Gamepad_RightTrigger`, `Gamepad_DPad_Up`, `Gamepad_DPad_Down`, `Gamepad_DPad_Right`, `Gamepad_DPad_Left`, `Gamepad_LeftStick_Up`, `Gamepad_LeftStick_Down`, `Gamepad_LeftStick_Right`, `Gamepad_LeftStick_Left`, `Gamepad_RightStick_Up`, `Gamepad_RightStick_Down`, `Gamepad_RightStick_Right`, `Gamepad_RightStick_Left` |

### 40.2 Modifiers in keybinds

Modifier keys are passed as **strings** in a table, not a separate constant table:

```lua
RegisterKeyBind({Key.S, "LeftShift"}, fn)            -- table form, key first
RegisterKeyBind("S", {"LeftControl"}, fn)            -- string + modifier list
RegisterKeyBind({Key.S, "LeftControl", "LeftAlt"}, fn)  -- multiple modifiers
```

Accepted modifier strings (case-insensitive): `shift`, `leftshift`, `rightshift`, `ctrl`,
`control`, `leftctrl`, `rightctrl`, `leftcontrol`, `rightcontrol`, `alt`, `leftalt`, `rightalt`.

### 40.3 String names (tolerant path)

`RegisterKeyBind` and `IsKeyDown` also accept plain strings, resolved case-, space-, underscore-
and dash-insensitively: `f1`…`f24`, `a`…`z`, `0`…`9`, `space`/`spacebar`, `enter`/`return`, `tab`,
`escape`/`esc`, `backspace`/`back`/`bksp`, `shift`/`lshift`/`rshift`, `ctrl`/`control`/`lctrl`/`rctrl`,
`alt`/`lalt`/`ralt`, `up`/`down`/`left`/`right`, `insert`/`ins`, `delete`/`del`, `home`/`end`,
`pageup`/`pgup`, `pagedown`/`pgdn`, `capslock`/`caps`, `numlock`, `scrolllock`, `pause`/`prtscn`,
`numpad0`…`numpad9`, `numpadadd`, `numpadsubtract`, `numpadmultiply`, `numpaddivide`, `numpaddecimal`,
`minus`/`dash`, `plus`/`equals`, `bracketleft`, `bracketright`, `semicolon`, `quote`/`apostrophe`,
`comma`, `period`/`dot`, `slash`, `backslash`, `tilde`, `lbutton`/`mouse1`/`leftclick`,
`rbutton`/`mouse2`/`rightclick`, `mbutton`/`mouse3`, `xbutton1`/`mouse4`, `xbutton2`/`mouse5`.
Raw virtual-key numbers work too.

Key state, anywhere on the game thread:

```lua
local crouching = IsKeyDown(Key.SpaceBar)
local running   = IsKeyDown(Key.LeftShift)
local sameThing = IsKeyDown("shift")                -- same thing, by name
local pressed   = IsKeyPressed(Key.F1)              -- alias: IsKeyHeld

if running then
    print("sprinting", crouching, sameThing, pressed)
end
```

## 41. Notifications and toasts

```lua
Notify("message")                       -- message
Notify("message", "success")            -- message, type
Notify("message", "success", 5.0)       -- message, type, seconds
Toast("message")  Toast("message", "error")  Toast("message", "error", 5.0)
```

`Overlay.Notify` and `Overlay.Toast` are the overlay-table forms, with one difference worth
knowing: **`Overlay.Notify` reads its first string as a title**, while `Toast` reads it as the
message.

```lua
Overlay.Notify("Title", "body", 5.0, "success")
Overlay.Toast("body", "warning")
Overlay.ClearNotifications()
```

Accepted type strings: `info`, `success`, `warning`, `error`, and the short forms `ok`, `warn`,
`err`. Anything else falls back to `info`.

## 42. Overlay, in two tables

**Stats HUD** (key → value readout):

```lua
Overlay.SetStat("Speed", 471)           -- create or update (optional 3rd arg: color)
Overlay.RemoveStat("Speed")             -- remove one
Overlay.ClearStats()                    -- remove all
Overlay.ShowStats(true)                 -- visibility (false hides)
print(Overlay.IsStatsVisible())
Overlay.SetStatsAnchor("top_left")       -- "top_left", "bottom_left", "bottom_right", else top-right
Overlay.SetStatsPosition(10, 10)        -- absolute position; overrides the anchor
```

**Canvas drawing** (inside a draw callback):

```lua
Overlay.RegisterDrawCallback(function()
    -- arguments are always positional-positional; every trailing one is optional:
    --   DrawText(text, x, y, color, fontSize)
    --   DrawRect(x, y, w, h, color, filled, thickness, rounding)
    --   DrawLine(x1, y1, x2, y2, color, thickness)
    --   DrawCircle(cx, cy, radius, color, filled, thickness)
    --   DrawProgressBar(x, y, w, h, fraction, text, barColor, backgroundColor)
    Overlay.DrawText("label", 100, 100)
    Overlay.DrawRect(100, 100, 200, 12)
    Overlay.DrawLine(0, 0, 100, 100)
    Overlay.DrawCircle(400, 300, 24)
    Overlay.DrawProgressBar(100, 140, 200, 12, 0.66)
end)
```

> **Colors are not `FLinearColor`.** A color slot accepts:
>
> | Form | Example | Note |
> | :--- | :--- | :--- |
> | packed integer | `0xFF20B2FF` | `AARRGGBB` |
> | hex string | `"#20B2FF"` | 6 digits = opaque; the leading `#` is optional |
> | hex string, 8 digits | `"#FF20B2FF"` | **`AARRGGBB`**, alpha comes first |
> | array table | `{1, 0, 0, 1}` | `{r, g, b[, a]}`, 0–1 floats *or* 0–255 bytes |
>
> An `FLinearColor` userdata silently yields the default color (white). Convert with
> `{ c.R, c.G, c.B, c.A }`, **not** `c:ToTable()`, which returns *named* keys (`R`/`G`/`B`/`A`)
> while the parser reads array slots `1`–`4`, producing transparent black. Likewise
> `c:ToHex()` emits `RRGGBBAA` but the parser reads `AARRGGBB`, swapping alpha and red.
> We convert explicitly; silent white is the default, so a wrong argument type doesn't error,
> it just looks like the style was ignored.

**Windows and widgets:**

```lua
Overlay.RegisterWindow("My Panel", function()         -- the body runs while the menu is open
    Overlay.Text("plain text")
    Overlay.TextColored("colored text", 0.6, 0.8, 1.0, 1.0)   -- the STRING comes first

    if Overlay.Button("Click me") then Notify("clicked") end

    -- all three stateful widgets return (value, changed) -- value FIRST
    local flag,  toggled = Overlay.Checkbox("Enabled", false)
    local size,  moved   = Overlay.SliderFloat("Size", 50, 0, 100)
    local count, changed = Overlay.SliderInt("Count", 3, 0, 10)
    print(flag, toggled, size, moved, count, changed)

    Overlay.Separator()
    Overlay.SameLine()
end)

print(Overlay.IsOpen())                    -- the GLOBAL menu, not one window
Overlay.Open()
Overlay.Close()
Overlay.Toggle()                           -- flip
Overlay.Toggle(true)                       -- or force a state
Overlay.SetMenuKey(Key.F8)                 -- accepts a Key value or a name string
```

## 43. Timers, in one table

| Need | Call |
| :--- | :--- |
| run once, soon | `ExecuteInGameThread(fn)` |
| run once, later | `ExecuteWithDelay(ms, fn)` |
| repeat | `LoopAsync(ms, fn)` |
| repeat, frame-based | `LoopInGameThreadAfterFrames(frames, fn)` |
| cancel | `CancelTimer(handle)` |
| reserve a handle | `MakeActionHandle()` |
| pause / resume | `PauseDelayedAction(h)` / `UnpauseDelayedAction(h)` |
| change the delay | `SetDelayedActionTimer(h, ms)` / `ResetDelayedActionTimer(h)` |
| inspect | `IsDelayedActionActive(h)`, `GetDelayedActionTimeRemaining(h)`, `GetDelayedActionRate(h)` |
| cancel everything | `ClearAllDelayedActions()` |
| off-thread one-shot | `ExecuteAsync(fn)` |

Aliases: `SetTimeout`, `SetInterval`, `SetTimer`, `RunOnGameThread`, `NextTick`,
`ClearTimer`, `CancelDelayedAction`.

## 44. Hooks, in one table

| Kind | Registration | Callback shape |
| :--- | :--- | :--- |
| function | `RegisterHook(name, pre[, post])` | `pre(self, ...params)`, return truthy to cancel |
| custom event | `RegisterCustomEvent(name, pre[, post])` | as above |
| new object | `NotifyOnNewObject(class, cb)` | `cb(obj)` |
| begin play | `RegisterBeginPlayHook(pre[, post])` | `self` is the actor |
| end play | `RegisterEndPlayHook(pre[, post])` | `self` is the actor |
| map load | `RegisterLoadMapHook(pre[, post])` | `self` is the new world |
| game state | `RegisterInitGameStateHook(pre[, post])` | `self` is the game state |
| console command | `RegisterConsoleCommandHandler(name, cb)` | `cb(command, args)` |
| console (global) | `RegisterConsoleCommandGlobalHandler([name], cb)` | `cb(command, args)` |

Unregister with the ids you received, `UnregisterHook(preId)`, `UnregisterCustomEvent(name)`, never by re-deriving names. A pre-hook that returns truthy cancels the engine call; the post hook's engine return value is in `__OriginalReturnValue`.

Inside a hook, parameters are **live proxies**:

```lua
param:get()      -- read   (aliases: Get)
param:set(v)     -- write  (aliases: Set)
param:Cancel()   -- cancel the call (aliases: IsCancelled)
param:GetSelf()  param:GetFunction()  param:GetReturnValue()  param:SetReturnValue(v)
param:Dump()
```

## 45. Flags and constants

```text
EObjectFlags.RF_Transient            (and the rest of the object flags)
EObjectFlags.RF_AllFlags
EInternalObjectFlags.RootSet
EInternalObjectFlags.GarbageCollectionKeepFlags
EFindName.FNAME_Find   EFindName.FNAME_Add   EFindName.FNAME_Replace
NAME_None
PropertyTypes.FloatProperty.Size     (plus every property class record)
```

On an object:

```lua
obj:HasAnyFlags(EObjectFlags.RF_Transient)
obj:HasAllFlags(...)
obj:HasAnyInternalFlags(EInternalObjectFlags.Native)
```

The flag tables are engine-version aware; entries that do not exist in the running engine are
simply absent from the table.

## 46. Version tables

```text
UnrealVersion.Major   UnrealVersion.Minor
UnrealVersion:GetMajor()   UnrealVersion:GetMinor()
UnrealVersion:IsBelow(a, b)   UnrealVersion:IsAtLeast(a, b)

RuntimeVersion.Major   RuntimeVersion.Minor   RuntimeVersion.Hotfix
RuntimeVersion.GetVersion()   RuntimeVersion.CompatMajor   RuntimeVersion.CompatMinor

UE4SS   Avalonium          aliases of RuntimeVersion
```

Guard anything version-sensitive:

```lua
if RuntimeVersion.CompatMinor >= 5 then
    -- use the v3.5 surface
end
```

## 47. Thread and process introspection

```text
IsInGameThread()  IsInAsyncThread()  IsInMainModThread()
GetGameThreadId()  GetAsyncThreadId()  GetMainModThreadId()  GetCurrentThreadId()
GetFrameTime()  GetFPS()  GetDeltaSeconds()
GetMemoryUsage()  ForceGarbageCollection()
GetObjectCount()  GetObjectByIndex(i)
ue.get_globals().guobjectarray  .gnames  .gworld  .gengine  .gmalloc  .gnatives
```

## 48. Find the right function, fast

| I want to… | Reach for |
| :--- | :--- |
| get the player | `GetPlayer()` / `GetPlayerPawn()` / `GetPlayerCharacter()` |
| get the player's controller | `GetPlayerController()` |
| get the camera | `GetPlayerCameraManager()` |
| get the world | `GetWorld()` |
| get the game mode / state | `GetGameMode()` / `GetGameState()` |
| get an engine singleton | `GetEngine()`, `GetGameInstance()`, `GetLocalPlayer()` |
| get a Kismet library | `GetKismetMathLibrary()` and friends, `GetGameplayStatics()` |
| find one object by name | `FindObject(name)` |
| find many by name | `FindObjects(name)` |
| find by class | `FindFirstOf(cls)`, `FindAllOf(cls)`, `FindClass(name)` |
| find by path | `StaticFindObject(path)`, `LoadAsset(path)` |
| enumerate the world | `GetWorldActors()`, `GetAllActorsOfClass(cls)` |
| enumerate everything | `ForEachUObjectFlat(cb)`, `ForEachUObject(cb)` |
| inspect an object | `obj:DumpProperties()`, `obj:DumpFunctions()`, `GetObjectShape(path)` |
| call a function | `obj:Call("Name", ...)`, `CallFunction(obj, "Name", ...)` |
| read/write a field | `obj.Field`, `obj:GetProperty("Field")`, `obj:SetProperty("Field", v)` |
| spawn something | `world:SpawnActor(cls, loc, rot)`, `SpawnActor(...)` |
| create an object | `NewObject(cls)`, `StaticConstructObject(cls)`, `DuplicateObject(src)` |
| check a class relationship | `obj:IsA(cls)`, `InheritsFrom(obj, base)`, `obj:IsChildOf(cls)` |
| read raw memory | `Memory.ReadInt32(addr)`, `Memory.ReadFloat(addr, offset)` |
| write raw memory | `Memory.WriteInt32(addr, value)` |
| persist something | `File.Write`, `JSON.Stringify` |
| read persisted state | `File.Read`, `JSON.Parse` |
| talk to the player | `Notify(...)`, `Toast(...)`, `Overlay.*` |
| react to an input | `RegisterKeyBind(...)`, `IsKeyDown(...)` |
| react to a function | `RegisterHook(...)` |
| react to a lifecycle moment | `RegisterLoadMapHook`, `RegisterBeginPlayHook`, `RegisterEndPlayHook` |
| build an in-game UI | `Overlay.RegisterWindow`, `Overlay.Button`, `Overlay.SliderFloat` |
| draw on the screen | `Overlay.RegisterDrawCallback`, `Overlay.DrawLine` |
| share state with another mod | `Mod:SetSharedVariable`, `ModRef(name):GetSharedVariable` |
| run a console command | `ExecuteConsoleCommand(cmd)` |
| read a CVar | `GetCVar(name)`, `SetCVar(name, value)`, `FindCVars()` |
| change time | `GetGameplayStatics():SetGlobalTimeDilation(world, scale)` |
| get editor autocomplete | `GenerateLuaTypes()` |
| dump the whole process | `DumpAllObjects()` |

## 49. The Kismet libraries

Each returns the CDO of a Blueprint-exposed utility class. These are the safest functions in the
engine to call, they are designed for scripting.

```lua
GetGameplayStatics()            -- spawn, time, players, saves, streaming
GetKismetSystemLibrary()        -- timers, math, printing, platform queries
GetKismetMathLibrary()          -- Abs, Lerp, VInterpTo, FindLookAtRotation, RInterpTo …
GetKismetStringLibrary()        -- Join, Split, Contains, Replace, ToUpper …
GetKismetArrayLibrary()         -- Add, Length, Get, Set, Shuffle …
GetKismetTextLibrary()
GetKismetGuidLibrary()
GetKismetRenderingLibrary()
```

```lua
local math3d = GetKismetMathLibrary()
print(math3d:Abs(-3))
print(math3d:Lerp(0, 100, 0.25))
local look = math3d:FindLookAtRotation(FVector(0,0,0), FVector(100,100,0))
print(look.Pitch, look.Yaw)
```

## 50. The `Math` convenience table

```lua
Math.Lerp(a, b, t)
Math.Clamp(x, min, max)
Math.VInterpTo(current, target, deltaTime, speed)
Math.RInterpTo(current, target, deltaTime, speed)
Math.FindLookAtRotation(start, target)
```

## 51. Console commands and CVars

```lua
RegisterConsoleCommandHandler("mymod.give", function(command, args)
    print("command:", command, "first arg:", args and args[1])
end)

ExecuteConsoleCommand("stat fps")

print(GetCVar("r.MotionBlurQuality"))
SetCVar("r.MotionBlurQuality", 0)

for i = 1, 100 do
    local name = FindCVars() and FindCVars()[i]
    if not name then break end
    print(i, name)
end
```
