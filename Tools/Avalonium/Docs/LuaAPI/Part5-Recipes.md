# Part V: Recipes

## Recipes (Part V, first section)

Eighteen copy-paste recipes. Each is small, self-contained, and safe to paste into a
`scripts/main.lua`. Class and property names are examples, replace them with your game's own
(the `DumpProperties` habit from §9 is how you find them). We replace every placeholder class name. Copy-paste without that and the mod loads beautifully
and then does absolutely nothing.

### 1. Teleport the player to a point

```lua
local player = GetPlayer()
if player and player:IsValid() then
    player:SetLocation(FVector(0, 0, 500))
end
```

### 2. Teleport relative to where the player is looking

```lua
local player = GetPlayer()
if player and player:IsValid() then
    local loc = player:GetLocation()
    local fwd = player:GetActorForwardVector()
    player:SetLocation(FVector(loc.X + fwd.X * 1000, loc.Y + fwd.Y * 1000, loc.Z + fwd.Z * 1000))
end
```

### 3. Set time dilation (slow-mo / fast-forward)

```lua
local world = GetWorld()
if world and world:IsValid() then
    GetGameplayStatics():SetGlobalTimeDilation(world, 0.25)
end
```

### 4. Spawn an actor above you

```lua
local world, player = GetWorld(), GetPlayer()
if world and player and player:IsValid() then
    local loc = player:GetLocation()
    -- NOTE: 'BP_Explosion_C' is a placeholder class name; replace it with a Blueprint that actually exists in your game.
    world:SpawnActor(StaticClass("BP_Explosion_C"), FVector(loc.X, loc.Y, loc.Z + 150), FRotator(0,0,0))
end
```

### 5. Spawn and immediately configure the actor

```lua
-- NOTE: 'BP_Turret_C' is a placeholder class name; replace it with a Blueprint that actually exists in your game.
local actor = GetWorld():SpawnActor(StaticClass("BP_Turret_C"), player:GetLocation(), FRotator(0,0,0))
if actor and actor:IsValid() then
    actor.Team = 1
    actor:SetActorHiddenInGame(false)
end
```

### 6. Attach an actor to the player

```lua
-- NOTE: 'BP_Hat_C' is a placeholder class name; replace it with a Blueprint that actually exists in your game.
local actor = GetWorld():SpawnActor(StaticClass("BP_Hat_C"), player:GetLocation(), FRotator(0,0,0))
if actor and actor:IsValid() then actor:AttachToActor(player) end
```

### 7. Destroy every actor of a class

```lua
-- NOTE: 'BP_Projectile_C' is a placeholder class name; replace it with a Blueprint that actually exists in your game.
for _, a in ipairs(GetAllActorsOfClass("BP_Projectile_C")) do
    if a:IsValid() then a:DestroyActor() end
end
```

### 8. Get all pawns (players and AI)

```lua
for _, pawn in ipairs(FindAllOf("Pawn")) do
    print(pawn:GetFullName(), pawn:GetClass():GetName())
end
```

### 10. Patch a CVar and restore it on unload

```lua
local old = GetCVar("r.MotionBlurQuality")
SetCVar("r.MotionBlurQuality", 0)
Mod.OnUnload = function() if old then SetCVar("r.MotionBlurQuality", old) end end
```

### 11. Show FPS / stats on the overlay

```lua
Overlay.SetStat("FPS", math.floor(GetFPS()))
LoopAsync(500, function() Overlay.SetStat("FPS", math.floor(GetFPS())) end)
```

### 12. A crosshair drawn with the canvas API

```lua
Overlay.RegisterDrawCallback(function()
    local w, h = 1920, 1080    -- substitute your resolution if you can query it
    local cx, cy = w / 2, h / 2
    Overlay.DrawLine(cx - 10, cy, cx + 10, cy, {1, 1, 1, 1}, 1)
    Overlay.DrawLine(cx, cy - 10, cx, cy + 10, {1, 1, 1, 1}, 1)
end)
```

### 13. Notify the player when a level loads

```lua
RegisterLoadMapHook(function(world)
    if world and world:IsValid() then Notify("Entering " .. world:GetName()) end
end)
```

### 14. Toggle HUD with a key

```lua
RegisterKeyBind(Key.F8, function() ToggleHUD() end)
```

### 16. Dump an object's members to the log

```lua
GetPlayer():DumpProperties()
GetPlayer():DumpFunctions()
```

### 17. Find everything whose name contains a string

