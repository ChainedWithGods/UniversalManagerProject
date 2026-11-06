# Part II: The runtime API, tour by tour

## 8. Finding things: the finder toolkit

### 8.1 Player and world

```lua
GetPlayer()                 -- local player pawn
GetPlayerPawn()             -- alias
GetPlayerCharacter()        -- alias
GetPlayerController()       -- local controller
GetFirstPlayerController()  -- alias
GetPlayerCameraManager()    -- camera manager
GetCameraManager()          -- alias
GetLocalPlayer()            -- ULocalPlayer
GetGameInstance()           -- UGameInstance
GetGameMode()               -- AGameModeBase
GetGameState()              -- AGameStateBase
GetWorld()                  -- the live world
GetEngine()                 -- GEngine
```

`GetPlayer` is smarter than a name lookup: it walks the local controller's pawn properties and
skips class default objects (`Default__`), then falls back to scanning world actors.

```lua
local player = GetPlayer()
if player and player:IsValid() then
    print("pawn:", player:GetName())
end
```

### 8.2 By class

```lua
FindFirstOf("PlayerController")         -- one instance (class-aware)
FindAllOf("Pawn")                       -- table of instances
GetAllActorsOfClass("BP_Door_C")        -- world actors of a class
GetAllActorsOfClass("Actor")            -- every actor
GetWorldActors()                        -- every actor in the world
```

Names accept the `_C` suffix or not, and `"Actor"`/`"AActor"` means everything.

```lua
local doors = GetAllActorsOfClass("BP_Door_C")
print(#doors .. " doors")
for i = 1, #doors do
    print(i, doors[i]:GetFullName())
end
```

### 8.3 By name and by path

```lua
FindObject("BP_PlayerCharacter_C")                          -- short name
FindObjects("BP_Door")                                      -- array, exact then prefix
StaticFindObject("/Script/Engine.Default__GameplayStatics")  -- full path
StaticFindObject(ClassObj, OuterObj, "ObjectName", true)     -- bounded search
FindClass("Actor")                                          -- class by name
StaticClass("Actor")                                        -- class by name
StaticStruct("Vector")                                      -- UScriptStruct by name
StaticEnum("ECollisionChannel")                             -- UEnum by name
```

`StaticFindObject` with a path is the reliable way to reach a class default object (CDO), which
is how you call "static" library functions:

```lua
local statics = StaticFindObject("/Script/Engine.Default__GameplayStatics")
print(statics:IsValid())
```

### 8.4 Distances and relationships

```lua
local d = GetDistance(player, someActor)          -- between objects
local d2 = GetDistance(vecA, vecB)                -- between vectors
print(player:GetDistanceTo(otherActor))
print(obj:IsA("Character"), obj:IsChildOf(CharClass))
```

### 8.5 Inspecting the finders themselves

```lua
print(GetObjectCount())                 -- size of the object array
print(GetObjectName(obj))
print(GetObjectFullName(obj))
print(GetObjectClass(obj))
print(GetSuperClass(someStruct))
print(InheritsFrom(obj, "Pawn"))
```

### 8.6 The object array as a whole

```lua
ForEachUObject(function(obj, chunkIndex, indexInChunk)
    -- called for every live object; return true to stop
end)

ForEachUObjectFlat(function(obj)
    -- flat form
end)
```

This is the heavy tool, it walks every object in the process. It is the right choice for
"find anything whose name contains X" when the index has no answer:

```lua
local found = {}
ForEachUObjectFlat(function(obj)
    if obj and obj:IsValid() then
        local n = obj:GetName()
        if n and n:find("Sword") then
            found[#found + 1] = obj
        end
    end
    if #found >= 20 then return true end   -- stop early
end)
```

> **Performance note:** full-array walks are expensive. A full walk every frame is a frame-time
> bug wearing a fancy name. We return `true` from `ForEachUObject` to stop early, and cache the
> result, preferably both.

### 8.7 Camera and viewport

```lua
local cam = GetCameraInfo()          -- {Location=FVector, Rotation=FRotator, FOV=float}
print(cam.Location.X, cam.Rotation.Yaw, cam.FOV)

local sx, sy, ok = ProjectWorldToScreen(FVector(0, 0, 100))
if ok then Overlay.DrawCircle(sx, sy, 6, 0xFF00FFFF, false, 2) end

local worldPos, worldDir, ok2 = DeprojectScreenToWorld(960, 540)

local mx, my, ok3 = GetMousePosition()

local vw, vh = GetViewportSize()     -- active viewport resolution
```

`GetCameraInfo` tries `PlayerController::GetPlayerViewPoint` first, then falls back to
`PlayerCameraManager` direct getters. `ProjectWorldToScreen` and `DeprojectScreenToWorld`
wrap the matching `PlayerController` methods. `GetMousePosition` returns viewport-relative
screen coordinates. `GetViewportSize` returns the swapchain-backed viewport size (use this
instead of hard-coding 1920×1080).

