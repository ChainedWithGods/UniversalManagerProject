# Part VI: Complete mod projects

This part is the payoff. Six mods, in increasing ambition, each with its full source, a
walkthrough of why every line exists, and a list of ways to grow it. They are written to run:
copy the folder into your mods directory, add it to `mods.json`, reload, and it works. We assume the placeholder class and property names get replaced; the code runs, but it
doesn't know the game's actual classes.

Every project follows the same shape:

```
Mods/<YourModsRoot>/<ProjectName>/
    scripts/main.lua      -- entry point, always
    scripts/*.lua         -- helpers, required from main
```

Recall the rules from §2: the folder name *is* the mod name, each mod gets its own globals
environment, and `require("helpers")` resolves inside *your* folder first. Everything below
relies only on the surface verified in Part II and Part III.

---

## Project 1: PlayerTweaks: movement, teleport, and cleanup

**Goal.** A small suite of player-facing tweaks that Avalonium can actually deliver: walk speed,
jump height, teleport, and bulk actor removal. This is the project that teaches *architecture*:
how to split a mod so it stays maintainable at 1 500 lines.

> **What this mod does NOT do.** Health, ammo, god mode, and inventory manipulation are
> intentionally absent. Most games do not expose those values as reflected properties; they are
> either computed every frame, stored in non-reflected C++ members, or managed by ability systems
> that Avalonium cannot reach. See §10.8 in Part II for the safe pattern when you do find a
> writable numeric property.

We don't poke raw memory for values that aren't reflected; that's a patch-time bomb, not a
design pattern.

**Files.**

```
PlayerTweaks/
    scripts/main.lua        -- wires everything together
    scripts/config.lua      -- load/save settings
    scripts/cheats.lua      -- the actual game manipulation
    scripts/ui.lua          -- the ImGui window
```

### `scripts/config.lua`

```lua
-- Config: JSON on disk, next to the mod, with defaults filled in.
local Config = {}

local DEFAULTS = {
    TimeDilation    = 1.0,
    JumpZVelocity   = 800.0,
    UIOpen          = false,
}

function Config.Path()
    return GetModDirectory() .. "/PlayerTweaks/settings.json"
end

local values = {}

function Config.Load()
    local raw = File.Read(Config.Path())
    local parsed = raw and JSON.Parse(raw) or nil

    for key, default in pairs(DEFAULTS) do
        if type(parsed) == "table" and parsed[key] ~= nil then
            values[key] = parsed[key]
        else
            values[key] = default
        end
    end

    print("PlayerTweaks: config loaded from", Config.Path())
    return values
end

function Config.Save()
    local ok = File.Write(Config.Path(), JSON.Stringify(values))
    if not ok then
        File.CreateDir(GetModDirectory() .. "/PlayerTweaks")
        ok = File.Write(Config.Path(), JSON.Stringify(values))
    end
    return ok
end

function Config.Get(key) return values[key] end
function Config.Set(key, value) values[key] = value end

return Config
```

### `scripts/cheats.lua`

```lua
-- Tweaks: everything that touches the game lives here, and nothing else does.
local Config = require("config")

local Tweaks = {}
local applied = { speed = nil, jump = nil }

-- Resolve the player's character movement component, tolerating every shape it
-- can take in a shipped game.
function Tweaks.Movement()
    local player = GetPlayer()
    if not (player and player:IsValid()) then return nil end

    local move = player.CharacterMovement
    if move and move:IsValid() then return move end

    move = player:GetComponent("CharacterMovementComponent")
    if move and move:IsValid() then return move end

    return nil
end

-- Remember the original values so "off" really means "off".
local function Remember(key, current)
    if applied[key] == nil and type(current) == "number" then applied[key] = current end
end

function Tweaks.ApplyTimeDilation()
    local player = GetPlayer()
    if not (player and player:IsValid()) then return false end
    Remember("dilation", player.CustomTimeDilation)
    player.CustomTimeDilation = Config.Get("TimeDilation")
    return true
end

function Tweaks.ApplyJump()
    local move = Tweaks.Movement()
    if not (move and move:IsValid()) then return false end
    Remember("jump", move.JumpZVelocity)
    move.JumpZVelocity = Config.Get("JumpZVelocity")
    return true
end

function Tweaks.RestoreDefaults()
    local player = GetPlayer()
    local move = Tweaks.Movement()
    if player and player:IsValid() and applied.dilation then
        player.CustomTimeDilation = applied.dilation
    end
    if move and move:IsValid() and applied.jump then
        move.JumpZVelocity = applied.jump
    end
    applied.dilation, applied.jump = nil, nil
end

function Tweaks.TeleportTo(x, y, z)
    local player = GetPlayer()
    if not (player and player:IsValid()) then return false end
    player:SetLocation(FVector(x, y, z))
    return true
end

function Tweaks.TeleportUp(delta)
    local player = GetPlayer()
    if not (player and player:IsValid()) then return false end
    local loc = player:GetLocation()
    player:SetLocation(FVector(loc.X, loc.Y, loc.Z + delta))
    return true
end

function Tweaks.KillEverything(className)
    local killed = 0
    for _, actor in ipairs(GetAllActorsOfClass(className or "Pawn")) do
        if actor:IsValid() and actor ~= GetPlayer() then
            actor:DestroyActor()
            killed = killed + 1
        end
    end
    return killed
end

return Tweaks
```

### `scripts/ui.lua`

```lua
-- UI: draws the palette. It reads config, writes config, and calls tweaks.
-- It never touches an engine property itself.
local Config = require("config")
local Tweaks = require("cheats")

local UI = {}
local status = "ready"

local function Note(message)
    status = message
    print("PlayerTweaks:", message)
end

function UI.Draw()
    if not Config.Get("UIOpen") then return end

    Overlay.TextColored("PlayerTweaks", 0.4, 1.0, 0.4, 1.0)   -- text first, then rgb(a)
    Overlay.Text("status: " .. status)
    Overlay.Separator()

    -- Speed (via CustomTimeDilation, works in ALL UE games including root-motion titles)
    local speed, moved = Overlay.SliderFloat("Time dilation", Config.Get("TimeDilation"), 0.5, 3.0)
    if moved then
        Config.Set("TimeDilation", speed)
        Tweaks.ApplyTimeDilation()
        status = "time dilation " .. string.format("%.2f", speed)
    end

    -- Jump
    local jump, jumped = Overlay.SliderFloat("Jump velocity", Config.Get("JumpZVelocity"), 0, 4000)
    if jumped then
        Config.Set("JumpZVelocity", jump)
        Tweaks.ApplyJump()
    end

    Overlay.Separator()

    if Overlay.Button("Teleport +500 up") then Tweaks.TeleportUp(500) end
    Overlay.SameLine()
    if Overlay.Button("Restore defaults") then Tweaks.RestoreDefaults(); Note("defaults restored") end

    Overlay.Separator()

    if Overlay.Button("Save settings") then
        Note(Config.Save() and "settings saved" or "save FAILED")
    end

    Overlay.Text("F7 toggles this window.")
end

return UI
```

