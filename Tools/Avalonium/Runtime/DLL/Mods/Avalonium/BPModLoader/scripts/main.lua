-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→
--
-- BPModLoader -- Blueprint (".pak") mod loader for the Avalonium Engine.
--
-- What it does, in order:
--   1. finds <Project>/Content/Paks/LogicMods (creating it when the game has
--      never been modded before),
--   2. reads each logic mod's own config.lua when it ships one, and gives a
--      default config to every ".pak" that has none,
--   3. resolves each mod's ModActor class through the asset registry,
--   4. spawns that class into the live world, and
--   5. respawns every mod whenever a new level begins play.
--
--
-- The two places where an engine detail decides the code:
--
--   * The asset-registry lookup argument is an FAssetData, and that structure
--     is not the same on both sides of UE 5.1.  In
--     Runtime/CoreUObject/Public/AssetRegistry/AssetData.h, 4.23 - 5.0 carry
--     "FName ObjectPath" and FAssetData::GetAsset() resolves the object through
--     it; from 5.1 the member is deprecated (editor-only) and the object path is
--     derived from PackageName + AssetName.  So the table handed to
--     AssetRegistryHelpers:GetAsset() is built per version, and the runtime's
--     writer fills only the members the running build actually reflects
--     (SetPropertyValue -> WriteStructFromTable in Runtime/lua/LuaUObject.cpp).
--
--   * AActor::ReceiveBeginPlay is the event every actor raises (GameFramework/
--     Actor.h) and AWorldSettings declares no begin-play event of its own; the
--     world settings actor -- the one actor that begins play exactly once per
--     level -- raises that same event.  The runtime resolves the load-map hook
--     to it and dispatches the callback for the world settings actor alone
--     (Runtime/lua/LuaHookManager.cpp), so a level transition is announced
--     once per level rather than once per actor.
--
--
-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→

local UEHelpers = require("UEHelpers")

-- The environment this script was compiled in.  config.lua files are run with
-- it as their __index fallback so they see the same globals this script does.
local ModEnvironment = _ENV

local LOG_PREFIX = "[BPModLoader] "
local VerboseLogging = false

-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→
-- Logging
-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→

local function Log(Message)
    print(LOG_PREFIX .. tostring(Message))
end

local function Logf(Format, ...)
    Log(string.format(Format, ...))
end

-- Details worth having while debugging and noise while playing.
local function Debug(Message)
    if VerboseLogging then Log(Message) end
end

local function Debugf(Format, ...)
    if VerboseLogging then Logf(Format, ...) end
end

-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→
-- Small filesystem / string helpers
-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→

local function FileExists(Path)
    if type(Path) ~= "string" or Path == "" then return false end
    local Handle = io.open(Path, "rb")
    if Handle == nil then return false end
    Handle:close()
    return true
end

