#include "CppModValue.h"

#include "../core/EngineDiscovery.h"
#include "../utils/SafeMemory.h"

#include <windows.h>

#include <array>
#include <cstring>
#include <vector>

namespace Avalonium::CppMods
{
    namespace
    {
        struct EngineStringHeader
        {
            const void* Data;
            int32_t Num;
            int32_t Max;
        };

        static_assert(sizeof(EngineStringHeader) == 0x10, "FString header must stay 16 bytes");

        bool TypeContains(const std::string& type, const char* needle)
        {
            return type.find(needle) != std::string::npos;
        }

        StringEncoding EncodingForType(const std::string& type)
        {
            if (TypeContains(type, "Utf8StrProperty")) return StringEncoding::Utf8;
            if (TypeContains(type, "AnsiStrProperty")) return StringEncoding::Ansi;
            return StringEncoding::Wide;
        }

        int32_t EnumWidth(const PropertyRef& property)
        {
            if (property.ElementSize >= 1 && property.ElementSize <= 8) return property.ElementSize;
            return 4;
        }

        int64_t IntFromValue(const AvaloniumValue& value)
        {
            if (value.kind == AVALONIUM_VALUE_FLOAT || value.kind == AVALONIUM_VALUE_DOUBLE)
                return static_cast<int64_t>(value.float_value);
            return value.int_value;
        }

        void ReadBool(const EngineState& state, const PropertyRef& property, const uint8_t* address,
                      AvaloniumValue& out)
        {
            uint8_t byteOffset = property.BoolByteOffset;
            uint8_t fieldMask = property.BoolFieldMask ? property.BoolFieldMask : property.BoolByteMask;
            if (fieldMask == 0) fieldMask = 1;

            if (property.Property && state.FBoolProperty_BaseOff >= 0)
            {
                SafeMem_Read(reinterpret_cast<uint8_t*>(property.Property) +
                                 state.FBoolProperty_BaseOff + 1,
                             &byteOffset, 1);
                SafeMem_Read(reinterpret_cast<uint8_t*>(property.Property) +
                                 state.FBoolProperty_BaseOff + 3,
                             &fieldMask, 1);
                if (fieldMask == 0) fieldMask = 1;
            }

            uint8_t byte = 0;
            if (!SafeMem_Read(address + byteOffset, &byte, 1)) return;
            out.kind = AVALONIUM_VALUE_BOOL;
            out.int_value = (byte & fieldMask) ? 1 : 0;
        }
    }

    std::string& ValueStringScratch()
    {

        static thread_local std::array<std::string, 8> ring;
        static thread_local size_t next = 0;
        std::string& scratch = ring[next % ring.size()];
        ++next;
        return scratch;
    }

    bool StabilizeStructValue(AvaloniumValue& value)
    {
        if (value.kind != AVALONIUM_VALUE_STRUCT || value.size <= 0 || !value.pointer)
            return false;

        static thread_local std::vector<uint8_t> scratch;
        scratch.resize(static_cast<size_t>(value.size));
        std::memcpy(scratch.data(), value.pointer, static_cast<size_t>(value.size));
        value.pointer = scratch.data();
        return true;
    }

    void SetNoneValue(AvaloniumValue& value)
    {
        value = {};
        value.kind = AVALONIUM_VALUE_NONE;
    }

    void SetStringValue(const std::string& text, AvaloniumValue& value)
    {
        std::string& scratch = ValueStringScratch();
        scratch = text;
        value = {};
        value.kind = AVALONIUM_VALUE_STRING;
        value.string = scratch.c_str();
        value.size = static_cast<int32_t>(scratch.size());
    }

    void SetNameValue(const std::string& text, uint32_t number, AvaloniumValue& value)
    {
        std::string& scratch = ValueStringScratch();
        scratch = text;
        value = {};
        value.kind = AVALONIUM_VALUE_NAME;
        value.string = scratch.c_str();
        value.size = static_cast<int32_t>(scratch.size());
        value.name_number = number;
    }

