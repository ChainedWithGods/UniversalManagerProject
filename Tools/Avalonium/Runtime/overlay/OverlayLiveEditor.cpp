#include "OverlayLiveEditor.h"
#include "../core/EngineDiscovery.h"
#include "../core/FunctionFinder.h"
#include "../core/ObjectIndex.h"
#include "../utils/SafeMemory.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

LiveEditorState gLiveEditor;

namespace {

std::string WideToUtf8(const wchar_t* wstr, int len)
{
    if (!wstr || len <= 0) return "";
    int req = WideCharToMultiByte(CP_UTF8, 0, wstr, len, nullptr, 0, nullptr, nullptr);
    if (req <= 0) return "";
    std::string res(req, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, len, &res[0], req, nullptr, nullptr);
    return res;
}

std::string EscapeJson(const std::string& s)
{
    std::ostringstream o;
    for (char c : s)
    {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\b') o << "\\b";
        else if (c == '\f') o << "\\f";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else if (static_cast<unsigned char>(c) <= 0x1f)
            o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
        else o << c;
    }
    return o.str();
}

}

namespace LiveEditor {

LivePropType ClassifyPropertyType(const std::string& typeName)
{
    if (typeName == "BoolProperty") return LivePropType::Bool;
    if (typeName == "ByteProperty") return LivePropType::Byte;
    if (typeName == "Int8Property") return LivePropType::Int8;
    if (typeName == "Int16Property") return LivePropType::Int16;
    if (typeName == "IntProperty") return LivePropType::Int32;
    if (typeName == "Int64Property") return LivePropType::Int64;
    if (typeName == "UInt16Property") return LivePropType::UInt16;
    if (typeName == "UInt32Property") return LivePropType::UInt32;
    if (typeName == "UInt64Property") return LivePropType::UInt64;
    if (typeName == "FloatProperty") return LivePropType::Float;
    if (typeName == "DoubleProperty") return LivePropType::Double;
    if (typeName == "StrProperty") return LivePropType::Str;
    if (typeName == "NameProperty") return LivePropType::Name;
    if (typeName == "TextProperty") return LivePropType::Text;
    if (typeName == "ObjectProperty" || typeName == "ObjectPtrProperty" || typeName == "WeakObjectProperty") return LivePropType::Object;
    if (typeName == "ClassProperty" || typeName == "ClassPtrProperty") return LivePropType::Class;
    if (typeName == "StructProperty") return LivePropType::Struct;
    if (typeName == "EnumProperty") return LivePropType::Enum;
    if (typeName == "ArrayProperty") return LivePropType::Array;
    if (typeName == "MapProperty") return LivePropType::Map;
    if (typeName == "SetProperty") return LivePropType::Set;
    return LivePropType::Unknown;
}

ImVec4 GetPropertyTypeColor(LivePropType type)
{
    switch (type)
    {
    case LivePropType::Bool:   return ImVec4(0.4f, 0.9f, 0.5f, 1.0f);
    case LivePropType::Int8:
    case LivePropType::Int16:
    case LivePropType::Int32:
    case LivePropType::Int64:
    case LivePropType::Byte:
    case LivePropType::UInt16:
    case LivePropType::UInt32:
    case LivePropType::UInt64:  return ImVec4(0.35f, 0.8f, 0.95f, 1.0f);
    case LivePropType::Float:
    case LivePropType::Double:  return ImVec4(0.4f, 0.65f, 1.0f, 1.0f);
    case LivePropType::Str:
    case LivePropType::Name:
    case LivePropType::Text:    return ImVec4(1.0f, 0.75f, 0.35f, 1.0f);
    case LivePropType::Object:
    case LivePropType::Class:   return ImVec4(0.85f, 0.5f, 0.95f, 1.0f);
    case LivePropType::Struct:  return ImVec4(0.95f, 0.9f, 0.4f, 1.0f);
    case LivePropType::Enum:    return ImVec4(0.95f, 0.6f, 0.7f, 1.0f);
    case LivePropType::Array:   return ImVec4(0.6f, 0.9f, 0.85f, 1.0f);
    default:                    return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    }
}

std::vector<std::pair<std::string, int64_t>> GetEnumNamesAndValues(void* uenum)
{
    std::vector<std::pair<std::string, int64_t>> result;
    if (!uenum) return result;

    uint8_t* enumBytes = reinterpret_cast<uint8_t*>(uenum);
    int namesOff = g_State.UEnum_NamesOff;

    if (namesOff < 0)
    {

        static const int candidateOffsets[] = { 0x40, 0x30, 0x38, 0x48, 0x50, 0x28 };
        for (int off : candidateOffsets)
        {
            void* dataPtr = nullptr;
            int32_t num = 0;
            if (SafeMem_Read(enumBytes + off, &dataPtr, sizeof(void*)) && dataPtr)
            {
                if (SafeMem_Read(enumBytes + off + sizeof(void*), &num, sizeof(int32_t)) && num > 0 && num < 2048)
                {
                    FName testName;
                    if (SafeMem_Read(dataPtr, &testName, sizeof(FName)))
                    {
                        std::string testStr = EngineDiscovery::GetCachedFName(g_State, testName.ComparisonIndex);
                        if (!testStr.empty() && testStr != "<invalid>")
                        {
                            namesOff = off;
                            g_State.UEnum_NamesOff = off;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (namesOff < 0)
        return result;

    void* namesData = nullptr;
    int32_t namesNum = 0;

    if (!SafeMem_Read(enumBytes + namesOff, &namesData, sizeof(void*)) || !namesData)
        return result;
    if (!SafeMem_Read(enumBytes + namesOff + sizeof(void*), &namesNum, sizeof(int32_t)))
        return result;

    if (namesNum <= 0 || namesNum > 2048)
        return result;

    for (int32_t i = 0; i < namesNum; ++i)
    {
        uint8_t* entryAddr = reinterpret_cast<uint8_t*>(namesData) + (i * 16);
        FName entryName;
        int64_t entryVal = i;
        if (SafeMem_Read(entryAddr, &entryName, sizeof(FName)))
        {
            SafeMem_Read(entryAddr + sizeof(FName), &entryVal, sizeof(int64_t));
            std::string nameStr = EngineDiscovery::GetCachedFName(g_State, entryName.ComparisonIndex);
            if (!nameStr.empty() && nameStr != "<invalid>")
            {
                size_t colon = nameStr.rfind("::");
                std::string shortName = (colon != std::string::npos) ? nameStr.substr(colon + 2) : nameStr;
                result.push_back({ shortName, entryVal });
            }
        }
    }
    return result;
}

static int32_t GetDefaultPropertySize(LivePropType type, int32_t currentSize = 0)
{
    if (currentSize > 0) return currentSize;
    switch (type)
    {
    case LivePropType::Bool:
    case LivePropType::Byte:
    case LivePropType::Enum:
    case LivePropType::Int8:
        return 1;
    case LivePropType::Int16:
    case LivePropType::UInt16:
        return 2;
    case LivePropType::Int32:
    case LivePropType::UInt32:
    case LivePropType::Float:
        return 4;
    case LivePropType::Int64:
    case LivePropType::UInt64:
    case LivePropType::Double:
    case LivePropType::Object:
    case LivePropType::Class:
    case LivePropType::Name:
        return 8;
    case LivePropType::Str:
    case LivePropType::Array:
        return 16;
    case LivePropType::Text:
        return 24;
    case LivePropType::Map:
    case LivePropType::Set:
        return 80;
    case LivePropType::Struct:
        return 16;
    default:
        return 4;
    }
}

std::vector<LivePropertyInfo> EnumerateStructProperties(void* ustruct)
{
    std::vector<LivePropertyInfo> properties;
    if (!ustruct) return properties;

    void* currentStruct = ustruct;

    if (g_State.UStruct_ChildPropsOff >= 0 && g_State.FField_NameOff >= 0)
    {
        while (currentStruct)
        {
            std::string structName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(currentStruct));
            void* child = EngineDiscovery::GetChildPropsHead(g_State, static_cast<UStruct*>(currentStruct));

            while (child)
            {
                std::string name = EngineDiscovery::GetFFieldName(g_State, child);
                if (name.empty()) break;

                std::string typeName = EngineDiscovery::GetFFieldTypeName(g_State, child);

                LivePropertyInfo prop;
                prop.Name = name;
                prop.TypeName = typeName;
                prop.OwnerClassName = structName;
                prop.Type = ClassifyPropertyType(typeName);
                prop.FieldPointer = child;

                if (g_State.FProperty_OffsetOff >= 0)
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &prop.Offset, 4);

                const int sizeOff = g_State.FProperty_ElementSizeOff >= 0 ? g_State.FProperty_ElementSizeOff : (g_State.UEMajor >= 5 ? 0x38 : 0x3C);
                SafeMem_Read(reinterpret_cast<uint8_t*>(child) + sizeOff, &prop.ElementSize, 4);
                if (prop.ElementSize <= 0)
                    prop.ElementSize = GetDefaultPropertySize(prop.Type);

                if (prop.Type == LivePropType::Bool && g_State.FBoolProperty_BaseOff >= 0)
                {
                    uint8_t fieldMask = 1, byteMask = 1, byteOffset = 0;
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 3, &fieldMask, 1);
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 2, &byteMask, 1);
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 1, &byteOffset, 1);
                    prop.FieldMask = fieldMask ? fieldMask : 1;
                    prop.ByteMask = byteMask ? byteMask : 1;
                    prop.ByteOffset = byteOffset;
                    prop.bIsBitfield = (prop.FieldMask != 0xFF);
                }

                if (prop.Type == LivePropType::Array && g_State.FArrayProperty_InnerOff >= 0)
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FArrayProperty_InnerOff, &prop.InnerProp, sizeof(void*));

                if (prop.Type == LivePropType::Struct)
                {
                    if (g_State.FStructProperty_StructOff >= 0)
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FStructProperty_StructOff, &prop.StructOrEnum, sizeof(void*));
                    if (!prop.StructOrEnum)
                    {
                        static const int candidateOffsets[] = { 0x78, 0x70, 0x80, 0x68, 0x48, 0x50, 0x40 };
                        for (int off : candidateOffsets)
                        {
                            void* testStruct = nullptr;
                            if (SafeMem_Read(reinterpret_cast<uint8_t*>(child) + off, &testStruct, sizeof(void*)) && testStruct)
                            {
                                uintptr_t addr = reinterpret_cast<uintptr_t>(testStruct);
                                if (addr >= 0x10000 && addr < 0x7FFFFFFFFFFFULL && IsReadableWorldExplorerObject(testStruct))
                                {
                                    std::string sName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(testStruct));
                                    if (!sName.empty() && sName[0] != '<')
                                    {
                                        prop.StructOrEnum = testStruct;
                                        if (g_State.FStructProperty_StructOff < 0)
                                            g_State.FStructProperty_StructOff = off;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if (prop.StructOrEnum)
                    {
                        std::string sName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(prop.StructOrEnum));
                        if (sName == "LinearColor" || sName == "Color4F") prop.ElementSize = 16;
                        else if (sName == "Color") prop.ElementSize = 4;
                        else if (sName == "Vector" || sName == "Rotator") prop.ElementSize = (g_State.UEMajor >= 5 ? 24 : 12);
                        else if (sName == "Vector2D") prop.ElementSize = (g_State.UEMajor >= 5 ? 16 : 8);
                        else if (sName == "Transform") prop.ElementSize = 48;
                        else if (sName == "Quat") prop.ElementSize = (g_State.UEMajor >= 5 ? 32 : 16);
                        else if (sName == "Guid") prop.ElementSize = 16;

                        if (prop.StructOrEnum != ustruct)
                        {
                            prop.InnerProperties = EnumerateStructProperties(prop.StructOrEnum);
                            int32_t innerTotal = 0;
                            for (const auto& ip : prop.InnerProperties)
                                innerTotal = std::max(innerTotal, ip.Offset + ip.ElementSize);
                            if (innerTotal > 0)
                                prop.ElementSize = innerTotal;
                        }
                    }
                }

                if (prop.Type == LivePropType::Enum || prop.Type == LivePropType::Byte)
                {
                    if (g_State.FEnumProperty_EnumOff >= 0)
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FEnumProperty_EnumOff, &prop.StructOrEnum, sizeof(void*));
                    if (!prop.StructOrEnum && g_State.FByteProperty_EnumOff >= 0)
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FByteProperty_EnumOff, &prop.StructOrEnum, sizeof(void*));
                    if (!prop.StructOrEnum)
                    {
                        static const int candidateOffsets[] = { 0x78, 0x70, 0x80, 0x68, 0x48, 0x50 };
                        for (int off : candidateOffsets)
                        {
                            void* testEnum = nullptr;
                            if (SafeMem_Read(reinterpret_cast<uint8_t*>(child) + off, &testEnum, sizeof(void*)) && testEnum)
                            {
                                uintptr_t addr = reinterpret_cast<uintptr_t>(testEnum);
                                if (addr >= 0x10000 && addr < 0x7FFFFFFFFFFFULL && IsReadableWorldExplorerObject(testEnum))
                                {
                                    std::string eName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(testEnum));
                                    if (!eName.empty() && eName[0] != '<')
                                    {
                                        prop.StructOrEnum = testEnum;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                properties.push_back(prop);
                child = EngineDiscovery::GetFFieldNext(g_State, child);
            }

            currentStruct = EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(currentStruct));
        }
    }
    else if (g_State.UStruct_ChildrenOff >= 0 && g_State.UField_NextOff >= 0)
    {
        while (currentStruct)
        {
            std::string structName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(currentStruct));
            void* child = EngineDiscovery::GetChildrenHead(g_State, static_cast<UStruct*>(currentStruct));

            while (child)
            {
                std::string name = EngineDiscovery::GetFieldName(g_State, child);
                if (name.empty()) break;

                UClass* propClass = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(child));
                std::string typeName = propClass ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(propClass)) : "Property";

                LivePropertyInfo prop;
                prop.Name = name;
                prop.TypeName = typeName;
                prop.OwnerClassName = structName;
                prop.Type = ClassifyPropertyType(typeName);
                prop.FieldPointer = child;

                if (g_State.UProperty_OffsetOff >= 0)
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.UProperty_OffsetOff, &prop.Offset, 4);

                prop.ElementSize = GetDefaultPropertySize(prop.Type);

                properties.push_back(prop);
                child = EngineDiscovery::GetFieldNext(g_State, child);
            }

            currentStruct = EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(currentStruct));
        }
    }

