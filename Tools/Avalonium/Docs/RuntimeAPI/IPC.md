# RuntimeAPI: IPC Command Protocol

`Runtime/api/IOCPServer.cpp` runs a single-threaded TCP command server on
`127.0.0.1:<ServerPort>` (default `20030`, from `Avalonium.ini`).

## Transport contract

| Property | Value |
| :--- | :--- |
| Protocol | TCP, line-oriented text, UTF-8 |
| Bind | loopback only (`INADDR_LOOPBACK`) |
| Connections | one at a time; handled serially on the server thread |
| Request size | up to 64 KiB (`recv` buffer 65536) |
| Response | sent in a loop until fully written, then the socket is closed with a clean FIN |
| Timeouts | 10 s receive / 10 s send |
| Framing | newline-terminated; a half-close of the write side also ends the command. The server loops on `recv` until it sees `\n` or EOF, so a command split across TCP segments is reassembled |
| Port reuse | `SO_REUSEADDR`, `TCP_NODELAY`, 1 MiB socket buffers |

The command verb is normalized before dispatch: a UTF-8 BOM is stripped, all
control/whitespace characters are removed, and the verb is uppercased. So
`status`, ` STATUS ` and `\xEF\xBB\xBFstatus` all dispatch. Arguments are parsed
with `istringstream`; quoted arguments are supported by commands that read whole
lines (`USMAP`, `GENSDK`, `GENDIFF`, `LUA`).

> **Pointers are printed by MSVC `%p`, which emits no `0x` prefix.** Every
> pointer in a response is bare hex, `0000029316B92140`, not
> `0x0000029316B92140`. Commands that *accept* a pointer are the opposite: they
> parse with `strtoull(..., 16)`, so the `0x` is optional on input. When feeding
> a value you read back into another command, it works either way.

Legacy connections are a **single request/response exchange**; connect, send,
receive until EOF, close.

A connection whose first line starts with `{` switches to **JSON session mode**:
one JSON request per line, the connection stays open, and every reply is a
newline-terminated JSON line, `{"id":<id>,"ok":true,"data":...}` or
`{"id":<id>,"ok":false,"error":"..."}`. Unsolicited lines carry `event`.
Session-only verbs: `JOB`, `GET`, `SET`, `WATCH`, `UNWATCH`, `SUBSCRIBE`,
`UNSUBSCRIBE`. Tested examples live in [COMMANDS.md](COMMANDS.md).

### Python client

```python
import socket

def query(cmd: str, port: int = 20030, timeout: float = 10.0) -> str:
    with socket.create_connection(("127.0.0.1", port), timeout=timeout) as s:
        s.sendall(cmd.encode("utf-8"))
        s.shutdown(socket.SHUT_WR)
        chunks = []
        while True:
            data = s.recv(65536)
            if not data:
                break
            chunks.append(data)
        return b"".join(chunks).decode("utf-8", "replace")

print(query("VERSION"))
```

Errors are returned in-band as `ERROR: <reason>\n`. Unknown verbs answer
`ERROR: Unknown command: <VERB>\n`.

---

## Command reference

### JSON session commands

| Verb | Purpose |
| :--- | :--- |
| `JOB START\|STATUS\|RESULT\|LIST\|CANCEL` | run `GENSDK`, `USMAP` or `GENDIFF` on a worker thread |
| `GET <ptr> <path>` / `SET <ptr> <path> <json>` | typed property read and write |
| `WATCH <ptr> <path> [ms]` / `UNWATCH <id>` | polled property change push |
| `SUBSCRIBE <event> [filter]` / `UNSUBSCRIBE <id>` | hook events: `ProcessEvent`, `ObjectCreated`, `ObjectDestroyed`, `LevelTransition` |

`WATCH` and `SUBSCRIBE` require a JSON session. Everything below works in both
modes.

### `STATUS`

