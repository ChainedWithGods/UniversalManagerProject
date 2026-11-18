# CppAPI Examples: What a Native Avalonium Mod Can Do

These examples build on the [tutorial](TUTORIAL.md). Each one shows a capability
that comes from the runtime itself: live reflection, the overlay texture bridge,
DataTable access, UFunction hooks and pak-backed mod detection. None of them uses
a generated SDK, a mappings file, a game name or an engine offset.

The same compiled DLL targets UE 4.23 through 5.8.

---

## 1. Texture Browser

List the game's textures and draw them in ImGui. This is the feature UE4SS does
not offer: the runtime resolves a live `UTexture2D` to a D3D11 shader resource
view or a D3D12 GPU descriptor and hands it to the overlay.

```cpp
#include <Avalonium/Avalonium.hpp>
#include <Avalonium/ImGuiHook.hpp>

class TextureBrowser final : public RC::CppUserModBase
{
  public:
    void on_unreal_init() override
    {
        m_Render = RC::ImGui::RegisterRender("TextureBrowser", &Render, this);
    }

    void on_update() override
    {
        if (!m_Textures.empty()) return;
        RC::Unreal::UClass* cls = RC::Unreal::UObject::FindClass("Texture2D");
        if (!cls) return;

        std::vector<RC::Unreal::UObject*> objects;
        RC::Unreal::UObject::GetObjectsOfClass(cls, objects, 4096);
        for (RC::Unreal::UObject* object : objects)
        {
            if (!object || !object->IsValid()) continue;
            const std::string path = object->GetPathName().ToString();
            if (path.find("None:") != std::string::npos) continue;
            m_Textures.push_back(object);
            if (m_Textures.size() >= 32) break;
        }
    }

    void on_unload() override
    {
        if (m_Render) RC::ImGui::UnregisterRender(m_Render);
    }

  private:
    static void Render(void* user)
    {
        auto* self = static_cast<TextureBrowser*>(user);
        if (!RC::ImGui::BeginFrame()) return;
        if (ImGui::Begin("Texture Browser"))
        {
            for (RC::Unreal::UObject* texture : self->m_Textures)
            {
                const uint64_t view = RC::ImGui::TextureId(texture);
                if (!view) continue;
                ImGui::TextUnformatted(texture->GetName().ToString().c_str());
                ImGui::SameLine(360.0f);
                ImGui::Image(static_cast<ImTextureID>(view), ImVec2(96.0f, 96.0f));
            }
        }
        ImGui::End();
    }

    uint32_t m_Render = 0;
    std::vector<RC::Unreal::UObject*> m_Textures;
};
```

Why it matters:

- The texture view works on D3D11 and D3D12 overlays with the same code.
- The runtime caches the view per texture and keeps it alive.
- Textures the game never displayed can keep low mips. Force the full chain once
  with `NeverStream` and `UpdateResource`, both shown in the tutorial.
- Icons are not square. Read `Blueprint_GetSizeX` and `Blueprint_GetSizeY` on the
  game thread and fit the rect.

---

## 2. DataTable Explorer

Enumerate every row through the runtime's row API, then read the row buffer
through the table's own `RowStruct`. No generated headers and no layout offsets
in the mod.

```cpp
void DumpRows(RC::Unreal::UObject* table)
{
    const int32_t count = RC::Unreal::GetDataTableRowCount(table);
    if (count <= 0) return;

    RC::Unreal::UObject* rowStruct = table->GetProperty<RC::Unreal::UObject*>("RowStruct");
    const auto properties =
        static_cast<RC::Unreal::UStruct*>(rowStruct)->GetProperties();

    for (int32_t i = 0; i < count; ++i)
    {
        void* row = nullptr;
        uint32_t nameIndex = 0;
        uint32_t number = 0;
        if (!RC::Unreal::GetDataTableRowAt(table, i, &row, nameIndex, number)) continue;

        std::string name;
        if (!RC::Unreal::GetDataTableRowName(table, i, name)) continue;

        for (const RC::Unreal::PropertyInfo& property : properties)
        {
            void* field = static_cast<uint8_t*>(row) + property.Offset;
            // property.Name, property.TypeName and property.ElementSize tell
            // you which reader to use.
        }
    }
}

// Every row name at once, in map order.
void ListRows(RC::Unreal::UObject* table)
{
    for (const std::string& name : RC::Unreal::GetDataTableRowNames(table))
    {
        void* row = RC::Unreal::GetDataTableRow(table, name);
        // row is the live buffer for this name
    }
}
```

Why it matters:

- The runtime resolves the table's row map from the engine's own reflection and
  confirms it against every live row before returning anything. The mod carries
  no offset and no stride.
- Rows are live table storage. Read them, do not write them.
- A rendered name can carry an instance number (`Foo_0` is `Foo` with number 1).
  Anything handed back to the game as an FName needs the key pair from
  `GetDataTableRowAt`, not just the text.
- Row icon fields are object properties with "icon" in the name. Scan for them
  instead of hardcoding one field name.
- A row name can exist in several tables. Try each table until the row contains
  the field you need.

