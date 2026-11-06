# Avalonium commands

How to drive an injected Avalonium runtime from the shell.

## Requirements

- The game runs with Avalonium loaded.
- The runtime IPC server listens on `127.0.0.1:20030` (`ServerPort` in `Avalonium.ini`).
- Python 3 is on `PATH`.

## Setup

Open a shell in the Helpers folder:

```powershell
cd F:\Programming\Projects\C++\Modsmanager\Tools\Avalonium\Helpers
```

## Sending a command

```powershell
python iocp_client.py STATUS
```

`iocp_client.py` opens one connection, sends one command, prints the reply, and closes. The verb is case-insensitive. Words after the verb become arguments; wrap paths in double quotes when they contain spaces.

| Option | Meaning |
| :--- | :--- |
| `-t <seconds>` | socket timeout, default `30` |
| `-o <file>` | save the reply to a file |
| `-q` | suppress the `Sending:` header |

`Connection refused` means the game is closed or the DLL is not loaded.

## JSON sessions

A connection whose first line starts with `{` stays open and exchanges one JSON object per line. Responses carry the request id and either `data` or `error`; unsolicited lines carry `event`.

```json
{"id":1,"cmd":"STATUS"}
{"id":2,"cmd":"QUERY","args":{"action":"find","name":"Player","limit":2,"fields":"ptr,name"}}
{"id":"lua","cmd":"LUA","args":"JSON return GetPlayer():GetName()"}
{"id":3,"cmd":"QUIT"}
```

```json
{"id":1,"ok":true,"data":"Game: <name> | Initialized: YES | Version: UE4.27 ..."}
{"id":2,"ok":true,"data":{"ok":true,"action":"find","scanned":48577,"matched":2,"records":[]}}
{"id":"lua","ok":true,"data":"Pawn_1"}
{"id":3,"ok":true,"data":"OK"}
```

- `args` is a string, an array, or a flat object; object fields become `key=value` arguments.
- An invalid line answers `{"id":null,"ok":false,"error":"invalid json request"}` and keeps the session open.
- Legacy connections still send one command and close, exactly as before.
- A session that sends nothing for 120 seconds is closed; watches and subscriptions end with the connection.

## Status and counters

```powershell
python iocp_client.py STATUS
python iocp_client.py VERSION
python iocp_client.py GLOBALS
python iocp_client.py STATS
```

`STATUS` prints the discovered engine globals and offsets as `key: value` lines. `GLOBALS` prints addresses only. `STATS` and `VERSION` reply in one line each.

```text
UE4.27
```

```json
{"ok":true,"objectCount":75135,"indexBuilt":true,"indexedClasses":3009,
 "regionCount":13274,"imageRegions":1108,"modules":23}
```

## Classes, objects, and actors

```powershell
python iocp_client.py CLASSES 5
python iocp_client.py FINDOBJECT Player
python iocp_client.py QUERY action=find name=Player limit=5 fields=ptr,name,class
python iocp_client.py QUERY action=find class=Pawn limit=2 fields=ptr,name,class
python iocp_client.py ACTORS
python iocp_client.py WORLDACTORS 10
python iocp_client.py FINDACTOR Player
python iocp_client.py ENUMS
```

- `CLASSES <n>` limits the list; `n` defaults to `10000`.
- `ACTORS` and `ENUMS` list everything and ignore a count argument.
- `WORLDACTORS <n>` lists actors from the current persistent level; non-positive values fall back to `10000`.
- `FINDACTOR` matches world actor names, case-sensitive, and stops after 100 hits.
- `FINDOBJECT` uses a prefix index first, then a substring scan.
- `QUERY action=find` filters by `name`, `exact`, `class`, `path`, `outer`, `match`, `actor`, `classonly`, with `limit`, `offset`, and `fields`.

```text
Count=2
000001BB1F068580 PlayerState_2147482060
000001BB6E402AE0 TKA_PlayerCameraManager_C_2147482059
```

`DISCOVER` inventories objects by type:

```powershell
python iocp_client.py DISCOVER items
python iocp_client.py DISCOVER actors Player
python iocp_client.py DISCOVER components
python iocp_client.py DISCOVER all
```

The optional second argument is a name prefix. Without a filter the scan covers the first 5000 objects, so `Count=0` is common in a live game.

## Reflection

Pointers come from earlier replies; `0x` is optional.

