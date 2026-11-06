# Part IV: Advanced territory

## 27. The FProperty wrapper: metadata as data

`GetProperty`, `ForEachProperty` and the compatibility pipelines hand you **property wrappers**,
userdata around the engine's own `FProperty`. They are how you introspect a field rather than
just read it.

### 27.1 Getting one

```lua
local spd = obj:GetProperty("CustomTimeDilation")
print(spd:GetName(), spd:GetOffset(), spd:GetSize(), spd:type())
print(tostring(spd))     -- "FProperty CustomTimeDilation <FloatProperty> +0x2A0 (0x...)"
```

### 27.2 The member set

```
GetFName  GetName  GetFullName  GetClass  GetStruct  GetInner  GetEnum
GetPropertyClass  GetInterfaceClass
GetOffset_Internal | GetOffset   GetArrayDim   GetSize | GetElementSize
GetFieldSize  GetByteOffset  GetByteMask  GetFieldMask  ContainerPtrToValuePtr
GetValue | get   SetValue | set   ImportText   IsA   IsValid   GetOwner   type
```

### 27.3 The same data as plain keys

The wrapper also answers metadata as plain reads, so it can stand in where a table used to:

```
hp.Type           hp.TypeName        -- the property class name
hp.Name
hp.Offset         hp.Offset_Internal
hp.Size           hp.ElementSize
hp.ArrayDim
hp.PropertyFlags
hp.IsOutParam     hp.IsArrayLike     hp.HasValue
hp.PropertyAddress                   -- the FProperty's own address
hp.ByteOffset     hp.ByteMask  hp.FieldMask  hp.FieldSize
```

### 27.4 Reading and writing through the wrapper

```lua
local spd = obj:GetProperty("CustomTimeDilation")
print(spd:GetValue(obj))          -- value at the object's own address
spd:SetValue(obj, 1.4)
print(spd:Get(obj))               -- shorthand: same as GetValue(obj)
spd:set(obj, 1.7)
```

### 27.5 Instance-bound wrappers (struct members)

When a wrapper is bound to an instance (a script-struct value or a struct-typed field), its
members are the struct's fields, and reads/writes go to the real payload:

```lua
local inst = SomeScriptStructInstance          -- e.g. from a struct-typed field
inst:GetProperty("Inner").Value = 5            -- writes into the struct
```

### 27.6 Why wrappers matter

They let a mod reason about *shape*: is this an array? an out-param? what is its element size?
Walk a class's fields and generate UI, serialize data, or drive generic tooling. I read the shape at runtime instead of hardcoding a member list; that's how a mod survives a
patch, and the hardcoded list is the first thing the patch breaks.

```lua
-- Build a small table describing every field on a class.
local function DescribeClass(cls)
    local out = {}
    obj:ForEachProperty(function(p)     -- or walk cls:GetChildProps()
        out[#out + 1] = {
            name = p:GetName(),
            type = p:type(),
            off  = p:GetOffset(),
            size = p:GetSize(),
            arr  = p:GetArrayDim(),
            out  = p.IsOutParam,
        }
    end)
    return out
end
```

## 28. DataTables: reading and rewriting game data

DataTables are where a lot of game data lives, item stats, weapon tables, spawn tables. They
are fully readable and mutable.

```lua
local dt = FindFirstOf("DataTable")     -- or FindObject of a specific table
```

### 28.1 Reading

```lua
print(dt:type())                    -- property/table type info
print(dt:GetRowStruct():GetName())  -- the row structure
print(#dt)                          -- row count (__len)

local names = dt:GetRowNames()
for i = 1, #names do print(i, names[i]) end

-- one row, as a table of reflected fields
local row = dt:FindRow("LongSword")
if row then
    for k, v in pairs(row) do print(k, v) end
end

-- or the raw row map / all rows
local all = dt:GetAllRows()
local map = dt:GetRowMap()

-- iterate rows
for _, rowName in ipairs(names) do
    local r = dt:FindRow(rowName)
    print(rowName, r and r.Damage)
end
```

### 28.2 Writing

```lua
-- insert or update a row
local ok = dt:AddRow("TestSword", {
    DisplayName = "Test Sword",
    Damage      = 999,
    Weight      = 1.5,
})
print("add row:", ok)

-- remove
local removed = dt:RemoveRow("TestSword")

-- wipe
local emptied = dt:EmptyTable()
```

### 28.3 A classic balance mod

```lua
local function BuffAllWeapons(multiplier)
    local dt = FindObject("WeaponTable")     -- adjust to your game
    if not (dt and dt:IsValid()) then return end

    local names = dt:GetRowNames()
    local changed = 0
    for _, name in ipairs(names) do
        local row = dt:FindRow(name)
        if row and type(row.Damage) == "number" then
            row.Damage = math.floor(row.Damage * multiplier)
            if dt:AddRow(name, row) then changed = changed + 1 end
        end
    end
    print(("buffed %d rows").."", changed)
end

BuffAllWeapons(1.5)
```

> Row writes go through the engine's own table code and, when needed, are deferred to the
> game thread automatically, so you can call them from wherever.

