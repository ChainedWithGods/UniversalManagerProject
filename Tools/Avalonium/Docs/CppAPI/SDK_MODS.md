# CppAPI: Mods Built on the Generated SDK

The [tutorial](TUTORIAL.md) and the [examples](EXAMPLES.md) use runtime reflection
only. That is the right tool for a mod that must work across games and engine
versions.

A generated SDK is the other half. It gives a mod typed classes, fields and
function wrappers for one game build, which keeps gameplay code short and lets
out parameters come back as normal C++ references.

Both routes use the same runtime. Pick the SDK when you write logic against known
game classes; keep reflection for discovery, tools and cross-game mods.

---

## 1. What the generator writes

`GENSDK` (runtime console or IPC) writes to `<gameRoot>\SDK` and
`<gameRoot>\Reflection`. For each package you get:

```text
Basic.h                        runtime backend, containers, FName, SDKHelpers
SDK.h                          includes every generated package header
UtfN.hpp                       string helpers
UnrealContainers.hpp           TArray, TMap, FString and friends
PropertyFixup.hpp              property access helpers
Assertions.inl                 optional layout checks (CHECK_SDK_LAYOUT)
<Package>_classes.h            classes, members at their real offsets, wrappers
<Package>_structs.h            structs at their real offsets
<Package>_parameters.h         parameter blocks used by the wrappers
<Package>_functions.cpp        function wrappers for that package
```

`Basic.h` resolves `AvaloniumGetSdkApi` from `Avalonium.dll` on first use. The
table is separate from the mod API and carries object enumeration, class,
function and name lookup plus `ProcessEvent`. No engine address is baked into the
generated files.

Keep the generator output per game and per build. A game patch needs a fresh
generation.

---

## 2. Project layout

Copy the SDK folder next to the mod and point the compiler at it:

```text
MyMod\
  sdk\             generated files for this game build
  src\main.cpp
  build.bat
```

Add the package implementation files you actually call. Each
`*_functions.cpp` includes `SDK.h`, so it pulls every generated header. If you
only read fields, include the specific `*_classes.h` and skip the function
files.

```bat
cl /nologo /std:c++20 /EHsc /O2 /MT /DNDEBUG ^
    /I"%SDK%\Runtime\cppmods\sdk" ^
    /I"%SDK%\Helpers\Universal-Dear-ImGui-Hook\imgui" ^
    /I"sdk" ^
    /Fo:build\ /Fe:build\main.dll /LD ^
    "src\main.cpp" ^
    "sdk\Project_Classes_Character_Base_functions.cpp" ^
    "%SDK%\Helpers\Universal-Dear-ImGui-Hook\imgui\imgui.cpp" ^
    "%SDK%\Helpers\Universal-Dear-ImGui-Hook\imgui\imgui_draw.cpp" ^
    "%SDK%\Helpers\Universal-Dear-ImGui-Hook\imgui\imgui_tables.cpp" ^
    "%SDK%\Helpers\Universal-Dear-ImGui-Hook\imgui\imgui_widgets.cpp"
```

The mod still exports `AvaloniumModAbiVersion`, `start_mod` and `uninstall_mod`,
and still loads through the mod loader. The generated SDK is compiled into the
mod; it is not a separate plugin.

---

## 3. Typed members

Generated members sit at their real offsets. Reading and writing them is plain
C++:

```cpp
#include "SDK/Basic.h"
#include "SDK/Project_Classes_Character_Base_classes.h"

struct FPlayerStats
{
    float Current = 0.0f;
    float Max = 0.0f;
};

FPlayerStats ReadStats(ACharacter_Base_C* character)
{
    FPlayerStats stats;
    if (!character) return stats;
    stats.Current = character->health;
    stats.Max = character->Health_Max;
    return stats;
}
```

This is the same memory reflection would reach, with a typed accessor instead of
a property name. The engine spelling constraint disappears because the generator
already resolved every name and offset.

---

## 4. Typed calls and out parameters

The wrappers are the biggest win. Out parameters are references, so no frame
assembly is needed:

```cpp
#include "SDK/Project_Classes_Character_Base_classes.h"

bool IsMyTarget(ACharacter_Base_C* observer, ACharacter_Base_C* candidate)
{
    bool yes = false;
    if (observer && candidate)
        observer->is_my_target_Character(candidate, yes);
    return yes;
}
```

Compare with the reflection route, which has to build a frame and read the out
value by parameter name. The wrapper does exactly that for you:

```cpp
void ACharacter_Base_C::is_my_target_Character(ACharacter_Base_C* char_, bool& Yes)
{
    static SDKHelpers::ReloadableFuncInfo FuncInfo{};
    UFunction* Func = SDKHelpers::GetCachedFunction(FuncInfo);
    if (Func == nullptr)
        Func = Class ? Class->GetFunction("Character_Base_C", "is_my_target_Character")
                     : SDKHelpers::GetFunction("Character_Base_C", "is_my_target_Character");
    SDKHelpers::SetCachedFunction(Func, FuncInfo);

    ACharacter_Base_C_is_my_target_Character_Params Parms{};
    Parms.char_ = char_;
    Parms.Yes = Yes;
    SDKHelpers::ProcessEvent(this, Func, &Parms);
    Yes = Parms.Yes;
}
```