```powershell
python iocp_client.py CLASSINFO Pawn
python iocp_client.py NAME 0x1BB5489CA00
python iocp_client.py FULLNAME 0x1BB5489CA00
python iocp_client.py CHAIN 0x1BB5489CA00
python iocp_client.py SUPER 0x1BB5489CA00
python iocp_client.py INHERITS 0x1BB5489CA00 Actor
python iocp_client.py PROPS 0x1BB5489CA00
python iocp_client.py FUNCS 0x1BB5489CA00
python iocp_client.py VTABLE 0x1BB5489CA00 5
python iocp_client.py QUERY action=props target=Pawn
python iocp_client.py QUERY action=funcs target=Pawn
python iocp_client.py QUERY action=class target=Pawn
python iocp_client.py QUERY action=vtable target=0x1BB5489CA00 limit=5
```

```text
Class /Script/Engine.Pawn
Pawn -> Actor -> Object
```

```text
000001BB553C3E80
Name=Actor
```

```json
{"ok":true,"object":"0x1BB5489CA00","vtable":"0x7FF7B3656FA0",
 "entries":[{"slot":0,"address":"0x7FF7B0911900","rva":"0xF31900"}],"count":5}
```

`PROPS` returns offsets, names, and type flags. Type strings can read incorrectly after the first reflection query in a session; offsets and names stay correct.

## Properties

```powershell
python iocp_client.py GET 0x1BB5489CA00 BaseEyeHeight
python iocp_client.py SET 0x1BB5489CA00 BaseEyeHeight 64
python iocp_client.py GET 0x1BB5489CA00 Pawn.BaseEyeHeight
```

```json
64
```

`GET` reads a property, including dotted paths, and answers JSON. `SET` writes one and requires a JSON value; the write is refused before touching memory when the value is not JSON.

```text
OK
ERROR: invalid JSON value
ERROR: property not found or not writable
```

## Watches

Watches live in a JSON session. The initial value comes back in the response; later changes are pushed as unsolicited lines.

```json
{"id":1,"cmd":"WATCH","args":"0x1BB5489CA00 BaseEyeHeight 200"}
{"id":2,"cmd":"SET","args":"0x1BB5489CA00 BaseEyeHeight 65"}
{"id":3,"cmd":"UNWATCH","args":"1"}
```

```json
{"id":1,"ok":true,"data":{"watchId":1,"value":64}}
{"event":"watch","watchId":1,"ptr":"0x1BB5489CA00","path":"BaseEyeHeight","value":65}
{"id":3,"ok":true,"data":"unwatched"}
```

- Interval is milliseconds, clamped to 50..60000, default 500.
- Up to 64 watches per session.
- A watch whose object is no longer readable is dropped silently.

## Event subscriptions

Subscriptions also live in a JSON session and use the runtime hook dispatcher. They require the Lua subsystem.

```json
{"id":1,"cmd":"SUBSCRIBE","args":"ProcessEvent K2_GetActorLocation"}
{"id":2,"cmd":"SUBSCRIBE","args":"ObjectCreated"}
{"id":3,"cmd":"SUBSCRIBE","args":"ObjectDestroyed"}
{"id":4,"cmd":"SUBSCRIBE","args":"LevelTransition"}
{"id":5,"cmd":"UNSUBSCRIBE","args":"1"}
```

```json
{"id":1,"ok":true,"data":{"subscription":1,"token":1,"kind":"ProcessEvent","filter":"K2_GetActorLocation"}}
{"event":"subscribe","token":1,"kind":"ProcessEvent","filter":"K2_GetActorLocation","self":"0x...","selfName":"Pawn_1","selfClass":"Pawn"}
```

| Name | Source |
| :--- | :--- |
| `ProcessEvent <Function>` | a named UFunction call |
| `ObjectCreated` | actor begin-play |
| `ObjectDestroyed` | actor end-play |
| `LevelTransition` | level-load signal |

Events carry the object identity, not the function parameters. A session receives only its own token values; queued events are dropped when the connection closes.

## Memory

```powershell
python iocp_client.py READMEM 0x1BB5489CA00 32
python iocp_client.py READPTR 0x1BB5489CA00 0x40
python iocp_client.py SCANINT 0x1BB5489CA00 256 0
python iocp_client.py DEEPSCAN 0x1BB5489CA00 0
```

```text
0000: A0 6F 65 B3 F7 7F 00 00 43 00 00 00 D7 00 00 00  .oe.....C.......
```

`READPTR` reads a pointer at `base + offset` and appends the object name when non-null. `SCANINT` scans `size` bytes at 4-byte alignment for an int32. `DEEPSCAN` scans the object and the canonical pointers inside it, two levels deep.