We don't call `AddRow` for every row every frame. On an existing row it rewrites through
Epic's table code, and the stutter is the engine doing exactly what it was told.

## 29. Enums: names, values, and mutation

```lua
local enum = StaticEnum("ECollisionChannel")

print(enum:GetNameByValue(1))
enum:ForEachName(function(name, value)
    print(name, value)
end)

local n2 = enum:GetEnumNameByIndex(1)
```

Mutation (best-effort; degrades to no-ops when the arrays cannot be verified):

```lua
enum:EditNameAt(1, "RenamedEntry")
enum:EditValueAt(1, 7)
enum:InsertIntoNames("NewEntry", 42)
enum:RemoveFromNamesAt(1)
```

We read the enum back. A no-op looks exactly like success until the value is verified, the
call doesn't tell the difference.

## 30. Structs: views and injection

### 30.1 Reading a struct property

```lua
local loc = actor:GetLocation()            -- math struct -> FVector value
print(loc.X, loc.Y, loc.Z)

local t = actor:GetTransform()             -- FTransform value
print(t.Translation.Z)

local hit = {}                             -- out-param structs come back as values/tables
```

### 30.2 Writing a struct property

```lua
-- whole-struct write
actor:SetLocation(FVector(0, 0, 300))

-- write a struct member of a reflected property
local ts = actor.Transform
-- mutate and write back through the property if the game exposes it
```

### 30.3 Script structs

The runtime keeps one representation of a structure value. A struct-typed field yields a values
object whose members you read directly; an instance-bound `FProperty` wrapper (§27.5) writes
straight into its payload.

## 31. Custom properties: teaching an object new members

Sometimes you want `obj.MyValue` to work even though the game never declared `MyValue`. Inject
it.

```lua
RegisterCustomProperty({
    Name           = "ModTag",
    Type           = "IntProperty",      -- or a type record
    BelongsToClass = SomeClass,           -- a UClass, or a class name string
    OffsetInternal = 0,                   -- usually 0 for a synthetic field
    ArrayDim       = 1,
    PropertyFlags  = 0,
})

-- now, on instances of that class:
obj.ModTag = 5
print(obj.ModTag)
```

Injected properties resolve **after** reflected lookup fails, so they never shadow a real
member. This is a clean way to attach mod state to engine objects without external tables.
We give mod state a distinctive name. A real property added by a game patch silently takes
priority over the injected one, and then the mod state is gone.

## 32. The compatibility layer and feature detection

Avalonium ships a compatibility surface (v3.5) so scripts written for other Unreal Lua
runtimes run unchanged. Even if you never target another runtime, several of its tables are
genuinely useful.

### 32.1 Version detection

```lua
print(UnrealVersion.Major, UnrealVersion.Minor)     -- engine
print(UnrealVersion:GetMajor(), UnrealVersion:GetMinor())

if UnrealVersion:IsBelow(5, 1) then
    print("old FAssetData layout")
elseif UnrealVersion:IsAtLeast(5, 4) then
    print("new FHitResult layout")
end

print(RuntimeVersion.Major, RuntimeVersion.Minor, RuntimeVersion.Hotfix)
print(RuntimeVersion.GetVersion())
print(RuntimeVersion.CompatMajor, RuntimeVersion.CompatMinor)   -- 3, 5
```

Feature-detect the surface, not the game:

```lua
assert(RuntimeVersion.CompatMinor >= 5, "Avalonium compat surface too old")
```

`Avalonium` and `UE4SS` are aliases of `RuntimeVersion`, for scripts that type-check their host.

### 32.2 Flags

```lua
print(EObjectFlags.RF_Transient)
print(EObjectFlags.RF_AllFlags)                 -- 0x1FFFFFFF pre-UE5, 0xFFFFFFFF on UE5
print(EInternalObjectFlags.RootSet)
print(EInternalObjectFlags.GarbageCollectionKeepFlags)

if obj:HasAnyFlags(EObjectFlags.RF_Transient) then
    print("this object is transient")
end
print(obj:HasAllFlags(EObjectFlags.RF_Public))
print(obj:HasAnyInternalFlags(EInternalObjectFlags.Native))
```

The flag tables are **version-aware**: `RF_GeneratePublicHash` appears on 5.6+,
`RF_MigratingAsset`/`RF_HasDynamicImports` on 5.8+, `RF_MirroredGarbage` on UE5, and so on.

### 32.3 Property types

```lua
for _, rec in ipairs(PropertyTypes) do
    print(rec.Name, rec.Size, rec.FFieldClassPointer)
end
print(PropertyTypes.FloatProperty.Size)     -- 4
print(PropertyTypes.MapProperty.Size)       -- 32
```

### 32.4 Names, weak pointers, invalid objects

```lua
print(NAME_None:IsValid())

local weak = FWeakObjectPtr(GetPlayer())
if weak:IsValid() then print(weak:Get():GetName()) end

local safe = CreateInvalidObject()   -- resolves methods, reads return neutral values
print(safe:IsValid())                -- false
```