    std::sort(properties.begin(), properties.end(), [](const LivePropertyInfo& a, const LivePropertyInfo& b)
    {
        if (a.Offset != b.Offset)
            return a.Offset < b.Offset;
        return a.Name < b.Name;
    });

    return properties;
}

std::vector<LivePropertyInfo> EnumerateClassProperties(void* uclass)
{
    std::vector<LivePropertyInfo> properties;
    if (!uclass) return properties;

    void* currentClass = uclass;

    if (g_State.UStruct_ChildPropsOff >= 0 && g_State.FField_NameOff >= 0)
    {
        while (currentClass)
        {
            std::string currentClassName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(currentClass));
            void* child = EngineDiscovery::GetChildPropsHead(g_State, static_cast<UStruct*>(currentClass));

            while (child)
            {
                std::string name = EngineDiscovery::GetFFieldName(g_State, child);
                if (name.empty()) break;

                std::string typeName = EngineDiscovery::GetFFieldTypeName(g_State, child);

                LivePropertyInfo prop;
                prop.Name = name;
                prop.TypeName = typeName;
                prop.OwnerClassName = currentClassName;
                prop.Type = ClassifyPropertyType(typeName);
                prop.FieldPointer = child;

                if (g_State.FProperty_OffsetOff >= 0)
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &prop.Offset, 4);

                const int sizeOff = g_State.FProperty_ElementSizeOff >= 0 ? g_State.FProperty_ElementSizeOff : (g_State.UEMajor >= 5 ? 0x38 : 0x3C);
                SafeMem_Read(reinterpret_cast<uint8_t*>(child) + sizeOff, &prop.ElementSize, 4);
                if (prop.ElementSize <= 0)
                    prop.ElementSize = GetDefaultPropertySize(prop.Type);

                if (prop.Type == LivePropType::Bool && g_State.FBoolProperty_BaseOff >= 0)
                {
                    uint8_t fieldMask = 1, byteMask = 1, byteOffset = 0;
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 3, &fieldMask, 1);
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 2, &byteMask, 1);
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 1, &byteOffset, 1);
                    prop.FieldMask = fieldMask ? fieldMask : 1;
                    prop.ByteMask = byteMask ? byteMask : 1;
                    prop.ByteOffset = byteOffset;
                    prop.bIsBitfield = (prop.FieldMask != 0xFF);
                }

                if (prop.Type == LivePropType::Array && g_State.FArrayProperty_InnerOff >= 0)
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FArrayProperty_InnerOff, &prop.InnerProp, sizeof(void*));

                if (prop.Type == LivePropType::Struct)
                {
                    if (g_State.FStructProperty_StructOff >= 0)
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FStructProperty_StructOff, &prop.StructOrEnum, sizeof(void*));
                    if (!prop.StructOrEnum)
                    {
                        static const int candidateOffsets[] = { 0x78, 0x70, 0x80, 0x68, 0x48, 0x50, 0x40 };
                        for (int off : candidateOffsets)
                        {
                            void* testStruct = nullptr;
                            if (SafeMem_Read(reinterpret_cast<uint8_t*>(child) + off, &testStruct, sizeof(void*)) && testStruct)
                            {
                                uintptr_t addr = reinterpret_cast<uintptr_t>(testStruct);
                                if (addr >= 0x10000 && addr < 0x7FFFFFFFFFFFULL && IsReadableWorldExplorerObject(testStruct))
                                {
                                    std::string sName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(testStruct));
                                    if (!sName.empty() && sName[0] != '<')
                                    {
                                        prop.StructOrEnum = testStruct;
                                        if (g_State.FStructProperty_StructOff < 0)
                                            g_State.FStructProperty_StructOff = off;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if (prop.StructOrEnum)
                    {
                        std::string sName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(prop.StructOrEnum));
                        if (sName == "LinearColor" || sName == "Color4F") prop.ElementSize = 16;
                        else if (sName == "Color") prop.ElementSize = 4;
                        else if (sName == "Vector" || sName == "Rotator") prop.ElementSize = (g_State.UEMajor >= 5 ? 24 : 12);
                        else if (sName == "Vector2D") prop.ElementSize = (g_State.UEMajor >= 5 ? 16 : 8);
                        else if (sName == "Transform") prop.ElementSize = 48;
                        else if (sName == "Quat") prop.ElementSize = (g_State.UEMajor >= 5 ? 32 : 16);
                        else if (sName == "Guid") prop.ElementSize = 16;

                        prop.InnerProperties = EnumerateStructProperties(prop.StructOrEnum);
                        int32_t innerTotal = 0;
                        for (const auto& ip : prop.InnerProperties)
                            innerTotal = std::max(innerTotal, ip.Offset + ip.ElementSize);
                        if (innerTotal > 0)
                            prop.ElementSize = innerTotal;
                    }
                }

                if (prop.Type == LivePropType::Enum || prop.Type == LivePropType::Byte)
                {
                    if (g_State.FEnumProperty_EnumOff >= 0)
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FEnumProperty_EnumOff, &prop.StructOrEnum, sizeof(void*));
                    if (!prop.StructOrEnum && g_State.FByteProperty_EnumOff >= 0)
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FByteProperty_EnumOff, &prop.StructOrEnum, sizeof(void*));
                    if (!prop.StructOrEnum)
                    {
                        static const int candidateOffsets[] = { 0x78, 0x70, 0x80, 0x68, 0x48, 0x50 };
                        for (int off : candidateOffsets)
                        {
                            void* testEnum = nullptr;
                            if (SafeMem_Read(reinterpret_cast<uint8_t*>(child) + off, &testEnum, sizeof(void*)) && testEnum)
                            {
                                uintptr_t addr = reinterpret_cast<uintptr_t>(testEnum);
                                if (addr >= 0x10000 && addr < 0x7FFFFFFFFFFFULL && IsReadableWorldExplorerObject(testEnum))
                                {
                                    std::string eName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(testEnum));
                                    if (!eName.empty() && eName[0] != '<')
                                    {
                                        prop.StructOrEnum = testEnum;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                properties.push_back(prop);
                child = EngineDiscovery::GetFFieldNext(g_State, child);
            }

            currentClass = EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(currentClass));
        }
    }
    else if (g_State.UStruct_ChildrenOff >= 0 && g_State.UField_NextOff >= 0)
    {
        while (currentClass)
        {
            std::string currentClassName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(currentClass));
            void* child = EngineDiscovery::GetChildrenHead(g_State, static_cast<UStruct*>(currentClass));

            while (child)
            {
                std::string name = EngineDiscovery::GetFieldName(g_State, child);
                if (name.empty()) break;

                UClass* propClass = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(child));
                std::string typeName = propClass ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(propClass)) : "Property";

                LivePropertyInfo prop;
                prop.Name = name;
                prop.TypeName = typeName;
                prop.OwnerClassName = currentClassName;
                prop.Type = ClassifyPropertyType(typeName);
                prop.FieldPointer = child;

                if (g_State.UProperty_OffsetOff >= 0)
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.UProperty_OffsetOff, &prop.Offset, 4);

                prop.ElementSize = GetDefaultPropertySize(prop.Type);

                properties.push_back(prop);
                child = EngineDiscovery::GetFieldNext(g_State, child);
            }

            currentClass = EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(currentClass));
        }
    }

    std::sort(properties.begin(), properties.end(), [](const LivePropertyInfo& a, const LivePropertyInfo& b)
    {
        if (a.Offset != b.Offset)
            return a.Offset < b.Offset;
        return a.Name < b.Name;
    });

    return properties;
}