## Pattern, string, and reference scans

```powershell
python iocp_client.py AOB "48 8B 05 ?? ?? ?? ??" image 5
python iocp_client.py STRINGS GameModeBase image 5
python iocp_client.py XREF 0x1BB5489CA00 image 5
python iocp_client.py QUERY '{"action":"pattern","pattern":"48 8B 05 ?? ?? ?? ??","scope":"image","limit":5}'
python iocp_client.py QUERY action=string needle=GameModeBase scope=image limit=5
python iocp_client.py QUERY action=pointers target=0x1BB5489CA00 scope=image limit=5
```

Scopes: `image`, `module`, `heap`, `exec`, `all`. The reply is JSON with `hits`, each carrying `address` and `module`. Patterns use space-separated hex bytes and `?` or `??` for a wildcard byte.

```json
{"ok":true,"pattern":"48 8B 05 ?? ?? ?? ??","patternBytes":7,"scanned":86761472,
 "count":2,"truncated":true,"hits":[{"address":"0x7FF7AFEFE059","module":"<game>-Win64-Shipping.exe"}]}
```

## Modules and memory regions

```powershell
python iocp_client.py MODULES
python iocp_client.py REGIONS image 5
```

```json
{"ok":true,"count":23,"modules":[{"name":"<game>-Win64-Shipping.exe","base":"0x7FF7AF9E0000",
 "size":86761472,"end":"0x7FF7B4C9E000","main":true}]}
```

```json
{"ok":true,"action":"regions","count":1107,"regions":[{"base":"0x400000","size":4096,
 "end":"0x401000","protect":"r","type":"image"}],"returned":3}
```

## Bulk class dump

```powershell
python iocp_client.py DUMP
```

```text
SharedMemory=AvaloniumDump_18276
Size=2567200
```

Open the named section with `OpenFileMappingA`; layout is `int32 count` followed by `ClassInfoEx[count]` with a stride of 1044 bytes.

## Generators

### USMAP

Writes the mappings used by model viewers and pak tools.

```powershell
python iocp_client.py -t 300 USMAP
python iocp_client.py -t 300 USMAP "D:\Dumps\Game.usmap"
```

Without a path the file goes to `<gameRoot>\USMAP\<GameName>.usmap`. A working reply:

```text
Ok: USMap generated at D:\Dumps\Game.usmap
```

The orchestrator sends `INIT` then `USMAP`:

```powershell
python MappingOrchestrator.py
python MappingOrchestrator.py "D:\Dumps\Game.usmap"
```

### SDK and Reflection

`GENSDK` writes the C++ SDK into the given folder and the Reflection kit into `Reflection` beside it.

```powershell
python SDKOrchestrator.py
python SDKOrchestrator.py GENSDK
python SDKOrchestrator.py GENSDK "D:\Dumps\SDK"
python iocp_client.py -t 600 GENSDK "D:\Dumps\SDK"
```

- Output: `D:\Dumps\SDK\...` and `D:\Dumps\Reflection\...`.
- Defaults: `<gameRoot>\SDK` and `<gameRoot>\Reflection`.
- A test run against 75k objects wrote 2339 SDK files and finished in about 10 seconds.
- Large games can need a longer timeout; the orchestrator defaults to `300`, `iocp_client.py` to `30`.

