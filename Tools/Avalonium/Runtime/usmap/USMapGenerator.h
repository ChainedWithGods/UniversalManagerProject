#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <filesystem>

struct EngineState;

namespace USMapDiscovery {
    void DiscoverPropertySubTypeOffsets(EngineState& state);
}

enum class EPropertyType : uint8_t {
    ByteProperty = 0,
    BoolProperty = 1,
    IntProperty = 2,
    FloatProperty = 3,
    ObjectProperty = 4,
    NameProperty = 5,
    DelegateProperty = 6,
    DoubleProperty = 7,
    ArrayProperty = 8,
    StructProperty = 9,
    StrProperty = 10,
    TextProperty = 11,
    InterfaceProperty = 12,
    MulticastDelegateProperty = 13,
    WeakObjectProperty = 14,
    LazyObjectProperty = 15,
    AssetObjectProperty = 16,
    SoftObjectProperty = 17,
    UInt64Property = 18,
    UInt32Property = 19,
    UInt16Property = 20,
    Int64Property = 21,
    Int16Property = 22,
    Int8Property = 23,
    MapProperty = 24,
    SetProperty = 25,
    EnumProperty = 26,
    FieldPathProperty = 27,
    OptionalProperty = 28,
    Utf8StrProperty = 29,
    AnsiStrProperty = 30,
    ClassProperty = 31,
    MulticastInlineDelegateProperty = 32,
    SoftClassProperty = 33,
    VerseStringProperty = 34,
    VerseDynamicProperty = 35,
    VerseFunctionProperty = 36,
    Unknown = 0xFF,
};

inline EPropertyType PropertyNameToUsmapType(const std::string& rawTypeName) {
    if (rawTypeName.empty()) return EPropertyType::Unknown;

    std::string typeName = rawTypeName;
    size_t underscorePos = typeName.rfind('_');
    if (underscorePos != std::string::npos && underscorePos > 0) {
        bool allDigits = true;
        for (size_t i = underscorePos + 1; i < typeName.size(); i++) {
            if (!isdigit(static_cast<unsigned char>(typeName[i]))) {
                allDigits = false;
                break;
            }
        }
        if (allDigits && underscorePos + 1 < typeName.size()) {
            typeName = typeName.substr(0, underscorePos);
        }
    }

    static const std::unordered_map<std::string, EPropertyType> typeMap = {
        {"ByteProperty",                      EPropertyType::ByteProperty},
        {"BoolProperty",                      EPropertyType::BoolProperty},
        {"IntProperty",                       EPropertyType::IntProperty},
        {"FloatProperty",                     EPropertyType::FloatProperty},
        {"ObjectProperty",                    EPropertyType::ObjectProperty},
        {"ObjectPtrProperty",                 EPropertyType::ObjectProperty},
        {"ClassProperty",                     EPropertyType::ObjectProperty},
        {"ClassPtrProperty",                  EPropertyType::ObjectProperty},
        {"NameProperty",                      EPropertyType::NameProperty},
        {"DelegateProperty",                  EPropertyType::DelegateProperty},
        {"DoubleProperty",                    EPropertyType::DoubleProperty},
        {"ArrayProperty",                     EPropertyType::ArrayProperty},
        {"StructProperty",                    EPropertyType::StructProperty},
        {"StrProperty",                       EPropertyType::StrProperty},
        {"TextProperty",                      EPropertyType::TextProperty},
        {"InterfaceProperty",                 EPropertyType::InterfaceProperty},
        {"MulticastDelegateProperty",         EPropertyType::MulticastDelegateProperty},
        {"MulticastSparseDelegateProperty",   EPropertyType::MulticastDelegateProperty},
        {"MulticastInlineDelegateProperty",   EPropertyType::MulticastDelegateProperty},
        {"WeakObjectProperty",                EPropertyType::WeakObjectProperty},
        {"LazyObjectProperty",                EPropertyType::LazyObjectProperty},
        {"SoftObjectProperty",                EPropertyType::SoftObjectProperty},
        {"SoftClassProperty",                 EPropertyType::SoftObjectProperty},
        {"AssetObjectProperty",               EPropertyType::AssetObjectProperty},
        {"AssetClassProperty",                EPropertyType::AssetObjectProperty},
        {"UInt64Property",                    EPropertyType::UInt64Property},
        {"UInt32Property",                    EPropertyType::UInt32Property},
        {"UInt16Property",                    EPropertyType::UInt16Property},
        {"Int64Property",                     EPropertyType::Int64Property},
        {"Int16Property",                     EPropertyType::Int16Property},
        {"Int8Property",                      EPropertyType::Int8Property},
        {"MapProperty",                       EPropertyType::MapProperty},
        {"SetProperty",                       EPropertyType::SetProperty},
        {"EnumProperty",                      EPropertyType::EnumProperty},
        {"FieldPathProperty",                 EPropertyType::FieldPathProperty},
        {"OptionalProperty",                  EPropertyType::OptionalProperty},
        {"Utf8StrProperty",                   EPropertyType::Utf8StrProperty},
        {"AnsiStrProperty",                   EPropertyType::AnsiStrProperty},

        {"VerseStringProperty",               EPropertyType::Unknown},
        {"VerseDynamicProperty",              EPropertyType::Unknown},
        {"VerseFunctionProperty",             EPropertyType::Unknown},
    };

    auto it = typeMap.find(typeName);
    if (it != typeMap.end()) return it->second;

    if (typeName.size() > 1 && (typeName[0] == 'F' || typeName[0] == 'U')) {
        auto it2 = typeMap.find(typeName.substr(1));
        if (it2 != typeMap.end()) return it2->second;
    }

    return EPropertyType::Unknown;
}

