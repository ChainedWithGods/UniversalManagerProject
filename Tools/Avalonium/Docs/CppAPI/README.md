# CppAPI: Native C++ Mods

Avalonium loads native x64 mod DLLs and gives them one C ABI table to the running
engine. The mod is compiled against the headers in `Runtime/cppmods/sdk/Avalonium`;
the engine side of every call is resolved from the live process, so the same mod
DLL targets UE 4.23 through 5.8 without a game name, an engine switch or an offset
table.

New to native mods? Start with the [tutorial](TUTORIAL.md): project setup, finding
content, icons, game calls, debugging and the traps to avoid. Then browse the
[examples](EXAMPLES.md) for texture, DataTable, inspector, HUD and hook patterns,
and the [generated-SDK guide](SDK_MODS.md) for typed gameplay code.

| Component | Source |
| :--- | :--- |
| Public SDK | [`Runtime/cppmods/sdk/Avalonium`](../../Runtime/cppmods/sdk/Avalonium) |
| Loader and lifecycle | [`Runtime/cppmods/CppModManager.cpp`](../../Runtime/cppmods/CppModManager.cpp) |
| Reflected values and calls | [`Runtime/cppmods/CppModValue.cpp`](../../Runtime/cppmods/CppModValue.cpp), [`Runtime/cppmods/CppModReflection.cpp`](../../Runtime/cppmods/CppModReflection.cpp) |
| Containers | [`Runtime/cppmods/CppModContainers.cpp`](../../Runtime/cppmods/CppModContainers.cpp) |
| Widget delegates | [`Runtime/cppmods/CppModWidgets.cpp`](../../Runtime/cppmods/CppModWidgets.cpp) |
| UFunction hooks | [`Runtime/cppmods/CppModHooks.cpp`](../../Runtime/cppmods/CppModHooks.cpp) |
| C ABI table | [`Runtime/cppmods/CppModApi.cpp`](../../Runtime/cppmods/CppModApi.cpp) |

---

## 1. Mod layout

```text
<game>\Binaries\Win64\Mods\Avalonium\
  MyMod\
    enabled.txt              optional
    dlls\
      main.dll               or MyMod.dll
```

- A C++ mod is a folder under the mods directory with a `dlls` folder and a DLL
  exporting `AvaloniumModAbiVersion`, `start_mod` and `uninstall_mod`.
- `main.dll` is preferred. A DLL named after the folder (`MyMod.dll`) is accepted.
- `enabled.txt` is optional. An empty file, `1`, `true`, `yes`, or `on` enables the
  mod. `0`, `false`, `no`, `off`, or `disabled` skips it. Native mods use this marker;
  `mods.json` and `mods.txt` remain the Lua loader's list.
- A folder may also contain `scripts/main.lua`. The C++ DLL loads before Lua scripts,
  so hooks and key binds exist before a script starts.
- `shared` and `Cache` are reserved directory names.

The DLL must be x64, built with MSVC, and use the same STL ABI as the runtime.
Static CRT is recommended.

Configuration keys in `Avalonium.ini`:

| Key | Default | Meaning |
| :--- | :--- | :--- |
| `EnableCppMods` | `true` | Enables the native mod loader. |
| `CppModsPath` | empty | Mods directory. Empty means `Mods/Avalonium` next to `Avalonium.dll`. |

The loader installs the engine's ProcessEvent hook only when at least one C++ mod
is present.

---

## 2. Mod lifecycle

