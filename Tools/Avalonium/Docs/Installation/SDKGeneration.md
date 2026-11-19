# Generate the Game SDK

The SDK is a compilable set of C++ headers generated from the game's live
reflection. It carries typed classes, structs, enums and function wrappers for one
game build, and it reaches the engine through `AvaloniumGetSdkApi` at runtime, so
it bakes no engine addresses. It is consumed in Visual Studio with MSVC. It is
not an Unreal Engine plugin and does not need the engine editor.

The dump reads the running process, so the game must be running with Avalonium
injected and initialized. Do [the runtime check](VerifyRuntime.md) first.

## Generate with the orchestrator

From the toolkit's `Helpers` folder:

```text
cd <Avalonium>\Helpers
python SDKOrchestrator.py
```

It sends `INIT`, then `GENSDK`, and prints the control port's response. Without
an argument the output lands under the log root:

```text
<log root>\<Game>\SDK
<log root>\<Game>\Reflection
```

Pick the output folder yourself:

```text
python SDKOrchestrator.py GENSDK "D:\Dumps\MyGame\SDK"
```

The reflection kit is written to `Reflection` beside that folder, so this example
also writes `D:\Dumps\MyGame\Reflection`.

Host, port and timeout are flags:

```text
python SDKOrchestrator.py --host 127.0.0.1 --port 20030 --timeout 600
```

`iocp_client.py` sends the same command when you already work through it. Raise
its timeout for a large game:

```text
python iocp_client.py -t 600 GENSDK D:\Dumps\MyGame\SDK
```

A path with spaces needs double quotes around it.

## What it writes

Each package produces:

```text
<Package>_classes.h        classes, members at their real offsets, wrappers
<Package>_structs.h        structs at their real offsets
<Package>_parameters.h     parameter blocks used by the wrappers
<Package>_functions.cpp    function wrappers for that package
```

The SDK root also carries `Basic.h`, `SDK.h`, `UtfN.hpp`,
`UnrealContainers.hpp` and `PropertyFixup.hpp`. `Basic.h` resolves
`AvaloniumGetSdkApi` from `Avalonium.dll` on first use. [SDK_MODS.md](../CppAPI/SDK_MODS.md)
documents every file and how a mod compiles against them.

## Verify the run

The control port answers:

```text
Ok: C++ SDK generated at <dir>
```

The runtime log ends with:

```text
[SDK] ====== Full C++ SDK generated successfully! ======
[SDK] Total Files: N (failed: 0)
```

A non-zero failed count is listed above the summary with the file names that
failed.

## Optional: build the SDK on its own

`GenerateBuild.py` writes a CMake build system into the SDK folder so the
generated code can compile without a mod:

```text
python GenerateBuild.py "D:\Dumps\MyGame\SDK" --game-name MyGame
cmake -B build -S "D:\Dumps\MyGame\SDK"
cmake --build build --config Release
```

The ini can also drive this after every dump: `GenerateBuildSystem=true` runs the
script (`BuildScriptPath` optional), and `SDKLibclangGeneration=true` adds the
libclang AST pass (`LibclangPath` optional). See
[CONFIG.md](../RuntimeAPI/CONFIG.md) for the exact keys and defaults.

## Other dumps

- `USMAP` writes `<gameRoot>\USMAP\<Game>.usmap`:
  `python iocp_client.py USMAP`.
- `GENDIFF "<outDir>" "<previous.tsv>" "<current.tsv>"` compares two reflection
  snapshots after a game update and writes `ReflectionDiff.json`.

Command syntax and quoting rules for all three are in
[IPC.md](../RuntimeAPI/IPC.md#generator-commands).

## When to regenerate

Regenerate after every game patch, when moving the mod to another game, or when a
generated wrapper reports a missing function at runtime. Keep each output with
the game build it came from; a generated SDK is not portable across builds.

## Next

Build the mod against the runtime headers first
([Build Your First C++ Mod](FirstCppMod.md)), then add the generated SDK when you
want typed game access ([SDK_MODS.md](../CppAPI/SDK_MODS.md)).