### `scripts/main.lua`

```lua
-- main.lua: the only file the runtime loads. It wires the three modules together.
local Config = require("config")
local Tweaks = require("cheats")
local UI     = require("ui")

Config.Load()

-- Push saved settings into the game once the world exists.
ExecuteWithDelay(2000, function()
    Tweaks.ApplyTimeDilation()
    Tweaks.ApplyJump()
    print("PlayerTweaks: applied saved settings")
end)

Overlay.RegisterWindow("PlayerTweaks", UI.Draw)

-- The menu is global; our panel is one window inside it.
RegisterKeyBind(Key.F7, function()
    Config.Set("UIOpen", not Overlay.IsOpen())
    Overlay.Toggle()
end)

-- Console-friendly entry point: run "pt.speed 1200" in the game console.
RegisterConsoleCommandHandler("pt.speed", function(cmd, args)
    local v = tonumber(args[1] or args[2] or "")
    if v then
        Config.Set("TimeDilation", v)
        Tweaks.ApplyTimeDilation()
    end
end)

-- Cleanup: hooks and timers die on reload anyway, but settings should persist.
Mod.OnUnload = function()
    Config.Save()
    Tweaks.RestoreDefaults()
    print("PlayerTweaks unloaded -- settings saved")
end

print("PlayerTweaks loaded -- F7 for the palette")
```

### Why it is written this way

* **Three layers, one direction of dependency.** `main` → `ui` → `cheats` → `config`. The UI
  never writes an engine property; the tweaks layer never draws. That means you can reuse
  `cheats.lua` in a different mod with a different UI and change nothing.
* **`config.lua` fills in defaults for missing keys.** A settings file written by an older
  version of your mod will not crash a newer one; that is the whole point of the loop over
  `DEFAULTS`.
* **`Remember`/`RestoreDefaults`.** Any tweak that overwrites an engine value must be
  reversible, or the player's save can end up permanently modified. Capture the original the
  first time you touch it. I persist the original separately when it matters: `Remember` is per-load, and a reload
  records the modded value as "original" because the saved one was in the old Lua state.
* **`Tweaks.Movement()` tries three shapes.** Shipped games spell the movement component
  differently; a resilient accessor beats a hard-coded path.
* **`Mod.OnUnload` saves and restores.** Reloads are frequent during development; treat unload
  as a first-class event, not an afterthought.

### Grow it

1. **Presets.** Add a `Presets` table to the config (`speedrun`, `cinematic`, `exploration`) and a
   dropdown that applies a whole set at once.
2. **Per-game adaptation.** Put the property names (`CustomTimeDilation`, `JumpZVelocity`) in the
   config too, then ship one mod that works across several games.
3. **Undo stack.** Record `(object, property, oldValue)` triples and add an "undo last" button.
4. **Multiplayer honesty.** Nothing here is safe on a listen server; add a guard that disables
   the palette when `GetGameState()` reports more than one player.

---

## Project 2: FlightRecorder: session telemetry that writes itself

**Goal.** Quietly record what happens in a session, spawns, deaths, level loads, frame timing,
object counts, and write a browsable JSON file. This is what you build when a bug is
intermittent and you need evidence, and it doubles as a performance profiler for your own mods.

**Files.** `FlightRecorder/scripts/main.lua`, plus an optional `scripts/report.lua`.

```lua
-- FlightRecorder -- a black box for your session.
local session = {
    started    = os.time(),
    events     = {},          -- ring buffer of {t=seconds, kind=..., text=...}
    maxEvents  = 5000,
    samples    = {},          -- periodic counters
    levelLoads = 0,
    spawns     = 0,
    kills      = 0,
}

local START = os.clock()

local function Now() return os.clock() - START end

local function Record(kind, text)
    local n = #session.events + 1
    if n > session.maxEvents then
        table.remove(session.events, 1)
        n = n - 1
    end
    session.events[n] = { t = math.floor(Now() * 100) / 100, kind = kind, text = text }
end

-- 1. Lifecycle events. RegisterLoadMapHook fires with the new world as `self`.
RegisterLoadMapHook(function(world)
    session.levelLoads = session.levelLoads + 1
    Record("level", world and world:GetName() or "?")
end)

RegisterBeginPlayHook(function(actor)
    -- BeginPlay fires for every actor in the game. Keep it cheap: count, don't log.
    session.spawns = session.spawns + 1
end)

RegisterEndPlayHook(function(actor)
    session.kills = session.kills + 1
end)

-- 2. A targeted spawn log: only actors whose class name matches, so the log
--    stays readable in a busy game.
local WATCH = { "Enemy", "Boss", "Pickup", "Projectile" }

local function Watched(name)
    for _, pattern in ipairs(WATCH) do
        if name:find(pattern, 1, true) then return pattern end
    end
    return nil
end

RegisterBeginPlayPostHook(function(actor)
    if not (actor and actor:IsValid()) then return end
    local name = actor:GetName()
    local tag = Watched(name)
    if tag then
        Record("spawn:" .. tag, name)
    end
end)

-- 3. Periodic sampling: fps, memory, live actor count, player position.
--    Ten seconds apart keeps the file small and the overhead invisible.
LoopAsync(10000, function()
    local player = GetPlayer()
    local sample = {
        t        = math.floor(Now() * 100) / 100,
        fps      = math.floor(GetFPS() * 10) / 10,
        frameMs  = math.floor(GetFrameTime() * 10000) / 10,
        memoryMB = math.floor(GetMemoryUsage() / (1024 * 1024) * 10) / 10,
        objects  = GetObjectCount(),
        actors   = #GetWorldActors(),
    }

    if player and player:IsValid() then
        local loc = player:GetLocation()
        sample.pos = { x = math.floor(loc.X), y = math.floor(loc.Y), z = math.floor(loc.Z) }
    end

    local n = #session.samples + 1
    session.samples[n] = sample
end)

-- 4. A manual bookmark key: mark the interesting moment while playing.
RegisterKeyBind(Key.F8, function()
    local player = GetPlayer()
    local where = player and player:IsValid() and player:GetName() or "nowhere"
    Record("bookmark", where)
    Notify("bookmarked", "info")
end)

-- 5. On unload, flush everything to disk.
local function Flush()
    local dir = GetModDirectory() .. "/FlightRecorder"
    File.CreateDir(dir)

    local summary = {
        started      = session.started,
        durationS    = math.floor(Now()),
        levelLoads   = session.levelLoads,
        beginPlay    = session.spawns,
        endPlay      = session.kills,
        sampleCount  = #session.samples,
        eventCount   = #session.events,
    }

    local payload = {
        summary = summary,
        samples = session.samples,
        events  = session.events,
    }

    local path = dir .. "/session.json"
    local ok = File.Write(path, JSON.Stringify(payload))
    print("FlightRecorder: wrote", path, ok and "ok" or "FAILED")
    return ok
end

Mod.OnUnload = Flush

-- Also flush on demand so you do not have to unload to see data.
RegisterKeyBind(Key.F10, function()
    if Flush() then Notify("session written to session.json", "success") end
end)

-- 6. A live readout in the stats HUD, so you can watch it while playing.
Overlay.SetStat("Recorder", "events " .. #session.events)
LoopAsync(1000, function()
    Overlay.SetStat("Recorder", ("ev %d \194\183 spawn %d \194\183 lvl %d"):format(
        #session.events, session.spawns, session.levelLoads))
end)

Record("boot", "FlightRecorder loaded")
print("FlightRecorder loaded -- F8 bookmark, F10 write")
```

