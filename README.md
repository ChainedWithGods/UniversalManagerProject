
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
<img width="1920" height="1032" alt="Cards" src="https://github.com/user-attachments/assets/03b0f5d7-90f2-4e96-b1c3-eb719e3a6865" />
<img width="1920" height="1032" alt="Dialog" src="https://github.com/user-attachments/assets/732ae0ef-c8ae-4b63-bd82-441f0fa63548" />
<img width="1919" height="1032" alt="Cards" src="https://github.com/user-attachments/assets/d19f3fbe-5cc6-4c02-98c6-5510c3ad79a9" />
<img width="1920" height="1032" alt="Dialog" src="https://github.com/user-attachments/assets/9707f11d-82a7-4020-bcc1-cf2773bbab1e" />
<img width="1920" height="1032" alt="context-menu" src="https://github.com/user-attachments/assets/3f13a0fb-b191-4c7c-853f-96d2502c7a2a" />
<img width="1920" height="1032" alt="list-dialog" src="https://github.com/user-attachments/assets/dad2d2e7-2686-422c-a352-510dab01edf5" />



### Game Profiles

- Auto-detect Unreal Engine games and keep a separate profile for each.
- Detect engine versions and support ~mods, mods, or LogicMods folder layouts.
- Launch a game with the selected profile and runtime in place.
<img width="1920" height="1032" alt="GamesManager" src="https://github.com/user-attachments/assets/05e5ddd2-ed13-4bf4-87ab-06c5d761db55" />
<img width="1919" height="1033" alt="image" src="https://github.com/user-attachments/assets/20f3e7d8-af59-49a0-a854-de7e5ad1ad99" />
<img width="1919" height="1029" alt="image" src="https://github.com/user-attachments/assets/f75aa66e-a5b4-49b2-889a-866ac6493206" />

### Nexus Mods

- Browse, search, filter, and sort the Nexus Mods catalogue without leaving the app.
- Sign in with OAuth and PKCE, or use a manual API key.
- Download through the manager, handle nxm:// links, and check installed mods for updates.
- Queue downloads with pause, resume, and progress tracking.
<img width="1920" height="1032" alt="Crimson" src="https://github.com/user-attachments/assets/5ff30571-1c0a-4edc-8408-3e8ac7aa5588" />
<img width="1920" height="1032" alt="Description" src="https://github.com/user-attachments/assets/2bc855b0-4caa-4ca6-995b-41d0d7252048" />

### Conflict Detection
<img width="1920" height="1032" alt="Conflict-detector" src="https://github.com/user-attachments/assets/58753254-f8dd-42da-9364-f37db66428e3" />

- Scan installed mods for file-level overlaps before launch.
- Group colliding mods, list affected files, filter by file type, and search results.
- Copy a summary, export JSON, or open the selected conflicts in the Assets Viewer.

### Assets Viewer (PakViewer)

- Mount .pak and IoStore archives, with mapping files, AES keys, and per-game engine versions.
- Browse assets, preview textures, materials, audio, and code, and inspect static, skeletal, and physics meshes.
- Hardware-accelerated 3D viewer with animation playback, material inspection, screenshots, and a dependency graph.
- Conflict Dashboard for per-mod collision summaries and reports.
<img width="1919" height="1034" alt="image" src="https://github.com/user-attachments/assets/693c713d-487e-431f-8c06-f396dfdc879c" />
<img width="1919" height="1033" alt="image" src="https://github.com/user-attachments/assets/24dcf91a-3915-404a-9ad1-8b92ff3f0f10" />
<img width="1919" height="1032" alt="image" src="https://github.com/user-attachments/assets/bb31e1d7-82b6-4050-b343-163cf585aa1c" />
<img width="1919" height="1033" alt="image" src="https://github.com/user-attachments/assets/4e5f361a-e89f-46cb-8715-8124cfaa7b43" />
<img width="1919" height="1034" alt="image" src="https://github.com/user-attachments/assets/e982dd0f-a67b-4300-9858-3bc70eaf3452" />

### Avalonium Runtime

- In-game menu with live object inspection, editing, function calls, watches, and universal search.
- Developer console with command unlocking and level-transition safety.
- Lua, Blueprint, and native C++ mod support.
- Built-in mods: HUD toggle, performance panel, Blueprint loader, shared events, and Lua helpers.
- Structured logs and optional crash reports.
<img width="1920" height="1032" alt="Avalonium Runtime engine" src="https://github.com/user-attachments/assets/a85dcbf0-6a03-4b11-9aca-2790d444f450" />
<img width="1920" height="1032" alt="Avalonium-dialog" src="https://github.com/user-attachments/assets/84faaff9-25f5-4778-9c87-f3d022920c08" />
<img width="1920" height="1032" alt="Lua" src="https://github.com/user-attachments/assets/b051a9a1-a44e-492c-8df7-3450ad4dd4e3" />

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
<img width="1919" height="1035" alt="image" src="https://github.com/user-attachments/assets/b0ec006d-32c7-4291-8930-898f5757dcc2" />
<img width="1919" height="1035" alt="image" src="https://github.com/user-attachments/assets/7067b07d-43d3-4736-b92d-4a84b4d70caf" />



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



