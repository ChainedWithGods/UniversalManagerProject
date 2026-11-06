#include "LuaUObject.h"
#include "LuaTypes.h"
#include "LuaCompatApi.h"
#include "../core/BlueprintDiscovery.h"
#include "../core/EngineDiscovery.h"
#include "LuaHookManager.h"
#include "../utils/SafeMemory.h"
#include "../console/ConsoleInternal.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>

extern EngineState g_State;

namespace LuaEngine {

static const char* UOBJECT_MT = "Avalonium.UObject";
static const char* UFUNCTION_DISPATCH_MT = "Avalonium.UFunctionDispatch";

struct UFunctionDispatchContext {
    UObject* TargetObject = nullptr;
    void* TargetFunction = nullptr;
};

using FoundProperty = FoundPropertyEx;

static FoundProperty FindPropertyOnClass(UClass* cls, const std::string& propName);
static int PushPropertyValue(lua_State* L, uint8_t* baseAddr, const FoundProperty& prop,
                             LuaCallFrame* frame = nullptr);
static bool SetPropertyValue(lua_State* L, uint8_t* baseAddr, const FoundProperty& prop, int valIdx);

static bool WriteStructFromTable(lua_State* L, uint8_t* structAddr, void* scriptStruct,
                                 int tableIdx);

static void PushStructView(lua_State* L, uint8_t* structAddr, const FoundProperty& prop,
                           LuaCallFrame* frame = nullptr);
static bool WriteArrayFromTable(lua_State* L, uint8_t* propAddr, const FoundProperty& prop,
                                int tableIdx);

static bool PushMathStructValue(lua_State* L, const FoundProperty& prop, uint8_t* propAddr);

static UStruct* FieldOwnerOf(UObject* o);

struct PropertyCacheKey {
    UClass* Class;
    std::string PropName;
    bool operator==(const PropertyCacheKey& o) const {
        return Class == o.Class && _stricmp(PropName.c_str(), o.PropName.c_str()) == 0;
    }
};

struct PropertyCacheKeyHash {
    std::size_t operator()(const PropertyCacheKey& k) const {
        std::size_t h1 = std::hash<void*>()(k.Class);
        std::size_t h2 = 0;
        for (char c : k.PropName) {
            h2 = (h2 * 131) + tolower(static_cast<unsigned char>(c));
        }
        return h1 ^ (h2 << 1);
    }
};

static void* FindFunctionOnClass(UClass* cls, const std::string& funcName);

static std::unordered_map<PropertyCacheKey, FoundProperty, PropertyCacheKeyHash> s_PropertyCache;
static std::mutex s_PropertyCacheMutex;

FoundPropertyEx FindPropertyOnClassEx(UClass* cls, const std::string& propName) {
    return FindPropertyOnClass(cls, propName);
}

void* FindFunctionOnClassEx(UClass* cls, const std::string& funcName) {
    return FindFunctionOnClass(cls, funcName);
}

int PushPropertyValueEx(lua_State* L, uint8_t* baseAddr, const FoundPropertyEx& prop) {
    return PushPropertyValue(L, baseAddr, prop);
}

bool SetPropertyValueEx(lua_State* L, uint8_t* baseAddr, const FoundPropertyEx& prop, int valIdx) {
    return SetPropertyValue(L, baseAddr, prop, valIdx);
}

static FoundProperty FindPropertyOnClass(UClass* cls, const std::string& propName) {
    if (!cls || propName.empty()) return {};

    PropertyCacheKey key{ cls, propName };
    {
        std::lock_guard<std::mutex> lock(s_PropertyCacheMutex);
        auto it = s_PropertyCache.find(key);
        if (it != s_PropertyCache.end()) {
            return it->second;
        }
    }

    FastMemScope fastMem;
    FoundProperty result;
    UStruct* cur = static_cast<UStruct*>(cls);
    while (cur) {
        if (g_State.UStruct_ChildPropsOff >= 0) {
            void* child = EngineDiscovery::GetChildPropsHead(g_State, cur);
            while (child) {
                std::string name = EngineDiscovery::GetFFieldName(g_State, child);
                if (_stricmp(name.c_str(), propName.c_str()) == 0) {
                    result.Prop = child;
                    result.TypeName = EngineDiscovery::GetFFieldTypeName(g_State, child);
                    if (g_State.FProperty_OffsetOff >= 0) {
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &result.Offset, 4);
                    }
                    if (g_State.FProperty_ElementSizeOff >= 0) {
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_ElementSizeOff, &result.ElementSize, 4);
                    }
                    if (result.TypeName.find("BoolProperty") != std::string::npos && g_State.FBoolProperty_BaseOff >= 0) {
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 1, &result.ByteOffset, 1);
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 2, &result.ByteMask, 1);
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 3, &result.FieldMask, 1);
                        if (result.ByteMask == 0) result.ByteMask = 1;
                        if (result.FieldMask == 0) result.FieldMask = result.ByteMask;
                    }
                    if (g_State.FArrayProperty_InnerOff >= 0 && result.TypeName.find("ArrayProperty") != std::string::npos) {
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FArrayProperty_InnerOff, &result.InnerProp, sizeof(void*));
                    }
                    if (g_State.FMapProperty_KeyPropOff >= 0 && result.TypeName.find("MapProperty") != std::string::npos) {
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FMapProperty_KeyPropOff, &result.KeyProp, sizeof(void*));
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FMapProperty_ValuePropOff, &result.ValueProp, sizeof(void*));
                    }
                    if (g_State.FSetProperty_ElementPropOff >= 0 && result.TypeName.find("SetProperty") != std::string::npos) {
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FSetProperty_ElementPropOff, &result.InnerProp, sizeof(void*));
                    }
                    result.Found = true;
                    break;
                }
                child = EngineDiscovery::GetFFieldNext(g_State, child);
            }
            if (result.Found) break;
        } else if (g_State.UStruct_ChildrenOff >= 0) {
            void* child = EngineDiscovery::GetChildrenHead(g_State, cur);
            while (child) {
                std::string name = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(child));
                if (_stricmp(name.c_str(), propName.c_str()) == 0) {
                    result.Prop = child;
                    UClass* pCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(child));
                    result.TypeName = pCls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(pCls)) : "Property";
                    int32_t off = 0;
                    if (g_State.FProperty_OffsetOff >= 0 && SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &off, 4)) {
                        result.Offset = off;
                    }
                    if (result.TypeName.find("BoolProperty") != std::string::npos && g_State.FBoolProperty_BaseOff >= 0) {
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 1, &result.ByteOffset, 1);
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 2, &result.ByteMask, 1);
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 3, &result.FieldMask, 1);
                        if (result.ByteMask == 0) result.ByteMask = 1;
                        if (result.FieldMask == 0) result.FieldMask = result.ByteMask;
                    }
                    result.Found = true;
                    break;
                }
                child = EngineDiscovery::GetFieldNext(g_State, child);
            }
            if (result.Found) break;
        }
        cur = EngineDiscovery::GetSuperStruct(g_State, cur);
    }

    if (!result.Found) {
        cur = static_cast<UStruct*>(cls);
        while (cur) {
            if (g_State.UStruct_ChildPropsOff >= 0) {
                void* child = EngineDiscovery::GetChildPropsHead(g_State, cur);
                while (child) {
                    std::string name = EngineDiscovery::GetFFieldName(g_State, child);
                    size_t u = name.find('_');
                    if (u != std::string::npos && u == propName.size() && _strnicmp(name.c_str(), propName.c_str(), u) == 0) {
                        result.Prop = child;
                        result.TypeName = EngineDiscovery::GetFFieldTypeName(g_State, child);
                        if (g_State.FProperty_OffsetOff >= 0) {
                            SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &result.Offset, 4);
                        }
                        if (g_State.FProperty_ElementSizeOff >= 0) {
                            SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_ElementSizeOff, &result.ElementSize, 4);
                        }
                        if (result.TypeName.find("BoolProperty") != std::string::npos && g_State.FBoolProperty_BaseOff >= 0) {
                            SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 1, &result.ByteOffset, 1);
                            SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 2, &result.ByteMask, 1);
                            SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 3, &result.FieldMask, 1);
                            if (result.ByteMask == 0) result.ByteMask = 1;
                            if (result.FieldMask == 0) result.FieldMask = result.ByteMask;
                        }
                        if (g_State.FArrayProperty_InnerOff >= 0 && result.TypeName.find("ArrayProperty") != std::string::npos) {
                            SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FArrayProperty_InnerOff, &result.InnerProp, sizeof(void*));
                        }
                        if (g_State.FMapProperty_KeyPropOff >= 0 && result.TypeName.find("MapProperty") != std::string::npos) {
                            SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FMapProperty_KeyPropOff, &result.KeyProp, sizeof(void*));
                            SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FMapProperty_ValuePropOff, &result.ValueProp, sizeof(void*));
                        }
                        if (g_State.FSetProperty_ElementPropOff >= 0 && result.TypeName.find("SetProperty") != std::string::npos) {
                            SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FSetProperty_ElementPropOff, &result.InnerProp, sizeof(void*));
                        }
                        result.Found = true;
                        break;
                    }
                    child = EngineDiscovery::GetFFieldNext(g_State, child);
                }
                if (result.Found) break;
            } else if (g_State.UStruct_ChildrenOff >= 0) {
                void* child = EngineDiscovery::GetChildrenHead(g_State, cur);
                while (child) {
                    std::string name = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(child));
                    size_t u = name.find('_');
                    if (u != std::string::npos && u == propName.size() && _strnicmp(name.c_str(), propName.c_str(), u) == 0) {
                        result.Prop = child;
                        UClass* pCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(child));
                        result.TypeName = pCls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(pCls)) : "Property";
                        int32_t off = 0;
                        if (g_State.FProperty_OffsetOff >= 0 && SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &off, 4)) {
                            result.Offset = off;
                        }
                        result.Found = true;
                        break;
                    }
                    child = EngineDiscovery::GetFieldNext(g_State, child);
                }
                if (result.Found) break;
            }
            cur = EngineDiscovery::GetSuperStruct(g_State, cur);
        }
    }

    {
        std::lock_guard<std::mutex> lock(s_PropertyCacheMutex);
        s_PropertyCache[key] = result;
    }
    return result;
}

static FoundProperty MakeFoundProperty(void* prop, const std::string& typeName, int32_t offset, int32_t elemSize) {
    FoundProperty fp;
    fp.Prop = prop;
    fp.TypeName = typeName;
    fp.Offset = offset;
    fp.ElementSize = elemSize > 0 ? elemSize : 8;
    fp.Found = true;
    if (typeName.find("BoolProperty") != std::string::npos && prop && g_State.FBoolProperty_BaseOff >= 0) {
        SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.FBoolProperty_BaseOff + 1, &fp.ByteOffset, 1);
        SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.FBoolProperty_BaseOff + 2, &fp.ByteMask, 1);
        SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.FBoolProperty_BaseOff + 3, &fp.FieldMask, 1);
        if (fp.ByteMask == 0) fp.ByteMask = 1;
        if (fp.FieldMask == 0) fp.FieldMask = fp.ByteMask;
    }
    if (prop) {
        if (g_State.FArrayProperty_InnerOff >= 0 && typeName.find("ArrayProperty") != std::string::npos) {
            SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.FArrayProperty_InnerOff, &fp.InnerProp, sizeof(void*));
        }
        if (g_State.FMapProperty_KeyPropOff >= 0 && typeName.find("MapProperty") != std::string::npos) {
            SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.FMapProperty_KeyPropOff, &fp.KeyProp, sizeof(void*));
            SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.FMapProperty_ValuePropOff, &fp.ValueProp, sizeof(void*));
        }
        if (g_State.FSetProperty_ElementPropOff >= 0 && typeName.find("SetProperty") != std::string::npos) {
            SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.FSetProperty_ElementPropOff, &fp.InnerProp, sizeof(void*));
        }
    }
    return fp;
}

struct FunctionCacheKey {
    UClass* Class;
    std::string FuncName;
    bool operator==(const FunctionCacheKey& o) const {
        return Class == o.Class && _stricmp(FuncName.c_str(), o.FuncName.c_str()) == 0;
    }
};

struct FunctionCacheKeyHash {
    std::size_t operator()(const FunctionCacheKey& k) const {
        std::size_t h1 = std::hash<void*>()(k.Class);
        std::size_t h2 = 0;
        for (char c : k.FuncName) {
            h2 = (h2 * 131) + tolower(static_cast<unsigned char>(c));
        }
        return h1 ^ (h2 << 1);
    }
};

static std::unordered_map<FunctionCacheKey, void*, FunctionCacheKeyHash> s_FunctionCache;
static std::mutex s_FunctionCacheMutex;

static void* FindFunctionOnClass(UClass* cls, const std::string& funcName) {
    if (!cls || funcName.empty()) return nullptr;

    FunctionCacheKey key{ cls, funcName };
    {
        std::lock_guard<std::mutex> lock(s_FunctionCacheMutex);
        auto it = s_FunctionCache.find(key);
        if (it != s_FunctionCache.end()) {
            return it->second;
        }
    }

    void* func = EngineDiscovery::FindFunctionByName(g_State, static_cast<UStruct*>(cls), funcName.c_str());

    if (!func && funcName.rfind("K2_", 0) == 0) {
        std::string stripped = funcName.substr(3);
        func = EngineDiscovery::FindFunctionByName(g_State, static_cast<UStruct*>(cls), stripped.c_str());
    } else if (!func && funcName.rfind("K2_", 0) != 0) {
        std::string prefixed = "K2_" + funcName;
        func = EngineDiscovery::FindFunctionByName(g_State, static_cast<UStruct*>(cls), prefixed.c_str());
    }

    if (!func) {
        auto it = g_State.FunctionByNameOnly.find(funcName);
        if (it != g_State.FunctionByNameOnly.end()) {
            func = it->second;
        }
    }

    {
        std::lock_guard<std::mutex> lock(s_FunctionCacheMutex);
        s_FunctionCache[key] = func;
    }
    return func;
}

static bool ReadEnumPropertyValue(const FoundProperty& prop, uint8_t* propAddr, int64_t& out) {
    if (!propAddr) return false;
    int32_t width = prop.ElementSize > 0 && prop.ElementSize <= 8 ? prop.ElementSize : 1;
    bool isUnsigned = true;
    if (prop.Prop && g_State.FEnumProperty_UnderlyingOff >= 0) {
        void* underlying = nullptr;
        if (SafeMem_Read(reinterpret_cast<uint8_t*>(prop.Prop) + g_State.FEnumProperty_UnderlyingOff,
                         &underlying, sizeof(underlying)) && underlying &&
            SafeMem_IsReadable(underlying, 0x20)) {
            UClass* underlyingClass = EngineDiscovery::GetObjectClass(
                g_State, static_cast<UObject*>(underlying));
            const std::string underlyingName = underlyingClass
                ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(underlyingClass))
                : std::string();
            if (underlyingName.find("ByteProperty") != std::string::npos ||
                underlyingName.find("UInt16Property") != std::string::npos ||
                underlyingName.find("UInt32Property") != std::string::npos ||
                underlyingName.find("UInt64Property") != std::string::npos) {
                isUnsigned = true;
            } else if (underlyingName.find("Int8Property") != std::string::npos ||
                       underlyingName.find("Int16Property") != std::string::npos ||
                       underlyingName.find("IntProperty") != std::string::npos ||
                       underlyingName.find("Int64Property") != std::string::npos) {
                isUnsigned = false;
            }
            if (g_State.FProperty_ElementSizeOff >= 0) {
                int32_t reported = 0;
                SafeMem_Read(reinterpret_cast<uint8_t*>(underlying) + g_State.FProperty_ElementSizeOff,
                             &reported, 4);
                if (reported > 0 && reported <= 8) width = reported;
            }
        }
    }
    uint64_t raw = 0;
    if (!SafeMem_Read(propAddr, &raw, static_cast<size_t>(width))) return false;
    if (width < 8) {
        const uint64_t mask = (static_cast<uint64_t>(1) << (width * 8)) - 1;
        raw &= mask;
        if (!isUnsigned && (raw & (static_cast<uint64_t>(1) << (width * 8 - 1)))) raw |= ~mask;
    }
    out = static_cast<int64_t>(raw);
    return true;
}

static int PushPropertyValue(lua_State* L, uint8_t* baseAddr, const FoundProperty& prop,
                             LuaCallFrame* frame) {
    if (prop.Offset < 0 || !baseAddr) {
        lua_pushnil(L);
        return 1;
    }

    uint8_t* propAddr = baseAddr + prop.Offset;
    const std::string& type = prop.TypeName;

    if (type.find("BoolProperty") != std::string::npos) {
        uint8_t byteOffset = prop.ByteOffset;
        uint8_t fieldMask = prop.FieldMask ? prop.FieldMask : prop.ByteMask;
        if (fieldMask == 0) fieldMask = 1;
        if (prop.Prop && g_State.FBoolProperty_BaseOff >= 0 && prop.ByteMask == 1 && prop.FieldMask == 1 && prop.ByteOffset == 0) {
            SafeMem_Read(reinterpret_cast<uint8_t*>(prop.Prop) + g_State.FBoolProperty_BaseOff + 1, &byteOffset, 1);
            SafeMem_Read(reinterpret_cast<uint8_t*>(prop.Prop) + g_State.FBoolProperty_BaseOff + 3, &fieldMask, 1);
            if (fieldMask == 0) fieldMask = 1;
        }
        uint8_t byteVal = 0;
        SafeMem_Read(propAddr + byteOffset, &byteVal, 1);
        lua_pushboolean(L, (byteVal & fieldMask) ? 1 : 0);
        return 1;
    }
    if (type.find("Int8Property") != std::string::npos || type.find("ByteProperty") != std::string::npos) {
        int8_t val = 0; SafeMem_Read(propAddr, &val, 1); lua_pushinteger(L, val); return 1;
    }
    if (type.find("Int16Property") != std::string::npos || type.find("UInt16Property") != std::string::npos) {
        int16_t val = 0; SafeMem_Read(propAddr, &val, 2); lua_pushinteger(L, val); return 1;
    }
    if (type.find("IntProperty") != std::string::npos || type.find("UInt32Property") != std::string::npos) {
        int32_t val = 0; SafeMem_Read(propAddr, &val, 4); lua_pushinteger(L, val); return 1;
    }
    if (type.find("Int64Property") != std::string::npos || type.find("UInt64Property") != std::string::npos) {
        int64_t val = 0; SafeMem_Read(propAddr, &val, 8); lua_pushinteger(L, val); return 1;
    }
    if (type.find("EnumProperty") != std::string::npos) {
        int64_t val = 0;
        if (ReadEnumPropertyValue(prop, propAddr, val)) {
            lua_pushinteger(L, static_cast<lua_Integer>(val));
        } else {
            lua_pushnil(L);
        }
        return 1;
    }
    if (type.find("FloatProperty") != std::string::npos) {
        float val = 0.0f; SafeMem_Read(propAddr, &val, 4); lua_pushnumber(L, val); return 1;
    }
    if (type.find("DoubleProperty") != std::string::npos) {
        double val = 0.0; SafeMem_Read(propAddr, &val, 8); lua_pushnumber(L, val); return 1;
    }
    if (type.find("StrProperty") != std::string::npos) {
        FString fstr = {};
        if (SafeMem_Read(propAddr, &fstr, sizeof(fstr))) {
            lua_pushstring(L, fstr.ToStdString().c_str());
            return 1;
        }
    }
    if (type.find("NameProperty") != std::string::npos) {
        FName fname = {};
        if (SafeMem_Read(propAddr, &fname, sizeof(fname))) {
            std::string nameStr = EngineDiscovery::GetCachedFName(g_State, fname.ComparisonIndex);
            lua_pushstring(L, nameStr.c_str());
            return 1;
        }
    }
    if (type.find("TextProperty") != std::string::npos) {

        FString fstr = {};
        if (SafeMem_Read(propAddr, &fstr, sizeof(fstr))) {
            PushFTextRef(L, propAddr);
            return 1;
        }
    }
    if (type.find("SoftObjectProperty") != std::string::npos || type.find("SoftClassProperty") != std::string::npos) {
        std::string pathStr;
        if (g_State.UEMajor >= 5 && g_State.UEMinor >= 1) {
            FName pkgName = {}, assetName = {};
            if (SafeMem_Read(propAddr, &pkgName, sizeof(pkgName)) &&
                SafeMem_Read(propAddr + sizeof(FName), &assetName, sizeof(assetName))) {
                std::string pkgStr = EngineDiscovery::GetCachedFName(g_State, pkgName.ComparisonIndex);
                std::string aStr = EngineDiscovery::GetCachedFName(g_State, assetName.ComparisonIndex);
                if (!pkgStr.empty() && pkgStr != "None") {
                    pathStr = pkgStr;
                    if (!aStr.empty() && aStr != "None") {
                        pathStr += "." + aStr;
                    }
                }
            }
        } else {
            FName fname = {};
            if (SafeMem_Read(propAddr, &fname, sizeof(fname))) {
                std::string nameStr = EngineDiscovery::GetCachedFName(g_State, fname.ComparisonIndex);
                if (!nameStr.empty() && nameStr != "None") {
                    pathStr = nameStr;
                }
            }
        }
        PushSoftObject(L, propAddr, pathStr.c_str());
        return 1;
    }
    if (type.find("InterfaceProperty") != std::string::npos) {

        void* ptr = nullptr;
        if (SafeMem_Read(propAddr, &ptr, sizeof(void*)) && ptr &&
            EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(ptr))) {
            PushUObject(L, static_cast<UObject*>(ptr));
        } else {
            lua_pushnil(L);
        }
        return 1;
    }
    if (type.find("ObjectProperty") != std::string::npos || type.find("ClassProperty") != std::string::npos) {
        void* ptr = nullptr;
        SafeMem_Read(propAddr, &ptr, sizeof(void*));
        PushUObject(L, static_cast<UObject*>(ptr));
        return 1;
    }
    if (type.find("StructProperty") != std::string::npos) {

        if (!PushMathStructValue(L, prop, propAddr)) {
            PushStructView(L, propAddr, prop, frame);
        }
        return 1;
    }
    if (type.find("ArrayProperty") != std::string::npos) {
        std::string innerName = prop.InnerProp ? EngineDiscovery::GetFFieldTypeName(g_State, prop.InnerProp) : "";
        int32_t innerSize = 0;
        if (prop.InnerProp && g_State.FProperty_ElementSizeOff >= 0) {
            SafeMem_Read(reinterpret_cast<uint8_t*>(prop.InnerProp) + g_State.FProperty_ElementSizeOff, &innerSize, 4);
        }
        PushTArray(L, propAddr, prop.InnerProp, innerName.c_str(), innerSize, frame);
        return 1;
    }
    if (type.find("MapProperty") != std::string::npos) {
        std::string keyName = prop.KeyProp ? EngineDiscovery::GetFFieldTypeName(g_State, prop.KeyProp) : "";
        std::string valName = prop.ValueProp ? EngineDiscovery::GetFFieldTypeName(g_State, prop.ValueProp) : "";
        int32_t keySize = 8, valSize = 8;
        if (prop.KeyProp && g_State.FProperty_ElementSizeOff >= 0)
            SafeMem_Read(reinterpret_cast<uint8_t*>(prop.KeyProp) + g_State.FProperty_ElementSizeOff, &keySize, 4);
        if (prop.ValueProp && g_State.FProperty_ElementSizeOff >= 0)
            SafeMem_Read(reinterpret_cast<uint8_t*>(prop.ValueProp) + g_State.FProperty_ElementSizeOff, &valSize, 4);
        PushTMap(L, propAddr, prop.KeyProp, prop.ValueProp, keyName.c_str(), valName.c_str(), keySize, valSize, frame);
        return 1;
    }
    if (type.find("SetProperty") != std::string::npos) {
        std::string elemName = prop.InnerProp ? EngineDiscovery::GetFFieldTypeName(g_State, prop.InnerProp) : "";
        int32_t elemSize = 8;
        if (prop.InnerProp && g_State.FProperty_ElementSizeOff >= 0)
            SafeMem_Read(reinterpret_cast<uint8_t*>(prop.InnerProp) + g_State.FProperty_ElementSizeOff, &elemSize, 4);
        PushTSet(L, propAddr, prop.InnerProp, elemName.c_str(), elemSize, frame);
        return 1;
    }
    if (type.find("DelegateProperty") != std::string::npos) {
        bool isMulti = type.find("Multicast") != std::string::npos;
        PushDelegate(L, propAddr, baseAddr, "", isMulti);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

static void LogFTextMarshalFallbackOnce(bool converterRefused) {
    static bool reported = false;
    if (reported) return;
    reported = true;
    EngineDiscovery::WriteLogLine(converterRefused
        ? "[Lua] an FText argument could not be built by the engine's converter; an empty text was passed instead.\n"
        : "[Lua] an FText argument was neither a string nor an FText handle; an empty text was passed instead.\n");
}

static void LogEngineAllocationOnce(bool ok, void* buffer, size_t bytes) {
    static std::atomic<bool> reported{false};
    bool expected = false;
    if (!reported.compare_exchange_strong(expected, true))
        return;
    char message[256] = {};
    snprintf(message, sizeof(message),
             "[Lua] Engine allocation %s: %zu bytes at %p via GMalloc slot %d.\n",
             ok ? "succeeded" : "failed", bytes, buffer,
             g_State.GMallocMallocSlot);
    EngineDiscovery::WriteLogLine(message);
}

static bool WriteFTextFromString(uint8_t* dest, const char* utf8, int32_t textSize) {
    if (!dest || !utf8) return false;

    const int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (wlen <= 0) return false;
    std::vector<wchar_t> wide(static_cast<size_t>(wlen), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide.data(), wlen) <= 0) return false;

    FString inString = { wide.data(), wlen, wlen };
    const EngineDiscovery::ReflectedArgument args[] = {
        { "InString", &inString, static_cast<int32_t>(sizeof(FString)) },
    };

    const int32_t size = textSize > 0 && textSize <= 32 ? textSize : 16;
    uint8_t text[32] = { 0 };
    if (!EngineDiscovery::CallStaticFunctionByName(g_State, "KismetTextLibrary",
                                                   "Conv_StringToText", args, 1,
                                                   text, size)) {
        return false;
    }
    SafeMem_Write(dest, text, size);
    return true;
}