---

## 3. Live Object Inspector

Search any class, list its live objects and edit simple properties. This is plain
runtime reflection, so it works in every supported game without an SDK.

```cpp
void ShowObjects(RC::Unreal::UClass* cls, const std::vector<RC::Unreal::UObject*>& objects,
                 RC::Unreal::UObject*& selected)
{
    for (RC::Unreal::UObject* object : objects)
    {
        if (!object || !object->IsValid()) continue;
        if (ImGui::Selectable(object->GetName().ToString().c_str()))
            selected = object;
    }
}

void ShowProperties(RC::Unreal::UObject* object)
{
    if (!object) return;
    for (const RC::Unreal::PropertyInfo& property :
         object->GetClass()->GetProperties())
    {
        RC::Unreal::Value value;
        if (!object->GetPropertyValue(property.Name, value)) continue;

        switch (value.Kind())
        {
        case AVALONIUM_VALUE_BOOL:
        {
            bool flag = value.As<bool>();
            if (ImGui::Checkbox(property.Name.c_str(), &flag))
                object->SetProperty(property.Name, flag);
            break;
        }
        case AVALONIUM_VALUE_INT32:
        {
            int32_t number = value.As<int32_t>();
            if (ImGui::DragInt(property.Name.c_str(), &number))
                object->SetProperty(property.Name, number);
            break;
        }
        case AVALONIUM_VALUE_FLOAT:
        {
            float number = value.As<float>();
            if (ImGui::DragFloat(property.Name.c_str(), &number))
                object->SetProperty(property.Name, number);
            break;
        }
        default:
            ImGui::Text("%s = %s", property.Name.c_str(), value.AsString().c_str());
            break;
        }
    }
}
```

Why it matters:

- Property names keep the engine spelling, spaces included. `Health Max` works,
  `Health_Max` does not.
- Writes are game-thread work. Run them from `on_update` or queue them with a flag.
- The same code edits any class in any supported game. There is no rebuild per
  game and no generated header.

---

## 4. Actor HUD Projection

Draw a HUD marker over world actors. The teaching point is reading an out
parameter with the frame API, which `CallValues` cannot return.

```cpp
bool ProjectToScreen(RC::Unreal::UObject* controller, const Vec3& world,
                     float& outX, float& outY)
{
    const AvaloniumModApi* api = Avalonium::Detail::Api();
    RC::Unreal::UFunction* fn =
        controller->FindFunction("ProjectWorldLocationToScreen");
    if (!api || !fn) return false;

    const int32_t size = (std::max)(fn->GetParametersSize(), 0x40);
    std::vector<uint8_t> frame(static_cast<size_t>((size + 15) & ~15), 0);
    RC::Unreal::Value location =
        RC::Unreal::Value::Struct(&world, static_cast<int32_t>(sizeof(world)));
    if (!api->set_function_argument(fn->Address(), frame.data(), "WorldLocation",
                                    &location.Raw()))
        return false;
    if (!api->process_event(controller->Address(), fn->Address(), frame.data()))
        return false;

    RC::Unreal::Value screen;
    if (!api->get_function_argument(fn->Address(), frame.data(), "ScreenLocation",
                                    &screen.RawMutable()))
        return false;

    // FVector2D is two floats on UE4 and two doubles on UE5 LWC builds.
    const void* data = screen.AsStruct();
    const int32_t bytes = screen.StructSize();
    if (!data || bytes < 8) return false;
    if (bytes >= 16)
    {
        outX = static_cast<float>(*static_cast<const double*>(data));
        outY = static_cast<float>(*reinterpret_cast<const double*>(
            static_cast<const uint8_t*>(data) + 8));
    }
    else
    {
        outX = *static_cast<const float*>(data);
        outY = *(static_cast<const float*>(data) + 1);
    }
    return true;
}
```

Why it matters:

- Out parameters are written into the frame and read back by parameter name.
- Struct sizes come from reflection, so the same code survives the UE5 double
  precision change.
- Markers are drawn on the ImGui draw list with screen coordinates from
  `GetCursorScreenPos()`.

---

## 5. UFunction Hook Monitor

Watch any UFunction and log its arguments, without a generated SDK.

```cpp
struct HookMonitor
{
    uint32_t Hook = 0;
    std::mutex Mutex;
    std::deque<std::string> Lines;
};

void __cdecl PreHook(void* userData, void* object, void* function, void* frame,
                     void* result)
{
    auto* self = static_cast<HookMonitor*>(userData);
    RC::Hook::Context context;
    context.Object = static_cast<RC::Unreal::UObject*>(object);
    context.Function = static_cast<RC::Unreal::UFunction*>(function);
    context.Frame = frame;
    context.Result = result;

    const std::string functionName = context.Function->GetName().ToString();
    const std::string objectName = context.Object->GetFullName().ToString();
    const int32_t count = context.GetArg<int32_t>("Num");

    std::lock_guard<std::mutex> lock(self->Mutex);
    self->Lines.push_back(functionName + " (" + std::to_string(count) + ") on " +
                          objectName);
    if (self->Lines.size() > 200) self->Lines.pop_front();
}

// on_unreal_init
m_Monitor.Hook = RC::Hook::Register("SomeClass", "SomeFunction", &PreHook, nullptr,
                                    &m_Monitor);

// on_unload
RC::Hook::Unregister(m_Monitor.Hook);
```

