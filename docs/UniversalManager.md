<div align="center">

# **Universal Manager**
### Unreal Engine modding without the tool pile
#### **Free & Open Source**

</div>

---

## **Overview**

**Introducing Universal Manager**: one Windows app for Unreal Engine modding. It brings the mod library, Nexus downloads, conflict checks, an asset viewer, and the Avalonium runtime into one window, with shared profiles and mod folders.

**Includes:**
- **Mod Library**: install, organize, enable, disable, bundle, and back up mods.
- **Game Profiles**: separate mod setups for each detected Unreal Engine game.
- **Nexus Mods**: browse, sign in, download, and check for updates without leaving the app.
- **Conflict Detector**: find file-level overlaps before launching the game.
- **Assets Viewer (PakViewer)**: FModel-style asset browser and 3D preview.
- **Avalonium Runtime**: in-game menu, developer console, live editor, and Lua, Blueprint, and native C++ mods.
- **Game Launcher and Creator Tools**: start the game with the right setup, generate SDKs and mapping files, and export archive reports.
- **Interface and engine support**: four themes, customizable colors, and Unreal Engine 4.0 through 5.8.

---

## **What Universal Manager Is**

Universal Manager is a modding platform for Unreal Engine games that replaces several separate tools with one shared workspace for mods, downloads, conflicts, assets, and the live runtime.

---

## **Quick Tour**

The app is organized around four areas: Home, Mods Library, Advanced, and Nexus Mods. Most work happens in Mods Library and Nexus Mods. Home is for a quick status check, and Advanced holds settings and the deeper tools. Nexus Mods appears after the integration is enabled and an account is connected.

---

## **Home**

Home is the dashboard. It shows installed, active, and inactive mod counts, storage usage, and quick actions.

**Ready to Play?** shows the current profile and starts the game with the selected mods and runtime in place. **Install Local Mod** opens the install flow. **Quick Conflict Scan** checks the current setup and reports assets that more than one mod changes, with a short list of suggested mods to disable. The full conflict detector has the detailed breakdown.

---

## **Mods Library**

The central hub for managing your mod collection.

### **Browsing and organizing**

Mods can be viewed as cards, a sortable list, collections, or bundles. Cards show a preview, name, file count, type, favorite state, status, and update flag. The list supports sortable columns and multi-select for batch operations.

The search bar filters by name, tag, or ID. Filters cover Active, Disabled, Favorites, Folder Mods, and Loose Files, with sorting by name, date, size, or status.

Opening a mod shows its preview, size, install date, file path, tags, and description. From there you can rename it, add tags, change the preview, open its folder, view it in the Assets Viewer, export its structure, enable or disable it, or delete it. Right-click actions cover the same operations, plus batch enable, disable, and delete for multiple selected mods.

### **Collections and bundles**

Collections are named groups of mods that can have preview images, be renamed or deleted, and be enabled or disabled as a unit.

A bundle groups mods that arrived in one archive. It can be created during install with an optional name. Toggling a bundle enables or disables every member. Existing mods update in their current folder, while new mods go under the bundle name. Bundles also show when a member has a newer Nexus file.

### **Installing mods**

**Install Mods** accepts **.zip, .rar, .7z, .pak, .ucas, .utoc, .pak-x, .ucas-x, .utoc-x**, and supports drag and drop for multiple archives.

Before anything is written, the install can be reviewed: display name, destination, and the files that will be updated. Archives with more than one mods folder ask where each group should go. Lua mods install disabled by default and can be enabled during install. Installs can be grouped into a bundle.

### **Backups, switching, and updates**

**Backup Mods** backs up folder mods or loose files to a chosen destination, with select-all and search.

**Switch Mods Folder** repoints the current profile to another mods folder. Before switching, individual mods or collections can be disabled, or everything can be left as it is.

**Check updates** compares installed mods and downloaded archives against Nexus, reports available updates, and flags affected mods until they are updated. Updated mods keep their current install location.

---

## **Nexus Mods**

Universal Manager talks to **Nexus Mods** directly: you search, browse, download, and update mods without leaving the app. The page becomes available once the integration is enabled under **Advanced → General → Nexus Mods**.

### **Connecting Your Account**

Two ways to authenticate:

- **Sign In with Nexus Mods**: The recommended path. A secure in-app window handles the **OAuth 2.0 with PKCE** sign-in, so your Nexus password never reaches Universal Manager. Session tokens are stored encrypted with **Windows DPAPI**. If the sign-in does not finish, the app offers **Try Again**.
- **Manual API Key**: The legacy path. Paste a personal API key into **API ACCESS**; it is stored locally.