In Avalonium, indexing a missing property or function on a `UObject` resolves to this invalid-object
sentinel instead of `nil`. This prevents chained property lookups (like `obj.Nested.Deep`) from
throwing nil-dereference errors, and unresolved method invocations safely return `nil`. Because the
sentinel is a userdata (and therefore truthy in Lua), check `:IsValid()` to verify whether an indexed
function or object genuinely exists:

```lua
if obj.SomeMethod and obj.SomeMethod:IsValid() then
    obj:SomeMethod()
end
```

### 32.5 Directory and mod management

```lua
local tree = IterateGameDirectories()
if tree then
    local project = tree.Game or tree
    print("project:", project.__absolute_path)
    for _, file in ipairs((project.Content and project.Content.__files) or {}) do
        print("file:", file.__name)
    end
end

CreateLogicModsDirectory()
LoadExport("Shared")            -- loads shared/exports/Shared.lua (or fallbacks)
RestartMod("MyMod")
UninstallMod("MyMod")
```

## 33. The Blueprint pipeline and asset inspection

These two globals let a mod see what the runtime itself discovered, without reimplementing it.

### 33.1 `GetBlueprintPipeline`

```lua
local p = GetBlueprintPipeline()
print(p.assetPipelineReady, p.assetPipelineDetail)
print(p.assetDataStruct, p.assetDataSize)
print(p.blueprintGeneratedClass, p.blueprintActorClassCount)
print(p.canCreateNames, p.nameWidth, p.nameNumberOffset)

for _, field in ipairs(p.assetDataFields or {}) do
    print(field.Name, field.Type, field.Offset, field.Size)
end

for name, ok in pairs(p.assetRegistryFunctions or {}) do
    print(name, ok)
end

local chain = p.memberChain or {}
print(chain.objectOuter, chain.structSuper, chain.childProperties)
```

This is the answer to "can this build resolve asset paths / create names?", which is exactly
what a Blueprint mod loader depends on. We use it as the real answer. It reports what discovery actually bound, not what the engine
version theoretically supports.

### 33.2 `GetObjectShape`

```lua
local s = GetObjectShape("/Script/Engine.Default__GameplayStatics")
print(s.resolved, s.name, s.className, s.fullName)
print(s.structFamily, s.structFamilyByIdentity, s.isClassObject, s.structClassName)
print(s.ownerIsReceiver)
for i, name in ipairs(s.classSuperChain or {}) do print(i, name) end
```

Use it whenever a member read returns nothing: it tells you the class, the super chain, and
whether the runtime enumerated the receiver's own members or its meta-class's.

### 33.3 Loading assets

```lua
local cls = LoadAsset("Blueprint'/Game/Items/BP_Sword.BP_Sword_C'")
if cls and cls:IsValid() then
    print("loaded class:", cls:GetFullName())
end

local existing = StaticLoadObject("/Game/Items/BP_Sword.BP_Sword_C")
```

## 34. Diagnostics: making the runtime explain itself

| Tool | Use it when |
| :--- | :--- |
| `obj:DumpProperties()` / `DumpFunctions()` | you do not know the game's member names |
| `GetClassProperties(obj)` | you want the list as data |
| `DumpAllObjects()` | you want to grep the whole process |
| `GetObjectShape(path)` | a lookup behaves unexpectedly |
| `GetBlueprintPipeline()` | asset/name resolution is failing |
| `GenerateLuaTypes()` | you want editor autocomplete |
| `obj:Reflection()` | you want superclass-inclusive metadata |
| `Memory.IsValidPtr` | you are about to dereference something |
| `ue.get_globals()` | you want to inspect discovered engine root addresses |

### 34.1 Generating type stubs for autocomplete

```lua
GenerateLuaTypes()
```

This writes `shared/types/<Game>/AvaloniumTypes.lua` (EmmyLua annotations) for the whole game.
Point your editor's Lua language server at that folder and every class member autocompletes.
This alone changes how modding feels: instead of guessing property names, you browse them.
We regenerate the stubs after a game patch. Debugging autocomplete against an old class list
is a special kind of waste of time.

### 34.2 A self-checking mod

```lua
-- A mod that tells you whether the API it needs actually exists before you debug it.
local REQUIRED = {
    "GetPlayer", "GetWorld", "FindFirstOf", "RegisterKeyBind", "Notify",
    "GetKismetMathLibrary", "Memory", "Overlay",
}

local missing = {}
for _, name in ipairs(REQUIRED) do
    if _G[name] == nil then missing[#missing + 1] = name end
end
if #missing > 0 then
    print("API missing: " .. table.concat(missing, ", "))
else
    print("all required API present")
end
```

(The shipped `AvaloniumAPIReporter` mod in this repository is the industrial version of this
idea: it checks the entire documented surface and writes a report file.)

### 34.3 Inspecting discovered engine roots

If you need to diagnose whether Avalonium located key engine structures (such as `GUObjectArray`, `GNames`, `GWorld`, or `GEngine`), `ue.get_globals()` returns a table of opaque pointer wrappers:

```lua
local roots = ue.get_globals()
for name, ptr in pairs(roots) do
    print(string.format("Discovered root %s = %s", name, tostring(ptr)))
end
```
