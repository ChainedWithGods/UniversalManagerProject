#pragma once

#include "ModApi.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace Avalonium
{
    inline std::string WideToUtf8(const wchar_t* text)
    {
        if (!text || !*text) return {};
        const int length = static_cast<int>(wcslen(text));
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
        if (bytes <= 0) return {};
        std::string result(static_cast<size_t>(bytes), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text, length, result.data(), bytes, nullptr, nullptr);
        return result;
    }

    class FString
    {
      public:
        FString() = default;
        FString(const char* text) : m_Text(text ? text : "") {}
        FString(std::string text) : m_Text(std::move(text)) {}
        FString(std::string_view text) : m_Text(text) {}
        explicit FString(const wchar_t* text) : m_Text(WideToUtf8(text)) {}

        const std::string& Raw() const { return m_Text; }
        std::string ToString() const { return m_Text; }
        const char* c_str() const { return m_Text.c_str(); }
        size_t Len() const { return m_Text.size(); }
        bool empty() const { return m_Text.empty(); }
        operator const std::string&() const { return m_Text; }
        bool operator==(const FString& other) const { return m_Text == other.m_Text; }
        bool operator==(const char* other) const { return m_Text == (other ? other : ""); }

      private:
        std::string m_Text;
    };

    class FName
    {
      public:
        FName() = default;
        FName(const char* text) : m_Text(text ? text : "") {}
        FName(std::string text) : m_Text(std::move(text)) {}
        FName(std::string_view text) : m_Text(text) {}
        FName(uint32_t comparisonIndex, uint32_t number = 0)
            : m_ComparisonIndex(comparisonIndex), m_Number(number), m_IndexOnly(true)
        {
        }

        std::string ToString() const
        {
            if (m_IndexOnly && m_Text.empty()) return "#" + std::to_string(m_ComparisonIndex);
            return m_Text;
        }

        const std::string& Text() const { return m_Text; }
        uint32_t GetComparisonIndex() const { return m_ComparisonIndex; }
        uint32_t GetNumber() const { return m_Number; }
        bool IsValid() const { return m_IndexOnly || !m_Text.empty(); }
        bool operator==(const FName& other) const
        {
            return m_Text == other.m_Text && m_Number == other.m_Number;
        }

        void SetNumber(uint32_t number) { m_Number = number; }

      private:
        std::string m_Text;
        uint32_t m_ComparisonIndex = 0;
        uint32_t m_Number = 0;
        bool m_IndexOnly = false;
    };

    class Value
    {
      public:
        Value() { m_Raw.kind = AVALONIUM_VALUE_NONE; }

        Value(bool value) : Value() { SetInt(AVALONIUM_VALUE_BOOL, value ? 1 : 0); }
        Value(int8_t value) : Value() { SetInt(AVALONIUM_VALUE_INT8, value); }
        Value(int16_t value) : Value() { SetInt(AVALONIUM_VALUE_INT16, value); }
        Value(int32_t value) : Value() { SetInt(AVALONIUM_VALUE_INT32, value); }
        Value(int64_t value) : Value() { SetInt(AVALONIUM_VALUE_INT64, value); }
        Value(uint8_t value) : Value() { SetUInt(AVALONIUM_VALUE_UINT8, value); }
        Value(uint16_t value) : Value() { SetUInt(AVALONIUM_VALUE_UINT16, value); }
        Value(uint32_t value) : Value() { SetUInt(AVALONIUM_VALUE_UINT32, value); }
        Value(uint64_t value) : Value() { SetUInt(AVALONIUM_VALUE_UINT64, value); }
        Value(float value) : Value() { m_Raw.kind = AVALONIUM_VALUE_FLOAT; m_Raw.float_value = value; }
        Value(double value) : Value() { m_Raw.kind = AVALONIUM_VALUE_DOUBLE; m_Raw.float_value = value; }
        Value(const char* value) : Value() { SetString(AVALONIUM_VALUE_STRING, value ? value : ""); }
        Value(std::string_view value) : Value() { SetString(AVALONIUM_VALUE_STRING, value); }
        Value(const std::string& value) : Value() { SetString(AVALONIUM_VALUE_STRING, value); }
        Value(const FString& value) : Value() { SetString(AVALONIUM_VALUE_STRING, value.Raw()); }
        Value(const FName& value) : Value()
        {
            SetString(AVALONIUM_VALUE_NAME, value.Text());
            m_Raw.name_number = value.GetNumber();
        }

        static Value Object(void* object)
        {
            Value result;
            result.m_Raw.kind = AVALONIUM_VALUE_OBJECT;
            result.m_Raw.pointer = object;
            return result;
        }

        static Value Struct(const void* data, int32_t size)
        {
            Value result;
            result.m_Raw.kind = AVALONIUM_VALUE_STRUCT;
            result.m_Raw.size = size;
            if (data && size > 0)
            {
                result.m_Bytes.resize(static_cast<size_t>(size));
                std::memcpy(result.m_Bytes.data(), data, static_cast<size_t>(size));
                result.m_Raw.pointer = result.m_Bytes.data();
            }
            return result;
        }

        static Value FromRaw(const AvaloniumValue& raw)
        {
            Value result;
            result.m_Raw = raw;
            if (raw.kind == AVALONIUM_VALUE_STRING || raw.kind == AVALONIUM_VALUE_NAME)
            {
                result.m_Text = raw.string ? raw.string : "";
                result.m_Raw.string = result.m_Text.c_str();
                result.m_Raw.size = static_cast<int32_t>(result.m_Text.size());
            }
            return result;
        }

        Value(const Value& other) { CopyFrom(other); }
        Value(Value&& other) noexcept { MoveFrom(std::move(other)); }
        Value& operator=(const Value& other)
        {
            if (this != &other) CopyFrom(other);
            return *this;
        }
        Value& operator=(Value&& other) noexcept
        {
            if (this != &other) MoveFrom(std::move(other));
            return *this;
        }

        const AvaloniumValue& Raw() const { return m_Raw; }
        AvaloniumValue& RawMutable() { return m_Raw; }

        int32_t Kind() const { return m_Raw.kind; }
        bool IsNone() const { return m_Raw.kind == AVALONIUM_VALUE_NONE; }

        bool AsBool() const
        {
            switch (m_Raw.kind)
            {
            case AVALONIUM_VALUE_BOOL:
            case AVALONIUM_VALUE_INT8:
            case AVALONIUM_VALUE_INT16:
            case AVALONIUM_VALUE_INT32:
            case AVALONIUM_VALUE_INT64:
                return m_Raw.int_value != 0;
            case AVALONIUM_VALUE_UINT8:
            case AVALONIUM_VALUE_UINT16:
            case AVALONIUM_VALUE_UINT32:
            case AVALONIUM_VALUE_UINT64:
                return static_cast<uint64_t>(m_Raw.int_value) != 0;
            case AVALONIUM_VALUE_FLOAT:
            case AVALONIUM_VALUE_DOUBLE:
                return m_Raw.float_value != 0.0;
            default:
                return false;
            }
        }

        int64_t AsInt64() const
        {
            if (m_Raw.kind == AVALONIUM_VALUE_FLOAT || m_Raw.kind == AVALONIUM_VALUE_DOUBLE)
                return static_cast<int64_t>(m_Raw.float_value);
            return m_Raw.int_value;
        }

        uint64_t AsUInt64() const
        {
            if (m_Raw.kind == AVALONIUM_VALUE_FLOAT || m_Raw.kind == AVALONIUM_VALUE_DOUBLE)
                return static_cast<uint64_t>(m_Raw.float_value);
            return static_cast<uint64_t>(m_Raw.int_value);
        }

        double AsDouble() const
        {
            if (m_Raw.kind == AVALONIUM_VALUE_FLOAT || m_Raw.kind == AVALONIUM_VALUE_DOUBLE)
                return m_Raw.float_value;
            return static_cast<double>(m_Raw.int_value);
        }

        std::string AsString() const
        {
            return m_Raw.string ? std::string(m_Raw.string) : std::string();
        }

        void* AsObject() const
        {
            return m_Raw.kind == AVALONIUM_VALUE_OBJECT ? const_cast<void*>(m_Raw.pointer) : nullptr;
        }

        const void* AsStruct() const
        {
            return m_Raw.kind == AVALONIUM_VALUE_STRUCT ? m_Raw.pointer : nullptr;
        }

        int32_t StructSize() const
        {
            return m_Raw.kind == AVALONIUM_VALUE_STRUCT ? m_Raw.size : 0;
        }

        bool CopyStruct(void* out, int32_t outSize) const
        {
            if (!out || m_Raw.kind != AVALONIUM_VALUE_STRUCT || !m_Raw.pointer ||
                outSize < m_Raw.size || m_Raw.size <= 0)
                return false;
            std::memcpy(out, m_Raw.pointer, static_cast<size_t>(m_Raw.size));
            return true;
        }

        template <class T>
        T As() const
        {
            if constexpr (std::is_same_v<T, bool>)
                return AsBool();
            else if constexpr (std::is_same_v<T, int8_t>)
                return static_cast<int8_t>(AsInt64());
            else if constexpr (std::is_same_v<T, int16_t>)
                return static_cast<int16_t>(AsInt64());
            else if constexpr (std::is_same_v<T, int32_t>)
                return static_cast<int32_t>(AsInt64());
            else if constexpr (std::is_same_v<T, int64_t>)
                return AsInt64();
            else if constexpr (std::is_same_v<T, uint8_t>)
                return static_cast<uint8_t>(AsUInt64());
            else if constexpr (std::is_same_v<T, uint16_t>)
                return static_cast<uint16_t>(AsUInt64());
            else if constexpr (std::is_same_v<T, uint32_t>)
                return static_cast<uint32_t>(AsUInt64());
            else if constexpr (std::is_same_v<T, uint64_t>)
                return AsUInt64();
            else if constexpr (std::is_same_v<T, float>)
                return static_cast<float>(AsDouble());
            else if constexpr (std::is_same_v<T, double>)
                return AsDouble();
            else if constexpr (std::is_same_v<T, std::string>)
                return AsString();
            else if constexpr (std::is_same_v<T, FString>)
                return FString(AsString());
            else if constexpr (std::is_same_v<T, FName>)
            {
                FName name(AsString());
                name.SetNumber(m_Raw.name_number);
                return name;
            }
            else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
            {
                return static_cast<T>(AsInt64());
            }
            else if constexpr (std::is_integral_v<T>)
            {
                return static_cast<T>(AsUInt64());
            }
            else if constexpr (std::is_void_v<T>)
                return;
            else
                static_assert(sizeof(T) == 0, "Avalonium::Value::As<T> has no conversion for T");
        }

      private:
        void SetInt(int32_t kind, int64_t value)
        {
            m_Raw.kind = kind;
            m_Raw.int_value = value;
        }

        void SetUInt(int32_t kind, uint64_t value)
        {
            m_Raw.kind = kind;
            m_Raw.int_value = static_cast<int64_t>(value);
        }

        void SetString(int32_t kind, std::string_view text)
        {
            m_Text.assign(text);
            m_Raw.kind = kind;
            m_Raw.string = m_Text.c_str();
            m_Raw.size = static_cast<int32_t>(m_Text.size());
        }

        void CopyFrom(const Value& other)
        {
            m_Raw = other.m_Raw;
            m_Text = other.m_Text;
            m_Bytes = other.m_Bytes;
            if (m_Raw.kind == AVALONIUM_VALUE_STRING || m_Raw.kind == AVALONIUM_VALUE_NAME)
                m_Raw.string = m_Text.c_str();
            if (m_Raw.kind == AVALONIUM_VALUE_STRUCT && !m_Bytes.empty())
                m_Raw.pointer = m_Bytes.data();
        }

        void MoveFrom(Value&& other)
        {
            m_Raw = other.m_Raw;
            m_Text = std::move(other.m_Text);
            m_Bytes = std::move(other.m_Bytes);
            if (m_Raw.kind == AVALONIUM_VALUE_STRING || m_Raw.kind == AVALONIUM_VALUE_NAME)
                m_Raw.string = m_Text.c_str();
            if (m_Raw.kind == AVALONIUM_VALUE_STRUCT && !m_Bytes.empty())
                m_Raw.pointer = m_Bytes.data();
            other.m_Raw = {};
            other.m_Raw.kind = AVALONIUM_VALUE_NONE;
        }

        AvaloniumValue m_Raw{};
        std::string m_Text;
        std::vector<uint8_t> m_Bytes;
    };

    template <class T>
    inline Value ToValue(const T& value)
    {
        if constexpr (std::is_same_v<std::decay_t<T>, Value>)
            return value;
        else if constexpr (std::is_same_v<std::decay_t<T>, bool>)
            return Value(value);
        else if constexpr (std::is_integral_v<std::decay_t<T>>)
        {
            if constexpr (std::is_signed_v<std::decay_t<T>>)
                return Value(static_cast<int64_t>(value));
            else
                return Value(static_cast<uint64_t>(value));
        }
        else if constexpr (std::is_floating_point_v<std::decay_t<T>>)
            return Value(static_cast<double>(value));
        else if constexpr (std::is_same_v<std::decay_t<T>, FString>)
            return Value(value);
        else if constexpr (std::is_same_v<std::decay_t<T>, FName>)
            return Value(value);
        else if constexpr (std::is_same_v<std::decay_t<T>, std::string> ||
                           std::is_same_v<std::decay_t<T>, std::string_view>)
            return Value(value);
        else if constexpr (std::is_same_v<std::decay_t<T>, const char*> ||
                           std::is_same_v<std::decay_t<T>, char*>)
            return Value(value);
        else if constexpr (std::is_same_v<std::decay_t<T>, std::nullptr_t>)
            return Value::Object(nullptr);
        else if constexpr (std::is_pointer_v<std::decay_t<T>>)
            return Value::Object(const_cast<void*>(static_cast<const void*>(value)));
        else
            static_assert(sizeof(T) == 0, "Avalonium::ToValue has no conversion for T");
    }
}