-- Reads a file's lines, dropping blanks and lines that start with the comment
-- marker.  Returns an empty list when the file is missing, which is the same
-- answer as for a file with nothing in it.
local function LinesFrom(Path, CommentPrefix)
    local Lines = {}
    if not FileExists(Path) then return Lines end

    local Prefix = CommentPrefix or ";"
    for Line in io.lines(Path) do
        local Trimmed = Line:gsub("^%s+", ""):gsub("%s+$", "")
        if Trimmed ~= "" and Trimmed:sub(1, #Prefix) ~= Prefix then
            Lines[#Lines + 1] = Trimmed
        end
    end
    return Lines
end

-- "Foo_Bar_P.pak" -> "Foo_Bar_P", ".pak"
local function SplitExtension(FileName)
    local Stem, Extension = FileName:match("^(.+)(%..+)$")
    if Stem == nil then return FileName, "" end
    return Stem, Extension
end

local function JoinPath(...)
    local Parts = { ... }
    local Path = table.concat(Parts, "/")
    return (Path:gsub("//+", "/"))
end

-- Compiles a Lua file with an explicit environment.  loadfile carries the
-- environment for text chunks; the manual read is the fallback for a runtime
-- or a file that the chunk loader refuses.
local function LoadChunk(Path, Environment)
    if type(loadfile) == "function" then
        local Chunk = loadfile(Path, "t", Environment)
        if Chunk ~= nil then return Chunk end
    end

    local Handle = io.open(Path, "rb")
    if Handle == nil then return nil, "the file could not be opened" end
    local Source = Handle:read("*a")
    Handle:close()
    if Source == nil then return nil, "the file could not be read" end
    return load(Source, "@" .. Path, "t", Environment)
end

-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→
-- Hook-argument helpers
--
-- Every callback the runtime raises for this mod is handed UObject userdata.
-- Reading one is always the same three steps, and each step is guarded because
-- an invalid handle answers rather than raising: :get() is identity, and the
-- invalid-object sink answers :IsValid() with false.
-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→

local function ToObject(Argument)
    if Argument == nil or type(Argument) ~= "userdata" then return nil end
    local Object = Argument:get()
    if Object == nil or not Object:IsValid() then return nil end
    return Object
end

local function ToWorld(Argument)
    local Object = ToObject(Argument)
    if Object ~= nil and Object:IsA("World") then return Object end
    return nil
end

-- UEHelpers.GetWorld is documented to answer an invalid object rather than nil
-- while the engine has no world, so "is there a world" is a validity question.
local function IsLiveWorld(Value)
    return Value ~= nil and Value:IsValid() == true
end

-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→
-- Mod registry
--
-- The registry is published as a global named Mods on purpose: a logic mod may
-- ship config.lua, and the convention those files are written to is
--
--     Mods["<mod name>"] = { AssetName = "...", AssetPath = "..." }
--
-- so the chunk the loader runs a config in is given exactly this table.
-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→

local Registry = {}
Mods = Registry

local OrderedMods = {}
local ModOrderList = {}

-- What a logic mod's ModActor is called unless its config.lua says otherwise.
-- "ModActor_C" is the class the Blueprint mod template generates, and the asset
-- path is the package the mod template cooks into.
local DefaultAssetName = "ModActor_C"
local DefaultAssetPathFormat = "/Game/Mods/%s/ModActor"

-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→
-- Directory discovery
-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→

-- A directory table answers with its children under their own names and keeps
-- its own bookkeeping under "__"-prefixed keys (name, absolute path, the child
-- array, and the lazily listed files).  Only the plain-named tables are child
-- directories.
local function ChildDirectories(Directory)
    local Children = {}
    if type(Directory) ~= "table" then return Children end

    for Name, Value in pairs(Directory) do
        if type(Name) == "string" and type(Value) == "table" and Name:sub(1, 2) ~= "__" then
            Children[#Children + 1] = { Name = Name, Directory = Value }
        end
    end
    table.sort(Children, function(A, B) return A.Name < B.Name end)
    return Children
end

-- A child by name.  The tree is keyed by the names on disk, so an exact hit is
-- tried first and a case-insensitive scan second: "LogicMods" is spelled that
-- way by convention, not by contract.
local function ChildNamed(Parent, Wanted)
    if type(Parent) ~= "table" or type(Wanted) ~= "string" then return nil end

    local Exact = Parent[Wanted]
    if type(Exact) == "table" then return Exact end

    local Lowered = Wanted:lower()
    for Name, Value in pairs(Parent) do
        if type(Name) == "string" and type(Value) == "table" and
            Name:sub(1, 2) ~= "__" and Name:lower() == Lowered then
            return Value
        end
    end
    return nil
end

-- The files of a directory, as the runtime lists them (__name plus
-- __absolute_path).  A directory with nothing in it answers with an empty list.
local function FilesIn(Directory)
    if type(Directory) ~= "table" then return {} end
    local Files = Directory.__files
    if type(Files) ~= "table" then return {} end
    return Files
end

-- Content/Paks/LogicMods, created when the game has no logic mods directory
-- yet.  Returns nil (with a log line saying why) rather than a bad handle.
local function FindLogicModsDirectory()
    local function Walk(NeedCreate)
        local GameRoot = IterateGameDirectories()
        if GameRoot == nil then
            Log("IterateGameDirectories could not describe the game directory " ..
                "layout; no Content directory was found above the executable.")
            return nil
        end

        -- Project root (the directory holding Content).  The tree aliases it as
        -- "Game"; a layout that puts Content directly under the root is accepted
        -- as the fallback.
        local Project = GameRoot.Game
        if type(Project) ~= "table" then Project = GameRoot end

        local Paks = ChildNamed(ChildNamed(Project, "Content"), "Paks")
        if Paks == nil then return nil end

        local LogicMods = ChildNamed(Paks, "LogicMods")
        if LogicMods == nil and NeedCreate then
            CreateLogicModsDirectory()
        end
        return LogicMods
    end

    local LogicMods = Walk(true)
    if LogicMods == nil then
        -- create_directories may have run after the tree was built, so the
        -- description is asked for once more before giving up.
        LogicMods = Walk(false)
    end
    if LogicMods == nil then
        Log("Content/Paks/LogicMods could not be found or created; no Blueprint mods can load.")
    end
    return LogicMods
end

-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→
-- Mod configuration
-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→

-- Runs one logic mod's config.lua in an environment whose Mods name is the
-- registry, so the file's assignment lands where the loader reads it.
local function RunModConfig(ModName, ConfigPath)
    local Environment = setmetatable({}, { __index = ModEnvironment })
    Environment.Mods = Registry

    local Chunk, LoadError = LoadChunk(ConfigPath, Environment)
    if Chunk == nil then
        Logf("config.lua for '%s' could not be compiled: %s", ModName, tostring(LoadError))
        return false
    end

    local Ok, RunError = pcall(Chunk)
    if not Ok then
        Logf("config.lua for '%s' raised: %s", ModName, tostring(RunError))
        return false
    end
    return true
end

-- Registers a mod under its own name.  An existing entry is kept, so a
-- config.lua that already described the mod is never overwritten by defaults.
local function RegisterDefaultMod(ModName, PakStem)
    if type(ModName) ~= "string" or ModName == "" then return end
    if type(Registry[ModName]) == "table" then return end

    Registry[ModName] = {
        AssetName = DefaultAssetName,
        AssetPath = string.format(DefaultAssetPathFormat, PakStem or ModName),
        Source = "default",
    }
end

-- Reads a mod directory's files: its config.lua, and the .pak files inside it.
local function ReadModDirectory(ModDirectoryTable)
    local Info = { PakStems = {}, ConfigPath = nil, Files = {} }

    for _, File in ipairs(FilesIn(ModDirectoryTable)) do
        local FileName = File.__name
        local FilePath = File.__absolute_path
        if type(FileName) == "string" and type(FilePath) == "string" then
            Info.Files[#Info.Files + 1] = FileName
            if FileName:lower() == "config.lua" then
                Info.ConfigPath = FilePath
            else
                local Stem, Extension = SplitExtension(FileName)
                if Extension:lower() == ".pak" then
                    Info.PakStems[#Info.PakStems + 1] = Stem
                end
            end
        end
    end
    table.sort(Info.Files)
    return Info
end

-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→
-- Mod order
-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→

-- load_order.txt lives beside the loader (Mods/BPModLoader/).  The mods root
-- is asked for first because the game's working directory is not this file's.
local function FindLoadOrderFile()
    local Candidates = {}
    if type(GetModsDirectory) == "function" then
        local ModsRoot = GetModsDirectory()
        if type(ModsRoot) == "string" and ModsRoot ~= "" then
            Candidates[#Candidates + 1] = JoinPath(ModsRoot, "BPModLoader", "load_order.txt")
        end
    end
    Candidates[#Candidates + 1] = "Mods/BPModLoader/load_order.txt"
    Candidates[#Candidates + 1] = "load_order.txt"

    for _, Candidate in ipairs(Candidates) do
        if FileExists(Candidate) then return Candidate end
    end
    return nil
end

local function LoadModOrder()
    local Path = FindLoadOrderFile()
    if Path == nil then
        Debug("No load_order.txt found; mods load in name order.")
        return
    end

    local Seen = {}
    for _, Line in ipairs(LinesFrom(Path)) do
        if not Seen[Line] then
            Seen[Line] = true
            ModOrderList[#ModOrderList + 1] = Line
        end
    end
    Debugf("%d entry/entries read from %s", #ModOrderList, Path)
end

-- Priority mods first, in the order the file lists them and numbered from 1;
-- everything else after them, by name and unnumbered, so a given install always
-- loads the same way.
local function ApplyModOrder()
    OrderedMods = {}
    local Included = {}

    for _, ModName in ipairs(ModOrderList) do
        local ModInfo = Registry[ModName]
        if type(ModInfo) == "table" and not Included[ModName] then
            Included[ModName] = true
            ModInfo.Name = ModName
            ModInfo.Priority = #OrderedMods + 1
            OrderedMods[#OrderedMods + 1] = ModInfo
        end
    end

    local Remaining = {}
    for ModName, ModInfo in pairs(Registry) do
        if type(ModInfo) == "table" and not Included[ModName] then
            Remaining[#Remaining + 1] = { Name = ModName, Info = ModInfo }
        end
    end
    table.sort(Remaining, function(A, B) return A.Name < B.Name end)

    for _, Entry in ipairs(Remaining) do
        Entry.Info.Name = Entry.Name
        Entry.Info.Priority = nil
        OrderedMods[#OrderedMods + 1] = Entry.Info
    end
end

-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→
-- Discovery
-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→

local function DiscoverLogicMods(LogicMods)
    local ListedMods = 0

    for _, Child in ipairs(ChildDirectories(LogicMods)) do
        local Info = ReadModDirectory(Child.Directory)
        ListedMods = ListedMods + 1

        Logf("Mod: %s", Child.Name)
        for _, FileName in ipairs(Info.Files) do
            Logf("    ModFile: %s", FileName)
        end

        if Info.ConfigPath ~= nil then
            -- A config.lua speaks for whichever mod names it chose; the
            -- directory name is only the fallback when it named none.
            local Before = {}
            for ModName in pairs(Registry) do Before[ModName] = true end
            RunModConfig(Child.Name, Info.ConfigPath)

            local NamedSomething = false
            for ModName in pairs(Registry) do
                if not Before[ModName] then
                    NamedSomething = true
                    break
                end
            end
            if not NamedSomething then
                RegisterDefaultMod(Child.Name, Info.PakStems[1])
            end
        else
            RegisterDefaultMod(Child.Name, Info.PakStems[1])
        end
    end

    -- ".pak" files sitting loose in LogicMods/ are mods too.
    for _, File in ipairs(FilesIn(LogicMods)) do
        local FileName = File.__name
        if type(FileName) == "string" then
            local Stem, Extension = SplitExtension(FileName)
            if Extension:lower() == ".pak" then
                RegisterDefaultMod(Stem, Stem)
            end
        end
    end

    return ListedMods
end

-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→
-- Asset registry
--
-- AssetRegistryHelpers is the class default object of UAssetRegistryHelpers
-- (Runtime/AssetRegistry/Public/AssetRegistry/AssetRegistryHelpers.h), whose
-- GetAsset(const FAssetData&) is what turns a described asset into its class.
-- The registry interface beside it is fetched for the same reason the reference
-- loader fetches it -- it proves the module is up -- but GetAsset is answered by
-- the helpers object, so a missing interface is reported and survived.
-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→

local AssetRegistryHelpers = nil
local AssetRegistry = nil

local function EnsureAssetRegistry()
    if AssetRegistryHelpers ~= nil and AssetRegistryHelpers:IsValid() then
        return AssetRegistryHelpers
    end
    AssetRegistryHelpers = nil

    local Helpers = StaticFindObject("/Script/AssetRegistry.Default__AssetRegistryHelpers")
    if Helpers == nil or not Helpers:IsValid() then
        Log("AssetRegistryHelpers is not valid: " ..
            "/Script/AssetRegistry.Default__AssetRegistryHelpers could not be resolved.")
        return nil
    end
    AssetRegistryHelpers = Helpers

    if AssetRegistry ~= nil and AssetRegistry:IsValid() then return AssetRegistryHelpers end

    local Interface = Helpers:GetAssetRegistry()
    if Interface ~= nil and Interface:IsValid() then
        AssetRegistry = Interface
        return AssetRegistryHelpers
    end

    local Implementation = StaticFindObject("/Script/AssetRegistry.Default__AssetRegistryImpl")
    if Implementation ~= nil and Implementation:IsValid() then
        AssetRegistry = Implementation
    else
        AssetRegistry = nil
        Debug("The asset registry interface could not be resolved; GetAsset is " ..
              "still answered by AssetRegistryHelpers.")
    end
    return AssetRegistryHelpers
end

-- The FAssetData a lookup needs, built for the engine on the other side.
local function BuildAssetData(AssetPath, AssetName)
    if UnrealVersion:IsBelow(5, 1) then
        -- 4.23 - 5.0: ObjectPath is the "Package.Object" pair GetAsset resolves.
        return {
            ObjectPath = UEHelpers.FindOrAddFName(string.format("%s.%s", AssetPath, AssetName)),
        }
    end

    -- 5.1 and later: the object path is derived from these two, and ObjectPath
    -- itself is deprecated/editor-only, so it is not written at all.
    return {
        PackageName = UEHelpers.FindOrAddFName(AssetPath),
        AssetName = UEHelpers.FindOrAddFName(AssetName),
    }
end

local function DescribeAssetData(AssetData)
    local ObjectPath = AssetData.ObjectPath
    if ObjectPath ~= nil then
        return "ObjectPath=" .. tostring(ObjectPath:ToString())
    end
    local PackageName = AssetData.PackageName
    local AssetName = AssetData.AssetName
    return string.format("PackageName=%s AssetName=%s",
        PackageName ~= nil and PackageName:ToString() or "",
        AssetName ~= nil and AssetName:ToString() or "")
end

-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→
-- Spawning
-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→

-- ModActor classes already seen, by the class's own full name
-- ("BlueprintGeneratedClass /Game/Mods/X/ModActor.ModActor_C"), so the BeginPlay
-- hook recognises a mod actor by identity rather than by a name it guesses.
local ModByClassFullName = {}
-- Which mods are already in which world, so a level load that is announced more
-- than once (the map-load and begin-play hooks can both reach the same world)
-- spawns each mod once.
local WorldLoads = {}
local WorldLoadOrder = {}
-- Only the most recent worlds are tracked: an old world is destroyed when its
-- level unloads, so its record can never be needed again.
local MaxTrackedWorlds = 8
-- The "nothing to load" line is worth reading once, not once per level.
local WarnedNoMods = false

local function WorldKey(World)
    local FullName = World:GetFullName()
    if type(FullName) == "string" and FullName ~= "" then return FullName end
    return tostring(World:GetName())
end

local function RememberWorldLoad(Key)
    if WorldLoads[Key] ~= nil then return WorldLoads[Key] end

    WorldLoads[Key] = {}
    WorldLoadOrder[#WorldLoadOrder + 1] = Key
    while #WorldLoadOrder > MaxTrackedWorlds do
        local Oldest = table.remove(WorldLoadOrder, 1)
        WorldLoads[Oldest] = nil
    end
    return WorldLoads[Key]
end

local function RememberModActorClass(ModInfo, ModClass)
    local FullName = ModClass:GetFullName()
    if type(FullName) == "string" and FullName ~= "" then
        ModByClassFullName[FullName] = ModInfo
    end
end

-- A Blueprint mod may declare its own PostBeginPlay/PreBeginPlay event; the
-- engine's AActor declares neither as a UFUNCTION (GameFramework/Actor.h), so
-- this only ever runs a mod's own.  Both handles are checked before being used,
-- because a name that is not reflected answers with an invalid object.
local function RunActorEvent(Actor, EventName, ModName)
    local Event = Actor[EventName]
    if Event ~= nil and Event:IsValid() then
        Logf("Executing '%s' for mod '%s' (%s)", EventName, ModName, Actor:GetFullName())
        Event()
        return true
    end
    Debugf("'%s' is not reflected on %s", EventName, Actor:GetFullName())
    return false
end

local function SpawnModActor(ModInfo, World)
    if ModInfo.AssetPath == nil or ModInfo.AssetName == nil then
        Logf("Could not load mod '%s' because it has no asset path or name.", tostring(ModInfo.Name))
        return false
    end

    local Helpers = EnsureAssetRegistry()
    if Helpers == nil then return false end

    local AssetData = BuildAssetData(ModInfo.AssetPath, ModInfo.AssetName)
    local ModClass = Helpers:GetAsset(AssetData)
    if ModClass == nil or not ModClass:IsValid() then
        Logf("ModClass for '%s' is not valid (%s)", tostring(ModInfo.Name), DescribeAssetData(AssetData))
        return false
    end

    local Actor = World:SpawnActor(ModClass, {}, {})
    if Actor == nil or not Actor:IsValid() then
        Logf("SpawnActor for '%s' (%s) returned no actor.", tostring(ModInfo.Name),
            ModClass:GetFullName())
        return false
    end

    Logf("Actor: %s", Actor:GetFullName())
    RememberModActorClass(ModInfo, ModClass)
    RunActorEvent(Actor, "PreBeginPlay", tostring(ModInfo.Name))
    return true
end

local function LoadMods(World, Force)
    local LiveWorld = ToWorld(World)
    if LiveWorld == nil then
        -- UEHelpers.GetWorld answers an invalid object while the engine has no
        -- world yet, and the load-map hook brings the next one.
        Log("No valid world to spawn mod actors into yet.")
        return false
    end

    if #OrderedMods == 0 then
        if not WarnedNoMods then
            WarnedNoMods = true
            Log("No Blueprint mods were found in Content/Paks/LogicMods.")
        end
        return false
    end

    local Key = WorldKey(LiveWorld)
    local Loaded = RememberWorldLoad(Key)
    local Spawned, Failed, Skipped = 0, 0, 0

    for _, ModInfo in ipairs(OrderedMods) do
        local ModName = tostring(ModInfo.Name or "?")
        if Loaded[ModName] and not Force then
            Skipped = Skipped + 1
            Debugf("Mod '%s' is already loaded in %s", ModName, Key)
        else
            if ModInfo.Priority ~= nil then
                Logf("Loading mod [Priority: #%d]: %s", ModInfo.Priority, ModName)
            else
                Logf("Loading mod: %s", ModName)
            end

            -- Marked before the spawn, because the spawn is what raises the new
            -- actor's begin-play: a level-load hook answers that event, so it can
            -- re-enter this loop for the world the spawn just landed in. Without the
            -- mark that pass sees the mod as not yet loaded and spawns it a second
            -- time into the same world. A spawn that fails is unmarked again, so the
            -- next pass in this world retries it.
            Loaded[ModName] = true

            if SpawnModActor(ModInfo, LiveWorld) then
                Spawned = Spawned + 1
            else
                Loaded[ModName] = nil
                Failed = Failed + 1
            end
        end
    end

    Logf("World %s: %d spawned, %d failed, %d already loaded (%d mod(s) registered)",
        Key, Spawned, Failed, Skipped, #OrderedMods)
    return Spawned > 0
end

-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→
-- Hooks
-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→

local function OnLoadMapPost(Engine, World)
    -- The world is the second argument by convention, but which argument carries
    -- it is read from what the values are rather than from their positions, and
    -- the runtime's own world is the last resort.
    local LoadWorld = ToWorld(World) or ToWorld(Engine)
    if not IsLiveWorld(LoadWorld) then
        local Fallback = UEHelpers.GetWorld()
        if IsLiveWorld(Fallback) then LoadWorld = Fallback end
    end
    if not IsLiveWorld(LoadWorld) then
        Log("A level finished loading, but no live world could be resolved; mods are not respawned.")
        return
    end

    Logf("Level loaded: %s", LoadWorld:GetFullName())
    LoadMods(LoadWorld, false)
end

local function OnBeginPlayPost(ContextParam)
    local Context = ToObject(ContextParam)
    if Context == nil then return end

    local Class = Context:GetClass()
    if Class == nil or not Class:IsValid() then return end

    local ModInfo = ModByClassFullName[Class:GetFullName()]
    if ModInfo == nil then
        -- A mod actor spawned by something other than this loader is still
        -- recognised: its class's path carries the mod's own package.
        local FullName = Class:GetFullName()
        if type(FullName) == "string" then
            local Lowered = FullName:lower()
            for _, Candidate in ipairs(OrderedMods) do
                if Candidate.AssetPath ~= nil and Candidate.AssetName ~= nil then
                    local Package = string.format("%s.%s", Candidate.AssetPath, Candidate.AssetName)
                    if Lowered:find(Package:lower(), 1, true) ~= nil then
                        ModInfo = Candidate
                        break
                    end
                end
            end
        end
    end
    if ModInfo == nil then return end

    RunActorEvent(Context, "PostBeginPlay", tostring(ModInfo.Name or "?"))
end

-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→
-- Startup
-- ←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→

-- Loading is a game-thread job, and it is forced only when the caller wants the
-- mods respawned in the world they are already in.  A world the loader has not
-- seen yet spawns everything on its own, so the startup call does not force.
local function RequestModLoad(Reason, Force)
    Logf("Loading mods (%s)...", Reason)
    ExecuteInGameThread(function()
        LoadMods(UEHelpers.GetWorld(), Force == true)
    end)
end

local function Startup()
    local LogicMods = FindLogicModsDirectory()
    if LogicMods == nil then return end

    local Listed = DiscoverLogicMods(LogicMods)
    LoadModOrder()
    ApplyModOrder()

    Logf("Found %d mod folder(s) in %s, %d mod(s) registered.",
        Listed, LogicMods.__absolute_path or "LogicMods", #OrderedMods)
    for _, ModInfo in ipairs(OrderedMods) do
        Logf("Registered mod '%s' -> %s.%s",
            tostring(ModInfo.Name), tostring(ModInfo.AssetPath), tostring(ModInfo.AssetName))
    end

    -- The hooks are registered before the first load attempt, so a world that
    -- is not up yet is caught by the level that brings one.
    if type(RegisterLoadMapPostHook) == "function" then
        RegisterLoadMapPostHook(OnLoadMapPost)
    end

    if type(RegisterBeginPlayPostHook) == "function" then
        RegisterBeginPlayPostHook(OnBeginPlayPost)
    end

    -- A manual reload is bound because a world can be entered before the mods
    -- were, and because the mods of the world in play are otherwise only
    -- re-registered by the next level. Trigger the BPModLoader manually by pressing Alt+PGUP.
    if type(RegisterKeyBind) == "function" and type(Key) == "table" and Key.PAGE_UP ~= nil then
        RegisterKeyBind(Key.PAGE_UP, { ModifierKey.ALT }, function() RequestModLoad("manual reload", true) end)
    end

    RequestModLoad("startup", false)
end

Startup()