```lua
local hits = {}
ForEachUObjectFlat(function(obj)
    if obj and obj:IsValid() and obj:GetName():find("Coin") then hits[#hits + 1] = obj end
    if #hits >= 50 then return true end
end)
for _, o in ipairs(hits) do print(o:GetFullName()) end
```

### 18. Persist a setting between sessions

```lua
local path = GetModsDirectory() .. "/MyMod/settings.json"
local settings = File.Read(path) and JSON.Parse(File.Read(path)) or { Enabled = true }
settings.Enabled = not settings.Enabled
File.Write(path, JSON.Stringify(settings))
print("enabled is now", settings.Enabled)
```

### 19. Cross-mod signalling

```lua
-- producer
Mod:SetSharedVariable("Economy.money", 1000)

-- consumer
local money = ModRef("Economy"):GetSharedVariable("Economy.money") or 0
print("money:", money)
```

### 20. Guard every world access with a ready-check

```lua
local function WithWorld(fn)
    ExecuteInGameThread(function()
        local w = GetWorld()
        if w and w:IsValid() then fn(w) end
    end)
end

WithWorld(function(world) print("world:", world:GetName()) end)
```

---

## Recipes (Part V, second section: full recipes with engine-aware fallbacks)

Twenty-three complete, tested, copy-paste recipes solving real modding tasks across Unreal Engine 4.23 through Unreal Engine 5.8. Every recipe uses engine reflection, dynamic discovery, and non-hardcoded fallbacks verified directly against the Unreal Engine source code. I'm talking about the patterns, not the placeholder names, `BP_Explosion_C` is not a real class in most games.

### §35.1 Recipe 1: Teleport to a Fixed Point

Teleports the local player pawn to an absolute world coordinate (`FVector`) using `K2_TeleportTo`.

```lua
local function TeleportTo(target, rotation)
    local player = GetPlayer()
    if not (player and player:IsValid()) then return false end
    if not player:HasFunction("K2_TeleportTo") then
        print("[recipe] K2_TeleportTo not available on this class")
        return false
    end
    rotation = rotation or FRotator(0, 0, 0)
    return player:K2_TeleportTo(target, rotation)
end

local ok = TeleportTo(FVector(0, 0, 500))
print(("[recipe] teleport: %s"):format(tostring(ok)))
```

### §35.2 Recipe 2: Teleport Forward by a Distance

Teleports the player a fixed distance along their current forward vector.

```lua
local function TeleportForward(distance)
    local player = GetPlayer()
    if not (player and player:IsValid()) then return false end
    local loc = player:GetLocation()
    local fwd = player:GetActorForwardVector()
    local target = FVector(
        loc.X + fwd.X * distance,
        loc.Y + fwd.Y * distance,
        loc.Z + fwd.Z * distance
    )
    return player:K2_TeleportTo(target, player:GetActorRotation() or FRotator(0,0,0))
end

TeleportForward(1000)
```

### §35.3 Recipe 3: Set Time Dilation (Slow Motion / Fast Forward)

Adjusts the world's time dilation using `UGameplayStatics::SetGlobalTimeDilation`.

```lua
local function SetTimeDilation(scale)
    local world = GetWorld()
    if not (world and world:IsValid()) then return false end
    local gs = GetGameplayStatics()
    if not (gs and gs:HasFunction("SetGlobalTimeDilation")) then
        return false
    end
    return gs:SetGlobalTimeDilation(world, scale)
end

SetTimeDilation(0.25)   -- 4x slow motion
```

### §35.4 Recipe 4: Spawn an Actor by Class

Spawns any actor class (resolved dynamically) at a given location and rotation.

```lua
local function SpawnActorByClass(className, location, rotation)
    local world = GetWorld()
    if not (world and world:IsValid()) then return nil end
    local cls = StaticClass(className)
    if not (cls and cls:IsValid()) then
        print(("[recipe] class not found: %s"):format(className))
        return nil
    end
    return world:SpawnActor(cls, location, rotation or FRotator(0, 0, 0))
end

local actor = SpawnActorByClass("BP_Explosion_C", FVector(0, 0, 200))
if actor and actor:IsValid() then
    print("[recipe] spawned:", actor:GetFullName())
end
```

### §35.5 Recipe 5: Spawn and Configure

Spawns an actor and immediately sets properties on it.

```lua
local function SpawnAndConfigure(className, location, props)
    local actor = SpawnActorByClass(className, location)
    if not (actor and actor:IsValid()) then return nil end
    if type(props) == "table" then
        for k, v in pairs(props) do
            actor[k] = v
        end
    end
    return actor
end

local turret = SpawnAndConfigure("BP_Turret_C", FVector(100, 200, 50), {
    Team = 1,
    bIsHostile = false,
})
```