The section also shows **Hourly API Limit** and **Daily API Limit** usage and can register Universal Manager as the handler for **nxm://** links.

Without a connection you can still open the page, but the mod list and filter options stay empty: *"Sign in or add an API key to load the filter options."*

### **Browsing**

Browsing covers the usual Nexus options: game database, title search, sorting by newest, downloads, endorsements, or recent updates, and filters for search fields, category, tags, language, and time range. Adult-flagged mods are marked, animated previews and media slideshows are supported, and results load page by page.

### **Mod Page**

Opening a mod creates a separate details window. It shows the description, author, uploader, virus scan status, files, requirements, and the mods that use it. Files list versions, sizes, and upload dates, with manual and manager download routes. Requirements split into Nexus, DLC, and off-site. Attached videos play in their own window, with a YouTube fallback when decoding fails.

### **Downloading**

Universal Manager checks mod file, DLC, and off-site requirements before a download. The file is then handled by the download manager.

**nxm:// links** work the same way. Once **Download Links** is associated, a link from the Nexus website is handed to Universal Manager and queued. Free accounts use a short-lived capture window instead of a direct API link. Completed archives land in the downloads library and can be installed immediately.

### **The Download Manager**

Lives in **Advanced → General → Downloads**. It separates all, finished, unfinished, mod updates, and downloaded mods, plus archives and loose files. Each download shows speed and time remaining and can be paused, resumed, cancelled, opened in its folder, or removed. The destination is configurable, and a sound plays when a download completes.

---

## **Advanced / Settings**

Settings are organized into **four tabs**: **General**, **Games Manager**, **Advanced Conflicts Detector**, and **Avalonium Runtime Engine**.

### **General**

General holds the app-wide settings, grouped into these categories:

- **Appearance**: four themes (Glassmorphic, Crimson, Cozy, Wavey), glass and wallpaper controls, surface darkness, accent colors, navigation colors, and per-theme card colors.
- **Paths**: game executable, mods directory, downloads folder, backup location, game paks folder, usmap file, and the Universal Manager data folder.
- **Tools**: extraction tool (7-Zip or WinRAR), Assets Viewer path and per-game configuration, conflict detector, Unreal Engine version detector, and auto scan for folder detection.
- **Shortcuts and deletion**: rebindable shortcuts for general actions and mod actions, plus Recycle Bin or permanent deletion.
- **Behavior**: debug console, tray minimizing, close-to-tray, auto-hide navigation bar, notifications, rename prompt on install, categorized asset view, AES key, and manual engine version selection for custom games.
- **Performance**: optimized mode, pause and animation behavior on focus loss, aggressive RAM cleaner with a threshold, and live RAM, CPU, and disk readouts.
- **Downloads and Nexus**: download destination, account connection, API limits, and nxm:// link association.
- **Guides and About**: in-app tutorial videos and the third-party license list.

The guides cover profile setup, the categorize system, injecting Avalonium, advanced injection, loading Avalonium through dwmapi.dll, generating usmap and SDK, and installing Blueprint and Lua mods.

### **Games Manager**

Games Manager keeps every game's mods separate. **Game Scan** scans selected drives for Unreal Engine games, and protected locations offer a restart as administrator when needed.

Each detected game can open its folder, open or create its mods folder, delete the mods folder, or copy its name or path. The engine version detector shows the detected Unreal Engine version, and each profile can use **~mods**, **mods**, or **LogicMods** as its mods folder type.

Profiles can be created, renamed, removed, given preview images, and marked active. Each profile keeps its own mod list, tags, collections, bundles, and settings. The page also shows total profiles, total mods, active mods.

## **Advanced Conflicts Detector**

The conflict detector scans installed mods and finds files changed by more than one mod. It does not choose a winner; it shows which mods collide, which files are involved, and how the overlaps group. The scan can include disabled mods, reports progress while it reads packages, and marks results out of date when the setup changes.

Results list affected mods with collision counts, clashing groups, and the individual files with their paths and rival mods. File type filters and search make large results easier to work through. Mods can be enabled or disabled from the results, and the counts update immediately. Copy a summary, export the dataset as JSON, or open the selected conflicts in the Assets Viewer. A clean result says every installed mod modifies separate files.

Quick Conflict Scan on the Home page covers the common cases. This tab is the full picture.

---