std::vector<LiveFunctionInfo> EnumerateClassFunctions(void* uclass)
{
    std::vector<LiveFunctionInfo> functions;
    if (!uclass) return functions;

    void* currentClass = uclass;

    while (currentClass)
    {
        void* child = EngineDiscovery::GetChildrenHead(g_State, static_cast<UStruct*>(currentClass));
        while (child)
        {
            UClass* childClass = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(child));
            std::string childClassName = childClass ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(childClass)) : "";

            if (childClassName == "Function" || childClass == g_State.UFunctionClass)
            {
                LiveFunctionInfo func;
                func.FunctionPtr = child;
                func.Name = EngineDiscovery::GetFieldName(g_State, child);
                func.FullName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(child));

                if (g_State.UFunction_FlagsOff >= 0)
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.UFunction_FlagsOff, &func.FunctionFlags, 4);

                if (g_State.UStruct_ChildPropsOff >= 0 && g_State.FField_NameOff >= 0)
                {
                    void* paramProp = EngineDiscovery::GetChildPropsHead(g_State, static_cast<UStruct*>(child));
                    while (paramProp)
                    {
                        LiveFunctionParam param;
                        param.Name = EngineDiscovery::GetFFieldName(g_State, paramProp);
                        param.TypeName = EngineDiscovery::GetFFieldTypeName(g_State, paramProp);
                        param.Type = ClassifyPropertyType(param.TypeName);

                        if (g_State.FProperty_OffsetOff >= 0)
                            SafeMem_Read(reinterpret_cast<uint8_t*>(paramProp) + g_State.FProperty_OffsetOff, &param.Offset, 4);
                        if (g_State.FProperty_ElementSizeOff >= 0)
                            SafeMem_Read(reinterpret_cast<uint8_t*>(paramProp) + g_State.FProperty_ElementSizeOff, &param.ElementSize, 4);
                        if (g_State.FProperty_PropertyFlagsOff >= 0)
                            SafeMem_Read(reinterpret_cast<uint8_t*>(paramProp) + g_State.FProperty_PropertyFlagsOff, &param.PropertyFlags, 8);

                        param.bIsOutParam = (param.PropertyFlags & 0x0000000000000100ULL) != 0;
                        param.bIsReturnParam = (param.PropertyFlags & 0x0000000000000400ULL) != 0;

                        if (param.Offset + param.ElementSize > func.ParmsSize)
                            func.ParmsSize = param.Offset + param.ElementSize;

                        if (param.bIsReturnParam)
                            func.ReturnValueOffset = param.Offset;

                        func.Params.push_back(param);
                        paramProp = EngineDiscovery::GetFFieldNext(g_State, paramProp);
                    }
                }
                else if (g_State.UStruct_ChildrenOff >= 0 && g_State.UField_NextOff >= 0)
                {
                    void* paramProp = EngineDiscovery::GetChildrenHead(g_State, static_cast<UStruct*>(child));
                    while (paramProp)
                    {
                        UClass* pClass = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(paramProp));
                        std::string pClassName = pClass ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(pClass)) : "";

                        if (pClassName != "Function")
                        {
                            LiveFunctionParam param;
                            param.Name = EngineDiscovery::GetFieldName(g_State, paramProp);
                            param.TypeName = pClassName;
                            param.Type = ClassifyPropertyType(pClassName);

                            if (g_State.UProperty_OffsetOff >= 0)
                                SafeMem_Read(reinterpret_cast<uint8_t*>(paramProp) + g_State.UProperty_OffsetOff, &param.Offset, 4);

                            func.Params.push_back(param);
                        }
                        paramProp = EngineDiscovery::GetFieldNext(g_State, paramProp);
                    }
                }

                functions.push_back(func);
            }
            child = EngineDiscovery::GetFieldNext(g_State, child);
        }
        currentClass = EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(currentClass));
    }

    std::sort(functions.begin(), functions.end(), [](const LiveFunctionInfo& a, const LiveFunctionInfo& b)
    {
        return a.Name < b.Name;
    });

    return functions;
}

void Initialize()
{
    gLiveEditor.CurrentObject = nullptr;
    gLiveEditor.CurrentClass = nullptr;
    gLiveEditor.Properties.clear();
    gLiveEditor.Functions.clear();
    gLiveEditor.Components.clear();
    gLiveEditor.History.clear();
    gLiveEditor.HistoryIndex = -1;
    LoadWatchesFromDisk();
}

void NavigateHistory(int direction)
{
    if (gLiveEditor.History.empty()) return;
    int target = gLiveEditor.HistoryIndex + direction;
    if (target >= 0 && target < static_cast<int>(gLiveEditor.History.size()))
    {
        gLiveEditor.HistoryIndex = target;
        void* targetObj = gLiveEditor.History[target];
        if (IsReadableWorldExplorerObject(targetObj))
        {
            gWorldExplorer.SelectedObject = targetObj;
            OnSelectObject(targetObj);
        }
    }
}

void AddObjectToHistory(void* object)
{
    if (!object) return;
    if (gLiveEditor.HistoryIndex >= 0 && gLiveEditor.HistoryIndex < static_cast<int>(gLiveEditor.History.size()))
    {
        if (gLiveEditor.History[gLiveEditor.HistoryIndex] == object)
            return;
        gLiveEditor.History.resize(gLiveEditor.HistoryIndex + 1);
    }
    gLiveEditor.History.push_back(object);
    gLiveEditor.HistoryIndex = static_cast<int>(gLiveEditor.History.size()) - 1;
}

void OnSelectObject(void* object)
{
    if (gLiveEditor.CurrentObject == object && gLiveEditor.CurrentClass != nullptr)
        return;

    gLiveEditor.CurrentObject = object;
    gLiveEditor.EditingPropertyOffset = -1;
    gLiveEditor.EditingPropertyName.clear();

    if (!object || !g_State.bInitialized || !IsReadableWorldExplorerObject(object))
    {
        gLiveEditor.CurrentObject = nullptr;
        gLiveEditor.CurrentClass = nullptr;
        gLiveEditor.Properties.clear();
        gLiveEditor.Functions.clear();
        gLiveEditor.Components.clear();
        return;
    }

    AddObjectToHistory(object);

    void* objClass = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(object));
    gLiveEditor.CurrentClass = objClass;

    RefreshProperties(object);
    RefreshFunctions(object);
    RefreshComponents(object);
}

void RefreshProperties(void* object)
{
    gLiveEditor.Properties.clear();
    if (!object || !gLiveEditor.CurrentClass) return;
    gLiveEditor.Properties = EnumerateClassProperties(gLiveEditor.CurrentClass);
}

void RefreshFunctions(void* object)
{
    gLiveEditor.Functions.clear();
    if (!object || !gLiveEditor.CurrentClass) return;
    gLiveEditor.Functions = EnumerateClassFunctions(gLiveEditor.CurrentClass);
}

void RefreshComponents(void* object)
{
    gLiveEditor.Components.clear();
    if (!object || !gLiveEditor.CurrentClass) return;

    if (!IsActorClass(gLiveEditor.CurrentClass))
        return;

    for (const auto& prop : gLiveEditor.Properties)
    {
        if (prop.Type == LivePropType::Object)
        {
            void* compPtr = nullptr;
            if (SafeMem_Read(reinterpret_cast<uint8_t*>(object) + prop.Offset, &compPtr, sizeof(compPtr)) && compPtr)
            {
                if (IsReadableWorldExplorerObject(compPtr))
                {
                    void* compCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(compPtr));
                    std::string compClsName = compCls ? ResolveCachedClassName(compCls) : "<unknown>";

                    if (compClsName.find("Component") != std::string::npos || prop.Name.find("Component") != std::string::npos || prop.Name == "RootComponent")
                    {
                        LiveActorComponentEntry entry;
                        entry.ComponentPtr = compPtr;
                        entry.Name = prop.Name + " -> " + ResolveObjectName(compPtr);
                        entry.ClassName = compClsName;
                        entry.FullName = ResolveObjectFullName(compPtr);
                        entry.IsRootComponent = (prop.Name == "RootComponent");
                        gLiveEditor.Components.push_back(entry);
                    }
                }
            }
        }
    }
}