```cpp
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

The loader checks `AvaloniumModAbiVersion` before calling `start_mod`. A DLL built
against another runtime's C++ ABI does not export it and is rejected without
calling any of its exported functions. `LoadLibrary` has already run its `DllMain`,
as it does for any library.

### C ABI table

The SDK is a header-only facade over one exported table:

```c
AVALONIUM_MOD_API_ENTRY const AvaloniumModApi* AvaloniumGetModApi(uint32_t requested_version);
```

`Api.hpp` resolves it once from the loaded `Avalonium.dll`. The table is size- and
version-stamped; the SDK refuses a table older or smaller than the one it was
built against, and a mod reads only the fields its own headers describe. The
table only grows: a runtime release appends callbacks, and a mod built against a
newer SDK needs a runtime whose table is at least as large. Ship or require the
matching runtime with the mod.

| Callback | Thread | When |
| :--- | :--- | :--- |
| Constructor / `start_mod` | loader thread | DLL is loaded. Initialize members only; do not write engine state. |
| `on_program_start` | game thread | First engine frame after all mods load. |
| `on_unreal_init` | game thread | Same pass. Engine reflection is ready. |
| `on_cpp_mods_loaded` | game thread | Same pass, after `on_unreal_init`. |
| `on_update` | game thread | Once per engine frame pump, approximately every 16 ms. |
| `on_unload` | unload thread | Explicit unload only. Not called when the process exits. |

The mod object carries `ModName`, `ModVersion`, `ModDescription`, `ModAuthors` and
`ModIntendedSDKVersion`, used for the load log; `GetModPath()` returns the mod's
own folder.

`on_ui_init` exists for source shape but is not invoked. ImGui windows are built
from a render callback registered with `RC::ImGui::RegisterRender` (section 3).

### Threading rules

- Property reads are safe from any thread.
- Property writes, object construction, reflected calls, raw ProcessEvent calls and
  every container call run on the game thread. The API refuses them elsewhere and
  logs one warning per kind.
- Key-bind callbacks run on the input thread. Queue engine work with
  `RC::RunOnGameThread`.
- Widget delegate callbacks run on the game thread.
- Hook callbacks run on the thread that invoked the engine function.
- `RC::RunOnGameThread` runs immediately when already on the game thread, and queues
  otherwise.

---

## 3. Engine API

The SDK is header-only; `Avalonium/Avalonium.hpp` includes the whole surface:

| Header | Surface |
| :--- | :--- |
| `ModApi.h` | C ABI struct, value kinds, callback types. |
| `SdkApi.h` | SDK generator runtime table (`AvaloniumGetSdkApi`); `Avalonium.hpp` does not include it. |
| `Api.hpp` | Runtime version, engine version, game name, availability. |
| `Value.hpp` | `FString`, `FName`, `Value`. |
| `Unreal.hpp` | `UObject`, `UClass`, `UStruct`, `UFunction`, calls, struct helpers, arrays, maps, sets, data table rows. |
| `ModBase.hpp` | `CppUserModBase` lifecycle. |
| `Hooks.hpp` | Native and interpreted UFunction hooks and argument/result access. |
| `Keys.hpp` | Key binds. |
| `Widgets.hpp` | Widget kit, object construction, UMG delegate bindings. |
| `ImGui.hpp` | ImGui context, allocators, render-callback registration, texture views. |
| `ImGuiHook.hpp` | Includes `imgui.h` and binds the mod's ImGui copy to the runtime's context and allocators. |
| `GameThread.hpp` | Game-thread queue, frame count, delta seconds. |
| `Log.hpp` | `RC::Output` and `Avalonium::Log*`. |

### Runtime services

```cpp
if (!Avalonium::Available()) return;

int32_t major = Avalonium::EngineMajor();
int32_t minor = Avalonium::EngineMinor();
std::string_view game = Avalonium::GameName();
std::string_view api = Avalonium::RuntimeVersion();

uint64_t frame = RC::GetFrameCount();
double dt = RC::GetDeltaSeconds();
bool onGameThread = RC::IsInGameThread();
```

| Call | Result |
| :--- | :--- |
| `Avalonium::Available()` | True when the mod runs inside Avalonium. |
| `EngineMajor`, `EngineMinor` | The running engine version. |
| `GameName` | The process name the runtime resolved. |
| `RuntimeVersion` | Runtime identification string. |
| `RC::GetFrameCount` | Game-thread frame counter. |
| `RC::GetDeltaSeconds` | Last frame delta. |
| `RC::IsInGameThread` | Thread test for the engine's game thread. |
| `RC::RunOnGameThread(callback, userData)` | Runs now on the game thread, queues otherwise. |

Logging:

```cpp
RC::Output::send<RC::Output::LogLevel::Info>("Loaded {} v{}", name, version);
Avalonium::LogInfo("value {}", 42);
Avalonium::LogWarning("Object '{}' was not found", path);
Avalonium::LogError("call failed");
Avalonium::Log(Avalonium::LogLevel::Debug, "explicit level");
```

### Values

```cpp
RC::Unreal::Value value = RC::Unreal::Value(42);
int32_t number = value.As<int32_t>();