### Why it is written this way

* **A ring buffer, not an ever-growing table.** `maxEvents` caps memory use; a four-hour
  session cannot exhaust RAM because someone left the game running. We cap the events, but `session.samples` is unbounded, ten-second sampling adds one entry
  forever, so a multi-day session still grows.
* **`RegisterBeginPlayHook` counts, `RegisterBeginPlayPostHook` logs.** `BeginPlay` fires for
  *every* actor; log lines there would be the single biggest cost in the mod. Counting is O(1);
  the filtered post-hook writes at most a few lines per second.
* **`os.clock()` instead of `os.time()`** for durations, `os.time()` has one-second
  granularity, `os.clock()` gives you sub-millisecond resolution within the process.
* **Sampling every 10 s**: not every frame. Frame-rate telemetry every frame would itself
  change the frame rate.
* **`File.CreateDir` before `File.Write`.** `File.Write` creates missing *parent directories*
  on most builds, but doing it explicitly makes the intent obvious and the failure obvious.
* **The local `Flush` closure is the only writer**: called from `Mod.OnUnload` and from a key.
  One code path to disk means one place to fix.

### Grow it

1. **HTML report.** `JSON` is written by hand here, but nothing stops you generating a
   `report.html` with a `<canvas>` timeline; the samples are already time-stamped.
2. **Diff two sessions.** Load two `session.json` files and print the fps delta per zone.
3. **Auto-flush on level load.** Call `Flush()` from the load-map hook so a crash still leaves
   a usable file behind.
4. **Mod attribution.** Wrap `print` with a per-mod counter to record how much each installed
   mod logs, a startlingly effective way to find a noisy mod.

---

## Project 3: DirectorMode: slow-motion, pauses and cinematics

**Goal.** A cinematic-control mod: global time dilation, a key-driven bullet-time toggle, a
scheduled slowdown that snaps back, and a "freeze" that stops everything. This project is about
**time**, **state machines**, and **composing timers safely**.

**Files.** `DirectorMode/scripts/main.lua`.

```lua
-- DirectorMode -- control the flow of time.
local Director = {}

local state = {
    current      = 1.0,
    restoreTo    = 1.0,
    bulletTime   = false,
    freeze       = false,
    schedule     = {},      -- { atRelMs, scale, holdMs, label }
    scheduleArmed = false,
}

-- 1. The one place that touches time dilation.
--    Prefer UGameplayStatics; fall back to the world settings property so a build
--    without the reflected function still works.
local function SetDilation(scale)
    local world = GetWorld()
    if not (world and world:IsValid()) then return false end

    local ok = pcall(function()
        GetGameplayStatics():SetGlobalTimeDilation(world, scale)
    end)

    if not ok then
        local settings = world.WorldSettings
        if settings and settings:IsValid() and type(settings.TimeDilation) == "number" then
            settings.TimeDilation = scale
        else
            return false
        end
    end

    state.current = scale
    Overlay.SetStat("Time", ("x%.2f"):format(scale))
    return true
end

function Director.Set(scale, label)
    if state.current == scale then return true end
    local ok = SetDilation(scale)
    if ok and label then print(("DirectorMode: %s -> x%.2f"):format(label, scale)) end
    return ok
end

function Director.ToggleBulletTime(slowScale, normalLabel)
    state.bulletTime = not state.bulletTime
    if state.bulletTime then
        state.restoreTo = state.current
        Director.Set(slowScale or 0.25, "bullet time")
        Notify("Bullet time ON", "info")
    else
        Director.Set(state.restoreTo or 1.0, "normal speed")
        Notify("Bullet time OFF", "info")
    end
end

function Director.ToggleFreeze()
    state.freeze = not state.freeze
    if state.freeze then
        state.restoreTo = state.current
        Director.Set(0.0001, "freeze")      -- not 0: some systems divide by dilation
        Notify("World frozen", "warning")
    else
        Director.Set(state.restoreTo or 1.0, "unfrozen")
        Notify("World running", "success")
    end
end

-- 2. A one-shot slowdown that restores itself.
function Director.SlowFor(ms, scale)
    scale = scale or 0.15
    local previous = state.current
    Director.Set(scale, ("slow %dms"):format(ms))

    ExecuteWithDelay(ms, function()
        -- Only restore if nothing else moved time while we waited.
        if state.current == scale then
            Director.Set(previous, "slow finished")
        end
    end)
end

-- 3. A schedule: "at +5s slow to 0.2 for 2s, then at +10s slow to 0.05 for 1s".
function Director.ArmDefaultSchedule()
    if state.scheduleArmed then
        Notify("schedule already armed", "warning")
        return
    end
    state.scheduleArmed = true

    state.schedule = {
        { atRelMs = 5000,  scale = 0.20, holdMs = 2000, label = "first hit" },
        { atRelMs = 10000, scale = 0.05, holdMs = 1000, label = "climax"   },
        { atRelMs = 15000, scale = 1.50, holdMs = 3000, label = "rush"     },
    }

    local previous = state.current

    for i, entry in ipairs(state.schedule) do
        ExecuteWithDelay(entry.atRelMs, function()
            Director.Set(entry.scale, "schedule " .. entry.label)
            ExecuteWithDelay(entry.holdMs, function()
                if state.current == entry.scale then
                    Director.Set(previous, "schedule release " .. entry.label)
                end
            end)
        end)
    end

    print(("DirectorMode: armed %d schedule entries"):format(#state.schedule))
end

-- 4. Let the game tell us when something dramatic happened.
--    Here: a one-second slow-mo every time a watched actor begins play.
local AUTO_WATCH = { "Boss", "Explosion" }
local autoSlowEnabled = false

RegisterBeginPlayPostHook(function(actor)
    if not autoSlowEnabled then return end
    if not (actor and actor:IsValid()) then return end
    local name = actor:GetName()
    for _, pattern in ipairs(AUTO_WATCH) do
        if name:find(pattern, 1, true) then
            Director.SlowFor(800, 0.15)
            return
        end
    end
end)

-- 5. Controls.
RegisterKeyBind({Key.T, "LeftControl"}, function()
    Director.ToggleBulletTime(0.25)
end)

RegisterKeyBind({Key.F, "LeftControl"}, function()
    Director.ToggleFreeze()
end)

RegisterKeyBind({Key.S, "LeftControl"}, function()
    Director.SlowFor(1500, 0.2)
end)

RegisterKeyBind({Key.D, "LeftControl"}, function()
    Director.ArmDefaultSchedule()
end)

RegisterKeyBind({Key.O, "LeftControl"}, function()
    autoSlowEnabled = not autoSlowEnabled
    Notify(autoSlowEnabled and "auto slow-mo ON" or "auto slow-mo OFF", "info")
end)

-- 6. Restore real time on unload, or the player is left in bullet time.
Mod.OnUnload = function()
    state.bulletTime, state.freeze = false, false
    Director.Set(1.0, "unload restore")
    print("DirectorMode unloaded -- time restored")
end

print("DirectorMode loaded -- Ctrl+T bullet time, Ctrl+F freeze")
```