### 8.8 Components

```lua
local mesh = GetComponentByClass(player, "SkeletalMeshComponent")
local weapon = GetComponentByClass(player, "BP_Weapon_C")

local comps = GetAllComponents(player)
for i = 1, #comps do
    print(i, comps[i]:GetName())
end
```

`GetComponentByClass` accepts short class names with or without `_C`. `GetAllComponents`
returns every component attached to the actor (root + children).

### 8.9 Player state and bounds

```lua
local ps = GetPlayerState()            -- APlayerState (name, score, ping, etc.)
local origin, extent = GetActorBounds(player)
print("Origin:", origin.X, origin.Y, origin.Z)
print("Extent:", extent.X, extent.Y, extent.Z)
```

### 8.10 Line traces (raycasting)

```lua
local cam = GetCameraInfo()
local forward = cam.Rotation:GetForwardVector()
local endPos = cam.Location + forward * 5000

local hit, ok = LineTraceSingle(cam.Location, endPos)
if ok and hit then
    print("Hit actor:", hit.Actor and hit.Actor:GetName())
    print("Impact point:", hit.ImpactPoint.X, hit.ImpactPoint.Y, hit.ImpactPoint.Z)
    print("Impact normal:", hit.Normal.X, hit.Normal.Y, hit.Normal.Z)
end
```

`LineTraceSingle` calls `UGameplayStatics::LineTraceSingle`. It returns an `FHitResult`
struct view (or `nil`) and a boolean. The hit result exposes `Location`, `ImpactPoint`,
`Normal`, `ImpactNormal`, `Distance`, `Actor`, `Component`, `BoneName`, `Item`, etc.
through reflection.

### 8.11 Actor location and teleport

```lua
local pos = GetActorLocation(player)
print("Player at", pos.X, pos.Y, pos.Z)

-- Teleport 500 units forward
local cam = GetCameraInfo()
local forward = cam.Rotation:GetForwardVector()
local newPos = cam.Location + forward * 500
local ok = SetActorLocation(player, newPos)
-- Defaults: bSweep=false, bTeleport=true (instant, no interpolation)
```

`GetActorLocation` wraps `K2_GetActorLocation`. `SetActorLocation` wraps `K2_SetActorLocation`
with sensible defaults (`bSweep=false`, `bTeleport=true`). You can override them:
`SetActorLocation(actor, pos, true, false)`.

## 9. Inspecting anything: reflection on demand

You will spend most of your modding life here. Avalonium makes the runtime describe itself.

### 9.1 The quick dump

```lua
local obj = GetPlayer()
obj:DumpProperties()     -- reflected properties
obj:DumpFunctions()      -- reflected functions
```

These write to the log. Read the output, it *is* the game's own reflection data.

### 9.2 Structured property list

```lua
for _, p in ipairs(GetClassProperties(obj) or {}) do
    print(string.format("%-30s %-24s off=0x%X size=%d array=%s struct=%s enum=%s map=%s bool=%s",
        p.name, p.type, p.offset or -1, p.size or 0,
        tostring(p.is_array), tostring(p.is_struct), tostring(p.is_enum),
        tostring(p.is_map), tostring(p.is_bool)))
end
```

Each entry carries: `name`, `type`, `offset`, `size`, `is_array`, `is_struct`, `is_enum`,
`is_map`, `is_bool`, `class_name`.

### 9.3 Targeted lookups

```lua
local prop = obj:GetProperty("CustomTimeDilation")   -- a property wrapper (§27)
local found = obj:FindProperty("CustomTimeDilation") -- lookup helper
print(obj:HasFunction("K2_SetActorLocation"))
local fn = obj:GetFunction("K2_SetActorLocation")
local props = obj:GetProperties()              -- array form
local funcs = obj:GetFunctions()
```

### 9.4 The `Reflection` table

```lua
local refl = obj:Reflection()          -- { [PropName] = { Type = ..., Name = ... }, GetProperty = f }
for name, meta in pairs(refl) do
    if type(meta) == "table" then print(name, meta.Type) end
end
local h = refl:GetProperty("CustomTimeDilation")   -- an FProperty wrapper for that field
```

### 9.5 Iterating superclass-inclusive

```lua
obj:ForEachProperty(function(prop)
    print(prop:GetName(), prop:GetOffset(), prop:GetSize())
end)

obj:ForEachFunction(function(fn)
    print(fn:GetName())
end)
```

Unlike `GetProperties()` (which lists what is reachable), these walk the class chain, including
the superclasses.

### 9.6 Class-level questions

```lua
local cls = obj:GetClass()
print(cls:GetSuperClass():GetName())
for _, child in ipairs(cls:GetChildClasses() or {}) do print(child:GetName()) end
```

