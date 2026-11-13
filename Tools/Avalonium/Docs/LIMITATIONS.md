# Limitations

Avalonium supports Lua, Blueprint and native C++ mods.

| Mod type | Status |
| --- | --- |
| Lua | Supported |
| Blueprint (BP) | Supported |
| C++ (native) | Supported through the [CppAPI](CppAPI/README.md) |

## Engine support

UE 4.23 to UE 5.8 only.

Making Avalonium work on both the UE4 layout and the UE5 layout without hardcoding for a game
or engine was really, really difficult. Thanks to Epic Games changing the layout of the damn
engine every couple of updates, making it work without hardcoding was even harder. I already
spent months fixing one bug in the UE4 layout that made UE5 games crash. I was thinking about
dropping UE 4.23/UE 4.27, but I didn't, so both stay supported. Now Avalonium reverse engineers
both layouts like nothing.

## C++ mods

Native C++ mods use the CppAPI surface: a DLL exports `AvaloniumModAbiVersion`,
`start_mod` and `uninstall_mod`, and receives one C ABI table from Avalonium.
Engine access is reflected at runtime, so one mod DLL targets every supported
engine version.

C++ mod limits:

- Native and interpreted UFunction hooks. Interpreted hooks are answered by the
  `ProcessLocalScriptFunction` hook; if that entry cannot be installed they cannot
  fire. A base BlueprintImplementableEvent with no bytecode never reaches the
  interpreter and is not observed; hook the class that implements it. A recompiled or
  reinstanced Blueprint UFunction is matched by full function path, flags and parameter
  frame size; a changed path or signature requires re-registration.
- Delegate callbacks carry no arguments, so the object's state is read in the
  callback. Inline multicast and single-cast delegates both bind, any number of
  callbacks per event, but the runtime's dispatch name comes from a delegate
  signature function on the object's class: a class with none on its chain cannot
  be bound. A single-cast delegate holds one value, so a game rebind while a
  binding is active replaces the runtime entry and the callback stops. Sparse
  multicast delegates keep their list in a side registry and are not bound.
- Arrays are typed, and maps and sets are read by live-entry position. Maps and
  sets are mutated through the engine's Blueprint container libraries, so growth,
  hashing and element lifetime are the engine's own. Struct keys are compared by
  raw bytes for lookup, so a padding difference can hide a match, and map keys
  cannot be renamed in place.
- Reflected calls return the function's return value first, then its out values in
  parameter order. A table supplied for an out parameter is also filled under the
  parameter name; structure parameters copy their exposed members into it, and array
  or set parameters fill it when the value is table-shaped.
- Native non-UFUNCTION calls are the mod's declaration: the runtime resolves an
  export, pattern or vtable slot and returns the address, and the mod calls it
  with the real signature on the game thread. A wrong pattern, slot or signature
  faults the process; the runtime cannot validate it. Patterns describe one
  build and are not carried by the runtime.
- No standalone FText value constructor. Widget text is built by
  `RC::Widgets::SetText`; other text values are copied as raw bytes.
- New widget Blueprint classes and new texture assets cannot be created. Mods
  compose the engine's native widget classes and reuse existing textures, fonts,
  brushes and styles. Texture views are D3D11 views or D3D12 descriptors the engine
  already created; a texture without a render resource, or an overlay backend other
  than D3D11 or D3D12, yields 0.
- Data table rows are live table-owned buffers. The API reads a row through the
  table's `RowStruct`; it does not copy or write the row.
- Constructed objects are not GC-rooted by the runtime. A mod attaches them to a
  live outer, property or viewport to keep them alive.
- C++ ImGui shares the runtime's context, so a mod must compile the same ImGui
  version and build configuration as the runtime. While a mod marks its ImGui
  windows input-active, the game does not receive mouse input.
- No Lua interop or mod hot reload.
- `on_unload` is not called at process exit.
- Mod DLLs must be x64 MSVC builds compatible with the runtime's STL ABI.
- C++ mod binaries from other runtimes are rejected before `start_mod`; port the
  source to the CppAPI headers and recompile.

The full API, mod layout and threading rules are in [CppAPI](CppAPI/README.md).

## Generated SDK

The SDK generator produces typed headers for the build it was generated from. Its runtime
layer calls `AvaloniumGetSdkApi` in Avalonium.dll, so a generated SDK runs only in a process
where the runtime is loaded.

- Typed class and struct layouts come from that build's reflection and property offsets.
  Regenerate the SDK after a game update that changes reflection or layout.
- The predefined core types `UObject`, `UField`, `UStruct`, `UClass` and `UFunction` take
  their member offsets and sizes from the live build, so those declarations do not depend
  on a fixed engine version.
- ProcessEvent through the generated SDK follows the runtime's game-thread rule.

## Lua and Blueprint mods

Lua mods run through the Lua runtime and its API ([LuaAPI](LuaAPI/README.md)). Blueprint mods
are cooked packages (`.pak`) whose `ModActor` is spawned by the BP loader mod
([AvaloniumBP](BPMods/AvaloniumBP.md)).