RC::Unreal::Value object = RC::Unreal::Value::Object(actor);
RC::Unreal::Value block = RC::Unreal::Value::Struct(&bytes, sizeof(bytes));
RC::Unreal::Value none;              // kind NONE
int32_t kind = none.Kind();
bool empty = none.IsNone();
```

`Value` carries one typed value across the boundary. `FString` wraps a UTF-8
string (`ToString`, `c_str`, `Len`, `empty`); `FName` carries text plus an
instance number (`ToString`, `Text`, `GetComparisonIndex`, `GetNumber`). STRING
and NAME values own their text. A STRUCT value built with `Value::Struct` owns a
copy of its bytes; a STRUCT returned by the runtime points into engine memory and
is valid until that memory changes.

### Objects and classes

```cpp
using namespace RC::Unreal;

UClass* actorClass = UObject::FindClass("Actor");
UObject* actor = actorClass ? UObject::FindFirstOfClass(actorClass) : nullptr;

UObject* object = UObject::FindObject("/Script/Engine.Default__Actor");

UObject* world = UObject::GetWorld();
UObject* note = UObject::Construct(UObject::FindClass("MyNoteObject"), world);
```

| Call | Result |
| :--- | :--- |
| `UObject::FindObject(path)` | Resolves a full object path, a package path, or a short name. |
| `UObject::FindClass(name)` | Resolves a class by short name, path, or Blueprint `_C` spelling. |
| `UObject::Construct(class, outer)` | Creates an instance outered to `outer` (the current world when null). Game thread only. |
| `UObject::GetWorld()` | The current `UWorld`, or null before one exists. |
| `UObject::FindFirstOfClass(class)` | First live instance of the class, not its class default object. |
| `RC::Unreal::FindFirstOf(className)` | Same, resolving the class by name. |
| `class->GetDefaultObject()` | The class default object. |
| `UObject::GetObjectsOfClass(class, vector)` | Live instances. |
| `UObject::GetObjectCount()`, `GetObjectByIndex(index)` | Walks the object array. |
| `IsValid()` | Readability test for the object pointer. |
| `GetName`, `GetFullName`, `GetPathName` | `FString` copies. |
| `GetClass`, `GetOuter`, `GetSuperStruct` | Class or outer pointers. |
| `IsA(name)` | Name-based class chain test. |

`UObject`, `UClass`, `UStruct` and `UFunction` are pointer facades. They hold no data,
are never constructed, and every method asks the runtime to read the running build.
The mod's `UObject*` is the engine object address. The free functions
`RC::Unreal::FindObject`, `FindClass` and `FindFirstOf` mirror the static calls.

### Properties

```cpp
FString name = object->GetProperty<FString>("Name");
bool bHidden = object->GetProperty<bool>("bHidden");
UObject* owner = object->GetProperty<UObject*>("Owner");

object->SetProperty("bHidden", true);
object->SetProperty("Name", FString("Renamed"));

RC::Unreal::Value raw;
object->GetPropertyValue("SomeStruct", raw);
object->SetPropertyValue("SomeStruct", raw);
```

Supported value types:

| Property type | Read | Write |
| :--- | :--- | :--- |
| `BoolProperty` | bool | bool or numeric |
| `Int8/16/32/64`, `UInt16/32/64`, `Byte` | integer | integer |
| `FloatProperty`, `DoubleProperty` | float or double | float or double |
| `EnumProperty` | integer, width from reflection | integer |
| `NameProperty` | `FName` | `FName` or text |
| `StrProperty`, `Utf8StrProperty`, `AnsiStrProperty` | `FString` | `FString` or text |
| `ObjectProperty`, `ClassProperty`, `InterfaceProperty` | `UObject*` | `UObject*` |
| `StructProperty`, `TextProperty` | raw byte view | raw byte copy of the exact size |
| `ArrayProperty`, `MapProperty`, `SetProperty` | typed access, see below | typed access, see below |
| Soft, weak, lazy, delegate | raw byte view | raw byte copy of the exact size |

Raw values are copied with `GetStructProperty(name, out, size)` /
`SetStructProperty(name, data, size)` or `Value::Struct`. A returned raw view
points into engine memory and is valid only until the engine changes it. `Value`
owns its text for STRING and NAME values; `FString` and `FName` are the text
handles the API crosses with.

### Reflected calls

```cpp
int32 value = object->Call<int32>("GetSomeInt");
FString text = object->Call<FString>("GetSomeString", 1, FString("input"));
object->Call("SetSomething", true);
```

Arguments are positional and matched to the function's parameter properties in
declaration order. Missing trailing arguments keep the frame's zero. One return
value is read back; out parameters other than the return value are not returned.

`CallValues` takes the same arguments as `Value`s, which is how structs, objects,
names and strings are passed:

```cpp
RC::Unreal::Value result;
std::vector<RC::Unreal::Value> args{
    RC::Unreal::Value::Object(world),
    RC::Unreal::Value::Struct(&transform, sizeof(transform))};