### Why it is written this way

* **One writer for time.** Every path, keybind, schedule, hook, unload, funnels through
  `Director.Set`. If you have three places calling `SetGlobalTimeDilation`, you will eventually
  fight yourself.
* **`pcall` around the reflected call, with a property fallback.** Some builds strip the
  function; the `WorldSettings.TimeDilation` property is the underlying storage and is almost
  always reflected.
* **Never dilation `0`.** Zero is a division hazard in several engine systems and can lock the
  frame loop. `0.0001` is visually frozen and mathematically safe.
* **"Restore if unchanged" guards.** `SlowFor` only restores when the value is still what it
  set; that is how overlapping effects avoid stomping each other.
* **`Mod.OnUnload` restores real time.** A mod that leaves the game at 0.25x after a reload is
  a bug report waiting to happen.

### Grow it

1. **Camera-aware cinematics.** Combine with the camera manager to add a dolly move while time
   is slowed.
2. **Config-driven schedules.** Load the schedule table from `settings.json` so users can
   author their own cinematics without touching Lua.
3. **Damage-triggered impact freeze.** Hook a damage function and call `Director.SlowFor(120, 0.1)`
   for a "hit stop" feel.
4. **Interpolation instead of steps.** Replace `Director.Set` with a 10-frame ramp so the
   slowdown eases in rather than snapping.

---

## Project 4: LiveBalance: an in-game DataTable editor

**Goal.** Browse any DataTable in the running game, edit a column, and see the change take
effect immediately, no restart, no asset rebuild. This is the mod you build when you are
tuning numbers, and it demonstrates the DataTable surface, table UX, and safe commit semantics.

**Files.** `LiveBalance/scripts/main.lua`.

```lua
-- LiveBalance -- edit any DataTable row in-game.
local state = {
    open       = false,
    tables     = {},        -- discovered UDataTable objects
    tableIndex = 1,
    rows       = {},        -- row names for the current table
    rowIndex   = 1,
    fields     = {},        -- editable fields for the current row
    fieldIndex = 1,
    editValue  = 0,
    dirty      = false,
}

-- 1. Discover tables. This is a one-shot scan; DataTables are ordinary UObjects.
local function DiscoverTables()
    state.tables = {}
    ForEachUObjectFlat(function(obj)
        if not obj or not obj:IsValid() then return end
        if obj:IsA("DataTable") then
            state.tables[#state.tables + 1] = obj
        end
    end)
    table.sort(state.tables, function(a, b) return a:GetName() < b:GetName() end)
    print(("LiveBalance: found %d DataTables"):format(#state.tables))
end

local function CurrentTable() return state.tables[state.tableIndex] end

local function CurrentRowName() return state.rows[state.rowIndex] end

-- 2. Re-read rows and fields whenever the selection changes.
local function RefreshRows()
    state.rows, state.rowIndex, state.fields, state.fieldIndex = {}, 1, {}, 1
    local dt = CurrentTable()
    if not (dt and dt:IsValid()) then return end

    local names = dt:GetRowNames()
    for i = 1, #names do state.rows[i] = names[i] end
    RefreshFields()
end

local function RefreshFields()
    state.fields, state.fieldIndex = {}, 1
    local dt = CurrentTable()
    local rowName = CurrentRowName()
    if not (dt and dt:IsValid() and rowName) then return end

    local row = dt:FindRow(rowName)
    if type(row) ~= "table" then return end

    -- Only numbers are editable in this UI; strings/bools are displayed read-only.
    for key, value in pairs(row) do
        state.fields[#state.fields + 1] = { key = key, value = value, kind = type(value) }
    end
    table.sort(state.fields, function(a, b) return a.key < b.key end)
end

-- 3. Commit: read the row, change one key, write it back through the engine.
local function CommitField(field, newValue)
    local dt = CurrentTable()
    local rowName = CurrentRowName()
    if not (dt and dt:IsValid() and rowName) then return false end

    local row = dt:FindRow(rowName)
    if type(row) ~= "table" then return false end

    local old = row[field.key]
    row[field.key] = newValue

    local ok = dt:AddRow(rowName, row)
    if ok then
        print(("LiveBalance: %s[%s].%s  %s -> %s"):format(
            dt:GetName(), rowName, field.key, tostring(old), tostring(newValue)))
        field.value = newValue
        state.dirty = true
    else
        print("LiveBalance: AddRow failed -- change not applied")
    end
    return ok
end

local function NudgeSelectedField(delta)
    local field = state.fields[state.fieldIndex]
    if not field then return end
    if field.kind ~= "number" then
        Notify("field " .. field.key .. " is not numeric", "warning")
        return
    end

    -- Scale the step to the magnitude so big numbers move fast and small ones stay precise.
    local magnitude = math.max(math.abs(field.value), 1)
    local step = 10 ^ (math.floor(math.log(magnitude, 10)) - 2)
    if step <= 0 then step = 1 end

    CommitField(field, field.value + delta * step)
end

-- 4. The window.
Overlay.RegisterWindow("LiveBalance", function()
    -- No gate needed: the body only runs while the in-game menu is open.
    if #state.tables == 0 then
        Overlay.Text("No DataTables found.")
        if Overlay.Button("Rescan") then DiscoverTables() end
        return
    end

    local dt = CurrentTable()
    Overlay.Text(("Table %d/%d: %s"):format(state.tableIndex, #state.tables,
        dt and dt:IsValid() and dt:GetName() or "<invalid>"))
    Overlay.Text(("Rows: %d"):format(#state.rows))
    Overlay.Separator()

    if Overlay.Button("< table") then
        state.tableIndex = math.max(1, state.tableIndex - 1)
        RefreshRows()
    end
    Overlay.SameLine()
    if Overlay.Button("table >") then
        state.tableIndex = math.min(#state.tables, state.tableIndex + 1)
        RefreshRows()
    end

    Overlay.Separator()
    Overlay.Text(("Row %d/%d: %s"):format(state.rowIndex, #state.rows, CurrentRowName() or "<none>"))

    if Overlay.Button("< row") then
        state.rowIndex = math.max(1, state.rowIndex - 1)
        RefreshFields()
    end
    Overlay.SameLine()
    if Overlay.Button("row >") then
        state.rowIndex = math.min(#state.rows, state.rowIndex + 1)
        RefreshFields()
    end

    Overlay.Separator()

    local field = state.fields[state.fieldIndex]
    if not field then
        Overlay.Text("(no editable fields)")
        return
    end

    Overlay.Text(("Field %d/%d: %s"):format(state.fieldIndex, #state.fields, field.key))
    Overlay.Text(("value (%s): %s"):format(field.kind, tostring(field.value)))

    if Overlay.Button("< field") then
        state.fieldIndex = math.max(1, state.fieldIndex - 1)
    end
    Overlay.SameLine()
    if Overlay.Button("field >") then
        state.fieldIndex = math.min(#state.fields, state.fieldIndex + 1)
    end

    Overlay.Separator()

    if Overlay.Button("- 1%") then NudgeSelectedField(-1) end
    Overlay.SameLine()
    if Overlay.Button("+ 1%") then NudgeSelectedField(1) end

    Overlay.Text(state.dirty and "changes applied live" or "no changes yet")
end)

-- 5. Controls: discover on demand, open with a key.
RegisterKeyBind({Key.B, "LeftControl"}, function()
    DiscoverTables()
    RefreshRows()
    state.open = true
    Overlay.Open()                       -- opens the shared in-game menu
    Notify("LiveBalance: " .. #state.tables .. " tables", "info")
end)

RegisterKeyBind({Key.B, "LeftAlt"}, function()
    state.open = not Overlay.IsOpen()
    Overlay.Toggle()
end)

print("LiveBalance loaded -- Ctrl+B scans tables, Alt+B toggles the window")
```

