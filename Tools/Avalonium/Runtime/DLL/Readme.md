# Avalonium Proxy Injection Guide (`dwmapi.dll`)

A guide for injecting Avalonium runtime into Unreal Engine games via proxy DLL loading using `dwmapi.dll`.

---

## Overview

**Proxy DLL Injection** allows Avalonium to load automatically alongside the target game executable without needing an external injector program or elevated administrator hooks.

When Windows launches a game executable located in `Binaries\Win64`, it searches for dynamic link libraries in the application directory before looking in system folders. By placing a custom `dwmapi.dll` next to the game executable, the game automatically loads Avalonium runtime upon startup.

---

## Required Files

Copy the following files and folders from:
```text
<Manager Root>\Tools\Avalonium\Runtime\DLL\
```

| File / Folder | Purpose | Required |
| :--- | :--- | :---: |
| `dwmapi.dll` | Proxy DLL loaded automatically by Windows upon game launch | **Yes** |
| `Avalonium.dll` | Core Avalonium runtime & engine hook payload | **Yes** |
| `Avalonium.ini` | Runtime configuration file (toggles, ports, mod paths) | **Yes** |
| `CrashHandler\AggressiveDog.exe` | External crash watchdog, launched when `[AggressiveDog] Enabled=1` | If watchdog enabled |
| `CrashHandler\Avalonium.pdb` | Avalonium debug symbols used by the watchdog for function names and source lines | Recommended |
| `Mods/` | Stock Lua mods and native C++ mods (`Mods/Avalonium/`) | If mods enabled |

> [!NOTE]
> **Do NOT copy** the `Universal Manager Lua Mods` folder. That directory is only used internally by Universal Manager for managing multiple game profiles.

---

## Installation Steps

### Step 1: Locate Game Shipping Executable Directory