static bool SetPropertyValue(lua_State* L, uint8_t* baseAddr, const FoundProperty& prop, int valIdx) {
    if (prop.Offset < 0 || !baseAddr) return false;

    uint8_t* propAddr = baseAddr + prop.Offset;
    const std::string& type = prop.TypeName;
    bool success = true;

    if (type.find("BoolProperty") != std::string::npos) {
        bool val = lua_toboolean(L, valIdx) != 0;
        uint8_t byteOffset = prop.ByteOffset;
        uint8_t byteMask = prop.ByteMask ? prop.ByteMask : 1;
        uint8_t fieldMask = prop.FieldMask ? prop.FieldMask : byteMask;
        if (prop.Prop && g_State.FBoolProperty_BaseOff >= 0 && prop.ByteMask == 1 && prop.FieldMask == 1 && prop.ByteOffset == 0) {
            SafeMem_Read(reinterpret_cast<uint8_t*>(prop.Prop) + g_State.FBoolProperty_BaseOff + 1, &byteOffset, 1);
            SafeMem_Read(reinterpret_cast<uint8_t*>(prop.Prop) + g_State.FBoolProperty_BaseOff + 2, &byteMask, 1);
            SafeMem_Read(reinterpret_cast<uint8_t*>(prop.Prop) + g_State.FBoolProperty_BaseOff + 3, &fieldMask, 1);
            if (byteMask == 0) byteMask = 1;
            if (fieldMask == 0) fieldMask = byteMask;
        }
        uint8_t* byteAddr = propAddr + byteOffset;
        uint8_t byteVal = 0;
        SafeMem_Read(byteAddr, &byteVal, 1);
        byteVal = (byteVal & ~fieldMask) | (val ? byteMask : 0);
        SafeMem_Write(byteAddr, &byteVal, 1);
    } else if (type.find("Int8Property") != std::string::npos || type.find("ByteProperty") != std::string::npos) {
        int8_t val = static_cast<int8_t>(lua_tointeger(L, valIdx));
        SafeMem_Write(propAddr, &val, 1);
    } else if (type.find("Int16Property") != std::string::npos || type.find("UInt16Property") != std::string::npos) {
        int16_t val = static_cast<int16_t>(lua_tointeger(L, valIdx));
        SafeMem_Write(propAddr, &val, 2);
    } else if (type.find("IntProperty") != std::string::npos || type.find("UInt32Property") != std::string::npos) {
        int32_t val = static_cast<int32_t>(lua_tointeger(L, valIdx));
        SafeMem_Write(propAddr, &val, 4);
    } else if (type.find("Int64Property") != std::string::npos || type.find("UInt64Property") != std::string::npos) {
        int64_t val = static_cast<int64_t>(lua_tointeger(L, valIdx));
        SafeMem_Write(propAddr, &val, 8);
    } else if (type.find("FloatProperty") != std::string::npos) {
        float val = static_cast<float>(lua_tonumber(L, valIdx));
        SafeMem_Write(propAddr, &val, 4);
    } else if (type.find("DoubleProperty") != std::string::npos) {
        double val = static_cast<double>(lua_tonumber(L, valIdx));
        SafeMem_Write(propAddr, &val, 8);
    } else if (type.find("StrProperty") != std::string::npos || type.find("Utf8StrProperty") != std::string::npos || type.find("AnsiStrProperty") != std::string::npos) {

        const char* str = lua_isstring(L, valIdx) ? lua_tostring(L, valIdx) : "";

        void* mallocInstance = g_State.GMallocLocation
            ? SafeMem_ReadPtr(g_State.GMallocLocation)
            : g_State.GMalloc;
        if (!mallocInstance) mallocInstance = g_State.GMalloc;
        if (str && mallocInstance) {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
            if (wlen > 0) {
                size_t allocBytes = static_cast<size_t>(wlen) * sizeof(wchar_t);

                void* buf = SafeMem_CallMallocVftable(mallocInstance, allocBytes, 0,
                                                      g_State.GMallocMallocSlot);
                LogEngineAllocationOnce(buf != nullptr, buf, allocBytes);
                if (buf) {
                    MultiByteToWideChar(CP_UTF8, 0, str, -1, static_cast<wchar_t*>(buf), wlen);

                    FString oldStr = {};
                    SafeMem_Read(propAddr, &oldStr, sizeof(oldStr));

                    FString newStr = { static_cast<wchar_t*>(buf), wlen, wlen };
                    SafeMem_Write(propAddr, &newStr, sizeof(newStr));
                    (void)oldStr;

                } else {
                    success = false;
                }
            } else {
                success = false;
            }
        } else {
            success = false;
        }
    } else if (type.find("TextProperty") != std::string::npos) {

        const char* str = lua_isstring(L, valIdx) ? lua_tostring(L, valIdx) : nullptr;
        std::string text;
        const bool recovered = str != nullptr || GetLuaFTextString(L, valIdx, text);
        if (!recovered || !WriteFTextFromString(propAddr, str ? str : text.c_str(),
                                                prop.ElementSize)) {
            LogFTextMarshalFallbackOnce(recovered);
            success = WriteFTextFromString(propAddr, "", prop.ElementSize);
        } else {
            success = true;
        }
    } else if (type.find("EnumProperty") != std::string::npos) {

        if (lua_isnumber(L, valIdx)) {
            const int64_t val = static_cast<int64_t>(lua_tointeger(L, valIdx));
            const int32_t width = prop.ElementSize > 0 && prop.ElementSize <= 8
                                      ? prop.ElementSize
                                      : 1;
            switch (width) {
            case 1: { int8_t  v = static_cast<int8_t>(val);  SafeMem_Write(propAddr, &v, 1); break; }
            case 2: { int16_t v = static_cast<int16_t>(val); SafeMem_Write(propAddr, &v, 2); break; }
            case 4: { int32_t v = static_cast<int32_t>(val); SafeMem_Write(propAddr, &v, 4); break; }
            default: { int64_t v = val;                      SafeMem_Write(propAddr, &v, 8); break; }
            }
        } else {
            success = false;
        }
    } else if (type.find("NameProperty") != std::string::npos) {

        const int32_t nameWidth = prop.ElementSize > 0 ? prop.ElementSize : 0;
        uint32_t handleIndex = 0, handleNumber = 0;
        if (LuaEngine::GetFNameHandle(L, valIdx, &handleIndex, &handleNumber)) {
            success = EngineDiscovery::WriteFNameBytes(g_State, handleIndex, handleNumber, propAddr,
                                                       nameWidth);
        } else if (lua_isstring(L, valIdx)) {
            const char* str = lua_tostring(L, valIdx);
            const int32_t nameIdx = str ? EngineDiscovery::ResolveFNameIndex(g_State, str) : -1;
            success = EngineDiscovery::WriteFNameBytes(g_State,
                                                       static_cast<uint32_t>(nameIdx >= 0 ? nameIdx : 0),
                                                       0, propAddr, nameWidth);
        } else {
            success = false;
        }
    } else if (type.find("InterfaceProperty") != std::string::npos) {

        UObject* obj = GetUObject(L, valIdx);
        SafeMem_Write(propAddr, &obj, sizeof(void*));
    } else if (type.find("SoftObjectProperty") != std::string::npos || type.find("SoftClassProperty") != std::string::npos) {
        std::string pathStr;
        if (lua_isstring(L, valIdx)) {
            pathStr = lua_tostring(L, valIdx);
        } else if (auto* soft = static_cast<LuaSoftObjectData*>(luaL_testudata(L, valIdx, "Avalonium.FSoftObjectPath"))) {
            pathStr = soft->AssetPath;
        } else if (UObject* targetObj = GetUObject(L, valIdx)) {
            pathStr = EngineDiscovery::BuildFullName(g_State, targetObj);
        }

        if (!pathStr.empty()) {
            if (g_State.UEMajor >= 5 && g_State.UEMinor >= 1) {
                size_t dotPos = pathStr.rfind('.');
                std::string pkg = (dotPos != std::string::npos) ? pathStr.substr(0, dotPos) : pathStr;
                std::string asset = (dotPos != std::string::npos) ? pathStr.substr(dotPos + 1) : "";
                int32_t pkgIdx = EngineDiscovery::ResolveFNameIndex(g_State, pkg.c_str());
                int32_t assetIdx = EngineDiscovery::ResolveFNameIndex(g_State, asset.c_str());
                FName pkgName = { static_cast<uint32_t>(pkgIdx >= 0 ? pkgIdx : 0), 0 };
                FName assetName = { static_cast<uint32_t>(assetIdx >= 0 ? assetIdx : 0), 0 };
                SafeMem_Write(propAddr, &pkgName, sizeof(pkgName));
                SafeMem_Write(propAddr + sizeof(FName), &assetName, sizeof(assetName));
            } else {
                int32_t nameIdx = EngineDiscovery::ResolveFNameIndex(g_State, pathStr.c_str());
                FName fname = { static_cast<uint32_t>(nameIdx >= 0 ? nameIdx : 0), 0 };
                SafeMem_Write(propAddr, &fname, sizeof(fname));
            }
        }
    } else if (type.find("ArrayProperty") != std::string::npos) {

        success = WriteArrayFromTable(L, propAddr, prop, valIdx);
    } else if (type.find("ObjectProperty") != std::string::npos || type.find("ClassProperty") != std::string::npos) {
        UObject* obj = GetUObject(L, valIdx);
        SafeMem_Write(propAddr, &obj, sizeof(void*));
    } else if (type.find("StructProperty") != std::string::npos) {

        Vector3D vec;
        Rotator3D rot;
        LinearColor4D col;
        if (GetVector(L, valIdx, vec)) {
            if (prop.ElementSize == 24 || (g_State.UEMajor >= 5 && prop.ElementSize != 12)) {
                struct DoubleVector3D { double X, Y, Z; } dv = {
                    static_cast<double>(vec.X),
                    static_cast<double>(vec.Y),
                    static_cast<double>(vec.Z)
                };
                SafeMem_Write(propAddr, &dv, sizeof(dv));
            } else {
                SafeMem_Write(propAddr, &vec, sizeof(vec));
            }
        } else if (GetRotator(L, valIdx, rot)) {
            if (prop.ElementSize == 24 || (g_State.UEMajor >= 5 && prop.ElementSize != 12)) {
                struct DoubleVector3D { double Pitch, Yaw, Roll; } dr = {
                    static_cast<double>(rot.Pitch),
                    static_cast<double>(rot.Yaw),
                    static_cast<double>(rot.Roll)
                };
                SafeMem_Write(propAddr, &dr, sizeof(dr));
            } else {
                SafeMem_Write(propAddr, &rot, sizeof(rot));
            }
        } else if (GetColor(L, valIdx, col)) {
            SafeMem_Write(propAddr, &col, sizeof(col));
        } else if (lua_istable(L, valIdx) && prop.Prop &&
                   g_State.FStructProperty_StructOff >= 0) {
            void* scriptStruct = nullptr;
            SafeMem_Read(reinterpret_cast<uint8_t*>(prop.Prop) + g_State.FStructProperty_StructOff,
                         &scriptStruct, sizeof(void*));
            success = WriteStructFromTable(L, propAddr, scriptStruct, valIdx);
        }
    } else {
        success = false;
    }

    return success;
}

static bool WriteStructFromTable(lua_State* L, uint8_t* structAddr, void* scriptStruct,
                                 int tableIdx) {
    if (!structAddr || !scriptStruct || !lua_istable(L, tableIdx)) return false;

    std::vector<EngineDiscovery::ReflectedMember> members;
    if (!EngineDiscovery::GetReflectedMembers(
            g_State, static_cast<UStruct*>(scriptStruct), members) ||
        members.empty()) {
        return false;
    }

    bool wroteAny = false;
    for (const EngineDiscovery::ReflectedMember& member : members) {
        if (member.Name.empty()) continue;

        lua_getfield(L, tableIdx, member.Name.c_str());
        const int valueIdx = lua_gettop(L);
        if (!lua_isnil(L, valueIdx)) {
            FoundProperty field;
            field.Prop = member.Property;
            field.TypeName = member.TypeName;
            field.Offset = member.Offset;
            field.ElementSize = member.ElementSize;
            field.Flags = member.Flags;
            field.Found = true;
            if (SetPropertyValue(L, structAddr, field, valueIdx)) wroteAny = true;
        }
        lua_pop(L, 1);
    }
    return wroteAny;
}

static const char* PROPERTY_VIEW_MT = "Avalonium.PropertyProxy";
static const char* STRUCT_VIEW_MT = "Avalonium.StructView";

struct LuaPropertyViewData {
    uint8_t* Base = nullptr;
    FoundProperty Prop;
    char Name[64] = { 0 };

    static LuaPropertyViewData* Create(lua_State* L) {
        void* bytes = lua_newuserdata(L, sizeof(LuaPropertyViewData));
        return new (bytes) LuaPropertyViewData();
    }

    void Destroy() { this->~LuaPropertyViewData(); }
};

static int PropertyView_ToString_Impl(lua_State* L) {
    auto* d = static_cast<LuaPropertyViewData*>(luaL_testudata(L, 1, PROPERTY_VIEW_MT));
    if (!d || !d->Base) { lua_pushstring(L, "RemoteUnrealParam(null)"); return 1; }
    if (PushPropertyValueEx(L, d->Base, d->Prop) > 0) {
        luaL_tolstring(L, -1, nullptr);
        lua_remove(L, -2);
        return 1;
    }
    lua_pushstring(L, "RemoteUnrealParam(null)");
    return 1;
}

static int PropertyView_Get_Impl(lua_State* L) {
    auto* d = static_cast<LuaPropertyViewData*>(luaL_checkudata(L, 1, PROPERTY_VIEW_MT));
    if (!d || !d->Base) { lua_pushnil(L); return 1; }
    return PushPropertyValueEx(L, d->Base, d->Prop);
}

static int PropertyView_Set_Impl(lua_State* L) {
    auto* d = static_cast<LuaPropertyViewData*>(luaL_checkudata(L, 1, PROPERTY_VIEW_MT));
    if (!d || !d->Base) { lua_pushboolean(L, 0); return 1; }
    bool ok = SetPropertyValueEx(L, d->Base, d->Prop, 2);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int PropertyView_Type_Impl(lua_State* L) {
    auto* d = static_cast<LuaPropertyViewData*>(luaL_checkudata(L, 1, PROPERTY_VIEW_MT));

    lua_pushstring(L, "RemoteUnrealParam");
    if (d) lua_pushstring(L, d->Prop.TypeName.c_str()); else lua_pushstring(L, "");
    return 2;
}

static int PropertyView_Index_Impl(lua_State* L) {
    const char* k = luaL_checkstring(L, 2);
    if (strcmp(k, "get") == 0 || strcmp(k, "Get") == 0) { lua_pushcfunction(L, PropertyView_Get_Impl);  return 1; }
    if (strcmp(k, "set") == 0 || strcmp(k, "Set") == 0) { lua_pushcfunction(L, PropertyView_Set_Impl);  return 1; }
    if (strcmp(k, "type") == 0)                          { lua_pushcfunction(L, PropertyView_Type_Impl); return 1; }
    if (strcmp(k, "ToString") == 0)                      { lua_pushcfunction(L, PropertyView_ToString_Impl); return 1; }
    if (strcmp(k, "IsValid") == 0) {

        lua_pushcfunction(L, [](lua_State* L2) -> int {
            auto* v = static_cast<LuaPropertyViewData*>(luaL_testudata(L2, 1, PROPERTY_VIEW_MT));
            lua_pushboolean(L2, (v && v->Base) ? 1 : 0);
            return 1;
        });
        return 1;
    }
    if (strcmp(k, "GetAddress") == 0) {
        auto* d = static_cast<LuaPropertyViewData*>(luaL_testudata(L, 1, PROPERTY_VIEW_MT));
        if (d && d->Base) {
            lua_pushlightuserdata(L, d->Base + d->Prop.Offset);
            return 1;
        }
        lua_pushnil(L);
        return 1;
    }
    return PropertyView_Get_Impl(L);
}

static int PropertyView_NewIndex_Impl(lua_State* L) {
    return PropertyView_Set_Impl(L);
}

static int PropertyView_Gc_Impl(lua_State* L) {
    auto* d = static_cast<LuaPropertyViewData*>(luaL_testudata(L, 1, PROPERTY_VIEW_MT));
    if (d) d->Destroy();
    return 0;
}

void PushPropertyView(lua_State* L, uint8_t* baseAddr, const FoundPropertyEx& prop, const char* name) {
    auto* d = LuaPropertyViewData::Create(L);
    d->Base = baseAddr;
    d->Prop = prop;
    if (name) strncpy_s(d->Name, name, _TRUNCATE);

    if (luaL_newmetatable(L, PROPERTY_VIEW_MT)) {
        lua_pushcfunction(L, PropertyView_Index_Impl);    lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, PropertyView_NewIndex_Impl); lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, PropertyView_ToString_Impl); lua_setfield(L, -2, "__tostring");
    }

    lua_pushcfunction(L, PropertyView_Gc_Impl);          lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -2);
}

struct LuaStructViewData {
    uint8_t* Addr = nullptr;
    void* ScriptStruct = nullptr;
    LuaCallFrame* Frame = nullptr;
};

static int StructView_Gc_Impl(lua_State* L) {
    auto* d = static_cast<LuaStructViewData*>(luaL_testudata(L, 1, STRUCT_VIEW_MT));
    if (d && d->Frame) { LuaCallFrame* frame = d->Frame; d->Frame = nullptr; frame->Release(); }
    return 0;
}

static int StructView_Index_Impl(lua_State* L) {
    auto* d = static_cast<LuaStructViewData*>(luaL_checkudata(L, 1, STRUCT_VIEW_MT));
    const char* key = luaL_checkstring(L, 2);

    if (strcmp(key, "type") == 0)       { lua_pushstring(L, "Struct"); return 1; }
    if (strcmp(key, "GetAddress") == 0) { lua_pushlightuserdata(L, d ? d->Addr : nullptr); return 1; }
    if (strcmp(key, "GetStruct") == 0)  { lua_pushlightuserdata(L, d ? d->ScriptStruct : nullptr); return 1; }
    if (strcmp(key, "IsValid") == 0) {
        lua_pushboolean(L, (d && d->Addr && d->ScriptStruct) ? 1 : 0);
        return 1;
    }

    if (d && d->Addr && d->ScriptStruct) {
        FoundProperty field = FindPropertyOnClassEx(reinterpret_cast<UClass*>(d->ScriptStruct), key);
        if (field.Found && field.Offset >= 0) {

            return PushPropertyValue(L, d->Addr, field, d->Frame);
        }
    }
    lua_pushnil(L);
    return 1;
}

static int StructView_NewIndex_Impl(lua_State* L) {
    auto* d = static_cast<LuaStructViewData*>(luaL_checkudata(L, 1, STRUCT_VIEW_MT));
    const char* key = luaL_checkstring(L, 2);
    if (!d || !d->Addr || !d->ScriptStruct) return 0;

    if (key && key[0]) {
        FoundProperty field = FindPropertyOnClassEx(reinterpret_cast<UClass*>(d->ScriptStruct), key);
        if (field.Found && field.Offset >= 0 && SetPropertyValueEx(L, d->Addr, field, 3)) return 0;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "[Lua] structure write to '%s' failed on %s", key ? key : "",
             (d && d->ScriptStruct)
                 ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(d->ScriptStruct)).c_str()
                 : "an unknown structure");
    Log("%s", buf);
    return 0;
}

static int StructView_ToString_Impl(lua_State* L) {
    auto* d = static_cast<LuaStructViewData*>(luaL_checkudata(L, 1, STRUCT_VIEW_MT));
    std::string name = (d && d->ScriptStruct)
        ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(d->ScriptStruct))
        : std::string();
    char buf[160];
    snprintf(buf, sizeof(buf), "%s(0x%p)", name.empty() ? "Struct" : name.c_str(),
             static_cast<const void*>(d ? d->Addr : nullptr));
    lua_pushstring(L, buf);
    return 1;
}

static void PushStructView(lua_State* L, uint8_t* structAddr, const FoundProperty& prop,
                           LuaCallFrame* frame) {
    void* scriptStruct = nullptr;
    if (prop.Prop && g_State.FStructProperty_StructOff >= 0) {
        SafeMem_Read(reinterpret_cast<uint8_t*>(prop.Prop) + g_State.FStructProperty_StructOff,
                     &scriptStruct, sizeof(void*));
    }

    auto* d = static_cast<LuaStructViewData*>(lua_newuserdata(L, sizeof(LuaStructViewData)));
    d->Addr = structAddr;
    d->ScriptStruct = scriptStruct;
    d->Frame = frame;

    if (luaL_newmetatable(L, STRUCT_VIEW_MT)) {
        lua_pushcfunction(L, StructView_Index_Impl);    lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, StructView_NewIndex_Impl); lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, StructView_ToString_Impl); lua_setfield(L, -2, "__tostring");
    }

    lua_pushcfunction(L, StructView_Gc_Impl);            lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -2);
    if (frame) frame->Retain();
}

static void ReadMathComponents(uint8_t* addr, int32_t count, bool wide, float* out) {
    for (int32_t i = 0; i < count; ++i) out[i] = 0.0f;
    if (!addr || count <= 0 || count > 4) return;
    if (wide) {
        double values[4] = {};
        if (!SafeMem_Read(addr, values, sizeof(double) * static_cast<size_t>(count))) return;
        for (int32_t i = 0; i < count; ++i) out[i] = static_cast<float>(values[i]);
    } else {
        float values[4] = {};
        if (!SafeMem_Read(addr, values, sizeof(float) * static_cast<size_t>(count))) return;
        for (int32_t i = 0; i < count; ++i) out[i] = values[i];
    }
}