### Why it is written this way

* **Discovery is a scan, not a lookup.** You cannot `FindObject("MyTable")` unless you already
  know the name, and half the time you do not. `obj:IsA("DataTable")` plus `ForEachUObjectFlat`
  finds the universe; the sort makes it navigable.
* **Three-level navigation** (table → row → field) matches how the data is actually shaped, and
  each level refreshes only the level below it.
* **`FindRow` returns a plain table.** Editing that table changes nothing in the engine, you
  must `AddRow` the table back. We keep one commit path because Lua sees a copy, not a live
  view; a write that looks successful on the table does nothing until `AddRow` pushes it back.
* **Magnitude-aware step size.** `10^(floor(log10(v)) - 2)` means a value of `1.0` moves by
  `0.01`, and `50 000` moves by `500`. A fixed step is useless across both scales.
* **`state.dirty` instead of a modal "unsaved changes" prompt.** Live editing means there is
  nothing to save; the flag just tells you whether this session has changed anything.
* **Non-numeric fields are visible but not editable.** Silently coercing strings into numbers
  is how you corrupt a table.

### Grow it

1. **Text entry for strings and bools.** Add a tiny editor or cycle through
   `true`/`false` with a checkbox.
2. **Diff view.** Snapshot a row when you first open it and show old vs new next to each field.
3. **Export/import.** `JSON.Stringify(dt:GetAllRows())` writes a whole table to disk; import it
   back to share a balance tuning with a friend.
4. **Row creation.** `dt:AddRow(name, template)` lets you clone an existing row into a new one, the core of a content mod.

---

## Project 5: Hooked: surgical gameplay patches

**Goal.** Change how the game *behaves* rather than how its data reads: cancel a function
conditionally, rewrite a parameter before the engine sees it, read the return value after.
This is the deepest power Avalonium exposes and the one with the sharpest edges, so this
project is written defensively on purpose.

**Files.** `Hooked/scripts/main.lua` plus `Hooked/scripts/patches.lua`.

### The three shapes of a patch

Before any code, internalise the three things a hook can do. Everything in this project is one
of them.

```lua
RegisterHook("SomeFunction",

    -- PRE: runs before the engine. Return truthy to CANCEL the engine call.
    function(self, a, b)
        -- a and b are live parameter proxies: reading fetches engine storage,
        -- writing writes back into the engine's frame.
        if b:get() < 0 then return true end        -- cancel: no call happens
        b:set(b:get() * 2)                         -- rewrite the argument
    end,

    -- POST: runs after. The engine's return value is in __OriginalReturnValue.
    function(self, a, b)
        local original = __OriginalReturnValue
        print("returned", tostring(original))
    end
)
```

### `scripts/patches.lua`