Returns `GetStatus()` (see [EXPORTS.md](EXPORTS.md#const-char-getstatus)).

```text
Game: ExampleGame
Initialized: YES
Version: UE4.27
GUObjectArray: 00007FF6E52AC668
GNames: 00007FF6E5270340
GWorld: 000002933A3D5160
GMalloc: 0000029314B46DC0
GNatives: 00007FF6E5388EC8
GUObjectHashTables: 00007FF6E53F0498
GEngine: 000002932428E020
ProcessEvent: VTable[68]
Class: 000002931860A600
Object: 0000029316B92140
Actor: 0000029317BA3E80
Pawn: 00000293185BCA00
Enum: 0000029318609640
Function: 000002931860A180
Package: 000002931860A3C0
Struct: 000002931860AA80
World: 00000293186B4300
Objects: 74045
Globals: YES
Functions: YES
CoreClasses: YES
Offsets: YES
SuperOff: 0x40
ChildrenOff: 0x48
UFieldNextOff: 0x28
ChildPropsOff: 0x50
FFieldNameOff: 0x28
FFieldNextOff: 0x20
```

### `INIT`

Runs `Initialize()`.

* `OK\n`: already initialized or discovery succeeded.
* `ERROR: <GetLastErrorMsg()>\n`: discovery failed.

### `GLOBALS`

Returns discovered globals, with `GWorld` read live through `GWorldPtr`:

```text
GUObjectArray=00007FF6E52AC668
FNamePool=00007FF6E5270340
GWorld=00000293587EE040
GWorldPtr=00007FF6E53F3DB0
GMalloc=0000029314B46DC0
GNatives=00007FF6E5388EC8
GUObjectHashTables=00007FF6E53F0498
ObjectCount=74045
```

`GWorld` is the live value read through `GWorldPtr`; `GWorldPtr` is the address of
the global itself. They are two different numbers by design.

### `VERSION`

`GetDetectedVersion()`: e.g. `UE5.3\n`.

### `CLASSES [maxCount]`

`maxCount` defaults to `10000`. Response:

```text
Count=2460
0000029316B92140 Object
000002931860A600 Class
...
```

### `ACTORS`

Fixed cap of 10000; enumerates **all** live actors via `GetAllActors`.

```text
Count=5019
0000029317BA01C0 BP_Player_C
...
```

### `WORLDACTORS [limit]`

Actors of the current persistent level; `limit` defaults to `10000`
(non-positive values are clamped to 10000).

```text
Count=4580
000002933A026980 StaticMeshActor
...
```

### `FINDACTOR <substring>`

Scans world actors for names containing `<substring>` (case-sensitive
`std::string::find`), stops after 100 matches.

```text
Count=2
000002933A026980 PlayerState_2147482075
0000029341B3...
```

### `ENUMS`

Same shape as `CLASSES` but for `UEnum` objects.

### `NAME <hexPtr>` / `FULLNAME <hexPtr>` / `CHAIN <hexPtr>` / `SUPER <hexPtr>`

Pointer is parsed with `strtoull(..., 16)`, `0x` prefix optional.

* `NAME` → `GetObjectName`
* `FULLNAME` → `GetObjectFullName`
* `CHAIN` → `GetInheritanceChain`
* `SUPER` → `"<ptr> <name>\n"` or `"0000000000000000\n"` when no super exists.
  Note the null case is **16 zero digits, not `0x0`**, MSVC `%p` pads to the
  pointer width.

### `INHERITS <hexPtr> <baseClassName>`

`true\n` or `false\n`, using `InheritsFrom` (super-chain name walk, depth ≤ 50).

The base class name is matched against the **whole** name at each step, so the
comparison is exact and case-sensitive (`Object`, not `object`).

> **Passing only a pointer answers `true`.** `INHERITS 0xDEADBEEF` with no second
> argument parses an empty name and `InheritsFrom` compares it against the first
> chain entry, an empty `GetObjectNameStr` result on an unreadable pointer
> compares equal to the empty needle. Always send both arguments; never treat a
> bare `INHERITS <ptr>` as a meaningful answer.

### `FINDOBJECT <substring>`

Finds objects by name.

* If the object index is built, uses `FindObjectsByPrefix` (prefix match) and
  returns all matches.
* If that returns nothing, falls back to a linear scan of the whole object
  array for a **substring** match, stopping after 100 matches.

```text
Count=100
0000029317BA01C0 NiagaraDataInterfaceAudioPlayer
...
```

The index-backed path is a **prefix** match, the fallback path a **substring**
match, so a name given as a substring is still found. The first prefix query
builds the lazily-materialised name index, which takes a one-off sort of the
name-to-object table under the index mutex; see [`STATS`](#stats) and
[`sortedNameIndexReady`](#stats).

### `PROPS <hexPtr | objectName>`

When the argument does not look like a hex pointer (no `0x`, length > 2, parse
yields 0) it is treated as an **exact object name**, searched linearly.

```text
Count=5
[0x2A8] Health : FloatProperty
[0x2B0] Inventory : ArrayProperty [Array]
[0x2C0] Stats : StructProperty [Struct]
```

Flags shown: `[Array]`, `[Map]`, `[Struct]`.

### `FUNCS <hexPtr>`

Walks the struct's field chain (max 500 entries), printing each function and,
when `UFunction_FlagsOff` is known, decoded flag names:

```text
0000029318C29FC0 ReceiveBeginPlay [0x80204000 Native Final BPCallable]
Count=120
```

Decoded bits: `0x1 Final`, `0x100 BlueprintEvent`, `0x400 Native`,
`0x200000 Public`, `0x4000 Net`, `0x04000000 BPCallable`.

### `CALLFUNC <hexObjPtr> <functionName>`

Looks up `functionName` on the object's class (or on the object itself when it
is a class), computes the parameter frame size (`UStruct_ChildPropsOff + 8`, or
`0x58` fallback), allocates and zeroes the frame, and dispatches through
`CallProcessEvent`.

```text
Called ReceiveBeginPlay on 0000029317BA01C0 -> OK
UFunction=0000029318C29FC0 ParamsSize=16
```

Errors: invalid pointer, `ProcessEvent not discovered`, function not found.

> Functions with parameters are called with **zeroed arguments**; there is no
> way to pass values through this command. Use the Lua `CallFunction` API or the
> overlay Live Editor for parameterized calls.

### `READMEM <hexAddr> [size]`

`size` defaults to 64. Anything outside 1..4096 (including `0`, negatives and
oversized values) is silently replaced by the 64-byte default. Output is a
classic hex dump:

```text
0000: 48 8B 05 11 22 33 44 55 90 90 90 90 90 90 90 90  H..."3DU........
```

Returns `ERROR: Read failed\n` when the range is not readable.

### `READPTR <hexBase> <hexOffset>`

Reads a pointer at `base + offset`; when non-null, appends the object's name:

```text
0000029316B92140
Name=BP_Player_C
```

When the pointer is null the `Name=` line is omitted entirely, so a lone
`0000000000000000` line means "read fine, target is null".

Offset is parsed as **hex** even without `0x`.

### `WRITEINT <hexAddr> <value>`

Writes a 32-bit integer: `VirtualProtect(addr, 4, PAGE_READWRITE)`, `memcpy`,
restore protection. `<value>` accepts base prefixes (`0x`, `0`). Response:

```text
Written 1234 (0x4D2) to 0000029317BA01C0
```

### `SCANINT <hexBase> <size> <value>`

Scans `size` bytes at 4-byte alignment for an exact `int32`. A size that is
`<= 0` or greater than 1 MiB is replaced by **64 KiB** (0x10000), not clamped
to the limit. Stops at 50 hits.

```text
FOUND at 0000029317BA01C0 (base+0x1A0) = 100 (0x64)
Total=12
```

### `DEEPSCAN <hexObjPtr> <value>`

Scans the first `0x1000` bytes of the object for the `int32`, then scans the
first `0x1000` bytes of every canonical pointer found inside those bytes
(skipping the main module's own image), and names each hit object.

```text
OBJ+0x2A8 = 100 at 0000029317BA01C0
PTR[OBJ+0x18]->0x0 = 100 at 0000029318C29FC0 (BP_Player_C)
Total=7
```

Capped at 50 results. Two nested levels only.

### `DUMP`

Dumps all UClasses into shared memory and reports the section name/size.

```text
SharedMemory=AvaloniumDump_12345
Size=123456
```

Consumer (out of process):

```cpp
HANDLE h = OpenFileMappingA(FILE_MAP_READ, FALSE, "AvaloniumDump_12345");
auto* base = (uint8_t*)MapViewOfFile(h, FILE_MAP_READ, 0, 0, 0);
int32_t count = *(int32_t*)base;
auto* classes = (ClassInfoEx*)(base + 4);
```

`ClassInfoEx` is packed (`Name[256]`, `FullPath[512]`, `SuperClassName[256]`,
`Address`, `PropertiesCount`, `FunctionsCount`, `Size`), stride **1044 bytes**.

The table holds **classes**, and three details of that word are load-bearing.

**A class-default object is not a class, and does not appear.** A CDO is an
*instance* of its class, so the CDO of `UClass` answers "what is your class" with
`UClass` exactly as a real class does, and a table built from that question alone
admits it. It arrives named `Default__Class` with an empty superclass and no
members, a phantom root. The engine separates the two by flag: it allocates a
CDO with `RF_Public|RF_ClassDefaultObject|RF_ArchetypeObject`
(`UClass::CreateDefaultObject`, `Private/UObject/Class.cpp`) and tests for a CDO
with `HasAnyFlags(RF_ClassDefaultObject)` (same file, lines 3564 / 5345 / 6241).
The enumeration uses the same test, so **`Object` is the only entry with an empty
`SuperClassName`**, because `UObject` is the root and genuinely has no superclass.

**`PropertiesCount` includes inherited properties.** It is the total the class
reports, its own plus every superclass's, which is what `CLASSINFO` returns for
the same class, so the two commands agree by construction. It is not capped the
way a single `CLASSINFO` query is.

**`FunctionsCount` counts `UFunction` entries only.** A `UClass`'s `Children` list
holds plain `UField`s as well, and those are not counted.

**`Size` is `UStruct::PropertiesSize`**, the engine's own
`UStruct::GetPropertiesSize()` (`Public/UObject/Class.h`): not `sizeof`, and not
`GetSize()`'s aligned form. For `AActor` in UE 4.27 it reads `0x220`.

Measured on UE 4.27: 2459 entries, `PropertiesCount` non-zero for 1968,
`FunctionsCount` for 644, `Size` for all 2459.

### `DISCOVER <type> [filter]`

Inventory probe. `type` ∈ `items` | `actors` | `components` | `all`
(default `items`). Optional `filter` is a name prefix, applied through the
prefix index; with no filter, the first 5000 objects are scanned. Max 100
results. `_C`/`_d`/`_m`/`_orm` suffixes and names starting with `Default__`
are filtered out.

* `items`: objects whose class has `MapProperty`/`ArrayProperty` members,
  plus nonzero numeric members: `[Map:X] [Array:Y] [Health=100]`.
* `actors`: objects deriving from `Actor`.
* `components`: objects deriving from `ActorComponent`.
* `all`: every surviving object with the same annotations.

```text
Count=2
0000029319E99480 BP_Container [Map:Items] [Array:Slots]
```

> **`actors` and `components` usually answer `Count=0` in a live game *without* a
> filter.** With no filter the candidates are the first 5000 entries of the
> object array, and spawned actors almost always live outside that window.
> Supplying a filter is not affected by the cap -- a filter is applied through
> the prefix index, which covers every indexed name -- but a filter narrows the
> set to what it matches, so `DISCOVER actors Actor` answers about the actors
> whose name starts with `Actor`, not about all actors. For unrestricted actor
> enumeration prefer [`ACTORS`](#actors),
> [`WORLDACTORS`](#worldactors-limit) or [`FINDACTOR`](#findactor-substring),
> which walk the full index.

### `QUIT` / `EXIT`

Returns `BYE\n` (the connection is closed by the server immediately after).

---

## Analysis commands (JSON)

These answer with a JSON document rather than the flat text of the commands
above. They are read-only with respect to game state: they enumerate, search and
describe, but never call into the engine or write memory. A bad query degrades
to `{"ok":false,"error":"..."}`, it can never crash the process.

Every address in a JSON answer carries an explicit `0x` prefix (unlike the
legacy text commands, which inherit MSVC `%p`). Address arguments are always
parsed as **hexadecimal**, with or without the prefix, so a pointer copied out of
any answer can be pasted straight back into any command.

---

### `QUERY <args>`

The general endpoint. Arguments are either `key=value` pairs or a flat JSON
object; both parse into the same map.

```text
QUERY action=find name=Player class=Pawn limit=50 fields=ptr,name,class,path
QUERY {"action":"find","name":"Player","limit":50}
```

#### `action=find`: filtered object search

Walks the whole object array and filters. Scans 74 045 objects in ~15 ms.

| Key | Meaning |
| :--- | :--- |
| `name` | Substring of the object name (case-insensitive) |
| `exact` | Exact object name |
| `class` | Class name; also matches **subclasses**. Prefix with `-` to force the inheritance test (`IsStructA`, which includes the class itself and derived classes) instead of the name/exact-pointer match |
| `path` | Substring of the full object path |
| `outer` | Substring of the outer object's name |
| `match` | Substring matched against name *or* class |
| `actor` | `true`, only `AActor`-derived objects |
| `classonly` | `true`, only `UClass` objects |
| `limit` | Max records returned (default 200, cap 100000) |
| `offset` | Skip the first N matches |
| `fields` | Comma list of members to emit; omit for all |

Available `fields`: `ptr`, `name`, `class`, `path`, `outer`, `index`, `flags`,
`size`. Selecting fields is what keeps a large answer small, asking for only
`ptr,name` cuts a 5000-record reply from 410 KB to about 130 KB.

```json
{"ok":true,"action":"find","scanned":74045,"matched":2,"offset":0,"limit":2,
 "records":[{"ptr":"0x2326E2B1A00","name":"Default__Pawn","class":"Pawn",
             "outer":"/Script/Engine","isActor":true}],"returned":2}
```

#### Other actions

| `action=` | Returns |
| :--- | :--- |
| `props` | Property list for `target` (pointer, name or full path) |
| `funcs` | Function list for `target` |
| `class` | Same document as [`CLASSINFO`](#classinfo) |
| `vtable` | Same document as [`VTABLE`](#vtable-ptr-limit) |
| `pattern` | Same as [`AOB`](#aob-pattern-scope-limit-base-size), needs `pattern` |
| `string` | Same as [`STRINGS`](#strings-needle-scope-limit-utf16), needs `needle` |
| `pointers` | Same as [`XREF`](#xref-target-scope-limit), needs `target` |
| `modules` | Same as [`MODULES`](#modules) |
| `regions` | Region list; `images`/`mapped`/`private`/`readable` toggle types |
| `stats` | Same as [`STATS`](#stats) |

An unknown action answers `{"ok":false,"error":"unknown action: ..."}` with the
list of valid actions.

---

### `CLASSINFO`

```text
CLASSINFO <ptr | ClassName | /Path.Name>
```

Everything known about one class, struct or object in a single call: identity,
class flags, object flags, full inheritance chain, struct sizes, every property
with its offset and type flags, every function with its decoded flags, and the
vtable address.

```text
CLASSINFO Pawn
```

```json
{"ok":true,"name":"Pawn","path":"Class /Script/Engine.Pawn",
 "address":"0x1E5BF16CA00","class":"Class","isClass":true,"isStruct":true,
 "objectFlags":67,"objectFlagsHex":"0x43",
 "hierarchy":["Pawn","Actor","Object"],
 "propertiesSize":640,"minAlignment":8,"propertyCount":90,
 "properties":[{"name":"bUseControllerRotationPitch","type":"BoolProperty",
                "offset":552,"offsetHex":"0x228","declaredIn":"Pawn","bool":true}],
 "functions":[{"name":"ReceiveBeginPlay","ptr":"0x...","flags":2148532224,
               "flagsHex":"0x80100000","native":true,"public":true}],
 "vtable":"0x7FF6E4216FA0"}
```

The target is resolved in this order: hex pointer, then exact name, then full
object path, then cached class lookup. `propertiesSize` is the value the engine
itself would use for `sizeof` of an instance, which is what a raw-memory reader
needs to know how much to copy.

---

### `VTABLE <ptr> [limit]`

Dumps the virtual function table reached from an object pointer. Each entry is
resolved to the module that owns it and given as an RVA, so a slot can be
compared across builds.

```text
VTABLE 0x23266992140 8
```

```json
{"ok":true,"object":"0x23266992140","vtable":"0x7FF6E4216FA0",
 "vtableModule":"ExampleGame-Win64-Shipping.exe",
 "entries":[{"slot":0,"address":"0x7FF6E4216B20",
             "module":"ExampleGame-Win64-Shipping.exe","rva":"0x3C76B20"}],
 "count":8}
```

The walk stops at the first slot that is not a canonical pointer, so the answer
is the real table rather than a fixed-size slice of whatever follows it.

---

### `AOB <pattern> [scope] [limit] [base] [size]`

Array-of-bytes search. Alias: `PATTERN`. The pattern is space-separated hex
bytes; `?` or `??` is a single-byte wildcard.

```text
AOB 48 8B 05 ?? ?? ?? ?? image 3
AOB 48 8B 05 ?? ?? ?? ?? image 50 0x7FF6E05A0000 0x100000
```

| Scope | Regions searched |
| :--- | :--- |
| `image` (default) / `main` | The main executable's image |
| `module` | Every loaded PE image |
| `heap` | Private committed memory |
| `exec` | Executable regions only |
| `all` | Everything readable |

Passing an explicit `base` and `size` overrides the scope entirely, which is how
you scan a single function or a specific heap block.

```json
{"ok":true,"pattern":"48 8B 05 ?? ?? ?? ??","patternBytes":7,
 "scanned":86761472,"count":3,"truncated":true,
 "hits":[{"address":"0x7FF6E0ABE059","module":"ExampleGame-Win64-Shipping.exe"}]}
```

Performance: the first pattern byte is indexed per region, so the full
comparison only runs where a match can begin. An 83 MB image scans in **~140 ms**
for a fixed leading byte and **~56 ms** when the pattern starts with a wildcard.
`truncated` tells you the limit was hit and more matches exist.

---

### `STRINGS <needle> [scope] [limit] [utf16]`

Substring search for text. Alias: `FINDSTR`. `utf16` defaults to `1` (on); pass
`0` to search ASCII only. Both encodings are searched in one pass when enabled.

```text
STRINGS GameModeBase image 20
STRINGS "Loading screen" module 50 1
```

Answer shape matches `AOB`. An 83 MB image scans in **~130–210 ms**.

---

### `XREF <target> [scope] [limit]`

Cross-reference scan. Alias: `POINTERS`. Finds every 8-byte aligned
pointer-sized value equal to `target`. This is the primitive for "who holds a reference to this object",
"where is this vtable stored", and "which global points at this class".

```text
XREF 0x2326E2B1A00 all 20
```

---

### `MODULES`

Every loaded module with base, size, end and path. The `main` flag marks the
game executable. 28 modules in ~6 ms.

```json
{"ok":true,"count":28,"modules":[
  {"name":"GameName-Win64-Shipping.exe","base":"0x7FF6E05A0000",
   "size":86761472,"end":"0x7FF6E585E000","main":true}]}
```

Module names are the **file name only**, not the full path, so they are short
enough to print in a hit list.

---

### `REGIONS [image|heap|all] [limit]`

Committed memory regions with base, size, protection and owning module. Use this
to pick a scan range, or to see where a process is actually spending memory.
Only `image` and `heap` select a filter; any other token (including `exec`) is
treated as `all`.

```text
REGIONS image 5
```

```json
{"ok":true,"action":"regions","count":1375,"regions":[
  {"base":"0x717E0000","size":16384,"end":"0x717E5000","protect":"rx","type":"image"}],
 "returned":5}
```

---

### `STATS`

Engine and process counters that do not fit the flat `STATUS` block: object
count, index readiness, region and byte totals by type, and module count.

```json
{"ok":true,"objectCount":74045,"indexBuilt":true,"indexedClasses":3009,
 "indexedNameSlots":1,"nameToFirstObjectSlots":29287,"sortedNameIndexReady":false,
 "regionCount":14849,"regionBytes":3846295552,"imageRegions":1376,
 "imageBytes":584617984,"modules":28}
```

`sortedNameIndexReady` reports whether the lazily-built prefix index has been
materialised yet. It starts `false` and becomes `true` after the first
prefix-index query, a filtered [`DISCOVER`](#discover-type-filter) or the
index-backed path of [`FINDOBJECT`](#findobject-substring). It is a cache that
the first prefix query fills, not a state that has to be waited for: no
separate warm-up is required, and a command that needs the index will build it
on demand.

---

## Generator commands

### `USMAP ["outputPath"]`

Without an argument, writes to
`<gameRoot>\USMAP\<GameName>.usmap`, where `<gameRoot>` is derived from the log
directory plus the sanitized game name. With an argument, the path is used
verbatim (quotes stripped).

```text
Ok: USMap generated at <path>\USMAP\Game.usmap
```

`ERROR: Failed to generate USMap` otherwise.

### `GENSDK ["outputDir"]`

Without an argument: `<gameRoot>\SDK`. With an argument: that directory.
The reflection kit is written to `Reflection` beside that directory, so the
default run also produces `<gameRoot>\Reflection`.

```text
Ok: C++ SDK generated at <dir>
```

### `GENDIFF "<outDir>" "<previousSnapshot.tsv>" "<currentSnapshot.tsv>"`

All three quoted arguments are required; paths are tokenized with quote support.

```text
Ok: Reflection diff generated at <outDir>\ReflectionDiff.json
```

---

## Lua commands

### `LUA STATUS`

```text
Lua: Initialized (Lua 5.4.6)
ModsDir: <path>
LoadedModsCount=4
Mod=UniversalHUDToggle
...
```

or `Lua: Not Initialized\n`.

### `LUA EVAL <code>` / `LUA RUN <code>`

Executes a compiled buffer:

* `OK: <result>` on success (`<result>` may be empty).
* `ERROR: <message>` on compile/runtime error.

The entire remainder of the line is the script.

### `LUA LOAD <path>` / `LUA DOFILE <path>`

Loads and executes a `.lua` file (quotes stripped).

* `OK: Loaded <path>`
* `ERROR: <message>`

### `LUA RELOAD` / `LUA RESET`

Reloads every enabled mod.

* `OK: Lua mods reloaded`
* `ERROR: Lua mods runtime is disabled in configuration` when the Lua subsystem
  never initialized.

### Unknown subcommand

```text
ERROR: Unknown LUA subcommand. Valid: STATUS, EVAL, LOAD, RELOAD
```

---

## Practical notes

* **Concurrency.** The server handles one connection at a time and executes
  commands on its own thread. Long generator commands (`GENSDK`, `USMAP`)
  block other clients until done. Discovered/reflection reads are internally
  locked; direct memory writes (`WRITEINT`) are not, you are modifying the
  game.
* **Timeouts.** A client that connects and sends nothing is dropped after 10 s.
* **Response length.** Large answers (`CLASSES`, `ACTORS`, `WORLDACTORS` all
  exceed 100 KB) are written with a retry loop, but a client must still loop on
  `recv` until EOF: TCP does not preserve message boundaries.
* **Count arguments are clamped.** `CLASSES` and `WORLDACTORS` accept at most
  1,000,000 and treat non-positive values as 0 (`WORLDACTORS` maps a non-positive
  value to its 10000 default first). Non-numeric text parses as 0. Response size
  grows linearly with the count, so `CLASSES 1000000` is a very large reply.
* **One command per connection** applies to legacy connections only. A JSON
  session dispatches every line until `QUIT` or disconnect, and its watches and
  subscriptions end with the connection.
* **Security.** Loopback-only, no authentication. Do not expose the port via
  proxies; any local process can issue `WRITEINT`.
* **Path quoting.** For `USMAP`/`GENSDK`/`JOB`/`LUA LOAD`, wrap paths with
  spaces in double quotes. `GENDIFF` requires quotes on all three paths.

## Capacity and performance

Measured live against a UE4.27 title (74,045 objects, 83 MB image,
14,849 regions):

| Operation | Size / work | Time |
| :--- | :--- | :--- |
| `VERSION` | 7 B | ~1 ms |
| `STATUS` | 688 B | <1 ms |
| `CLASSES` | 2460 classes, 100 KB | ~20 ms |
| `WORLDACTORS` | 4580 actors, 153 KB | ~100 ms |
| `QUERY find limit=20000` | 1.3 MB | ~42 ms |
| `AOB` over 83 MB | 3 hits | ~140 ms |
| `STRINGS` over 83 MB | 2 hits | ~130–210 ms |
| `MODULES` | 28 modules | ~6 ms |
| `CLASSINFO Pawn` | 90 properties, 14 KB | ~13 ms |

Practical limits: a scan reads one region at a time into a buffer and skips
regions larger than 16 GB; searches cap at 10 000 hits; `limit` on `find` caps
at 100 000 records. No command holds a lock while scanning, so a long scan does
not stall the overlay.

## notes

1. **Remote crash from a malformed argument.** `CLASSES -5` reached
   `std::vector<void*>(maxCount)` with a negative count. The constructor threw
   `std::length_error`, nothing caught it, and the server thread's exception
   escaped into `std::terminate` → `abort()` → `__fastfail`. That killed the
   entire game process with `0xC0000409` inside `IOCPServer::ProcessCommand`.
   `ProcessCommand` is now wrapped in a `try`/`catch` that answers
   `ERROR: Internal exception: <what>` and counts are clamped before allocation.
2. **Lost replies on fragmented commands.** A single `recv` returned whatever
   arrived first, so a command split across TCP segments was dispatched
   truncated; the queued remainder then made the close send an RST, and the
   client lost the response (`WSAECONNRESET`, 0 bytes). The server now reads
   until `\n` or EOF.
3. **RST on close.** `shutdown(SD_BOTH)` with unread data queued resets the
   connection. The close path now shuts down only the send direction and drains
   briefly, so clients see a clean EOF.

## See also

* [EXPORTS.md](EXPORTS.md): the same operations as in-process C calls.
* [DiscoveryAPI/REFERENCE.md](../DiscoveryAPI/REFERENCE.md): meaning of every
  pointer and property field.
* [LuaAPI/README.md](../LuaAPI/README.md): richer scripting alternative to IPC.
</content>