object->CallValues("SpawnSomething", args, &result);
```

`ProcessEvent(UFunction*, void*)` is available for mods that build an engine-layout
parameter block themselves. The runtime performs no translation on that buffer.

Blueprint functions and properties keep the engine's own spelling, which may
contain spaces. A generated SDK may sanitize the identifier for C++
(`Enable Player Control` becomes `Enable_Player_Control`); lookups and reflected
calls take the engine name.

### Data tables

`RC::Unreal::GetDataTableRow` returns the live row buffer for a row name, or null:

```cpp
using namespace RC::Unreal;

UObject* table = UObject::FindObject("/Game/Project/Items/ItemTable.ItemTable");
void* row = RC::Unreal::GetDataTableRow(table, "Shotgun");
```

The layout of that buffer is the table's `RowStruct`, so the row is read through
the struct's own reflected properties. `UStruct::GetProperties()` returns each
member's name, type, offset and size:

```cpp
UObject* rowStruct = table->GetProperty<UObject*>("RowStruct");
for (const RC::Unreal::PropertyInfo& property :
     static_cast<RC::Unreal::UStruct*>(rowStruct)->GetProperties())
{
    void* field = static_cast<uint8_t*>(row) + property.Offset;
    // property.TypeName tells the member type; property.ElementSize its size
}
```

- Game thread only. The buffer points into table-owned storage, is read-only, and
  stays valid while the table owns the row. Do not keep pointers into it across
  level changes or table reloads.
- Row names are the engine FName text. The runtime also accepts a name the engine
  renders with an instance suffix: `Foo_0` resolves the pool name `Foo` with
  instance number 1, which is how duplicate FNames are displayed.
- A row name that names no row returns null. A member with no value reads as a
  null object pointer or zero bytes.

Example: draw the first icon-like object member of a row.

```cpp
RC::Unreal::UObject* FindRowIcon(RC::Unreal::UObject* table, const char* rowName)
{
    void* row = RC::Unreal::GetDataTableRow(table, rowName);
    if (!row) return nullptr;

    auto lower = [](std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return text;
    };

    RC::Unreal::UObject* rowStruct = table->GetProperty<RC::Unreal::UObject*>("RowStruct");
    for (const RC::Unreal::PropertyInfo& property :
         static_cast<RC::Unreal::UStruct*>(rowStruct)->GetProperties())
    {
        if (lower(property.Name).find("icon") == std::string::npos) continue;
        if (lower(property.TypeName).find("object") == std::string::npos) continue;

        void* icon = nullptr;
        std::memcpy(&icon, static_cast<uint8_t*>(row) + property.Offset, sizeof(icon));
        auto* object = static_cast<RC::Unreal::UObject*>(icon);
        if (object && object->IsValid() && object->IsA("Texture2D")) return object;
    }
    return nullptr;
}
```

### Widget events

```cpp
uint32_t binding = RC::Widgets::BindEvent(slider, "OnValueChanged", &MyMod::OnSlider, this);
RC::Widgets::BindClick(button, &MyMod::OnClicked, this);
RC::Widgets::Unbind(binding);
```

`BindEvent` binds any inline delegate property the widget carries by name.
Multicast events include `OnClicked`, `OnPressed`, `OnReleased`, `OnHovered`,
`OnUnhovered`, `OnValueChanged`, `OnCheckStateChanged`, `OnTextChanged`,
`OnTextCommitted`, `OnSelectionChanged`, and so on; single-cast delegates are
found the same way. A widget and event accepts any number of callbacks, from
one mod or several, and every binding is unbound on its own. The callback runs
on the game thread when the engine broadcasts the delegate and carries no
arguments; read the widget's state from it. Sparse multicast delegates keep
their list in a side registry and are not bound.

The runtime appends one dispatch entry per event, naming a delegate signature
function the object's class already declares. That name is what tells the
runtime which event fired, so several events on one widget stay distinct and no
fixed function is required. A single-cast delegate holds one value: the binding
saves the handler already there, dispatches it first on every call, then runs
the callbacks. Bind on the game thread. An unbind from the unload thread marks
the binding inactive; a multicast entry left behind dispatches nothing, and a
single-cast value is restored in place. `BindClick` is the `OnClicked` shortcut
and `UnbindClick` is an alias of `Unbind`.

A constructed panel is only reachable when the game lets the cursor reach it.
While the panel is open a mod calls `SetInputMode_GameAndUIEx` or
`SetInputMode_UIOnlyEx` on `WidgetBlueprintLibrary`, focuses the panel and sets
`bShowMouseCursor`; hiding the panel restores `SetInputMode_GameOnly`. GameAndUI
keeps gameplay input alive, so `SetIgnoreLookInput(true)` stops the camera
turning under the mouse. A title can capture the mouse on a click and clear
`bShowMouseCursor` even in UI-only mode; write the flag again from the widget
callbacks so the cursor returns after each interaction.

### Widget kit

Every native UMG widget class is constructible by name (`TextBlock`, `Button`,
`Slider`, `CheckBox`, `EditableTextBox`, `Image`, `Border`, the panel types,
scroll and list views, and the rest) and composed through `AddChild` and
`AddToCanvas`. `RC::Widgets` also carries the small builders a panel needs:

| Call | Result |
| :--- | :--- |
| `New(className, outer)` | Constructs a widget (or any object) from an engine class. |
| `AddChild(panel, child)` | Adds content to a Button, SizeBox, Overlay, and other single-slot panels. |
| `AddToCanvas(canvas, widget, x, y, w, h)` | Places and sizes a child in a CanvasPanel. |
| `SetText(widget, text)` | Builds FText through `KismetTextLibrary` and calls `SetText`. |
| `SetVisible(widget, visible)` | Visible or collapsed. |
| `CopyStruct(from, fromProperty, to, toProperty)` | Copies a font, brush or other struct property between widgets of the same kind. |
| `BindEvent`, `BindClick`, `Unbind` | Delegate bindings. |

### ImGui

The overlay owns one ImGui context and builds its frame on the render thread.
A C++ mod draws through it with the full ImGui API:

```cpp
#include <Avalonium/ImGuiHook.hpp>

