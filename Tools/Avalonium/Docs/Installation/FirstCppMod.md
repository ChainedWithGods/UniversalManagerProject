# Build Your First C++ Mod

This page builds a complete mod and gets it running in the game. It uses the
runtime's own headers, so it works on any supported game without a generated SDK.
Adding the generated SDK later takes one include path.

Prerequisites:

- The runtime is injected and initialized ([Confirm the Runtime Is Running](VerifyRuntime.md)).
- Visual Studio with the C++ workload, or the MSVC Build Tools, plus CMake 3.21
  or newer. The CMake that ships with Visual Studio works.
- The Avalonium toolkit folder, the one that contains `Runtime` and `Helpers`.

## 1. Project layout

```text
MyFirstMod\
  src\main.cpp
  CMakeLists.txt
```

## 2. src\main.cpp

```cpp
#include <Avalonium/Avalonium.hpp>
#include <Avalonium/ImGuiHook.hpp>

#include <atomic>
#include <cstdint>

class MyFirstMod final : public RC::CppUserModBase
{
  public:
    MyFirstMod()
    {
        ModName = "My First Mod";
        ModVersion = "1.0.0";
        ModDescription = "Shows a window with a line of text.";
        ModAuthors = "Your name";
    }

    void on_unreal_init() override
    {
        m_Render = RC::ImGui::RegisterRender("MyFirstMod", &MyFirstMod::Render, this);
        m_Key = RC::Keys::Register("F6", &MyFirstMod::OnKey, this);
        Avalonium::LogInfo("MyFirstMod: loaded; press F6 in game");
    }

    void on_update() override
    {
        if (!m_Toggle.exchange(false)) return;
        m_Open.store(!m_Open.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    void on_unload() override
    {
        if (m_Render)
        {
            RC::ImGui::SetInputActive(m_Render, false);
            RC::ImGui::UnregisterRender(m_Render);
            m_Render = 0;
        }
        if (m_Key)
        {
            RC::Keys::Unregister(m_Key);
            m_Key = 0;
        }
    }

  private:
    static void OnKey(void* user)
    {
        static_cast<MyFirstMod*>(user)->m_Toggle.store(true);
    }

    static void Render(void* user)
    {
        auto* self = static_cast<MyFirstMod*>(user);
        if (!self->m_Render) return;

        const bool open = self->m_Open.load(std::memory_order_relaxed);
        RC::ImGui::SetInputActive(self->m_Render, open);
        if (!open || !RC::ImGui::BeginFrame()) return;

        if (ImGui::Begin("My First Mod"))
        {
            ImGui::TextUnformatted("hello from my mod");
        }
        ImGui::End();
    }

    std::atomic<bool> m_Toggle{false};
    std::atomic<bool> m_Open{false};
    uint32_t m_Render = 0;
    uint32_t m_Key = 0;
};

extern "C" __declspec(dllexport) uint32_t AvaloniumModAbiVersion()
{
    return AVALONIUM_MOD_ABI_VERSION;
}

extern "C" __declspec(dllexport) RC::CppUserModBase* start_mod()
{
    return new MyFirstMod();
}

extern "C" __declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod)
{
    delete mod;
}
```

The three exports are the loader contract. A DLL without them is not treated as a
C++ mod.

## 3. CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyFirstMod LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

set(AVALONIUM_ROOT "" CACHE PATH "Folder that contains Runtime and Helpers")
if(NOT EXISTS "${AVALONIUM_ROOT}/Runtime/cppmods/sdk/Avalonium/ModApi.h")
    message(FATAL_ERROR "Set AVALONIUM_ROOT to the Avalonium folder")
endif()

set(SDK_DIR "${AVALONIUM_ROOT}/Runtime/cppmods/sdk")
set(IMGUI_DIR "${AVALONIUM_ROOT}/Helpers/Universal-Dear-ImGui-Hook/imgui")

add_library(MyFirstMod MODULE
    src/main.cpp
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
)

target_include_directories(MyFirstMod PRIVATE
    "${SDK_DIR}"
    "${IMGUI_DIR}"
)

target_compile_definitions(MyFirstMod PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
)

target_compile_options(MyFirstMod PRIVATE
    /EHsc
    $<$<CONFIG:Release>:/O2>
)