## **PakViewer (Assets Viewer)**

PakViewer is a bundled FModel-style asset browser and 3D viewer. Open it from a mod or conflict, or launch it from Advanced → General → Advanced Tools. When opened from a conflict, it starts in the Conflict Dashboard with that mod's data loaded.

PakViewer mounts .pak and IoStore archives, with optional mapping files, AES keys, per-game engine versions, and mod-only or full-game loading. I'm sure you're familiar with FModel, so I won't over-explain the browser, previews, 3D viewer, and material tools.

### **Conflict Dashboard**

When opened from a conflict, the viewer shows a read-only dashboard with the mod's collision total, affected mesh and texture counts, the primary rival mod, a ranked rival list, category filters, and a searchable collision list. Inspect 3D opens the affected models, and the report can be exported as plain text. Zero collisions still shows the header and replaces the stats with "No mod collisions detected".

### **Launch Modes & Integration**

Open PakViewer from a mod or conflict, launch it from **Advanced → General → Advanced Tools**, or run the bundled executable directly.

---

## **Avalonium Runtime Engine**

**Avalonium** is the runtime engine behind every **live-game feature** of Universal Manager. It reads the running game directly in memory, with no hardcoded offsets and no per-game setup, and Universal Manager installs and starts it with the game for you, so there is nothing to wire up by hand.

Avalonium works across **Unreal Engine 4.23 through 5.8**.

### **What Avalonium Adds**

- An in-game menu for browsing, searching and editing the live game.
- A developer console that works in builds where the game normally leaves it unused.
- Lua, Blueprint and native C++ mod support.
- On-screen overlays, HUD controls and a live performance panel.
- Creator tools for SDKs, mapping files, game type data and archive reports.
- Structured logs and optional crash reports.

### **Works Across Supported Unreal Games**

- Unreal Engine 4.23 through 5.8.
- No per-game configuration package and no setup file to hand-edit for each game.
- Learns the running game's structure when it loads and reuses safe findings on later starts.
- Works with large or protected games by using multiple discovery paths instead of one fixed method.
- Reaches Blueprint and script-based game functions in newer builds, including cases where the game hides them from a simple scan.
- Refuses unsupported engine versions and keeps a feature off instead of guessing when it cannot confirm something.

### **In-Game Menu**

The menu opens with **Insert** or **F5**, or with a mod-defined key.

- **Globals**: shows the detected game setup, renderer, live object count and copyable support details.
- **Explorer and Inspector**: browse the current level, actors or objects and inspect names, classes, ownership, inheritance and properties.
- **Live Editor**: change properties, call functions, keep watches, browse components and move through edited targets with history.
- **Universal Search**: find objects, functions, properties, values or addresses across levels, actors, objects, class defaults or a selected hierarchy.
- The overlay also shows mod toasts, stats panels, simple drawings and custom mod windows.

### **Developer Console**

- Enables the game's developer console in builds that normally leave it unused.
- Makes cheat-style console commands available when a shipping build hides them.
- Sets console keys up safely in cooked builds.
- Keeps the console usable after level changes.
- Protects level transitions when a console command temporarily detaches the player's controller.
- Can write optional reports for declared commands, spawnable classes, console variables, help text and loaded modules.

### **Mods**

Avalonium supports three mod types in the same mod folder.

**Lua mods**
- Written as plain script files, with no compiler needed.
- Load, unload and reload without restarting the game.
- Each mod gets its own environment so scripts do not overwrite each other's settings.
- Mods add keybinds, timers, console commands, on-screen text, HUD panels, property watchers, actor changes and custom windows.
- Mods read camera and screen positions, draw simple HUDs and project world points to the screen.
- Mods change game speed for effects such as slow motion or freeze.
- Mods react to game functions before or after they run, change arguments or skip them.
- Mods build game UI panels at runtime from the game's own widget classes.
- Mods pass game text and property values correctly through the game's own handling.
- Mods generate C++ SDK and mapping files for creator use.

**Blueprint mods**
- Blueprint mods use packaged game files and a mod actor.
- Mods load from the game's LogicMods folder.
- Per-mod config and load order are supported.
- Blueprint mods respawn after level changes.
- **Alt+PageUp** reloads Blueprint mods.

**Native C++ mods**
- A native mod is a compiled Windows 64-bit DLL built for Avalonium.
- The same mod build works across the supported engine range.
- Native mods read and change game properties, call game functions, inspect live objects, hook functions and react to game frames.
- Native mods build in-game UI panels and draw custom windows inside the overlay.
- A native mod folder can also include a Lua script, with the native side loading first.
- Mods built for another runtime are rejected until they are rebuilt for Avalonium.