Read arguments by the function's own parameter names inside the callback:

```cpp
int32_t count = context.GetArg<int32_t>("Num");
float damage = context.GetArg<float>("Damage");
RC::Unreal::UObject* target =
    context.GetArg<RC::Unreal::UObject*>("Target");
```

Why it matters:

- Native and interpreted functions use the same registration.
- Arguments are copied out of the engine frame by name.
- The callback runs on the thread that called the function. Copy what you need and
  let the render thread draw it.

---

## 6. Destruction Guard

Some games spawn an actor and destroy it again during its own initialization. A
mod that needs the actor can veto that destroy for the duration of the spawn.

```cpp
class MyMod final : public RC::CppUserModBase
{
  public:
    void on_unreal_init() override
    {
        uint32_t id = RC::Hook::RegisterVeto("Actor", "K2_DestroyActor",
                                             &MyMod::OnDestroy, this);
        if (!id) LogError("destroy veto unavailable");
    }

    bool SpawnAndKeep(RC::Unreal::UObject* state, std::vector<RC::Unreal::Value>& args)
    {
        std::vector<RC::Unreal::Value> out;
        spawning = true;
        const bool ok = state->CallOutValues("Spawn Item Ex", args, {"act"}, out);
        spawning = false;
        if (!ok || out.empty()) return false;

        auto* actor = static_cast<RC::Unreal::UObject*>(out.front().AsObject());
        return actor && !actor->IsPendingKill();
    }

  private:
    static bool OnDestroy(void* user, void* object, void* function, void* frame, void* result)
    {
        auto* mod = static_cast<MyMod*>(user);
        return !mod->spawning;
    }

    bool spawning = false;
};
```

`RegisterVeto` blocks a UFunction call. `RegisterAddressVeto` does the same for a
function that never passes through a UFunction entry; resolve its address from the
module for the engine build you target.

Why it matters:

- The out value tells you what the spawn actually produced.
- `IsPendingKill` tells you whether the game already scheduled the actor for
  destruction, before any other call silently does nothing.
- The veto only blocks the game's cleanup while the mod holds the spawn window.

---

## 7. One DLL, Every Engine Version

A single binary can report and adapt to the engine it runs on:

```cpp
void Render(void*)
{
    if (!RC::ImGui::BeginFrame()) return;
    if (ImGui::Begin("Runtime"))
    {
        ImGui::Text("Engine: UE%d.%d", Avalonium::EngineMajor(),
                    Avalonium::EngineMinor());
        ImGui::Text("Runtime: %s", std::string(Avalonium::RuntimeVersion()).c_str());
        ImGui::Text("Objects: %d", RC::Unreal::UObject::GetObjectCount());
    }
    ImGui::End();
}
```

Useful branches:

- `Blueprint_GetSizeX/Y` and any reflected call resolve by name on the live class,
  so they do not change per version.
- Struct and FName widths come from the runtime. Do not hardcode offsets.
- A texture view is resolved from the live object graph on D3D11 or D3D12.

Why it matters:

- This is the difference from a mappings-based workflow. There is no per-game
  dump, no generated SDK rebuild, and no engine switch. The same mod DLL runs on
  every supported tree, and every example above inherits that.

---

## 8. Mod Content Detection

Local paks mount into the game's own asset paths, so a package path cannot tell a
re-skin from base content. `IsModAsset` checks the pak set instead:

```cpp
struct Row
{
    std::string Name;
    RC::Unreal::UObject* Icon = nullptr;
    bool IsMod = false;
};

void MarkRows(std::vector<Row>& rows)
{
    for (Row& row : rows)
    {
        row.IsMod = row.Icon && row.Icon->IsModAsset();
    }
}
```

The runtime scans `<game>/Content/Paks/~mods` once and caches the package set.
Both a package a mod adds and a game package a mod pak overrides match, so a base
weapon whose icon and materials come from a skin pak is marked correctly.

Useful branches:

- Check the item's own object first; content that mounts under `/Game/Mod/...`
  is caught by the path alone.
- Check the displayed assets next. A row can be base content while its icon,
  mesh or material is overridden.
- Call it after the first level load or during a roster scan; the first call
  builds the index and every later call is a set lookup.
- An encrypted pak index is skipped, and a mount added after the first call is
  not in the set.

Why it matters:

- A path test only sees mods that chose a mod-specific mount point. The pak scan
  sees every local pak the game loads, including asset overrides, without a game
  name or a hardcoded mount list.

---

## SDK-generated mods

Every example above uses reflection so the same DLL works across games. When you
write gameplay logic against one game's classes, the generated SDK gives typed
members and function wrappers with out parameters as references. That workflow,
its build layout and a full example are in [SDK_MODS.md](SDK_MODS.md).