```lua
-- patches.lua -- each patch is a self-contained install function that returns its
-- uninstall function. Keeping the pair together is what makes them safe to reload.

local Patches = {}
local installed = {}     -- name -> { pre = id, post = id }

local function Track(name, preId, postId)
    installed[name] = { pre = preId, post = postId }
    print(("Hooked: installed '%s' (pre=%s post=%s)"):format(name, tostring(preId), tostring(postId)))
end

-- ---------------------------------------------------------------------------
-- Patch 1: negate incoming damage for the local player.
-- ---------------------------------------------------------------------------
function Patches.NoPlayerDamage()
    local preId, postId = RegisterHook("TakeDamage", function(self, amount, damageEvent)
        local player = GetPlayer()
        if player and self == player then
            if amount and amount.get then
                print("Hooked: suppressed damage", tostring(amount:get()))
            end
            return true                 -- cancel the engine call entirely
        end
    end)
    Track("TakeDamage:noplayerdamage", preId, postId)
end

-- ---------------------------------------------------------------------------
-- Patch 2: halve every spawn cost / double every reward, by name pattern.
-- ---------------------------------------------------------------------------
function Patches.DoubleRewards()
    local preId, postId = RegisterHook("GrantReward", function(self, amount)
        if amount and amount.get and amount.set then
            local value = amount:get()
            if type(value) == "number" then
                amount:set(value * 2)
                print(("Hooked: reward %s -> %s"):format(value, value * 2))
            end
        end
    end)
    Track("GrantReward:double", preId, postId)
end

-- ---------------------------------------------------------------------------
-- Patch 3: log the engine's return, without changing behaviour at all.
--    Observation is the cheapest patch to write and the one you should write first.
-- ---------------------------------------------------------------------------
function Patches.LogFunctionReturn(functionName)
    functionName = functionName or "GetPlayerScore"
    local preId, postId = RegisterHook(functionName, nil, function(self)
        print(("Hooked: %s returned %s"):format(functionName, tostring(__OriginalReturnValue)))
    end)
    Track(functionName .. ":log", preId, postId)
end

-- ---------------------------------------------------------------------------
-- Patch 4: this-frame only -- a patch that acts once and uninstalls itself.
-- ---------------------------------------------------------------------------
function Patches.Once(functionName, callback)
    local preId
    preId = RegisterHook(functionName, function(...)
        UnregisterHook(preId)
        installed[functionName] = nil
        return callback(...)
    end)
    Track(functionName .. ":once", preId, 0)
end

-- ---------------------------------------------------------------------------
-- Patch 5: a custom event -- your own pipeline other mods can hook.
--    RegisterCustomEvent creates the name; ExecuteCommand-style dispatch is up to
--    you, but the pre/post pair runs like any engine hook.
-- ---------------------------------------------------------------------------
function Patches.InstallEventBus()
    RegisterCustomEvent("AvaloniumHookExample.OnEnemyKilled",
        function(name, payload)
            print(("Hooked: event '%s' fired with %s"):format(name, tostring(payload)))
        end)
    print("Hooked: event bus installed")
end

function Patches.UninstallAll()
    local count = 0
    for name, ids in pairs(installed) do
        if ids.pre and ids.pre ~= 0 then UnregisterHook(ids.pre) end
        if ids.post and ids.post ~= 0 then UnregisterHook(ids.post) end
        count = count + 1
    end
    installed = {}
    print(("Hooked: uninstalled %d patches"):format(count))
end

function Patches.List()
    local names = {}
    for name in pairs(installed) do names[#names + 1] = name end
    table.sort(names)
    return names
end

return Patches
```

### `scripts/main.lua`

```lua
local Patches = require("patches")

-- Observation first: install only the non-destructive patch at boot, so a
-- mis-targeted name can never break the game.
Patches.LogFunctionReturn("GetPlayerScore")

-- The destructive patches are behind an explicit key. Deliberate friction.
local armed = false

RegisterKeyBind({Key.H, "LeftControl"}, function()
    armed = not armed
    if armed then
        Patches.NoPlayerDamage()
        Patches.DoubleRewards()
        Notify("Hooked: patches ARMED", "warning")
    else
        Patches.UninstallAll()
        Patches.LogFunctionReturn("GetPlayerScore")
        Notify("Hooked: patches disarmed", "success")
    end
end)

RegisterKeyBind({Key.L, "LeftControl"}, function()
    local names = Patches.List()
    print("Hooked: active patches: " .. table.concat(names, ", "))
end)

-- Reload wipes hooks engine-side; our bookkeeping must not lie about that.
Mod.OnUnload = function()
    Patches.UninstallAll()
    print("Hooked unloaded")
end

print("Hooked loaded -- Ctrl+H arms the patch set, Ctrl+L lists them")
```

### Why it is written this way

* **Observation before mutation.** `LogFunctionReturn` proves the hook fires on the function you
  think it does, with the parameters you think it has, before you start rewriting arguments.
  Ninety percent of "hooks do not work" is a function name or a self object that is not what you
  assumed; one logged call answers both questions.
* **Install/uninstall pairs live together.** `Patches.NoPlayerDamage` knows how to install
  itself; `UninstallAll` uses the tracked ids. That symmetry is what makes a patch mod safe to
  toggle at runtime.
* **`UnregisterHook(id)` with the id you got back**: never by re-deriving the name. We unregister
  by id when more than one patch shares a function name; a name is not a unique key.
* **Destructive patches sit behind a deliberate key press.** The boot path installs only the
  read-only patch: if a name is wrong, the consequence is a log line, not a broken game.
* **`Patches.Once` shows the self-uninstalling pattern**: which is exactly what you want for
  "let the first call through, then disable myself" behaviours.
* **Reload semantics are stated in code.** Hooks never survive a reload, so `OnUnload` clears
  the registry, the mod must not claim patches exist when they do not.

### Grow it

1. **A patch browser UI.** Reuse Project 4's three-level navigation: class → function → patch
   type, and install a logging patch on any function you click.
2. **Argument recording.** Capture the first N calls of a function into a table and dump it, instant documentation of an undocumented API.
3. **Conditional cancels.** Add a predicate table so `NoPlayerDamage` can express, "only when the local player is below 10 % health" without editing the hook body (if the game exposes a readable health property).
4. **Hot-toggle per patch** with individual keys, and persist the enabled set in JSON so a reload restores exactly the patched state you were testing.

---

## Project 6: ModKit: a library other mods depend on

**Goal.** Ship a *service* rather than a feature: a mod that other mods consume. It provides
logging with levels, per-mod configuration, a cleanup-safe timer registry, a reusable ImGui
toolkit, and a per-frame tick bus, and it publishes itself through a shared variable so any
other mod can `require` it at runtime.

This project is where everything else in this guide converges. It is the single highest-leverage
thing you can write if you intend to publish more than one mod.

**Files.**

```
ModKit/
    scripts/main.lua       -- builds the API table, publishes it, installs the draw loop
    scripts/logger.lua
    scripts/store.lua
    scripts/timers.lua
    scripts/widgets.lua
```

### `scripts/logger.lua`