## 10. Reading and writing properties

### 10.1 The two syntaxes

```lua
-- member syntax (reflected lookup)
obj.CustomTimeDilation = 1.4
print(obj.CustomTimeDilation)

-- explicit syntax (also works with a name you computed)
obj:SetPropertyByName("CustomTimeDilation", 1.4)
print(obj:GetPropertyByName("CustomTimeDilation"))
```

Both go through the same property resolver, so behaviour is identical. The explicit form is
handy when the name is dynamic.

### 10.2 Primitives

```lua
obj.SomeInt     = 42
obj.SomeFloat   = 3.5
obj.SomeBool    = true
obj.SomeString  = "hello"
obj.SomeName    = "MyName"        -- FName property
obj.SomeObject  = otherObj        -- object reference
```

### 10.3 Structs

Struct properties read back as value objects with their fields exposed:

```lua
local loc = player:GetLocation()
print(loc.X, loc.Y, loc.Z)

-- write the whole struct
player:SetLocation(FVector(loc.X, loc.Y, loc.Z + 500))

-- read a reflected struct property
local ts = player.Transform  -- FTransform value
print(ts.Translation.X, ts.Translation.Y, ts.Translation.Z)
```

### 10.4 Containers

Containers come back as **live views** (§14):

```lua
local arr = obj.SomeArray
print(arr:Num())

-- read an element
local first = arr[1]

-- iterate
arr:ForEach(function(index, element)
    print(index, element)
end)
```

### 10.5 Enums

Enum properties come back as their numeric or name value depending on the property type;
`FByteProperty` with an enum, `FEnumProperty`, and plain byte values are all handled.
`FEnumProperty` values are read through their declared underlying width and returned as
numbers. Use the enum object for names:

```lua
local enum = StaticEnum("ECollisionChannel")
if enum and enum:IsValid() then
    print(enum:GetNameByValue(1))
    enum:ForEachName(function(name, value) print(name, value) end)
end
```

### 10.6 Booleans and bitfields

Bool properties are bitfields in Unreal. Avalonium handles the bit for you through the property
descriptor:

```lua
obj.bHidden = true
print(obj.bHidden)
```

For raw bytes with no reflection, we use the memory API's mask-aware write (§16.4). Write the
whole byte and one bool becomes eight; the other seven belong to the game.

### 10.7 Nested access

Path-style reads are supported through the property wrapper's value objects, and you can chain
handles:

```lua
local mesh = player:GetComponent("Mesh")
if mesh and mesh:IsValid() then
    print(mesh:GetClass():GetName())
end

-- struct member through a reflected struct property
print(player.Transform.Translation.Z)
```

### 10.8 A practical read/write example

```lua
-- Boost the player's effective speed via CustomTimeDilation, safely.
-- This works in ALL UE games (including root-motion titles, where
-- CharacterMovement.MaxWalkSpeed has no effect).
local player = GetPlayer()
if player and player:IsValid() then
    local spd = player.CustomTimeDilation
    if type(spd) == "number" then
        player.CustomTimeDilation = spd * 1.4
        print(("time dilation %.2f -> %.2f"):format(spd, player.CustomTimeDilation))
    else
        print("no numeric CustomTimeDilation on this player")
    end
end
```

That `else` branch is the professional habit: when a game-specific name might not exist, dump
instead of guessing.

## 11. Calling engine functions

### 11.1 Method syntax

```lua
player:K2_SetActorLocation(FVector(0, 0, 1000), false, false, nil)
player:SetActorHiddenInGame(false)
print(player:K2_GetActorLocation().Z)
```

The runtime resolves the short name, tries `K2_` variants, marshals your arguments into the
function's own parameter layout, dispatches through the engine's script VM, and marshals out
parameters and the return value back. You get the same result the Blueprint would.

### 11.2 Alias members

A number of high-traffic calls have friendly aliases:

```text
player:GetLocation()          = K2_GetActorLocation
player:SetLocation(vec)       also (x, y, z)
player:GetRotation()          = K2_GetActorRotation
player:SetRotation(rot)
player:GetScale()             = K2_GetActorScale3D
player:SetScale(vec)
player:GetTransform()         = K2_GetActorTransform
player:SetTransform(t)
player:Teleport(loc, rot)     = K2_TeleportTo
player:GetVelocity()
player:GetForwardVector()  player:GetRightVector()  player:GetUpVector()
player:DestroyActor()         = K2_DestroyActor
player:SetActorHiddenInGame(true)
player:SetActorEnableCollision(false)
player:GetAttachedActors()
player:GetActorBounds()
player:AddActorWorldOffset(vec)   player:AddActorLocalOffset(vec)
player:AddActorWorldRotation(rot) player:AddActorLocalRotation(rot)
```