std::string ReadLivePropertyValueStr(void* object, const LivePropertyInfo& prop, int32_t parentOffset)
{
    if (!object || prop.Offset < 0) return "<invalid>";

    const uint8_t* addr = reinterpret_cast<const uint8_t*>(object) + parentOffset + prop.Offset;

    const size_t checkSize = static_cast<size_t>(std::clamp(prop.ElementSize, 1, 16));
    if (!SafeMem_IsReadable(addr, checkSize))
        return "<unreadable>";

    char buf[256] = {};

    switch (prop.Type)
    {
    case LivePropType::Bool:
    {
        uint8_t byteVal = 0;
        if (!SafeMem_Read(addr + prop.ByteOffset, &byteVal, 1)) return "<unreadable>";
        bool bVal = (byteVal & prop.FieldMask) != 0;
        return bVal ? "true" : "false";
    }
    case LivePropType::Byte:
    case LivePropType::Enum:
    {
        uint8_t byteVal = 0;
        if (!SafeMem_Read(addr, &byteVal, 1)) return "<unreadable>";
        if (prop.StructOrEnum)
        {
            auto enumList = GetEnumNamesAndValues(prop.StructOrEnum);
            for (const auto& entry : enumList)
            {
                if (entry.second == byteVal)
                {
                    snprintf(buf, sizeof(buf), "%s (%u)", entry.first.c_str(), byteVal);
                    return buf;
                }
            }
        }
        snprintf(buf, sizeof(buf), "%u (0x%02X)", byteVal, byteVal);
        return buf;
    }
    case LivePropType::Int8:
    {
        int8_t v = 0;
        if (!SafeMem_Read(addr, &v, 1)) return "<unreadable>";
        return std::to_string(v);
    }
    case LivePropType::Int16:
    {
        int16_t v = 0;
        if (!SafeMem_Read(addr, &v, 2)) return "<unreadable>";
        return std::to_string(v);
    }
    case LivePropType::Int32:
    {
        int32_t v = 0;
        if (!SafeMem_Read(addr, &v, 4)) return "<unreadable>";
        return std::to_string(v);
    }
    case LivePropType::Int64:
    {
        int64_t v = 0;
        if (!SafeMem_Read(addr, &v, 8)) return "<unreadable>";
        return std::to_string(v);
    }
    case LivePropType::UInt16:
    {
        uint16_t v = 0;
        if (!SafeMem_Read(addr, &v, 2)) return "<unreadable>";
        return std::to_string(v);
    }
    case LivePropType::UInt32:
    {
        uint32_t v = 0;
        if (!SafeMem_Read(addr, &v, 4)) return "<unreadable>";
        return std::to_string(v);
    }
    case LivePropType::UInt64:
    {
        uint64_t v = 0;
        if (!SafeMem_Read(addr, &v, 8)) return "<unreadable>";
        return std::to_string(v);
    }
    case LivePropType::Float:
    {
        float v = 0.0f;
        if (!SafeMem_Read(addr, &v, 4)) return "<unreadable>";
        snprintf(buf, sizeof(buf), "%.4f", v);
        return buf;
    }
    case LivePropType::Double:
    {
        double v = 0.0;
        if (!SafeMem_Read(addr, &v, 8)) return "<unreadable>";
        snprintf(buf, sizeof(buf), "%.6f", v);
        return buf;
    }
    case LivePropType::Str:
    {
        wchar_t* strData = nullptr;
        int32_t strCount = 0;
        SafeMem_Read(addr, &strData, sizeof(strData));
        SafeMem_Read(addr + sizeof(strData), &strCount, 4);
        if (strData && strCount > 0 && strCount < 4096)
        {
            if (SafeMem_IsReadable(strData, strCount * sizeof(wchar_t)))
                return "\"" + WideToUtf8(strData, strCount - 1) + "\"";
        }
        return "\"\"";
    }
    case LivePropType::Name:
    {
        int32_t comparisonIndex = 0;
        int32_t number = 0;
        SafeMem_Read(addr, &comparisonIndex, 4);
        SafeMem_Read(addr + 4, &number, 4);
        std::string nameStr = EngineDiscovery::GetCachedFName(g_State, comparisonIndex);
        if (number > 0)
            nameStr += "_" + std::to_string(number - 1);
        return "\"" + nameStr + "\"";
    }
    case LivePropType::Object:
    case LivePropType::Class:
    {
        void* ptr = nullptr;
        SafeMem_Read(addr, &ptr, sizeof(ptr));
        if (!ptr) return "nullptr (0x0)";
        if (IsReadableWorldExplorerObject(ptr))
            return FormatPointer(ptr) + " (" + ResolveObjectName(ptr) + ")";
        return FormatPointer(ptr);
    }
    case LivePropType::Struct:
    {
        std::string sName;
        if (prop.StructOrEnum)
        {
            sName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(prop.StructOrEnum));
        }

        if (prop.ElementSize == 16 || sName == "LinearColor" || sName == "Color4F")
        {
            float r = 0, g = 0, b = 0, a = 0;
            if (SafeMem_Read(addr, &r, 4) && SafeMem_Read(addr + 4, &g, 4) && SafeMem_Read(addr + 8, &b, 4) && SafeMem_Read(addr + 12, &a, 4))
            {
                snprintf(buf, sizeof(buf), "(R=%.2f, G=%.2f, B=%.2f, A=%.2f)", r, g, b, a);
                return buf;
            }
        }
        else if (prop.ElementSize == 4 || sName == "Color")
        {
            uint8_t r = 0, g = 0, b = 0, a = 0;
            if (SafeMem_Read(addr, &r, 1) && SafeMem_Read(addr + 1, &g, 1) && SafeMem_Read(addr + 2, &b, 1) && SafeMem_Read(addr + 3, &a, 1))
            {
                snprintf(buf, sizeof(buf), "(R=%u, G=%u, B=%u, A=%u)", r, g, b, a);
                return buf;
            }
        }
        else if (prop.ElementSize == 12 || (prop.ElementSize == 24 && g_State.UEMajor >= 5) || sName == "Vector" || sName == "Rotator")
        {
            if (g_State.UEMajor >= 5 && prop.ElementSize == 24)
            {
                double x = 0, y = 0, z = 0;
                if (SafeMem_Read(addr, &x, 8) && SafeMem_Read(addr + 8, &y, 8) && SafeMem_Read(addr + 16, &z, 8))
                {
                    snprintf(buf, sizeof(buf), "(X=%.2f, Y=%.2f, Z=%.2f)", x, y, z);
                    return buf;
                }
            }
            else
            {
                float x = 0, y = 0, z = 0;
                if (SafeMem_Read(addr, &x, 4) && SafeMem_Read(addr + 4, &y, 4) && SafeMem_Read(addr + 8, &z, 4))
                {
                    snprintf(buf, sizeof(buf), "(X=%.2f, Y=%.2f, Z=%.2f)", x, y, z);
                    return buf;
                }
            }
        }
        else if (prop.ElementSize == 8 || (prop.ElementSize == 16 && g_State.UEMajor >= 5) || sName == "Vector2D")
        {
            if (g_State.UEMajor >= 5 && prop.ElementSize == 16)
            {
                double x = 0, y = 0;
                if (SafeMem_Read(addr, &x, 8) && SafeMem_Read(addr + 8, &y, 8))
                {
                    snprintf(buf, sizeof(buf), "(X=%.2f, Y=%.2f)", x, y);
                    return buf;
                }
            }
            else
            {
                float x = 0, y = 0;
                if (SafeMem_Read(addr, &x, 4) && SafeMem_Read(addr + 4, &y, 4))
                {
                    snprintf(buf, sizeof(buf), "(X=%.2f, Y=%.2f)", x, y);
                    return buf;
                }
            }
        }

        if (!prop.InnerProperties.empty())
        {
            std::string label = sName.empty() ? "Struct" : sName;
            snprintf(buf, sizeof(buf), "%s (%d fields)", label.c_str(), static_cast<int>(prop.InnerProperties.size()));
            return buf;
        }

        std::string label = sName.empty() ? "Struct" : sName;
        snprintf(buf, sizeof(buf), "%s (%d bytes)", label.c_str(), prop.ElementSize);
        return buf;
    }
    case LivePropType::Array:
    {
        int32_t count = 0;
        SafeMem_Read(addr + sizeof(void*), &count, 4);
        snprintf(buf, sizeof(buf), "[Array Num=%d]", count);
        return buf;
    }
    default:
    {
        uint64_t rawVal = 0;
        SafeMem_Read(addr, &rawVal, sizeof(rawVal));
        snprintf(buf, sizeof(buf), "0x%llX", rawVal);
        return buf;
    }
    }
}