class USMapWriter {
public:
    std::vector<uint8_t> buffer;

    void WriteByte(uint8_t v) {
        buffer.push_back(v);
    }

    void WriteInt16(int16_t v) {
        buffer.push_back(v & 0xFF);
        buffer.push_back((v >> 8) & 0xFF);
    }

    void WriteUInt16(uint16_t v) {
        WriteInt16((int16_t)v);
    }

    void WriteInt32(int32_t v) {
        buffer.push_back(v & 0xFF);
        buffer.push_back((v >> 8) & 0xFF);
        buffer.push_back((v >> 16) & 0xFF);
        buffer.push_back((v >> 24) & 0xFF);
    }

    void WriteUInt32(uint32_t v) {
        WriteInt32((int32_t)v);
    }

    void WriteInt64(int64_t v) {
        for (int i = 0; i < 8; i++) {
            buffer.push_back((v >> (i * 8)) & 0xFF);
        }
    }

    void WriteUInt64(uint64_t v) {
        WriteInt64((int64_t)v);
    }

    void WriteString(const std::string& s) {

        WriteUInt16((uint16_t)s.length());
        for (char c : s) {
            buffer.push_back((uint8_t)c);
        }
    }

    void WriteBytes(const void* data, size_t size) {
        const uint8_t* p = (const uint8_t*)data;
        buffer.insert(buffer.end(), p, p + size);
    }

    bool SaveToFile(const std::string& path, void (*logFunc)(const char*, ...)) {
        auto toWidePath = [](const std::string& narrowPath) -> std::wstring {
            if (narrowPath.empty()) return {};

            int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, narrowPath.c_str(), -1, nullptr, 0);
            UINT codePage = CP_UTF8;
            DWORD flags = MB_ERR_INVALID_CHARS;
            if (wideLength <= 0) {
                codePage = CP_ACP;
                flags = 0;
                wideLength = MultiByteToWideChar(codePage, flags, narrowPath.c_str(), -1, nullptr, 0);
            }
            if (wideLength <= 0) {
                return {};
            }

            std::wstring widePath(static_cast<size_t>(wideLength - 1), L'\0');
            MultiByteToWideChar(codePage, flags, narrowPath.c_str(), -1, widePath.data(), wideLength);
            return widePath;
        };

        const std::wstring widePath = toWidePath(path);
        if (widePath.empty()) {
            if (logFunc) logFunc("[USMap] Failed to convert output path to UTF-16: %s", path.c_str());
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(widePath).parent_path(), ec);

        HANDLE hFile = CreateFileW(
            widePath.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            if (logFunc) logFunc("[USMap] CreateFileW failed: %d (path: %s)", GetLastError(), path.c_str());
            return false;
        }

        DWORD bytesWritten = 0;
        BOOL success = WriteFile(hFile, buffer.data(), (DWORD)buffer.size(), &bytesWritten, NULL);
        if (!success) {
            if (logFunc) logFunc("[USMap] WriteFile failed: %d", GetLastError());
        }
        CloseHandle(hFile);

        return success && bytesWritten == buffer.size();
    }
};

class USMapNameTable {
public:

    int32_t AddName(const std::string& name) {
        auto it = m_NameToIndex.find(name);
        if (it != m_NameToIndex.end()) {
            return it->second;
        }
        int32_t idx = (int32_t)m_Names.size();
        m_Names.push_back(name);
        m_NameToIndex[name] = idx;
        return idx;
    }

    uint32_t GetCount() const { return (uint32_t)m_Names.size(); }

    void Serialize(USMapWriter& writer) const {
        writer.WriteUInt32((uint32_t)m_Names.size());
        for (const auto& name : m_Names) {
            writer.WriteString(name);
        }
    }

    const std::vector<std::string>& GetNames() const { return m_Names; }

private:
    std::vector<std::string> m_Names;
    std::unordered_map<std::string, int32_t> m_NameToIndex;
};
