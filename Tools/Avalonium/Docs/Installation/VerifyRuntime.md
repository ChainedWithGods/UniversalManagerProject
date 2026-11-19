# Confirm the Runtime Is Running

Every step after installation depends on the runtime being up: engine discovery
done, the control port open, and the mod loader initialized. Check it once after
installing, and again after a game update.

## The log

Avalonium writes `Avalonium.log` beside `Avalonium.dll`. Universal Manager
profiles keep the log in the profile folder. The proxy also writes
`dwmapi_proxy.log` beside `dwmapi.dll` and tails the main log into it after the
hand-off.

A healthy start contains both lines:

```text
[Avalonium] Initialization successful
[Avalonium] IOCP server started on port 20030
```

- Universal Manager: press Launch Game, let the game reach the main menu, then
  press Inject Avalonium in the Avalonium Runtime Engine tab. The profile sets
  the log path.
- Manual install: launch the game normally. The `dwmapi.dll` proxy loads the
  runtime.

If either line is missing, stop here and fix that first. Every later step reads
from that process. [Troubleshooting](Troubleshooting.md) lists the usual causes.

## Talk to it from outside

The runtime serves newline commands on `127.0.0.1:20030` (`ServerPort` in
`Avalonium.ini`). The scripts in the toolkit's `Helpers` folder speak that
protocol:

```text
cd <Avalonium>\Helpers
python iocp_client.py VERSION
```

`VERSION` answers with the detected engine version, for example `UE5.3`. `INIT`
answers `OK` when discovery is already complete:

```text
python iocp_client.py INIT
```

A refused connection prints:

```text
ERROR: Connection refused. is the DLL injected? Maybe the game is not running?
```

That means nothing is listening: the game is closed, the injection did not
happen, or `ServerPort` changed. `INIT` failing with `ERROR: <reason>` means the
runtime is up but discovery failed, and the log explains why.

The scripts default to port 20030. After changing `ServerPort`, use
`SDKOrchestrator.py --port <port>` for generator work, and edit the `HOST` and
`PORT` constants at the top of `iocp_client.py` for one-off commands.

## C++ mod loader

With `EnableCppMods=true`, which is the default, the loader starts with the
runtime and logs:

```text
[CppMod] C++ mod runtime initialized
```

When `InGameGUI=false` and Lua mods are off, the overlay still starts for native
mods:

```text
[Overlay] Starting overlay bootstrap for native C++ mods
```

Both lines mean the pieces a C++ mod needs are loaded. Mod-specific lines
(`[CppMod] Loaded ...`) appear after the first mod is installed; see
[Build Your First C++ Mod](FirstCppMod.md).

## Next

1. [Generate the game SDK](SDKGeneration.md)
2. [Build your first C++ mod](FirstCppMod.md)