Find your game's 64-bit binary directory. In Unreal Engine games, this is typically located at:
```text
<Game Root>\<GameName>\Binaries\Win64\
```
*(Example: `D:\Games\MyGame\ProjectName\Binaries\Win64\`)*

> [!IMPORTANT]
> Make sure to place the files next to the actual game engine executable (usually named `<GameName>-Win64-Shipping.exe`), **not** the launcher executable in the root directory.

### Step 2: Check `Avalonium.ini` (`LuaModsPath` and `CppModsPath`)

Before copying, open `Avalonium.ini` and ensure `LuaModsPath` and `CppModsPath` are completely empty:
```ini
LuaModsPath=
CppModsPath=
```
> [!WARNING]
> If either path points to an absolute path from your Universal Manager directory (such as a profile folder), Avalonium will attempt to load mods from that location instead of the game's local folder. Leaving both empty guarantees Avalonium loads from the game's local `Mods\Avalonium` folder.

---

### Step 3: Copy Files

Copy the required files into the `Binaries\Win64\` directory so the directory structure looks like this:

```text
Binaries\Win64\
 ├── <GameName>-Win64-Shipping.exe   <-- Target game executable
 ├── dwmapi.dll                      <-- Avalonium Proxy DLL
 ├── Avalonium.dll                   <-- Avalonium Runtime
 ├── Avalonium.ini                   <-- Configuration Settings
 ├── CrashHandler\                   <-- Watchdog executable (if enabled)
 │    ├── AggressiveDog.exe
 │    └── Avalonium.pdb               <-- Watchdog Symbols
 └── Mods\                           <-- Lua mods and native C++ mods
      └── Avalonium\
           ├── GenericFunctions\
           ├── BPModLoader\
           ├── shared\
           ├── UniversalHUDToggle\
           ├── UniversalOverlay\
           ├── MyCppMod\              <-- native mod folder
           │    └── dlls\
           │         └── main.dll     <-- exports AvaloniumModAbiVersion, start_mod, uninstall_mod
           └── mods.json
```

---
> [!TIP]
> - If `LuaModsPath` and `CppModsPath` are left empty, Avalonium loads mods from the default `Mods\Avalonium` subfolder.
> - Native C++ mods are loaded from `<mods>\<ModName>\dlls\main.dll` or `<mods>\<ModName>\dlls\<ModName>.dll`. The DLL exports `AvaloniumModAbiVersion`, `start_mod` and `uninstall_mod`. An optional `enabled.txt` disables a mod with `0`, `false`, `no`, `off`, or `disabled`.
> - See [CppAPI](../../Docs/CppAPI/README.md) for the native mod API, lifecycle and build instructions.
> - **Initialization Delay**: It is highly recommended to configure a startup delay (e.g., `InitDelaySeconds=10` or `InitDelayMinutes=0` in `Avalonium.ini`). This ensures the game engine is fully initialized and loaded into memory before Avalonium attempts to hook, preventing premature injection failures or crashes.

---

### Step 4: Launch the Game

Launch the game normally (via Steam, Epic Games Launcher, or directly through the game executable).

1. The game will automatically load `dwmapi.dll`.
2. Avalonium hooks will initialize.

If the watchdog is enabled in `Avalonium.ini`, Avalonium also starts `CrashHandler\AggressiveDog.exe` to monitor the game process.

---

## Crash Watchdog (AggressiveDog)

AggressiveDog is an external Windows process monitor for Avalonium. It attaches to the game process so it can capture diagnostic details when a fatal exception occurs. It records evidence for diagnosis; it does not prevent or fix crashes. The game may pause briefly while the watchdog captures a report.

The launcher looks for the executable at exactly:

```text
Binaries\Win64\CrashHandler\AggressiveDog.exe
```

Keep the `CrashHandler` folder beside `Avalonium.dll`. The watchdog writes text reports and optional minidumps to the directory configured by `[AggressiveDog] LogDirectory`. A normal exit with status `0` should not create a crash report (Hopefully XD). Old reports and dumps are retained and are not automatically deleted.

### Configure the watchdog

In `Avalonium.ini`, set:

```ini
[AggressiveDog]
Enabled=1 (0 false, 1 true)
LogDirectory=CrashHandler\Crashes
DumpRegisters=1 (0 false, 1 true)
CatchFastFail=1 (0 false, 1 true)
TerminationBreakpoint=0 (0 false, 1 true)
AttachDebugger=0 (0 false, 1 true)
MaxStackDepth=64
OtherThreadStackDepth=8
WriteMiniDump=0 (0 false, 1 true)
```

`Enabled=1` starts the watchdog. Relative `LogDirectory` paths resolve beside `Avalonium.dll`, so `CrashHandler\Crashes` writes under `Binaries\Win64\CrashHandler\Crashes`. This is also the default when `LogDirectory` is missing. Use a writable location on the target machine; replace any absolute path before copying the INI to another computer. Minidumps can be large and may contain data from game memory, so enable them only when useful for diagnosis.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `Enabled` | `false` | Starts the watchdog process. |
| `LogDirectory` | *(empty)* | Folder for watchdog text reports, status log, and optional minidumps. Relative paths resolve beside `Avalonia.dll`. |
| `DumpRegisters` | `false` | Include the faulting thread's CPU registers in the text report. |
| `CatchFastFail` | `false` | Capture Windows fast-fail events in addition to standard fatal exceptions. |
| `TerminationBreakpoint` | `false` | Arm a hardware breakpoint on `NtTerminateProcess` so abnormal terminations are caught. Disable when a protected game treats debug registers as tampering. |
| `AttachDebugger` | `false` | Attach as a debugger for exception-level crash reports. Off by default because protected games can treat an attached debugger as tampering and terminate. |
| `MaxStackDepth` | `255` | Maximum call-stack frames for the faulting thread (1-256). |
| `OtherThreadStackDepth` | `8` | Maximum stack frames per other thread (0-64; 0 disables them). |
| `WriteMiniDump` | `false` | Write a minidump alongside the text report (can use substantial disk space). |

`TerminationBreakpoint` and `AttachDebugger` are both off by default because anti-cheat/denovo protected games can treat debug registers or an attached debugger as tampering and terminate the process. Leave them off unless you are diagnosing a crash on an unprotected title.

The launcher writes startup and path errors to `AggressiveDog-launch.log` beside `Avalonium.dll`. The watchdog writes its lifecycle and capture results to `AggressiveDog-status.log` inside `LogDirectory`.

Crash text reports include the exception summary and parameters, process and thread details, symbolized call stack when `CrashHandler\Avalonium.pdb` matches the installed DLL, memory windows, other-thread snapshots, a sorted module list, and recent Avalonium log lines. The automated triage section reports recognized exception facts; it does not prove a root cause. When `WriteMiniDump=1`, a `.dmp` accompanies the text report.

---

## Uninstallation / Disabling

To remove or temporarily disable Avalonium injection:

- **Disable Injection**: Rename or delete `dwmapi.dll` from the `Binaries\Win64\` folder.
- **Full Removal**: Delete `dwmapi.dll`, `Avalonium.dll`, `Avalonium.ini`, `CrashHandler`, and `Mods` from `Binaries\Win64\`.