static void* StructPropertyStruct(const FoundProperty& prop) {
    if (!prop.Prop || g_State.FStructProperty_StructOff < 0) return nullptr;
    void* scriptStruct = nullptr;
    if (!SafeMem_Read(reinterpret_cast<uint8_t*>(prop.Prop) + g_State.FStructProperty_StructOff,
                      &scriptStruct, sizeof(void*)))
        return nullptr;
    return scriptStruct;
}

static std::string StructReflectedName(const FoundProperty& prop) {
    void* scriptStruct = StructPropertyStruct(prop);
    if (!scriptStruct) return {};
    return EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(scriptStruct));
}

static bool StructDeclaresMembers(void* scriptStruct) {
    UStruct* cur = static_cast<UStruct*>(scriptStruct);
    for (int depth = 0; depth < 32 && cur; ++depth) {
        if (EngineDiscovery::GetChildPropsHead(g_State, cur)) return true;
        cur = EngineDiscovery::GetSuperStruct(g_State, cur);
    }
    return false;
}

static bool PushMathStructValue(lua_State* L, const FoundProperty& prop, uint8_t* propAddr) {
    void* scriptStruct = StructPropertyStruct(prop);
    const std::string name = StructReflectedName(prop);
    const bool wide = prop.ElementSize >= 20;

    if (name == "Vector" || name == "Vector3f" || name == "Vector3d") {
        float v[3]; ReadMathComponents(propAddr, 3, wide, v);
        PushVector(L, v[0], v[1], v[2]);
        return true;
    }
    if (name == "Rotator" || name == "Rotator3f" || name == "Rotator3d") {
        float r[3]; ReadMathComponents(propAddr, 3, wide, r);
        PushRotator(L, r[0], r[1], r[2]);
        return true;
    }
    if (name == "LinearColor" || name == "Vector4" || name == "Vector4f" || name == "Vector4d") {
        float c[4]; ReadMathComponents(propAddr, 4, wide, c);
        PushColor(L, c[0], c[1], c[2], c[3]);
        return true;
    }
    if (name == "Vector2D" || name == "Vector2f" || name == "Vector2d") {
        float v[2]; ReadMathComponents(propAddr, 2, wide, v);

        PushVector(L, v[0], v[1], 0.0f);
        return true;
    }

    if (StructDeclaresMembers(scriptStruct)) return false;

    switch (prop.ElementSize) {
    case 8:  { float v[2]; ReadMathComponents(propAddr, 2, false, v); PushVector(L, v[0], v[1], 0.0f); return true; }
    case 12: { float v[3]; ReadMathComponents(propAddr, 3, false, v); PushVector(L, v[0], v[1], v[2]); return true; }
    case 16: { float c[4]; ReadMathComponents(propAddr, 4, false, c); PushColor(L, c[0], c[1], c[2], c[3]); return true; }
    case 24: { float v[3]; ReadMathComponents(propAddr, 3, true, v);  PushVector(L, v[0], v[1], v[2]); return true; }
    default: break;
    }
    return false;
}

static void CopyOutStructField(lua_State* L, int tableIdx, int valueIdx, const char* field) {
    lua_getfield(L, valueIdx, field);
    if (!lua_isnil(L, -1)) {
        lua_pushvalue(L, tableIdx);
        lua_pushvalue(L, -2);
        lua_setfield(L, -2, field);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

static void FillOutStructTable(lua_State* L, int tableIdx, int valueIdx, const FoundProperty& prop) {
    const std::string name = StructReflectedName(prop);
    if (name == "Vector2D" || name == "Vector2f" || name == "Vector2d") {
        CopyOutStructField(L, tableIdx, valueIdx, "X");
        CopyOutStructField(L, tableIdx, valueIdx, "Y");
        return;
    }
    if (name == "Vector" || name == "Vector3f" || name == "Vector3d") {
        CopyOutStructField(L, tableIdx, valueIdx, "X");
        CopyOutStructField(L, tableIdx, valueIdx, "Y");
        CopyOutStructField(L, tableIdx, valueIdx, "Z");
        return;
    }
    if (name == "Rotator" || name == "Rotator3f" || name == "Rotator3d") {
        CopyOutStructField(L, tableIdx, valueIdx, "Pitch");
        CopyOutStructField(L, tableIdx, valueIdx, "Yaw");
        CopyOutStructField(L, tableIdx, valueIdx, "Roll");
        return;
    }
    if (name == "LinearColor" || name == "Vector4" || name == "Vector4f" || name == "Vector4d") {
        CopyOutStructField(L, tableIdx, valueIdx, "R");
        CopyOutStructField(L, tableIdx, valueIdx, "G");
        CopyOutStructField(L, tableIdx, valueIdx, "B");
        CopyOutStructField(L, tableIdx, valueIdx, "A");
        return;
    }
    if (name == "Transform") {
        CopyOutStructField(L, tableIdx, valueIdx, "Translation");
        CopyOutStructField(L, tableIdx, valueIdx, "Rotation");
        CopyOutStructField(L, tableIdx, valueIdx, "Scale3D");
        return;
    }
    if (name == "Quat") {
        CopyOutStructField(L, tableIdx, valueIdx, "X");
        CopyOutStructField(L, tableIdx, valueIdx, "Y");
        CopyOutStructField(L, tableIdx, valueIdx, "Z");
        CopyOutStructField(L, tableIdx, valueIdx, "W");
        return;
    }
    void* scriptStruct = StructPropertyStruct(prop);
    if (!scriptStruct) return;
    UStruct* cur = static_cast<UStruct*>(scriptStruct);
    for (int depth = 0; depth < 32 && cur; ++depth) {
        if (g_State.UStruct_ChildPropsOff >= 0) {
            for (void* child = EngineDiscovery::GetChildPropsHead(g_State, cur); child;
                 child = EngineDiscovery::GetFFieldNext(g_State, child)) {
                const std::string field = EngineDiscovery::GetFFieldName(g_State, child);
                if (!field.empty()) CopyOutStructField(L, tableIdx, valueIdx, field.c_str());
            }
        } else if (g_State.UStruct_ChildrenOff >= 0) {
            for (void* child = EngineDiscovery::GetChildrenHead(g_State, cur); child;
                 child = EngineDiscovery::GetFieldNext(g_State, child)) {
                const std::string field = EngineDiscovery::GetObjectNameStr(
                    g_State, static_cast<UObject*>(child));
                if (!field.empty()) CopyOutStructField(L, tableIdx, valueIdx, field.c_str());
            }
        }
        cur = EngineDiscovery::GetSuperStruct(g_State, cur);
    }
}

static bool WriteArrayFromTable(lua_State* L, uint8_t* propAddr, const FoundProperty& prop,
                                int tableIdx) {
    if (!propAddr || !prop.InnerProp || !lua_istable(L, tableIdx)) return false;

    const size_t count = static_cast<size_t>(lua_rawlen(L, tableIdx));
    FoundProperty inner = DescribePropertyEx(prop.InnerProp);
    inner.Offset = 0;
    const int32_t elementSize = inner.ElementSize > 0 ? inner.ElementSize : 8;

    TArray<uint8_t> current = {};
    if (!SafeMem_Read(propAddr, &current, sizeof(current))) return false;

    uint8_t* elements = current.Data;
    int32_t capacity = current.ArrayMax;
    if (count > 0 && (elements == nullptr || capacity < static_cast<int32_t>(count))) {

        void* mallocInstance = g_State.GMallocLocation
            ? SafeMem_ReadPtr(g_State.GMallocLocation)
            : g_State.GMalloc;
        if (!mallocInstance) mallocInstance = g_State.GMalloc;
        const size_t bytes = count * static_cast<size_t>(elementSize > 0 ? elementSize : 8);
        if (!mallocInstance || bytes == 0) return false;

        void* buffer = SafeMem_CallMallocVftable(mallocInstance, bytes, 0, g_State.GMallocMallocSlot);
        LogEngineAllocationOnce(buffer != nullptr, buffer, bytes);
        if (!buffer) return false;

        memset(buffer, 0, bytes);
        elements = static_cast<uint8_t*>(buffer);
        capacity = static_cast<int32_t>(count);
    }

    for (size_t i = 0; i < count; ++i) {
        lua_rawgeti(L, tableIdx, static_cast<lua_Integer>(i) + 1);
        const bool wrote = SetPropertyValue(
            L, elements + (i * static_cast<size_t>(elementSize > 0 ? elementSize : 8)), inner,
            lua_gettop(L));
        lua_pop(L, 1);
        if (!wrote) return false;
    }

    TArray<uint8_t> updated = {};
    updated.Data = elements;
    updated.ArrayNum = static_cast<int32_t>(count);
    updated.ArrayMax = capacity;
    return SafeMem_Write(propAddr, &updated, sizeof(updated));
}

static void ReportCallShapeOnce(void* ufunc, int32_t frameSize, const std::string& shape) {
    static std::mutex reportMutex;
    static std::unordered_set<void*> reported;

    std::lock_guard<std::mutex> lock(reportMutex);
    if (!reported.insert(ufunc).second) return;
    Log("[Blueprint] call frame=%d:%s", frameSize, shape.c_str());
}

struct UFunctionParamInfo
{
    void* Prop = nullptr;
    std::string Name;
    std::string TypeName;
    int32_t Offset = 0;
    int32_t ElementSize = 8;
    uint64_t Flags = 0;
    uint8_t ByteOffset = 0;
    uint8_t ByteMask = 1;
    uint8_t FieldMask = 1;
    bool IsReturn = false;
    bool IsOut = false;
};

struct CachedUFunctionCall
{
    int32_t ParamsSize = 0;
    int32_t ReturnOffset = -1;
    void* PropertyHead = nullptr;
    std::vector<UFunctionParamInfo> InParams;
    std::vector<UFunctionParamInfo> OutParams;
    std::string Shape;
    bool ShapeReported = false;
};

static thread_local std::unordered_map<void*, std::unique_ptr<CachedUFunctionCall>>
    s_UFunctionCallCache;

int CallUFunctionDynamic(lua_State* L, UObject* object, void* ufunc, int luaArgStart) {
    if (!object || !ufunc) {
        return luaL_error(L, "Invalid object or function for invocation");
    }

    void* propertyHead = nullptr;
    if (g_State.UStruct_ChildPropsOff >= 0)
        propertyHead = EngineDiscovery::GetChildPropsHead(g_State, static_cast<UStruct*>(ufunc));
    else if (g_State.UStruct_ChildrenOff >= 0)
        propertyHead = EngineDiscovery::GetChildrenHead(g_State, static_cast<UStruct*>(ufunc));

    CachedUFunctionCall* cached = nullptr;
    auto found = s_UFunctionCallCache.find(ufunc);
    if (found != s_UFunctionCallCache.end() && found->second->PropertyHead == propertyHead) {
        cached = found->second.get();
    }

    if (!cached) {
        auto entry = std::make_unique<CachedUFunctionCall>();
        entry->ParamsSize = EngineDiscovery::GetFunctionFrameSize(g_State, ufunc);
        entry->ReturnOffset = EngineDiscovery::GetFunctionReturnValueOffset(g_State, ufunc);
        entry->PropertyHead = propertyHead;
        std::string& shape = entry->Shape;

        auto ProcessChildProp = [&](void* child, const std::string& name,
                                    const std::string& typeName, int32_t off,
                                    int32_t elemSize, uint64_t flags) {
            bool isParam = (flags & 0x80) != 0;
            bool isReturn = (flags & 0x400) != 0 || name == "ReturnValue" ||
                            (entry->ReturnOffset >= 0 && off == entry->ReturnOffset);
            bool isOut = (flags & 0x100) != 0 && !isReturn && (flags & 0x2) == 0;

            if (isParam) {
                char text[224];
                snprintf(text, sizeof(text), " %s:%s@%d+%d flags=0x%llX%s", name.c_str(),
                         typeName.c_str(), off, elemSize,
                         static_cast<unsigned long long>(flags),
                         isReturn ? "(return)" : (isOut ? "(out)" : ""));
                shape += text;
            }

            uint8_t bOff = 0, bMask = 1, fMask = 1;
            if (typeName.find("BoolProperty") != std::string::npos &&
                g_State.FBoolProperty_BaseOff >= 0) {
                SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 1,
                             &bOff, 1);
                SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 2,
                             &bMask, 1);
                SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 3,
                             &fMask, 1);
                if (bMask == 0) bMask = 1;
                if (fMask == 0) fMask = bMask;
            }

            UFunctionParamInfo pi = { child, name, typeName, off, elemSize, flags,
                                      bOff, bMask, fMask, isReturn, isOut };
            if (isReturn || isOut) entry->OutParams.push_back(pi);
            if (isParam && !isReturn) entry->InParams.push_back(pi);
        };

        if (g_State.UStruct_ChildPropsOff >= 0) {
            for (void* child = propertyHead; child;
                 child = EngineDiscovery::GetFFieldNext(g_State, child)) {
                std::string name = EngineDiscovery::GetFFieldName(g_State, child);
                std::string typeName = EngineDiscovery::GetFFieldTypeName(g_State, child);
                int32_t off = 0;
                int32_t elemSize = 8;
                uint64_t flags = 0;
                if (g_State.FProperty_OffsetOff >= 0)
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff,
                                 &off, 4);
                if (g_State.FProperty_ElementSizeOff >= 0)
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_ElementSizeOff,
                                 &elemSize, 4);
                if (g_State.FProperty_PropertyFlagsOff >= 0)
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_PropertyFlagsOff,
                                 &flags, 8);
                ProcessChildProp(child, name, typeName, off, elemSize, flags);
            }
        } else if (g_State.UStruct_ChildrenOff >= 0) {
            for (void* child = propertyHead; child;
                 child = EngineDiscovery::GetFieldNext(g_State, child)) {
                std::string name =
                    EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(child));
                UClass* pCls = EngineDiscovery::GetObjectClass(g_State,
                                                               static_cast<UObject*>(child));
                std::string typeName =
                    pCls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(pCls))
                         : "Property";
                int32_t off = 0;
                int32_t elemSize = 8;
                uint64_t flags = 0;
                if (g_State.FProperty_OffsetOff >= 0)
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff,
                                 &off, 4);
                if (g_State.FProperty_ElementSizeOff >= 0)
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_ElementSizeOff,
                                 &elemSize, 4);
                if (g_State.FProperty_PropertyFlagsOff >= 0)
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_PropertyFlagsOff,
                                 &flags, 8);
                ProcessChildProp(child, name, typeName, off, elemSize, flags);
            }
        }

        cached = entry.get();
        s_UFunctionCallCache.emplace(ufunc, std::move(entry));
    }

    const int32_t paramsSize = cached->ParamsSize;
    LuaCallFrame* frame = CreateCallFrame(std::vector<uint64_t>(
        static_cast<size_t>((paramsSize > 0 ? paramsSize : 1) + 7) / 8 + 1, 0));
    uint8_t* paramsBase = reinterpret_cast<uint8_t*>(frame->Block.data());

    const bool reportShape = !cached->ShapeReported;
    std::string* shape = reportShape ? &cached->Shape : nullptr;
    int luaArgIdx = luaArgStart;
    std::vector<int32_t> outArgIndex(cached->OutParams.size(), 0);

    for (const UFunctionParamInfo& p : cached->InParams) {
        if (luaArgIdx <= lua_gettop(L)) {
            if (p.IsOut) {
                for (std::size_t i = 0; i < cached->OutParams.size(); ++i) {
                    if (cached->OutParams[i].Prop == p.Prop) {
                        outArgIndex[i] = luaArgIdx;
                        break;
                    }
                }
            }
            FoundProperty fp = { p.Prop, p.TypeName, p.Offset, p.ElementSize, nullptr, nullptr,
                                 nullptr, p.ByteOffset, p.ByteMask, p.FieldMask, true };
            const int wrote = SetPropertyValue(L, paramsBase, fp, luaArgIdx) ? 1 : 0;
            if (shape) {
                char bound[128];
                snprintf(bound, sizeof(bound), " %s<-%s#%d%s", p.Name.c_str(),
                         lua_typename(L, lua_type(L, luaArgIdx)), luaArgIdx,
                         wrote ? "" : "(not written)");
                *shape += bound;
            }
            ++luaArgIdx;
        } else if (shape) {
            char bound[128];
            snprintf(bound, sizeof(bound), " %s<-(missing)", p.Name.c_str());
            *shape += bound;
        }
    }

    EngineDiscovery::CallProcessEvent(g_State, object, ufunc, paramsBase);

    int returnCount = 0;
    auto pushOutValue = [&](std::size_t i) {
        const UFunctionParamInfo& p = cached->OutParams[i];
        FoundProperty fp = { p.Prop, p.TypeName, p.Offset, p.ElementSize, nullptr, nullptr, nullptr,
                             p.ByteOffset, p.ByteMask, p.FieldMask, true };

        PushPropertyValue(L, paramsBase, fp, frame);

        if (outArgIndex[i] > 0 && lua_istable(L, outArgIndex[i])) {
            const int valueIdx = lua_gettop(L);
            const bool reuseTable = p.TypeName.find("StructProperty") != std::string::npos ||
                                    p.TypeName.find("ArrayProperty") != std::string::npos ||
                                    p.TypeName.find("SetProperty") != std::string::npos;
            if (reuseTable) {
                if (lua_istable(L, valueIdx)) {
                    lua_pushvalue(L, outArgIndex[i]);
                    const int outTable = lua_gettop(L);
                    lua_pushnil(L);
                    while (lua_next(L, valueIdx) != 0) {
                        lua_pushvalue(L, -2);
                        lua_insert(L, -2);
                        lua_settable(L, outTable);
                    }
                    lua_pop(L, 1);
                } else if (lua_type(L, valueIdx) == LUA_TUSERDATA &&
                           p.TypeName.find("StructProperty") != std::string::npos) {
                    lua_pushvalue(L, outArgIndex[i]);
                    const int outTable = lua_gettop(L);
                    FillOutStructTable(L, outTable, valueIdx, fp);
                    lua_pop(L, 1);
                }
            } else {
                lua_pushvalue(L, outArgIndex[i]);
                lua_pushvalue(L, valueIdx);
                lua_setfield(L, -2, p.Name.c_str());
                lua_pop(L, 1);
            }
        }

        ++returnCount;
        if (shape) {
            char pushed[128];
            snprintf(pushed, sizeof(pushed), " %s->%s", p.Name.c_str(),
                     lua_typename(L, lua_type(L, -1)));
            *shape += pushed;
        }
    };

    for (std::size_t i = 0; i < cached->OutParams.size(); ++i) {
        if (cached->OutParams[i].IsReturn) pushOutValue(i);
    }
    for (std::size_t i = 0; i < cached->OutParams.size(); ++i) {
        if (!cached->OutParams[i].IsReturn) pushOutValue(i);
    }

    if (reportShape) {
        ReportCallShapeOnce(ufunc, paramsSize, *shape);
        cached->ShapeReported = true;
        cached->Shape.clear();
    }

    frame->Release();

    return returnCount;
}

static int UObj_GetAddress(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    lua_pushinteger(L, reinterpret_cast<uintptr_t>(o)); return 1;
}
static int UObj_GetName(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    std::string name = EngineDiscovery::GetObjectNameStr(g_State, o);
    lua_pushstring(L, name.c_str()); return 1;
}
static int UObj_GetFullName(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    std::string fn = EngineDiscovery::BuildFullName(g_State, o);
    lua_pushstring(L, fn.c_str()); return 1;
}

static int UObj_GetPathName(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    std::string pn = EngineDiscovery::BuildPathName(g_State, o);
    lua_pushstring(L, pn.c_str()); return 1;
}
static int UObj_GetClass(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    UClass* c = EngineDiscovery::GetObjectClass(g_State, o);
    PushUObject(L, static_cast<UObject*>(c)); return 1;
}
static int UObj_GetOuter(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    UObject* outer = EngineDiscovery::GetObjectOuter(g_State, o);
    PushUObject(L, outer); return 1;
}

static bool ClassChainCarriesName(const EngineState& state, UObject* o, const char* clsName) {
    if (!o || !clsName || !*clsName) return false;

    auto withoutGeneratedSuffix = [](const std::string& name) -> std::string {
        return (name.size() > 2 && name.compare(name.size() - 2, 2, "_C") == 0)
                   ? name.substr(0, name.size() - 2)
                   : name;
    };

    const std::string wanted = clsName;
    const std::string wantedBare = withoutGeneratedSuffix(wanted);

    UClass* cls = EngineDiscovery::GetObjectClass(state, o);
    const std::string ownClassName =
        cls ? EngineDiscovery::GetObjectNameStr(state, static_cast<UObject*>(cls)) : std::string();
    UStruct* cur = (ownClassName == "Class" || ownClassName == "BlueprintGeneratedClass")
                       ? static_cast<UStruct*>(o)
                       : static_cast<UStruct*>(cls);

    for (int depth = 0; depth < 32 && cur; ++depth) {
        const std::string n = EngineDiscovery::GetObjectNameStr(state, static_cast<UObject*>(cur));
        if (_stricmp(n.c_str(), wanted.c_str()) == 0) return true;
        if (_stricmp(withoutGeneratedSuffix(n).c_str(), wantedBare.c_str()) == 0) return true;
        cur = EngineDiscovery::GetSuperStruct(state, cur);
    }
    return false;
}

static int UObj_IsA(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o || !SafeMem_IsReadable(o, 32)) { lua_pushboolean(L, 0); return 1; }

    const char* clsName = luaL_checkstring(L, 2);
    if (!clsName || !*clsName) { lua_pushboolean(L, 0); return 1; }

    if (ClassChainCarriesName(g_State, o, clsName)) { lua_pushboolean(L, 1); return 1; }

    UClass* targetCls = ConsoleEnabler::FindClassByNameExact(g_State, clsName);
    if (!targetCls) {
        std::string withC = std::string(clsName) + "_C";
        targetCls = ConsoleEnabler::FindClassByNameExact(g_State, withC.c_str());
    }
    const bool isA = targetCls && ConsoleEnabler::IsObjectA(g_State, o, targetCls);
    lua_pushboolean(L, isA ? 1 : 0);
    return 1;
}
static int UObj_IsValid(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    lua_pushboolean(L, (o && SafeMem_IsReadable(o, 32)) ? 1 : 0); return 1;
}
static int UObj_Get(lua_State* L) {
    lua_pushvalue(L, 1);
    return 1;
}

static bool Helper_GetActorLocation(UObject* obj, Vector3D& outLoc) {
    if (!obj || !SafeMem_IsReadable(obj, 32)) return false;
    FastMemScope fastMem;
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    if (!cls) return false;

    FoundProperty rcProp = FindPropertyOnClass(cls, "RootComponent");
    if (rcProp.Found && rcProp.Offset >= 0) {
        UObject* rootComp = nullptr;
        SafeMem_Read(reinterpret_cast<uint8_t*>(obj) + rcProp.Offset, &rootComp, sizeof(void*));
        if (rootComp && SafeMem_IsReadable(rootComp, 32)) {
            UClass* rcCls = EngineDiscovery::GetObjectClass(g_State, rootComp);
            if (rcCls) {
                FoundProperty locProp = FindPropertyOnClass(rcCls, "RelativeLocation");
                if (locProp.Found && locProp.Offset >= 0) {
                    if (g_State.UEMajor >= 5 || locProp.ElementSize == 24) {
                        struct DoubleVector3D { double X, Y, Z; } dv = {};
                        SafeMem_Read(reinterpret_cast<uint8_t*>(rootComp) + locProp.Offset, &dv, sizeof(dv));
                        outLoc = { static_cast<float>(dv.X), static_cast<float>(dv.Y), static_cast<float>(dv.Z) };
                    } else {
                        SafeMem_Read(reinterpret_cast<uint8_t*>(rootComp) + locProp.Offset, &outLoc, sizeof(Vector3D));
                    }
                    return true;
                }
            }
        }
    }

    void* fn = FindFunctionOnClass(cls, "K2_GetActorLocation");
    if (!fn) fn = FindFunctionOnClass(cls, "GetActorLocation");
    if (fn) {
        uint8_t buf[64] = {0};
        if (EngineDiscovery::CallProcessEvent(g_State, obj, fn, buf)) {
            if (g_State.UEMajor >= 5) {
                struct DoubleVector3D { double X, Y, Z; } dv = {};
                SafeMem_Read(buf, &dv, sizeof(dv));
                outLoc = { static_cast<float>(dv.X), static_cast<float>(dv.Y), static_cast<float>(dv.Z) };
            } else {
                SafeMem_Read(buf, &outLoc, sizeof(Vector3D));
            }
            return true;
        }
    }

    return false;
}