`SetLocation` accepts an `FVector` **or** three numbers, and returns a boolean:

```lua
player:SetLocation(FVector(0, 0, 500))
player:SetLocation(0, 0, 500)
```

### 11.3 Explicit call form

```lua
local ok = player:Call("K2_SetActorLocation", FVector(0,0,500), false, false, nil)
print(player:CallFunction("GetActorLocation").Z)
```

`CallFunction(obj, name, ...)` is the free-function form:

```lua
CallFunction(player, "SetActorHiddenInGame", false)
```

### 11.4 Property export / import

Read or write a single reflected property by name without going through the full descriptor:

```lua
-- Read a property
local health = player:ExportProperty("Health")
print(health)

-- Write a property
local ok = player:ImportProperty("Health", 100)
print("write succeeded:", ok)

-- Free-function forms also exist:
local speed = ExportProperty(player, "MovementSpeed")
ImportProperty(player, "MovementSpeed", 600)
```

### 11.5 Out-parameters

Many engine functions use out-params. Avalonium returns the function's own return value
first, then its out-params:

```lua
-- K2_SetActorLocation(NewLocation, bSweep, bTeleport, OutSweepHitResult) -> bool
local ok, hit = player:Call("K2_SetActorLocation", FVector(0, 0, 500), true, false, nil)
print(ok, hit)
```

The return value is always first. Out-params follow in parameter order, and a table
supplied for an out-param is filled in place as well: primitive, enum, name, string,
object and map values are stored under the parameter name, a structure parameter copies
the members its value exposes, and array or set parameters fill the table's own fields
when the value is table-shaped.

```lua
local pc = GetPlayerController()
local sx, sy = {}, {}
pc:GetViewportSize(sx, sy)
print(sx.SizeX, sy.SizeY)  -- the caller's tables hold the out values
```

### 11.5 Static libraries

Call CDO functions directly:

```lua
local math = GetKismetMathLibrary()
print(math:Abs(-3))
print(math:Clamp(12, 0, 10))
print(math:Lerp(0, 100, 0.25))

local sys = GetKismetSystemLibrary()
local str = GetKismetStringLibrary()
local arr = GetKismetArrayLibrary()
local txt = GetKismetTextLibrary()
local guid = GetKismetGuidLibrary()
local rend = GetKismetRenderingLibrary()
local gs = GetGameplayStatics()
```

### 11.6 Finding a function when the call fails

If `obj:SomeFunction` returns an invalid object, the name is not reflected. Check:

```lua
print(obj:HasFunction("SomeFunction"))
obj:DumpFunctions()
```

Then look for the right spelling (for example `K2_Foo` versus `Foo`, or a `ReceiveFoo` event).

### 11.7 Warming up: a "call anything" helper

```lua
local function TryCall(obj, name, ...)
    if not (obj and obj:IsValid()) then return nil, "invalid object" end
    if not obj:HasFunction(name) then return nil, "no such function: " .. name end
    local ok, err = pcall(obj.Call, obj, name, ...)
    if not ok then return nil, err end
    return ok
end

TryCall(GetPlayer(), "K2_SetActorHiddenInGame", false)
```

## 12. Creating and destroying objects

### 12.1 Spawning actors into the world

```lua
local world = GetWorld()
local cls = StaticClass("BP_Explosion_C")   -- or a UClass you found

-- member form (preferred; the world is bound for you)
local actor = world:SpawnActor(cls, FVector(0, 0, 200), FRotator(0, 0, 0))

-- global form
local actor2 = SpawnActor(cls, FVector(100, 0, 200), FRotator(0, 90, 0))

-- by class name
local actor3 = world:SpawnActor("BP_Pickup_C", FVector(0,0,100), FRotator(0,0,0))
```

`SpawnActor` tries the engine's reflected spawn path first and falls back to the native
constructor, so it works even on builds where one route is unavailable. It returns `nil` on
failure.

> The Blueprint mod loader spawns its mod actors exactly this way,
> `World:SpawnActor(ModClass, {}, {})`, which is a good indicator the path is well-trodden.

### 12.2 Constructing non-actor objects

```lua
local obj = StaticConstructObject(SomeClass, OuterObject)
local obj2 = NewObject(SomeClass, OuterObject)          -- alias
local obj3 = ConstructObject(SomeClass, OuterObject)    -- alias
```

If you omit the outer, the live world is used. A persistent object (one that survives garbage
collection) is created with the garbage-collection-keep flags, which is what the BP loader does:

```lua
local GarbageCollectionKeepFlags = 0x0E000000
local gameInstance = GetGameInstance()
local persistent = StaticConstructObject(SomeClass, gameInstance, 0, 0,
                                         GarbageCollectionKeepFlags,
                                         false, false, nil, nil, nil)
```