```lua
-- logger.lua -- levelled logging with an in-memory ring buffer per consumer.
local Logger = {}
local LEVELS = { trace = 1, debug = 2, info = 3, warn = 4, error = 5 }

local sinks = {}          -- ownerName -> { level = n, lines = {}, max = n }

function Logger.Sink(ownerName, level, maxLines)
    sinks[ownerName] = {
        level = LEVELS[level or "info"] or LEVELS.info,
        lines = {},
        max   = maxLines or 200,
    }
    return sinks[ownerName]
end

local function Emit(ownerName, level, message)
    local sink = sinks[ownerName]
    if not sink then return end
    if (LEVELS[level] or 0) < sink.level then return end

    local line = ("[%s][%s] %s"):format(ownerName, level:upper(), tostring(message))
    print(line)

    local n = #sink.lines + 1
    if n > sink.max then table.remove(sink.lines, 1); n = n - 1 end
    sink.lines[n] = line
end

function Logger.Make(ownerName)
    Logger.Sink(ownerName)
    return {
        trace = function(msg) Emit(ownerName, "trace", msg) end,
        debug = function(msg) Emit(ownerName, "debug", msg) end,
        info  = function(msg) Emit(ownerName, "info",  msg) end,
        warn  = function(msg) Emit(ownerName, "warn",  msg) end,
        error = function(msg) Emit(ownerName, "error", msg) end,
        lines = function() return sinks[ownerName].lines end,
    }
end

return Logger
```

### `scripts/store.lua`

```lua
-- store.lua -- per-mod JSON settings with defaults and dirty tracking.
local Store = {}

local function DeepFill(target, defaults)
    for key, default in pairs(defaults) do
        if target[key] == nil then
            target[key] = default
        elseif type(default) == "table" and type(target[key]) == "table" then
            DeepFill(target[key], default)
        end
    end
    return target
end

function Store.New(ownerName, defaults)
    local dir  = GetModDirectory() .. "/" .. ownerName
    local path = dir .. "/settings.json"
    local data, dirty = {}, false

    local raw = File.Read(path)
    local parsed = raw and JSON.Parse(raw) or nil
    if type(parsed) == "table" then
        data = DeepFill(parsed, defaults or {})
    else
        data = DeepFill({}, defaults or {})
        dirty = true
    end

    local api = {}

    function api.Get(key) return data[key] end

    function api.Set(key, value)
        if data[key] ~= value then
            data[key] = value
            dirty = true
        end
        return value
    end

    function api.Data() return data end
    function api.Path() return path end
    function api.IsDirty() return dirty end

    function api.Save()
        File.CreateDir(dir)
        local ok = File.Write(path, JSON.Stringify(data))
        if ok then dirty = false end
        return ok
    end

    -- Autosave every 30s if something changed, and on unload.
    LoopAsync(30000, function() if dirty then api.Save() end end)

    return api
end

return Store
```

### `scripts/timers.lua`

```lua
-- timers.lua -- a registry so a consumer can cancel everything it owns at once.
local Timers = {}

local owned = {}     -- ownerName -> { [handle] = true }

local function bucket(ownerName)
    owned[ownerName] = owned[ownerName] or {}
    return owned[ownerName]
end

function Timers.Every(ownerName, seconds, callback)
    -- The engine API takes milliseconds; accepting seconds here is a deliberate
    -- choice so call sites read naturally: Every("mymod", 0.25, fn) is 250 ms.
    local ms = math.max(16, math.floor(seconds * 1000))
    local handle = LoopAsync(ms, callback)
    bucket(ownerName)[handle] = true
    return handle
end

function Timers.After(ownerName, seconds, callback)
    local ms = math.max(1, math.floor(seconds * 1000))
    local handle = ExecuteWithDelay(ms, function()
        bucket(ownerName)[handle] = nil
        callback()
    end)
    bucket(ownerName)[handle] = true
    return handle
end

function Timers.Cancel(ownerName, handle)
    bucket(ownerName)[handle] = nil
    return CancelTimer(handle)
end

function Timers.CancelAll(ownerName)
    local b = bucket(ownerName)
    local n = 0
    for handle in pairs(b) do
        CancelTimer(handle)
        b[handle] = nil
        n = n + 1
    end
    return n
end

return Timers
```

### `scripts/widgets.lua`

```lua
-- widgets.lua -- the reusable ImGui pieces, all of them thin wrappers.
local Widgets = {}

function Widgets.Header(text)
    Overlay.Separator()
    Overlay.TextColored(text, 0.60, 0.80, 1.00, 1.00)   -- text first, then rgba
end

function Widgets.KeyValue(key, value)
    Overlay.Text(("%s: %s"):format(key, tostring(value)))
end

-- All three widgets return (value, changed) -- value FIRST. These wrappers hide that
-- behind a familiar (newValue) return plus an onChanged callback.
function Widgets.Slider(label, value, min, max, onChanged)
    local newValue, changed = Overlay.SliderFloat(label, value, min, max)
    if changed and onChanged then onChanged(newValue) end
    return newValue
end

function Widgets.IntSlider(label, value, min, max, onChanged)
    local newValue, changed = Overlay.SliderInt(label, value, min, max)
    if changed and onChanged then onChanged(newValue) end
    return newValue
end

function Widgets.Toggle(label, value, onChanged)
    local newValue, changed = Overlay.Checkbox(label, value)
    if changed and onChanged then onChanged(newValue) end
    return newValue
end

function Widgets.ButtonRow(items)
    -- items = { { label = "A", action = fn }, ... }
    for index, item in ipairs(items) do
        if index > 1 then Overlay.SameLine() end
        if Overlay.Button(item.label) and item.action then item.action() end
    end
end

function Widgets.StatRow(rows)
    for _, row in ipairs(rows) do
        Overlay.SetStat(row[1], tostring(row[2]))
    end
end

return Widgets
```

### `scripts/main.lua`