static bool Helper_SetActorLocation(UObject* obj, const Vector3D& loc) {
    if (!obj || !SafeMem_IsReadable(obj, 32)) return false;
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    if (!cls) return false;

    void* fn = FindFunctionOnClass(cls, "K2_SetActorLocation");
    if (fn) {
        uint8_t buf[256] = {0};
        if (g_State.UEMajor >= 5) {
            struct DoubleVector3D { double X, Y, Z; } dv = { loc.X, loc.Y, loc.Z };
            memcpy(buf, &dv, sizeof(dv));
        } else {
            memcpy(buf, &loc, sizeof(Vector3D));
        }
        return EngineDiscovery::CallProcessEvent(g_State, obj, fn, buf);
    }

    FoundProperty rcProp = FindPropertyOnClass(cls, "RootComponent");
    if (rcProp.Found && rcProp.Offset >= 0) {
        UObject* rootComp = nullptr;
        SafeMem_Read(reinterpret_cast<uint8_t*>(obj) + rcProp.Offset, &rootComp, sizeof(void*));
        if (rootComp && SafeMem_IsReadable(rootComp, 32)) {
            UClass* rcCls = EngineDiscovery::GetObjectClass(g_State, rootComp);
            if (rcCls) {
                FoundProperty locProp = FindPropertyOnClass(rcCls, "RelativeLocation");
                if (locProp.Found && locProp.Offset >= 0) {
                    if (g_State.UEMajor >= 5 || locProp.ElementSize == 24) {
                        struct DoubleVector3D { double X, Y, Z; } dv = { loc.X, loc.Y, loc.Z };
                        DWORD old = 0;
                        VirtualProtect(reinterpret_cast<uint8_t*>(rootComp) + locProp.Offset, sizeof(dv), PAGE_READWRITE, &old);
                        memcpy(reinterpret_cast<uint8_t*>(rootComp) + locProp.Offset, &dv, sizeof(dv));
                        VirtualProtect(reinterpret_cast<uint8_t*>(rootComp) + locProp.Offset, sizeof(dv), old, &old);
                    } else {
                        DWORD old = 0;
                        VirtualProtect(reinterpret_cast<uint8_t*>(rootComp) + locProp.Offset, sizeof(loc), PAGE_READWRITE, &old);
                        memcpy(reinterpret_cast<uint8_t*>(rootComp) + locProp.Offset, &loc, sizeof(loc));
                        VirtualProtect(reinterpret_cast<uint8_t*>(rootComp) + locProp.Offset, sizeof(loc), old, &old);
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

static bool Helper_GetActorRotation(UObject* obj, Rotator3D& outRot) {
    if (!obj || !SafeMem_IsReadable(obj, 32)) return false;
    FastMemScope fastMem;
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    if (!cls) return false;

    void* fn = FindFunctionOnClass(cls, "K2_GetActorRotation");
    if (!fn) fn = FindFunctionOnClass(cls, "GetActorRotation");
    if (fn) {
        uint8_t buf[64] = {0};
        if (EngineDiscovery::CallProcessEvent(g_State, obj, fn, buf)) {
            if (g_State.UEMajor >= 5) {
                struct DoubleRotator3D { double Pitch, Yaw, Roll; } dr = {};
                SafeMem_Read(buf, &dr, sizeof(dr));
                outRot = { static_cast<float>(dr.Pitch), static_cast<float>(dr.Yaw), static_cast<float>(dr.Roll) };
            } else {
                SafeMem_Read(buf, &outRot, sizeof(Rotator3D));
            }
            return true;
        }
    }

    FoundProperty rcProp = FindPropertyOnClass(cls, "RootComponent");
    if (rcProp.Found && rcProp.Offset >= 0) {
        UObject* rootComp = nullptr;
        SafeMem_Read(reinterpret_cast<uint8_t*>(obj) + rcProp.Offset, &rootComp, sizeof(void*));
        if (rootComp && SafeMem_IsReadable(rootComp, 32)) {
            UClass* rcCls = EngineDiscovery::GetObjectClass(g_State, rootComp);
            if (rcCls) {
                FoundProperty rotProp = FindPropertyOnClass(rcCls, "RelativeRotation");
                if (rotProp.Found && rotProp.Offset >= 0) {
                    if (g_State.UEMajor >= 5 || rotProp.ElementSize == 24) {
                        struct DoubleRotator3D { double Pitch, Yaw, Roll; } dr = {};
                        SafeMem_Read(reinterpret_cast<uint8_t*>(rootComp) + rotProp.Offset, &dr, sizeof(dr));
                        outRot = { static_cast<float>(dr.Pitch), static_cast<float>(dr.Yaw), static_cast<float>(dr.Roll) };
                    } else {
                        SafeMem_Read(reinterpret_cast<uint8_t*>(rootComp) + rotProp.Offset, &outRot, sizeof(Rotator3D));
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

static bool Helper_SetActorRotation(UObject* obj, const Rotator3D& rot) {
    if (!obj || !SafeMem_IsReadable(obj, 32)) return false;
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    if (!cls) return false;

    void* fn = FindFunctionOnClass(cls, "K2_SetActorRotation");
    if (fn) {
        uint8_t buf[128] = {0};
        int32_t rotSize = (g_State.UEMajor >= 5) ? 24 : 12;
        if (g_State.UEMajor >= 5) {
            struct DoubleRotator3D { double Pitch, Yaw, Roll; } dr = { rot.Pitch, rot.Yaw, rot.Roll };
            memcpy(buf, &dr, sizeof(dr));
        } else {
            memcpy(buf, &rot, sizeof(Rotator3D));
        }
        buf[rotSize] = 1;
        return EngineDiscovery::CallProcessEvent(g_State, obj, fn, buf);
    }
    return false;
}

static bool Helper_Teleport(UObject* obj, const Vector3D& loc, const Rotator3D* optRot = nullptr) {
    if (!obj || !SafeMem_IsReadable(obj, 32)) return false;
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    if (!cls) return false;

    void* fn = FindFunctionOnClass(cls, "K2_TeleportTo");
    if (fn) {
        uint8_t buf[128] = {0};
        if (g_State.UEMajor >= 5) {
            struct DoubleVector3D { double X, Y, Z; } dv = { loc.X, loc.Y, loc.Z };
            memcpy(buf, &dv, sizeof(dv));
            if (optRot) {
                struct DoubleRotator3D { double Pitch, Yaw, Roll; } dr = { optRot->Pitch, optRot->Yaw, optRot->Roll };
                memcpy(buf + 24, &dr, sizeof(dr));
            } else {
                Rotator3D curRot;
                Helper_GetActorRotation(obj, curRot);
                struct DoubleRotator3D { double Pitch, Yaw, Roll; } dr = { curRot.Pitch, curRot.Yaw, curRot.Roll };
                memcpy(buf + 24, &dr, sizeof(dr));
            }
        } else {
            memcpy(buf, &loc, sizeof(Vector3D));
            if (optRot) {
                memcpy(buf + 12, optRot, sizeof(Rotator3D));
            } else {
                Rotator3D curRot;
                Helper_GetActorRotation(obj, curRot);
                memcpy(buf + 12, &curRot, sizeof(Rotator3D));
            }
        }
        return EngineDiscovery::CallProcessEvent(g_State, obj, fn, buf);
    }

    bool ok = Helper_SetActorLocation(obj, loc);
    if (optRot) {
        ok = Helper_SetActorRotation(obj, *optRot) && ok;
    }
    return ok;
}

static bool Helper_GetVelocity(UObject* obj, Vector3D& outVel) {
    if (!obj || !SafeMem_IsReadable(obj, 32)) return false;
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    if (!cls) return false;

    void* fn = FindFunctionOnClass(cls, "GetVelocity");
    if (fn) {
        uint8_t buf[64] = {0};
        if (EngineDiscovery::CallProcessEvent(g_State, obj, fn, buf)) {
            if (g_State.UEMajor >= 5) {
                struct DoubleVector3D { double X, Y, Z; } dv = {};
                SafeMem_Read(buf, &dv, sizeof(dv));
                outVel = { static_cast<float>(dv.X), static_cast<float>(dv.Y), static_cast<float>(dv.Z) };
            } else {
                SafeMem_Read(buf, &outVel, sizeof(Vector3D));
            }
            return true;
        }
    }
    return false;
}

static int UObj_GetLocation(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    Vector3D loc;
    if (Helper_GetActorLocation(o, loc)) {
        PushVector(L, loc.X, loc.Y, loc.Z);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int UObj_SetLocation(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    Vector3D loc;
    if (lua_gettop(L) >= 4 && !lua_isuserdata(L, 2)) {
        loc.X = static_cast<float>(luaL_checknumber(L, 2));
        loc.Y = static_cast<float>(luaL_checknumber(L, 3));
        loc.Z = static_cast<float>(luaL_checknumber(L, 4));
    } else if (GetVector(L, 2, loc)) {

    } else {
        return luaL_error(L, "SetLocation requires a Vector or (x, y, z)");
    }
    bool ok = Helper_SetActorLocation(o, loc);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int UObj_GetRotation(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    Rotator3D rot;
    if (Helper_GetActorRotation(o, rot)) {
        PushRotator(L, rot.Pitch, rot.Yaw, rot.Roll);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int UObj_SetRotation(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    Rotator3D rot;
    if (lua_gettop(L) >= 4 && !lua_isuserdata(L, 2)) {
        rot.Pitch = static_cast<float>(luaL_checknumber(L, 2));
        rot.Yaw   = static_cast<float>(luaL_checknumber(L, 3));
        rot.Roll  = static_cast<float>(luaL_checknumber(L, 4));
    } else if (GetRotator(L, 2, rot)) {

    } else {
        return luaL_error(L, "SetRotation requires a Rotator or (pitch, yaw, roll)");
    }
    bool ok = Helper_SetActorRotation(o, rot);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int UObj_Teleport(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    Vector3D loc;
    int nextArg = 2;
    if (lua_gettop(L) >= 4 && !lua_isuserdata(L, 2)) {
        loc.X = static_cast<float>(luaL_checknumber(L, 2));
        loc.Y = static_cast<float>(luaL_checknumber(L, 3));
        loc.Z = static_cast<float>(luaL_checknumber(L, 4));
        nextArg = 5;
    } else if (GetVector(L, 2, loc)) {
        nextArg = 3;
    } else {
        return luaL_error(L, "Teleport requires a Vector or (x, y, z)");
    }

    Rotator3D rot;
    bool hasRot = false;
    if (lua_gettop(L) >= nextArg + 2 && !lua_isuserdata(L, nextArg)) {
        rot.Pitch = static_cast<float>(luaL_checknumber(L, nextArg));
        rot.Yaw   = static_cast<float>(luaL_checknumber(L, nextArg + 1));
        rot.Roll  = static_cast<float>(luaL_checknumber(L, nextArg + 2));
        hasRot = true;
    } else if (nextArg <= lua_gettop(L) && GetRotator(L, nextArg, rot)) {
        hasRot = true;
    }

    bool ok = Helper_Teleport(o, loc, hasRot ? &rot : nullptr);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int UObj_GetVelocity(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    Vector3D vel;
    if (Helper_GetVelocity(o, vel)) {
        PushVector(L, vel.X, vel.Y, vel.Z);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int UObj_GetDistanceTo(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    Vector3D loc1;
    if (!Helper_GetActorLocation(o, loc1)) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    Vector3D loc2 = {};
    if (lua_isuserdata(L, 2)) {
        UObject* otherObj = GetUObject(L, 2);
        if (otherObj) {
            Helper_GetActorLocation(otherObj, loc2);
        } else {
            GetVector(L, 2, loc2);
        }
    } else if (lua_gettop(L) >= 4) {
        loc2.X = static_cast<float>(luaL_checknumber(L, 2));
        loc2.Y = static_cast<float>(luaL_checknumber(L, 3));
        loc2.Z = static_cast<float>(luaL_checknumber(L, 4));
    }

    float dx = loc1.X - loc2.X;
    float dy = loc1.Y - loc2.Y;
    float dz = loc1.Z - loc2.Z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    lua_pushnumber(L, static_cast<lua_Number>(dist));
    return 1;
}

static int UObj_GetPropertyNested(lua_State* L, UObject* o, const std::string& path) {
    if (!o || path.empty()) { lua_pushnil(L); return 1; }
    size_t dotPos = path.find('.');
    if (dotPos == std::string::npos) {
        UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
        if (cls) {
            FoundProperty prop = FindPropertyOnClass(cls, path);
            if (prop.Found) {
                return PushPropertyValue(L, reinterpret_cast<uint8_t*>(o), prop);
            }
        }
        lua_pushnil(L);
        return 1;
    }

    std::string head = path.substr(0, dotPos);
    std::string tail = path.substr(dotPos + 1);

    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    if (!cls) { lua_pushnil(L); return 1; }

    FoundProperty prop = FindPropertyOnClass(cls, head);
    if (!prop.Found) { lua_pushnil(L); return 1; }

    PushPropertyValue(L, reinterpret_cast<uint8_t*>(o), prop);
    if (lua_isnil(L, -1)) return 1;

    UObject* nextObj = GetUObject(L, -1);
    if (nextObj) {
        lua_pop(L, 1);
        return UObj_GetPropertyNested(L, nextObj, tail);
    }

    if (lua_istable(L, -1) || lua_isuserdata(L, -1)) {
        size_t nextDot = tail.find('.');
        std::string subKey = (nextDot == std::string::npos) ? tail : tail.substr(0, nextDot);
        std::string remainder = (nextDot == std::string::npos) ? "" : tail.substr(nextDot + 1);

        lua_getfield(L, -1, subKey.c_str());
        lua_remove(L, -2);

        if (remainder.empty()) return 1;

        UObject* subObj = GetUObject(L, -1);
        if (subObj) {
            lua_pop(L, 1);
            return UObj_GetPropertyNested(L, subObj, remainder);
        }
    }

    return 1;
}

static bool UObj_SetPropertyNested(lua_State* L, UObject* o, const std::string& path, int valueIdx) {
    if (!o || path.empty()) return false;
    size_t dotPos = path.find('.');
    if (dotPos == std::string::npos) {
        UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
        if (cls) {
            FoundProperty prop = FindPropertyOnClass(cls, path);
            if (prop.Found) {
                return SetPropertyValue(L, reinterpret_cast<uint8_t*>(o), prop, valueIdx);
            }
        }
        return false;
    }

    std::string head = path.substr(0, dotPos);
    std::string tail = path.substr(dotPos + 1);

    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    if (!cls) return false;

    FoundProperty prop = FindPropertyOnClass(cls, head);
    if (!prop.Found) return false;

    PushPropertyValue(L, reinterpret_cast<uint8_t*>(o), prop);
    UObject* nextObj = GetUObject(L, -1);
    if (nextObj) {
        lua_pop(L, 1);
        return UObj_SetPropertyNested(L, nextObj, tail, valueIdx);
    }

    if (lua_istable(L, -1) || lua_isuserdata(L, -1)) {
        lua_pushvalue(L, valueIdx);
        lua_setfield(L, -2, tail.c_str());
        lua_pop(L, 1);
        return true;
    }

    lua_pop(L, 1);
    return false;
}

static int UObj_GetProperty(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_pushnil(L); return 1; }
    const char* propName = luaL_checkstring(L, 2);
    return UObj_GetPropertyNested(L, o, propName);
}

static int UObj_SetProperty(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_pushboolean(L, 0); return 1; }
    const char* propName = luaL_checkstring(L, 2);
    bool ok = UObj_SetPropertyNested(L, o, propName, 3);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int UObj_GetCDO(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_pushnil(L); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    if (!cls) { lua_pushnil(L); return 1; }
    std::string cName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cls));
    UClass* targetCls = (cName == "Class" || cName == "BlueprintGeneratedClass") ? static_cast<UClass*>(o) : cls;

    if (g_State.UClass_ClassDefaultObjectOff >= 0) {
        void* cdoPtr = nullptr;
        if (SafeMem_Read(reinterpret_cast<uint8_t*>(targetCls) + g_State.UClass_ClassDefaultObjectOff,
                         &cdoPtr, sizeof(void*)) && cdoPtr) {
            if (EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(cdoPtr)) == targetCls) {
                PushUObject(L, static_cast<UObject*>(cdoPtr));
                return 1;
            }
        }
    }

    UObject* cdo = EngineDiscovery::FindClassDefaultObject(g_State, targetCls);
    if (cdo) {
        PushUObject(L, cdo);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

static int UObj_GetSuperClass(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_pushnil(L); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    if (!cls) { lua_pushnil(L); return 1; }
    std::string cName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cls));
    UStruct* targetStruct = (cName == "Class" || cName == "BlueprintGeneratedClass") ? static_cast<UStruct*>(o) : static_cast<UStruct*>(cls);
    UStruct* superStruct = EngineDiscovery::GetSuperStruct(g_State, targetStruct);
    PushUObject(L, static_cast<UObject*>(superStruct));
    return 1;
}

static int UObj_GetChildClasses(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_newtable(L); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    std::string cName = cls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cls)) : "";
    UClass* targetCls = (cName == "Class" || cName == "BlueprintGeneratedClass") ? static_cast<UClass*>(o) : cls;

    lua_newtable(L);
    if (!targetCls || !g_State.GUObjectArray) return 1;

    int idx = 1;
    int count = EngineDiscovery::GetObjectCount(g_State);
    for (int i = 0; i < count; ++i) {
        void* candidate = EngineDiscovery::GetObjectByIndex(g_State, i);
        if (!candidate) continue;
        UClass* cCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(candidate));
        std::string ccName = cCls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cCls)) : "";
        if (ccName == "Class" || ccName == "BlueprintGeneratedClass") {
            UStruct* s = EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(candidate));
            while (s) {
                if (s == static_cast<UStruct*>(targetCls)) {
                    PushUObject(L, static_cast<UObject*>(candidate));
                    lua_rawseti(L, -2, idx++);
                    break;
                }
                s = EngineDiscovery::GetSuperStruct(g_State, s);
            }
        }
    }
    return 1;
}

static int UObj_IsChildOf(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_pushboolean(L, 0); return 1; }
    const char* targetName = luaL_checkstring(L, 2);

    lua_pushboolean(L, ClassChainCarriesName(g_State, o, targetName) ? 1 : 0);
    return 1;
}

static int UObj_GetWorld(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_pushnil(L); return 1; }

    UObject* cur = o;
    while (cur) {
        UClass* c = EngineDiscovery::GetObjectClass(g_State, cur);
        std::string cName = c ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(c)) : "";
        if (cName == "World") {
            PushUObject(L, cur);
            return 1;
        }
        cur = EngineDiscovery::GetObjectOuter(g_State, cur);
    }

    void* liveWorld = g_State.GWorld;
    if (!liveWorld && g_State.GWorldPtr) SafeMem_Read(g_State.GWorldPtr, &liveWorld, sizeof(void*));
    if (liveWorld) {
        PushUObject(L, static_cast<UObject*>(liveWorld));
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int UObj_DumpProperties(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_newtable(L); return 1; }
    UStruct* cur = FieldOwnerOf(o);
    lua_newtable(L);
    if (!cur) return 1;

    std::string objName = EngineDiscovery::GetObjectNameStr(g_State, o);
    std::string className = cur ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cur)) : "Unknown";
    Log("[DumpProperties] === Properties for %s (%s) ===", objName.c_str(), className.c_str());

    int propCount = 0;
    while (cur) {
        if (g_State.UStruct_ChildPropsOff >= 0) {
            void* child = EngineDiscovery::GetChildPropsHead(g_State, cur);
            while (child) {
                std::string name = EngineDiscovery::GetFFieldName(g_State, child);
                std::string typeName = EngineDiscovery::GetFFieldTypeName(g_State, child);
                int32_t off = -1, elemSize = 0;
                if (g_State.FProperty_OffsetOff >= 0) SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &off, 4);
                if (g_State.FProperty_ElementSizeOff >= 0) SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_ElementSizeOff, &elemSize, 4);

                if (!name.empty() && typeName.find("Function") == std::string::npos) {
                    lua_newtable(L);
                    lua_pushstring(L, typeName.c_str()); lua_setfield(L, -2, "Type");
                    lua_pushinteger(L, off); lua_setfield(L, -2, "Offset");
                    lua_pushinteger(L, elemSize); lua_setfield(L, -2, "Size");
                    FoundProperty fp = MakeFoundProperty(child, typeName, off, elemSize);
                    PushPropertyValue(L, reinterpret_cast<uint8_t*>(o), fp);
                    lua_setfield(L, -2, "Value");

                    lua_setfield(L, -2, name.c_str());
                    Log("[DumpProperties]   %-32s (Type: %-16s, Offset: 0x%04X, Size: %d)", name.c_str(), typeName.c_str(), off, elemSize);
                    propCount++;
                }
                child = EngineDiscovery::GetFFieldNext(g_State, child);
            }
        } else if (g_State.UStruct_ChildrenOff >= 0) {
            void* child = nullptr;
            SafeMem_Read(reinterpret_cast<uint8_t*>(cur) + g_State.UStruct_ChildrenOff, &child, sizeof(void*));
            int guard = 0;
            while (child && ++guard < 8192) {
                std::string name = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(child));
                UClass* childCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(child));
                std::string childClsName = childCls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(childCls)) : "";
                if (childClsName.find("Property") != std::string::npos && !name.empty()) {
                    int32_t off = -1, elemSize = 0;
                    if (g_State.FProperty_OffsetOff >= 0) SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &off, 4);
                    if (g_State.FProperty_ElementSizeOff >= 0) SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_ElementSizeOff, &elemSize, 4);

                    lua_newtable(L);
                    lua_pushstring(L, childClsName.c_str()); lua_setfield(L, -2, "Type");
                    lua_pushinteger(L, off); lua_setfield(L, -2, "Offset");
                    lua_pushinteger(L, elemSize); lua_setfield(L, -2, "Size");
                    FoundProperty fp = MakeFoundProperty(child, childClsName, off, elemSize);
                    PushPropertyValue(L, reinterpret_cast<uint8_t*>(o), fp);
                    lua_setfield(L, -2, "Value");

                    lua_setfield(L, -2, name.c_str());
                    Log("[DumpProperties]   %-32s (Type: %-16s, Offset: 0x%04X, Size: %d)", name.c_str(), childClsName.c_str(), off, elemSize);
                    propCount++;
                }
                void* next = nullptr;
                int32_t nextOff = CompatVerifiedFieldNextOffset();
                if (nextOff >= 0) SafeMem_Read(reinterpret_cast<uint8_t*>(child) + nextOff, &next, sizeof(void*));
                child = next;
            }
        }
        cur = EngineDiscovery::GetSuperStruct(g_State, cur);
    }
    Log("[DumpProperties] Total: %d properties dumped", propCount);
    return 1;
}