### 12.3 Duplicating

```lua
local copy = DuplicateObject(templateObject, SomeOuter)
```

### 12.4 Attaching and detaching

```lua
child:AttachToActor(parent)
child:AttachToComponent(parentMesh, "SocketName")
child:DetachFromActor()
```

`AttachToActor` uses the engine's own `K2_AttachToActor` with world-space rules and welded
simulation bodies by default.

### 12.5 Destroying

```lua
actor:DestroyActor()          -- = K2_DestroyActor
```

We re-check `:IsValid()` because a destroyed actor's handle can look valid for a moment; the
allocator reuses addresses, and touching a stale handle later is how phantom objects and random
crashes get born.

### 12.6 Components

```lua
local mesh = actor:GetComponent("Mesh")
local all  = actor:GetComponents()
local caps = actor:GetComponentsByClass("CapsuleComponent")
for i = 1, #caps do print(caps[i]:GetClass():GetName()) end
```

### 12.7 A worked spawn example

```lua
-- Drop a pickup 200 units above the player, every time F7 is pressed.
RegisterKeyBind(Key.F7, function()
    ExecuteInGameThread(function()
        local player = GetPlayer()
        local world  = GetWorld()
        if not (player and player:IsValid() and world and world:IsValid()) then return end

        local loc = player:GetLocation()
        local cls = StaticClass("BP_Pickup_C")
        if not (cls and cls:IsValid()) then
            print("BP_Pickup_C not found in this game")
            return
        end

        local spawned = world:SpawnActor(cls, FVector(loc.X, loc.Y, loc.Z + 200), FRotator(0,0,0))
        if spawned and spawned:IsValid() then
            print("spawned:", spawned:GetFullName())
            Notify("Pickup dropped")
        else
            print("spawn failed")
        end
    end)
end)
```

## 13. Value types, structs and math

### 13.1 `FVector` (alias `Vector`)

```lua
local v = FVector(100, 200, 300)

print(v.X, v.Y, v.Z)
print(v.Length, v.Size)               -- magnitude
print(v.LengthSquared, v.SizeSquared)
print(v.Length2D, v.Size2D)
print(v:Dot(other), v:Cross(other))
print(v:Normalize())                  -- normalized copy
print(v:ToRotator())                  -- FRotator pointing along v
print(v:Distance(other))
print(v:ToTable())                    -- { X=..., Y=..., Z=... }
print(v:Clone())
print(v:IsValid())

local sum  = v + other
local diff = v - other
local scl  = v * 2
local div  = v / 2
local neg  = -v
print(v == other)
```

X/Y/Z can also be read through `Pitch`/`Yaw`/`Roll` aliases, which is convenient when scrubbing
rotation-like vectors.

### 13.2 `FRotator` (alias `Rotator`)

```lua
local r = FRotator(0, 90, 0)   -- Pitch, Yaw, Roll

print(r.Pitch, r.Yaw, r.Roll)
print(r:Vector())               -- unit vector along the rotator
print(r:GetForwardVector(), r:GetRightVector(), r:GetUpVector())
print(r:Normalize(), r:Clamp())
print(r:ToTable(), r:Clone())

local r2 = r + FRotator(0, 15, 0)
```

### 13.3 `FTransform` (alias `Transform`)

```lua
local t = FTransform(FVector(0,0,100), FRotator(0,90,0), FVector(1,1,1))

print(t.Location.X, t.Translation.X)   -- aliases
print(t.Rotation.Yaw)
print(t.Scale3D.X, t.Scale.X)

print(t:TransformPosition(FVector(10,0,0)))
print(t:TransformLocation(FVector(10,0,0)))
print(t:ToTable(), t:Clone())
```

### 13.4 `FLinearColor` / `FColor`

```lua
local c = FLinearColor(1, 0, 0, 1)
print(c.R, c.G, c.B, c.A)
print(c:ToHex(), c.Hex)
print(c:ToTable())

local brighter = c * 1.5
print((c + FLinearColor(0,0.2,0,0)).G)
```

### 13.5 `FQuat`, `FVector2D`, `FVector4`

```lua
local q = FQuat(0, 0, 0, 1)
print(q.X, q.Y, q.Z, q.W, q:Rotator().Yaw)

local v2 = FVector2D(3, 4)
print(v2.Length)          -- 5

local v4 = FVector4(1, 2, 3, 4)
print(v4.W)
```

### 13.6 Soft object references

```lua
local soft = FSoftObjectPath("/Game/Items/BP_Sword.BP_Sword_C")
print(soft:GetAssetPath(), soft:GetAssetPathName())
print(soft:IsNull(), soft:IsValid())
print(soft:ToString())

local hard = soft:Load()             -- resolve now
local maybe = soft:Get()             -- resolve if loaded
print(soft:GetSubPathString())
```