static std::string TrimString(const std::string& str)
{
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static bool ResolveStringToFName(const std::string& rawInput, FName& outName)
{
    std::string str = TrimString(rawInput);
    if (str.empty())
        return false;

    if (str.size() >= 2 && str.front() == '"' && str.back() == '"')
    {
        str = str.substr(1, str.size() - 2);
        str = TrimString(str);
    }
    if (str.empty())
        return false;

    if (str == "None" || str == "none" || str == "NONE")
    {
        outName.ComparisonIndex = 0;
        outName.Number = 0;
        return true;
    }

    std::string base = str;
    int32_t suffixNumber = -1;
    size_t lastUnderscore = str.rfind('_');
    if (lastUnderscore != std::string::npos && lastUnderscore + 1 < str.size())
    {
        bool allDigits = true;
        for (size_t k = lastUnderscore + 1; k < str.size(); ++k)
        {
            if (!isdigit(static_cast<unsigned char>(str[k])))
            {
                allDigits = false;
                break;
            }
        }
        if (allDigits)
        {
            try {
                suffixNumber = std::stoi(str.substr(lastUnderscore + 1));
                base = str.substr(0, lastUnderscore);
            } catch (...) {}
        }
    }

    int32_t exactIdx = EngineDiscovery::ResolveFNameIndex(g_State, str);
    if (exactIdx >= 0)
    {
        outName.ComparisonIndex = exactIdx;
        outName.Number = 0;
        return true;
    }

    if (suffixNumber >= 0)
    {
        int32_t baseIdx = EngineDiscovery::ResolveFNameIndex(g_State, base);
        if (baseIdx >= 0)
        {
            outName.ComparisonIndex = baseIdx;
            outName.Number = suffixNumber + 1;
            return true;
        }
    }

    std::string lowerStr = ToLowerCopy(str);
    std::string lowerBase = ToLowerCopy(base);

    if (g_State.IndexMutex)
    {
        std::lock_guard<std::recursive_mutex> lock(*g_State.IndexMutex);
        for (size_t i = 0; i < g_State.FNameCacheArray.size(); ++i)
        {
            const char* name = g_State.FNameCacheArray[i];
            if (!name || !name[0]) continue;
            std::string lowerKey = ToLowerCopy(std::string(name));
            if (lowerKey == lowerStr)
            {
                outName.ComparisonIndex = static_cast<int32_t>(i);
                outName.Number = 0;
                return true;
            }
            if (suffixNumber >= 0 && lowerKey == lowerBase)
            {
                outName.ComparisonIndex = static_cast<int32_t>(i);
                outName.Number = suffixNumber + 1;
                return true;
            }
        }
    }

    return false;
}

bool WriteLivePropertyValue(void* object, const LivePropertyInfo& prop, const std::string& newValueStr, int32_t parentOffset)
{
    if (!object || prop.Offset < 0) return false;

    uint8_t* addr = reinterpret_cast<uint8_t*>(object) + parentOffset + prop.Offset;
    if (!SafeMem_IsReadable(addr, std::max(1, prop.ElementSize)))
        return false;

    std::string trimmed = TrimString(newValueStr);
    if (trimmed.empty() && prop.Type != LivePropType::Str && prop.Type != LivePropType::Name && prop.Type != LivePropType::Text)
        return false;

    DWORD oldProtect = 0;
    size_t writeSize = std::max(1, prop.ElementSize > 0 ? prop.ElementSize : 8);
    VirtualProtect(addr, writeSize, PAGE_EXECUTE_READWRITE, &oldProtect);

    bool written = false;
    try
    {
        switch (prop.Type)
        {
        case LivePropType::Bool:
        {
            bool bNew = (trimmed == "true" || trimmed == "1" || trimmed == "True" || trimmed == "TRUE" || trimmed == "[ON] true");
            uint8_t currentByte = 0;
            SafeMem_Read(addr + prop.ByteOffset, &currentByte, 1);
            if (bNew)
                currentByte |= prop.FieldMask;
            else
                currentByte &= ~prop.FieldMask;
            *reinterpret_cast<uint8_t*>(addr + prop.ByteOffset) = currentByte;
            written = true;
            break;
        }
        case LivePropType::Byte:
        case LivePropType::Enum:
        {
            if (prop.StructOrEnum)
            {
                auto enumList = GetEnumNamesAndValues(prop.StructOrEnum);
                for (const auto& entry : enumList)
                {
                    if (entry.first == trimmed || ToLowerCopy(entry.first) == ToLowerCopy(trimmed))
                    {
                        *reinterpret_cast<uint8_t*>(addr) = static_cast<uint8_t>(entry.second);
                        written = true;
                        break;
                    }
                }
            }
            if (!written)
            {
                int val = std::stoi(trimmed, nullptr, 0);
                *reinterpret_cast<uint8_t*>(addr) = static_cast<uint8_t>(val);
                written = true;
            }
            break;
        }
        case LivePropType::Int8:
        {
            int val = std::stoi(trimmed, nullptr, 0);
            *reinterpret_cast<int8_t*>(addr) = static_cast<int8_t>(val);
            written = true;
            break;
        }
        case LivePropType::Int16:
        case LivePropType::UInt16:
        {
            int val = std::stoi(trimmed, nullptr, 0);
            *reinterpret_cast<int16_t*>(addr) = static_cast<int16_t>(val);
            written = true;
            break;
        }
        case LivePropType::Int32:
        case LivePropType::UInt32:
        {
            long val = std::stol(trimmed, nullptr, 0);
            *reinterpret_cast<int32_t*>(addr) = static_cast<int32_t>(val);
            written = true;
            break;
        }
        case LivePropType::Int64:
        case LivePropType::UInt64:
        {
            long long val = std::stoll(trimmed, nullptr, 0);
            *reinterpret_cast<int64_t*>(addr) = val;
            written = true;
            break;
        }
        case LivePropType::Float:
        {
            std::string fStr = trimmed;
            std::replace(fStr.begin(), fStr.end(), ',', '.');
            float val = std::stof(fStr);
            *reinterpret_cast<float*>(addr) = val;
            written = true;
            break;
        }
        case LivePropType::Double:
        {
            std::string dStr = trimmed;
            std::replace(dStr.begin(), dStr.end(), ',', '.');
            double val = std::stod(dStr);
            *reinterpret_cast<double*>(addr) = val;
            written = true;
            break;
        }
        case LivePropType::Object:
        case LivePropType::Class:
        {
            uintptr_t ptrVal = 0;
            if (trimmed != "nullptr" && trimmed != "0" && trimmed != "NULL")
            {
                ptrVal = std::stoull(trimmed, nullptr, 16);
            }
            *reinterpret_cast<void**>(addr) = reinterpret_cast<void*>(ptrVal);
            written = true;
            break;
        }
        case LivePropType::Name:
        {
            FName newName;
            if (ResolveStringToFName(trimmed, newName))
            {
                *reinterpret_cast<FName*>(addr) = newName;
                written = true;
            }
            break;
        }
        case LivePropType::Str:
        {
            std::string unquoted = trimmed;
            if (unquoted.size() >= 2 && unquoted.front() == '"' && unquoted.back() == '"')
                unquoted = unquoted.substr(1, unquoted.size() - 2);

            wchar_t* strData = nullptr;
            int32_t strMax = 0;
            SafeMem_Read(addr, &strData, sizeof(strData));
            SafeMem_Read(addr + sizeof(strData) + 4, &strMax, 4);
            if (strData && strMax > 0)
            {
                int wlen = MultiByteToWideChar(CP_UTF8, 0, unquoted.c_str(), -1, nullptr, 0);
                if (wlen > 0 && wlen <= strMax)
                {
                    MultiByteToWideChar(CP_UTF8, 0, unquoted.c_str(), -1, strData, strMax);
                    *reinterpret_cast<int32_t*>(addr + sizeof(strData)) = wlen;
                    written = true;
                }
            }
            break;
        }
        case LivePropType::Struct:
        {
            if (prop.ElementSize == 16)
            {
                float f1 = 0, f2 = 0, f3 = 0, f4 = 1.0f;
                if (sscanf_s(trimmed.c_str(), "(R=%f, G=%f, B=%f, A=%f)", &f1, &f2, &f3, &f4) >= 3 ||
                    sscanf_s(trimmed.c_str(), "(X=%f, Y=%f, Z=%f, W=%f)", &f1, &f2, &f3, &f4) >= 3 ||
                    sscanf_s(trimmed.c_str(), "%f %f %f %f", &f1, &f2, &f3, &f4) >= 3 ||
                    sscanf_s(trimmed.c_str(), "%f, %f, %f, %f", &f1, &f2, &f3, &f4) >= 3)
                {
                    *reinterpret_cast<float*>(addr) = f1;
                    *reinterpret_cast<float*>(addr + 4) = f2;
                    *reinterpret_cast<float*>(addr + 8) = f3;
                    *reinterpret_cast<float*>(addr + 12) = f4;
                    written = true;
                }
            }
            else if (prop.ElementSize == 12)
            {
                float f1 = 0, f2 = 0, f3 = 0;
                if (sscanf_s(trimmed.c_str(), "(X=%f, Y=%f, Z=%f)", &f1, &f2, &f3) == 3 ||
                    sscanf_s(trimmed.c_str(), "(R=%f, G=%f, B=%f)", &f1, &f2, &f3) == 3 ||
                    sscanf_s(trimmed.c_str(), "(Pitch=%f, Yaw=%f, Roll=%f)", &f1, &f2, &f3) == 3 ||
                    sscanf_s(trimmed.c_str(), "%f %f %f", &f1, &f2, &f3) == 3 ||
                    sscanf_s(trimmed.c_str(), "%f, %f, %f", &f1, &f2, &f3) == 3)
                {
                    *reinterpret_cast<float*>(addr) = f1;
                    *reinterpret_cast<float*>(addr + 4) = f2;
                    *reinterpret_cast<float*>(addr + 8) = f3;
                    written = true;
                }
            }
            else if (prop.ElementSize == 4)
            {
                unsigned int u1 = 0, u2 = 0, u3 = 0, u4 = 255;
                if (sscanf_s(trimmed.c_str(), "(R=%u, G=%u, B=%u, A=%u)", &u1, &u2, &u3, &u4) >= 3 ||
                    sscanf_s(trimmed.c_str(), "%u %u %u %u", &u1, &u2, &u3, &u4) >= 3)
                {
                    uint8_t b1 = (uint8_t)u1, b2 = (uint8_t)u2, b3 = (uint8_t)u3, b4 = (uint8_t)u4;
                    *reinterpret_cast<uint8_t*>(addr) = b1;
                    *reinterpret_cast<uint8_t*>(addr + 1) = b2;
                    *reinterpret_cast<uint8_t*>(addr + 2) = b3;
                    *reinterpret_cast<uint8_t*>(addr + 3) = b4;
                    written = true;
                }
            }
            break;
        }
        default:
            break;
        }
    }
    catch (...)
    {
        written = false;
    }

    if (oldProtect != 0)
    {
        VirtualProtect(addr, writeSize, oldProtect, &oldProtect);
    }

    if (written)
    {
        gLiveEditor.ActionStatus = "Saved " + prop.Name + " = " + trimmed;
        gLiveEditor.ActionStatusTimer = 3.0f;
    }
    else
    {
        gLiveEditor.ActionStatus = "Failed to write " + prop.Name;
        gLiveEditor.ActionStatusTimer = 3.0f;
    }

    return written;
}

bool ExecuteLiveFunction(void* object, LiveFunctionInfo& func)
{
    if (!object || !func.FunctionPtr || !g_State.bInitialized)
        return false;

    auto startTime = std::chrono::high_resolution_clock::now();

    std::vector<uint8_t> parmBuffer(static_cast<size_t>(std::max(64, func.ParmsSize + 32)), 0);

    for (const auto& param : func.Params)
    {
        if (param.bIsReturnParam) continue;

        uint8_t* pAddr = parmBuffer.data() + param.Offset;

        try
        {
            switch (param.Type)
            {
            case LivePropType::Bool:
                *reinterpret_cast<uint32_t*>(pAddr) = param.BoolValue ? 1 : 0;
                break;
            case LivePropType::Byte:
            case LivePropType::Int8:
                *reinterpret_cast<uint8_t*>(pAddr) = static_cast<uint8_t>(param.IntValue);
                break;
            case LivePropType::Int16:
                *reinterpret_cast<int16_t*>(pAddr) = static_cast<int16_t>(param.IntValue);
                break;
            case LivePropType::Int32:
                *reinterpret_cast<int32_t*>(pAddr) = static_cast<int32_t>(param.IntValue);
                break;
            case LivePropType::Int64:
                *reinterpret_cast<int64_t*>(pAddr) = param.IntValue;
                break;
            case LivePropType::Float:
                *reinterpret_cast<float*>(pAddr) = static_cast<float>(param.FloatValue);
                break;
            case LivePropType::Double:
                *reinterpret_cast<double*>(pAddr) = param.FloatValue;
                break;
            case LivePropType::Object:
                if (param.InputBuffer[0])
                {
                    uintptr_t ptrVal = std::stoull(param.InputBuffer, nullptr, 16);
                    *reinterpret_cast<void**>(pAddr) = reinterpret_cast<void*>(ptrVal);
                }
                break;
            default:
                break;
            }
        }
        catch (...) {}
    }

    bool ok = EngineDiscovery::CallProcessEvent(
        g_State,
        static_cast<UObject*>(object),
        static_cast<UFunction*>(func.FunctionPtr),
        parmBuffer.data()
    );

    auto endTime = std::chrono::high_resolution_clock::now();
    func.ExecutionTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    func.ExecutionSuccess = ok;
    func.HasExecuted = true;

    if (ok)
    {
        if (func.ReturnValueOffset >= 0 && func.ReturnValueOffset < static_cast<int32_t>(parmBuffer.size()))
        {
            uint64_t retVal = *reinterpret_cast<uint64_t*>(parmBuffer.data() + func.ReturnValueOffset);
            char resBuf[128] = {};
            snprintf(resBuf, sizeof(resBuf), "Return: 0x%llX (%lld)", retVal, static_cast<long long>(retVal));
            func.LastResult = resBuf;
        }
        else
        {
            func.LastResult = "Success (void return)";
        }
    }
    else
    {
        func.LastResult = "Execution Failed (ProcessEvent error)";
    }

    return ok;
}

void AddPropertyToWatches(void* object, const LivePropertyInfo& prop, int32_t parentOffset)
{
    if (!object) return;

    int32_t totalOffset = parentOffset + prop.Offset;

    for (const auto& w : gLiveEditor.Watches)
    {
        if (w.ObjectPtr == object && w.Offset == totalOffset && w.PropertyName == prop.Name)
            return;
    }

    LiveWatchItem item;
    item.ObjectPtr = object;
    item.ObjectName = ResolveObjectName(object);
    item.ObjectPath = ResolveObjectFullName(object);
    void* cls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(object));
    item.ClassName = cls ? ResolveCachedClassName(cls) : "";
    item.PropertyName = prop.Name;
    item.TypeName = prop.TypeName;
    item.Offset = totalOffset;
    item.Type = prop.Type;
    item.bIsBitfield = prop.bIsBitfield;
    item.FieldMask = prop.FieldMask;
    item.ByteOffset = prop.ByteOffset;
    item.Enabled = true;
    item.CurrentValueStr = ReadLivePropertyValueStr(object, prop, parentOffset);
    item.LastValueStr = item.CurrentValueStr;

    gLiveEditor.Watches.push_back(item);
    gLiveEditor.ActionStatus = "Pinned \"" + prop.Name + "\" to Watch list.";
    gLiveEditor.ActionStatusTimer = 3.0f;

    SaveWatchesToDisk();
}

void AddFunctionToWatches(void* object, const LiveFunctionInfo& func)
{
    if (!object || !func.FunctionPtr) return;

    for (const auto& w : gLiveEditor.Watches)
    {
        if (w.ObjectPtr == object && w.PropertyName == func.Name && w.IsFunctionWatch)
            return;
    }

    LiveWatchItem item;
    item.ObjectPtr = object;
    item.ObjectName = ResolveObjectName(object);
    item.ObjectPath = ResolveObjectFullName(object);
    void* cls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(object));
    item.ClassName = cls ? ResolveCachedClassName(cls) : "";
    item.PropertyName = func.Name;
    item.TypeName = "UFunction";
    item.Offset = 0;
    item.Type = LivePropType::Unknown;
    item.IsFunctionWatch = true;
    item.Enabled = true;
    item.CurrentValueStr = "Callable (0 calls)";
    item.LastValueStr = item.CurrentValueStr;

    gLiveEditor.Watches.push_back(item);
    gLiveEditor.ActionStatus = "Pinned Function \"" + func.Name + "\" to Watch list.";
    gLiveEditor.ActionStatusTimer = 3.0f;

    SaveWatchesToDisk();
}

void RemoveWatch(size_t index)
{
    if (index < gLiveEditor.Watches.size())
    {
        gLiveEditor.Watches.erase(gLiveEditor.Watches.begin() + index);
        SaveWatchesToDisk();
    }
}

void ClearAllWatches()
{
    gLiveEditor.Watches.clear();
    SaveWatchesToDisk();
}