static int UObj_ExportProperty(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    const char* propName = luaL_checkstring(L, 2);
    if (!o || !propName) { lua_pushnil(L); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    if (!cls) { lua_pushnil(L); return 1; }
    FoundProperty prop = FindPropertyOnClass(cls, propName);
    if (!prop.Found) { lua_pushnil(L); return 1; }
    return PushPropertyValue(L, reinterpret_cast<uint8_t*>(o), prop);
}

static int UObj_ImportProperty(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    const char* propName = luaL_checkstring(L, 2);
    if (!o || !propName) { lua_pushboolean(L, 0); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    if (!cls) { lua_pushboolean(L, 0); return 1; }
    FoundProperty prop = FindPropertyOnClass(cls, propName);
    if (!prop.Found) { lua_pushboolean(L, 0); return 1; }
    bool ok = SetPropertyValue(L, reinterpret_cast<uint8_t*>(o), prop, 3);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int UObj_DumpFunctions(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_newtable(L); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    lua_newtable(L);
    if (!cls) return 1;

    std::string objName = EngineDiscovery::GetObjectNameStr(g_State, o);
    std::string className = cls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cls)) : "Unknown";
    Log("[DumpFunctions] === Functions for %s (%s) ===", objName.c_str(), className.c_str());

    int fnCount = 0;
    UStruct* cur = static_cast<UStruct*>(cls);
    while (cur) {
        if (g_State.UStruct_ChildrenOff >= 0) {
            void* child = nullptr;
            SafeMem_Read(reinterpret_cast<uint8_t*>(cur) + g_State.UStruct_ChildrenOff, &child, sizeof(void*));
            int guard = 0;
            while (child && ++guard < 8192) {
                std::string name = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(child));
                UClass* childCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(child));
                std::string childClsName = childCls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(childCls)) : "";
                if (childClsName.find("Function") != std::string::npos && !name.empty()) {
                    lua_newtable(L);
                    uint64_t fnFlags = 0;
                    if (g_State.UFunction_FlagsOff >= 0) {
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.UFunction_FlagsOff, &fnFlags, sizeof(fnFlags));
                    }
                    char hexFlags[32];
                    snprintf(hexFlags, sizeof(hexFlags), "0x%llX", fnFlags);
                    lua_pushstring(L, hexFlags); lua_setfield(L, -2, "Flags");
                    lua_pushinteger(L, static_cast<lua_Integer>(fnFlags)); lua_setfield(L, -2, "RawFlags");
                    lua_setfield(L, -2, name.c_str());

                    Log("[DumpFunctions]   %-40s (Flags: %s)", name.c_str(), hexFlags);
                    fnCount++;
                }
                void* next = nullptr;
                int32_t nextOff = CompatVerifiedFieldNextOffset();
                if (nextOff >= 0) SafeMem_Read(reinterpret_cast<uint8_t*>(child) + nextOff, &next, sizeof(void*));
                child = next;
            }
        }
        cur = EngineDiscovery::GetSuperStruct(g_State, cur);
    }
    Log("[DumpFunctions] Total: %d functions dumped", fnCount);
    return 1;
}