static void Render(void* user)
{
    if (!RC::ImGui::BeginFrame()) return;
    if (ImGui::Begin("My mod")) ImGui::TextUnformatted("hello");
    ImGui::End();
}

uint32_t id = RC::ImGui::RegisterRender("MyMod", &Render, this);
RC::ImGui::SetInputActive(id, true);   // windows visible: take mouse and keyboard
RC::ImGui::UnregisterRender(id);
```

- Compile the runtime's own ImGui sources into the mod (`Helpers/Universal-Dear-ImGui-Hook/imgui`, version 1.92.0 WIP) with the same `imconfig`. The context crosses the DLL boundary, so a different version or build configuration is not layout-compatible.
- `BeginFrame()` points the mod's ImGui copy at the runtime's context and allocator functions. Call it first in every render callback.
- The callback runs on the render thread: build UI only, and keep engine reflection and property access on the game thread.
- Do not call `NewFrame`, `EndFrame` or `Render` in the callback; the overlay owns the frame.
- `SetInputActive(id, true)` while the windows are visible makes the overlay free the
  cursor and route mouse and keyboard input to ImGui instead of the game. Call it
  with `false` when the windows hide.

`RC::ImGui::Context()` and `RC::ImGui::Available()` report the shared context;
`GetAllocatorFunctions` exposes the runtime allocators for advanced use.

### Textures

`RC::ImGui::TextureId` maps a live `UTexture2D` to the texture view the overlay
renderer draws with:

```cpp
uint64_t id = RC::ImGui::TextureId(texture);
if (id) ImGui::Image(static_cast<ImTextureID>(id), ImVec2(64.0f, 64.0f));
```

- Render thread. The runtime resolves the view once per texture and caches it; the
  returned value stays valid while the overlay lives.
- On D3D11 the value is an `ID3D11ShaderResourceView*`. On D3D12 it is a GPU
  descriptor handle in the overlay's shader-visible SRV heap. Both are consumed by
  the runtime's ImGui context.
- 0 means no view: the pointer is not a live `Texture2D`, the overlay backend is
  neither D3D11 nor D3D12, or the engine has no render resource for that texture.
- The value is backend data for the runtime's ImGui context. Use it inside the
  runtime's frame, not as a handle for any other renderer.
- `ImGui::Image` converts coordinates. A custom draw-list call needs screen
  coordinates, so take the position from `ImGui::GetCursorScreenPos()` rather than
  `GetCursorPos()`.
- Icons are not always square. Read the source size with the reflected
  `Blueprint_GetSizeX` and `Blueprint_GetSizeY` calls on the game thread, then fit
  the draw rect to keep the aspect ratio:

```cpp
int32_t width = texture->Call<int32_t>("Blueprint_GetSizeX");
int32_t height = texture->Call<int32_t>("Blueprint_GetSizeY");
float scale = 64.0f / std::max(width, height);
```

- The API reads textures the game already loaded. It does not create, upload or
  modify texture assets.

### Arrays

```cpp
int32_t count = object->GetArrayLength("PendingMapChangeLevelNames");
int32_t elementSize = object->GetArrayElementSize("PendingMapChangeLevelNames");
RC::Unreal::Value element;
object->GetArrayElement("PendingMapChangeLevelNames", 0, element);