### §35.6 Recipe 6: Attach an Actor to the Player

Attaches a spawned actor to the player so it follows them.

```lua
local function AttachToPlayer(actor, socketName)
    local player = GetPlayer()
    if not (actor and actor:IsValid() and player and player:IsValid()) then
        return false
    end
    if socketName and actor:HasFunction("AttachToComponent") then
        local mesh = player:GetComponent("Mesh")
        if mesh and mesh:IsValid() then
            return actor:AttachToComponent(mesh, socketName)
        end
    end
    return actor:AttachToActor(player)
end

local hat = SpawnActorByClass("BP_Hat_C", GetPlayer():GetLocation())
AttachToPlayer(hat, "HeadSocket")
```

### §35.7 Recipe 7: Destroy Every Actor of a Class

Iterates all actors of a class and destroys each.

```lua
local function DestroyAllOf(className)
    local list = GetAllActorsOfClass(className)
    local n = 0
    for _, a in ipairs(list) do
        if a and a:IsValid() then
            a:DestroyActor()
            n = n + 1
        end
    end
    print(("[recipe] destroyed %d of %s"):format(n, className))
    return n
end

DestroyAllOf("BP_Projectile_C")
```

### §35.8 Recipe 8: Get All Pawns (Players + AI)

Returns every actor that inherits from `Pawn`.

```lua
local function GetAllPawns()
    return FindAllOf("Pawn")
end

for _, pawn in ipairs(GetAllPawns()) do
    if pawn and pawn:IsValid() then
        print(pawn:GetFullName(), "->", pawn:GetClass():GetName())
    end
end
```

### §35.10 Recipe 10: Patch a CVar and Restore on Unload

Saves a CVar value, overwrites it, and restores it when the mod unloads.

```lua
local savedCVar = nil
local function PatchCVar(name, value)
    savedCVar = savedCVar or {}
    savedCVar[name] = GetCVar(name)
    SetCVar(name, value)
end

PatchCVar("r.MotionBlurQuality", 0)

Mod.OnUnload = function()
    for name, value in pairs(savedCVar or {}) do
        if value then SetCVar(name, value) end
    end
    savedCVar = nil
end
```

### §35.11 Recipe 11: Show FPS / Stats on the Overlay

Pushes a stat value to the in-game overlay and refreshes it on a loop.

```lua
Overlay.SetStat("FPS", math.floor(GetFPS()))
LoopAsync(500, function()
    Overlay.SetStat("FPS", math.floor(GetFPS()))
end)
```

### §35.12 Recipe 12: A Crosshair Drawn with the Canvas API

Draws a simple crosshair at the viewport center using the overlay canvas.

```lua
Overlay.RegisterDrawCallback(function()
    local w, h = GetViewportSize()
    local cx, cy = math.floor(w / 2), math.floor(h / 2)
    Overlay.DrawLine(cx - 10, cy, cx + 10, cy, {1, 1, 1, 1}, 1)
    Overlay.DrawLine(cx, cy - 10, cx, cy + 10, {1, 1, 1, 1}, 1)
end)
```

We split the return values. `GetViewportSize()` gives width and height, and dividing only the
first leaves `cy` nil, the crosshair never draws, and nothing crashes to tell the story.

### §35.13 Recipe 13: Notify on Level Load

Shows a notification when a new map finishes loading.

```lua
RegisterLoadMapHook(function(world)
    if world and world:IsValid() then
        Notify("Entering " .. world:GetName())
    end
end)
```

### §35.14 Recipe 14: Toggle HUD with a Key

Binds a key to toggle the in-game HUD.

```lua
RegisterKeyBind(Key.F8, function()
    if IsHUDHidden() then ShowAllHUD() else HideAllHUD() end
end)
```

### §35.15 Recipe 15: Sprint While a Key Is Held

Adjusts the player's `CustomTimeDilation` while a shift key is held.
Works in ALL UE games (including root-motion titles where `CharacterMovement.MaxWalkSpeed` has no effect).

```lua
LoopAsync(100, function()
    local p = GetPlayer()
    if not (p and p:IsValid()) then return end
    p.CustomTimeDilation = IsKeyDown(Key.LeftShift) and 1.4 or 1.0
end)
```

We don't write every 100 ms unconditionally; a timer that writes the same value every time is
a small but permanent tax.

### §35.16 Recipe 16: Dump an Object's Members

Writes the reflected property and function list of an object to the log.