**Built-in mods**: optional, enabled from the mod list:
- **HUD Toggle** hides and restores the game HUD, including normal game UI, HUD overlays and attached widget components. Default keys: **F7** toggle and **F9** refresh.
- **Performance Panel** shows FPS, frame time, memory, CPU usage, GPU usage, GPU temperature and live object count. Default key: **F6**.
- **Blueprint Mod Loader** loads Blueprint `.pak` mods from LogicMods.
- **Generic Functions** provides shared events for Blueprint mods.
- **UE Helpers** provides common game and world helpers for Lua mods.

### **Crypto & Asset Discovery**

Avalonium finds and verifies AES encryption keys automatically.

**Core discovery engine**
- **Dynamic in-process AES-256 key discovery**: no hardcoded keys and no external tools; the key is found and verified inside the running game process.
- **Multi-source memory harvest**: scans instruction sequences, raw high-entropy buffers and hex/base64 strings across the main executable and loaded game modules.
- **SHA-1 + structural ground truth**: pak candidates are validated against the archive's own index hash; IoStore-only games are validated by structurally parsing the directory index.
- **Cross-DLL key probing**: sweeps loaded game and plugin DLLs inside the game root when the key is not in the main executable.
- **Config-gated startup scan**: runs once during initialization when the option is on, and can be called again on demand.

**Archive format support**
- **Universal UE4 / UE5 pak index deserializer**: resolves mount point, entry count, path hash seed and index layout flags.
- **IoStore UTOC container flag parsing**: decodes compressed, encrypted, signed, indexed and on-demand containers across UTOC v2 (UE 4.26) through v6 (UE 5.4+).
- **Custom pak version handling**: non-standard forks and obfuscated headers are parsed where the footer still describes a valid index.
- **Complete container inventory**: enumerates every `.pak`, `.utoc` and `.ucas` in the game's content directory and writes per-container details: footer/TOC versions, index hashes, mount points, directory trees, extension histograms and decoded entry samples.

**Performance**
- **Preview-gated verification**: a single small preview read rejects wrong candidates before any full index decryption.
- **Unencrypted short-circuit**: skips the key search when archives are confirmed unencrypted and still writes the container census.

**Export & output**
- **Structured JSON export**: `<Executable>_AESKey.json` beside the game binary: key formats (hex, spaced hex, base64, C-array, UnrealPak argument), detection source and address, executable and engine metadata, verification counts, census totals, per-container pak/UTOC/ucas reports and a format reference.
- Unencrypted games and failed key searches still produce a census report; only the no-container case skips the JSON write.

**Verified results**
- *UE 4.23 title*: key found in **230 ms** (12 candidates), 26,135 entries decoded
- *UE 4.27 title*: key found in **69 ms** (42 candidates), 37,280 entries decoded
- *UE 4.27 title*: key found in **23 ms** (308 candidates), 35,104 entries decoded
- *UE 5.5 title*: key found in **500 ms** (1,944 candidates), 260,317 entries decoded
- *UE 5.3 title (Pak v101)*: key found in **288 ms** (1,590 candidates), 48,166 entries decoded
- *UE 4.27 title*: key found in **749 ms** (38 candidates), 231,061 entries decoded
- *UE 5.3 title*: key found in **885 ms** (1,527 candidates), 4,868 entries decoded
- *UE 5.6 title*: key found in **2040 ms** (1,757 candidates), 6,021 entries decoded
- *UE 4.26 (UTOC v2), UE 5.4 (UTOC v6) and UE 5.2 (UTOC v5) titles*: detected as unencrypted, key search skipped (**0 ms**), inventory reported

### **Creator Tools**

These tools are for mod authors, asset creators and people building their own tools.

- Generate a compilable C++ SDK from the running game.
- Produce a `.usmap` mapping file for supported asset tools.
- Export game type data, class relationships and package dependency graphs.
- Validate a generated SDK against the running game and write a report.
- Generate ready-to-open project files for the created SDK.
- Compare two generated SDKs and export the changes.

### **Configuration**

Settings live in a config file next to the runtime. Universal Manager edits it for you from the Avalonium tab, so you rarely touch the file itself.

