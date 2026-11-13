# CppAPI Examples: What a Native Avalonium Mod Can Do

These examples build on the [tutorial](TUTORIAL.md). Each one shows a capability
that comes from the runtime itself: live reflection, the overlay texture bridge,
DataTable access and UFunction hooks. None of them uses a generated SDK, a
mappings file, a game name or an engine offset.

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

Walk every DataTable row and read its fields through the table's own
`RowStruct`. No generated headers, no per-game struct definitions.

```cpp
std::string Utf16ToUtf8(const void* data, int32_t count)
{
    const wchar_t* chars = static_cast<const wchar_t*>(data);
    std::string out;
    for (int32_t i = 0; i < count; ++i)
    {
        uint32_t code = static_cast<uint16_t>(chars[i]);
        if (code >= 0xD800 && code <= 0xDBFF && i + 1 < count)
        {
            const uint32_t low = static_cast<uint16_t>(chars[i + 1]);
            if (low >= 0xDC00 && low <= 0xDFFF)
            {
                code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                ++i;
            }
        }
        if (code < 0x80) out += static_cast<char>(code);
        else if (code < 0x800)
        {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        else if (code < 0x10000)
        {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }
    return out;
}

void DumpRows(RC::Unreal::UObject* table)
{
    RC::Unreal::UFunction* names = table->FindFunction("GetDataTableRowNames");
    RC::Unreal::UClass* stringsClass =
        RC::Unreal::UObject::FindClass("KismetStringLibrary");
    RC::Unreal::UObject* strings =
        stringsClass ? stringsClass->GetDefaultObject() : nullptr;
    RC::Unreal::UFunction* conv =
        strings ? strings->FindFunction("Conv_NameToString") : nullptr;
    if (!names || !conv) return;

    int32_t frameSize = (std::max)(names->GetParametersSize(), 0x40);
    std::vector<uint8_t> frame(static_cast<size_t>((frameSize + 15) & ~15), 0);
    void* tablePointer = table;
    std::memcpy(frame.data(), &tablePointer, 8);
    if (!table->ProcessEvent(names, frame.data())) return;

    void* nameArray = nullptr;
    int32_t count = 0;
    std::memcpy(&nameArray, frame.data() + 0x8, 8);
    std::memcpy(&count, frame.data() + 0x10, 4);
    if (!nameArray || count <= 0) return;

    RC::Unreal::UObject* rowStruct =
        table->GetProperty<RC::Unreal::UObject*>("RowStruct");
    const auto properties =
        static_cast<RC::Unreal::UStruct*>(rowStruct)->GetProperties();

    for (int32_t i = 0; i < count; ++i)
    {
        const uint8_t* rawName = static_cast<const uint8_t*>(nameArray) + i * 8;
        std::string name;
        {
            std::vector<uint8_t> textFrame(0x40, 0);
            std::memcpy(textFrame.data(), rawName, 8);
            if (strings->ProcessEvent(conv, textFrame.data()))
            {
                void* text = nullptr;
                int32_t length = 0;
                std::memcpy(&text, textFrame.data() + 0x8, 8);
                std::memcpy(&length, textFrame.data() + 0x10, 4);
                if (text && length > 0) name = Utf16ToUtf8(text, length);
            }
        }

        void* row = RC::Unreal::GetDataTableRow(table, name.c_str());
        if (!row) continue;

        for (const RC::Unreal::PropertyInfo& property : properties)
        {
            void* field = static_cast<uint8_t*>(row) + property.Offset;
            // property.Name, property.TypeName and property.ElementSize tell
            // you which reader to use.
        }
    }
}
```

Why it matters:

- Rows are live table storage. Read them, do not write them.
- Names that render as `Foo_0` are FNames with an instance number. The runtime
  resolves both the rendered name and the base name, so pass what the table shows.
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

## 6. One DLL, Every Engine Version

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

## SDK-generated mods

Every example above uses reflection so the same DLL works across games. When you
write gameplay logic against one game's classes, the generated SDK gives typed
members and function wrappers with out parameters as references. That workflow,
its build layout and a full example are in [SDK_MODS.md](SDK_MODS.md).
