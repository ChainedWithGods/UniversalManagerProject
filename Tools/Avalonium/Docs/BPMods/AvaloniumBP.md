# AvaloniumBP.md: Avalonium Blueprint (`.pak`) Modding

Avalonium's **BPModding** surface loads Blueprint mods packaged as `.pak` files, the same
shape UE4SS ships as "LogicMods". A BP mod is a cooked Unreal package whose
`ModActor` Blueprint is spawned into the live world by a Lua loader mod. There is no separate
native API: BPModding is built entirely from the runtime's Lua API plus two shipped mods. We kept it to Lua doing the dirty work, no magic DLL, no parallel framework.

Shipped pieces (under `Mods/Avalonium/`):

| Mod | Role |
| :--- | :--- |
| `BPModLoader` | the loader: discovers LogicMods, resolves each mod's `ModActor` class and spawns it |
| `GenericFunctions` | the shared custom events every BP mod may call (`PrintToModLoader`, `ConstructPersistentObject`) |
| `shared/UEHelpers/UEHelpers.lua` | the UEHelpers module the loader `require`s (derived from RE-UE4SS, MIT) |

Both loader mods ship **disabled** in the default `mods.json`; set `"enabled": true`.

---

## 1. Enabling BPModding

`Mods/Avalonium/mods.json`. The shipped default disables every entry
(`UniversalHUDToggle`, `UniversalOverlay`, `GenericFunctions`,
`BPModLoader`, `EmptyTest`); enable the two BP loader mods to use Blueprint
mods:

```json
{
  "mods": [
    { "enabled": false, "name": "UniversalHUDToggle" },
    { "enabled": false, "name": "UniversalOverlay" },
    { "enabled": true,  "name": "GenericFunctions" },
    { "enabled": true,  "name": "BPModLoader" }
  ]
}
```

At startup `BPModLoader/scripts/main.lua` runs `Startup()`, which:

1. finds `<Project>/Content/Paks/LogicMods` (creating it when missing via
   `CreateLogicModsDirectory()`), using `IterateGameDirectories()` to locate the project
   root (the node named `Game`);
2. reads each mod folder's own `config.lua` (when shipped) and otherwise registers defaults;
3. reads optional `load_order.txt` for priority ordering;
4. registers `RegisterLoadMapPostHook` and `RegisterBeginPlayPostHook` **before** the first
   load attempt, so a world that is not up yet is caught by the level that brings one. I keep
   that order: backwards, the mods silently do jack shit;
5. binds **Alt+PageUp** to a manual forced reload (`RegisterKeyBind(Key.PAGE_UP,
   { ModifierKey.ALT }, …)`);
6. requests the first load on the game thread via `ExecuteInGameThread`.

## 2. Blueprint mod package layout

```
<Project>/Content/Paks/LogicMods/          <- the LogicMods directory
├── MyBlueprintMod/
│   ├── MyBlueprintMod_P.pak               <- the cooked mod package
│   └── config.lua                         <- optional (see §4)
└── LooseMod.pak                           <- a loose .pak in LogicMods is a mod too
```

Inside the cooked package the mod's Blueprint is expected at:

```
/Game/Mods/<ModName>/ModActor        (the generated class is ModActor_C)
```

The defaults are the Blueprint mod template's: asset name `ModActor_C`, asset path
`/Game/Mods/%s/ModActor`, where `%s` is the **first `.pak` file stem** in the mod folder.
`MyBlueprintMod_P.pak` therefore resolves to `/Game/Mods/MyBlueprintMod_P/ModActor`
(the `_P` suffix is part of the stem). A mod may override the path/asset name in its own
`config.lua` (§4). I treat the `_P` suffix as part of the stem; getting cute with it breaks the path. Strip it
and the loader finds jack shit, then the mod silently never spawns.

## 3. What the loader does per mod

For each registered mod, in priority/name order, `SpawnModActor`:

1. resolves `UAssetRegistryHelpers` by path
   `/Script/AssetRegistry.Default__AssetRegistryHelpers` (and best-effort the registry
   interface / `Default__AssetRegistryImpl`);
2. builds the `FAssetData` for the mod's asset and calls
   `AssetRegistryHelpers:GetAsset(AssetData)` to obtain the mod's `ModActor` class;
3. spawns it with `World:SpawnActor(ModClass, {}, {})`;
4. records the class (by full name) so the begin-play hook recognises the actor by identity;
5. runs the actor's own `PreBeginPlay` event when it is reflected.

**Level transitions:** the map-load hook (`RegisterLoadMapPostHook`) calls `LoadMods` for the
new world; each mod is spawned once per world (the world is keyed by full name, the most
recent 8 worlds are tracked). A manual reload forces a respawn in the current world.

**Begin-play:** `RegisterBeginPlayPostHook` runs a mod actor's own `PostBeginPlay` event when
its class matches a registered mod. `AActor` declares neither `PreBeginPlay` nor
`PostBeginPlay` as a UFUNCTION, so these only ever run a mod's own events.

### 3.1 `FAssetData` differs across versions

I make the loader version-aware because the `FAssetData` structure is not the same on both
sides of UE 5.1, and Epic can't leave a struct the fuck alone across versions. The runtime
discovers the live structure and its member offsets:

* **4.23 – 5.0**: the lookup object is `{ ObjectPath = <FName "Package.Asset"> }`, which
  `FAssetData::GetAsset()` resolves.