The Reflection folder contains `ReflectionDB.json`, `Inheritance.json`, `PackageGraph.json`, `Runtime\`, and `Diff\`.

### Reflection diff

All three paths are required. Wrap each in literal quotes when it contains spaces:

```powershell
python iocp_client.py -t 300 GENDIFF '"D:\Out Dir\Diff"' '"D:\prev dir\ReflectionSnapshot.tsv"' '"D:\cur dir\ReflectionSnapshot.tsv"'
```

The file is written to `<outDir>\Diff\ReflectionDiff.json`. The reply line omits the `Diff` folder.

## Jobs

Long generators run on one worker thread, so the IPC server keeps answering while a dump runs. The commands work in a JSON session and as legacy one-shot commands.

```json
{"id":1,"cmd":"JOB","args":"START GENSDK \"D:\\Dumps\\SDK\""}
{"id":2,"cmd":"JOB","args":"STATUS 1"}
{"id":3,"cmd":"JOB","args":"RESULT 1"}
{"id":4,"cmd":"JOB","args":"LIST"}
{"id":5,"cmd":"JOB","args":"CANCEL 1"}
```

```json
{"id":1,"ok":true,"data":"OK: job 1 started: GENSDK D:\\Dumps\\SDK"}
{"id":2,"ok":true,"data":{"ok":true,"id":1,"label":"GENSDK D:\\Dumps\\SDK","state":"running","elapsedMs":297}}
{"id":3,"ok":true,"data":"Ok: C++ SDK generated at D:\\Dumps\\SDK"}
```

States: `queued`, `running`, `done`, `failed`, `cancelling`, `cancelled`. `JOB RESULT` answers the generator reply once the state is `done` or `failed`, and an error while it is still running.

`JOB CANCEL` drops a queued job and marks a running job cancelled; the running generator is not interrupted, its result is discarded. One job runs at a time.

## Build files for a generated SDK

Offline, no game connection:

```powershell
python GenerateBuild.py "D:\Dumps\SDK" --game-name <Game>
```

It writes `CMakeLists.txt`, a Visual Studio solution and project, filters, and `compile_commands.json` into that folder. Then either open the `.sln` or run:

```powershell
cmake -B build -S "D:\Dumps\SDK"
cmake --build build --config Release
```

Without the path argument it scans the script's own folder.

## Lua

```powershell
python iocp_client.py LUA STATUS
```

```text
Lua: Initialized (Lua 5.4.6)
ModsDir: <mods dir>
LoadedModsCount=0
```

Evaluate an expression or statement; the returned value is printed:

```powershell
python iocp_client.py LUA EVAL "return 1+2"
python iocp_client.py LUA EVAL "return GetObjectCount()"
python iocp_client.py LUA EVAL "local p=GetPlayer(); return p and p:GetName() or 'nil'"
python iocp_client.py LUA EVAL "local p=GetPlayer(); if not p then return 'nil' end return tostring(p:GetProperty('BaseEyeHeight'))"
python iocp_client.py LUA RUN "return 5*5"
```

```text
OK: 3
OK: 48577
OK: Pawn_1
OK: 64.0
OK: 25
```

Use single quotes inside the code and double quotes around it.

`LUA JSON` returns the value as a JSON document instead of a printed string:

```powershell
python iocp_client.py LUA JSON "return {a=1,b='x',c={1,2,3},ok=true}"
python iocp_client.py LUA JSON "return 1, 'two', true"
python iocp_client.py LUA JSON "local p=GetPlayer(); return {name=p:GetName(), addr=p:GetAddressString()}"
```

```json
{"a":1,"b":"x","c":[1,2,3],"ok":true}
[1,"two",true]
{"name":"Pawn_1","addr":"0x..."}
```

Tables, strings, numbers, booleans, and `nil` keep their JSON types. Multiple return values become an array; no return value becomes `null`.

Load files and reload mods:

```powershell
python iocp_client.py LUA LOAD "D:\Scripts\probe.lua"
python iocp_client.py LUA DOFILE "D:\Scripts\probe.lua"
python iocp_client.py LUA RELOAD
```

`LOAD` and `DOFILE` run the file in the shared Lua state, so globals set there are visible to later `LUA EVAL` calls. The scripting surface (object queries, property get/set, hooks, timers, UE4SS compatibility) is documented in [LuaAPI](../LuaAPI/README.md). When the subsystem is disabled, `LUA STATUS` reports `Lua: Not Initialized`.

## State-changing commands

These are available but not safe for blind use:

```powershell
python iocp_client.py WRITEINT <addr> <value>
python iocp_client.py CALLFUNC <objPtr> <functionName>
```

`WRITEINT` writes a 32-bit value into the process. `CALLFUNC` invokes a UFunction with zeroed arguments. `LUA EVAL`, `LUA RUN`, and `LUA LOAD` also execute arbitrary code in the process. All of these can crash or corrupt state.

## Output locations

| Command | Default output |
| :--- | :--- |
| `USMAP` | `<gameRoot>\USMAP\<GameName>.usmap` |
| `GENSDK` | `<gameRoot>\SDK` plus `<gameRoot>\Reflection` |
| `GENDIFF` | `<outDir>\Diff\ReflectionDiff.json` |

`<gameRoot>` is derived from the runtime log directory and the sanitized game name.

## Notes

- Every command blocks the single IPC thread. Long generators block other clients until done.
- A command that takes a pointer parses it as hex, so `0x...` and bare hex both work.
- `QUERY` accepts `key=value` pairs or a flat JSON object for values that contain spaces.
- Keep generated output out of the game install folder when you only need a dump.