- Turn the in-game menu, developer console, Lua mods, native mods and archive reports on or off.
- Choose the graphics backend for the overlay.
- Set a startup delay for games that finish loading slowly.
- Point Lua and native mods at a custom folder, or leave them empty to use the local `Mods/Avalonium` folder.
- Set logging level and output folders for generated files.

### **Logging And Troubleshooting**

- One structured log per launch, with older logs rotated.
- The log records host facts, game facts, loaded modules and a closing summary.
- An optional crash monitor writes reports and, when enabled, dump files for abnormal exits. The monitor is passive by default; deeper capture is opt-in.
- Crash reports include the game modules, recent log lines and stack details, so a bug can be described without guessing.
- Logs can be deleted at any time.

### **Compatibility And Limits**

- Supports Unreal Engine 4.23 through 5.8.
- The in-game overlay needs a supported DirectX game; games that do not expose a supported rendering path do not get an overlay.
- Blueprint mods must be packaged game files, not source assets.
- Native mods must be built for Avalonium; binaries from another mod runtime will not load.
- Mods reuse existing game textures, fonts and styles, but cannot create new game assets or new widget classes.
- Avalonium does not modify the game's existing files. It loads beside the game and writes its own logs, reports, dumps and mod folders.
- The tool connection is local to the PC and has no network authentication.

### **The Avalonium Tab Inside Universal Manager**

The **Avalonium Runtime Engine** tab under **Advanced** controls injection, the live runtime console, engine globals, generated output, runtime feature toggles, SDK generation, paths, startup delay, and Lua mod management. It warns when the game is still running or another runtime is present, and every setting includes built-in help.

---

## **Requirements**

*Qt 6 and all required libraries are **bundled with Universal Manager**: no separate installation needed.*