### 13.7 Delegates

```lua
local del = obj.SomeMulticastDelegate
print(del:IsBound(), del:Num())
del:Add(function(...) print("fired", ...) end)
del:Broadcast(1, 2)
del:RemoveAll()
```

### 13.8 The `Math` table

```lua
print(Math.Lerp(0, 10, 0.5))                       -- 5
print(Math.Clamp(17, 0, 10))                       -- 10
print(Math.VInterpTo(curVec, targetVec, 0.016, 5)) -- smooth vector
print(Math.RInterpTo(curRot, targetRot, 0.016, 5)) -- smooth rotator
print(Math.FindLookAtRotation(startVec, targetVec)) -- aim rotator
```

`VInterpTo`/`RInterpTo` are the backbone of any "smoothly move the camera/actor" mod: call them
each frame with `GetDeltaSeconds()`.

## 14. Containers: TArray, TMap, TSet, delegates

Containers are **views onto the engine's own storage**, not copies. Reads see live data; writes
(where permitted) go straight in.

### 14.1 `TArray`

```lua
local arr = obj.SomeArray

print(arr:Num(), arr:Count(), arr:GetArrayNum(), #arr)
print(arr:IsValid())
print(arr:GetArrayMax(), arr:GetArrayAddress(), arr:GetArrayDataAddress())
print(arr:type())                     -- "TArray"
print(tostring(arr))                  -- "TArray<InnerType>[num/max] (0x...)"

-- element access (1-based, like Lua)
local first = arr[1]
for i = 1, #arr do print(i, arr[i]) end

-- iteration with a live element view
arr:ForEach(function(index, element)
    -- element is a view: read with :get(), write with :set(table)
    local v = element:get()
    print(index, v)
    -- return false to stop early
end)

arr:Empty()     -- clears logical count (keeps allocation)
```

> The element handed to `ForEach` is a **view**. `element:get()` yields the value;
> `element:set(valueOrTable)` writes it back. That is how a mod fills an array the engine owns.

Writing elements that came from a property write also works through the property system when
the array is itself a property (§10.4).

### 14.2 `TMap`

```lua
local map = obj.SomeMap
print(map:Num(), map:Count(), #map, map:IsValid())

print(map:Find(key))
print(map:Contains(key), map:ContainsKey(key))

map:ForEach(function(k, v)
    print(k, v)
end)

-- TMap mutation is intentionally refused: engine hashing cannot be bypassed safely.
-- Add/Remove exist but do not mutate the engine's map.
map:Empty()
```

Because writing a `TMap` would require reproducing the engine's hash and reallocation, the
runtime refuses it rather than corrupting a live table. If you must change a game map, change
the data source (a DataTable row, a config object) and let the game rebuild its own map. We don't poke the raw memory; that doesn't make the map mutable, it makes the hash table lie.
Then lookups fail in ways that look random.

### 14.3 `TSet`

```lua
local set = obj.SomeSet
print(set:Contains(value))
set:ForEach(function(v) print(v) end)
print(set:Num(), set:Count(), #set)
```

Same refusal rule as `TMap`.

### 14.4 Delegates as containers

See §13.7.

### 14.5 Shared variables: cross-mod state

The only container that crosses mod boundaries is the runtime's shared store:

```lua
Mod:SetSharedVariable("Toolkit.enabled", true)
local enabled = Mod:GetSharedVariable("Toolkit.enabled")

-- another mod's namespace
local other = ModRef("OtherMod"):GetSharedVariable("SomeKey")
```

Anything Lua can hold works, including object handles. That is how mods cooperate without
colliding.

## 15. Names, text and strings

### 15.1 `FName`

```lua
local n = FName("MyName")                 -- add if missing (default flag)
local f = FName("MyName", FNAME_Find)     -- pure lookup
local i = FName(1234)                     -- from a comparison index

print(n:ToString(), n:get())
print(n:GetComparisonIndex(), n:GetNumber())
print(n:IsValid(), n:type())              -- "FName"
print(n:Equals("MyName"))
print("prefix_" .. n)                     -- __concat
print(n)
print(NAME_None)
```

Constant table:

```text
EFindName.FNAME_Find                          0
EFindName.FNAME_Add                           1
EFindName.FNAME_Replace_Not_Safe_For_Threading  2
```

The distinction matters: `FNAME_Find` never creates a name, `FNAME_Add` does. Blueprint mods
depend on the add path, because asset paths are not in the name table until something asks for
them.

### 15.2 `FText`

```lua
local t = FText("Hello")
print(t:ToString(), t:get(), t:type())   -- "Hello"  ...  "FText"
```

Text properties read from engine memory push a reference wrapper that resolves the live
payload, so `ToString()` gives you the current string.

### 15.3 String members

Native Lua strings gain the engine's string API:

```lua
local s = "Hello World"

print(s:Len(), s:Length(), #s)
print(s:IsEmpty())
print(s:Find("World"))            -- 1-based index or nil
print(s:StartsWith("Hello"), s:EndsWith("World"))
print(s:ToUpper(), s:ToLower())
print(s:Append("!"), s:Concat("?"))
print(s:Clone(), s:GetName(), s:GetFullName(), s:type())
print(s:GetFName():ToString())
print((""):IsEmpty(), s:IsValid())

-- both call shapes work
print(string.Len("abc"), ("abc"):Len())
```

Mutation members (`Append`, `Concat`, `Clear`, `Empty`) have **value semantics**: Lua strings
are values, so they return the new string instead of modifying in place.

### 15.4 `FPackageName`

```lua
print(FPackageName.IsShortPackageName("/Game/Foo"))  -- false
print(FPackageName.IsShortPackageName("Foo"))        -- true
print(FPackageName.IsValidLongPackageName("/Game/Foo/Bar"))
```

### 15.5 `FString`, `FUtf8String`, `FAnsiString`

```lua
print(FString(123))         -- "123"
print(FUtf8String("utf8"))
print(FAnsiString("ansi"))
```

These are constructors for engine string values used as arguments.

## 16. The Memory API: going below reflection

Sometimes reflection is not enough: a value is not a property, a struct's layout is not what you
need, a flag lives in a bit, a pointer chain must be walked. Then you use memory.

### 16.1 Getting an address

```lua
local addr = obj:GetAddress()             -- light userdata
print(obj:GetAddressString())             -- "0x7FF712345678"
local prop = obj:GetProperty("CustomTimeDilation")
local propAddr = prop:PropertyAddress()   -- the FProperty itself
```

Addresses entered in the memory API accept light userdata **or** numbers, so
`addr` and `tonumber(tostring(addr):gsub("0x",""), 16)` are both usable, the first is cleaner.

### 16.2 Typed reads

Readers take a base and an optional byte offset:

```text
Memory.ReadInt8(base [, offset])       Memory.ReadByte
Memory.ReadInt16(base [, offset])      Memory.ReadUInt16
Memory.ReadInt32(base [, offset])      Memory.ReadUInt32
Memory.ReadInt64(base [, offset])      Memory.ReadUInt64
Memory.ReadFloat(base [, offset])
Memory.ReadDouble(base [, offset])
Memory.ReadPointer(base [, offset])
Memory.ReadBool(base [, offset [, mask]])          mask defaults to 1
Memory.ReadString(base [, offset [, maxLen]])      ANSI, NUL-terminated (max 4096)
Memory.ReadFString(base [, offset])                UE FString (UTF-16/ANSI aware)
Memory.ReadFName(base [, offset])                  resolved through the name table
Memory.IsValidPtr(ptr)
```

```lua
-- Offsets are game-specific and change every patch. Use only when reflection
-- does not expose the value you need.
local spd = Memory.ReadFloat(move:GetAddress(), 0x2A0)
print("raw speed:", spd)
local name = Memory.ReadFName(player:GetAddress(), 0x18)
print("raw name:", name:ToString())
```

We use reflection first. A hardcoded offset is a time bomb with a patch date stamped on it.

### 16.3 Typed writes

Writers take a base, a value, and an optional offset:

```text
Memory.WriteInt32(base, 100 [, offset])
Memory.WriteFloat(base, 1.5 [, offset])
Memory.WritePointer(base, somePtr [, offset])
Memory.WriteBool(base, true [, offset [, mask]])
plus Int8 / Int16 / Int64 / UInt8 / UInt16 / UInt32 / UInt64 / Double / Byte variants
```

```lua
Memory.WriteFloat(player:GetAddress(), 9999.0, 0x2A0)
```

### 16.4 Bit flags

Masks let a single byte hold many booleans:

```lua
local flags = 0
-- read bit 3
local isSet = Memory.ReadBool(base, 0x40, 1 << 3)
-- set bit 3 without touching the others
Memory.WriteBool(base, true, 0x40, 1 << 3)
-- clear it
Memory.WriteBool(base, false, 0x40, 1 << 3)
```

### 16.5 Snake_case spellings

Both `Memory` and `ue` carry the lower-case names, so a habit from either works everywhere:

```lua
Memory.read_i32(base, 0x10)
Memory.write_float(base, 2.5, 0x14)
ue.read_ptr(base, 0x8)
ue.read_fstring(base, 0x30)
Memory.read_bool(base, 0x40, 0x4)
```

### 16.6 Safety

Always validate before dereferencing:

```lua
if Memory.IsValidPtr(ptr) then
    local v = Memory.ReadInt32(ptr)
end
```

Reads that fail return `nil` rather than crashing or throwing. Treat `nil` as "not mapped".

