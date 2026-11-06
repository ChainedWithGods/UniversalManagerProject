# AggressiveDog Watchdog

AggressiveDog is an external Windows process watchdog for Avalonium. Avalonium starts it when the DLL loads if `[AggressiveDog] Enabled=1`. By default it runs as a **passive monitor**: it opens the process, samples thread state, writes an exit report when the process dies with a nonzero code, and appends Avalonium's log tail. It does not attach as a debugger.

Setting `AttachDebugger=1` switches it to **debugger mode**, in which it can capture first-chance and second-chance exceptions, call stacks, register state, and minidumps. Debugger mode is opt-in because protected games (anti-cheat, DRM, Steam emulators) can treat an attached debugger and hardware breakpoints as tampering and terminate the process themselves.

The watchdog records evidence for diagnosis. It does not prevent a crash, repair the game, or guarantee that a report identifies the root cause.

## Build and file locations

On Windows with MSVC, CMake defines an `AggressiveDog` executable target. Its output is:

```text
Runtime/DLL/CrashHandler/AggressiveDog.exe
```

CMake keeps this path consistent across single- and multi-configuration generators. The launcher source, `Launcher.cpp`, is compiled into the `Avalonium` DLL target; it is not a separate library target. Building `Avalonium` also depends on the watchdog executable.

In the IDE's CMake target list, build `AggressiveDog` to build only the watchdog. Build `Avalonium` to build the DLL and its watchdog dependency together.

At runtime, the launcher looks for `CrashHandler/AggressiveDog.exe` beside `Avalonium.dll`. It does not fall back to an executable beside the DLL or in another folder. If the watchdog is missing or cannot start, the launcher writes details to:

```text
<folder containing Avalonium.dll>/AggressiveDog-launch.log
```

The launcher's symbol file path is `CrashHandler/Avalonium.pdb`, beside the watchdog executable. CMake deploys the PDB there. The watchdog uses it to symbolize frames in `Avalonium.dll`; reports still contain addresses and other evidence if the PDB is unavailable.

## Runtime flow

1. Avalonium reads the `[AggressiveDog]` section in `Avalonium.ini`.
2. If enabled, the launcher starts `CrashHandler/AggressiveDog.exe` with the target process ID, thread ID, PDB path, report directory, INI path, and runtime log path.
3. With `AttachDebugger=0` (default) the watchdog opens the process for query/read access, samples every thread's instruction pointer and the module list every 500 ms, and waits for the process to exit. It never creates a debug object or touches debug registers.
4. With `AttachDebugger=1` the watchdog attaches with `DebugActiveProcess` and waits for Windows debug events. The sampling thread keeps running, and the debug loop keeps a bounded event tail of exceptions, thread create/exit, module load/unload, and debug strings. When `TerminationBreakpoint=1` it also arms a hardware breakpoint on `NtTerminateProcess` on every thread.
5. In debugger mode, a fatal exception pauses other target threads while the watchdog writes the text report and, if enabled, a minidump. It then passes the unhandled exception back to Windows so normal process termination can continue.
6. A successful `NtTerminateProcess` request with exit status `0` is treated as normal shutdown. A nonzero or unavailable termination status is captured as abnormal or unknown termination evidence.
7. If the process exits with a nonzero code, the exit report includes the captured evidence from whichever mode is active.

Because debugger mode attaches as a debugger, another debugger may prevent it from attaching, and a protected game may terminate when it detects the debugger. Capturing stacks and writing a minidump also pauses the target while the report is collected. The sampling thread reads thread contexts through `OpenThread`/`GetThreadContext`, so protected threads may report `context unavailable`.

## Configuration

These settings live under `[AggressiveDog]` in `Runtime/DLL/Avalonium.ini`. The values from the current INI file are shown below; the fallback values apply when a key is missing.

| Key | Current INI value | Fallback | Description |
| --- | --- | --- | --- |
| `Enabled` | `1` | `false` | Starts the external watchdog when Avalonium loads. |
| `LogDirectory` | `CrashHandler/Crashes` | `CrashHandler/Crashes` | Destination for text reports, minidumps, and `AggressiveDog-status.log`. Relative paths resolve beside `Avalonium.dll`. |
| `DumpRegisters` | `0` | `true` | Prints the faulting thread's CPU registers in the text report. Context capture is still used for stack walking when register printing is disabled. |
| `CatchFastFail` | `1` | `true` | Enables early capture of Windows fast-fail exceptions. An exception that becomes unhandled can still be captured on its second chance. |
| `TerminationBreakpoint` | `1` | `true` | Arms a hardware breakpoint on `NtTerminateProcess` on every thread so abnormal terminations are attributed to an in-process call. Only used in debugger mode. Disable it when a protected game treats debug registers as tampering; exit reports then state that no termination breakpoint was armed. |
| `AttachDebugger` | `0` | `false` | Attaches the watchdog as a debugger, enabling exception-level crash reports, call stacks, and minidumps. Leave off for protected games; the passive monitor still writes exit reports with thread snapshots and the runtime log tail. |
| `MaxStackDepth` | `256` | `64` | Maximum frames for the faulting thread's stack. Values are clamped to `1–256`. |
| `OtherThreadStackDepth` | `8` | `8` | Maximum frames for each other thread. `0` disables other-thread stack walks; values are clamped to `0–64`. |
| `WriteMiniDump` | `0` | `true` | Writes a `.dmp` file alongside each crash text report. Dump size depends on the target process and captured memory. |