void SaveWatchesToDisk()
{
    std::ofstream ofs("Avalonium_Watches.json");
    if (!ofs.is_open()) return;

    ofs << "[\n";
    for (size_t i = 0; i < gLiveEditor.Watches.size(); ++i)
    {
        const auto& w = gLiveEditor.Watches[i];
        ofs << "  {\n";
        ofs << "    \"ObjectName\": \"" << EscapeJson(w.ObjectName) << "\",\n";
        ofs << "    \"ObjectPath\": \"" << EscapeJson(w.ObjectPath) << "\",\n";
        ofs << "    \"ClassName\": \"" << EscapeJson(w.ClassName) << "\",\n";
        ofs << "    \"PropertyName\": \"" << EscapeJson(w.PropertyName) << "\",\n";
        ofs << "    \"TypeName\": \"" << EscapeJson(w.TypeName) << "\",\n";
        ofs << "    \"Offset\": " << w.Offset << ",\n";
        ofs << "    \"Type\": " << static_cast<int>(w.Type) << ",\n";
        ofs << "    \"Enabled\": " << (w.Enabled ? "true" : "false") << ",\n";
        ofs << "    \"LogToFile\": " << (w.LogToFile ? "true" : "false") << ",\n";
        ofs << "    \"bIsBitfield\": " << (w.bIsBitfield ? "true" : "false") << ",\n";
        ofs << "    \"FieldMask\": " << static_cast<int>(w.FieldMask) << ",\n";
        ofs << "    \"ByteOffset\": " << static_cast<int>(w.ByteOffset) << ",\n";
        ofs << "    \"IsFunctionWatch\": " << (w.IsFunctionWatch ? "true" : "false") << "\n";
        ofs << "  }" << (i + 1 < gLiveEditor.Watches.size() ? ",\n" : "\n");
    }
    ofs << "]\n";
}

void LoadWatchesFromDisk()
{
    std::ifstream ifs("Avalonium_Watches.json");
    if (!ifs.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (content.empty()) return;

    gLiveEditor.Watches.clear();

    size_t pos = 0;
    while ((pos = content.find('{', pos)) != std::string::npos)
    {
        size_t endPos = content.find('}', pos);
        if (endPos == std::string::npos) break;

        std::string objBlock = content.substr(pos, endPos - pos + 1);

        auto ExtractStr = [&](const std::string& key) -> std::string {
            std::string needle = "\"" + key + "\": \"";
            size_t kpos = objBlock.find(needle);
            if (kpos == std::string::npos) return "";
            size_t valStart = kpos + needle.length();
            size_t valEnd = objBlock.find('"', valStart);
            if (valEnd == std::string::npos) return "";
            return objBlock.substr(valStart, valEnd - valStart);
        };

        auto ExtractInt = [&](const std::string& key, int defaultVal = 0) -> int {
            std::string needle = "\"" + key + "\": ";
            size_t kpos = objBlock.find(needle);
            if (kpos == std::string::npos) return defaultVal;
            size_t valStart = kpos + needle.length();
            try { return std::stoi(objBlock.substr(valStart)); } catch (...) { return defaultVal; }
        };

        auto ExtractBool = [&](const std::string& key, bool defaultVal = false) -> bool {
            std::string needle = "\"" + key + "\": ";
            size_t kpos = objBlock.find(needle);
            if (kpos == std::string::npos) return defaultVal;
            size_t valStart = kpos + needle.length();
            return (objBlock.substr(valStart, 4) == "true");
        };

        LiveWatchItem item;
        item.ObjectName = ExtractStr("ObjectName");
        item.ObjectPath = ExtractStr("ObjectPath");
        item.ClassName = ExtractStr("ClassName");
        item.PropertyName = ExtractStr("PropertyName");
        item.TypeName = ExtractStr("TypeName");
        item.Offset = ExtractInt("Offset");
        item.Type = static_cast<LivePropType>(ExtractInt("Type"));
        item.Enabled = ExtractBool("Enabled", true);
        item.LogToFile = ExtractBool("LogToFile", false);
        item.bIsBitfield = ExtractBool("bIsBitfield", false);
        item.FieldMask = static_cast<uint8_t>(ExtractInt("FieldMask", 1));
        item.ByteOffset = static_cast<uint8_t>(ExtractInt("ByteOffset", 0));
        item.IsFunctionWatch = ExtractBool("IsFunctionWatch", false);

        if (g_State.bInitialized)
        {
            if (!item.ObjectPath.empty())
                item.ObjectPtr = EngineDiscovery::FindObjectByNameFast(g_State, item.ObjectPath);
            if (!item.ObjectPtr && !item.ObjectName.empty())
                item.ObjectPtr = EngineDiscovery::FindObjectByNameFast(g_State, item.ObjectName);
        }

        gLiveEditor.Watches.push_back(item);
        pos = endPos + 1;
    }
}

void UpdateWatches()
{
    if (!g_State.bInitialized || gLiveEditor.Watches.empty())
        return;

    SafeMem_SetFastMode(1);

    auto GetCurrentTimeString = []() -> std::string {
        time_t rawtime;
        struct tm timeinfo;
        time(&rawtime);
        localtime_s(&timeinfo, &rawtime);
        char buffer[64];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return std::string(buffer);
    };

    for (auto& w : gLiveEditor.Watches)
    {
        if (!w.Enabled)
            continue;

        if (!w.ObjectPtr || !IsReadableWorldExplorerObject(w.ObjectPtr))
        {
            if (!w.ObjectPath.empty())
                w.ObjectPtr = EngineDiscovery::FindObjectByNameFast(g_State, w.ObjectPath);
            if (!w.ObjectPtr && !w.ObjectName.empty())
                w.ObjectPtr = EngineDiscovery::FindObjectByNameFast(g_State, w.ObjectName);
        }

        if (w.ObjectPtr && IsReadableWorldExplorerObject(w.ObjectPtr))
        {
            if (w.IsFunctionWatch)
            {
                char fbuf[128];
                snprintf(fbuf, sizeof(fbuf), "Callable (%u calls)", w.InvocationCount);
                w.CurrentValueStr = fbuf;
            }
            else
            {
                LivePropertyInfo dummy;
                dummy.Name = w.PropertyName;
                dummy.Offset = w.Offset;
                dummy.Type = w.Type;
                dummy.bIsBitfield = w.bIsBitfield;
                dummy.FieldMask = w.FieldMask;
                dummy.ByteOffset = w.ByteOffset;
                dummy.ElementSize = 8;

                std::string newVal = ReadLivePropertyValueStr(w.ObjectPtr, dummy);
                if (!w.CurrentValueStr.empty() && newVal != w.CurrentValueStr && newVal != "<unreadable>" && newVal != "<invalid>")
                {
                    std::string timeStr = GetCurrentTimeString();
                    std::string historyEntry = "[" + timeStr + "] " + w.CurrentValueStr + " -> " + newVal;
                    w.History.push_back(historyEntry);
                    if (w.History.size() > 50)
                        w.History.erase(w.History.begin());

                    if (w.LogToFile)
                    {
                        CreateDirectoryA("Avalonium_Watch_Logs", NULL);
                        std::string safeName = w.ObjectName + "_" + w.PropertyName;
                        for (char& c : safeName) { if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_'; }
                        std::string logPath = "Avalonium_Watch_Logs/" + safeName + ".log";
                        std::ofstream ofs(logPath, std::ios::app);
                        if (ofs.is_open())
                        {
                            ofs << "[" << timeStr << "] " << w.ObjectName << "." << w.PropertyName << " = " << newVal << " (was " << w.CurrentValueStr << ")\n";
                        }
                    }
                }
                w.CurrentValueStr = newVal;
            }
        }
        else
        {
            w.CurrentValueStr = "<object not found>";
        }
    }

    SafeMem_SetFastMode(0);
}

void DrawNavigationBreadcrumbs()
{
    const bool canGoBack = (gLiveEditor.HistoryIndex > 0);
    const bool canGoForward = (gLiveEditor.HistoryIndex >= 0 && gLiveEditor.HistoryIndex < static_cast<int>(gLiveEditor.History.size()) - 1);

    if (!canGoBack) ImGui::BeginDisabled();
    if (ImGui::Button("< Back"))
        NavigateHistory(-1);
    if (!canGoBack) ImGui::EndDisabled();

    ImGui::SameLine();

    if (!canGoForward) ImGui::BeginDisabled();
    if (ImGui::Button("Forward >"))
        NavigateHistory(1);
    if (!canGoForward) ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled("| History: %d/%d", gLiveEditor.HistoryIndex + 1, static_cast<int>(gLiveEditor.History.size()));

    if (!gLiveEditor.ActionStatus.empty())
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.85f, 0.45f, 1.0f));
        ImGui::TextUnformatted(gLiveEditor.ActionStatus.c_str());
        ImGui::PopStyleColor();
    }
}