static int UObj_GetForwardVector(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    Rotator3D rot;
    if (Helper_GetActorRotation(o, rot)) {
        constexpr float DEG_TO_RAD = 3.1415926535f / 180.0f;
        float p = rot.Pitch * DEG_TO_RAD, y = rot.Yaw * DEG_TO_RAD;
        float cp = std::cos(p), sp = std::sin(p);
        float cy = std::cos(y), sy = std::sin(y);
        PushVector(L, cp * cy, cp * sy, sp);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int UObj_GetRightVector(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    Rotator3D rot;
    if (Helper_GetActorRotation(o, rot)) {
        constexpr float DEG_TO_RAD = 3.1415926535f / 180.0f;
        float p = rot.Pitch * DEG_TO_RAD, y = rot.Yaw * DEG_TO_RAD, roll = rot.Roll * DEG_TO_RAD;
        float cp = std::cos(p), sp = std::sin(p);
        float cy = std::cos(y), sy = std::sin(y);
        float cr = std::cos(roll), sr = std::sin(roll);
        PushVector(L, cy * sp * sr - sy * cr, sy * sp * sr + cy * cr, -cp * sr);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int UObj_GetUpVector(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    Rotator3D rot;
    if (Helper_GetActorRotation(o, rot)) {
        constexpr float DEG_TO_RAD = 3.1415926535f / 180.0f;
        float p = rot.Pitch * DEG_TO_RAD, y = rot.Yaw * DEG_TO_RAD, roll = rot.Roll * DEG_TO_RAD;
        float cp = std::cos(p), sp = std::sin(p);
        float cy = std::cos(y), sy = std::sin(y);
        float cr = std::cos(roll), sr = std::sin(roll);
        PushVector(L, -cy * sp * cr - sy * sr, -sy * sp * cr + cy * sr, cp * cr);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int UObj_AddActorWorldOffset(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    Vector3D delta;
    if (!GetVector(L, 2, delta)) return luaL_error(L, "AddActorWorldOffset expects FVector");
    Vector3D cur;
    if (Helper_GetActorLocation(o, cur)) {
        bool ok = Helper_SetActorLocation(o, { cur.X + delta.X, cur.Y + delta.Y, cur.Z + delta.Z });
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int UObj_AddActorLocalOffset(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    Vector3D delta;
    if (!GetVector(L, 2, delta)) return luaL_error(L, "AddActorLocalOffset expects FVector");
    Vector3D curLoc;
    Rotator3D curRot;
    if (Helper_GetActorLocation(o, curLoc) && Helper_GetActorRotation(o, curRot)) {
        constexpr float DEG_TO_RAD = 3.1415926535f / 180.0f;
        float p = curRot.Pitch * DEG_TO_RAD, y = curRot.Yaw * DEG_TO_RAD, r = curRot.Roll * DEG_TO_RAD;
        float cp = std::cos(p), sp = std::sin(p);
        float cy = std::cos(y), sy = std::sin(y);
        float cr = std::cos(r), sr = std::sin(r);
        Vector3D worldDelta = {
            delta.X * (cp * cy) + delta.Y * (cy * sp * sr - sy * cr) + delta.Z * (-cy * sp * cr - sy * sr),
            delta.X * (cp * sy) + delta.Y * (sy * sp * sr + cy * cr) + delta.Z * (-sy * sp * cr + cy * sr),
            delta.X * (sp)       + delta.Y * (-cp * sr)               + delta.Z * (cp * cr)
        };
        bool ok = Helper_SetActorLocation(o, { curLoc.X + worldDelta.X, curLoc.Y + worldDelta.Y, curLoc.Z + worldDelta.Z });
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int UObj_AddActorWorldRotation(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    Rotator3D delta;
    if (!GetRotator(L, 2, delta)) return luaL_error(L, "AddActorWorldRotation expects FRotator");
    Rotator3D curRot;
    if (Helper_GetActorRotation(o, curRot)) {
        bool ok = Helper_SetActorRotation(o, { curRot.Pitch + delta.Pitch, curRot.Yaw + delta.Yaw, curRot.Roll + delta.Roll });
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int UObj_AddActorLocalRotation(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    Rotator3D delta;
    if (!GetRotator(L, 2, delta)) return luaL_error(L, "AddActorLocalRotation expects FRotator");
    Rotator3D curRot;
    if (Helper_GetActorRotation(o, curRot)) {
        bool ok = Helper_SetActorRotation(o, { curRot.Pitch + delta.Pitch, curRot.Yaw + delta.Yaw, curRot.Roll + delta.Roll });
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int UObj_GetRootComponent(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_pushnil(L); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    if (cls) {
        FoundProperty rcProp = FindPropertyOnClass(cls, "RootComponent");
        if (rcProp.Found && rcProp.Offset >= 0) {
            UObject* rc = nullptr;
            SafeMem_Read(reinterpret_cast<uint8_t*>(o) + rcProp.Offset, &rc, sizeof(void*));
            PushUObject(L, rc);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int UObj_AttachToActor(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    UObject* parent = GetUObject(L, 2);
    if (!o || !parent) { lua_pushboolean(L, 0); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    void* fn = FindFunctionOnClass(cls, "K2_AttachToActor");
    if (!fn) fn = FindFunctionOnClass(cls, "AttachToActor");
    if (fn) {
        struct Params {
            UObject* ParentActor;
            FName SocketName;
            uint8_t LocationRule;
            uint8_t RotationRule;
            uint8_t ScaleRule;
            bool bWeldSimulatedBodies;
        } params = { parent, {}, 1, 1, 1, true };
        bool ok = EngineDiscovery::CallProcessEvent(g_State, o, fn, &params);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    void* fnRoot = FindFunctionOnClass(cls, "K2_AttachRootComponentToActor");
    if (fnRoot) {
        struct ParamsRoot {
            UObject* InParentActor;
            FName InSocketName;
            uint8_t AttachLocationType;
            bool bWeldSimulatedBodies;
        } pRoot = { parent, {}, 1, true };
        bool ok = EngineDiscovery::CallProcessEvent(g_State, o, fnRoot, &pRoot);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int UObj_AttachToComponent(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    UObject* parentComp = GetUObject(L, 2);
    if (!o || !parentComp) { lua_pushboolean(L, 0); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    void* fn = FindFunctionOnClass(cls, "K2_AttachToComponent");
    if (fn) {
        struct Params {
            UObject* Parent;
            FName SocketName;
            uint8_t LocationRule;
            uint8_t RotationRule;
            uint8_t ScaleRule;
            bool bWeldSimulatedBodies;
        } params = { parentComp, {}, 1, 1, 1, true };
        bool ok = EngineDiscovery::CallProcessEvent(g_State, o, fn, &params);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int UObj_DetachFromActor(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_pushboolean(L, 0); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    void* fn = FindFunctionOnClass(cls, "K2_DetachFromActor");
    if (fn) {
        struct Params {
            uint8_t LocationRule;
            uint8_t RotationRule;
            uint8_t ScaleRule;
        } params = { 1, 1, 1 };
        bool ok = EngineDiscovery::CallProcessEvent(g_State, o, fn, &params);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int UObj_GetAttachedActors(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_newtable(L); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    void* fn = FindFunctionOnClass(cls, "GetAttachedActors");
    if (fn) {
        struct Params {
            void* OutActors;
            int32_t Count;
            int32_t Max;
            bool bResetArray;
        } params = { nullptr, 0, 0, true };
        if (EngineDiscovery::CallProcessEvent(g_State, o, fn, &params) && params.OutActors && params.Count > 0) {
            lua_newtable(L);
            UObject** actors = reinterpret_cast<UObject**>(params.OutActors);
            for (int i = 0; i < params.Count; ++i) {
                if (actors[i]) {
                    PushUObject(L, actors[i]);
                    lua_rawseti(L, -2, i + 1);
                }
            }
            return 1;
        }
    }
    lua_newtable(L);
    FoundProperty chProp = FindPropertyOnClass(cls, "Children");
    if (chProp.Found && chProp.Offset >= 0) {
        TArray<UObject*> chArr = {};
        if (SafeMem_Read(reinterpret_cast<uint8_t*>(o) + chProp.Offset, &chArr, sizeof(chArr)) && chArr.Data) {
            for (int i = 0; i < chArr.ArrayNum && i < 100; ++i) {
                UObject* child = nullptr;
                SafeMem_Read(reinterpret_cast<uint8_t*>(chArr.Data) + (i * sizeof(void*)), &child, sizeof(void*));
                if (child) {
                    PushUObject(L, child);
                    lua_rawseti(L, -2, i + 1);
                }
            }
        }
    }
    return 1;
}

static int UObj_GetActorBounds(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    bool bOnlyColliding = lua_toboolean(L, 2) != 0;
    if (!o) {
        PushVector(L, 0, 0, 0);
        PushVector(L, 0, 0, 0);
        return 2;
    }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    void* fn = FindFunctionOnClass(cls, "GetActorBounds");
    if (fn) {
        if (g_State.UEMajor >= 5) {
            struct Params5 {
                bool bOnlyCollidingComponents;
                struct { double X, Y, Z; } Origin;
                struct { double X, Y, Z; } BoxExtent;
            } p5 = { bOnlyColliding, {}, {} };
            if (EngineDiscovery::CallProcessEvent(g_State, o, fn, &p5)) {
                PushVector(L, static_cast<float>(p5.Origin.X), static_cast<float>(p5.Origin.Y), static_cast<float>(p5.Origin.Z));
                PushVector(L, static_cast<float>(p5.BoxExtent.X), static_cast<float>(p5.BoxExtent.Y), static_cast<float>(p5.BoxExtent.Z));
                return 2;
            }
        } else {
            struct Params4 {
                bool bOnlyCollidingComponents;
                Vector3D Origin;
                Vector3D BoxExtent;
            } p4 = { bOnlyColliding, {}, {} };
            if (EngineDiscovery::CallProcessEvent(g_State, o, fn, &p4)) {
                PushVector(L, p4.Origin.X, p4.Origin.Y, p4.Origin.Z);
                PushVector(L, p4.BoxExtent.X, p4.BoxExtent.Y, p4.BoxExtent.Z);
                return 2;
            }
        }
    }
    Vector3D loc;
    Helper_GetActorLocation(o, loc);
    PushVector(L, loc.X, loc.Y, loc.Z);
    PushVector(L, 50.0f, 50.0f, 50.0f);
    return 2;
}

static UStruct* FieldOwnerOf(UObject* o) {
    if (!o) return nullptr;
    if (CompatIsStructFamilyObject(o)) return static_cast<UStruct*>(o);
    return static_cast<UStruct*>(EngineDiscovery::GetObjectClass(g_State, o));
}

static int UObj_GetProperties(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_newtable(L); return 1; }
    UStruct* cur = FieldOwnerOf(o);
    lua_newtable(L);
    if (!cur) return 1;

    while (cur) {
        if (g_State.UStruct_ChildPropsOff >= 0) {
            void* child = EngineDiscovery::GetChildPropsHead(g_State, cur);
            while (child) {
                std::string name = EngineDiscovery::GetFFieldName(g_State, child);
                std::string typeName = EngineDiscovery::GetFFieldTypeName(g_State, child);
                if (!name.empty()) {
                    lua_pushstring(L, typeName.c_str());
                    lua_setfield(L, -2, name.c_str());
                }
                child = EngineDiscovery::GetFFieldNext(g_State, child);
            }
        }
        cur = EngineDiscovery::GetSuperStruct(g_State, cur);
    }
    return 1;
}

static int UObj_FindProperty(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_pushnil(L); return 1; }
    const char* search = luaL_checkstring(L, 2);
    std::string term = search;
    std::transform(term.begin(), term.end(), term.begin(), ::tolower);

    UStruct* cur = FieldOwnerOf(o);
    if (!cur) { lua_pushnil(L); return 1; }

    while (cur) {
        if (g_State.UStruct_ChildPropsOff >= 0) {
            void* child = EngineDiscovery::GetChildPropsHead(g_State, cur);
            while (child) {
                std::string name = EngineDiscovery::GetFFieldName(g_State, child);
                std::string nameLower = name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                if (nameLower.find(term) != std::string::npos) {
                    lua_pushstring(L, name.c_str());
                    return 1;
                }
                child = EngineDiscovery::GetFFieldNext(g_State, child);
            }
        }
        cur = EngineDiscovery::GetSuperStruct(g_State, cur);
    }
    lua_pushnil(L);
    return 1;
}

static int UObj_Call(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { return luaL_error(L, "Call on nil UObject"); }
    const char* funcName = luaL_checkstring(L, 2);
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    if (!cls) { return luaL_error(L, "UObject has no class"); }
    void* ufunc = FindFunctionOnClass(cls, funcName);
    if (!ufunc) {
        return luaL_error(L, "UFunction '%s' not found on class '%s'", funcName, EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cls)).c_str());
    }
    return CallUFunctionDynamic(L, o, ufunc, 3);
}

static int UObj_GetComponents(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_newtable(L); return 1; }

    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    if (!cls) { lua_newtable(L); return 1; }

    void* fn = FindFunctionOnClass(cls, "K2_GetComponentsByClass");
    UClass* compCls = ConsoleEnabler::FindClassByNameExact(g_State, "ActorComponent");
    if (!compCls) compCls = ConsoleEnabler::FindClassByNameExact(g_State, "SceneComponent");

    if (fn && compCls) {
        struct Params {
            UClass* ComponentClass;
            void* ArrayData;
            int32_t ArrayCount;
            int32_t ArrayMax;
        } params = { compCls, nullptr, 0, 0 };

        if (EngineDiscovery::CallProcessEvent(g_State, o, fn, &params)) {
            lua_newtable(L);
            if (params.ArrayData && params.ArrayCount > 0 && params.ArrayCount < 1000) {
                UObject** compPtrs = reinterpret_cast<UObject**>(params.ArrayData);
                for (int i = 0; i < params.ArrayCount; ++i) {
                    if (compPtrs[i] && SafeMem_IsReadable(compPtrs[i], 32)) {
                        PushUObject(L, compPtrs[i]);
                        lua_rawseti(L, -2, i + 1);
                    }
                }
                return 1;
            }
        }
    }

    lua_newtable(L);
    int tblIdx = 1;
    FoundProperty rcProp = FindPropertyOnClass(cls, "RootComponent");
    if (rcProp.Found && rcProp.Offset >= 0) {
        UObject* rc = nullptr;
        SafeMem_Read(reinterpret_cast<uint8_t*>(o) + rcProp.Offset, &rc, sizeof(void*));
        if (rc && SafeMem_IsReadable(rc, 32)) {
            PushUObject(L, rc);
            lua_rawseti(L, -2, tblIdx++);
        }
    }
    return 1;
}

static int UObj_GetComponent(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_pushnil(L); return 1; }
    const char* search = luaL_checkstring(L, 2);
    std::string term = search;
    std::transform(term.begin(), term.end(), term.begin(), ::tolower);

    lua_pushcfunction(L, UObj_GetComponents);
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);

    if (lua_istable(L, -1)) {
        int len = static_cast<int>(lua_rawlen(L, -1));
        for (int i = 1; i <= len; ++i) {
            lua_rawgeti(L, -1, i);
            UObject* comp = GetUObject(L, -1);
            if (comp) {
                std::string compName = EngineDiscovery::GetObjectNameStr(g_State, comp);
                std::string compNameLower = compName;
                std::transform(compNameLower.begin(), compNameLower.end(), compNameLower.begin(), ::tolower);

                UClass* cCls = EngineDiscovery::GetObjectClass(g_State, comp);
                std::string cClsName = cCls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cCls)) : "";
                std::string cClsLower = cClsName;
                std::transform(cClsLower.begin(), cClsLower.end(), cClsLower.begin(), ::tolower);

                if (compNameLower.find(term) != std::string::npos || cClsLower.find(term) != std::string::npos) {
                    lua_remove(L, -2);
                    return 1;
                }
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    lua_pushnil(L);
    return 1;
}

static UClass* ResolveClassArg(lua_State* L, int index) {
    if (lua_isstring(L, index)) {
        const char* name = lua_tostring(L, index);
        if (!name || !*name) return nullptr;
        UClass* cls = EngineDiscovery::FindClassCached(g_State, name);
        if (!cls) {
            std::string withC = std::string(name) + "_C";
            cls = EngineDiscovery::FindClassCached(g_State, withC);
        }
        if (!cls && (name[0] == 'A' || name[0] == 'U')) {
            cls = EngineDiscovery::FindClassCached(g_State, name + 1);
        }
        if (!cls) {
            cls = ConsoleEnabler::FindClassByNameExact(g_State, name);
        }
        return cls;
    }
    UObject* obj = GetUObject(L, index);
    return static_cast<UClass*>(obj);
}

static bool IsAnyClassObject(UObject* object) {
    if (!object || !g_State.UClassClass) return false;
    UClass* meta = EngineDiscovery::GetObjectClass(g_State, object);
    if (!meta) return false;
    if (meta == g_State.UClassClass) return true;
    return EngineDiscovery::IsStructA(g_State, static_cast<UStruct*>(meta),
                                      static_cast<UStruct*>(g_State.UClassClass));
}

static bool IsWorldObject(UObject* object) {
    if (!object || !g_State.UWorldClass) return false;
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, object);
    if (!cls) return false;
    if (cls == g_State.UWorldClass) return true;
    return EngineDiscovery::IsStructA(g_State, static_cast<UStruct*>(cls),
                                      static_cast<UStruct*>(g_State.UWorldClass));
}

static UObject* LiveWorld() {
    if (g_State.GWorldPtr) {
        if (void* live = SafeMem_ReadPtr(g_State.GWorldPtr)) {
            if (IsWorldObject(static_cast<UObject*>(live))) return static_cast<UObject*>(live);
        }
    }
    return static_cast<UObject*>(g_State.GWorld);
}

template <typename T>
static void WriteThreeComponents(uint8_t* target, int32_t elementSize, T first, T second,
                                 T third) {
    if (!target) return;
    if (elementSize >= static_cast<int32_t>(3 * sizeof(double))) {
        const double values[3] = { static_cast<double>(first), static_cast<double>(second),
                                   static_cast<double>(third) };
        SafeMem_Write(target, values, sizeof(values));
        return;
    }
    const float values[3] = { static_cast<float>(first), static_cast<float>(second),
                              static_cast<float>(third) };
    SafeMem_Write(target, values, sizeof(values));
}

UObject* SpawnActorThroughEngineApi(UObject* world, UClass* actorClass,
                                          const Vector3D& loc, const Rotator3D& rot,
                                          std::string* why) {
    if (!world) {
        if (why) *why = "there is no world to spawn into";
        return nullptr;
    }
    if (!actorClass) {
        if (why) *why = "the class to spawn was not resolved";
        return nullptr;
    }

    EngineDiscovery::ReflectedMember transformParam;
    if (!EngineDiscovery::GetFunctionParameter(g_State, "GameplayStatics",
                                              "BeginDeferredActorSpawnFromClass",
                                              "SpawnTransform", transformParam) ||
        transformParam.ElementSize <= 0) {
        if (why) *why = "BeginDeferredActorSpawnFromClass has no transform parameter";
        return nullptr;
    }

    EngineDiscovery::ReflectedMember locationParam, rotationParam, scaleParam;
    if (!EngineDiscovery::GetFunctionParameter(g_State, "KismetMathLibrary", "MakeTransform",
                                              "Location", locationParam) ||
        !EngineDiscovery::GetFunctionParameter(g_State, "KismetMathLibrary", "MakeTransform",
                                              "Rotation", rotationParam) ||
        !EngineDiscovery::GetFunctionParameter(g_State, "KismetMathLibrary", "MakeTransform",
                                              "Scale", scaleParam)) {
        if (why) *why = "KismetMathLibrary::MakeTransform is not reflected on this build";
        return nullptr;
    }

    std::vector<uint8_t> location(static_cast<size_t>(locationParam.ElementSize), 0);
    std::vector<uint8_t> rotation(static_cast<size_t>(rotationParam.ElementSize), 0);
    std::vector<uint8_t> scale(static_cast<size_t>(scaleParam.ElementSize), 0);
    WriteThreeComponents(location.data(), locationParam.ElementSize, loc.X, loc.Y, loc.Z);
    WriteThreeComponents(rotation.data(), rotationParam.ElementSize, rot.Pitch, rot.Yaw, rot.Roll);
    WriteThreeComponents(scale.data(), scaleParam.ElementSize, 1.0f, 1.0f, 1.0f);

    const EngineDiscovery::ReflectedArgument makerArgs[] = {
        { "Location", location.data(), locationParam.ElementSize },
        { "Rotation", rotation.data(), rotationParam.ElementSize },
        { "Scale",    scale.data(),    scaleParam.ElementSize },
    };

    std::vector<uint8_t> transform(static_cast<size_t>(transformParam.ElementSize), 0);
    if (!EngineDiscovery::CallStaticFunctionByName(g_State, "KismetMathLibrary", "MakeTransform",
                                                  makerArgs, 3, transform.data(),
                                                  transformParam.ElementSize)) {
        if (why) *why = "the engine's own transform maker refused the argument frame";
        return nullptr;
    }

    const EngineDiscovery::ReflectedArgument beginArgs[] = {
        { "WorldContextObject", &world,      static_cast<int32_t>(sizeof(void*)) },
        { "ActorClass",         &actorClass, static_cast<int32_t>(sizeof(void*)) },
        { "SpawnTransform",     transform.data(), transformParam.ElementSize },
    };

    UObject* actor = nullptr;
    if (!EngineDiscovery::CallStaticFunctionByName(
            g_State, "GameplayStatics", "BeginDeferredActorSpawnFromClass", beginArgs, 3,
            &actor, sizeof(void*)) ||
        !actor) {
        if (why) *why = "the engine's deferred spawn answered no actor";
        return nullptr;
    }

    const EngineDiscovery::ReflectedArgument finishArgs[] = {
        { "Actor",          &actor,              static_cast<int32_t>(sizeof(void*)) },
        { "SpawnTransform", transform.data(),    transformParam.ElementSize },
    };

    UObject* finished = nullptr;
    if (!EngineDiscovery::CallStaticFunctionByName(g_State, "GameplayStatics",
                                                  "FinishSpawningActor", finishArgs, 2,
                                                  &finished, sizeof(void*)) ||
        !finished) {
        return actor;
    }
    return finished;
}

static int Lua_SpawnActor(lua_State* L) {

    int classArg = 1;
    UObject* callerWorld = nullptr;

    if (!lua_isstring(L, 1)) {
        UObject* first = GetUObject(L, 1);
        if (!IsAnyClassObject(first) && IsWorldObject(first)) {
            callerWorld = first;
            classArg = 2;
        }
    }

    UClass* actorClass = ResolveClassArg(L, classArg);
    if (!actorClass || !IsAnyClassObject(static_cast<UObject*>(actorClass))) {
        lua_pushnil(L);
        return 1;
    }

    Vector3D loc = { 0, 0, 0 };
    Rotator3D rot = { 0, 0, 0 };
    if (lua_gettop(L) >= classArg + 1) GetVector(L, classArg + 1, loc);
    if (lua_gettop(L) >= classArg + 2) GetRotator(L, classArg + 2, rot);

    UObject* world = callerWorld ? callerWorld : LiveWorld();

    std::string why;
    UObject* spawnedActor = SpawnActorThroughEngineApi(world, actorClass, loc, rot, &why);
    if (!spawnedActor && g_State.StaticConstructObject) {

        spawnedActor = ConsoleEnabler::TrySEHConstruct(g_State, actorClass, world);
    }

    if (spawnedActor) {
        PushUObject(L, spawnedActor);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int Lua_StaticConstructObject(lua_State* L) {
    UClass* cls = ResolveClassArg(L, 1);
    if (!cls) {
        lua_pushnil(L);
        return 1;
    }

    bool isActor = false;
    UStruct* cur = static_cast<UStruct*>(cls);
    for (int d = 0; d < 16 && cur; ++d) {
        std::string n = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cur));
        if (_stricmp(n.c_str(), "Actor") == 0 || _stricmp(n.c_str(), "AActor") == 0) { isActor = true; break; }
        cur = EngineDiscovery::GetSuperStruct(g_State, cur);
    }

    if (isActor) {
        return Lua_SpawnActor(L);
    }

    UObject* outer = (lua_gettop(L) >= 2) ? GetUObject(L, 2) : nullptr;
    if (!outer) {
        outer = static_cast<UObject*>(g_State.GWorld ? g_State.GWorld : (g_State.GWorldPtr ? *g_State.GWorldPtr : nullptr));
    }

    UObject* createdObj = nullptr;

    if (outer) {
        createdObj = EngineDiscovery::SpawnObjectThroughEngine(g_State, cls, outer);
    }

    if (!createdObj && g_State.StaticConstructObject) {
        createdObj = ConsoleEnabler::TrySEHConstruct(g_State, cls, outer);
    }

    if (createdObj) {
        PushUObject(L, createdObj);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int Lua_StaticClass(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    UClass* cls = ResolveClassArg(L, 1);
    if (cls) {
        PushUObject(L, static_cast<UObject*>(cls));
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int Lua_StaticStruct(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    if (!name || !*name) {
        lua_pushnil(L);
        return 1;
    }

    if (g_State.bIndexBuilt) {
        std::vector<UObject*> matches = EngineDiscovery::FindObjectsByNameFast(g_State, name);
        for (UObject* obj : matches) {
            if (!obj) continue;
            UClass* c = EngineDiscovery::GetObjectClass(g_State, obj);
            std::string cName = c ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(c)) : "";
            if (cName.find("ScriptStruct") != std::string::npos || cName.find("Struct") != std::string::npos) {
                PushUObject(L, obj);
                return 1;
            }
        }
        lua_pushnil(L);
        return 1;
    }

    if (g_State.GUObjectArray) {
        int count = EngineDiscovery::GetObjectCount(g_State);
        SafeMem_SetFastMode(1);
        for (int i = 0; i < count; ++i) {
            void* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
            if (!obj) continue;
            std::string n = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(obj));
            if (_stricmp(n.c_str(), name) == 0) {
                UClass* c = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj));
                std::string cName = c ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(c)) : "";
                if (cName.find("ScriptStruct") != std::string::npos || cName.find("Struct") != std::string::npos) {
                    SafeMem_SetFastMode(0);
                    PushUObject(L, static_cast<UObject*>(obj));
                    return 1;
                }
            }
        }
        SafeMem_SetFastMode(0);
    }
    lua_pushnil(L);
    return 1;
}

static int Lua_StaticEnum(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    if (!name || !*name) {
        lua_pushnil(L);
        return 1;
    }

    if (g_State.bIndexBuilt) {
        std::vector<UObject*> matches = EngineDiscovery::FindObjectsByNameFast(g_State, name);
        for (UObject* obj : matches) {
            if (!obj) continue;
            UClass* c = EngineDiscovery::GetObjectClass(g_State, obj);
            std::string cName = c ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(c)) : "";
            if (cName.find("Enum") != std::string::npos) {
                PushUObject(L, obj);
                return 1;
            }
        }
        lua_pushnil(L);
        return 1;
    }

    if (g_State.GUObjectArray) {
        int count = EngineDiscovery::GetObjectCount(g_State);
        SafeMem_SetFastMode(1);
        for (int i = 0; i < count; ++i) {
            void* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
            if (!obj) continue;
            std::string n = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(obj));
            if (_stricmp(n.c_str(), name) == 0) {
                UClass* c = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj));
                std::string cName = c ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(c)) : "";
                if (cName.find("Enum") != std::string::npos) {
                    SafeMem_SetFastMode(0);
                    PushUObject(L, static_cast<UObject*>(obj));
                    return 1;
                }
            }
        }
        SafeMem_SetFastMode(0);
    }
    lua_pushnil(L);
    return 1;
}

static int32_t DetectRowMapStride(uint8_t* arrayData, int32_t arrayNum) {

    return 24;
}

static UStruct* FindRowStruct(UObject* dt) {
    if (!dt) return nullptr;
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, dt);
    if (cls) {
        FoundProperty prop = FindPropertyOnClass(cls, "RowStruct");
        if (prop.Found && prop.Offset > 0) {
            UStruct* s = nullptr;
            if (SafeMem_Read(reinterpret_cast<uint8_t*>(dt) + prop.Offset, &s, sizeof(void*)) && s && SafeMem_IsReadable(s, 64)) {
                return s;
            }
        }
    }
    static const int32_t candidateOffsets[] = { 0x28, 0x30, 0x38, 0x40, 0x48 };
    for (int32_t off : candidateOffsets) {
        UStruct* s = nullptr;
        if (SafeMem_Read(reinterpret_cast<uint8_t*>(dt) + off, &s, sizeof(void*)) && s && SafeMem_IsReadable(s, 64)) {
            UClass* sCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(s));
            if (sCls) {
                std::string cName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(sCls));
                if (cName.find("ScriptStruct") != std::string::npos || cName.find("Struct") != std::string::npos ||
                    sCls == g_State.UClassClass || (g_State.UStructClass && EngineDiscovery::IsStructA(g_State, static_cast<UStruct*>(sCls), static_cast<UStruct*>(g_State.UStructClass)))) {
                    return s;
                }
            }
        }
    }
    return nullptr;
}

static int32_t FindRowMapOffset(UObject* dt) {
    if (!dt) return -1;
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, dt);
    if (cls) {
        FoundProperty prop = FindPropertyOnClass(cls, "RowStruct");
        if (prop.Found && prop.Offset > 0) {

            int32_t off = prop.Offset + sizeof(void*);
            TArray<uint8_t> mapData = {};
            if (SafeMem_Read(reinterpret_cast<uint8_t*>(dt) + off, &mapData, sizeof(mapData)) && mapData.ArrayNum >= 0 && mapData.ArrayNum <= 50000) {
                return off;
            }
        }
    }

    UStruct* rs = FindRowStruct(dt);
    if (rs) {
        for (int32_t off : { 0x28, 0x30, 0x38, 0x40 }) {
            void* ptr = nullptr;
            if (SafeMem_Read(reinterpret_cast<uint8_t*>(dt) + off, &ptr, sizeof(void*)) && ptr == rs) {
                int32_t mapOff = off + sizeof(void*);
                TArray<uint8_t> mapData = {};
                if (SafeMem_Read(reinterpret_cast<uint8_t*>(dt) + mapOff, &mapData, sizeof(mapData)) && mapData.ArrayNum >= 0 && mapData.ArrayNum <= 50000) {
                    return mapOff;
                }
            }
        }
    }

    static const int32_t candidateOffsets[] = { 0x30, 0x38, 0x40, 0x48, 0x50 };
    for (int32_t off : candidateOffsets) {
        TArray<uint8_t> mapData = {};
        if (SafeMem_Read(reinterpret_cast<uint8_t*>(dt) + off, &mapData, sizeof(mapData))) {
            if (mapData.Data && mapData.ArrayNum >= 0 && mapData.ArrayNum <= 50000 && mapData.ArrayMax >= mapData.ArrayNum) {
                if (mapData.ArrayNum > 0) {
                    uint8_t* rowBuf = nullptr;
                    if (SafeMem_Read(reinterpret_cast<uint8_t*>(mapData.Data) + sizeof(FName), &rowBuf, sizeof(void*)) && rowBuf && SafeMem_IsReadable(rowBuf, 8)) {
                        return off;
                    }
                } else {
                    return off;
                }
            }
        }
    }
    return (g_State.UEMajor >= 5 ? 0x38 : 0x30);
}

static int DataTable_GetRowNames(lua_State* L) {
    UObject* dt = GetUObject(L, 1);
    if (!dt) { lua_newtable(L); return 1; }

    int32_t rowMapOff = FindRowMapOffset(dt);
    if (rowMapOff < 0) { lua_newtable(L); return 1; }

    uint8_t* rowMapAddr = reinterpret_cast<uint8_t*>(dt) + rowMapOff;
    TArray<uint8_t> mapData = {};
    if (!SafeMem_Read(rowMapAddr, &mapData, sizeof(mapData)) || !mapData.Data || mapData.ArrayNum <= 0) {
        lua_newtable(L);
        return 1;
    }

    lua_newtable(L);
    int luaIdx = 1;
    const int32_t stride = DetectRowMapStride(reinterpret_cast<uint8_t*>(mapData.Data), mapData.ArrayNum);

    for (int32_t i = 0; i < mapData.ArrayNum && i < 10000; ++i) {
        uint8_t* pairAddr = reinterpret_cast<uint8_t*>(mapData.Data) + (i * stride);
        if (!SafeMem_IsReadable(pairAddr, sizeof(FName) + sizeof(void*))) continue;

        uint8_t* rowBuf = nullptr;
        SafeMem_Read(pairAddr + sizeof(FName), &rowBuf, sizeof(void*));
        if (!rowBuf || !SafeMem_IsReadable(rowBuf, 4)) continue;

        FName rowName = {};
        SafeMem_Read(pairAddr, &rowName, sizeof(FName));
        std::string nameStr = EngineDiscovery::FNameToStr(g_State, rowName);
        if (!nameStr.empty() && nameStr != "None") {
            lua_pushstring(L, nameStr.c_str());
            lua_rawseti(L, -2, luaIdx++);
        }
    }
    return 1;
}

void CompatPushRowTable(lua_State* L, void* rowStruct, uint8_t* rowBuf);

static int DataTable_FindRow(lua_State* L) {
    UObject* dt = GetUObject(L, 1);
    const char* targetRowName = luaL_checkstring(L, 2);
    if (!dt) { lua_pushnil(L); return 1; }

    int32_t rowMapOff = FindRowMapOffset(dt);
    if (rowMapOff < 0) { lua_pushnil(L); return 1; }

    UStruct* rowStruct = FindRowStruct(dt);

    uint8_t* rowMapAddr = reinterpret_cast<uint8_t*>(dt) + rowMapOff;
    TArray<uint8_t> mapData = {};
    if (!SafeMem_Read(rowMapAddr, &mapData, sizeof(mapData)) || !mapData.Data || mapData.ArrayNum <= 0) {
        lua_pushnil(L);
        return 1;
    }

    const int32_t stride = DetectRowMapStride(reinterpret_cast<uint8_t*>(mapData.Data), mapData.ArrayNum);
    const int32_t targetNameIdx = EngineDiscovery::ResolveFNameIndex(g_State, targetRowName);

    for (int32_t i = 0; i < mapData.ArrayNum && i < 10000; ++i) {
        uint8_t* pairAddr = reinterpret_cast<uint8_t*>(mapData.Data) + (i * stride);
        if (!SafeMem_IsReadable(pairAddr, sizeof(FName) + sizeof(void*))) continue;

        uint8_t* rowBuf = nullptr;
        SafeMem_Read(pairAddr + sizeof(FName), &rowBuf, sizeof(void*));
        if (!rowBuf || !SafeMem_IsReadable(rowBuf, 4)) continue;

        FName rowName = {};
        SafeMem_Read(pairAddr, &rowName, sizeof(FName));

        bool nameMatches = (targetNameIdx >= 0 && rowName.ComparisonIndex == static_cast<uint32_t>(targetNameIdx));
        if (!nameMatches) {
            std::string nameStr = EngineDiscovery::FNameToStr(g_State, rowName);
            if (_stricmp(nameStr.c_str(), targetRowName) == 0) {
                nameMatches = true;
            }
        }

        if (nameMatches) {
            CompatPushRowTable(L, rowStruct, rowBuf);
            return 1;
        }
    }

    lua_pushnil(L);
    return 1;
}

static int DataTable_ForEachRow(lua_State* L) {
    UObject* dt = GetUObject(L, 1);
    if (!lua_isfunction(L, 2) || !dt) return 0;

    int32_t rowMapOff = FindRowMapOffset(dt);
    if (rowMapOff < 0) return 0;

    UStruct* rowStruct = FindRowStruct(dt);

    uint8_t* rowMapAddr = reinterpret_cast<uint8_t*>(dt) + rowMapOff;
    TArray<uint8_t> mapData = {};
    if (!SafeMem_Read(rowMapAddr, &mapData, sizeof(mapData)) || !mapData.Data) return 0;

    int32_t stride = DetectRowMapStride(reinterpret_cast<uint8_t*>(mapData.Data), mapData.ArrayNum);

    for (int32_t i = 0; i < mapData.ArrayNum && i < 10000; ++i) {
        uint8_t* pairAddr = reinterpret_cast<uint8_t*>(mapData.Data) + (i * stride);
        if (!SafeMem_IsReadable(pairAddr, sizeof(FName) + sizeof(void*))) continue;

        FName rowName = {};
        SafeMem_Read(pairAddr, &rowName, sizeof(FName));
        std::string nameStr = EngineDiscovery::FNameToStr(g_State, rowName);
        if (nameStr.empty() || nameStr == "None") continue;

        uint8_t* rowBuf = nullptr;
        SafeMem_Read(pairAddr + sizeof(FName), &rowBuf, sizeof(void*));
        if (!rowBuf) continue;

        lua_pushvalue(L, 2);
        lua_pushstring(L, nameStr.c_str());

        lua_newtable(L);
        if (rowStruct && g_State.UStruct_ChildPropsOff >= 0) {
            void* child = EngineDiscovery::GetChildPropsHead(g_State, rowStruct);
            while (child) {
                std::string fName = EngineDiscovery::GetFFieldName(g_State, child);
                std::string fType = EngineDiscovery::GetFFieldTypeName(g_State, child);
                int32_t off = 0;
                if (g_State.FProperty_OffsetOff >= 0)
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &off, 4);

                FoundProperty fp = MakeFoundProperty(child, fType, off, 8);
                lua_pushstring(L, fName.c_str());
                PushPropertyValue(L, rowBuf, fp);
                lua_settable(L, -3);

                child = EngineDiscovery::GetFFieldNext(g_State, child);
            }
        }

        if (lua_pcall(L, 2, 1, 0) == LUA_OK) {
            bool stop = lua_isboolean(L, -1) && !lua_toboolean(L, -1);
            lua_pop(L, 1);
            if (stop) break;
        } else {
            lua_pop(L, 1);
        }
    }
    return 0;
}

static int DataTable_ExportAsJSON(lua_State* L) {
    UObject* dt = GetUObject(L, 1);
    bool pretty = (lua_gettop(L) >= 2) ? (lua_toboolean(L, 2) != 0) : false;
    if (!dt) { lua_pushstring(L, "{}"); return 1; }

    int32_t rowMapOff = FindRowMapOffset(dt);
    if (rowMapOff < 0) { lua_pushstring(L, "{}"); return 1; }

    UStruct* rowStruct = FindRowStruct(dt);

    uint8_t* rowMapAddr = reinterpret_cast<uint8_t*>(dt) + rowMapOff;
    TArray<uint8_t> mapData = {};
    if (!SafeMem_Read(rowMapAddr, &mapData, sizeof(mapData)) || !mapData.Data) {
        lua_pushstring(L, "{}");
        return 1;
    }

    int32_t stride = DetectRowMapStride(reinterpret_cast<uint8_t*>(mapData.Data), mapData.ArrayNum);

    lua_newtable(L);
    for (int32_t i = 0; i < mapData.ArrayNum && i < 10000; ++i) {
        uint8_t* pairAddr = reinterpret_cast<uint8_t*>(mapData.Data) + (i * stride);
        if (!SafeMem_IsReadable(pairAddr, sizeof(FName) + sizeof(void*))) continue;

        FName rowName = {};
        SafeMem_Read(pairAddr, &rowName, sizeof(FName));
        std::string nameStr = EngineDiscovery::FNameToStr(g_State, rowName);
        if (nameStr.empty() || nameStr == "None") continue;

        uint8_t* rowBuf = nullptr;
        SafeMem_Read(pairAddr + sizeof(FName), &rowBuf, sizeof(void*));
        if (!rowBuf) continue;

        lua_pushstring(L, nameStr.c_str());
        CompatPushRowTable(L, rowStruct, rowBuf);
        lua_settable(L, -3);
    }

    lua_getglobal(L, "JSON");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_pushstring(L, "{}");
        return 1;
    }
    lua_getfield(L, -1, "Stringify");
    lua_remove(L, -2);
    lua_pushvalue(L, -2);
    lua_pushboolean(L, pretty ? 1 : 0);
    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        lua_pop(L, 1);
        lua_pushstring(L, "{}");
        return 1;
    }
    lua_remove(L, -2);
    return 1;
}

int32_t CompatRowMapOffset(UObject* dataTable) {
    return FindRowMapOffset(dataTable);
}

void* CompatRowStruct(UObject* dataTable) {
    return FindRowStruct(dataTable);
}

int32_t CompatRowMapStride(uint8_t* rowMapData, int32_t count) {
    return DetectRowMapStride(rowMapData, count);
}

void CompatPushRowTable(lua_State* L, void* rowStruct, uint8_t* rowBuf) {
    lua_newtable(L);
    if (!rowStruct || !rowBuf) return;

    for (void* cur = rowStruct; cur; cur = EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(cur))) {
        if (g_State.bUseFProperty && g_State.UStruct_ChildPropsOff >= 0) {
            void* child = EngineDiscovery::GetChildPropsHead(g_State, static_cast<UStruct*>(cur));
            int guard = 0;
            while (child && ++guard < 8192) {
                std::string fieldName = EngineDiscovery::GetFFieldName(g_State, child);
                std::string fieldType = EngineDiscovery::GetFFieldTypeName(g_State, child);
                int32_t off = 0;
                if (g_State.FProperty_OffsetOff >= 0) {
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &off, 4);
                }
                FoundProperty fp = MakeFoundProperty(child, fieldType, off, 8);
                lua_pushstring(L, fieldName.c_str());
                PushPropertyValue(L, rowBuf, fp);
                lua_settable(L, -3);

                size_t firstUnderscore = fieldName.find('_');
                if (firstUnderscore != std::string::npos && firstUnderscore > 0) {
                    std::string baseName = fieldName.substr(0, firstUnderscore);
                    lua_pushstring(L, baseName.c_str());
                    PushPropertyValue(L, rowBuf, fp);
                    lua_settable(L, -3);
                }

                child = EngineDiscovery::GetFFieldNext(g_State, child);
            }
        }
        if (g_State.UStruct_ChildrenOff >= 0) {
            void* child = nullptr;
            SafeMem_Read(reinterpret_cast<uint8_t*>(cur) + g_State.UStruct_ChildrenOff, &child, sizeof(void*));
            int guard = 0;
            while (child && ++guard < 8192) {
                UClass* childCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(child));
                std::string fieldType = childCls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(childCls)) : "";
                if (fieldType.find("Property") != std::string::npos) {
                    std::string fieldName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(child));
                    int32_t off = 0;
                    if (g_State.FProperty_OffsetOff >= 0) {
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &off, 4);
                    }
                    FoundProperty fp = MakeFoundProperty(child, fieldType, off, 8);
                    lua_pushstring(L, fieldName.c_str());
                    PushPropertyValue(L, rowBuf, fp);
                    lua_settable(L, -3);

                    size_t firstUnderscore = fieldName.find('_');
                    if (firstUnderscore != std::string::npos && firstUnderscore > 0) {
                        std::string baseName = fieldName.substr(0, firstUnderscore);
                        lua_pushstring(L, baseName.c_str());
                        PushPropertyValue(L, rowBuf, fp);
                        lua_settable(L, -3);
                    }
                }
                void* next = nullptr;
                int32_t nextOff = CompatVerifiedFieldNextOffset();
                if (nextOff >= 0) SafeMem_Read(reinterpret_cast<uint8_t*>(child) + nextOff, &next, sizeof(void*));
                child = next;
            }
        }
    }
}

static UObject* FindLoadedObjectByFullName(const std::string& query) {
    if (query.empty()) return nullptr;

    const int count = EngineDiscovery::GetObjectCount(g_State);
    for (int i = 0; i < count; ++i) {
        void* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
        if (!obj) continue;
        std::string fn = EngineDiscovery::BuildFullName(g_State, static_cast<UObject*>(obj));
        if (_stricmp(fn.c_str(), query.c_str()) == 0) {
            return static_cast<UObject*>(obj);
        }
        if (fn.size() >= query.size() &&
            _stricmp(fn.c_str() + (fn.size() - query.size()), query.c_str()) == 0) {
            return static_cast<UObject*>(obj);
        }
    }
    return nullptr;
}