### 16.7 Walking a pointer chain

```lua
local function Chain(base, ...)
    local cur = base
    for _, off in ipairs({...}) do
        if not Memory.IsValidPtr(cur) then return nil end
        cur = Memory.ReadPointer(cur, off)
    end
    return cur
end

local obj = Chain(gameState:GetAddress(), 0x50, 0x18, 0x0)
```

### 16.8 Reading a struct you know

```lua
local v3 = {}
v3.X = Memory.ReadFloat(base, 0x0)
v3.Y = Memory.ReadFloat(base, 0x4)
v3.Z = Memory.ReadFloat(base, 0x8)
print(("loc %.1f %.1f %.1f"):format(v3.X, v3.Y, v3.Z))
```

### 16.9 `DerefToInt32`

The compatibility helper for a single 32-bit read:

```lua
print(DerefToInt32(address))     -- also: dereftoint32(address)
```

## 17. CVars: the game's own switches

CVars are the console variables the engine and game expose. Avalonium can read them, set them,
and enumerate them.

> **Console availability:** Reading CVars and enumerating them requires the Unreal Engine
> console manager to be discovered at startup. If the console is disabled in `Avalonium.ini`
> (`EnableConsole = false`), `GetCVar` returns `nil` and `FindCVars` returns an empty table `{}`.
> `SetCVar` and `ExecuteConsoleCommand` route via the engine's Kismet library fallback if
> available. When reading CVars, guard with `if #FindCVars() > 0 then` or verify `GetCVar(...) ~= nil`.

```lua
print(GetCVar("r.ScreenPercentage"))     -- integer or nil
SetCVar("r.ScreenPercentage", 70)        -- executes "r.ScreenPercentage 70"
local names = FindCVars()                -- up to 10 000 names (or empty if console disabled)
```

Turning off a specific effect for your own visual mod:

```lua
local old = GetCVar("r.MotionBlurQuality")
SetCVar("r.MotionBlurQuality", 0)
-- put it back on unload
Mod.OnUnload = function()
    if old then SetCVar("r.MotionBlurQuality", old) end
end
```

`SetCVar` goes through the engine's console, so anything the game accepts in its console works.
For arbitrary command strings use `ExecuteConsoleCommand`:

```lua
ExecuteConsoleCommand("stat fps")
ExecuteConsoleCommand("ShowDebug AbilitySystem")
```

To handle custom console commands entered by players in the in-game console:

```lua
RegisterConsoleCommandHandler("my_mod_reset", function(cmd, args)
    print("Mod command dispatched:", cmd, args)
end)
```

## 18. Files and JSON

### 18.1 `File`

```lua
File.Read(path)                  -- whole file as a string, or nil
File.Write(path, data)           -- overwrite -> boolean
File.Append(path, data)          -- append   -> boolean
File.Exists(path)                -- boolean
File.List(path)                  -- directory listing
File.CreateDir(path)             -- also File.MakeDir(path)
File.Delete(path)
```

Relative paths resolve against the game's working directory, so build absolute paths from the
mods directory:

```lua
local function ModPath(...)
    return GetModsDirectory() .. "/MyMod/" .. table.concat({...}, "/")
end

File.CreateDir(GetModsDirectory() .. "/MyMod")
File.Write(ModPath("notes.txt"), "hello")
print(File.Read(ModPath("notes.txt")))
```

### 18.2 `JSON`

```lua
local cfg = JSON.Parse(File.Read(path)) or {}    -- JSON.decode also works
File.Write(path, JSON.Stringify(cfg))            -- JSON.encode also works
```

```lua
local settings = JSON.Parse([[
{
  "Mod": { "Enabled": true, "Height": 300, "Keys": ["F6", "F7"] }
}
]]) or {}

print(settings.Mod.Height)          -- 300
print(settings.Mod.Keys[2])         -- "F7"

settings.Mod.Height = 500
File.Write(GetModDirectory() .. "/MyMod/config.json", JSON.Stringify(settings))
```

### 18.3 A loader pattern you can reuse

```lua
local DEFAULTS = { Height = 300, Enabled = true, Keys = { "F6" } }

local function LoadConfig()
    -- GetModDirectory() is the *mods root*; append your own folder name.
    local path = GetModDirectory() .. "/MyMod/config.json"
    local merged = {}
    for k, v in pairs(DEFAULTS) do merged[k] = v end

    local raw = File.Read(path)
    if raw then
        local parsed = JSON.Parse(raw)
        if type(parsed) == "table" then
            for k, v in pairs(parsed) do merged[k] = v end
        else
            print("config.json did not parse; using defaults")
        end
    else
        File.Write(path, JSON.Stringify(DEFAULTS))   -- write defaults on first run
    end
    return merged
end

local config = LoadConfig()
```