Boolean settings accept `true`/`false`, `yes`/`no`, `on`/`off`, and `1`/`0` (case-insensitive).

`RuntimeConfig.cpp` preserves existing watchdog values when it regenerates the INI and writes comments describing the settings.

## Output files

### Crash report

Fatal exception reports are produced in debugger mode (`AttachDebugger=1`). They use names like:

```text
AggressiveDog_YYYY-MM-DD_HH-MM-SS_mmm_pid<PID>_tid<TID>.txt
```

When `WriteMiniDump=1`, a matching `.dmp` is written next to the `.txt`. The watchdog does not automatically delete or rotate old reports and dumps. Minidumps can be large and may contain sensitive data from the game's memory.

The text report is organized into these sections:

- **Quick summary** — process, exception, fault address, thread, debugger chance, and symbol status.
- **Watchdog settings** — effective settings and configured paths at capture time.
- **Recent runtime log** — up to the last 120 lines from Avalonium's log. `[!]` marks lines matching generic error indicators; it is only a keyword filter.
- **Artifacts** — report and minidump paths and write status.
- **Process and exception details** — image, process metadata, exception parameters, and decoded exception information when available.
- **Faulting module** — module containing the exception address, its base, and the address offset.
- **Thread context and call stack** — register values when enabled, symbolized stack frames when symbols are available, and unwind diagnostics.
- **Memory windows** — bytes around the exception instruction and the faulting thread's stack pointer, when readable.
- **Watchdog event tail** — the most recent debug events before the crash: exceptions with module offsets, thread create/exit, module load/unload, and debug strings.
- **Automated triage** — recognized exception facts and correlation guidance. It does not claim a root cause from the exception code alone.
- **Other thread snapshots** — thread state and optional short stacks for other process threads.
- **Loaded modules** — alphabetically sorted modules, with the faulting module marked by `*`.

Reports may have incomplete stacks or memory windows if Windows cannot read the target memory, unwind metadata is unavailable, or thread context capture fails. Check the report's error notes before interpreting missing data.

### Process exit report

If the process exits with a nonzero code and no crash report was already written, the watchdog writes an `AggressiveDog_exit_...txt` report. A normal exit with code `0` does not create a crash or exit report.

The exit report records the exit code and explains that no fatal exception event was delivered. It also appends the watchdog's own evidence, because a fatal exit status can be produced by `TerminateProcess` (including a direct system call) or by an external kill rather than by an exception:

- **Termination evidence** — how many threads carried the `NtTerminateProcess` breakpoint, how many hits occurred, and the last in-process termination status. "No in-process NtTerminateProcess call was observed" means the exit did not travel through ntdll's termination entry.
- **Exception counters** — first-chance and second-chance exception totals delivered to the watchdog.
- **Event tail** — recent exceptions, thread and module events, and debug strings.
- **Last thread snapshot** — each thread's instruction pointer resolved to `module + offset`, captured up to about 500 ms before the exit.
- **Modules at last snapshot** — non-Windows modules with base addresses and paths, useful for spotting protection or anti-cheat components.
- **Recent runtime log** — Avalonium's log tail, showing the last actions of the injected runtime before the exit.

A report that shows no exception event, no termination-breakpoint hit, and thread instruction pointers concentrated in a non-Windows module indicates termination from outside the process or through a direct system call; it does not identify the caller with certainty.

### Status and launcher logs

- `LogDirectory/AggressiveDog-status.log` records watchdog startup, debugger attachment, report writes, failures, and target exit.
- `<folder containing Avalonium.dll>/AggressiveDog-launch.log` records whether the watchdog was enabled, the paths used, and whether `CreateProcess` succeeded.

These logs can still exist after a normal game exit. A normal exit should not produce crash text reports or minidumps.

## Troubleshooting

### No watchdog starts

Check that `Enabled=1`, then confirm `CrashHandler/AggressiveDog.exe` exists beside `Avalonium.dll`. Read `AggressiveDog-launch.log` for missing-file, path-length, and process-start errors.

### The watchdog starts but cannot attach

Read `AggressiveDog-status.log` for the `OpenProcess` or `DebugActiveProcess` error. Check that the watchdog and game run with compatible permissions and that another debugger is not already attached to the game.

### The game terminates when the watchdog is enabled

Some protected games (anti-cheat, DRM, Steam emulators) terminate themselves when they detect a debugger, hardware breakpoints, or foreign DLL activity. Keep `AttachDebugger=0` so the watchdog stays a passive monitor, and leave `TerminationBreakpoint=0`. If the game still dies with `Enabled=1`, set `Enabled=0` and compare exit behavior; the passive monitor writes an exit report only when the process exits nonzero.

### Reports contain raw addresses instead of Avalonium function names

Make sure `CrashHandler/Avalonium.pdb` matches the exact `Avalonium.dll` build loaded by the game. The report's `Symbol details` field explains whether the PDB was found and loaded.

### A normal close creates crash artifacts

A successful termination with status `0` should be logged as a suppressed crash report. If crash reports or dumps are still created, inspect the report's exception code, exit status, and call stack, and check that the game loaded the newly built watchdog executable from `CrashHandler`.

### The report directory grows too large

Set `WriteMiniDump=0` to keep text reports without minidumps. Existing files are retained; AggressiveDog does not clean up historical output automatically. Review old dumps before deleting them because they may be needed for diagnosis.