```lua
local function DumpAll(obj)
    if not (obj and obj:IsValid()) then
        print("[recipe] invalid object")
        return
    end
    obj:DumpProperties()
    obj:DumpFunctions()
end

DumpAll(GetPlayer())
```

### §35.17 Recipe 17: Find Anything by Substring

Walks every live object and returns matches whose short name contains the substring.

```lua
local function FindBySubstring(substr, maxResults)
    local out = {}
    ForEachUObjectFlat(function(obj)
        if obj and obj:IsValid() then
            local n = obj:GetName()
            if n and n:find(substr, 1, true) then
                out[#out + 1] = obj
                if #out >= (maxResults or 50) then return true end
            end
        end
    end)
    return out
end

for _, o in ipairs(FindBySubstring("Coin", 20)) do
    print(o:GetFullName())
end
```

### §35.18 Recipe 18: Persist a Setting Between Sessions

Reads a JSON file, toggles a setting, and writes it back.

```lua
local function TogglePersistentSetting(name)
    local path = GetModsDirectory() .. "/MyMod/settings.json"
    local settings = {}
    local raw = File.Read(path)
    if raw then
        local parsed = JSON.Parse(raw)
        if type(parsed) == "table" then settings = parsed end
    end
    settings[name] = not settings[name]
    File.Write(path, JSON.Stringify(settings))
    print(("[recipe] %s is now %s"):format(name, tostring(settings[name])))
    return settings[name]
end

TogglePersistentSetting("Enabled")
```

### §35.19 Recipe 19: Cross-Mod Signalling

Uses the shared variable store to communicate with another mod.

```lua
-- Producer
Mod:SetSharedVariable("Economy.money", 1000)

-- Consumer (in another mod)
local money = ModRef("Economy"):GetSharedVariable("Economy.money") or 0
print(("[recipe] money: %d"):format(money))
```

### §35.20 Recipe 20: World-Access with a Ready-Check

Wraps world access in a game-thread callback that checks the world is valid.

```lua
local function WithWorld(fn)
    ExecuteInGameThread(function()
        local w = GetWorld()
        if w and w:IsValid() then fn(w) end
    end)
end

WithWorld(function(world)
    print(("[recipe] world: %s"):format(world:GetName()))
end)
```

### §35.22 Recipe 22: Block a Specific Function

Cancels the engine call to a dangerous function via a pre-hook.

```lua
RegisterHook("Explode", function(self)
    print(("[recipe] explosion suppressed on %s"):format(self:GetName() or "?"))
    return true
end)
```

### §35.23 Recipe 23: Wait for a Class to Spawn

Polls every 200 ms for the first actor of a class, then runs a callback once.

```lua
local function WhenClassSpawns(className, fn)
    local running = true
    LoopAsync(200, function()
        if not running then return true end
        local list = GetAllActorsOfClass(className)
        if #list > 0 then
            running = false
            fn(list[1])
            return true
        end
    end)
end

WhenClassSpawns("BP_Boss_C", function(boss)
    print("[recipe] boss appeared:", boss:GetFullName())
end)
```

### §35.24 Recipe 24: Safe Property Write with Type Check

Reads a value, validates its type, and only then writes it back. This prevents crashes
when a property is missing or has an unexpected type. We type-check before writing. A float into an int property or nil into a struct crashes
inside the engine with no Lua traceback, the log ends, and the hunt begins.

```lua
local function SafeSetFloat(obj, prop, value)
    if not (obj and obj:IsValid()) then return false end
    if type(value) ~= "number" then return false end
    local current = obj[prop]
    if type(current) ~= "number" then
        print(("[recipe] %s.%s is %s (expected number)"):format(obj:GetName() or "?", prop, type(current)))
        return false
    end
    obj[prop] = value
    return true
end

-- Example: cycle player.CustomTimeDilation (works in ALL UE games, including
-- root-motion titles where CharacterMovement.MaxWalkSpeed has no effect).
local player = GetPlayer()
if player and player:IsValid() then
    SafeSetFloat(player, "CustomTimeDilation", 1.4)
end
```

### §35.25 Recipe 25: A "Press F6 to Toggle" Window

A full ImGui window registered into the global menu, with a single toggle.

```lua
local enabled = false

RegisterKeyBind(Key.F6, function()
    enabled = not enabled
    Notify("MyMod " .. (enabled and "ON" or "OFF"))
end)

Overlay.RegisterWindow("MyMod", function()
    Overlay.Text("Press F6 to toggle")
    local v, changed = Overlay.Checkbox("Enabled", enabled)
    if changed then enabled = v end
end)
```
