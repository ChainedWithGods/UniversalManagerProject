# CppAPI Tutorial: Build a Native C++ Mod

This tutorial follows the same steps used to build the in-game item spawner for
The Killing Antidote. It covers the parts that are not in the API reference:
project setup, finding game content, drawing icons, calling game functions,
debugging live, and the traps that cost the most time.

The example game calls are from The Killing Antidote (UE4.27), but every pattern
here works on any supported engine version.

---

## 1. Project setup

A native mod is a standalone x64 DLL, built outside the runtime.

```text
MyMod\
  src\main.cpp
  build.bat
```

Minimal mod:

```cpp
#include <Avalonium/Avalonium.hpp>
#include <Avalonium/ImGuiHook.hpp>

class MyMod final : public RC::CppUserModBase
{
  public:
    MyMod()
    {
        ModName = "My Mod";
        ModVersion = "1.0.0";
        ModDescription = "Does one thing.";
        ModAuthors = "Your name";
    }

    void on_unreal_init() override
    {
        m_Render = RC::ImGui::RegisterRender("MyMod", &MyMod::Render, this);
    }

    void on_update() override
    {
        // Runs once per game frame on the game thread.
    }

    void on_unload() override
    {
        if (m_Render) RC::ImGui::UnregisterRender(m_Render);
    }

  private:
    static void Render(void* user)
    {
        auto* self = static_cast<MyMod*>(user);
        if (!RC::ImGui::BeginFrame()) return;
        if (ImGui::Begin("My Mod")) ImGui::TextUnformatted("hello");
        ImGui::End();
    }

    uint32_t m_Render = 0;
};

extern "C" __declspec(dllexport) uint32_t AvaloniumModAbiVersion()
{
    return AVALONIUM_MOD_ABI_VERSION;
}

extern "C" __declspec(dllexport) RC::CppUserModBase* start_mod()
{
    return new MyMod();
}

extern "C" __declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod)
{
    delete mod;
}
```

`build.bat` compiles the mod plus the runtime's four ImGui sources and copies the
DLL to the game (`%SDK%` is the Avalonium install folder, the one that contains
`Runtime` and `Helpers`):

```bat
cl /nologo /std:c++20 /EHsc /O2 /MT /DNDEBUG /DNOMINMAX /DWIN32_LEAN_AND_MEAN ^
    /I"%SDK%\Runtime\cppmods\sdk" ^
    /I"%SDK%\Helpers\Universal-Dear-ImGui-Hook\imgui" ^
    /Fo:build\ /Fe:build\main.dll /LD ^
    "src\main.cpp" ^
    "%SDK%\Helpers\Universal-Dear-ImGui-Hook\imgui\imgui.cpp" ^
    "%SDK%\Helpers\Universal-Dear-ImGui-Hook\imgui\imgui_draw.cpp" ^
    "%SDK%\Helpers\Universal-Dear-ImGui-Hook\imgui\imgui_tables.cpp" ^
    "%SDK%\Helpers\Universal-Dear-ImGui-Hook\imgui\imgui_widgets.cpp"
```

- C++20 is required (the SDK uses `std::format`).
- `/MT` keeps one static CRT for the DLL.
- The mod must export `AvaloniumModAbiVersion`, `start_mod` and `uninstall_mod`.
- Build target: `bin\main.dll`, deployed to
  `<game>\Binaries\Win64\Mods\Avalonium\<ModName>\dlls\main.dll`.
- The game needs `EnableCppMods=true` in `Avalonium.ini` next to `Avalonium.dll`.

If the game is running, the DLL is locked. Close it before deploying.

---

## 2. Thread model

Two threads matter:

- `on_update` runs on the game thread. Reflection, property reads and writes,
  function calls and UMG work belong here.
- The ImGui render callback runs on the render thread. Build UI only; keep engine
  access out of it.

Data read on the game thread and drawn on the render thread must be copied into
plain values first. The item spawner builds its catalog on the game thread and the
draw code only reads the copied strings, pointers and sizes.

Engine object pointers can cross threads as values, but calling them cannot.

---

## 3. Liveness, the hard part

`IsValid()` only says the memory is readable. A destroyed actor can still pass it,
and its old property values can look correct. Validate paths instead:

```cpp
bool IsUsable(RC::Unreal::UObject* object)
{
    if (!object || !object->IsValid()) return false;
    const std::string path = object->GetPathName().ToString();
    return !path.empty() && path.rfind("None", 0) != 0 &&
           path.find("None:") == std::string::npos &&
           path.find(".None") == std::string::npos;
}
```