object->AddArrayElement("PendingMapChangeLevelNames",
                        RC::Unreal::Value(RC::Unreal::FName("Test")));
object->SetArrayElement("PendingMapChangeLevelNames", 0, element);
object->RemoveArrayElement("PendingMapChangeLevelNames", 0);
```

Elements are read and written through the array's own element property, so
numbers, names, strings, objects and structs all work. Growth uses geometric
capacity and removal shifts the tail down. Game thread only; the length is `-1`
when the property does not resolve.

### Maps and sets

```cpp
int32_t entries = object->GetMapLength("SomeMap");
RC::Unreal::Value key;
RC::Unreal::Value value;
object->GetMapEntry("SomeMap", 0, key, value);

RC::Unreal::Value recorded;
object->FindMapValue("SomeMap", RC::Unreal::Value(RC::Unreal::FName("Key")), recorded);
object->AddMapEntry("SomeMap", RC::Unreal::Value(RC::Unreal::FName("Key")),
                    RC::Unreal::Value(42));
object->RemoveMapEntry("SomeMap", RC::Unreal::Value(RC::Unreal::FName("Key")));
object->ClearMap("SomeMap");
bool hasKey = object->ContainsMapKey("SomeMap", key);

int32_t elements = object->GetSetLength("SomeSet");
RC::Unreal::Value element;
object->GetSetElement("SomeSet", 0, element);
object->AddSetElement("SomeSet", RC::Unreal::Value(RC::Unreal::FName("Flag")));
object->RemoveSetElement("SomeSet", RC::Unreal::Value(RC::Unreal::FName("Flag")));
object->ClearSet("SomeSet");
bool member = object->ContainsSetElement("SomeSet", element);
```

Entries are addressed by their position among the live elements (`0 ..
length-1`). Mutation runs through the engine's own `BlueprintMapLibrary` and
`BlueprintSetLibrary` entries, so growth, rehashing and element lifetime are the
engine's. `AddMapEntry` overwrites the value of a key that is already present.
`FindMapValue` and the remove/contains calls compare keys through their
reflected type: numbers, booleans, text, names, objects and struct bytes. Game
thread only.

### Nested properties

```cpp
RC::Unreal::Value speed;
object->GetPropertyPath("Stats.Speed", speed);
object->SetPropertyPath("Stats.Speed", RC::Unreal::Value(12.5f));

RC::Unreal::Value label;
object->GetPropertyPath("Items[2].Name", label);
object->GetPropertyPath("Lookup{42}.Label", label);
```

A path starts with a property on the object's class, then walks one step at a
time: `.field` enters a struct field or a field of a referenced object, `[index]`
selects an array element, `{key}` selects a map value by key and `{index}`
selects a set element by position. Numeric, boolean, text, name and object keys
are accepted inside braces. Game thread only.

### Native functions

Reflection covers UFUNCTIONs and properties. Functions that are neither are
found by address and called with the signature the mod declares:

```cpp
void* module = RC::Native::Module();  // main executable

