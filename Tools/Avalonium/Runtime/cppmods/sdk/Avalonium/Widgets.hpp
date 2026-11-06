#pragma once

#include "Api.hpp"
#include "Unreal.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace RC::Widgets
{
    using EventCallback = AvaloniumWidgetCallback;
    using ClickCallback = EventCallback;

    namespace Detail
    {
        inline constexpr uint64_t kCpfParm = 0x0000000000000080ull;
        inline constexpr uint64_t kCpfReturnParm = 0x0000000000000400ull;

        inline Unreal::UObject* TextLibrary()
        {
            static Unreal::UObject* library = []() -> Unreal::UObject* {
                Unreal::UClass* libraryClass = Unreal::UObject::FindClass("KismetTextLibrary");
                return libraryClass ? libraryClass->GetDefaultObject() : nullptr;
            }();
            return library;
        }

        inline int32_t FirstParameterSize(Unreal::UFunction* function)
        {
            if (!function) return 0;
            for (const Unreal::PropertyInfo& property : function->GetProperties())
            {
                if ((property.Flags & kCpfParm) == 0) continue;
                if ((property.Flags & kCpfReturnParm) != 0) continue;
                return property.ElementSize;
            }
            return 0;
        }

        inline Unreal::Value Vector2(double x, double y, int32_t size)
        {
            if (size == 8)
            {
                const float components[2] = {static_cast<float>(x), static_cast<float>(y)};
                return Unreal::Value::Struct(components,
                                             static_cast<int32_t>(sizeof(components)));
            }
            if (size == 16)
            {
                const double components[2] = {x, y};
                return Unreal::Value::Struct(components,
                                             static_cast<int32_t>(sizeof(components)));
            }
            return {};
        }

        inline bool SetSlotVector2(Unreal::UObject* slot, const char* functionName,
                                   double x, double y)
        {
            if (!slot) return false;
            Unreal::UFunction* function = slot->FindFunction(functionName);
            const Unreal::Value value = Vector2(x, y, FirstParameterSize(function));
            if (value.IsNone()) return false;
            return slot->CallValues(functionName, {value}, nullptr);
        }
    }

    inline Unreal::UObject* New(std::string_view className,
                                Unreal::UObject* outer = nullptr)
    {
        Unreal::UClass* objectClass = Unreal::UObject::FindClass(className);
        if (!objectClass) return nullptr;
        return Unreal::UObject::Construct(objectClass, outer);
    }

    inline bool AddChild(Unreal::UObject* panel, Unreal::UObject* child)
    {
        if (!panel || !child) return false;
        return panel->CallValues("AddChild", {Unreal::Value::Object(child)}, nullptr);
    }

    inline bool AddToCanvas(Unreal::UObject* canvas, Unreal::UObject* widget,
                            double x, double y, double width, double height)
    {
        if (!canvas || !widget) return false;

        Unreal::Value slotValue;
        if (!canvas->CallValues("AddChildToCanvas", {Unreal::Value::Object(widget)},
                                &slotValue))
        {
            return false;
        }

        auto* slot = static_cast<Unreal::UObject*>(slotValue.AsObject());
        if (!slot) return false;

        Detail::SetSlotVector2(slot, "SetPosition", x, y);
        Detail::SetSlotVector2(slot, "SetSize", width, height);
        return true;
    }

    inline bool SetText(Unreal::UObject* widget, std::string_view text)
    {
        Unreal::UObject* library = Detail::TextLibrary();
        if (!widget || !library) return false;

        Unreal::Value value;
        if (!library->CallValues("Conv_StringToText",
                                 {Unreal::Value(std::string(text))}, &value) ||
            value.StructSize() <= 0)
        {
            return false;
        }
        return widget->CallValues("SetText", {value}, nullptr);
    }

    inline bool SetVisible(Unreal::UObject* widget, bool visible)
    {
        if (!widget) return false;
        return widget->CallValues("SetVisibility",
                                  {Unreal::Value(visible ? 0 : 1)}, nullptr);
    }

    inline bool CopyStruct(Unreal::UObject* from, std::string_view fromProperty,
                           Unreal::UObject* to, std::string_view toProperty)
    {
        if (!from || !to) return false;
        Unreal::Value value;
        if (!from->GetPropertyValue(fromProperty, value) || value.StructSize() <= 0)
            return false;
        return to->SetPropertyValue(toProperty, value);
    }

    inline uint32_t BindEvent(Unreal::UObject* widget, std::string_view eventName,
                              EventCallback callback, void* userData = nullptr)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->bind_widget_event || !widget || !callback || eventName.empty())
            return 0;
        return api->bind_widget_event(Unreal::UObject::AddressOf(widget),
                                      std::string(eventName).c_str(), callback, userData);
    }

    inline uint32_t BindClick(Unreal::UObject* widget, ClickCallback callback,
                              void* userData = nullptr)
    {
        return BindEvent(widget, "OnClicked", callback, userData);
    }

    inline bool Unbind(uint32_t bindingId)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        return api && api->unbind_widget_click && api->unbind_widget_click(bindingId);
    }

    inline bool UnbindClick(uint32_t bindingId)
    {
        return Unbind(bindingId);
    }
}