    PropertyRef MakePropertyRef(const EngineState& state,
                                const EngineDiscovery::ReflectedMember& member)
    {
        PropertyRef property;
        property.Property = member.Property;
        property.Name = member.Name;
        property.TypeName = member.TypeName;
        property.Offset = member.Offset;
        property.ElementSize = member.ElementSize;
        property.ArrayDim = member.ArrayDim;
        property.Flags = member.Flags;

        if (property.IsBool() && member.Property && state.FBoolProperty_BaseOff >= 0)
        {
            SafeMem_Read(reinterpret_cast<uint8_t*>(member.Property) + state.FBoolProperty_BaseOff + 1,
                         &property.BoolByteOffset, 1);
            SafeMem_Read(reinterpret_cast<uint8_t*>(member.Property) + state.FBoolProperty_BaseOff + 2,
                         &property.BoolByteMask, 1);
            SafeMem_Read(reinterpret_cast<uint8_t*>(member.Property) + state.FBoolProperty_BaseOff + 3,
                         &property.BoolFieldMask, 1);
            if (property.BoolByteMask == 0) property.BoolByteMask = 1;
            if (property.BoolFieldMask == 0) property.BoolFieldMask = property.BoolByteMask;
        }
        return property;
    }

    std::string ReadFNameAt(EngineState& state, const uint8_t* address, int32_t width)
    {
        if (!address) return {};

        int32_t nameWidth = width;
        if (nameWidth <= 0) nameWidth = state.FName_Size;
        if (nameWidth <= 0 || nameWidth > 16) return {};

        uint32_t comparisonIndex = 0;
        uint32_t number = 0;

        if (state.FName_CompIdxOff >= 0 &&
            state.FName_CompIdxOff + static_cast<int32_t>(sizeof(uint32_t)) <= nameWidth)
        {
            SafeMem_Read(address + state.FName_CompIdxOff, &comparisonIndex,
                         sizeof(comparisonIndex));
        }
        if (state.FName_NumberOff >= 0 &&
            state.FName_NumberOff + static_cast<int32_t>(sizeof(uint32_t)) <= nameWidth)
        {
            SafeMem_Read(address + state.FName_NumberOff, &number, sizeof(number));
        }

        std::string text = EngineDiscovery::GetCachedFName(state, static_cast<int32_t>(comparisonIndex));
        if (number != 0 && !state.bUseOutlineNumber)
        {
            const std::string suffix = "_" + std::to_string(number);
            if (text.size() < suffix.size() ||
                text.compare(text.size() - suffix.size(), suffix.size(), suffix) != 0)
            {
                text += suffix;
            }
        }
        return text;
    }