set_target_properties(MyFirstMod PROPERTIES
    OUTPUT_NAME main
    PREFIX ""
    SUFFIX ".dll"
)
```

- `/MT` through `CMAKE_MSVC_RUNTIME_LIBRARY` keeps one static CRT, which is what
  the runtime expects from native mods.
- The mod compiles the runtime's four ImGui sources. The shared ImGui context is
  only layout-compatible with the same sources and version.
- A `build.bat` with raw `cl` flags is the alternative; the
  [tutorial](../CppAPI/TUTORIAL.md) shows it.

## 4. Build with Visual Studio

```text
cmake -S . -B build -A x64 -DAVALONIUM_ROOT="C:\Path\To\Avalonium"
cmake --build build --config Release
```

Without `-G`, CMake picks the Visual Studio generator installed on the machine.
To name it, use the value from `cmake --help` for your version, for example
`-G "Visual Studio 17 2022"` or `-G "Visual Studio 18 2026"`. Older CMake
releases do not know newer generators.

Open `build\MyFirstMod.sln` to build and debug in the IDE; select `x64` and
`Release`. The output is `build\Release\main.dll`. Debug builds are fine for
testing but slower; rebuild Release before you ship.

## 5. Deploy

Close the game first. A loaded DLL is locked and cannot be replaced.

Manual install, with `CppModsPath` empty in `Avalonium.ini`:

```text
<game>\Binaries\Win64\Mods\Avalonium\MyFirstMod\dlls\main.dll
```

- `main.dll` is the preferred name. When it is absent, `dlls\<FolderName>.dll`
  is also accepted, and a `<FolderName>.dll` directly in the mod folder loads
  too.
- `enabled.txt` in the mod folder is optional. Missing or empty enables the mod.
  `0`, `false`, `no`, `off` or `disabled` skips it.
- `shared` and `Cache` are reserved folder names and are never scanned.
- With `CppModsPath` set, the loader scans that folder instead. Universal Manager
  uses this to point the runtime at the profile's C++ mods folder, which is where
  the manager installs mods.

## 6. Verify

Launch the game and read `Avalonium.log`. A successful load shows:

```text
[CppMod] Loaded 'My First Mod' v1.0.0 by Your name (<path>\main.dll)
[CppMod] 1 C++ mod(s) loaded from <mods directory>
[CppMod] Lifecycle callbacks delivered to 1 mod(s)
```

The mod's own line appears too:

```text
MyFirstMod: loaded; press F6 in game
```

Press F6. The window appears; while it is open the overlay owns the mouse, and
closing it returns input to the game.

## Adding the generated SDK

Generate it per game build ([Generate the Game SDK](SDKGeneration.md)), then add
the folder as an include path:

```cmake
set(AVALONIUM_SDK_DIR "" CACHE PATH "Generated SDK folder for this game build")
if(AVALONIUM_SDK_DIR)
    target_include_directories(MyFirstMod PRIVATE "${AVALONIUM_SDK_DIR}")
endif()
```

```text
cmake -S . -B build -A x64 -DAVALONIUM_ROOT="C:\Path\To\Avalonium" -DAVALONIUM_SDK_DIR="D:\Dumps\MyGame\SDK"
```

Typed classes, members and function wrappers are covered in
[SDK_MODS.md](../CppAPI/SDK_MODS.md). The generated SDK is optional; the mod keeps
working without it.

## Adding a logo or another image

The runtime decodes image files and embedded image bytes into live textures. Both
routes, including the build copy and the resource script, are in
[TUTORIAL.md section 7](../CppAPI/TUTORIAL.md). A full example mod is in
[EXAMPLES.md section 9](../CppAPI/EXAMPLES.md).

## When it does not load

| Symptom | Check |
| :--- | :--- |
| No `[CppMod]` lines at all | `EnableCppMods=true`, and the runtime loaded at all |
| `[CppMod] Mods directory does not exist` | `CppModsPath` points at a missing folder |
| `reports mod ABI N; this runtime accepts ABI M` | rebuild the mod against this runtime's headers |
| `has no start_mod/uninstall_mod exports` | the three exports are missing |
| Loaded, but every action is dead | the mod was built against a newer runtime API than the game's runtime |
| The new DLL did not take effect | the game was running and the old DLL stayed locked |

The full list is in [Troubleshooting](Troubleshooting.md).