```lua
-- main.lua -- assemble the API, publish it, and run the shared draw/tick loop.
local Logger  = require("logger")
local Store   = require("store")
local Timers  = require("timers")
local Widgets = require("widgets")

local ModKit = {
    version = "1.0.0",
    Log     = Logger.Make("ModKit"),
    Widgets = Widgets,
}

-- Per-consumer factories: every mod gets its own logger, store and timer bucket,
-- so one consumer cannot interfere with another.
function ModKit.NewLogger(ownerName) return Logger.Make(ownerName) end
function ModKit.NewStore(ownerName, defaults) return Store.New(ownerName, defaults) end

function ModKit.NewTimers(ownerName)
    return {
        Every     = function(seconds, cb) return Timers.Every(ownerName, seconds, cb) end,
        After     = function(seconds, cb) return Timers.After(ownerName, seconds, cb) end,
        Cancel    = function(handle) return Timers.Cancel(ownerName, handle) end,
        CancelAll = function() return Timers.CancelAll(ownerName) end,
    }
end

-- A window registry: many mods contribute panels to one shared toolbox.
local windows = {}      -- name -> draw function

function ModKit.AddWindow(name, draw) windows[name] = draw end
function ModKit.RemoveWindow(name) windows[name] = nil end
function ModKit.Windows()
    local names = {}
    for name in pairs(windows) do names[#names + 1] = name end
    table.sort(names)
    return names
end

-- A tick bus: consumers register per-frame callbacks and can cheaply opt out.
local ticks = {}        -- ownerName -> callback
local ticksEnabled = true

function ModKit.OnTick(ownerName, callback) ticks[ownerName] = callback end
function ModKit.OffTick(ownerName) ticks[ownerName] = nil end

-- A service registry, so one mod can provide data another consumes.
local services = {}

function ModKit.Provide(name, value)
    services[name] = value
    ModKit.Log.info(("service provided: %s"):format(name))
end

function ModKit.Use(name)
    local value = services[name]
    if value == nil then
        ModKit.Log.warn(("service requested but missing: %s"):format(name))
    end
    return value
end

function ModKit.Services()
    local names = {}
    for name in pairs(services) do names[#names + 1] = name end
    table.sort(names)
    return names
end

-- Publish the API where every other mod can reach it.
ModRef:SetSharedVariable("ModKit", ModKit)

-- One draw callback drives every panel and every tick, so there is exactly one
-- ImGui entry point in the whole process for kit-based mods.
Overlay.RegisterDrawCallback(function()
    if not ticksEnabled then return end

    for ownerName, callback in pairs(ticks) do
        local ok, err = pcall(callback)
        if not ok then
            ModKit.Log.error(("tick from %s failed: %s"):format(ownerName, tostring(err)))
            ticks[ownerName] = nil      -- a broken consumer must not kill the bus
        end
    end

    for _, draw in pairs(windows) do
        pcall(draw)
    end
end)

-- A tiny self-test window so the kit proves itself out of the box.
ModKit.AddWindow("ModKit", function()
    Widgets.Header("ModKit " .. ModKit.version)
    Widgets.KeyValue("windows", #ModKit.Windows())
    Widgets.KeyValue("services", #ModKit.Services())
    Widgets.KeyValue("ticks", (function()
        local n = 0
        for _ in pairs(ticks) do n = n + 1 end
        return n
    end)())
    Widgets.ButtonRow({
        { label = ticksEnabled and "pause bus" or "resume bus",
          action = function() ticksEnabled = not ticksEnabled end },
    })
end)

RegisterKeyBind({Key.K, "LeftControl"}, function()
    ticksEnabled = not ticksEnabled
    Notify(ticksEnabled and "ModKit bus running" or "ModKit bus paused", "info")
end)

Mod.OnUnload = function()
    ModKit.Log.info("ModKit unloading")
    ModRef:SetSharedVariable("ModKit", nil)
end

ModKit.Log.info(("ModKit %s published -- consumers can ModRef('ModKit'):GetSharedVariable('ModKit')")
    :format(ModKit.version))
```

### Consuming it: a 40-line mod that gets all of that for free

```lua
-- ANOTHER mod's scripts/main.lua. No duplication of logging, config or timers.
local kit = ModRef("ModKit"):GetSharedVariable("ModKit")
if not kit then
    print("MyCoolMod: ModKit is not installed. Please enable it.")
    return
end

local log    = kit.NewLogger("MyCoolMod")
local store  = kit.NewStore("MyCoolMod", { SprintSpeed = 900, AutoSprint = true })
local timers = kit.NewTimers("MyCoolMod")

log.info("booting with kit " .. kit.version)

-- Recurring work, cancelled as a group on unload.
timers.Every(0.5, function()
    if not store.Get("AutoSprint") then return end
    local player = GetPlayer()
    if not (player and player:IsValid()) then return end
    player.CustomTimeDilation = store.Get("SprintSpeed")
end)

-- A panel in the shared toolbox.
kit.AddWindow("MyCoolMod", function()
    kit.Widgets.Header("MyCoolMod")
    kit.Widgets.Toggle("Auto sprint", store.Get("AutoSprint"), function(v) store.Set("AutoSprint", v) end)
    kit.Widgets.Slider("Sprint speed", store.Get("SprintSpeed"), 300, 3000,
        function(v) store.Set("SprintSpeed", v) end)
end)

-- Per-frame work, on one shared tick.
local frames = 0
kit.OnTick("MyCoolMod", function() frames = frames + 1 end)
kit.Provide("MyCoolMod.frames", function() return frames end)

-- Offer a service other mods can consume.
kit.Provide("MyCoolMod.GetSprintSpeed", function() return store.Get("SprintSpeed") end)

Mod.OnUnload = function()
    timers.CancelAll()
    kit.OffTick("MyCoolMod")
    kit.RemoveWindow("MyCoolMod")
    store.Save()
    log.info("unloaded cleanly")
end
```

### Why it is written this way

* **It publishes itself through a shared variable.** `Mod:SetSharedVariable("ModKit", api)` at
  boot means any other mod can pick it up with one line and no file-system coupling. Consumers
  degrade gracefully when the kit is not installed, the guard in the consumer is three lines.
  We put ModKit near the top of `mods.json`; a consumer that boots before it sees nil and
bails.
* **Per-consumer factories, not global singletons.** `NewLogger`/`NewStore`/`NewTimers` all key
  on an owner name, so two mods never share a buffer, a config file or a timer bucket.
* **One draw callback, one tick bus.** ImGui callbacks are a finite resource and every one you
  register is a frame cost. Centralising them means one loop, one place to add budgeting, and
  one place to pause everything (`ticksEnabled`).
* **`pcall` around consumer callbacks.** A library must never be brought down by a bug in a
  consumer. Failing the tick *and removing that consumer* is the correct blast radius.
* **`Timers.Every` takes seconds even though the engine takes milliseconds.** The wrapper
  exists to make call sites readable; document the unit in the name and in the comment, and
  enforce it with `math.max`.
* **The service registry turns a collection of mods into a platform.** `Provide`/`Use` means a
  mod that knows nothing about another mod can still build on it, that is what makes a modding
  ecosystem rather than a pile of scripts.

### Grow it

1. **A dependency graph.** Consumers declare `Requires = { "ModKit" }`; the kit refuses to
   publish until every requirement is satisfied and logs a readable cycle report.
2. **Rate-limited logging.** De-duplicate identical lines within a window so a hook firing
   10 000 times a second produces one line and a count.
3. **A budgeted tick bus.** Give each consumer a millisecond budget per frame, measured with
   `os.clock()`, and drop consumers that blow it, self-defending performance.
4. **Version negotiation.** `ModKit.Require("1.2")` returns nil when the installed kit is older,
   so consumers fail loudly at boot instead of subtly at runtime.