    bool ReadFStringAt(EngineState& state, const uint8_t* address, StringEncoding encoding,
                       std::string& out)
    {
        (void)state;
        if (!address) return false;

        EngineStringHeader header{};
        if (!SafeMem_Read(address, &header, sizeof(header))) return false;
        if (header.Num == 0)
        {
            out.clear();
            return true;
        }
        if (!header.Data || header.Num < 0 || header.Num > 16 * 1024 * 1024) return false;

        if (encoding == StringEncoding::Wide)
        {
            std::vector<wchar_t> buffer(static_cast<size_t>(header.Num) + 1, 0);
            const size_t bytes = static_cast<size_t>(header.Num) * sizeof(wchar_t);
            if (!SafeMem_Read(header.Data, buffer.data(), bytes)) return false;

            size_t length = static_cast<size_t>(header.Num);
            while (length > 0 && buffer[length - 1] == 0) --length;
            if (length == 0)
            {
                out.clear();
                return true;
            }

            const int utf8Bytes = WideCharToMultiByte(CP_UTF8, 0, buffer.data(),
                                                      static_cast<int>(length), nullptr, 0,
                                                      nullptr, nullptr);
            if (utf8Bytes <= 0) return false;
            out.assign(static_cast<size_t>(utf8Bytes), '\0');
            WideCharToMultiByte(CP_UTF8, 0, buffer.data(), static_cast<int>(length),
                                out.data(), utf8Bytes, nullptr, nullptr);
            return true;
        }

        std::vector<char> buffer(static_cast<size_t>(header.Num) + 1, 0);
        if (!SafeMem_Read(header.Data, buffer.data(), static_cast<size_t>(header.Num))) return false;
        size_t length = static_cast<size_t>(header.Num);
        while (length > 0 && buffer[length - 1] == 0) --length;

        if (encoding == StringEncoding::Utf8)
        {
            out.assign(buffer.data(), length);
            return true;
        }

        if (length == 0)
        {
            out.clear();
            return true;
        }

        const int wideBytes = MultiByteToWideChar(CP_ACP, 0, buffer.data(),
                                                  static_cast<int>(length), nullptr, 0);
        if (wideBytes <= 0) return false;
        std::vector<wchar_t> wide(static_cast<size_t>(wideBytes), 0);
        MultiByteToWideChar(CP_ACP, 0, buffer.data(), static_cast<int>(length),
                            wide.data(), wideBytes);

        const int utf8Bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideBytes,
                                                  nullptr, 0, nullptr, nullptr);
        if (utf8Bytes <= 0) return false;
        out.assign(static_cast<size_t>(utf8Bytes), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideBytes, out.data(), utf8Bytes,
                            nullptr, nullptr);
        return true;
    }

    bool WriteFStringAt(EngineState& state, uint8_t* address, const std::string& text,
                        StringEncoding encoding)
    {
        if (!address) return false;

        void* mallocInstance = state.GMallocLocation
                                   ? SafeMem_ReadPtr(state.GMallocLocation)
                                   : state.GMalloc;
        if (!mallocInstance) mallocInstance = state.GMalloc;
        if (!mallocInstance || state.GMallocMallocSlot < 0) return false;

        EngineStringHeader header{};

        if (encoding == StringEncoding::Wide)
        {
            const int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
            if (wlen <= 0) return false;
            const size_t bytes = static_cast<size_t>(wlen) * sizeof(wchar_t);
            void* buffer = SafeMem_CallMallocVftable(mallocInstance, bytes, 0,
                                                     state.GMallocMallocSlot);
            if (!buffer) return false;
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, static_cast<wchar_t*>(buffer), wlen);
            header.Data = buffer;
            header.Num = wlen;
            header.Max = wlen;
        }
        else if (encoding == StringEncoding::Utf8)
        {
            const size_t bytes = text.size() + 1;
            void* buffer = SafeMem_CallMallocVftable(mallocInstance, bytes, 0,
                                                     state.GMallocMallocSlot);
            if (!buffer) return false;
            std::memcpy(buffer, text.c_str(), bytes);
            header.Data = buffer;
            header.Num = static_cast<int32_t>(bytes);
            header.Max = static_cast<int32_t>(bytes);
        }
        else
        {
            const int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
            if (wlen <= 0) return false;
            std::vector<wchar_t> wide(static_cast<size_t>(wlen), 0);
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), wlen);

            const int alen = WideCharToMultiByte(CP_ACP, 0, wide.data(), -1, nullptr, 0,
                                                 nullptr, nullptr);
            if (alen <= 0) return false;
            const size_t bytes = static_cast<size_t>(alen);
            void* buffer = SafeMem_CallMallocVftable(mallocInstance, bytes, 0,
                                                     state.GMallocMallocSlot);
            if (!buffer) return false;
            WideCharToMultiByte(CP_ACP, 0, wide.data(), -1, static_cast<char*>(buffer), alen,
                                nullptr, nullptr);
            header.Data = buffer;
            header.Num = alen;
            header.Max = alen;
        }

        return SafeMem_Write(address, &header, sizeof(header)) != 0;
    }

    bool ReadValueAt(EngineState& state, const PropertyRef& property, const uint8_t* address,
                     AvaloniumValue& out)
    {
        SetNoneValue(out);
        if (!address || property.TypeName.empty()) return false;

        const std::string& type = property.TypeName;

        if (property.IsBool())
        {
            if (!property.Property && property.BoolFieldMask == 0)
            {

                uint8_t byte = 0;
                if (!SafeMem_Read(address, &byte, 1)) return false;
                out.kind = AVALONIUM_VALUE_BOOL;
                out.int_value = byte != 0;
                return true;
            }
            ReadBool(state, property, address, out);
            return out.kind == AVALONIUM_VALUE_BOOL;
        }

        if (TypeContains(type, "ByteProperty") || TypeContains(type, "Int8Property"))
        {
            int8_t value = 0;
            if (!SafeMem_Read(address, &value, sizeof(value))) return false;
            out.kind = AVALONIUM_VALUE_INT8;
            out.int_value = value;
            return true;
        }
        if (TypeContains(type, "Int16Property") || TypeContains(type, "UInt16Property"))
        {
            int16_t value = 0;
            if (!SafeMem_Read(address, &value, sizeof(value))) return false;
            out.kind = AVALONIUM_VALUE_INT16;
            out.int_value = value;
            return true;
        }
        if (TypeContains(type, "IntProperty") || TypeContains(type, "UInt32Property"))
        {
            int32_t value = 0;
            if (!SafeMem_Read(address, &value, sizeof(value))) return false;
            out.kind = AVALONIUM_VALUE_INT32;
            out.int_value = value;
            return true;
        }
        if (TypeContains(type, "Int64Property") || TypeContains(type, "UInt64Property"))
        {
            int64_t value = 0;
            if (!SafeMem_Read(address, &value, sizeof(value))) return false;
            out.kind = AVALONIUM_VALUE_INT64;
            out.int_value = value;
            return true;
        }
        if (TypeContains(type, "FloatProperty"))
        {
            float value = 0.0f;
            if (!SafeMem_Read(address, &value, sizeof(value))) return false;
            out.kind = AVALONIUM_VALUE_FLOAT;
            out.float_value = value;
            return true;
        }
        if (TypeContains(type, "DoubleProperty"))
        {
            double value = 0.0;
            if (!SafeMem_Read(address, &value, sizeof(value))) return false;
            out.kind = AVALONIUM_VALUE_DOUBLE;
            out.float_value = value;
            return true;
        }
        if (TypeContains(type, "EnumProperty"))
        {
            const int32_t width = EnumWidth(property);
            uint64_t value = 0;
            if (!SafeMem_Read(address, &value, static_cast<size_t>(width))) return false;
            out.kind = AVALONIUM_VALUE_INT64;
            out.int_value = static_cast<int64_t>(value);
            return true;
        }
        if (TypeContains(type, "NameProperty"))
        {
            const int32_t width = property.ElementSize > 0 ? property.ElementSize : state.FName_Size;
            uint32_t number = 0;
            if (state.FName_NumberOff >= 0 &&
                state.FName_NumberOff + static_cast<int32_t>(sizeof(uint32_t)) <= width)
            {
                SafeMem_Read(address + state.FName_NumberOff, &number, sizeof(number));
            }
            SetNameValue(ReadFNameAt(state, address, width), number, out);
            return true;
        }
        if (TypeContains(type, "StrProperty"))
        {
            std::string text;
            if (!ReadFStringAt(state, address, EncodingForType(type), text)) return false;
            SetStringValue(text, out);
            return true;
        }

        if (TypeContains(type, "SoftObjectProperty") || TypeContains(type, "SoftClassProperty") ||
            TypeContains(type, "WeakObjectProperty") || TypeContains(type, "LazyObjectProperty"))
        {
            if (property.ElementSize > 0)
            {
                out.kind = AVALONIUM_VALUE_STRUCT;
                out.pointer = address;
                out.size = property.ElementSize;
                return true;
            }
            return false;
        }

        if (TypeContains(type, "ObjectProperty") || TypeContains(type, "ClassProperty") ||
            TypeContains(type, "InterfaceProperty"))
        {
            void* object = nullptr;
            if (!SafeMem_Read(address, &object, sizeof(object))) return false;
            out.kind = AVALONIUM_VALUE_OBJECT;
            out.pointer = object;
            return true;
        }

        if (property.ElementSize > 0)
        {
            out.kind = AVALONIUM_VALUE_STRUCT;
            out.pointer = address;
            out.size = property.ElementSize;
            return true;
        }

        return false;
    }

    bool WriteValueAt(EngineState& state, const PropertyRef& property, uint8_t* address,
                      const AvaloniumValue& value)
    {
        if (!address || property.TypeName.empty()) return false;

        const std::string& type = property.TypeName;

        if (property.IsBool())
        {
            uint8_t byteOffset = property.BoolByteOffset;
            uint8_t byteMask = property.BoolByteMask ? property.BoolByteMask : 1;
            uint8_t fieldMask = property.BoolFieldMask ? property.BoolFieldMask : byteMask;
            if (byteMask == 0) byteMask = 1;
            if (fieldMask == 0) fieldMask = byteMask;

            uint8_t byte = 0;
            if (!SafeMem_Read(address + byteOffset, &byte, 1)) return false;
            const bool enabled = value.kind == AVALONIUM_VALUE_BOOL
                                     ? value.int_value != 0
                                     : (value.kind == AVALONIUM_VALUE_FLOAT ||
                                        value.kind == AVALONIUM_VALUE_DOUBLE)
                                           ? value.float_value != 0.0
                                           : value.int_value != 0;
            byte = static_cast<uint8_t>((byte & ~fieldMask) | (enabled ? byteMask : 0));
            return SafeMem_Write(address + byteOffset, &byte, 1) != 0;
        }

        if (TypeContains(type, "ByteProperty") || TypeContains(type, "Int8Property"))
        {
            const int8_t v = static_cast<int8_t>(IntFromValue(value));
            return SafeMem_Write(address, &v, 1) != 0;
        }
        if (TypeContains(type, "Int16Property") || TypeContains(type, "UInt16Property"))
        {
            const int16_t v = static_cast<int16_t>(IntFromValue(value));
            return SafeMem_Write(address, &v, 2) != 0;
        }
        if (TypeContains(type, "IntProperty") || TypeContains(type, "UInt32Property"))
        {
            const int32_t v = static_cast<int32_t>(IntFromValue(value));
            return SafeMem_Write(address, &v, 4) != 0;
        }
        if (TypeContains(type, "Int64Property") || TypeContains(type, "UInt64Property"))
        {
            const int64_t v = IntFromValue(value);
            return SafeMem_Write(address, &v, 8) != 0;
        }
        if (TypeContains(type, "FloatProperty"))
        {
            const float v = value.kind == AVALONIUM_VALUE_FLOAT ||
                                    value.kind == AVALONIUM_VALUE_DOUBLE
                                ? static_cast<float>(value.float_value)
                                : static_cast<float>(value.int_value);
            return SafeMem_Write(address, &v, 4) != 0;
        }
        if (TypeContains(type, "DoubleProperty"))
        {
            const double v = value.kind == AVALONIUM_VALUE_FLOAT ||
                                     value.kind == AVALONIUM_VALUE_DOUBLE
                                 ? value.float_value
                                 : static_cast<double>(value.int_value);
            return SafeMem_Write(address, &v, 8) != 0;
        }
        if (TypeContains(type, "EnumProperty"))
        {
            const int32_t width = EnumWidth(property);
            uint64_t v = static_cast<uint64_t>(IntFromValue(value));
            return SafeMem_Write(address, &v, static_cast<size_t>(width)) != 0;
        }
        if (TypeContains(type, "NameProperty"))
        {
            std::string text;
            uint32_t number = 0;
            if (value.kind == AVALONIUM_VALUE_NAME)
            {
                text = value.string ? value.string : "";
                number = value.name_number;
            }
            else if (value.kind == AVALONIUM_VALUE_STRING)
            {
                text = value.string ? value.string : "";
            }
            else
            {
                return false;
            }

            if (number != 0)
            {
                const std::string suffix = "_" + std::to_string(number);
                if (text.size() >= suffix.size() &&
                    text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0)
                {
                    text.resize(text.size() - suffix.size());
                }
            }

            if (number == 0 && !text.empty() &&
                EngineDiscovery::ResolveFNameIndex(state, text) < 0)
            {
                size_t end = text.size();
                while (end > 0 && text[end - 1] >= '0' && text[end - 1] <= '9') --end;
                if (end < text.size() && end > 0 && text[end - 1] == '_')
                {
                    uint64_t external = 0;
                    for (size_t i = end; i < text.size(); ++i)
                        external = external * 10 + static_cast<uint64_t>(text[i] - '0');
                    const std::string base = text.substr(0, end - 1);
                    if (external < 0xFFFF &&
                        EngineDiscovery::ResolveFNameIndex(state, base) >= 0)
                    {
                        text = base;
                        number = static_cast<uint32_t>(external + 1);
                    }
                }
            }

            std::string detail;
            const int32_t index = EngineDiscovery::FindOrAddFNameIndex(state, text, &detail);
            if (index < 0) return false;

            int32_t width = property.ElementSize > 0 ? property.ElementSize : state.FName_Size;
            if (width <= 0 || width > 16) width = state.FName_Size;
            return EngineDiscovery::WriteFNameBytes(state, static_cast<uint32_t>(index), number,
                                                    address, width);
        }
        if (TypeContains(type, "StrProperty"))
        {
            if (value.kind != AVALONIUM_VALUE_STRING && value.kind != AVALONIUM_VALUE_NAME)
                return false;
            return WriteFStringAt(state, address, value.string ? value.string : "",
                                  EncodingForType(type));
        }
        if (TypeContains(type, "SoftObjectProperty") || TypeContains(type, "SoftClassProperty") ||
            TypeContains(type, "WeakObjectProperty") || TypeContains(type, "LazyObjectProperty"))
        {
            if (value.kind == AVALONIUM_VALUE_STRUCT && property.ElementSize > 0 &&
                value.size == property.ElementSize && value.pointer)
            {
                return SafeMem_Write(address, value.pointer, static_cast<size_t>(value.size)) != 0;
            }
            return false;
        }
        if (TypeContains(type, "ObjectProperty") || TypeContains(type, "ClassProperty") ||
            TypeContains(type, "InterfaceProperty"))
        {
            if (value.kind != AVALONIUM_VALUE_OBJECT && value.kind != AVALONIUM_VALUE_NONE)
                return false;
            void* object = const_cast<void*>(value.pointer);
            return SafeMem_Write(address, &object, sizeof(object)) != 0;
        }
        if (TypeContains(type, "TextProperty"))
        {

            if (value.kind == AVALONIUM_VALUE_STRUCT && property.ElementSize > 0 &&
                value.size == property.ElementSize)
            {
                return SafeMem_Write(address, value.pointer, static_cast<size_t>(value.size)) != 0;
            }
            return false;
        }

        if (value.kind == AVALONIUM_VALUE_STRUCT && property.ElementSize > 0 &&
            value.size == property.ElementSize && value.pointer)
        {
            return SafeMem_Write(address, value.pointer, static_cast<size_t>(value.size)) != 0;
        }

        return false;
    }
}