Other rules that saved time:

- After a level change, the old world, state and controller can stay readable.
  Re-resolve them per level instead of caching pointers forever.
- `GetObjectsOfClass` returns objects oldest first. The live instance is usually
  the last match.
- The persistent route to the player after travel is
  `UWorld.OwningGameInstance -> UGameInstance.LocalPlayers -> ULocalPlayer.PlayerController`.
  The GameInstance survives level loads, so its LocalPlayer is replaced while a
  stale controller is not.
- Loading worlds and loading pawns also look valid. Reject objects whose path
  contains `Loading`.

---

## 4. Property names

Reflection uses the engine's own names, which may contain spaces:

```cpp
RC::Unreal::Value value;
object->GetPropertyValue("Health Max", value);
float health = value.As<float>();
```

`Health_Max` is a generated-SDK spelling and does not resolve at runtime. When a
property seems missing, print the class properties and look for the exact name.

For properties that are not `UPROPERTY` (for example a texture's platform data),
read them through the object graph or a native call instead of reflection.

---

## 5. DataTables and row icons

DataTable rows are the cheapest source of item data: names, icons, meshes and
stats all live there.

```cpp
RC::Unreal::UObject* table =
    RC::Unreal::UObject::FindObject("/Game/.../ItemTable.ItemTable");
void* row = RC::Unreal::GetDataTableRow(table, "Shotgun");
```

Read the row through the table's `RowStruct`:

```cpp
RC::Unreal::UObject* rowStruct = table->GetProperty<RC::Unreal::UObject*>("RowStruct");
for (const RC::Unreal::PropertyInfo& property :
     static_cast<RC::Unreal::UStruct*>(rowStruct)->GetProperties())
{
    void* field = static_cast<uint8_t*>(row) + property.Offset;
    // property.Name, property.TypeName, property.ElementSize
}
```

Row icon fields are usually object properties named with "icon" somewhere. Scan
for them instead of hardcoding one name:

```cpp
if (property.TypeName.find("Object") == std::string::npos) continue;
if (property.Name.find("icon") == std::string::npos) continue;
void* icon = nullptr;
std::memcpy(&icon, static_cast<uint8_t*>(row) + property.Offset, 8);
```

Two traps:

- The row buffer is live engine storage. It is read-only and does not survive a
  table reload.
- Row names ending in `_0`, `_1` are often FNames that render as base text plus an
  instance number (`Foo` with number 1 renders as `Foo_0`). The runtime resolves
  both forms, so pass the name the row list shows.

---

## 6. Icons in ImGui

Resolve the texture pointer from the row, then draw it:

```cpp
const uint64_t texture = RC::ImGui::TextureId(iconObject);
if (texture) ImGui::Image(static_cast<ImTextureID>(texture), ImVec2(96.0f, 96.0f));
```

Rules learned the hard way:

- `TextureId` returns a D3D11 shader resource view or a D3D12 GPU descriptor
  handle. It returns 0 for a texture without a render resource or an unsupported
  backend.
- `ImGui::Image` handles coordinates. If you draw through a draw list, use
  `ImGui::GetCursorScreenPos()`, not `GetCursorPos()`. Window-local coordinates
  put the image outside the window and it disappears.
- Icons are rarely square. Read `Blueprint_GetSizeX` and `Blueprint_GetSizeY` on
  the game thread, then fit the draw rect into a square box to keep the shape.
- Textures the game never displayed may keep only low mips, which looks blurry.
  Force the full chain once, before the view is cached:

```cpp
texture->SetPropertyValue("NeverStream", RC::Unreal::Value(true));
RefreshTexture(texture); // UpdateResource through CanvasRenderTarget2D, game thread
```

`RefreshTexture` uses the reflected `UpdateResource` on `CanvasRenderTarget2D`.
That function dispatches virtually, so calling it on any Texture2D rebuilds that
texture's resource:

```cpp
void RefreshTexture(RC::Unreal::UObject* texture)
{
    static RC::Unreal::UFunction* update = nullptr;
    if (!update)
    {
        RC::Unreal::UClass* cls =
            RC::Unreal::UObject::FindClass("CanvasRenderTarget2D");
        update = cls ? cls->FindFunction("UpdateResource") : nullptr;
    }
    if (!update) return;
    const int32_t size = (std::max)(update->GetParametersSize(), 0x20);
    std::vector<uint8_t> frame(static_cast<size_t>((size + 15) & ~15), 0);
    texture->ProcessEvent(update, frame.data());
}
```

The runtime caches one view per texture and keeps it alive, so do this during the
scan and draw afterwards.

---

## 7. Calling game functions

Simple calls use positional values:

```cpp
RC::Unreal::Value result;
std::vector<RC::Unreal::Value> args{
    RC::Unreal::Value(RC::Unreal::FName("Shotgun")),
    RC::Unreal::Value(1)};
object->CallValues("Some Function", args, &result);
```

Blueprint functions keep their engine name, spaces included.

Out parameters are the catch: `CallValues` returns only the function's return
value. To read an out value, build the frame yourself:

```cpp
const AvaloniumModApi* api = Avalonium::Detail::Api();
RC::Unreal::UFunction* function = object->FindFunction("Spawn Item Ex");
std::vector<uint8_t> frame(frameSize, 0);

api->set_function_argument(function->Address(), frame.data(), "ItemName", &nameValue.Raw());
api->process_event(object->Address(), function->Address(), frame.data());

RC::Unreal::Value out;
api->get_function_argument(function->Address(), frame.data(), "act", &out.RawMutable());
RC::Unreal::UObject* actor = static_cast<RC::Unreal::UObject*>(out.AsObject());
```

This is how a spawn call can be verified: a null out actor means the game placed
nothing, so the mod can fall back to another action instead of reporting success.

Names that render with an instance suffix are handled: passing `Foo_0` writes the
engine FName `Foo` with number 1, so the game finds the row the list showed.

---

## 8. Give vs spawn

Not every item has a world model. Clothing tables, for example, are data rows with
no actor class, so a spawn request silently returns nothing.

Detect it at scan time instead of guessing:

- Entries found through live item components or actor class defaults can spawn.
- Entries that exist only as DataTable rows may only be given.
- Store a `CanSpawn` flag per row, make the UI give for the rest, and verify the
  spawn result as above.

---

## 9. Marking mod content

Workshop content mounts under `/Game/Mod/...` in this game. A row can be marked as
mod content from its actor path or its icon package:

```cpp
bool IsModPath(const std::string& path)
{
    return path.find("/Game/Mod/") != std::string::npos;
}
```

Icons and names still work the same; the mark is only for display.

---

## 10. Debugging live

The runtime has a TCP command server (`RuntimeAPI/IPC.md`, default port 20030).
It answers while the game runs:

```text
VERSION
QUERY action=find class=Texture2D name=Icon limit=50 fields=ptr,name,path
PROPS <ptr>
GET <ptr> <property.path>
READPTR <base> <offset>
READMEM <addr> <size>
CALLFUNC <ptr> <FunctionName>
```

Typical loop:

1. `QUERY` to find an object by class and name.
2. `GET` or `PROPS` to read its properties and offsets.
3. `READMEM` to walk pointers that reflection does not expose.
4. Add a temporary `Avalonium::LogInfo` line in the mod for values the IPC cannot
   show, then remove it once the cause is known.

The runtime log also carries `[CppMod]` lines from every loaded mod.

---

## 11. Settings and UI

- Save settings next to the mod with `GetModPath()`. That call is empty inside the
  render callback, so cache the path during `on_unreal_init`.
- Keep rows simple. A two-column table with an icon, a name and one action button
  scales to hundreds of items.
- Right-click menus are cheap and keep the main row clean.
- Do heavy scans when the menu opens or a level changes, not every frame, and
  never do engine work in the render callback.

---

## 12. Checklist before release

- The mod loads with `EnableCppMods=true` and logs its name and version.
- F4 or the chosen key opens and closes the UI, and input focus returns to the game.
- Every action works after a level load, a restart and a main-menu round trip.
- Null and stale objects are rejected by path, not only by `IsValid`.
- Icons are cached once, fitted to their aspect ratio, and no engine calls run on
  the render thread.
- Temporary diagnostics are removed and the log is clean.

---

## More patterns

[EXAMPLES.md](EXAMPLES.md) has complete shapes for a texture browser, a DataTable
explorer, a live object inspector, an actor HUD with screen projection, a UFunction
hook monitor, and a single DLL that adapts to every supported engine version.

For gameplay code written against one game's classes, [SDK_MODS.md](SDK_MODS.md)
covers mods built on the generated SDK: typed members, wrapper calls with out
parameters, and the build layout that compiles only the packages you use.