- **Operating System:** Windows 10 (1809+) or Windows 11, 64-bit only
- **Python Runtime:** Version 3.x or later: required for Avalonium (Unreal Engine Runtime features like SDK generation, USMAP generation, and runtime injection). [Download](https://www.python.org/downloads/)
- **.NET Desktop Runtime:** Version 10.0 or later: required for PakViewer (Assets Viewer) and the Advanced Conflict Detector. [Download](https://dotnet.microsoft.com/download/dotnet/10.0)
- **libclang.dll:** Required for SDK generation with AST compilation and verification. Included with Visual Studio: Avalonium automatically detects it from your Visual Studio installation.
- **Storage:** 850 MB for Universal Manager
- **RAM:** 2 GB minimum, 4 GB recommended
- **Extraction Tools:** 7-Zip or WinRAR (for compressed mod archives)

*Mod management features work without Python, .NET, or libclang. Python is only needed for Avalonium / Unreal Engine Runtime features. .NET is only needed for PakViewer and the Advanced Conflict Detector. libclang.dll is only needed for SDK generation with AST compilation and verification.*

---

## **Unreal Engine Compatibility**

**Universal Manager:** Unreal Engine **4.0 → 5.8**

**Avalonium Runtime:** Unreal Engine **4.23 → 5.8**

The architecture is designed to accommodate **future Unreal Engine versions** through runtime discovery rather than game-specific hardcoded offsets.

---

## **Project Structure**

Universal Manager combines a mod library, game profiles, Nexus integration, download manager, conflict detector, Assets Viewer, game launcher, Avalonium runtime, and creator tooling into one app, with every part sharing the same profiles and mod folders.

---

## **Performance**

Universal Manager and Avalonium are designed with **performance in mind**.

**Avalonium:**
- Learns the game once and reuses safe findings on later starts
- Runs its heavier scans in the background so the game keeps booting
- Keeps a feature off rather than guessing at something it cannot confirm

**Universal Manager:**
- **Optimized Mode**: reduces memory use sharply when the window loses focus, and can pause animations between pages
- **Aggressive RAM Cleaner** (beta): releases memory once a usage threshold you set is crossed
- **System Overview**: live CPU, RAM and disk readouts, with cleanup counters
- A memory-aware cleaner inside the Assets Viewer that pauses open viewers when the process grows too large

> There is no point in making a mod manager if it hurts performance or consumes unnecessary resources while you are playing.

---

## **Development Journey and the Challenges**

Universal Manager was developed from **2024 to 2026** as a solo project. What began as a mod-management project gradually became a much larger system containing a desktop application, asset viewer, conflict detector, Nexus Mods integration with its own download manager, Unreal Engine runtime, version detection, SDK generation, Lua scripting, and multiple supporting tools.

A major challenge was supporting **Unreal Engine across years of changing internal layouts**. Avalonium had to operate across heavily stripped, obfuscated, encrypted, and customized Unreal Engine builds while avoiding game-specific hardcoding. The arrival of **Unreal Engine 5.8** introduced additional engine changes that required another round of compatibility work before release.

Developing alone also meant handling the entire development cycle personally: **programming**, **UI development**, **testing**, **debugging**, **documentation**, and **compatibility testing**. Testing sometimes meant installing games well over 100 GB simply to verify a single feature, then removing them to make room for another test target, and repeating the cycle when something needed to be fixed.

Despite the difficulty, I continued because I enjoy building this. Universal Manager is a **hobby project**, and it is being shared freely because I value creating useful tools and making them available to other modders.

Along the way, I learned **C#**, **Python**, **Unreal Engine internals**, **memory analysis**, **runtime systems**, **QML**, and a great deal more while building on my original **C++ experience**.

> After that journey, Universal Manager was born.

---

## **Disclaimer**

### **Universal Manager Project Disclaimer**

Universal Manager is provided for **lawful modding**, **interoperability research**, **compatibility testing**, **educational analysis**, and **game development workflows** involving supported Unreal Engine titles.

These tools provide extensive capabilities including but not limited to: **SDK generation**, **reflection data extraction**, **runtime inspection**, **in-game debugging interfaces**, **live memory manipulation**, **property and function hooking**, **developer console unlocking**, **object browser and editor**, **Lua scripting runtime**, **AES encryption key discovery**, **pak/utoc archive decryption**, **asset extraction and analysis**, **direct memory read/write operations**, **native function interception**, and **process injection**. These features are designed to assist legitimate developers, modders, researchers, and content creators in understanding game structures, building compatible modifications, analyzing interoperability, and creating educational content.

**This software is explicitly NOT intended for:**
- **Cheating or gaining unfair advantages** in multiplayer, competitive, or online games
- **Bypassing anti-cheat systems**, digital rights management (DRM), copy protection, or security measures
- **Unauthorized access** to game servers, player accounts, or protected systems
- **Creating, distributing, or using cheats, hacks, or exploits** for competitive advantage
- **Reverse engineering for commercial piracy** or intellectual property theft
- **Service disruption, denial of service attacks**, or any form of network abuse
- **Malicious tampering** with game files, save data, or online services
- **Violating End User License Agreements (EULAs)**, Terms of Service (ToS), or platform policies
- **Circumventing regional restrictions**, content locks, or licensing limitations
- **Abusing third-party services**: including scraping, exceeding rate limits, or downloading content in ways the host site's rules do not allow
- **Any activity that violates applicable laws**, regulations, or third-party rights

**User Responsibility & Acknowledgment**

By downloading, installing, or using Universal Manager and Avalonium, you explicitly acknowledge and agree that:
- You are **solely and exclusively responsible** for all actions taken with this software
- You will **use these tools only for lawful, authorized purposes** consistent with this disclaimer
- You will **comply with all applicable laws**, regulations, EULAs, Terms of Service, and third-party agreements
- You understand that **modifying online or multiplayer games may result in permanent bans**, account suspensions, or legal action from game publishers
- You understand that **crypto and asset discovery features** are provided for lawful interoperability research and authorized mod development only
- You understand that **Nexus Mods is a third-party service** you use under its own Terms of Service and API rules. Universal Manager is not affiliated with or endorsed by Nexus Mods, and your API key and sign-in session are yours to keep private
- You will **not distribute, sell, or commercialize** decrypted game assets, extracted code, or generated SDKs without proper authorization
- I am not responsible for any consequences arising from your use or misuse of this software

**Limitation of Liability**

This project is provided **"AS IS"** without any warranties, express or implied, including but not limited to warranties of merchantability, fitness for a particular purpose, or non-infringement.

I assume no responsibility or liability for:
- Account bans, suspensions, or terminations resulting from software use
- Legal action, cease and desist orders, or litigation from game publishers or rights holders
- Data loss, corruption, or damage to game files, save data, or system files
- System instability, crashes, or compatibility issues
- Violation of any laws, regulations, agreements, or third-party rights
- Misuse, abuse, or unauthorized application of the software's capabilities
- Any direct, indirect, incidental, consequential, or punitive damages arising from software use
- Actions taken by third parties (including game developers, publishers, or platform holders) in response to software use

**Use responsibly. Use ethically. Use legally.**

---

<div align="center">

### **Thank you for using Universal Manager Project!**