static void DrawPropertyRow(void* selectedObj, const LivePropertyInfo& prop, int32_t parentOffset, int depth, int& propIdx, void*& pendingJumpObject)
{
    ImGui::PushID(propIdx++);
    ImGui::TableNextRow();

    const int32_t effectiveOffset = parentOffset + prop.Offset;

    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", FormatOffset(effectiveOffset).c_str());

    ImGui::TableSetColumnIndex(1);
    if (depth > 0)
    {
        ImGui::Indent(depth * 14.0f);
    }

    const bool isStructWithInner = (prop.Type == LivePropType::Struct && !prop.InnerProperties.empty());
    bool nodeOpen = false;

    if (isStructWithInner)
    {
        std::string label = prop.Name + " (" + std::to_string(prop.InnerProperties.size()) + " fields)";
        nodeOpen = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen);
    }
    else
    {
        if (depth > 0)
            ImGui::TextDisabled("-> %s", prop.Name.c_str());
        else
            ImGui::TextUnformatted(prop.Name.c_str());
    }

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Owner: %s\nOffset: 0x%X (+0x%X in parent)\nSize: %d bytes\nRight-click for options",
                          prop.OwnerClassName.c_str(), effectiveOffset, prop.Offset, prop.ElementSize);

    if (ImGui::BeginPopupContextItem("##PropContextMenu"))
    {
        if (ImGui::MenuItem("Copy Property Name"))
            ImGui::SetClipboardText(prop.Name.c_str());
        if (ImGui::MenuItem("Copy Offset Hex"))
            ImGui::SetClipboardText(FormatOffset(effectiveOffset).c_str());
        if (ImGui::MenuItem("Copy Offset Decimal"))
            ImGui::SetClipboardText(std::to_string(effectiveOffset).c_str());
        if (ImGui::MenuItem("Copy Current Value"))
            ImGui::SetClipboardText(ReadLivePropertyValueStr(selectedObj, prop, parentOffset).c_str());
        if (ImGui::MenuItem("Pin to Watch List"))
            AddPropertyToWatches(selectedObj, prop, parentOffset);
        ImGui::EndPopup();
    }

    if (depth > 0)
    {
        ImGui::Unindent(depth * 14.0f);
    }

    ImGui::TableSetColumnIndex(2);
    ImVec4 typeColor = GetPropertyTypeColor(prop.Type);
    ImGui::PushStyleColor(ImGuiCol_Text, typeColor);
    ImGui::TextUnformatted(prop.TypeName.c_str());
    ImGui::PopStyleColor();

    ImGui::TableSetColumnIndex(3);

    if (prop.Type == LivePropType::Bool)
    {
        std::string curVal = ReadLivePropertyValueStr(selectedObj, prop, parentOffset);
        bool bChecked = (curVal == "true");
        if (ImGui::Checkbox("##InlineBoolToggle", &bChecked))
        {
            WriteLivePropertyValue(selectedObj, prop, bChecked ? "true" : "false", parentOffset);
        }
        ImGui::SameLine();
        ImGui::TextColored(bChecked ? ImVec4(0.35f, 0.95f, 0.45f, 1.0f) : ImVec4(0.85f, 0.35f, 0.35f, 1.0f),
            bChecked ? "[ON] true" : "[OFF] false");
    }
    else if ((prop.Type == LivePropType::Enum || (prop.Type == LivePropType::Byte && prop.StructOrEnum)) && prop.StructOrEnum)
    {

        auto enumList = GetEnumNamesAndValues(prop.StructOrEnum);
        uint8_t byteVal = 0;
        SafeMem_Read(reinterpret_cast<uint8_t*>(selectedObj) + effectiveOffset, &byteVal, 1);

        int currentIdx = -1;
        for (int ei = 0; ei < static_cast<int>(enumList.size()); ++ei)
        {
            if (enumList[ei].second == byteVal)
            {
                currentIdx = ei;
                break;
            }
        }

        std::string comboPreview = (currentIdx >= 0) ? enumList[currentIdx].first + " (" + std::to_string(byteVal) + ")" : std::to_string(byteVal);
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("##EnumValDropdown", comboPreview.c_str()))
        {
            for (int ei = 0; ei < static_cast<int>(enumList.size()); ++ei)
            {
                const bool isSelected = (ei == currentIdx);
                std::string itemLabel = enumList[ei].first + " (" + std::to_string(enumList[ei].second) + ")";
                if (ImGui::Selectable(itemLabel.c_str(), isSelected))
                {
                    WriteLivePropertyValue(selectedObj, prop, std::to_string(enumList[ei].second), parentOffset);
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
    else
    {
        std::string curVal = ReadLivePropertyValueStr(selectedObj, prop, parentOffset);
        const bool isEditingThis = (gLiveEditor.EditingPropertyOffset == effectiveOffset && gLiveEditor.EditingPropertyName == prop.Name);

        if (isEditingThis)
        {
            ImGui::SetNextItemWidth(-90.0f);
            if (ImGui::InputText("##InlineValEdit", gLiveEditor.TempEditBuffer, sizeof(gLiveEditor.TempEditBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                WriteLivePropertyValue(selectedObj, prop, gLiveEditor.TempEditBuffer, parentOffset);
                gLiveEditor.EditingPropertyOffset = -1;
                gLiveEditor.EditingPropertyName.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("Set"))
            {
                WriteLivePropertyValue(selectedObj, prop, gLiveEditor.TempEditBuffer, parentOffset);
                gLiveEditor.EditingPropertyOffset = -1;
                gLiveEditor.EditingPropertyName.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("X"))
            {
                gLiveEditor.EditingPropertyOffset = -1;
                gLiveEditor.EditingPropertyName.clear();
            }
        }
        else
        {
            ImGui::TextUnformatted(curVal.c_str());

            if ((prop.Type == LivePropType::Object || prop.Type == LivePropType::Class) && curVal.find("0x") != std::string::npos && curVal.find("nullptr") == std::string::npos)
            {
                void* targetPtr = nullptr;
                SafeMem_Read(reinterpret_cast<uint8_t*>(selectedObj) + effectiveOffset, &targetPtr, sizeof(targetPtr));
                if (targetPtr && IsReadableWorldExplorerObject(targetPtr))
                {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Jump ->"))
                    {
                        pendingJumpObject = targetPtr;
                    }
                }
            }
        }
    }

    ImGui::TableSetColumnIndex(4);
    if (ImGui::SmallButton("Edit"))
    {
        gLiveEditor.EditingPropertyOffset = effectiveOffset;
        gLiveEditor.EditingPropertyName = prop.Name;
        std::string cur = ReadLivePropertyValueStr(selectedObj, prop, parentOffset);
        strncpy(gLiveEditor.TempEditBuffer, cur.c_str(), sizeof(gLiveEditor.TempEditBuffer) - 1);
        gLiveEditor.TempEditBuffer[sizeof(gLiveEditor.TempEditBuffer) - 1] = '\0';
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+Watch"))
    {
        AddPropertyToWatches(selectedObj, prop, parentOffset);
    }

    ImGui::PopID();

    if (isStructWithInner && nodeOpen)
    {
        for (const auto& innerProp : prop.InnerProperties)
        {
            DrawPropertyRow(selectedObj, innerProp, effectiveOffset, depth + 1, propIdx, pendingJumpObject);
        }
        ImGui::TreePop();
    }
}

void DrawLivePropertiesTab()
{
    void* selectedObj = gLiveEditor.CurrentObject;
    if (!selectedObj)
    {
        DrawMutedText("Select an object on the left to inspect and edit its live memory values.");
        return;
    }

    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("##LivePropFilter", "Filter properties (name, type)...", gLiveEditor.PropertyFilter, sizeof(gLiveEditor.PropertyFilter));
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
        RefreshProperties(selectedObj);
    ImGui::SameLine();
    ImGui::Checkbox("Live Polling (60 FPS)", &gLiveEditor.AutoRefreshLiveValues);
    ImGui::SameLine();
    ImGui::Checkbox("Show Gaps (Unreflected Data)", &gLiveEditor.ShowUnreflectedGaps);
    ImGui::SameLine();
    ImGui::Checkbox("Limit Results##LivePropLimitToggle", &gLiveEditor.EnableLimit);
    if (gLiveEditor.EnableLimit)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(75.0f);
        ImGui::InputInt("##LivePropCustomLimit", &gLiveEditor.CustomLimitValue, 10, 50);
        if (gLiveEditor.CustomLimitValue < 1) gLiveEditor.CustomLimitValue = 1;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%d properties)", static_cast<int>(gLiveEditor.Properties.size()));

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    const ImGuiTableFlags tableFlags =
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInnerH |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("LivePropertiesEditorTable", 5, tableFlags, ImVec2(0.0f, 0.0f)))
    {
        ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 1.3f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Live Memory Value", ImGuiTableColumnFlags_WidthStretch, 1.8f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();

        void* pendingJumpObject = nullptr;
        int propIdx = 0;
        int displayedProps = 0;
        const int maxProps = (gLiveEditor.EnableLimit && gLiveEditor.CustomLimitValue > 0) ? gLiveEditor.CustomLimitValue : 1000000;
        int32_t lastEndOffset = 0;

        for (size_t i = 0; i < gLiveEditor.Properties.size(); ++i)
        {
            if (displayedProps >= maxProps)
                break;

            const auto& prop = gLiveEditor.Properties[i];

            if (gLiveEditor.PropertyFilter[0] &&
                !ContainsCaseInsensitive(prop.Name, gLiveEditor.PropertyFilter) &&
                !ContainsCaseInsensitive(prop.TypeName, gLiveEditor.PropertyFilter) &&
                !ContainsCaseInsensitive(prop.OwnerClassName, gLiveEditor.PropertyFilter))
            {
                continue;
            }

            if (gLiveEditor.ShowUnreflectedGaps && prop.Offset > lastEndOffset && lastEndOffset > 0 && !gLiveEditor.PropertyFilter[0])
            {
                const int32_t gapSize = prop.Offset - lastEndOffset;
                if (gapSize > 0 && gapSize <= 0x10000)
                {
                    ImGui::PushID(propIdx++);
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("%s", FormatOffset(lastEndOffset).c_str());

                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.65f, 0.7f, 0.85f));
                    ImGui::Text("[Gap: +0x%X (%d bytes)] Unreflected Data", gapSize, gapSize);
                    ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Unreflected Memory Gap\nStart Offset: 0x%X\nEnd Offset: 0x%X\nGap Size: %d bytes (0x%X)\nLikely padding or non-reflected C++ member",
                                          lastEndOffset, prop.Offset, gapSize, gapSize);

                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextDisabled("Padding / Gap");

                    ImGui::TableSetColumnIndex(3);
                    uint8_t gapSample[8] = {};
                    int32_t sampleLen = std::min(8, gapSize);
                    if (SafeMem_Read(reinterpret_cast<uint8_t*>(selectedObj) + lastEndOffset, gapSample, sampleLen))
                    {
                        char hexBuf[64] = {};
                        int cur = 0;
                        for (int k = 0; k < sampleLen; ++k)
                            cur += snprintf(hexBuf + cur, sizeof(hexBuf) - cur, "%02X ", gapSample[k]);
                        if (gapSize > 8)
                            snprintf(hexBuf + cur, sizeof(hexBuf) - cur, "...");
                        ImGui::TextDisabled("%s", hexBuf);
                    }
                    else
                    {
                        ImGui::TextDisabled("<unreadable>");
                    }

                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextDisabled("-");

                    ImGui::PopID();
                }
            }

            DrawPropertyRow(selectedObj, prop, 0, 0, propIdx, pendingJumpObject);
            displayedProps++;

            lastEndOffset = prop.Offset + std::max(1, prop.ElementSize);
        }

        ImGui::EndTable();

        if (pendingJumpObject && IsReadableWorldExplorerObject(pendingJumpObject))
        {
            gWorldExplorer.SelectedObject = pendingJumpObject;
            OnSelectObject(pendingJumpObject);
        }
    }
}

void DrawLiveFunctionCallerTab()
{
    void* selectedObj = gLiveEditor.CurrentObject;
    if (!selectedObj)
    {
        DrawMutedText("Select an object to inspect its callable UFunctions and invoke them live.");
        return;
    }

    ImGui::SetNextItemWidth(320.0f);
    ImGui::InputTextWithHint("##FuncFilter", "Filter functions...", gLiveEditor.FunctionFilter, sizeof(gLiveEditor.FunctionFilter));
    ImGui::SameLine();
    ImGui::Checkbox("Limit Results##LiveFuncLimitToggle", &gLiveEditor.EnableLimit);
    if (gLiveEditor.EnableLimit)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(75.0f);
        ImGui::InputInt("##LiveFuncCustomLimit", &gLiveEditor.CustomLimitValue, 10, 50);
        if (gLiveEditor.CustomLimitValue < 1) gLiveEditor.CustomLimitValue = 1;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%d total functions discovered)", static_cast<int>(gLiveEditor.Functions.size()));

    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::BeginChild("FunctionsListPanel", ImVec2(0.0f, 0.0f), true);

    int funcIdx = 0;
    int displayedFuncs = 0;
    const int maxFuncs = (gLiveEditor.EnableLimit && gLiveEditor.CustomLimitValue > 0) ? gLiveEditor.CustomLimitValue : 1000000;

    for (auto& func : gLiveEditor.Functions)
    {
        if (displayedFuncs >= maxFuncs)
            break;

        if (gLiveEditor.FunctionFilter[0] && !ContainsCaseInsensitive(func.Name, gLiveEditor.FunctionFilter))
            continue;

        displayedFuncs++;
        ImGui::PushID(funcIdx++);

        const bool isExec = (func.FunctionFlags & 0x00000200) != 0;
        const bool isNative = (func.FunctionFlags & 0x00000400) != 0;

        std::string headerLabel = func.Name;
        if (isExec) headerLabel += " [Exec]";
        if (isNative) headerLabel += " [Native]";
        if (!func.Params.empty())
            headerLabel += " (" + std::to_string(func.Params.size()) + " params)";

        if (ImGui::CollapsingHeader(headerLabel.c_str()))
        {
            ImGui::Indent(12.0f);
            ImGui::TextDisabled("Flags: 0x%08X | Size: %d bytes", func.FunctionFlags, func.ParmsSize);
            ImGui::SameLine();
            if (ImGui::SmallButton("+Watch Function##WatchFuncBtn"))
            {
                AddFunctionToWatches(selectedObj, func);
            }

            if (!func.Params.empty())
            {
                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                ImGui::TextColored(ImVec4(0.8f, 0.85f, 1.0f, 1.0f), "Parameters:");

                int paramIdx = 0;
                for (auto& param : func.Params)
                {
                    ImGui::PushID(paramIdx++);
                    std::string pLabel = param.Name + " (" + param.TypeName + ")";
                    if (param.bIsReturnParam) pLabel += " [Return]";
                    else if (param.bIsOutParam) pLabel += " [Out]";

                    if (param.bIsReturnParam)
                    {
                        ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f), "  %s", pLabel.c_str());
                    }
                    else if (param.Type == LivePropType::Bool)
                    {
                        ImGui::Checkbox(pLabel.c_str(), &param.BoolValue);
                    }
                    else if (param.Type == LivePropType::Float || param.Type == LivePropType::Double)
                    {
                        float fVal = static_cast<float>(param.FloatValue);
                        if (ImGui::DragFloat(pLabel.c_str(), &fVal, 0.1f))
                            param.FloatValue = fVal;
                    }
                    else if (param.Type == LivePropType::Int32 || param.Type == LivePropType::Int64 || param.Type == LivePropType::Byte)
                    {
                        int iVal = static_cast<int>(param.IntValue);
                        if (ImGui::DragInt(pLabel.c_str(), &iVal, 1))
                            param.IntValue = iVal;
                    }
                    else
                    {
                        ImGui::InputText(pLabel.c_str(), param.InputBuffer, sizeof(param.InputBuffer));
                    }
                    ImGui::PopID();
                }
            }

            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            if (ImGui::Button("▶ Execute Function", ImVec2(170.0f, 26.0f)))
            {
                ExecuteLiveFunction(selectedObj, func);
            }

            if (func.HasExecuted)
            {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, func.ExecutionSuccess ? ImVec4(0.35f, 0.95f, 0.45f, 1.0f) : ImVec4(0.95f, 0.35f, 0.35f, 1.0f));
                ImGui::Text("%s [%.2f ms]", func.LastResult.c_str(), func.ExecutionTimeMs);
                ImGui::PopStyleColor();
            }

            ImGui::Unindent(12.0f);
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
}