The wrapper resolves the live UFunction by name at runtime, so the generated code
carries offsets but no addresses. Function bodies still cost a lookup the first
time; the generated cache handles later calls.

---

## 5. Finding live objects

`Basic.h` exposes the object array. Iterate it and filter with the generated
class:

```cpp
ACharacter_Base_C* FindNewestCharacter()
{
    UClass* cls = ACharacter_Base_C::StaticClass();
    ACharacter_Base_C* found = nullptr;
    const int32_t count = SDKHelpers::NumObjects();
    for (int32_t i = 0; i < count; ++i)
    {
        UObject* object = SDKHelpers::GetObjectByIndex(i);
        if (!object || !SDKHelpers::IsA(object, cls)) continue;
        if (SDKHelpers::GetObjectName(object).rfind("Default__", 0) == 0) continue;
        found = static_cast<ACharacter_Base_C*>(object);
    }
    return found;
}
```

The object array is ordered oldest first, so the last match is the newest
instance. Liveness still matters: check the object path the same way the tutorial
does before using it across a level load.

---

## 6. Example: typed player HUD

This mod reads the live player through the SDK and draws health with ImGui. It
uses the mod API for lifecycle and rendering, and the SDK for game access.

```cpp
#include <Avalonium/Avalonium.hpp>
#include <Avalonium/ImGuiHook.hpp>
#include "SDK/Basic.h"
#include "SDK/Project_Classes_Character_Base_classes.h"

class PlayerHud final : public RC::CppUserModBase
{
  public:
    void on_unreal_init() override
    {
        m_Render = RC::ImGui::RegisterRender("PlayerHud", &Render, this);
    }

    void on_update() override
    {
        ACharacter_Base_C* player = nullptr;
        UClass* cls = ACharacter_Base_C::StaticClass();
        const int32_t count = SDKHelpers::NumObjects();
        for (int32_t i = 0; i < count; ++i)
        {
            UObject* object = SDKHelpers::GetObjectByIndex(i);
            if (!object || !SDKHelpers::IsA(object, cls)) continue;
            if (SDKHelpers::GetObjectName(object).rfind("Default__", 0) == 0) continue;
            player = static_cast<ACharacter_Base_C*>(object);
        }
        m_Player = player;
        if (player)
        {
            m_Current = player->health;
            m_Max = player->Health_Max;
        }
    }

    void on_unload() override
    {
        if (m_Render) RC::ImGui::UnregisterRender(m_Render);
    }

  private:
    static void Render(void* user)
    {
        auto* self = static_cast<PlayerHud*>(user);
        if (!RC::ImGui::BeginFrame()) return;
        if (ImGui::Begin("Player"))
        {
            const float fraction = self->m_Max > 0.0f ? self->m_Current / self->m_Max : 0.0f;
            ImGui::Text("Health %.0f / %.0f", self->m_Current, self->m_Max);
            ImGui::ProgressBar(fraction, ImVec2(220.0f, 12.0f));
        }
        ImGui::End();
    }

    uint32_t m_Render = 0;
    ACharacter_Base_C* m_Player = nullptr;
    float m_Current = 0.0f;
    float m_Max = 0.0f;
};
```

Note the split: the game thread reads typed memory into plain floats, and the
render thread only draws those values.

---

## 7. Names and strings

Generated `FName` is the engine pair (comparison index and number), with
`ToString()`. It has no string constructor because creating engine names needs the
name pool.

How to get names to pass into wrappers:

- Take the `FName` from a live property, for example an item component's
  `item_name` field, and pass that value through.
- Take it from row data read through the mod API, which accepts text.
- Compare names through `ToString()` instead of building new ones.

The runtime's mod API resolves rendered instance names such as `Foo_0` to the
engine FName `Foo` with number 1. Generated wrappers expect the same FName value,
so a name read from the game can be passed back safely.

---

## 8. SDK and mod API in one DLL

Use both:

| Need | Use |
| :--- | :--- |
| Lifecycle, ImGui, key binds, settings | `AvaloniumModApi` (`Avalonium.hpp`) |
| Texture views, DataTable rows, property paths | `AvaloniumModApi` |
| Typed members and wrapper calls for one game | Generated SDK |

The two tables are independent. The generated SDK checks
`AvaloniumGetSdkApi`; the mod's own code checks `AvaloniumGetModApi`. Shipping a
mod that uses both means requiring a runtime that answers both, which every
current build does.

---

## 9. When to regenerate

Regenerate the SDK when:

- the game updates and offsets or functions change,
- you move the mod to another game,
- a wrapper reports a missing function at runtime.

Layout checks are optional but useful during a game update:

```bat
cl /DCHECK_SDK_LAYOUT ... sdk\Project_Classes_Character_Base_functions.cpp
```

`Assertions.inl` then fails the build when a generated offset no longer matches
the live class.

---

## 10. Checklist

- The SDK folder matches the installed game build.
- Only the needed `*_functions.cpp` files are compiled into the mod.
- Object lookups skip `Default__` objects and use the newest live instance.
- Typed values are read on the game thread and copied before drawing.
- The mod still exports the three loader symbols and loads through the mod
  folder.
