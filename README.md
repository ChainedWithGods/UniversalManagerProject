# Universal Manager

**A modding platform for Unreal Engine games.** One Windows application for mod management, Nexus Mods, conflict detection, asset browsing, and the Avalonium runtime.

Built with C++, Qt 6, QML, C#, and Python.

For the full feature documentation, see [docs/UniversalManager.md](docs/UniversalManager.md).

## Features

### Mod Management

- Install .zip, .rar, .7z, .pak, .ucas, and .utoc archives, including .pak-x, .ucas-x, and .utoc-x variants.
- Organize mods with tags, descriptions, previews, favorites, collections, and bundles.
- Enable, disable, back up, and check for updates. Updates keep their current install location.
- Review installs before writing, including destination folders and files that will change.

### Game Profiles

- Auto-detect Unreal Engine games and keep a separate profile for each.
- Detect engine versions and support ~mods, mods, or LogicMods folder layouts.
- Launch a game with the selected profile and runtime in place.

### Nexus Mods

- Browse, search, filter, and sort the Nexus Mods catalogue without leaving the app.
- Sign in with OAuth and PKCE, or use a manual API key.
- Download through the manager, handle nxm:// links, and check installed mods for updates.
- Queue downloads with pause, resume, and progress tracking.

### Conflict Detection

- Scan installed mods for file-level overlaps before launch.
- Group colliding mods, list affected files, filter by file type, and search results.
- Copy a summary, export JSON, or open the selected conflicts in the Assets Viewer.

### Assets Viewer (PakViewer)

- Mount .pak and IoStore archives, with mapping files, AES keys, and per-game engine versions.
- Browse assets, preview textures, materials, audio, and code, and inspect static, skeletal, and physics meshes.
- Hardware-accelerated 3D viewer with animation playback, material inspection, screenshots, and a dependency graph.
- Conflict Dashboard for per-mod collision summaries and reports.

### Avalonium Runtime

- In-game menu with live object inspection, editing, function calls, watches, and universal search.
- Developer console with command unlocking and level-transition safety.
- Lua, Blueprint, and native C++ mod support.
- Built-in mods: HUD toggle, performance panel, Blueprint loader, shared events, and Lua helpers.
- Structured logs and optional crash reports.

### Creator Tools

- Generate compilable C++ SDKs and .usmap mapping files.
- Export type data, class relationships, dependency graphs, and console reports.
- Validate SDKs against the running game, generate project files, and compare SDK versions.
- Discover and verify AES archive keys when a game uses them.

### Interface

- Four themes: Glassmorphic, Crimson, Cozy, and Wavey.
- Custom accent, navigation, and interface colors.
- Rebindable shortcuts, Recycle Bin or permanent deletion, and text-field context menus.
- Optimized Mode, Aggressive RAM Cleaner, and live CPU, RAM, and disk monitoring.

## Avalonium API

API documentation and modding guides: [Tools/Avalonium/Docs](Tools/Avalonium/Docs)

- [Lua API](Tools/Avalonium/Docs/LuaAPI/README.md)
- [C++ API](Tools/Avalonium/Docs/CppAPI/README.md)
- [Runtime API](Tools/Avalonium/Docs/RuntimeAPI/README.md)
- [Blueprint mods](Tools/Avalonium/Docs/BPMods/AvaloniumBP.md)

## Requirements

- Windows 10 (1809+) or Windows 11, 64-bit.
- Python 3.x for Avalonium runtime features.
- .NET Desktop Runtime 10.0 for PakViewer and the Advanced Conflict Detector.
- libclang.dll for SDK generation and validation.
- 850 MB storage.
- 2 GB RAM minimum, 4 GB recommended.
- 7-Zip or WinRAR for compressed mod archives.
- Universal Manager supports Unreal Engine 4.0 through 5.8.
- Avalonium supports Unreal Engine 4.23 through 5.8.

## Build Requirements

Building Universal Manager from source requires:

- **Universal Manager**: [Qt Creator (Community)](https://www.qt.io/development/tools) with Qt 6, about 13 GB for the required modules and toolchain.
- **C# applications**: [Visual Studio 2026](https://visualstudio.microsoft.com/) for PakViewer and the Advanced Conflict Detector, including the .NET desktop development workload. Workload size depends on selected components.
- **WinUI C# tooling**: about 9 GB.
- **Windows SDK for Windows 11**: 10.0.28000.2957, about 7 GB.
- **Visual Studio C++ toolchain**: about 12 GB.
- **CMake**: required for configuring the C++ build. [Download CMake](https://cmake.org/download/)
- **Ninja**: required build tool for the C++ build. [Download Ninja](https://github.com/ninja-build/ninja/releases)

Plan for roughly 41 GB or more of free disk space before starting a full build. Sizes depend on the selected workloads.

## Disclaimer

For lawful modding, interoperability research, compatibility testing, and game development only. Do not use for cheating, anti-cheat bypass, DRM circumvention, piracy, or unauthorized access. Provided as is, without warranty. You are responsible for how you use it.