static int Lua_LoadAsset(lua_State* L) {
    const char* rawPath = luaL_checkstring(L, 1);
    if (!rawPath || !*rawPath || !g_State.GUObjectArray) {
        lua_pushnil(L);
        return 1;
    }

    const std::string query = EngineDiscovery::StripTypeTokenWrapper(rawPath);

    if (!query.empty() && query[0] == '/') {
        std::string why;
        if (UObject* loaded = EngineDiscovery::LoadAssetByPath(g_State, query, &why)) {
            PushUObject(L, loaded);
            return 1;
        }
    }

    if (UObject* existing = FindLoadedObjectByFullName(query)) {
        PushUObject(L, existing);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

static int Lua_GetBlueprintPipeline(lua_State* L) {
    const EngineDiscovery::AssetRegistryBinding& binding =
        EngineDiscovery::GetAssetRegistryBinding(g_State);

    lua_newtable(L);

    lua_pushboolean(L, binding.bReady ? 1 : 0);
    lua_setfield(L, -2, "assetPipelineReady");
    lua_pushstring(L, binding.Detail.c_str());
    lua_setfield(L, -2, "assetPipelineDetail");

    if (binding.bReady) {
        lua_pushstring(L, binding.AssetDataStructName.c_str());
        lua_setfield(L, -2, "assetDataStruct");
        lua_pushinteger(L, binding.AssetDataSize);
        lua_setfield(L, -2, "assetDataSize");

        lua_newtable(L);
        for (const EngineDiscovery::AssetDataField& field : binding.Fields) {
            lua_newtable(L);
            lua_pushstring(L, field.Name.c_str());
            lua_setfield(L, -2, "Name");
            lua_pushstring(L, field.TypeName.c_str());
            lua_setfield(L, -2, "Type");
            lua_pushinteger(L, field.Offset);
            lua_setfield(L, -2, "Offset");
            lua_pushinteger(L, field.Size);
            lua_setfield(L, -2, "Size");
            lua_rawseti(L, -2, static_cast<lua_Integer>(&field - binding.Fields.data()) + 1);
        }
        lua_setfield(L, -2, "assetDataFields");

        const struct { const char* Name; void* Function; } functions[] = {
            { "GetAsset", binding.FnGetAsset },
            { "GetAssetRegistry", binding.FnGetAssetRegistry },
            { "GetClass", binding.FnGetClass },
            { "IsValid", binding.FnIsValid },
            { "CreateAssetData", binding.FnCreateAssetData },
        };
        lua_newtable(L);
        for (const auto& fn : functions) {
            lua_pushboolean(L, fn.Function ? 1 : 0);
            lua_setfield(L, -2, fn.Name);
        }
        lua_setfield(L, -2, "assetRegistryFunctions");

        if (binding.Registry) {
            PushUObject(L, binding.Registry);
        } else {
            lua_pushnil(L);
        }
        lua_setfield(L, -2, "assetRegistry");

        lua_pushinteger(L, binding.AssetDataOffset);
        lua_setfield(L, -2, "assetDataOffset");
        lua_pushinteger(L, binding.GetAssetFrameSize);
        lua_setfield(L, -2, "getAssetFrameSize");
        lua_pushinteger(L, binding.OffObjectPath);
        lua_setfield(L, -2, "objectPathOffset");
        lua_pushinteger(L, binding.OffPackageName);
        lua_setfield(L, -2, "packageNameOffset");
        lua_pushinteger(L, binding.OffAssetName);
        lua_setfield(L, -2, "assetNameOffset");
    }

    lua_pushboolean(L, EngineDiscovery::CanCreateFNames(g_State) ? 1 : 0);
    lua_setfield(L, -2, "canCreateNames");
    lua_pushinteger(L, g_State.FName_Size > 0 ? g_State.FName_Size : 8);
    lua_setfield(L, -2, "nameWidth");
    lua_pushinteger(L, g_State.FName_NumberOff);
    lua_setfield(L, -2, "nameNumberOffset");

    lua_newtable(L);
    lua_pushinteger(L, g_State.UObject_OuterOff);
    lua_setfield(L, -2, "objectOuter");
    lua_pushinteger(L, g_State.UField_NextOff);
    lua_setfield(L, -2, "uFieldNext");
    lua_pushinteger(L, g_State.UStruct_SuperOff);
    lua_setfield(L, -2, "structSuper");
    lua_pushinteger(L, g_State.UStruct_ChildrenOff);
    lua_setfield(L, -2, "structChildren");
    lua_pushinteger(L, g_State.UStruct_ChildPropsOff);
    lua_setfield(L, -2, "childProperties");
    lua_pushinteger(L, g_State.FField_NameOff);
    lua_setfield(L, -2, "fieldName");
    lua_pushinteger(L, g_State.FField_NextOff);
    lua_setfield(L, -2, "fieldNext");
    lua_setfield(L, -2, "memberChain");

    UClass* blueprintClass = EngineDiscovery::GetBlueprintGeneratedClassObject(g_State);
    if (blueprintClass) {
        PushUObject(L, blueprintClass);
    } else {
        lua_pushnil(L);
    }
    lua_setfield(L, -2, "blueprintGeneratedClass");
    lua_pushinteger(L, static_cast<lua_Integer>(
                           EngineDiscovery::FindBlueprintClasses(g_State, "Actor").size()));
    lua_setfield(L, -2, "blueprintActorClassCount");

    return 1;
}

static int Lua_GetObjectShape(lua_State* L) {
    const char* path = lua_type(L, 1) == LUA_TSTRING ? lua_tostring(L, 1) : nullptr;

    lua_newtable(L);
    lua_pushstring(L, path ? path : "");
    lua_setfield(L, -2, "path");

    UObject* subject = path ? EngineDiscovery::ResolveObjectPath(g_State, path) : nullptr;
    lua_pushboolean(L, subject ? 1 : 0);
    lua_setfield(L, -2, "resolved");
    if (!subject) return 1;

    UClass* cls = EngineDiscovery::GetObjectClass(g_State, subject);
    lua_pushstring(L, EngineDiscovery::GetObjectNameStr(g_State, subject).c_str());
    lua_setfield(L, -2, "name");
    lua_pushstring(L, cls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cls)).c_str() : "");
    lua_setfield(L, -2, "className");
    lua_pushstring(L, EngineDiscovery::BuildFullName(g_State, subject).c_str());
    lua_setfield(L, -2, "fullName");

    lua_pushboolean(L, (g_State.UStructClass &&
                        ConsoleEnabler::IsObjectA(g_State, subject, g_State.UStructClass))
                           ? 1 : 0);
    lua_setfield(L, -2, "structFamilyByIdentity");
    lua_pushboolean(L, ConsoleEnabler::IsClassObject(g_State, subject) ? 1 : 0);
    lua_setfield(L, -2, "isClassObject");
    lua_pushboolean(L, CompatIsStructFamilyObject(subject) ? 1 : 0);
    lua_setfield(L, -2, "structFamily");
    lua_pushstring(L, g_State.UStructClass
                           ? EngineDiscovery::GetObjectNameStr(
                                 g_State, reinterpret_cast<UObject*>(g_State.UStructClass)).c_str()
                           : "");
    lua_setfield(L, -2, "structClassName");

    lua_newtable(L);
    if (cls) {
        UStruct* cur = reinterpret_cast<UStruct*>(cls);
        for (int depth = 0; cur && depth < 32; ++depth) {
            lua_pushinteger(L, depth + 1);
            lua_pushstring(L,
                           EngineDiscovery::GetObjectNameStr(g_State, reinterpret_cast<UObject*>(cur)).c_str());
            lua_settable(L, -3);
            cur = EngineDiscovery::GetSuperStruct(g_State, cur);
        }
    }
    lua_setfield(L, -2, "classSuperChain");

    UStruct* owner = FieldOwnerOf(subject);
    lua_pushboolean(L, owner == reinterpret_cast<UStruct*>(subject) ? 1 : 0);
    lua_setfield(L, -2, "ownerIsReceiver");
    lua_pushstring(L, owner ? EngineDiscovery::GetObjectNameStr(g_State, reinterpret_cast<UObject*>(owner)).c_str() : "");
    lua_setfield(L, -2, "ownerName");

    int32_t memberCount = 0;
    lua_newtable(L);
    for (UStruct* cur = owner; cur; cur = EngineDiscovery::GetSuperStruct(g_State, cur)) {
        const std::string declaringOwner =
            EngineDiscovery::GetObjectNameStr(g_State, reinterpret_cast<UObject*>(cur));
        std::vector<EngineDiscovery::ReflectedMember> level;
        if (!EngineDiscovery::GetReflectedMembers(g_State, cur, level)) continue;
        for (const EngineDiscovery::ReflectedMember& member : level) {
            if (member.Name.empty()) continue;
            lua_newtable(L);
            lua_pushstring(L, member.Name.c_str());
            lua_setfield(L, -2, "name");
            lua_pushstring(L, member.TypeName.c_str());
            lua_setfield(L, -2, "type");
            lua_pushinteger(L, member.Offset);
            lua_setfield(L, -2, "offset");
            lua_pushinteger(L, member.ElementSize);
            lua_setfield(L, -2, "size");
            lua_pushinteger(L, static_cast<lua_Integer>(member.Flags));
            lua_setfield(L, -2, "flags");
            lua_pushstring(L, declaringOwner.c_str());
            lua_setfield(L, -2, "from");
            lua_rawseti(L, -2, ++memberCount);
        }
    }
    lua_setfield(L, -2, "members");
    lua_pushinteger(L, memberCount);
    lua_setfield(L, -2, "memberCount");

    return 1;
}

static int UObj_GetAddressString(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    if (!obj) { lua_pushnil(L); return 1; }
    char buf[32];
    snprintf(buf, sizeof(buf), "0x%p", obj);
    lua_pushstring(L, buf);
    return 1;
}

static int UObj_GetFName(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    if (!obj) { lua_pushnil(L); return 1; }

    FName fn = {};
    bool read = false;
    if (g_State.UObject_NameOff >= 0) {
        if (g_State.FName_Size == 4 || g_State.FName_NumberOff < 0) {
            read = SafeMem_Read(reinterpret_cast<uint8_t*>(obj) + g_State.UObject_NameOff,
                                &fn.ComparisonIndex, 4);
            fn.Number = 0;
        } else {
            read = SafeMem_Read(reinterpret_cast<uint8_t*>(obj) + g_State.UObject_NameOff,
                                &fn, sizeof(fn));
        }
    }
    if (read) {
        LuaEngine::PushFName(L, fn.ComparisonIndex, fn.Number);
        return 1;
    }

    std::string name = EngineDiscovery::GetObjectNameStr(g_State, obj);
    int32_t idx = name.empty() ? -1 : EngineDiscovery::ResolveFNameIndex(g_State, name);
    LuaEngine::PushFName(L, idx >= 0 ? static_cast<uint32_t>(idx) : 0);
    return 1;
}

static int UObj_GetPackage(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    if (!obj) { lua_pushnil(L); return 1; }
    UObject* cur = obj;
    while (cur) {
        UObject* outer = EngineDiscovery::GetObjectOuter(g_State, cur);
        if (!outer) break;
        cur = outer;
    }
    PushUObject(L, cur);
    return 1;
}

static int UObj_GetFunctions(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    if (!obj) { lua_newtable(L); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    lua_newtable(L);
    if (!cls) return 1;

    int idx = 1;
    UStruct* cur = static_cast<UStruct*>(cls);
    while (cur) {
        if (g_State.UStruct_ChildrenOff >= 0) {
            void* child = nullptr;
            if (SafeMem_Read(reinterpret_cast<uint8_t*>(cur) + g_State.UStruct_ChildrenOff, &child, sizeof(void*))) {
                int guard = 0;
                while (child && ++guard < 8192) {
                    UClass* childCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(child));
                    std::string cName = childCls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(childCls)) : "";
                    if (cName.find("Function") != std::string::npos) {
                        std::string fnName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(child));
                        lua_pushstring(L, fnName.c_str());
                        lua_rawseti(L, -2, idx++);
                    }
                    void* next = nullptr;
                    int32_t nextOff = CompatVerifiedFieldNextOffset();
                    if (nextOff >= 0) {
                        SafeMem_Read(reinterpret_cast<uint8_t*>(child) + nextOff, &next, sizeof(void*));
                    }
                    child = next;
                }
            }
        }
        cur = EngineDiscovery::GetSuperStruct(g_State, cur);
    }
    return 1;
}

static int UObj_HasFunction(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    const char* funcName = luaL_checkstring(L, 2);
    if (!obj || !funcName) { lua_pushboolean(L, 0); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    void* ufunc = FindFunctionOnClass(cls, funcName);
    lua_pushboolean(L, ufunc != nullptr ? 1 : 0);
    return 1;
}

static int UObj_Cast(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    if (!obj) { lua_pushnil(L); return 1; }
    if (lua_isstring(L, 2)) {
        const char* className = lua_tostring(L, 2);
        UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
        while (cls) {
            std::string cName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cls));
            if (_stricmp(cName.c_str(), className) == 0) {
                PushUObject(L, obj);
                return 1;
            }
            cls = static_cast<UClass*>(EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(cls)));
        }
    } else {
        UObject* targetClassObj = GetUObject(L, 2);
        if (targetClassObj) {
            UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
            while (cls) {
                if (static_cast<UObject*>(cls) == targetClassObj) {
                    PushUObject(L, obj);
                    return 1;
                }
                cls = static_cast<UClass*>(EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(cls)));
            }
        }
    }
    lua_pushnil(L);
    return 1;
}

static int UObj_GetComponentsByClass(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    const char* className = luaL_checkstring(L, 2);
    lua_newtable(L);
    if (!obj || !className) return 1;

    int idx = 1;
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    void* getCompsFunc = FindFunctionOnClass(cls, "K2_GetComponentsByClass");
    UClass* targetCls = ConsoleEnabler::FindClassByNameExact(g_State, className);

    if (getCompsFunc && targetCls) {
        struct {
            UClass* ComponentClass;
            TArray<UObject*> OutComponents;
        } params = { targetCls, {} };
        if (EngineDiscovery::CallProcessEvent(g_State, obj, getCompsFunc, &params)) {
            if (params.OutComponents.Data && params.OutComponents.ArrayNum > 0) {
                for (int32_t i = 0; i < params.OutComponents.ArrayNum; ++i) {
                    UObject* comp = params.OutComponents.Data[i];
                    if (comp) {
                        PushUObject(L, comp);
                        lua_rawseti(L, -2, idx++);
                    }
                }
                return 1;
            }
        }
    }

    if (g_State.GUObjectArray) {
        int count = EngineDiscovery::GetObjectCount(g_State);
        for (int i = 0; i < count; ++i) {
            void* o = EngineDiscovery::GetObjectByIndex(g_State, i);
            if (!o) continue;
            UObject* u = static_cast<UObject*>(o);
            if (EngineDiscovery::GetObjectOuter(g_State, u) == obj) {
                UClass* uCls = EngineDiscovery::GetObjectClass(g_State, u);
                while (uCls) {
                    std::string uName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(uCls));
                    if (_stricmp(uName.c_str(), className) == 0) {
                        PushUObject(L, u);
                        lua_rawseti(L, -2, idx++);
                        break;
                    }
                    uCls = static_cast<UClass*>(EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(uCls)));
                }
            }
        }
    }
    return 1;
}

