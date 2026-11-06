#pragma once

#include "../core/Types.h"
#include "../core/BlueprintDiscovery.h"
#include "sdk/Avalonium/ModApi.h"

#include <cstdint>
#include <string>

namespace Avalonium::CppMods
{

    struct PropertyRef
    {
        void* Property = nullptr;
        std::string Name;
        std::string TypeName;
        int32_t Offset = 0;
        int32_t ElementSize = 0;
        int32_t ArrayDim = 1;
        uint64_t Flags = 0;
        uint8_t BoolByteOffset = 0;
        uint8_t BoolByteMask = 1;
        uint8_t BoolFieldMask = 1;

        bool IsBool() const { return TypeName.find("BoolProperty") != std::string::npos; }
    };

    PropertyRef MakePropertyRef(const EngineState& state,
                                const EngineDiscovery::ReflectedMember& member);

    bool ReadValueAt(EngineState& state, const PropertyRef& property,
                     const uint8_t* address, AvaloniumValue& out);

    bool WriteValueAt(EngineState& state, const PropertyRef& property,
                      uint8_t* address, const AvaloniumValue& value);

    enum class StringEncoding
    {
        Wide,
        Utf8,
        Ansi
    };

    std::string ReadFNameAt(EngineState& state, const uint8_t* address, int32_t width);
    bool ReadFStringAt(EngineState& state, const uint8_t* address, StringEncoding encoding,
                       std::string& out);
    bool WriteFStringAt(EngineState& state, uint8_t* address, const std::string& text,
                        StringEncoding encoding);

    std::string& ValueStringScratch();

    bool StabilizeStructValue(AvaloniumValue& value);

    void SetNoneValue(AvaloniumValue& value);
    void SetStringValue(const std::string& text, AvaloniumValue& value);
    void SetNameValue(const std::string& text, uint32_t number, AvaloniumValue& value);
}