uint64_t pattern = RC::Native::FindPattern("48 89 5C 24 ?? 57 48 83 EC 20");
if (pattern)
{
    using Fn = void(__fastcall*)(void*);
    RC::Native::Call<void>(pattern, actor);
}

void* exported = RC::Native::FindExport("SomeFunction");
void* slot = RC::Native::GetVirtualFunction(object, 0x2A);
RC::Native::CallVirtual<void>(object, 0x2A, argument);
```

`FindExport` resolves a loaded module's exported symbol. `FindPattern` scans an
IDA-style byte pattern, executable sections only by default; pass the whole
module when data is part of the pattern. `ResolveRelative` turns a RIP-relative
operand into its target address. `GetVtable` and `GetVirtualFunction` read a
vtable slot.

Resolution returns an address and nothing else. A wrong pattern, slot or
signature faults the process, so validate the result and call on the game
thread. Patterns describe one build; the runtime holds none of them.

### Visuals and animation

Fonts, brushes and widget styles are struct properties: `CopyStruct` moves one
from an existing widget of the same kind onto a constructed one. Existing
textures, fonts and materials are reachable through `UObject::FindObject` and
object properties.

Widget animations are ordinary objects in the widget class's `Animations` array.
Read one with the array API and hand it to `PlayAnimation`; an animation binds to
widgets by name, so it has a visible effect on a widget whose tree carries those
names.

Constructing a texture from image bytes is not exposed: `UTexture2D`'s creation
functions are C++ only, so mods reuse the game's existing assets.

### Function information

```cpp
UFunction* function = objectClass->FindFunction("PrintString");
uint32_t flags = function->GetFunctionFlags();
int32_t frameSize = function->GetParametersSize();
bool native = function->IsNative();
```

`UStruct::GetProperties()` returns `PropertyInfo` entries with the property name,
type name, offset, element size, array dimension and flags.
`UClass::GetFunctions()` returns `FunctionInfo` entries with the name, `UFunction`
pointer, flags and frame size.

---

## 4. UFunction hooks

```cpp
using namespace RC;

uint32_t hookId = Hook::Register(
    "GameplayStatics", "GetPlayerController",
    &MyMod::PreGetPlayerController, &MyMod::PostGetPlayerController, this);