void DrawLiveWatchesTab()
{
    ImGui::AlignTextToFramePadding();
    if (ImGui::Button("Save Watches JSON"))
    {
        SaveWatchesToDisk();
        gLiveEditor.ActionStatus = "Watches saved to Avalonium_Watches.json";
        gLiveEditor.ActionStatusTimer = 3.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Watches JSON"))
    {
        LoadWatchesFromDisk();
        gLiveEditor.ActionStatus = "Watches loaded from Avalonium_Watches.json";
        gLiveEditor.ActionStatusTimer = 3.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All"))
    {
        ClearAllWatches();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("| %d active watches", static_cast<int>(gLiveEditor.Watches.size()));

    if (gLiveEditor.Watches.empty())
    {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        DrawMutedText("No properties pinned yet. Click '+Watch' next to any property in the Properties or Search tabs to monitor it here in real-time.");
        return;
    }

    UpdateWatches();

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    const ImGuiTableFlags tableFlags =
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInnerH |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("LiveWatchesTable", 7, tableFlags, ImVec2(0.0f, 0.0f)))
    {
        ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_WidthFixed, 45.0f);
        ImGui::TableSetupColumn("Object", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Property / Func", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 75.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Live Value / Status", ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableHeadersRow();

        void* pendingJumpObject = nullptr;

        for (size_t i = 0; i < gLiveEditor.Watches.size(); ++i)
        {
            auto& w = gLiveEditor.Watches[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            if (ImGui::Checkbox("##WatchActiveToggle", &w.Enabled))
            {
                SaveWatchesToDisk();
            }

            ImGui::TableSetColumnIndex(1);
            if (!w.Enabled) ImGui::BeginDisabled();
            ImGui::TextUnformatted(w.ObjectName.c_str());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Object Address: %s\nFull Path: %s\nClass: %s",
                                  FormatPointer(w.ObjectPtr).c_str(), w.ObjectPath.c_str(), w.ClassName.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(w.PropertyName.c_str());

            ImGui::TableSetColumnIndex(3);
            if (w.IsFunctionWatch)
                ImGui::TextDisabled("-");
            else
                ImGui::TextDisabled("%s", FormatOffset(w.Offset).c_str());

            ImGui::TableSetColumnIndex(4);
            ImVec4 typeColor = w.IsFunctionWatch ? ImVec4(0.85f, 0.4f, 0.9f, 1.0f) : GetPropertyTypeColor(w.Type);
            ImGui::PushStyleColor(ImGuiCol_Text, typeColor);
            ImGui::TextUnformatted(w.TypeName.c_str());
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(5);
            if (w.IsEditing)
            {
                ImGui::SetNextItemWidth(-70.0f);
                if (ImGui::InputText("##WatchInlineValEdit", w.EditBuffer, sizeof(w.EditBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    LivePropertyInfo dummy;
                    dummy.Name = w.PropertyName;
                    dummy.Offset = w.Offset;
                    dummy.Type = w.Type;
                    dummy.bIsBitfield = w.bIsBitfield;
                    dummy.FieldMask = w.FieldMask;
                    dummy.ByteOffset = w.ByteOffset;
                    dummy.ElementSize = 8;
                    WriteLivePropertyValue(w.ObjectPtr, dummy, w.EditBuffer);
                    w.IsEditing = false;
                }
                ImGui::SameLine();
                if (ImGui::Button("Set"))
                {
                    LivePropertyInfo dummy;
                    dummy.Name = w.PropertyName;
                    dummy.Offset = w.Offset;
                    dummy.Type = w.Type;
                    dummy.bIsBitfield = w.bIsBitfield;
                    dummy.FieldMask = w.FieldMask;
                    dummy.ByteOffset = w.ByteOffset;
                    dummy.ElementSize = 8;
                    WriteLivePropertyValue(w.ObjectPtr, dummy, w.EditBuffer);
                    w.IsEditing = false;
                }
                ImGui::SameLine();
                if (ImGui::Button("X"))
                {
                    w.IsEditing = false;
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "%s", w.CurrentValueStr.c_str());
            }

            ImGui::TableSetColumnIndex(6);
            if (!w.IsFunctionWatch)
            {
                if (ImGui::SmallButton("Edit##WatchEdit"))
                {
                    w.IsEditing = true;
                    strncpy(w.EditBuffer, w.CurrentValueStr.c_str(), sizeof(w.EditBuffer) - 1);
                    w.EditBuffer[sizeof(w.EditBuffer) - 1] = '\0';
                }
                ImGui::SameLine();
            }

            if (ImGui::SmallButton("Jump##WatchJump"))
            {
                if (IsReadableWorldExplorerObject(w.ObjectPtr))
                {
                    pendingJumpObject = w.ObjectPtr;
                }
            }

            ImGui::SameLine();
            std::string histLabel = w.ShowHistory ? "Hist▲" : "Hist▼";
            if (ImGui::SmallButton((histLabel + "##WatchHist").c_str()))
            {
                w.ShowHistory = !w.ShowHistory;
            }

            ImGui::SameLine();
            if (ImGui::Checkbox("Log##WatchLogFile", &w.LogToFile))
            {
                SaveWatchesToDisk();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Log value changes with timestamps to Avalonium_Watch_Logs/*.log");

            ImGui::SameLine();
            if (ImGui::SmallButton("Del##WatchDel"))
            {
                RemoveWatch(i);
                if (!w.Enabled) ImGui::EndDisabled();
                ImGui::PopID();
                break;
            }

            if (!w.Enabled) ImGui::EndDisabled();

            if (w.ShowHistory)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(1);
                ImGui::Indent(12.0f);
                ImGui::TextDisabled("Value History (%d recorded changes):", static_cast<int>(w.History.size()));

                if (w.History.empty())
                {
                    ImGui::TextDisabled("  (No changes recorded yet)");
                }
                else
                {
                    for (int h = static_cast<int>(w.History.size()) - 1; h >= 0; --h)
                    {
                        ImGui::TextColored(ImVec4(0.75f, 0.85f, 0.95f, 0.9f), "  %s", w.History[h].c_str());
                    }
                }
                ImGui::Unindent(12.0f);
            }

            ImGui::PopID();
        }

        ImGui::EndTable();

        if (pendingJumpObject && IsReadableWorldExplorerObject(pendingJumpObject))
        {
            gWorldExplorer.SelectedObject = pendingJumpObject;
            OnSelectObject(pendingJumpObject);
        }
    }
}

void DrawLiveComponentsTab()
{
    void* selectedObj = gLiveEditor.CurrentObject;
    if (!selectedObj)
    {
        DrawMutedText("Select an Actor on the left to inspect its attached components.");
        return;
    }

    if (gLiveEditor.Components.empty())
    {
        DrawMutedText("No subcomponents discovered for the current selection.");
        return;
    }

    ImGui::TextDisabled("Attached Components (%d discovered):", static_cast<int>(gLiveEditor.Components.size()));
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    void* pendingSelectComponent = nullptr;
    int compIdx = 0;
    for (const auto& comp : gLiveEditor.Components)
    {
        ImGui::PushID(compIdx++);

        std::string label = (comp.IsRootComponent ? "[Root] " : "  -> ") + comp.Name + " (" + comp.ClassName + ")";
        if (ImGui::Selectable(label.c_str(), false))
        {
            pendingSelectComponent = comp.ComponentPtr;
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Component Pointer: %s\nFull Path: %s\nClick to inspect component", FormatPointer(comp.ComponentPtr).c_str(), comp.FullName.c_str());

        ImGui::PopID();
    }

    if (pendingSelectComponent && IsReadableWorldExplorerObject(pendingSelectComponent))
    {
        gWorldExplorer.SelectedObject = pendingSelectComponent;
        OnSelectObject(pendingSelectComponent);
    }
}

void DrawLiveEditorTab()
{
    static bool s_LiveEditorInitialized = false;
    if (!s_LiveEditorInitialized)
    {
        s_LiveEditorInitialized = true;
        Initialize();
    }

    if (gWorldExplorer.SelectedObject && gWorldExplorer.SelectedObject != gLiveEditor.CurrentObject)
    {
        OnSelectObject(gWorldExplorer.SelectedObject);
    }
    else if (gLiveEditor.CurrentObject && gLiveEditor.CurrentObject != gWorldExplorer.SelectedObject)
    {
        gWorldExplorer.SelectedObject = gLiveEditor.CurrentObject;
    }

    DrawNavigationBreadcrumbs();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    if (gLiveEditor.CurrentObject)
    {
        std::string objName = ResolveObjectName(gLiveEditor.CurrentObject);
        std::string clsName = gLiveEditor.CurrentClass ? ResolveObjectName(gLiveEditor.CurrentClass) : "UnknownClass";
        std::string ptrStr = FormatPointer(gLiveEditor.CurrentObject);

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Target:");
        ImGui::SameLine();
        ImGui::TextUnformatted(objName.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("| Class: %s | %s", clsName.c_str(), ptrStr.c_str());
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
    }

    if (ImGui::BeginTabBar("LiveEditorWorkspaceTabs", ImGuiTabBarFlags_FittingPolicyResizeDown))
    {
        if (ImGui::BeginTabItem("Live Properties"))
        {
            DrawLivePropertiesTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Function Caller"))
        {
            DrawLiveFunctionCallerTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Watches"))
        {
            DrawLiveWatchesTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Components"))
        {
            DrawLiveComponentsTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

}