static int UObj_GetScale(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    if (!obj) { lua_pushnil(L); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    void* fn = FindFunctionOnClass(cls, "GetActorScale3D");
    if (!fn) fn = FindFunctionOnClass(cls, "K2_GetActorScale3D");
    if (fn) {
        if (g_State.UEMajor >= 5) {
            struct { double X, Y, Z; } ret = { 1.0, 1.0, 1.0 };
            EngineDiscovery::CallProcessEvent(g_State, obj, fn, &ret);
            PushVector(L, static_cast<float>(ret.X), static_cast<float>(ret.Y), static_cast<float>(ret.Z));
            return 1;
        } else {
            struct { float X, Y, Z; } ret = { 1.0f, 1.0f, 1.0f };
            EngineDiscovery::CallProcessEvent(g_State, obj, fn, &ret);
            PushVector(L, ret.X, ret.Y, ret.Z);
            return 1;
        }
    }
    PushVector(L, 1.0f, 1.0f, 1.0f);
    return 1;
}

static int UObj_SetScale(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    if (!obj) { lua_pushboolean(L, 0); return 1; }
    Vector3D scale = { 1.0f, 1.0f, 1.0f };
    if (!GetVector(L, 2, scale)) {
        if (lua_gettop(L) >= 4) {
            scale.X = static_cast<float>(lua_tonumber(L, 2));
            scale.Y = static_cast<float>(lua_tonumber(L, 3));
            scale.Z = static_cast<float>(lua_tonumber(L, 4));
        }
    }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    void* fn = FindFunctionOnClass(cls, "SetActorScale3D");
    if (!fn) fn = FindFunctionOnClass(cls, "K2_SetActorScale3D");
    if (fn) {
        if (g_State.UEMajor >= 5) {
            struct { double X, Y, Z; } params = { scale.X, scale.Y, scale.Z };
            bool ok = EngineDiscovery::CallProcessEvent(g_State, obj, fn, &params);
            lua_pushboolean(L, ok ? 1 : 0);
            return 1;
        } else {
            struct { float X, Y, Z; } params = { scale.X, scale.Y, scale.Z };
            bool ok = EngineDiscovery::CallProcessEvent(g_State, obj, fn, &params);
            lua_pushboolean(L, ok ? 1 : 0);
            return 1;
        }
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int UObj_GetTransform(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_pushnil(L); return 1; }
    Vector3D loc = { 0, 0, 0 };
    Rotator3D rot = { 0, 0, 0 };
    Vector3D scale = { 1.0f, 1.0f, 1.0f };
    Helper_GetActorLocation(o, loc);
    Helper_GetActorRotation(o, rot);

    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    void* fnScale = FindFunctionOnClass(cls, "GetActorScale3D");
    if (!fnScale) fnScale = FindFunctionOnClass(cls, "K2_GetActorScale3D");
    if (fnScale) {
        if (g_State.UEMajor >= 5) {
            struct { double X, Y, Z; } ret = { 1.0, 1.0, 1.0 };
            EngineDiscovery::CallProcessEvent(g_State, o, fnScale, &ret);
            scale.X = static_cast<float>(ret.X);
            scale.Y = static_cast<float>(ret.Y);
            scale.Z = static_cast<float>(ret.Z);
        } else {
            struct { float X, Y, Z; } ret = { 1.0f, 1.0f, 1.0f };
            EngineDiscovery::CallProcessEvent(g_State, o, fnScale, &ret);
            scale = { ret.X, ret.Y, ret.Z };
        }
    }
    Transform3D t = { rot, loc, scale };
    PushTransform(L, t);
    return 1;
}

static int UObj_SetTransform(lua_State* L) {
    UObject* o = GetUObject(L, 1);
    if (!o) { lua_pushboolean(L, 0); return 1; }
    Transform3D t;
    lua_getfield(L, 2, "Translation");
    if (lua_isuserdata(L, -1) || lua_istable(L, -1)) {
        GetVector(L, -1, t.Translation);
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "Rotation");
    if (lua_isuserdata(L, -1) || lua_istable(L, -1)) {
        GetRotator(L, -1, t.Rotation);
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "Scale3D");
    if (lua_isuserdata(L, -1) || lua_istable(L, -1)) {
        GetVector(L, -1, t.Scale3D);
    }
    lua_pop(L, 1);

    bool okLoc = Helper_SetActorLocation(o, t.Translation);
    bool okRot = Helper_SetActorRotation(o, t.Rotation);
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, o);
    void* fnScale = FindFunctionOnClass(cls, "SetActorScale3D");
    if (!fnScale) fnScale = FindFunctionOnClass(cls, "K2_SetActorScale3D");
    if (fnScale) {
        if (g_State.UEMajor >= 5) {
            struct { double X, Y, Z; } params = { t.Scale3D.X, t.Scale3D.Y, t.Scale3D.Z };
            EngineDiscovery::CallProcessEvent(g_State, o, fnScale, &params);
        } else {
            struct { float X, Y, Z; } params = { t.Scale3D.X, t.Scale3D.Y, t.Scale3D.Z };
            EngineDiscovery::CallProcessEvent(g_State, o, fnScale, &params);
        }
    }
    lua_pushboolean(L, (okLoc || okRot) ? 1 : 0);
    return 1;
}

static int UObj_DestroyActor(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    if (!obj) { lua_pushboolean(L, 0); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    void* fn = FindFunctionOnClass(cls, "K2_DestroyActor");
    if (!fn) fn = FindFunctionOnClass(cls, "DestroyActor");
    if (fn) {
        bool ok = EngineDiscovery::CallProcessEvent(g_State, obj, fn, nullptr);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int UObj_SetActorHiddenInGame(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    bool bHidden = lua_toboolean(L, 2) != 0;
    if (!obj) { lua_pushboolean(L, 0); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    void* fn = FindFunctionOnClass(cls, "SetActorHiddenInGame");
    if (fn) {
        struct { bool bNewHidden; } params = { bHidden };
        bool ok = EngineDiscovery::CallProcessEvent(g_State, obj, fn, &params);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int UObj_IsHidden(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    if (!obj || !SafeMem_IsReadable(obj, 32)) { lua_pushboolean(L, 0); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    if (!cls) { lua_pushboolean(L, 0); return 1; }

    FoundProperty prop = FindPropertyOnClass(cls, "bHidden");
    if (!prop.Found) prop = FindPropertyOnClass(cls, "bHiddenInGame");
    if (!prop.Found) prop = FindPropertyOnClass(cls, "bActorHiddenInGame");
    if (prop.Found && prop.Offset >= 0) {
        return PushPropertyValue(L, reinterpret_cast<uint8_t*>(obj), prop);
    }

    void* fn = FindFunctionOnClass(cls, "K2_IsActorVisible");
    if (fn) {
        struct { bool ReturnValue; } params = { false };
        EngineDiscovery::CallProcessEvent(g_State, obj, fn, &params);
        lua_pushboolean(L, !params.ReturnValue ? 1 : 0);
        return 1;
    }

    void* fnHidden = FindFunctionOnClass(cls, "IsHidden");
    if (fnHidden) {
        struct { bool ReturnValue; } params = { false };
        EngineDiscovery::CallProcessEvent(g_State, obj, fnHidden, &params);
        lua_pushboolean(L, params.ReturnValue ? 1 : 0);
        return 1;
    }

    lua_pushboolean(L, 0);
    return 1;
}

static int UObj_SetActorEnableCollision(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    bool bEnable = lua_toboolean(L, 2) != 0;
    if (!obj) { lua_pushboolean(L, 0); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    void* fn = FindFunctionOnClass(cls, "SetActorEnableCollision");
    if (fn) {
        struct { bool bNewActorEnableCollision; } params = { bEnable };
        bool ok = EngineDiscovery::CallProcessEvent(g_State, obj, fn, &params);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int Lua_DuplicateObject(lua_State* L) {
    UObject* source = GetUObject(L, 1);
    if (!source) { lua_pushnil(L); return 1; }
    UObject* outer = lua_gettop(L) >= 2 ? GetUObject(L, 2) : EngineDiscovery::GetObjectOuter(g_State, source);
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, source);
    if (!cls) { lua_pushnil(L); return 1; }

    UObject* clone = nullptr;
    if (outer) {
        clone = EngineDiscovery::SpawnObjectThroughEngine(g_State, cls, outer);
    }

    if (!clone && g_State.StaticConstructObject) {
        clone = ConsoleEnabler::TrySEHConstruct(g_State, cls, outer);
    }

    if (clone) {
        PushUObject(L, clone);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int UObject_Index(lua_State* L) {
    auto* data = static_cast<LuaUObjectData*>(luaL_checkudata(L, 1, UOBJECT_MT));
    const char* key = lua_tostring(L, 2);
    if (!data || !data->Object) {
        if (key && _stricmp(key, "IsValid") == 0) {
            lua_pushcfunction(L, UObj_IsValid);
            return 1;
        }
        return CompatPushInvalidObject(L);
    }
    if (!key) return CompatPushInvalidObject(L);

    UObject* obj = data->Object;

    if (_stricmp(key, "GetAddress")       == 0) { lua_pushcfunction(L, UObj_GetAddress);       return 1; }
    if (_stricmp(key, "GetAddressString") == 0) { lua_pushcfunction(L, UObj_GetAddressString); return 1; }
    if (_stricmp(key, "GetName")          == 0) { lua_pushcfunction(L, UObj_GetName);          return 1; }
    if (_stricmp(key, "GetFullName")      == 0) { lua_pushcfunction(L, UObj_GetFullName);      return 1; }
    if (_stricmp(key, "GetPathName")      == 0) { lua_pushcfunction(L, UObj_GetPathName);      return 1; }
    if (_stricmp(key, "GetFName")         == 0) { lua_pushcfunction(L, UObj_GetFName);         return 1; }
    if (_stricmp(key, "GetClass")         == 0) { lua_pushcfunction(L, UObj_GetClass);         return 1; }
    if (_stricmp(key, "GetOuter")         == 0) { lua_pushcfunction(L, UObj_GetOuter);         return 1; }
    if (_stricmp(key, "GetPackage")       == 0) { lua_pushcfunction(L, UObj_GetPackage);       return 1; }
    if (_stricmp(key, "IsA")              == 0) { lua_pushcfunction(L, UObj_IsA);              return 1; }
    if (_stricmp(key, "IsValid")          == 0) { lua_pushcfunction(L, UObj_IsValid);          return 1; }
    if (_stricmp(key, "get") == 0 || _stricmp(key, "Get") == 0) { lua_pushcfunction(L, UObj_Get); return 1; }
    if (_stricmp(key, "Cast")             == 0) { lua_pushcfunction(L, UObj_Cast);             return 1; }
    if (_stricmp(key, "SpawnActor")       == 0) { lua_pushcfunction(L, Lua_SpawnActor);        return 1; }

    if (_stricmp(key, "HasAllFlags")        == 0) { lua_pushcfunction(L, UObj_HasAllFlags);        return 1; }
    if (_stricmp(key, "HasAnyFlags")        == 0) { lua_pushcfunction(L, UObj_HasAnyFlags);        return 1; }
    if (_stricmp(key, "HasAnyInternalFlags") == 0) { lua_pushcfunction(L, UObj_HasAnyInternalFlags); return 1; }
    if (_stricmp(key, "IsClass")           == 0) { lua_pushcfunction(L, UObj_IsClass);           return 1; }
    if (_stricmp(key, "IsAnyClass")        == 0) { lua_pushcfunction(L, UObj_IsClass);           return 1; }
    if (_stricmp(key, "Reflection")        == 0) { lua_pushcfunction(L, UObj_Reflection);        return 1; }
    if (_stricmp(key, "GetPropertyByName")  == 0) { lua_pushcfunction(L, UObj_GetPropertyByName); return 1; }
    if (_stricmp(key, "SetPropertyByName")  == 0) { lua_pushcfunction(L, UObj_SetPropertyByName); return 1; }

    if (_stricmp(key, "GetProperty")      == 0 || _stricmp(key, "GetPropertyValue") == 0) { lua_pushcfunction(L, UObj_GetProperty);    return 1; }
    if (_stricmp(key, "SetProperty")      == 0 || _stricmp(key, "SetPropertyValue") == 0) { lua_pushcfunction(L, UObj_SetProperty);    return 1; }
    if (_stricmp(key, "GetProperties")    == 0) { lua_pushcfunction(L, UObj_GetProperties);  return 1; }
    if (_stricmp(key, "FindProperty")     == 0) { lua_pushcfunction(L, UObj_FindProperty);   return 1; }
    if (_stricmp(key, "GetFunctions")     == 0) { lua_pushcfunction(L, UObj_GetFunctions);   return 1; }
    if (_stricmp(key, "HasFunction")      == 0) { lua_pushcfunction(L, UObj_HasFunction);    return 1; }
    if (_stricmp(key, "Call")             == 0 || _stricmp(key, "CallFunction")     == 0) { lua_pushcfunction(L, UObj_Call);           return 1; }
    if (_stricmp(key, "GetComponent")     == 0) { lua_pushcfunction(L, UObj_GetComponent);   return 1; }
    if (_stricmp(key, "GetComponents")    == 0) { lua_pushcfunction(L, UObj_GetComponents);  return 1; }
    if (_stricmp(key, "GetComponentsByClass") == 0) { lua_pushcfunction(L, UObj_GetComponentsByClass); return 1; }
    if (_stricmp(key, "GetCDO")           == 0 || _stricmp(key, "GetClassDefaultObject") == 0) { lua_pushcfunction(L, UObj_GetCDO); return 1; }
    if (_stricmp(key, "GetSuperClass")    == 0 || _stricmp(key, "GetSuper")          == 0) { lua_pushcfunction(L, UObj_GetSuperClass); return 1; }
    if (_stricmp(key, "GetChildClasses")  == 0) { lua_pushcfunction(L, UObj_GetChildClasses); return 1; }
    if (_stricmp(key, "IsChildOf")        == 0) { lua_pushcfunction(L, UObj_IsChildOf);       return 1; }
    if (_stricmp(key, "GetWorld")         == 0) { lua_pushcfunction(L, UObj_GetWorld);        return 1; }
    if (_stricmp(key, "DumpProperties")   == 0) { lua_pushcfunction(L, UObj_DumpProperties);  return 1; }
    if (_stricmp(key, "DumpFunctions")    == 0) { lua_pushcfunction(L, UObj_DumpFunctions);   return 1; }
    if (_stricmp(key, "ExportProperty")   == 0) { lua_pushcfunction(L, UObj_ExportProperty);  return 1; }
    if (_stricmp(key, "ImportProperty")   == 0) { lua_pushcfunction(L, UObj_ImportProperty);  return 1; }

    if (_stricmp(key, "GetLocation")        == 0 || _stricmp(key, "K2_GetActorLocation") == 0) { lua_pushcfunction(L, UObj_GetLocation);    return 1; }
    if (_stricmp(key, "SetLocation")        == 0 || _stricmp(key, "SetActorLocation")    == 0 || _stricmp(key, "K2_SetActorLocation") == 0) { lua_pushcfunction(L, UObj_SetLocation);    return 1; }
    if (_stricmp(key, "GetRotation")        == 0 || _stricmp(key, "K2_GetActorRotation") == 0) { lua_pushcfunction(L, UObj_GetRotation);    return 1; }
    if (_stricmp(key, "SetRotation")        == 0 || _stricmp(key, "SetActorRotation")    == 0 || _stricmp(key, "K2_SetActorRotation") == 0) { lua_pushcfunction(L, UObj_SetRotation);    return 1; }
    if (_stricmp(key, "GetScale")           == 0 || _stricmp(key, "GetActorScale3D")     == 0 || _stricmp(key, "K2_GetActorScale3D") == 0) { lua_pushcfunction(L, UObj_GetScale);       return 1; }
    if (_stricmp(key, "SetScale")           == 0 || _stricmp(key, "SetActorScale3D")     == 0 || _stricmp(key, "K2_SetActorScale3D") == 0) { lua_pushcfunction(L, UObj_SetScale);       return 1; }
    if (_stricmp(key, "GetTransform")       == 0 || _stricmp(key, "GetActorTransform")   == 0 || _stricmp(key, "K2_GetActorTransform") == 0) { lua_pushcfunction(L, UObj_GetTransform);   return 1; }
    if (_stricmp(key, "SetTransform")       == 0 || _stricmp(key, "SetActorTransform")   == 0 || _stricmp(key, "K2_SetActorTransform") == 0) { lua_pushcfunction(L, UObj_SetTransform);   return 1; }
    if (_stricmp(key, "Teleport")           == 0 || _stricmp(key, "TeleportTo")          == 0 || _stricmp(key, "K2_TeleportTo") == 0) { lua_pushcfunction(L, UObj_Teleport);       return 1; }
    if (_stricmp(key, "GetVelocity")        == 0) { lua_pushcfunction(L, UObj_GetVelocity);    return 1; }
    if (_stricmp(key, "GetDistanceTo")      == 0) { lua_pushcfunction(L, UObj_GetDistanceTo);  return 1; }
    if (_stricmp(key, "GetForwardVector")   == 0 || _stricmp(key, "GetActorForwardVector") == 0) { lua_pushcfunction(L, UObj_GetForwardVector); return 1; }
    if (_stricmp(key, "GetRightVector")     == 0 || _stricmp(key, "GetActorRightVector")   == 0) { lua_pushcfunction(L, UObj_GetRightVector);   return 1; }
    if (_stricmp(key, "GetUpVector")        == 0 || _stricmp(key, "GetActorUpVector")      == 0) { lua_pushcfunction(L, UObj_GetUpVector);      return 1; }
    if (_stricmp(key, "AddActorWorldOffset")== 0) { lua_pushcfunction(L, UObj_AddActorWorldOffset); return 1; }
    if (_stricmp(key, "AddActorLocalOffset")== 0) { lua_pushcfunction(L, UObj_AddActorLocalOffset); return 1; }
    if (_stricmp(key, "AddActorWorldRotation") == 0) { lua_pushcfunction(L, UObj_AddActorWorldRotation); return 1; }
    if (_stricmp(key, "AddActorLocalRotation") == 0) { lua_pushcfunction(L, UObj_AddActorLocalRotation); return 1; }
    if (_stricmp(key, "GetRootComponent")   == 0) { lua_pushcfunction(L, UObj_GetRootComponent); return 1; }
    if (_stricmp(key, "AttachToActor")      == 0) { lua_pushcfunction(L, UObj_AttachToActor);    return 1; }
    if (_stricmp(key, "AttachToComponent")  == 0) { lua_pushcfunction(L, UObj_AttachToComponent);return 1; }
    if (_stricmp(key, "DetachFromActor")    == 0) { lua_pushcfunction(L, UObj_DetachFromActor);  return 1; }
    if (_stricmp(key, "GetAttachedActors")  == 0) { lua_pushcfunction(L, UObj_GetAttachedActors);return 1; }
    if (_stricmp(key, "GetActorBounds")     == 0 || _stricmp(key, "GetBounds") == 0) { lua_pushcfunction(L, UObj_GetActorBounds); return 1; }
    if (_stricmp(key, "DestroyActor")       == 0 || _stricmp(key, "K2_DestroyActor")     == 0) { lua_pushcfunction(L, UObj_DestroyActor);   return 1; }
    if (_stricmp(key, "SetActorHiddenInGame") == 0) { lua_pushcfunction(L, UObj_SetActorHiddenInGame); return 1; }
    if (_stricmp(key, "IsHidden")           == 0) { lua_pushcfunction(L, UObj_IsHidden);       return 1; }
    if (_stricmp(key, "SetActorEnableCollision") == 0) { lua_pushcfunction(L, UObj_SetActorEnableCollision); return 1; }

    if (_stricmp(key, "GetRowNames")== 0) { lua_pushcfunction(L, DataTable_GetRowNames); return 1; }
    if (_stricmp(key, "FindRow")    == 0) { lua_pushcfunction(L, DataTable_FindRow);     return 1; }
    if (_stricmp(key, "ForEachRow") == 0) { lua_pushcfunction(L, DataTable_ForEachRow);  return 1; }
    if (_stricmp(key, "ExportAsJSON") == 0 || _stricmp(key, "ToJSON") == 0) { lua_pushcfunction(L, DataTable_ExportAsJSON); return 1; }

    if (_stricmp(key, "GetRowMap")   == 0) { lua_pushcfunction(L, Compat_UObj_DataTableGetRowMap);   return 1; }
    if (_stricmp(key, "GetAllRows")  == 0) { lua_pushcfunction(L, Compat_UObj_DataTableGetAllRows);  return 1; }
    if (_stricmp(key, "GetRowStruct")== 0) { lua_pushcfunction(L, Compat_UObj_DataTableGetRowStruct); return 1; }
    if (_stricmp(key, "AddRow")      == 0) { lua_pushcfunction(L, Compat_UObj_DataTableAddRow);      return 1; }
    if (_stricmp(key, "RemoveRow")   == 0) { lua_pushcfunction(L, Compat_UObj_DataTableRemoveRow);   return 1; }
    if (_stricmp(key, "EmptyTable")  == 0) { lua_pushcfunction(L, Compat_UObj_DataTableEmptyTable);  return 1; }

    if (_stricmp(key, "GetNameByValue")    == 0) { lua_pushcfunction(L, Compat_UObj_EnumGetNameByValue);  return 1; }
    if (_stricmp(key, "ForEachName")       == 0) { lua_pushcfunction(L, Compat_UObj_EnumForEachName);     return 1; }
    if (_stricmp(key, "GetEnumNameByIndex")== 0) { lua_pushcfunction(L, Compat_UObj_EnumGetNameByIndex);  return 1; }

    if (_stricmp(key, "GetFunctionFlags")  == 0) { lua_pushcfunction(L, Compat_UObj_GetFunctionFlags);    return 1; }
    if (_stricmp(key, "SetFunctionFlags")  == 0) { lua_pushcfunction(L, Compat_UObj_SetFunctionFlags);    return 1; }

    if (_stricmp(key, "ForEachProperty")   == 0) { lua_pushcfunction(L, Compat_UObj_ForEachProperty);     return 1; }
    if (_stricmp(key, "ForEachFunction")   == 0) { lua_pushcfunction(L, Compat_UObj_ForEachFunction);     return 1; }

    if (_stricmp(key, "GetLevel")          == 0) { lua_pushcfunction(L, Compat_UObj_GetLevel);            return 1; }

    if (_stricmp(key, "EditNameAt")        == 0) { lua_pushcfunction(L, Compat_UObj_EnumEditNameAt);        return 1; }
    if (_stricmp(key, "EditValueAt")       == 0) { lua_pushcfunction(L, Compat_UObj_EnumEditValueAt);       return 1; }
    if (_stricmp(key, "InsertIntoNames")   == 0) { lua_pushcfunction(L, Compat_UObj_EnumInsertIntoNames);   return 1; }
    if (_stricmp(key, "RemoveFromNamesAt") == 0) { lua_pushcfunction(L, Compat_UObj_EnumRemoveFromNamesAt); return 1; }

    {
        int handled = Compat_Surface_Index(L);
        if (handled) return handled;
    }

    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    if (cls) {
        FoundProperty prop = FindPropertyOnClass(cls, key);
        if (prop.Found) {
            return PushPropertyValue(L, reinterpret_cast<uint8_t*>(obj), prop);
        }

        void* ufunc = FindFunctionOnClass(cls, key);
        if (ufunc) {
            auto* dispatch = static_cast<UFunctionDispatchContext*>(lua_newuserdata(L, sizeof(UFunctionDispatchContext)));
            dispatch->TargetObject = obj;
            dispatch->TargetFunction = ufunc;
            luaL_setmetatable(L, UFUNCTION_DISPATCH_MT);
            return 1;
        }
    }

    {
        void* customProp = nullptr;
        int32_t customOffset = 0;
        int32_t customSize = 0;
        std::string customType;
        if (FindCustomPropertyForClass(cls, key, &customProp, &customOffset, &customSize, customType)) {
            return PushCustomPropertyValue(L, reinterpret_cast<uint8_t*>(obj), customOffset, customSize, customType);
        }
    }

    return CompatPushInvalidObject(L);
}

static int UObject_NewIndex(lua_State* L) {
    auto* data = static_cast<LuaUObjectData*>(luaL_checkudata(L, 1, UOBJECT_MT));
    if (!data || !data->Object) return 0;

    const char* key = luaL_checkstring(L, 2);
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, data->Object);
    if (cls) {
        FoundProperty prop = FindPropertyOnClass(cls, key);
        if (prop.Found) {
            SetPropertyValue(L, reinterpret_cast<uint8_t*>(data->Object), prop, 3);
            return 0;
        }
    }

    {
        void* customProp = nullptr;
        int32_t customOffset = 0;
        int32_t customSize = 0;
        std::string customType;
        if (FindCustomPropertyForClass(cls, key, &customProp, &customOffset, &customSize, customType)) {
            SetCustomPropertyValue(L, reinterpret_cast<uint8_t*>(data->Object), customOffset, customSize, customType, 3);
        }
    }
    return 0;
}

static int UObject_ToString(lua_State* L) {
    auto* data = static_cast<LuaUObjectData*>(luaL_checkudata(L, 1, UOBJECT_MT));
    if (!data || !data->Object) {
        lua_pushstring(L, "[UObject] nil");
        return 1;
    }
    std::string name = EngineDiscovery::GetObjectNameStr(g_State, data->Object);
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, data->Object);
    std::string clsName = cls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cls)) : "Unknown";
    char buf[256];
    snprintf(buf, sizeof(buf), "[UObject: %s] %s (0x%p)", clsName.c_str(), name.c_str(), data->Object);
    lua_pushstring(L, buf);
    return 1;
}

static int UObject_Eq(lua_State* L) {
    UObject* o1 = GetUObject(L, 1);
    UObject* o2 = GetUObject(L, 2);
    lua_pushboolean(L, (o1 && o2 && o1 == o2) ? 1 : 0);
    return 1;
}

static int UFunctionDispatch_Call(lua_State* L) {
    auto* dispatch = static_cast<UFunctionDispatchContext*>(luaL_checkudata(L, 1, UFUNCTION_DISPATCH_MT));
    if (!dispatch || !dispatch->TargetObject || !dispatch->TargetFunction) {
        return luaL_error(L, "Attempted to call invalid UFunction dispatcher");
    }

    const int luaArgStart = (GetUObject(L, 2) == dispatch->TargetObject) ? 3 : 2;
    return CallUFunctionDynamic(L, dispatch->TargetObject, dispatch->TargetFunction, luaArgStart);
}

static int UFunctionDispatch_IsValid(lua_State* L) {
    auto* dispatch = static_cast<UFunctionDispatchContext*>(luaL_checkudata(L, 1, UFUNCTION_DISPATCH_MT));
    bool valid = dispatch && dispatch->TargetObject && dispatch->TargetFunction && SafeMem_IsReadable(dispatch->TargetObject, 32);
    lua_pushboolean(L, valid ? 1 : 0);
    return 1;
}

static int UFunctionDispatch_Get(lua_State* L) {
    auto* dispatch = static_cast<UFunctionDispatchContext*>(luaL_checkudata(L, 1, UFUNCTION_DISPATCH_MT));
    if (!dispatch || !dispatch->TargetObject || !dispatch->TargetFunction) {
        return CompatPushInvalidObject(L);
    }
    if (lua_gettop(L) > 1) {
        return UFunctionDispatch_Call(L);
    }
    lua_pushvalue(L, 1);
    return 1;
}

static int UFunctionDispatch_Index(lua_State* L) {
    const char* key = lua_tostring(L, 2);
    if (!key) return CompatPushInvalidObject(L);

    if (_stricmp(key, "IsValid") == 0) {
        lua_pushcfunction(L, UFunctionDispatch_IsValid);
        return 1;
    }
    if (_stricmp(key, "get") == 0 || _stricmp(key, "Get") == 0) {
        lua_pushcfunction(L, UFunctionDispatch_Get);
        return 1;
    }
    if (_stricmp(key, "GetFunction") == 0 || _stricmp(key, "GetUFunction") == 0) {
        auto* dispatch = static_cast<UFunctionDispatchContext*>(luaL_checkudata(L, 1, UFUNCTION_DISPATCH_MT));
        if (dispatch && dispatch->TargetFunction) {
            PushUObject(L, static_cast<UObject*>(dispatch->TargetFunction));
            return 1;
        }
        return CompatPushInvalidObject(L);
    }
    return CompatPushInvalidObject(L);
}

static int UFunctionDispatch_ToString(lua_State* L) {
    auto* dispatch = static_cast<UFunctionDispatchContext*>(luaL_checkudata(L, 1, UFUNCTION_DISPATCH_MT));
    if (dispatch && dispatch->TargetFunction) {
        std::string name = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(dispatch->TargetFunction));
        lua_pushfstring(L, "[UFunctionDispatch: %s]", name.c_str());
        return 1;
    }
    lua_pushstring(L, "[UFunctionDispatch: invalid]");
    return 1;
}

static int UObject_CallCompat(lua_State* L) {
    UObject* ufunc = GetUObject(L, 1);
    if (!ufunc) {
        return luaL_error(L, "attempted to call a nil UObject");
    }

    bool isUFunction = false;
    if (g_State.UFunctionClass) {
        isUFunction = EngineDiscovery::GetObjectClass(g_State, ufunc) == g_State.UFunctionClass;
    }
    if (!isUFunction) {
        UClass* fcls = EngineDiscovery::GetObjectClass(g_State, ufunc);
        isUFunction = fcls && _stricmp(EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(fcls)).c_str(), "UFunction") == 0;
    }
    if (!isUFunction) {
        return luaL_error(L, "attempted to call a UObject that is not a UFunction (fetch functions through their owner or pass a context)");
    }

    UObject* ctx = GetUObject(L, 2);
    if (!ctx) {
        return luaL_error(L, "calling a UFunction without a bound context requires a context object as the first argument");
    }
    return CallUFunctionDynamic(L, ctx, ufunc, 3);
}

void PushUObject(lua_State* L, UObject* object) {
    if (!object) {
        lua_pushnil(L);
        return;
    }
    auto* data = static_cast<LuaUObjectData*>(lua_newuserdata(L, sizeof(LuaUObjectData)));
    data->Object = object;
    luaL_setmetatable(L, UOBJECT_MT);
}

UObject* GetUObject(lua_State* L, int index) {
    if (luaL_testudata(L, index, UOBJECT_MT)) {
        auto* data = static_cast<LuaUObjectData*>(lua_touserdata(L, index));
        return data ? data->Object : nullptr;
    }
    if (lua_islightuserdata(L, index)) {
        return static_cast<UObject*>(lua_touserdata(L, index));
    }
    if (lua_isinteger(L, index)) {
        return reinterpret_cast<UObject*>(static_cast<uintptr_t>(lua_tointeger(L, index)));
    }
    return nullptr;
}

void RegisterLuaUObject(lua_State* L) {
    luaL_newmetatable(L, UOBJECT_MT);
    lua_pushcfunction(L, UObject_Index);      lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, UObject_NewIndex);   lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, UObject_ToString);   lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, UObject_Eq);         lua_setfield(L, -2, "__eq");
    lua_pushcfunction(L, UObject_CallCompat); lua_setfield(L, -2, "__call");

    lua_pushcfunction(L, Compat_UObj_Len);    lua_setfield(L, -2, "__len");
    lua_pop(L, 1);

    luaL_newmetatable(L, UFUNCTION_DISPATCH_MT);
    lua_pushcfunction(L, UFunctionDispatch_Call);     lua_setfield(L, -2, "__call");
    lua_pushcfunction(L, UFunctionDispatch_Index);    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, UFunctionDispatch_ToString); lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    lua_pushcfunction(L, Lua_SpawnActor);            lua_setglobal(L, "SpawnActor");
    lua_pushcfunction(L, Lua_StaticConstructObject); lua_setglobal(L, "StaticConstructObject");
    lua_pushcfunction(L, Lua_StaticConstructObject); lua_setglobal(L, "NewObject");
    lua_pushcfunction(L, Lua_StaticConstructObject); lua_setglobal(L, "ConstructObject");
    lua_pushcfunction(L, Lua_DuplicateObject);       lua_setglobal(L, "DuplicateObject");
    lua_pushcfunction(L, Lua_StaticClass);           lua_setglobal(L, "StaticClass");
    lua_pushcfunction(L, Lua_StaticStruct);          lua_setglobal(L, "StaticStruct");
    lua_pushcfunction(L, Lua_StaticEnum);            lua_setglobal(L, "StaticEnum");
    lua_pushcfunction(L, Lua_LoadAsset);             lua_setglobal(L, "LoadAsset");
    lua_pushcfunction(L, Lua_LoadAsset);             lua_setglobal(L, "StaticLoadObject");
    lua_pushcfunction(L, Lua_GetBlueprintPipeline);  lua_setglobal(L, "GetBlueprintPipeline");
    lua_pushcfunction(L, Lua_GetObjectShape);        lua_setglobal(L, "GetObjectShape");
}

}