```

`Hook::Register` accepts native and interpreted UFunctions. A native function keeps
the native-entry hook described below. An interpreted (Blueprint bytecode) function
is answered by the runtime's `ProcessLocalScriptFunction` hook: pre callbacks run
before the VM executes it, post callbacks after it. `Hook::RegisterNative` is the
historical spelling and behaves the same.

When the mod already holds a `UFunction*`, `Hook::RegisterFunction(function, ...)`
registers it without a class or function name lookup. This reaches functions whose
class is not present in the runtime's name index, which is the case for some classes
created or loaded after discovery.

Callbacks receive the engine object, UFunction, argument frame and result storage:

```cpp
static void PreGetPlayerController(void* user, void* object, void* function, void* frame, void* result)
{
    Hook::Context context{
        static_cast<Unreal::UObject*>(object),
        static_cast<Unreal::UFunction*>(function),
        frame, result};

    int32_t playerIndex = context.GetArg<int32_t>("PlayerIndex");
}
```

| Member | Meaning |
| :--- | :--- |
| `GetArg<T>(parameterName)` | Reads an argument by the function's own parameter name. |
| `SetArg<T>(parameterName, value)` | Writes the argument frame before the original runs. |
| `GetResult<T>()` | Reads the return value in a post callback. |

For a native function the runtime patches its native entry through `InlineHook`, so
the hook observes reflected calls and direct calls through that entry. For an
interpreted function the interpreter hook carries the call, so both routes into
Blueprint bytecode are observed. Both hooks identify the UFunction from the engine
frame, not from a per-hook signature.

Hook limits:

- Native-entry hooks cover native UFunctions. Interpreted UFunctions are answered by
  the `ProcessLocalScriptFunction` hook; if that entry cannot be installed the
  runtime logs a warning and Blueprint hooks cannot fire.
- Shared VM entries (`ProcessEvent`, `ProcessInternal`,
  `ProcessLocalScriptFunction`) cannot be hooked as ordinary native functions.
- At most 128 unique native entry points can be hooked during one process lifetime.
  Blueprint hooks do not consume those slots.
- Out parameters reach a post callback through `GetArg`, but the API does not write
  them back through the caller's out-parameter list.
- A Blueprint hook observes the function the interpreter actually executes. A base
  BlueprintImplementableEvent that a class does not implement has no bytecode, so the
  engine returns before the interpreter runs it; register the hook on the class that
  implements the event.
- A Blueprint hook is filed against a stable function identity: full function path,
  function flags and parameter frame size. A recompiled or reinstanced UFunction with
  the same identity still matches; a changed path or signature requires
  re-registration.

```cpp
RC::Hook::Unregister(hookId);
```

Unregister in `on_unload` or when the hook is no longer needed. The runtime also
removes a mod's hooks before unloading its DLL.

---

## 5. Key binds

```cpp
uint32_t keyId = RC::Keys::Register("F8", &MyMod::OnKey, this);
RC::Keys::Unregister(keyId);
```

The bind uses the same key names and modifier spellings as the Lua API (`F8`,
`ctrl+F8`, `alt+shift+O`). The callback runs on the input thread; queue engine
work with `RC::RunOnGameThread`. Log lines from mods appear in the runtime log
under the `C++ MOD RUNTIME` section.

---

## 6. Engine version independence

The API deliberately exposes no engine structure layout:

- `FName` is a text and number handle. The runtime writes it at the width its own
  discovery measured.
- `FString` is a UTF-8 string value, not the engine's `TArray<TCHAR>`.
- Property offsets, element sizes and container metadata come from the property
  itself.
- A data table row is found through the table's reflected `RowStruct`: the runtime
  reads the row map that follows it and compares FName indices and instance
  numbers, not rendered text.
- A texture view is resolved from the live object graph and the active D3D11 or
  D3D12 device, not from engine offsets.
- `UFunction::Func`, `UFunction` flags and the FFrame layout are discovered or taken
  from constants that are identical across the supported engine trees.
- Class and function lookup is by name and reflection, never by a hardcoded address.

A raw `ProcessEvent` caller is the exception: it owns the parameter block layout and
therefore owns the version-specific structure.

---

## 7. Limitations

- If the engine's ProcessEvent entry cannot be hooked, lifecycle callbacks run on the
  loader thread and `on_update` is unavailable. If `ProcessLocalScriptFunction` cannot
  be hooked, interpreted UFunction hooks cannot fire.
- Blueprint hooks are filed against the live UFunction; a Blueprint recompile that
  replaces it requires registering the hook again.
- Delegate callbacks carry no arguments; read the object's state in the callback.
  A delegate binding uses a dispatch name taken from a delegate signature function
  on the object's class, so a class with no delegate signature function on its chain
  cannot be bound. A single-cast delegate holds one value: a game rebind while the
  binding is active replaces the runtime's entry and the callback stops.
- `TArray` is typed, and maps and sets are addressed by live-entry position.
  Map and set mutation uses the engine's Blueprint container libraries. Struct
  keys are compared by their raw bytes for lookup, so a padding difference can
  hide a match, and a map key cannot be renamed in place.
- Reflected calls return one value; out parameters are not returned.
- No standalone FText value constructor. `RC::Widgets::SetText` builds one through
  the engine; text properties can otherwise be copied as raw bytes.
- New widget Blueprint classes and new texture assets cannot be created. Mods compose
  the engine's native widget classes and reuse existing textures, fonts and brushes.
- Texture views cover D3D11 and D3D12 textures the engine already prepared for
  rendering. A texture without a render resource, or an overlay backend other than
  D3D11 or D3D12, returns 0.
- Data table rows are live table-owned buffers. Read them only; the API does not
  copy or write them.
- Constructed objects are not GC-rooted by the runtime. Attach them to a live outer,
  property or viewport to keep them alive.
- C++ ImGui uses the runtime's ImGui version and build configuration; a mod
  compiled against another ImGui version is not context-compatible.
- No Lua interop or mod hot reload.
- `on_unload` is skipped at process exit. Hook and key-bind registrations are removed
  before the process leaves.
- Mod DLLs must be x64 MSVC builds compatible with the runtime's STL ABI.
- C++ mod binaries from other runtimes (UE4SS, for example) are not compatible and
  are rejected before `start_mod`. Port the source to this SDK and recompile.
- A mod that registers a hook or key bind outside a mod callback owns that
  registration for the life of the process.