* **5.1 and later**: `ObjectPath` is deprecated/editor-only, so the object is described with
  `{ PackageName = <FName>, AssetName = <FName> }` instead.

The version check is `UnrealVersion:IsBelow(5, 1)`.

## 4. `config.lua` and the `Mods` registry

The loader publishes a global table named **`Mods`** (its registry) and runs each mod's
`config.lua` in an environment whose `Mods` names that table, so a config file assigns:

```lua
Mods["My Mod Name"] = {
    AssetName = "ModActor_C",              -- default
    AssetPath = "/Game/Mods/MyMod/ModActor" -- default: /Game/Mods/<pak stem>/ModActor
}
```

A config that names no mod falls back to a default registration under the folder name. A mod
folder with no `config.lua` is registered with the defaults above. `config.lua` is compiled
with the loader's own environment as its `__index` fallback, so it sees the same globals.

## 5. Load order

`load_order.txt` (looked up at `<ModsRoot>/BPModLoader/load_order.txt`, then
`Mods/BPModLoader/load_order.txt`, then `load_order.txt`) lists mod names one per line
(`;`-prefixed lines are comments). Listed mods load first, numbered as priorities; everything
else loads afterward by name.

## 6. Calling back into the loader (BPML custom events)

`GenericFunctions` registers custom events that a BP mod dispatches by UFunction short
name through `RegisterCustomEvent`:

### `PrintToModLoader(Message)`

Prints a message attributed to the calling mod. The callback declares
`function(ParamContext, ParamMessage)`, reads `ParamMessage:get()` and requires it to be an
`FString`, then logs `[<ModName>] <message>` where `ModName` is derived from the caller's class
full name.

### `ConstructPersistentObject(Class, OutParam)`

Constructs an object through the engine and writes it to the out-parameter:

```lua
StaticConstructObject(Class, GameInstance, 0, 0, GarbageCollectionKeepFlags, false, false, nil, nil, nil)
```

`GarbageCollectionKeepFlags` is `0x0E000000`. The class must be a valid `UClass`
(`StaticFindObject("/Script/CoreUObject.Class")` is used as the base check). The out-value
check is currently ineffective: `if not OutValue:type() == "UObject"` parses as
`(not OutValue:type()) == "UObject"`, so a non-`UObject` out value is not rejected.

`PrintToModLoader` type-checks its message and raises on a mismatch. Do not rely on the same
guard for `ConstructPersistentObject`'s out value, I'd fix it, but right now that check is
broken, not quirky, and I am not pretending otherwise.

## 7. Lua API used by BP mods

A BP mod (through these loader mods) exercises this documented Lua surface:

```
RegisterCustomEvent  RegisterLoadMapPostHook  RegisterBeginPlayPostHook  RegisterKeyBind
ExecuteInGameThread  StaticFindObject  StaticConstructObject  SpawnActor
FindFirstOf  FindAllOf  IterateGameDirectories  CreateLogicModsDirectory
GetModsDirectory  Key  ModifierKey  EFindName  NAME_None  FName  UnrealVersion
CreateInvalidObject  print  ModRef:GetSharedVariable  ModRef:SetSharedVariable
```

UObject members used on the objects returned: `:get()`, `:IsValid()`, `:IsA()`, `:GetClass()`,
`:GetFullName()`, `:GetName()`, `:GetWorld()`, `:SpawnActor()`, `World:SpawnActor()`,
`Object:GetAssetRegistry()`, `Helpers:GetAsset(AssetData)`, dynamic member reads
(`Engine.GameViewport`, `GameState.PlayerArray`, `PlayerController.Pawn`,
`World.PersistentLevel`, `World.AuthorityGameMode`, …) which resolve through reflection.

## 8. `UEHelpers` (shared module)

`require("UEHelpers")` resolves to the built-in helper table (preloaded by the runtime) merged
with `shared/UEHelpers/UEHelpers.lua` on first use. Exported functions (module version 3):

```
GetUEHelpersVersion  GetEngine  GetGameInstance  GetGameViewportClient
GetPlayerController  GetPlayer  GetWorld  GetPersistentLevel
GetGameModeBase  GetGameStateBase  GetWorldSettings  GetWorldContextObject
GetAllPlayerStates  GetAllPlayers  GetActorFromHitResult
GetGameplayStatics  GetKismetSystemLibrary  GetKismetMathLibrary
GetKismetStringLibrary  GetKismetTextLibrary  GetGameMapsSettings
FindFName  AddFName  FindOrAddFName
```

`GetWorld()` returns an invalid object (not `nil`) while the engine has no world, so callers
test with `:IsValid()`. `FindOrAddFName(name)` performs `FName(name, FNAME_Find)` then
`FName(name, FNAME_Add)` when the name is `NAME_None`. `GetActorFromHitResult` handles the
`FHitResult` layout differences across UE versions.

## 9. Notes and limits

* BP modding requires the game's cooked assets to be mountable from `Content/Paks/LogicMods`;
  I only resolve and spawn already-cooked Blueprint classes; source `.uasset` bullshit isn't
mountable.
* A mod actor that declares no `PreBeginPlay`/`PostBeginPlay` simply is not notified; nothing
  fails.
* The loader creates `Content/Paks/LogicMods` when it is missing and logs when the game
  directory layout cannot be described.
* Respawn is idempotent per world and re-entrancy safe: a mod is marked loaded before its
  spawn so a level-load re-entry does not spawn it twice; a failed spawn is unmarked so the
  next pass retries it.
