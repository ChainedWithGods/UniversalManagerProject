#include "LuaHookManager.h"
#include "LuaManager.h"
#include "LuaUObject.h"
#include "LuaTypes.h"
#include "LuaTypeGenerator.h"
#include "LuaCompatApi.h"
#include "../core/EngineDiscovery.h"
#include "../core/FunctionFinder.h"
#include "../core/ObjectIndex.h"
#include "../core/Types.h"
#include "../core/WorldDiscovery.h"
#include "../console/ConsoleInternal.h"
#include "../utils/SafeMemory.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <iostream>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <thread>
#include <unordered_set>

extern "C" {
#include "../../Helpers/minhook/src/hde/hde64.h"
}

extern EngineState g_State;

namespace LuaEngine {

static std::atomic<uint64_t> s_LastProcessEventTick{0};

static std::atomic<uint64_t> s_EngineCall{1};

static thread_local int s_EngineCallDepth = 0;

static thread_local bool s_ArrivedViaActorVtable = false;

struct EngineCallGuard {
    const bool Outermost;
    EngineCallGuard() : Outermost(s_EngineCallDepth == 0) {
        if (Outermost) s_EngineCall.fetch_add(1, std::memory_order_relaxed);
        ++s_EngineCallDepth;
    }
    ~EngineCallGuard() { --s_EngineCallDepth; }
};
static const char* HOOK_PARAMS_MT = "Avalonium.HookParams";

struct LuaHookParamsData {
    UObject* TargetObject = nullptr;
    void* TargetFunction = nullptr;
    void* ParamsBuffer = nullptr;
    bool Cancelled = false;
};

static int HookParams_Get_Impl(lua_State* L) {
    auto* d = static_cast<LuaHookParamsData*>(luaL_checkudata(L, 1, HOOK_PARAMS_MT));
    const char* propName = luaL_checkstring(L, 2);
    if (!d || !d->TargetFunction || !d->ParamsBuffer) { lua_pushnil(L); return 1; }

    auto ProcessChild = [&](void* child, const std::string& name, const std::string& type) -> bool {
        if (_stricmp(name.c_str(), propName) == 0) {
            int32_t off = 0;
            if (g_State.FProperty_OffsetOff >= 0)
                SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &off, 4);
            uint8_t* pAddr = reinterpret_cast<uint8_t*>(d->ParamsBuffer) + off;

            if (type.find("Int8")   != std::string::npos || type.find("Byte") != std::string::npos) { uint8_t v=0; SafeMem_Read(pAddr,&v,1); lua_pushinteger(L,v); return true; }
            if (type.find("Int16")  != std::string::npos) { int16_t v=0; SafeMem_Read(pAddr,&v,2); lua_pushinteger(L,v); return true; }
            if (type.find("Int")    != std::string::npos) { int32_t v=0; SafeMem_Read(pAddr,&v,4); lua_pushinteger(L,v); return true; }
            if (type.find("Float")  != std::string::npos) { float   v=0; SafeMem_Read(pAddr,&v,4); lua_pushnumber(L,v);  return true; }
            if (type.find("Double") != std::string::npos) { double  v=0; SafeMem_Read(pAddr,&v,8); lua_pushnumber(L,v);  return true; }
            if (type.find("Bool")   != std::string::npos) {
                uint8_t bOff = 0, fMask = 1;
                if (g_State.FBoolProperty_BaseOff >= 0) {
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 1, &bOff, 1);
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 3, &fMask, 1);
                    if (fMask == 0) fMask = 1;
                }
                uint8_t v=0; SafeMem_Read(pAddr + bOff, &v, 1); lua_pushboolean(L, (v & fMask) ? 1 : 0); return true;
            }
            if (type.find("Object") != std::string::npos || type.find("Class") != std::string::npos) {
                void* p=nullptr; SafeMem_Read(pAddr,&p,sizeof(void*)); PushUObject(L,static_cast<UObject*>(p)); return true;
            }
            if (type.find("Str")    != std::string::npos) {
                FString fs={}; if (SafeMem_Read(pAddr,&fs,sizeof(fs))) { lua_pushstring(L,fs.ToStdString().c_str()); return true; }
            }
            if (type.find("Name")   != std::string::npos) {
                FName fn={}; if (SafeMem_Read(pAddr,&fn,sizeof(fn))) {
                    std::string s = EngineDiscovery::GetCachedFName(g_State, fn.ComparisonIndex);
                    lua_pushstring(L, s.c_str()); return true;
                }
            }
            if (type.find("Struct") != std::string::npos) {
                Vector3D v; SafeMem_Read(pAddr, &v, sizeof(v));
                PushVector(L, v.X, v.Y, v.Z); return true;
            }
            lua_pushlightuserdata(L, pAddr); return true;
        }
        return false;
    };

    if (g_State.UStruct_ChildPropsOff >= 0) {
        void* child = EngineDiscovery::GetChildPropsHead(g_State, static_cast<UStruct*>(d->TargetFunction));
        while (child) {
            std::string name = EngineDiscovery::GetFFieldName(g_State, child);
            std::string type = EngineDiscovery::GetFFieldTypeName(g_State, child);
            if (ProcessChild(child, name, type)) return 1;
            child = EngineDiscovery::GetFFieldNext(g_State, child);
        }
    } else if (g_State.UStruct_ChildrenOff >= 0) {
        void* child = EngineDiscovery::GetChildrenHead(g_State, static_cast<UStruct*>(d->TargetFunction));
        while (child) {
            std::string name = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(child));
            UClass* pCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(child));
            std::string type = pCls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(pCls)) : "Property";
            if (ProcessChild(child, name, type)) return 1;
            child = EngineDiscovery::GetFieldNext(g_State, child);
        }
    }
    lua_pushnil(L); return 1;
}

static int HookParams_Set_Impl(lua_State* L) {
    auto* d = static_cast<LuaHookParamsData*>(luaL_checkudata(L, 1, HOOK_PARAMS_MT));
    const char* propName = luaL_checkstring(L, 2);
    if (!d || !d->TargetFunction || !d->ParamsBuffer) { lua_pushboolean(L,0); return 1; }

    auto ProcessChild = [&](void* child, const std::string& name, const std::string& type) -> bool {
        if (_stricmp(name.c_str(), propName) == 0) {
            int32_t off = 0;
            if (g_State.FProperty_OffsetOff >= 0)
                SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &off, 4);
            uint8_t* pAddr = reinterpret_cast<uint8_t*>(d->ParamsBuffer) + off;

            if      (type.find("Int8")   != std::string::npos || type.find("Byte") != std::string::npos) { uint8_t v=static_cast<uint8_t>(lua_tointeger(L,3)); memcpy(pAddr,&v,1); }
            else if (type.find("Int16")  != std::string::npos) { int16_t v=static_cast<int16_t>(lua_tointeger(L,3)); memcpy(pAddr,&v,2); }
            else if (type.find("Int")    != std::string::npos) { int32_t v=static_cast<int32_t>(lua_tointeger(L,3)); memcpy(pAddr,&v,4); }
            else if (type.find("Float")  != std::string::npos) { float   v=static_cast<float>(lua_tonumber(L,3));   memcpy(pAddr,&v,4); }
            else if (type.find("Double") != std::string::npos) { double  v=static_cast<double>(lua_tonumber(L,3));  memcpy(pAddr,&v,8); }
            else if (type.find("Bool")   != std::string::npos) {
                bool val = lua_toboolean(L, 3) != 0;
                uint8_t bOff = 0, bMask = 1, fMask = 1;
                if (g_State.FBoolProperty_BaseOff >= 0) {
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 1, &bOff, 1);
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 2, &bMask, 1);
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 3, &fMask, 1);
                    if (bMask == 0) bMask = 1;
                    if (fMask == 0) fMask = bMask;
                }
                uint8_t* byteAddr = pAddr + bOff;
                uint8_t v = 0;
                SafeMem_Read(byteAddr, &v, 1);
                v = (v & ~fMask) | (val ? bMask : 0);
                memcpy(byteAddr, &v, 1);
            }
            else if (type.find("Object") != std::string::npos || type.find("Class") != std::string::npos) {
                UObject* uo=GetUObject(L,3); memcpy(pAddr,&uo,sizeof(void*));
            }
            return true;
        }
        return false;
    };

    if (g_State.UStruct_ChildPropsOff >= 0) {
        void* child = EngineDiscovery::GetChildPropsHead(g_State, static_cast<UStruct*>(d->TargetFunction));
        while (child) {
            std::string name = EngineDiscovery::GetFFieldName(g_State, child);
            std::string type = EngineDiscovery::GetFFieldTypeName(g_State, child);
            if (ProcessChild(child, name, type)) { lua_pushboolean(L, 1); return 1; }
            child = EngineDiscovery::GetFFieldNext(g_State, child);
        }
    } else if (g_State.UStruct_ChildrenOff >= 0) {
        void* child = EngineDiscovery::GetChildrenHead(g_State, static_cast<UStruct*>(d->TargetFunction));
        while (child) {
            std::string name = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(child));
            UClass* pCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(child));
            std::string type = pCls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(pCls)) : "Property";
            if (ProcessChild(child, name, type)) { lua_pushboolean(L, 1); return 1; }
            child = EngineDiscovery::GetFieldNext(g_State, child);
        }
    }
    lua_pushboolean(L,0); return 1;
}

static int HookParams_Cancel_Impl(lua_State* L) {
    auto* d = static_cast<LuaHookParamsData*>(luaL_checkudata(L, 1, HOOK_PARAMS_MT));
    if (d) d->Cancelled = true;
    return 0;
}

static int HookParams_IsCancelled_Impl(lua_State* L) {
    auto* d = static_cast<LuaHookParamsData*>(luaL_checkudata(L, 1, HOOK_PARAMS_MT));
    lua_pushboolean(L, (d && d->Cancelled) ? 1 : 0);
    return 1;
}

static int HookParams_GetSelf_Impl(lua_State* L) {
    auto* d = static_cast<LuaHookParamsData*>(luaL_checkudata(L, 1, HOOK_PARAMS_MT));
    if (d && d->TargetObject) {
        PushUObject(L, d->TargetObject);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int HookParams_GetFunction_Impl(lua_State* L) {
    auto* d = static_cast<LuaHookParamsData*>(luaL_checkudata(L, 1, HOOK_PARAMS_MT));
    if (d && d->TargetFunction) {
        PushUObject(L, static_cast<UObject*>(d->TargetFunction));
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int HookParams_GetReturnValue_Impl(lua_State* L) {
    auto* d = static_cast<LuaHookParamsData*>(luaL_checkudata(L, 1, HOOK_PARAMS_MT));
    if (!d || !d->TargetFunction || !d->ParamsBuffer) { lua_pushnil(L); return 1; }

    auto ProcessReturn = [&](void* child, const std::string& name, const std::string& type, uint64_t flags) -> bool {
        if ((flags & 0x400) != 0 || name == "ReturnValue") {
            int32_t off = 0;
            if (g_State.FProperty_OffsetOff >= 0) SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &off, 4);
            uint8_t* pAddr = reinterpret_cast<uint8_t*>(d->ParamsBuffer) + off;
            if (type.find("Int8") != std::string::npos || type.find("Byte") != std::string::npos) { uint8_t v=0; SafeMem_Read(pAddr,&v,1); lua_pushinteger(L,v); return true; }
            if (type.find("Int16") != std::string::npos) { int16_t v=0; SafeMem_Read(pAddr,&v,2); lua_pushinteger(L,v); return true; }
            if (type.find("Int") != std::string::npos) { int32_t v=0; SafeMem_Read(pAddr,&v,4); lua_pushinteger(L,v); return true; }
            if (type.find("Float") != std::string::npos) { float v=0; SafeMem_Read(pAddr,&v,4); lua_pushnumber(L,v); return true; }
            if (type.find("Double") != std::string::npos) { double v=0; SafeMem_Read(pAddr,&v,8); lua_pushnumber(L,v); return true; }
            if (type.find("Bool") != std::string::npos) {
                uint8_t bOff = 0, fMask = 1;
                if (g_State.FBoolProperty_BaseOff >= 0) {
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 1, &bOff, 1);
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 3, &fMask, 1);
                    if (fMask == 0) fMask = 1;
                }
                uint8_t v=0; SafeMem_Read(pAddr + bOff, &v, 1); lua_pushboolean(L, (v & fMask) ? 1 : 0); return true;
            }
            if (type.find("Object") != std::string::npos || type.find("Class") != std::string::npos) {
                void* p=nullptr; SafeMem_Read(pAddr,&p,sizeof(void*)); PushUObject(L,static_cast<UObject*>(p)); return true;
            }
            if (type.find("Str") != std::string::npos) {
                FString fs={}; if (SafeMem_Read(pAddr,&fs,sizeof(fs))) { lua_pushstring(L,fs.ToStdString().c_str()); return true; }
            }
            lua_pushlightuserdata(L, pAddr);
            return true;
        }
        return false;
    };

    if (g_State.UStruct_ChildPropsOff >= 0) {
        void* child = EngineDiscovery::GetChildPropsHead(g_State, static_cast<UStruct*>(d->TargetFunction));
        while (child) {
            std::string name = EngineDiscovery::GetFFieldName(g_State, child);
            std::string type = EngineDiscovery::GetFFieldTypeName(g_State, child);
            uint64_t flags = 0;
            if (g_State.FProperty_PropertyFlagsOff >= 0) {
                SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_PropertyFlagsOff, &flags, 8);
            }
            if (ProcessReturn(child, name, type, flags)) return 1;
            child = EngineDiscovery::GetFFieldNext(g_State, child);
        }
    } else if (g_State.UStruct_ChildrenOff >= 0) {
        void* child = EngineDiscovery::GetChildrenHead(g_State, static_cast<UStruct*>(d->TargetFunction));
        while (child) {
            std::string name = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(child));
            UClass* pCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(child));
            std::string type = pCls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(pCls)) : "Property";
            uint64_t flags = 0;
            if (g_State.FProperty_PropertyFlagsOff >= 0) {
                SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_PropertyFlagsOff, &flags, 8);
            }
            if (ProcessReturn(child, name, type, flags)) return 1;
            child = EngineDiscovery::GetFieldNext(g_State, child);
        }
    }
    lua_pushnil(L); return 1;
}

static int HookParams_SetReturnValue_Impl(lua_State* L) {
    auto* d = static_cast<LuaHookParamsData*>(luaL_checkudata(L, 1, HOOK_PARAMS_MT));
    if (!d || !d->TargetFunction || !d->ParamsBuffer) { lua_pushboolean(L, 0); return 1; }

    auto ProcessSetReturn = [&](void* child, const std::string& name, const std::string& type, uint64_t flags) -> bool {
        if ((flags & 0x400) != 0 || name == "ReturnValue") {
            int32_t off = 0;
            if (g_State.FProperty_OffsetOff >= 0) SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &off, 4);
            uint8_t* pAddr = reinterpret_cast<uint8_t*>(d->ParamsBuffer) + off;
            if (type.find("Int8") != std::string::npos || type.find("Byte") != std::string::npos) { uint8_t v=static_cast<uint8_t>(lua_tointeger(L,2)); memcpy(pAddr,&v,1); }
            else if (type.find("Int16") != std::string::npos) { int16_t v=static_cast<int16_t>(lua_tointeger(L,2)); memcpy(pAddr,&v,2); }
            else if (type.find("Int") != std::string::npos) { int32_t v=static_cast<int32_t>(lua_tointeger(L,2)); memcpy(pAddr,&v,4); }
            else if (type.find("Float") != std::string::npos) { float v=static_cast<float>(lua_tonumber(L,2)); memcpy(pAddr,&v,4); }
            else if (type.find("Double") != std::string::npos) { double v=static_cast<double>(lua_tonumber(L,2)); memcpy(pAddr,&v,8); }
            else if (type.find("Bool") != std::string::npos) {
                bool val = lua_toboolean(L, 2) != 0;
                uint8_t bOff = 0, bMask = 1, fMask = 1;
                if (g_State.FBoolProperty_BaseOff >= 0) {
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 1, &bOff, 1);
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 2, &bMask, 1);
                    SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 3, &fMask, 1);
                    if (bMask == 0) bMask = 1;
                    if (fMask == 0) fMask = bMask;
                }
                uint8_t* byteAddr = pAddr + bOff;
                uint8_t v = 0;
                SafeMem_Read(byteAddr, &v, 1);
                v = (v & ~fMask) | (val ? bMask : 0);
                memcpy(byteAddr, &v, 1);
            }
            else if (type.find("Object") != std::string::npos || type.find("Class") != std::string::npos) {
                UObject* uo=GetUObject(L,2); memcpy(pAddr,&uo,sizeof(void*));
            }
            return true;
        }
        return false;
    };

    if (g_State.UStruct_ChildPropsOff >= 0) {
        void* child = EngineDiscovery::GetChildPropsHead(g_State, static_cast<UStruct*>(d->TargetFunction));
        while (child) {
            std::string name = EngineDiscovery::GetFFieldName(g_State, child);
            std::string type = EngineDiscovery::GetFFieldTypeName(g_State, child);
            uint64_t flags = 0;
            if (g_State.FProperty_PropertyFlagsOff >= 0) {
                SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_PropertyFlagsOff, &flags, 8);
            }
            if (ProcessSetReturn(child, name, type, flags)) { lua_pushboolean(L, 1); return 1; }
            child = EngineDiscovery::GetFFieldNext(g_State, child);
        }
    } else if (g_State.UStruct_ChildrenOff >= 0) {
        void* child = EngineDiscovery::GetChildrenHead(g_State, static_cast<UStruct*>(d->TargetFunction));
        while (child) {
            std::string name = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(child));
            UClass* pCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(child));
            std::string type = pCls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(pCls)) : "Property";
            uint64_t flags = 0;
            if (g_State.FProperty_PropertyFlagsOff >= 0) {
                SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_PropertyFlagsOff, &flags, 8);
            }
            if (ProcessSetReturn(child, name, type, flags)) { lua_pushboolean(L, 1); return 1; }
            child = EngineDiscovery::GetFieldNext(g_State, child);
        }
    }
    lua_pushboolean(L, 0); return 1;
}

static int HookParams_Dump_Impl(lua_State* L) {
    auto* d = static_cast<LuaHookParamsData*>(luaL_checkudata(L, 1, HOOK_PARAMS_MT));
    if (!d || !d->TargetFunction || !d->ParamsBuffer) { lua_newtable(L); return 1; }

    lua_newtable(L);
    auto ProcessDumpProp = [&](void* child, const std::string& name, const std::string& type, int32_t off, uint64_t flags) {
        lua_newtable(L);
        lua_pushstring(L, type.c_str()); lua_setfield(L, -2, "Type");
        lua_pushinteger(L, off); lua_setfield(L, -2, "Offset");
        lua_pushboolean(L, (flags & 0x100) != 0); lua_setfield(L, -2, "IsOut");
        lua_pushboolean(L, (flags & 0x400) != 0 || name == "ReturnValue"); lua_setfield(L, -2, "IsReturn");

        lua_pushcfunction(L, HookParams_Get_Impl);
        lua_pushvalue(L, 1);
        lua_pushstring(L, name.c_str());
        if (lua_pcall(L, 2, 1, 0) == LUA_OK) {
            lua_setfield(L, -2, "Value");
        } else {
            lua_pop(L, 1);
        }

        lua_setfield(L, -2, name.c_str());
    };

    if (g_State.UStruct_ChildPropsOff >= 0) {
        void* child = EngineDiscovery::GetChildPropsHead(g_State, static_cast<UStruct*>(d->TargetFunction));
        while (child) {
            std::string name = EngineDiscovery::GetFFieldName(g_State, child);
            std::string type = EngineDiscovery::GetFFieldTypeName(g_State, child);
            int32_t off = 0;
            if (g_State.FProperty_OffsetOff >= 0) SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &off, 4);
            uint64_t flags = 0;
            if (g_State.FProperty_PropertyFlagsOff >= 0) SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_PropertyFlagsOff, &flags, 8);

            ProcessDumpProp(child, name, type, off, flags);
            child = EngineDiscovery::GetFFieldNext(g_State, child);
        }
    } else if (g_State.UStruct_ChildrenOff >= 0) {
        void* child = EngineDiscovery::GetChildrenHead(g_State, static_cast<UStruct*>(d->TargetFunction));
        while (child) {
            std::string name = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(child));
            UClass* pCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(child));
            std::string type = pCls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(pCls)) : "Property";
            int32_t off = 0;
            if (g_State.FProperty_OffsetOff >= 0) SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &off, 4);
            uint64_t flags = 0;
            if (g_State.FProperty_PropertyFlagsOff >= 0) SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_PropertyFlagsOff, &flags, 8);

            ProcessDumpProp(child, name, type, off, flags);
            child = EngineDiscovery::GetFieldNext(g_State, child);
        }
    }
    return 1;
}

static int HookParams_Index_Impl(lua_State* L) {
    const char* k = luaL_checkstring(L, 2);
    if (strcmp(k, "Get") == 0)            { lua_pushcfunction(L, HookParams_Get_Impl);            return 1; }
    if (strcmp(k, "get") == 0)            { lua_pushcfunction(L, HookParams_Get_Impl);            return 1; }
    if (strcmp(k, "Set") == 0)            { lua_pushcfunction(L, HookParams_Set_Impl);            return 1; }
    if (strcmp(k, "set") == 0)            { lua_pushcfunction(L, HookParams_Set_Impl);            return 1; }
    if (strcmp(k, "Cancel") == 0)         { lua_pushcfunction(L, HookParams_Cancel_Impl);         return 1; }
    if (strcmp(k, "IsCancelled") == 0)    { lua_pushcfunction(L, HookParams_IsCancelled_Impl);    return 1; }
    if (strcmp(k, "GetSelf") == 0)        { lua_pushcfunction(L, HookParams_GetSelf_Impl);        return 1; }
    if (strcmp(k, "GetFunction") == 0)    { lua_pushcfunction(L, HookParams_GetFunction_Impl);    return 1; }
    if (strcmp(k, "GetReturnValue") == 0) { lua_pushcfunction(L, HookParams_GetReturnValue_Impl); return 1; }
    if (strcmp(k, "SetReturnValue") == 0) { lua_pushcfunction(L, HookParams_SetReturnValue_Impl); return 1; }
    if (strcmp(k, "Dump") == 0)           { lua_pushcfunction(L, HookParams_Dump_Impl);           return 1; }
    return HookParams_Get_Impl(L);
}

static int HookParams_NewIndex_Impl(lua_State* L) {
    return HookParams_Set_Impl(L);
}

static void* ResolveInnerProperty(void* prop, const std::string& typeName) {
    int32_t off = -1;
    if (typeName.find("ArrayProperty") != std::string::npos && g_State.FArrayProperty_InnerOff >= 0) {
        off = g_State.FArrayProperty_InnerOff;
    } else if (typeName.find("SetProperty") != std::string::npos && g_State.FSetProperty_ElementPropOff >= 0) {
        off = g_State.FSetProperty_ElementPropOff;
    }
    if (off < 0) return nullptr;
    void* inner = nullptr;
    SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + off, &inner, sizeof(void*));
    return inner;
}

static void CollectParmProperties(void* ufunction, std::unordered_set<const void*>& out) {
    if (!ufunction) return;
    if (g_State.UStruct_ChildPropsOff >= 0) {
        for (void* child = EngineDiscovery::GetChildPropsHead(g_State, static_cast<UStruct*>(ufunction));
             child; child = EngineDiscovery::GetFFieldNext(g_State, child)) {
            out.insert(child);
        }
    } else if (g_State.UStruct_ChildrenOff >= 0) {
        for (void* child = EngineDiscovery::GetChildrenHead(g_State, static_cast<UStruct*>(ufunction));
             child; child = EngineDiscovery::GetFieldNext(g_State, child)) {
            out.insert(child);
        }
    }
}

static bool IsOutParmList(const FOutParmRec* head, const std::unordered_set<const void*>& parms) {
    if (!head || parms.empty()) return false;
    std::unordered_set<const FOutParmRec*> seen;
    for (const FOutParmRec* node = head; node; node = node->NextOutParm) {
        if (!SafeMem_IsReadable(node, sizeof(FOutParmRec))) return false;
        if (!seen.insert(node).second) return false;
        if (!parms.count(node->Property)) return false;
        if (seen.size() > 64) return false;
    }
    return true;
}

static const FOutParmRec* ReadOutParmListAt(const void* frame, int32_t off,
                                           const std::unordered_set<const void*>& parms) {
    const FOutParmRec* head = nullptr;
    if (!SafeMem_Read(static_cast<const uint8_t*>(frame) + off, &head, sizeof(head))) return nullptr;
    return IsOutParmList(head, parms) ? head : nullptr;
}

static uint8_t* FindOutParmAddress(const FOutParmRec* list, const void* property) {
    for (const FOutParmRec* node = list; node; node = node->NextOutParm) {
        if (node->Property != property) continue;

        return (node->PropAddr && SafeMem_IsReadable(node->PropAddr, 1)) ? node->PropAddr : nullptr;
    }
    return nullptr;
}

static uint8_t* ResolveOutParmAddress(const void* frame, const void* property,
                                     const std::unordered_set<const void*>& parms) {
    if (!frame || !property || parms.empty()) return nullptr;

    static std::atomic<int32_t> s_ListOffset{ -1 };
    const int32_t measured = s_ListOffset.load(std::memory_order_relaxed);
    if (measured >= 0) {
        return FindOutParmAddress(ReadOutParmListAt(frame, measured, parms), property);
    }

    if (const FOutParmRec* list = ReadOutParmListAt(frame, FFrame_OutParmsOff, parms)) {
        s_ListOffset.store(FFrame_OutParmsOff, std::memory_order_relaxed);
        return FindOutParmAddress(list, property);
    }

    for (int32_t off = FFrame_ScanFirstOff; off <= FFrame_ScanLastOff;
         off += static_cast<int32_t>(sizeof(void*))) {
        if (off == FFrame_OutParmsOff) continue;
        if (const FOutParmRec* list = ReadOutParmListAt(frame, off, parms)) {
            s_ListOffset.store(off, std::memory_order_relaxed);
            return FindOutParmAddress(list, property);
        }
    }
    return nullptr;
}

int PushHookParamProxies(lua_State* L, void* ufunction, void* params, const void* frame = nullptr) {
    if (!ufunction || !params) return 0;

    int pushed = 0;
    FastMemScope fastMem;

    std::unordered_set<const void*> parmProperties;
    if (frame) CollectParmProperties(ufunction, parmProperties);

    bool reportFrame = false;
    {
        static std::mutex reportedMutex;
        static std::unordered_set<void*> reported;
        std::lock_guard<std::mutex> lock(reportedMutex);
        reportFrame = reported.insert(ufunction).second;
    }
    std::string report;

    auto ProcessChild = [&](void* child, const std::string& name, const std::string& type, uint64_t flags) {
        if (name.empty()) return;
        if (_stricmp(name.c_str(), "ReturnValue") == 0) return;
        if (flags & kCpfReturnParm) return;
        if (!(flags & kCpfParm)) return;

        FoundPropertyEx prop;
        prop.Prop = child;
        prop.TypeName = type;
        prop.Flags = flags;
        if (g_State.FProperty_OffsetOff >= 0) {
            SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_OffsetOff, &prop.Offset, 4);
        }
        if (g_State.FProperty_ElementSizeOff >= 0) {
            SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_ElementSizeOff, &prop.ElementSize, 4);
        }
        if (prop.TypeName.find("BoolProperty") != std::string::npos && g_State.FBoolProperty_BaseOff >= 0) {
            SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 1, &prop.ByteOffset, 1);
            SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 2, &prop.ByteMask, 1);
            SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FBoolProperty_BaseOff + 3, &prop.FieldMask, 1);
            if (prop.ByteMask == 0) prop.ByteMask = 1;
            if (prop.FieldMask == 0) prop.FieldMask = prop.ByteMask;
        }
        if (prop.TypeName.find("ArrayProperty") != std::string::npos ||
            prop.TypeName.find("SetProperty") != std::string::npos) {
            prop.InnerProp = ResolveInnerProperty(child, prop.TypeName);
        } else if (prop.TypeName.find("MapProperty") != std::string::npos) {
            if (g_State.FMapProperty_KeyPropOff >= 0)
                SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FMapProperty_KeyPropOff, &prop.KeyProp, sizeof(void*));
            if (g_State.FMapProperty_ValuePropOff >= 0)
                SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FMapProperty_ValuePropOff, &prop.ValueProp, sizeof(void*));
        }

        uint8_t* propBase = reinterpret_cast<uint8_t*>(params);
        if ((flags & kCpfOutParm) != 0 && frame) {
            if (uint8_t* callerAddr = ResolveOutParmAddress(frame, child, parmProperties)) {
                propBase = callerAddr;
                prop.Offset = 0;
            }
        }

        if (reportFrame && report.size() < 1024) {
            const bool isOut = (flags & kCpfOutParm) != 0;
            const bool readable = prop.Offset >= 0 &&
                SafeMem_IsReadable(propBase + prop.Offset,
                                   prop.ElementSize > 0 ? static_cast<size_t>(prop.ElementSize) : 1) != 0;
            char entry[256];
            snprintf(entry, sizeof(entry), " %s:%s@%d+%d%s%s%s", name.c_str(), type.c_str(),
                     prop.Offset, prop.ElementSize, isOut ? " out" : "",
                     readable ? " readable" : " UNREADABLE",
                     isOut ? (propBase == reinterpret_cast<uint8_t*>(params)
                                  ? " destination=none" : " destination=recorded")
                           : "");
            report += entry;
        }

        PushPropertyView(L, propBase, prop, name.c_str());
        ++pushed;
    };

    if (g_State.UStruct_ChildPropsOff >= 0) {
        void* child = EngineDiscovery::GetChildPropsHead(g_State, static_cast<UStruct*>(ufunction));
        while (child) {
            std::string name = EngineDiscovery::GetFFieldName(g_State, child);
            std::string type = EngineDiscovery::GetFFieldTypeName(g_State, child);
            uint64_t flags = 0;
            if (g_State.FProperty_PropertyFlagsOff >= 0) {
                SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_PropertyFlagsOff, &flags, 8);
            }
            ProcessChild(child, name, type, flags);
            child = EngineDiscovery::GetFFieldNext(g_State, child);
        }
    } else if (g_State.UStruct_ChildrenOff >= 0) {
        void* child = EngineDiscovery::GetChildrenHead(g_State, static_cast<UStruct*>(ufunction));
        while (child) {
            std::string name = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(child));
            UClass* pCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(child));
            std::string type = pCls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(pCls)) : "Property";
            uint64_t flags = 0;
            if (g_State.FProperty_PropertyFlagsOff >= 0) {
                SafeMem_Read(reinterpret_cast<uint8_t*>(child) + g_State.FProperty_PropertyFlagsOff, &flags, 8);
            }
            ProcessChild(child, name, type, flags);
            child = EngineDiscovery::GetFieldNext(g_State, child);
        }
    }

    if (reportFrame) {
        char line[1400];
        snprintf(line, sizeof(line),
                 "[LuaHook] Param frame for '%s': Locals=%p frame=%p, %d parameter(s):%s",
                 EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(ufunction)).c_str(),
                 params, frame, pushed, report.c_str());
        EngineDiscovery::WriteLogLine(line);
    }
    return pushed;
}

LuaHookManager::LuaHookManager() {}
LuaHookManager::~LuaHookManager() { Shutdown(); }

LuaHookManager& LuaHookManager::Get() {
    static LuaHookManager instance;
    return instance;
}

static bool InstallVTableHook(EngineState& state, void* uobjectImpl, void* aactorImpl,
                              void* uobjectDetour, void* aactorDetour,
                              void** outUObjectOriginal, void** outAActorOriginal) {
    if (state.ProcessEvent_VTableIdx < 0 || !state.GUObjectArray) return false;
    if (!uobjectImpl && !aactorImpl) return false;

    const int count = EngineDiscovery::GetObjectCount(state);
    const int kMaxScan = 3000;
    const int64_t stride = (count > kMaxScan) ? (count / kMaxScan) : 1;
    int hookedCount = 0;
    std::unordered_set<void**> hookedVTables;

    for (int64_t i = 0; i < count && hookedVTables.size() < (size_t)kMaxScan; i += stride) {
        void* obj = EngineDiscovery::GetObjectByIndex(state, static_cast<int32_t>(i));
        if (!obj) continue;
        void** vft = nullptr;
        if (!SafeMem_Read(obj, &vft, sizeof(void*)) || !vft) continue;
        if (reinterpret_cast<uintptr_t>(vft) < 0x10000) continue;
        if (hookedVTables.count(vft) != 0) continue;

        void* origFunc = nullptr;
        if (!SafeMem_Read(&vft[state.ProcessEvent_VTableIdx], &origFunc, sizeof(void*)) || !origFunc)
            continue;

        void* detour = nullptr;
        void** originalSlot = nullptr;
        if (aactorImpl && origFunc == aactorImpl) {
            detour = aactorDetour;
            originalSlot = outAActorOriginal;
        } else if (uobjectImpl && origFunc == uobjectImpl) {
            detour = uobjectDetour;
            originalSlot = outUObjectOriginal;
        } else {
            continue;
        }

        hookedVTables.insert(vft);
        if (originalSlot && !*originalSlot) *originalSlot = origFunc;
        DWORD oldProt = 0;
        if (VirtualProtect(&vft[state.ProcessEvent_VTableIdx], sizeof(void*), PAGE_READWRITE, &oldProt)) {
            vft[state.ProcessEvent_VTableIdx] = detour;
            VirtualProtect(&vft[state.ProcessEvent_VTableIdx], sizeof(void*), oldProt, &oldProt);
            ++hookedCount;
        }
    }
    return hookedCount > 0 &&
           ((outUObjectOriginal && *outUObjectOriginal) ||
            (outAActorOriginal && *outAActorOriginal));
}

static int FindBaseImplementationCall(void* aactorImpl, void* uobjectImpl) {
    return EngineDiscovery::FindDirectBranchTo(aactorImpl, uobjectImpl);
}

struct ProcessEventImplementations {
    void* UObjectImpl = nullptr;
    void* AActorImpl = nullptr;
    bool Derived = false;
};

static void* GetProcessEventAddress(ProcessEventImplementations& impls) {
    impls = ProcessEventImplementations{};
    if (g_State.ProcessEvent_VTableIdx < 0 || !g_State.GUObjectArray) return nullptr;

    struct SlotStats {
        int objectReads = 0;
        int actorReads = 0;
        int objectMatch = 0;
        int actorMatch = 0;
        int unexplained = 0;
        void* uobjectImpl = nullptr;
        void* aactorImpl = nullptr;
        bool derived = false;

        int branchOffset = -1;
        int flagsOffset = -1;

        uintptr_t objectMajority = 0;
        int objectMajorityCount = 0;
        uintptr_t actorMajority = 0;
        int actorMajorityCount = 0;
    };

    constexpr int kMaxScan = 2000;
    constexpr int kMinReads = 30;
    constexpr int lo = 0x30, hi = 0x60;

    std::unordered_map<int, SlotStats> stats;
    std::unordered_map<int, std::unordered_map<uintptr_t, int>> objectFreqBySlot;
    std::unordered_map<int, std::unordered_map<uintptr_t, int>> actorFreqBySlot;

    for (int slot = lo; slot <= hi; ++slot) {
        SlotStats& s = stats[slot];
        void* uobjectImpl = nullptr;
        void* aactorImpl = nullptr;
        bool distinct = false;
        if (EngineDiscovery::ResolveProcessEventImplementations(g_State, slot, &uobjectImpl,
                                                                &aactorImpl, &distinct)) {
            s.uobjectImpl = uobjectImpl;
            s.aactorImpl = aactorImpl;
            s.derived = distinct;
            if (s.derived) {
                s.branchOffset = FindBaseImplementationCall(s.aactorImpl, s.uobjectImpl);
                const int uobjectFlags =
                    EngineDiscovery::FindFuncNativeFlagsOffset(s.uobjectImpl);
                const int aactorFlags =
                    EngineDiscovery::FindFuncNativeFlagsOffset(s.aactorImpl);
                if (uobjectFlags >= 0 && uobjectFlags == aactorFlags)
                    s.flagsOffset = uobjectFlags;
            }
        }
    }

    int count = EngineDiscovery::GetObjectCount(g_State);
    int scanned = 0;
    int actorScanned = 0;

    const int64_t stride = (count > kMaxScan) ? (count / kMaxScan) : 1;
    for (int64_t i = 0; i < count && (scanned + actorScanned) < kMaxScan; i += stride) {
        void* obj = EngineDiscovery::GetObjectByIndex(g_State, static_cast<int32_t>(i));
        if (!obj) continue;
        void** vft = nullptr;
        if (!SafeMem_Read(obj, &vft, sizeof(void*)) || !vft) continue;

        UClass* cls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj));
        const bool isActor =
            cls && g_State.AActorClass &&
            EngineDiscovery::IsStructA(g_State, reinterpret_cast<UStruct*>(cls),
                                       reinterpret_cast<UStruct*>(g_State.AActorClass));
        if (isActor) ++actorScanned; else ++scanned;

        for (int slot = lo; slot <= hi; ++slot) {
            void* fn = nullptr;
            if (!SafeMem_Read(&vft[slot], &fn, sizeof(void*)) || !fn) continue;
            SlotStats& s = stats[slot];
            if (isActor) ++s.actorReads; else ++s.objectReads;

            if (s.derived) {
                const uintptr_t predicted =
                    isActor ? reinterpret_cast<uintptr_t>(s.aactorImpl)
                            : reinterpret_cast<uintptr_t>(s.uobjectImpl);
                if (reinterpret_cast<uintptr_t>(fn) == predicted) {
                    if (isActor) ++s.actorMatch; else ++s.objectMatch;
                } else {
                    ++s.unexplained;
                }
                continue;
            }
            (isActor ? actorFreqBySlot : objectFreqBySlot)[slot][reinterpret_cast<uintptr_t>(fn)]++;
        }
    }

    for (auto& kv : stats) {
        SlotStats& s = kv.second;
        if (s.derived) continue;
        auto itObjects = objectFreqBySlot.find(kv.first);
        if (itObjects != objectFreqBySlot.end()) {
            for (const auto& f : itObjects->second) {
                if (f.second > s.objectMajorityCount) {
                    s.objectMajorityCount = f.second;
                    s.objectMajority = f.first;
                }
            }
        }
        auto itActors = actorFreqBySlot.find(kv.first);
        if (itActors != actorFreqBySlot.end()) {
            for (const auto& f : itActors->second) {
                if (f.second > s.actorMajorityCount) {
                    s.actorMajorityCount = f.second;
                    s.actorMajority = f.first;
                }
            }
        }
    }

    auto PercentTenths = [](int part, int whole) -> int {
        return whole > 0 ? (part * 1000) / whole : 0;
    };

    constexpr int kMinPopulationPercent = 70;

    auto lawHolds = [](const SlotStats& s) -> bool {
        return s.derived && s.branchOffset >= 0 && s.flagsOffset >= 0;
    };
    int bestLawSlot = -1;
    int bestLawScore = -1;
    for (const auto& kv : stats) {
        const SlotStats& s = kv.second;
        if (!lawHolds(s)) continue;
        const int lawScore = s.objectMatch + s.actorMatch;
        if (lawScore > bestLawScore) {
            bestLawScore = lawScore;
            bestLawSlot = kv.first;
        }
    }
    const bool lawAvailable = bestLawSlot >= 0;

    auto passesConsensus = [lawAvailable, &lawHolds](const SlotStats& s) -> bool {
        if (lawAvailable && !lawHolds(s)) return false;

        const int classified = s.objectReads + s.actorReads;
        if (classified < kMinReads) return false;

        if (s.derived) {
            if (s.objectReads > 0 &&
                s.objectMatch * 100 < kMinPopulationPercent * s.objectReads) return false;
            if (s.actorReads > 0 &&
                s.actorMatch * 100 < kMinPopulationPercent * s.actorReads) return false;
            return true;
        }

        if (s.objectReads > 0 &&
            s.objectMajorityCount * 100 < kMinPopulationPercent * s.objectReads) return false;
        if (s.actorReads > 0 &&
            s.actorMajorityCount * 100 < kMinPopulationPercent * s.actorReads) return false;

        if (s.actorReads >= 10 && s.objectReads >= 10 &&
            s.actorMajority == s.objectMajority) return false;
        return true;
    };

    auto score = [](const SlotStats& s) -> int {
        return s.derived ? (s.objectMatch + s.actorMatch)
                         : (s.objectMajorityCount + s.actorMajorityCount);
    };

    auto describe = [&PercentTenths](const SlotStats& s) -> std::string {
        char buf[640];
        if (s.derived) {
            const int classified = s.objectReads + s.actorReads;
            const int matched = s.objectMatch + s.actorMatch;
            const int totalPct = PercentTenths(matched, classified);
            const int objectPct = PercentTenths(s.objectMatch, s.objectReads);
            const int actorPct = PercentTenths(s.actorMatch, s.actorReads);
            snprintf(buf, sizeof(buf),
                     "%d/%d objects (%d.%d%%) carry the implementation their class "
                     "chain predicts - non-AActor %d/%d (%d.%d%%) on %p (UObject's "
                     "implementation), AActor %d/%d (%d.%d%%) on %p "
                     "(AActor::ProcessEvent)%s",
                     matched, classified, totalPct / 10, totalPct % 10, s.objectMatch,
                     s.objectReads, objectPct / 10, objectPct % 10, s.uobjectImpl,
                     s.actorMatch, s.actorReads, actorPct / 10, actorPct % 10,
                     s.aactorImpl, s.unexplained ? " - the rest carry neither" : "");
            return std::string(buf);
        }
        const int objectPct = PercentTenths(s.objectMajorityCount, s.objectReads);
        if (s.actorReads > 0) {
            const int actorPct = PercentTenths(s.actorMajorityCount, s.actorReads);
            snprintf(buf, sizeof(buf),
                     "%d.%d%% of %d non-AActor objects agree on %p, %d.%d%% of %d "
                     "AActor objects agree on %p (class defaults unread, so these are "
                     "the sample's own majorities)",
                     objectPct / 10, objectPct % 10, s.objectReads,
                     reinterpret_cast<void*>(s.objectMajority), actorPct / 10,
                     actorPct % 10, s.actorReads,
                     reinterpret_cast<void*>(s.actorMajority));
            return std::string(buf);
        }
        snprintf(buf, sizeof(buf), "%d.%d%% of %d objects agree on %p", objectPct / 10,
                 objectPct % 10, s.objectReads,
                 reinterpret_cast<void*>(s.objectMajority));
        return std::string(buf);
    };

    auto fill = [](const SlotStats& s, ProcessEventImplementations& out) {
        if (s.derived) {
            out.UObjectImpl = s.uobjectImpl;
            out.AActorImpl = s.aactorImpl;
            out.Derived = true;
        } else {
            out.UObjectImpl = reinterpret_cast<void*>(s.objectMajority);
            out.AActorImpl = reinterpret_cast<void*>(s.actorMajority);
            out.Derived = false;
        }
    };

    const int resolvedSlot = g_State.ProcessEvent_VTableIdx;
    auto itCurrent = stats.find(resolvedSlot);
    const SlotStats current = (itCurrent != stats.end()) ? itCurrent->second : SlotStats{};

    if (current.derived) {
        char msg[640];
        snprintf(msg, sizeof(msg),
                 "[LuaHook] ProcessEvent slot %d: UObject's implementation %p (read from "
                 "the UObject class default) and %p (Actor.h's override, read from the "
                 "AActor class default) - the hook goes on UObject's implementation, "
                 "which AActor::ProcessEvent's Super::ProcessEvent call reaches for "
                 "every actor",
                 resolvedSlot, current.uobjectImpl, current.aactorImpl);
        EngineDiscovery::WriteLogLine(msg);

        const int funnel = current.branchOffset;
        if (funnel >= 0) {
            snprintf(msg, sizeof(msg),
                     "[LuaHook] ProcessEvent slot %d: AActor::ProcessEvent %p branches to "
                     "UObject's implementation %p at +0x%X (Actor.cpp's Super::ProcessEvent) "
                     "- the hook on the implementation observes an actor's dispatch through it",
                     resolvedSlot, current.aactorImpl, current.uobjectImpl, funnel);
            EngineDiscovery::WriteLogLine(msg);
        } else {
            snprintf(msg, sizeof(msg),
                     "[LuaHook] ProcessEvent slot %d: AActor::ProcessEvent %p has no direct "
                     "branch to UObject's implementation %p in its first 0x400 bytes - an "
                     "actor's dispatch does not reach the hook through it on this build",
                     resolvedSlot, current.aactorImpl, current.uobjectImpl);
            EngineDiscovery::WriteLogLine(msg);
            if (lawAvailable) {
                snprintf(msg, sizeof(msg),
                         "[LuaHook] ProcessEvent slot %d: the engine's laws do not hold here - "
                         "%s, so this index is not the slot Object.h declares and Actor.h "
                         "overrides (slot %d carries the pair that satisfies them)",
                         resolvedSlot,
                         current.flagsOffset < 0
                             ? "Script.h's FUNC_Native test is not in both bodies"
                             : "the override has no branch into the implementation",
                         bestLawSlot);
                EngineDiscovery::WriteLogLine(msg);
            }
        }
    }

    if (passesConsensus(current)) {
        char msg[768];
        snprintf(msg, sizeof(msg), "[LuaHook] ProcessEvent slot %d consensus OK: %s",
                 resolvedSlot, describe(current).c_str());
        EngineDiscovery::WriteLogLine(msg);
        fill(current, impls);
        g_State.ProcessEvent_UObjectImpl = impls.UObjectImpl;
        g_State.ProcessEvent_AActorImpl = impls.AActorImpl;
        if (current.flagsOffset > 0) g_State.UFunction_FlagsOff = current.flagsOffset;
        return impls.UObjectImpl;
    }

    std::vector<std::pair<int, int>> ranked;
    for (const auto& kv : stats) {
        if (kv.first == resolvedSlot) continue;
        if (!passesConsensus(kv.second)) continue;
        ranked.emplace_back(score(kv.second), kv.first);
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    for (const auto& rv : ranked) {
        const SlotStats& s = stats[rv.second];
        char msg[1024];
        snprintf(msg, sizeof(msg),
                 "[LuaHook] ProcessEvent slot %d failed cross-object consensus (%s) - "
                 "recalibrated to slot %d (%s); fixing vtable dispatch for hooks and "
                 "direct calls",
                 resolvedSlot, describe(current).c_str(), rv.second,
                 describe(s).c_str());
        EngineDiscovery::WriteLogLine(msg);
        g_State.ProcessEvent_VTableIdx = rv.second;
        fill(s, impls);
        g_State.ProcessEvent_UObjectImpl = impls.UObjectImpl;
        g_State.ProcessEvent_AActorImpl = impls.AActorImpl;
        if (s.flagsOffset > 0) g_State.UFunction_FlagsOff = s.flagsOffset;
        return impls.UObjectImpl;
    }

    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < count && i < 500; ++i) {
            void* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
            if (!obj) continue;
            UClass* cls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj));
            const bool isActor =
                cls && g_State.AActorClass &&
                EngineDiscovery::IsStructA(g_State, reinterpret_cast<UStruct*>(cls),
                                           reinterpret_cast<UStruct*>(g_State.AActorClass));
            if ((pass == 0) == isActor) continue;
            void** vft = nullptr;
            if (SafeMem_Read(obj, &vft, sizeof(void*)) && vft) {
                void* peFunc = nullptr;
                if (SafeMem_Read(&vft[g_State.ProcessEvent_VTableIdx], &peFunc, sizeof(void*)) && peFunc) {
                    impls.UObjectImpl = peFunc;
                    impls.Derived = false;
                    return peFunc;
                }
            }
        }
    }
    return nullptr;
}

static bool IsInterpretedFunction(void* ufunction) {
    if (!ufunction || !g_State.ProcessInternal || g_State.UFunction_FuncOff < 0) return false;
    uintptr_t func = 0;
    if (!SafeMem_Read(reinterpret_cast<const uint8_t*>(ufunction) + g_State.UFunction_FuncOff,
                      &func, sizeof(func)))
        return false;
    if (func != reinterpret_cast<uintptr_t>(g_State.ProcessInternal)) return false;
    if (g_State.UFunction_FlagsOff >= 0) {
        uint32_t flags = 0;
        if (SafeMem_Read(reinterpret_cast<const uint8_t*>(ufunction) + g_State.UFunction_FlagsOff,
                         &flags, sizeof(flags)))
            return (flags & kFuncNative) == 0;
    }
    return true;
}

static void ReportHookInstallFailure(const char* what, void* target, const std::string& reason,
                                     const char* consequence) {
    if (!target) return;

    char block[512];
    snprintf(block, sizeof(block), "[LuaHook] %s hook install failed: %s at %p",
             what, reason.c_str(), target);
    EngineDiscovery::WriteLogLine(block);

    uint8_t bytes[16] = {};
    if (SafeMem_Read(target, bytes, sizeof(bytes))) {
        int used = snprintf(block, sizeof(block), "[LuaHook] %s target bytes:", what);
        for (size_t i = 0; i < sizeof(bytes) && used > 0 && static_cast<size_t>(used) + 4 < sizeof(block); ++i) {
            used += snprintf(block + used, sizeof(block) - static_cast<size_t>(used), " %02X", bytes[i]);
        }
        EngineDiscovery::WriteLogLine(block);
    }

    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(target, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        const bool bExecutable = (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
        snprintf(block, sizeof(block),
                 "[LuaHook] %s target region: base %p size 0x%llX state 0x%X protection 0x%X (%s)",
                 what, mbi.BaseAddress, static_cast<unsigned long long>(mbi.RegionSize),
                 static_cast<unsigned>(mbi.State), static_cast<unsigned>(mbi.Protect),
                 bExecutable ? "executable" : "NOT executable");
        EngineDiscovery::WriteLogLine(block);
    }
}

static void ReportPlacementNotes(const char* what, const Avalonium::InlineHook& patch) {
    if (patch.PlacementNotes().empty()) return;

    char msg[1024];
    snprintf(msg, sizeof(msg), "[LuaHook] %s: %s", what, patch.PlacementNotes().c_str());
    EngineDiscovery::WriteLogLine(msg);
}

bool LuaHookManager::Initialize(lua_State* L) {
    m_L = L;
    m_MainModThreadId = std::this_thread::get_id();

    ProcessEventImplementations impls;
    void* peAddress = GetProcessEventAddress(impls);
    if (!m_HookInstalled && peAddress) {
        m_HookedTargetAddress = peAddress;

        std::string installReason;
        if (m_ProcessEventPatch.Install(peAddress, reinterpret_cast<void*>(&Hooked_ProcessEvent),
                                        reinterpret_cast<void**>(&m_OriginalProcessEvent),
                                        &installReason)) {
            m_HookInstalled = true;
            char msg[640];
            snprintf(msg, sizeof(msg),
                     "[LuaHook] ProcessEvent hooked at %p (absolute-jump inline patch): %s",
                     peAddress, m_ProcessEventPatch.Summary().c_str());
            EngineDiscovery::WriteLogLine(msg);
            ReportPlacementNotes("ProcessEvent", m_ProcessEventPatch);

            {
                const int totalObjects = EngineDiscovery::GetObjectCount(g_State);
                const int64_t auditStride = (totalObjects > 3000) ? (totalObjects / 3000) : 1;
                int objectOnHook = 0, objectReads = 0;
                int actorOnHook = 0, actorOnOverride = 0, actorReads = 0;
                int other = 0;
                void* firstOther = nullptr;
                int sampled = 0;
                for (int64_t i = 0; i < totalObjects && sampled < 3000; i += auditStride) {
                    void* obj = EngineDiscovery::GetObjectByIndex(g_State, static_cast<int32_t>(i));
                    if (!obj) continue;
                    void** vft = nullptr;
                    if (!SafeMem_Read(obj, &vft, sizeof(void*)) || !vft) continue;
                    void* fn = nullptr;
                    if (!SafeMem_Read(&vft[g_State.ProcessEvent_VTableIdx], &fn, sizeof(void*)) || !fn) continue;

                    UClass* cls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj));
                    const bool isActor =
                        cls && g_State.AActorClass &&
                        EngineDiscovery::IsStructA(g_State, reinterpret_cast<UStruct*>(cls),
                                                   reinterpret_cast<UStruct*>(g_State.AActorClass));
                    ++sampled;
                    if (fn == peAddress) {
                        if (isActor) {
                            ++actorOnHook;
                            ++actorReads;
                        } else {
                            ++objectOnHook;
                            ++objectReads;
                        }
                        continue;
                    }
                    if (isActor) ++actorReads; else ++objectReads;
                    if (isActor && impls.AActorImpl && fn == impls.AActorImpl) {
                        ++actorOnOverride;
                    } else {
                        ++other;
                        if (!firstOther) firstOther = fn;
                    }
                }

                char dmsg[640];
                if (impls.AActorImpl && other == 0) {
                    snprintf(dmsg, sizeof(dmsg),
                             "[LuaHook] ProcessEvent vtable audit: %d/%d non-AActor objects carry the hooked implementation, %d/%d AActor objects carry %p (AActor::ProcessEvent - Actor.h's only override) whose Super::ProcessEvent call lands on it - all %d sampled objects route into the hook",
                             objectOnHook, objectReads, actorOnOverride, actorReads,
                             impls.AActorImpl, sampled);
                } else if (impls.AActorImpl) {
                    snprintf(dmsg, sizeof(dmsg),
                             "[LuaHook] ProcessEvent vtable audit: %d/%d non-AActor objects carry the hooked implementation, %d/%d AActor objects route through %p, %d of %d objects carry neither implementation (first %p) - an override outside Object.h/Actor.h is not observed",
                             objectOnHook, objectReads, actorOnOverride, actorReads,
                             impls.AActorImpl, other, sampled, firstOther);
                } else if (other > 0) {
                    snprintf(dmsg, sizeof(dmsg),
                             "[LuaHook] ProcessEvent vtable audit: %d/%d non-AActor objects and %d/%d AActor objects on the hooked address %p, %d of %d objects on another address (first %p) - the AActor class default could not be read, so the alternatives are not attributed",
                             objectOnHook, objectReads, actorOnHook, actorReads, peAddress,
                             other, sampled, firstOther);
                } else {
                    snprintf(dmsg, sizeof(dmsg),
                             "[LuaHook] ProcessEvent vtable audit: %d/%d non-AActor objects and %d/%d AActor objects carry the hooked address %p",
                             objectOnHook, objectReads, actorOnHook, actorReads, peAddress);
                }
                EngineDiscovery::WriteLogLine(dmsg);
            }
        } else {
            char msg[384];
            snprintf(msg, sizeof(msg),
                     "[LuaHook] ProcessEvent could not be hooked inline (%s); installing the vtable hook fallback...",
                     installReason.c_str());
            EngineDiscovery::WriteLogLine(msg);
            ReportPlacementNotes("ProcessEvent", m_ProcessEventPatch);
            ReportHookInstallFailure("ProcessEvent", peAddress, installReason,
                                     "ProcessEvent is answered by the vtable fallback below");

            if (InstallVTableHook(g_State, impls.UObjectImpl, impls.AActorImpl,
                                  reinterpret_cast<void*>(&Hooked_ProcessEvent),
                                  reinterpret_cast<void*>(&Hooked_ProcessEvent_Actor),
                                  reinterpret_cast<void**>(&m_OriginalProcessEvent),
                                  reinterpret_cast<void**>(&m_OriginalActorProcessEvent))) {
                m_HookInstalled = true;
                m_IsVTableHook = true;
                char msg3[256];
                snprintf(msg3, sizeof(msg3), "[LuaHook] ProcessEvent VTable hook installed successfully (slot %d)", g_State.ProcessEvent_VTableIdx);
                EngineDiscovery::WriteLogLine(msg3);
            } else {
                EngineDiscovery::WriteLogLine("[LuaHook] Warning: Could not install ProcessEvent hook. Fallback timer heartbeat active.");
            }
        }
    }

    if (!m_ScriptHookInstalled) {
        void* plsf = g_State.ProcessLocalScriptFunction;
        if (!plsf && g_State.bIndexBuilt) {
            if (!g_State.ProcessInternal) {
                EngineDiscovery::FindProcessInternal(g_State);
            }
            EngineDiscovery::FindProcessLocalScriptFunction(g_State);
            plsf = g_State.ProcessLocalScriptFunction;
        }
        if (plsf) {
            m_HookedProcessLocalScriptFunctionAddress = plsf;
            std::string installReason;
            if (m_ScriptPatch.Install(plsf, reinterpret_cast<void*>(&Hooked_ProcessLocalScriptFunction),
                                      reinterpret_cast<void**>(&m_OriginalProcessLocalScriptFunction),
                                      &installReason)) {
                m_ScriptHookInstalled = true;
                char msg[768];
                snprintf(msg, sizeof(msg),
                         "[LuaHook] ProcessLocalScriptFunction hooked at %p (absolute-jump inline patch): %s; hooks on interpreted (Blueprint) functions and custom events are answered here",
                         plsf, m_ScriptPatch.Summary().c_str());
                EngineDiscovery::WriteLogLine(msg);
                ReportPlacementNotes("ProcessLocalScriptFunction", m_ScriptPatch);
            } else {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "[LuaHook] Warning: ProcessLocalScriptFunction could not be hooked inline (%s); hooks on interpreted (Blueprint) functions will fall back to ProcessEvent dispatch",
                         installReason.c_str());
                EngineDiscovery::WriteLogLine(msg);
                ReportPlacementNotes("ProcessLocalScriptFunction", m_ScriptPatch);
                ReportHookInstallFailure("ProcessLocalScriptFunction", plsf, installReason,
                                         "calls made from other Blueprint bytecode are not observed");
            }
        } else {
            EngineDiscovery::WriteLogLine(
                "[LuaHook] Warning: ProcessLocalScriptFunction was not resolved; hooks and custom "
                "events on Blueprint (interpreted) functions cannot fire");
        }
    }

    if (!m_TimerThreadRunning) {
        m_TimerThreadRunning = true;
        m_TimerThread = std::thread([this]() {
            m_AsyncThreadId = std::this_thread::get_id();
            while (m_TimerThreadRunning) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                PumpFallbackHeartbeat();
            }
        });
    }

    if (m_L) {
        RegisterLuaHookBindings(m_L);
    }
    return m_HookInstalled;
}

int64_t LuaHookManager::GetCurrentThreadId() {
    return static_cast<int64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0x7FFFFFFF);
}

int64_t LuaHookManager::GetMainModThreadId() const {
    return m_MainModThreadId != std::thread::id{}
        ? static_cast<int64_t>(std::hash<std::thread::id>{}(m_MainModThreadId) & 0x7FFFFFFF)
        : 0;
}

int64_t LuaHookManager::GetGameThreadId() const {
    return m_GameThreadIdCaptured
        ? static_cast<int64_t>(std::hash<std::thread::id>{}(m_GameThreadId) & 0x7FFFFFFF)
        : 0;
}

int64_t LuaHookManager::GetAsyncThreadId() const {
    return m_AsyncThreadId != std::thread::id{}
        ? static_cast<int64_t>(std::hash<std::thread::id>{}(m_AsyncThreadId) & 0x7FFFFFFF)
        : 0;
}

bool LuaHookManager::IsInGameThread() const {
    return m_GameThreadIdCaptured && std::this_thread::get_id() == m_GameThreadId;
}

bool LuaHookManager::IsInMainModThread() const {
    return m_MainModThreadId != std::thread::id{} && std::this_thread::get_id() == m_MainModThreadId;
}

bool LuaHookManager::IsInAsyncThread() const {
    return m_AsyncThreadId != std::thread::id{} && std::this_thread::get_id() == m_AsyncThreadId;
}

bool LuaHookManager::MayRunModCodeOnThisThread() const {

    return !m_GameThreadIdCaptured || std::this_thread::get_id() == m_GameThreadId;
}

static void ReportModCodeRefusedOffGameThread(const char* what) {
    static std::atomic<bool> reported{ false };
    bool expected = false;
    if (!reported.compare_exchange_strong(expected, true)) return;

    char msg[512];
    snprintf(msg, sizeof(msg),
             "[LuaHook] %s was queued for the game thread and is not run on this thread: the "
             "engine gives its per-thread state (the async-loading state a synchronous load "
             "flushes through) only to the thread it runs the game on, so the work waits for "
             "that thread's next ProcessEvent",
             what);
    EngineDiscovery::WriteLogLine(msg);
}

lua_State* LuaHookManager::LiveStateUnderLock() const {
    lua_State* live = LuaManager::Get().GetState();
    return (live && live == m_L) ? live : nullptr;
}

static std::unique_lock<std::recursive_mutex> LockLuaForHandOff() {
    LuaManager& manager = LuaManager::Get();
    std::recursive_mutex& state = manager.GetLuaMutex();
    std::unique_lock<std::recursive_mutex> lock(state, std::defer_lock);

    if (!manager.IsLoadingMods()) {
        lock.lock();
        return lock;
    }

    if (state.try_lock()) {
        lock = std::unique_lock<std::recursive_mutex>(state, std::adopt_lock);
    }
    return lock;
}

void LuaHookManager::Shutdown() {
    m_TimerThreadRunning = false;
    if (m_TimerThread.joinable()) {
        m_TimerThread.join();
    }

    ClearAllHooks();
    ClearAllTimers();

    m_ProcessEventPatch.Remove();
    m_ScriptPatch.Remove();
    m_ScriptHookInstalled = false;
    m_HookedProcessLocalScriptFunctionAddress = nullptr;
    m_HookInstalled = false;
    m_HookedTargetAddress = nullptr;

    m_L = nullptr;

    m_FrameSink.store(nullptr, std::memory_order_release);
    m_FrameSinkUser.store(nullptr, std::memory_order_release);
    m_CppScriptFunctionSink.store(nullptr, std::memory_order_release);
    {
        std::lock_guard<std::mutex> keyLock(m_KeyBindMutex);
        m_CppKeyBinds.clear();
    }

}

void LuaHookManager::UpdateFrameTiming() {
    auto now = std::chrono::high_resolution_clock::now();
    if (m_LastFrameTimestamp.time_since_epoch().count() > 0) {
        double elapsedMs = std::chrono::duration<double, std::milli>(now - m_LastFrameTimestamp).count();
        if (elapsedMs >= 0.2) {
            m_LastFrameTimeMs = elapsedMs;
            double instantFPS = (elapsedMs > 0.001) ? (1000.0 / elapsedMs) : 60.0;
            if (m_SmoothedFPS <= 0.0) {
                m_SmoothedFPS = instantFPS;
                m_SmoothedFrameTimeMs = elapsedMs;
            } else {
                m_SmoothedFPS = 0.95 * m_SmoothedFPS + 0.05 * instantFPS;
                m_SmoothedFrameTimeMs = 0.95 * m_SmoothedFrameTimeMs + 0.05 * elapsedMs;
            }
            m_LastFrameTimestamp = now;
        }
    } else {
        m_LastFrameTimestamp = now;
        m_LastFrameTimeMs = 16.666;
        m_SmoothedFrameTimeMs = 16.666;
        m_SmoothedFPS = 60.0;
    }
}

void LuaHookManager::ResolvePendingHooks() {
    std::unique_lock<std::recursive_mutex> lock(m_HookMutex, std::try_to_lock);
    if (!lock.owns_lock() || m_PendingNameHooks.empty() || !g_State.GUObjectArray) return;

    for (auto it = m_PendingNameHooks.begin(); it != m_PendingNameHooks.end(); ) {
        const std::string& target = it->first;
        void* foundFunc = EngineDiscovery::FindFunctionCached(g_State, target);

        if (foundFunc) {
            for (const auto& entry : it->second) {
                m_FunctionHooks[foundFunc].push_back(entry);
            }
            m_HasActiveHooks.store(true, std::memory_order_release);
            m_PendingNameWatches.erase(target);
            char msg[256];
            snprintf(msg, sizeof(msg), "[LuaHook] Deferred hook resolved on UFunction %p (%s)", foundFunc, target.c_str());
            EngineDiscovery::WriteLogLine(msg);
            it = m_PendingNameHooks.erase(it);
        } else {

            PendingNameWatch& watch = m_PendingNameWatches[target];
            if (watch.FirstSeenTick == 0) watch.FirstSeenTick = GetTickCount64();
            ++it;
        }
    }

    const uint64_t nowTick = GetTickCount64();
    constexpr uint64_t kUnresolvedReportDelayMs = 10000;
    for (auto it = m_PendingNameWatches.begin(); it != m_PendingNameWatches.end(); ) {
        const std::string& name = it->first;

        if (m_PendingNameHooks.find(name) == m_PendingNameHooks.end()) {
            it = m_PendingNameWatches.erase(it);
            continue;
        }
        if (!it->second.Reported && nowTick - it->second.FirstSeenTick >= kUnresolvedReportDelayMs) {
            it->second.Reported = true;
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "[LuaHook] Hook on '%s' has not resolved: no UFunction of that name is in "
                     "the object array. It stays registered for a class that loads later",
                     name.c_str());
            EngineDiscovery::WriteLogLine(msg);
        }
        ++it;
    }
}

void LuaHookManager::PumpFallbackHeartbeat() {

    uint64_t nowTick = GetTickCount64();
    uint64_t lastPE = s_LastProcessEventTick.load(std::memory_order_relaxed);

    if (m_HookInstalled && lastPE != 0 && (nowTick - lastPE < 100)) {
        return;
    }

    std::unique_lock<std::recursive_mutex> lock(m_HookMutex, std::try_to_lock);
    if (lock.owns_lock()) {
        UpdateFrameTiming();

        static uint64_t s_LastPendingResolveTick = 0;
        if (nowTick - s_LastPendingResolveTick >= 3000) {
            s_LastPendingResolveTick = nowTick;
            ResolvePendingHooks();
        }
    }
}

void LuaHookManager::ClearAllHooks() {
    std::lock_guard<std::recursive_mutex> lock(m_HookMutex);
    if (m_L) {
        for (auto& [func, list] : m_FunctionHooks) {
            for (auto& e : list) {
                if (e.PreCallbackRef != LUA_NOREF)  luaL_unref(m_L, LUA_REGISTRYINDEX, e.PreCallbackRef);
                if (e.PostCallbackRef != LUA_NOREF) luaL_unref(m_L, LUA_REGISTRYINDEX, e.PostCallbackRef);
            }
        }
        for (auto& [name, list] : m_PendingNameHooks) {
            for (auto& e : list) {
                if (e.PreCallbackRef != LUA_NOREF)  luaL_unref(m_L, LUA_REGISTRYINDEX, e.PreCallbackRef);
                if (e.PostCallbackRef != LUA_NOREF) luaL_unref(m_L, LUA_REGISTRYINDEX, e.PostCallbackRef);
            }
        }
        for (auto& [name, refs] : m_NewObjectListeners) {
            for (int ref : refs) luaL_unref(m_L, LUA_REGISTRYINDEX, ref);
        }
        for (auto& [cmd, refs] : m_ConsoleCommandHandlers) {
            for (int ref : refs) luaL_unref(m_L, LUA_REGISTRYINDEX, ref);
        }
    }
    {
        std::lock_guard<std::mutex> keyLock(m_KeyBindMutex);
        if (m_L) {
            for (auto& [name, refs] : m_KeyBinds) {
                for (int ref : refs) luaL_unref(m_L, LUA_REGISTRYINDEX, ref);
            }
        }
        m_KeyBinds.clear();
    }
    m_FunctionHooks.clear();
    m_PendingNameHooks.clear();
    m_PendingNameWatches.clear();
    m_KnownNonEventNameIndexes.clear();
    m_WorldNotices.clear();
    m_NewObjectListeners.clear();
    m_ConsoleCommandHandlers.clear();
    {
        std::lock_guard<std::recursive_mutex> hookLock(m_HookMutex);
        if (m_L) {
            for (const GlobalConsoleHandler& handler : m_GlobalConsoleHandlers) luaL_unref(m_L, LUA_REGISTRYINDEX, handler.CallbackRef);
        }
        m_GlobalConsoleHandlers.clear();
    }
    m_HasActiveHooks.store(false, std::memory_order_release);
}

void LuaHookManager::ClearAllTimers() {
    std::lock_guard<std::mutex> lock(m_TimerMutex);
    if (m_L) {
        for (auto& t : m_Timers) {
            if (t.CallbackRef != LUA_NOREF) luaL_unref(m_L, LUA_REGISTRYINDEX, t.CallbackRef);
        }
    }
    m_Timers.clear();
}

uint32_t LuaHookManager::RegisterHook(const std::string& functionName, int preCallbackRef, int postCallbackRef) {
    uint32_t preId = 0, postId = 0;
    RegisterHookCompat(functionName, preCallbackRef, postCallbackRef, &preId, &postId);
    return preId;
}

static UFunction* FindActorEventFunction(const char* eventName) {
    if (!eventName || !*eventName) return nullptr;

    UClass* actorClass = g_State.AActorClass;
    if (!actorClass || !g_State.UFunctionClass) return nullptr;

    void* found = EngineDiscovery::FindFunctionByName(
        g_State, reinterpret_cast<UStruct*>(actorClass), eventName);
    if (!found) return nullptr;

    if (EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(found)) !=
        reinterpret_cast<UClass*>(g_State.UFunctionClass)) {
        return nullptr;
    }
    return reinterpret_cast<UFunction*>(found);
}

static bool IsLevelWorld(UObject* world) {
    if (!world) return false;

    UClass* worldClass = EngineDiscovery::GetObjectClass(g_State, world);
    if (!worldClass) return false;

    const int32_t gameModeOff = ConsoleEnabler::FindPropertyOffset(
        g_State, reinterpret_cast<UStruct*>(worldClass), "AuthorityGameMode");
    if (gameModeOff < 0) {

        static bool s_ReportedMissingField = false;
        if (!s_ReportedMissingField) {
            s_ReportedMissingField = true;
            EngineDiscovery::WriteLogLine(
                "[LuaHook] UWorld::AuthorityGameMode was not found; every world that begins "
                "play announces a level load");
        }
        return true;
    }

    const uint8_t* field = reinterpret_cast<const uint8_t*>(world) + gameModeOff;
    if (!SafeMem_IsReadable(field, sizeof(void*))) return false;
    return SafeMem_ReadPtr(field) != nullptr;
}

static UObject* ResolveAuthorityGameMode(UObject* world, bool* bFieldFound = nullptr) {
    if (bFieldFound) *bFieldFound = false;
    if (!world) return nullptr;

    UClass* worldClass = EngineDiscovery::GetObjectClass(g_State, world);
    if (!worldClass) return nullptr;

    const int32_t gameModeOff = ConsoleEnabler::FindPropertyOffset(
        g_State, reinterpret_cast<UStruct*>(worldClass), "AuthorityGameMode");
    if (gameModeOff < 0) return nullptr;
    if (bFieldFound) *bFieldFound = true;

    const uint8_t* field = reinterpret_cast<const uint8_t*>(world) + gameModeOff;
    if (!SafeMem_IsReadable(field, sizeof(void*))) {
        if (bFieldFound) *bFieldFound = false;
        return nullptr;
    }
    return static_cast<UObject*>(SafeMem_ReadPtr(field));
}

static bool IsGameModeObject(UObject* object) {
    if (!object) return false;

    static UClass* s_GameModeBaseClass = nullptr;
    if (!s_GameModeBaseClass) {

        if (!g_State.bInitialized) return false;
        s_GameModeBaseClass = ConsoleEnabler::FindClassByNameExact(g_State, "GameModeBase");
        if (!s_GameModeBaseClass) {
            static bool s_ReportedMissing = false;
            if (!s_ReportedMissing) {
                s_ReportedMissing = true;
                EngineDiscovery::WriteLogLine(
                    "[LuaHook] AGameModeBase was not found in the reflection index; the gameplay "
                    "authority is matched by UWorld::AuthorityGameMode alone");
            }
            return false;
        }
    }
    return ConsoleEnabler::IsObjectA(g_State, object, s_GameModeBaseClass);
}

static bool NameIsTheSameFunction(const std::string& resolvedName, const std::string& requestedName) {
    if (_stricmp(resolvedName.c_str(), requestedName.c_str()) == 0) return true;

    const size_t colon = requestedName.rfind(':');
    const size_t dot = requestedName.rfind('.');
    const size_t separator = (colon == std::string::npos) ? dot
                           : (dot == std::string::npos)   ? colon
                                                          : (colon > dot ? colon : dot);
    if (separator == std::string::npos) return false;
    return _stricmp(resolvedName.c_str(), requestedName.c_str() + separator + 1) == 0;
}

static bool IsActorEventName(const char* functionName) {
    if (!functionName) return false;
    return _stricmp(functionName, "ReceiveBeginPlay") == 0 ||
           _stricmp(functionName, "ReceiveEndPlay") == 0;
}

static bool IsLevelLoadEventName(const char* functionName) {
    if (!functionName) return false;
    return _stricmp(functionName, "LoadMap") == 0 ||
           _stricmp(functionName, "Engine:LoadMap") == 0 ||
           _stricmp(functionName, "UEngine:LoadMap") == 0 ||
           _stricmp(functionName, "/Script/Engine.Engine:LoadMap") == 0;
}

void LuaHookManager::RegisterHookCompat(const std::string& functionName, int preCallbackRef, int postCallbackRef,
                                        uint32_t* outPreId, uint32_t* outPostId, bool bLoadMapHook,
                                        bool bInitGameStateHook) {
    std::lock_guard<std::recursive_mutex> lock(m_HookMutex);

    uint32_t hookId = m_NextHookId++;
    LuaHookEntry entry;
    entry.HookId = hookId;
    entry.PreCallbackRef = preCallbackRef;
    entry.PostCallbackRef = postCallbackRef;
    entry.FunctionName = functionName;

    entry.IsLoadMapHook = bLoadMapHook ||
        (_stricmp(functionName.c_str(), "WorldSettings:ReceiveBeginPlay") == 0);
    entry.IsInitGameStateHook = bInitGameStateHook;

    if (!entry.IsLoadMapHook && !entry.IsInitGameStateHook &&
        IsLevelLoadEventName(functionName.c_str()) &&
        !EngineDiscovery::FindFunctionCached(g_State, functionName)) {
        entry.IsLoadMapHook = true;

        if (entry.PreCallbackRef != LUA_NOREF && entry.PostCallbackRef == LUA_NOREF) {
            entry.PostCallbackRef = entry.PreCallbackRef;
            entry.PreCallbackRef = LUA_NOREF;
            preCallbackRef = LUA_NOREF;
            postCallbackRef = entry.PostCallbackRef;
        }

        char remapMsg[512];
        snprintf(remapMsg, sizeof(remapMsg),
                 "[LuaHook] '%s' names no UFunction in this engine (UEngine::LoadMap is a plain "
                 "virtual); filing the hook as the level-load post callback, once per loaded world",
                 functionName.c_str());
        EngineDiscovery::WriteLogLine(remapMsg);
    }

    void* actorBeginPlayFunc = reinterpret_cast<void*>(FindActorEventFunction("ReceiveBeginPlay"));

    void* targetFunc = nullptr;
    if (entry.IsLoadMapHook || entry.IsInitGameStateHook) {
        targetFunc = actorBeginPlayFunc;
    } else if (IsActorEventName(functionName.c_str())) {
        targetFunc = reinterpret_cast<void*>(FindActorEventFunction(functionName.c_str()));
    }
    if (!targetFunc) {
        targetFunc = EngineDiscovery::FindFunctionCached(g_State, functionName);
    }

    const bool bOnBeginPlayEvent = (!entry.IsLoadMapHook && !entry.IsInitGameStateHook &&
                                    targetFunc != nullptr && targetFunc == actorBeginPlayFunc);
    if (bOnBeginPlayEvent) entry.IsBeginPlayEventHook = true;

    if (outPreId)  *outPreId = preCallbackRef != LUA_NOREF ? hookId : 0;
    if (outPostId) *outPostId = postCallbackRef != LUA_NOREF ? (m_NextHookId++) : 0;
    uint32_t postRegId = outPostId ? *outPostId : 0;

    if (targetFunc) {
        m_FunctionHooks[targetFunc].push_back(entry);
        if (postRegId != 0) {
            m_FunctionHooks[targetFunc].back().PostId = postRegId;
        }

        if (entry.IsLoadMapHook || entry.IsInitGameStateHook || bOnBeginPlayEvent) {
            m_BeginPlayAnchorFunction = targetFunc;
        }
        m_HasActiveHooks.store(true, std::memory_order_release);

        char msg[512];
        snprintf(msg, sizeof(msg), "[LuaHook] (ID:%u) Registered hook on UFunction %p (%s) -> %s",
                 hookId, targetFunc, functionName.c_str(),
                 EngineDiscovery::BuildPathName(g_State, static_cast<UObject*>(targetFunc)).c_str());
        EngineDiscovery::WriteLogLine(msg);

        if (IsInterpretedFunction(targetFunc)) {

            const std::string hookedName =
                EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(targetFunc));
            std::string interpretedAs = hookedName.empty() ? functionName : hookedName;
            if (!hookedName.empty() && !NameIsTheSameFunction(hookedName, functionName)) {
                interpretedAs += " (filed for '" + functionName + "')";
            }

            if (m_ScriptHookInstalled) {
                snprintf(msg, sizeof(msg),
                         "[LuaHook] (ID:%u) %s is interpreted (Blueprint bytecode); answered after the "
                         "call by the ProcessLocalScriptFunction hook",
                         hookId, interpretedAs.c_str());
            } else {
                snprintf(msg, sizeof(msg),
                         "[LuaHook] (ID:%u) %s is interpreted (Blueprint bytecode); the interpreter "
                         "could not be hooked, so it is answered after the call by ProcessEvent "
                         "dispatch - calls made from other Blueprint bytecode will not reach it",
                         hookId, interpretedAs.c_str());
            }
            EngineDiscovery::WriteLogLine(msg);
        }
    } else {
        m_PendingNameHooks[functionName].push_back(entry);
        if (postRegId != 0) {
            m_PendingNameHooks[functionName].back().PostId = postRegId;
        }
        PendingNameWatch& watch = m_PendingNameWatches[functionName];
        if (watch.FirstSeenTick == 0) watch.FirstSeenTick = GetTickCount64();
        char msg[256];
        snprintf(msg, sizeof(msg), "[LuaHook] (ID:%u) Registered deferred hook for: %s",
                 hookId, functionName.c_str());
        EngineDiscovery::WriteLogLine(msg);
    }
}

bool LuaHookManager::UnregisterHookById(uint32_t id) {

    std::lock_guard<std::recursive_mutex> lock(m_HookMutex);
    bool found = false;

    auto eraseMatching = [&](std::vector<LuaHookEntry>& list) {
        auto it = list.begin();
        while (it != list.end()) {
            if (it->HookId == id || (id != 0 && it->PostId == id)) {
                if (m_L && it->PreCallbackRef != LUA_NOREF)  luaL_unref(m_L, LUA_REGISTRYINDEX, it->PreCallbackRef);
                if (m_L && it->PostCallbackRef != LUA_NOREF) luaL_unref(m_L, LUA_REGISTRYINDEX, it->PostCallbackRef);

                m_WorldNotices.erase(it->HookId);
                it = list.erase(it);
                found = true;
            } else {
                ++it;
            }
        }
    };

    for (auto& [func, list] : m_FunctionHooks) {
        eraseMatching(list);
    }
    for (auto& [name, list] : m_PendingNameHooks) {
        eraseMatching(list);
    }
    for (auto& [name, list] : m_CustomEvents) {
        eraseMatching(list);
    }
    for (auto& [event, list] : m_ExecLifecycleCallbacks) {
        eraseMatching(list);
    }
    m_KnownNonEventNameIndexes.clear();

    m_HasActiveHooks.store(!m_FunctionHooks.empty(), std::memory_order_release);
    return found;
}

bool LuaHookManager::UnregisterHook(uint32_t hookId) {
    return UnregisterHookById(hookId);
}

void LuaHookManager::RegisterCustomEvent(const std::string& eventName, int preCallbackRef, int postCallbackRef) {
    std::lock_guard<std::recursive_mutex> lock(m_HookMutex);

    std::string lower = eventName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    uint32_t hookId = m_NextHookId++;
    LuaHookEntry entry;
    entry.HookId = hookId;
    entry.PreCallbackRef = preCallbackRef;
    entry.PostCallbackRef = postCallbackRef;
    entry.FunctionName = eventName;
    entry.IsCustomEvent = true;

    m_CustomEvents[lower].push_back(entry);

    m_KnownNonEventNameIndexes.clear();
    m_HasActiveHooks.store(true, std::memory_order_release);

    char msg[256];
    snprintf(msg, sizeof(msg), "[LuaHook] (ID:%u) Registered custom event listener for: %s", hookId, eventName.c_str());
    EngineDiscovery::WriteLogLine(msg);
}

bool LuaHookManager::UnregisterCustomEvent(const std::string& eventName, int preCallbackRef, int postCallbackRef) {
    std::lock_guard<std::recursive_mutex> lock(m_HookMutex);

    std::string lower = eventName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    auto it = m_CustomEvents.find(lower);
    if (it == m_CustomEvents.end()) return false;

    bool found = false;
    auto rmIt = std::remove_if(it->second.begin(), it->second.end(), [&](const LuaHookEntry& e) {
        bool match = e.PreCallbackRef == preCallbackRef ||
                     (postCallbackRef != LUA_NOREF && e.PostCallbackRef == postCallbackRef);
        if (match) {
            if (m_L && e.PreCallbackRef != LUA_NOREF)  luaL_unref(m_L, LUA_REGISTRYINDEX, e.PreCallbackRef);
            if (m_L && e.PostCallbackRef != LUA_NOREF) luaL_unref(m_L, LUA_REGISTRYINDEX, e.PostCallbackRef);
            found = true;
        }
        return match;
    });
    it->second.erase(rmIt, it->second.end());
    if (it->second.empty()) m_CustomEvents.erase(it);
    m_KnownNonEventNameIndexes.clear();

    m_HasActiveHooks.store(!m_FunctionHooks.empty() || !m_CustomEvents.empty(), std::memory_order_release);
    return found;
}

void LuaHookManager::ClearAllCustomEvents() {
    std::lock_guard<std::recursive_mutex> lock(m_HookMutex);
    if (m_L) {
        for (auto& [name, list] : m_CustomEvents) {
            for (auto& e : list) {
                if (e.PreCallbackRef != LUA_NOREF)  luaL_unref(m_L, LUA_REGISTRYINDEX, e.PreCallbackRef);
                if (e.PostCallbackRef != LUA_NOREF) luaL_unref(m_L, LUA_REGISTRYINDEX, e.PostCallbackRef);
            }
        }
    }
    m_CustomEvents.clear();
    m_KnownNonEventNameIndexes.clear();
    m_HasActiveHooks.store(!m_FunctionHooks.empty(), std::memory_order_release);
}

bool LuaHookManager::UnregisterHookByName(const std::string& functionName) {
    std::lock_guard<std::recursive_mutex> lock(m_HookMutex);
    bool found = false;

    auto it = m_PendingNameHooks.find(functionName);
    if (it != m_PendingNameHooks.end()) {
        for (auto& e : it->second) {
            if (m_L && e.PreCallbackRef != LUA_NOREF)  luaL_unref(m_L, LUA_REGISTRYINDEX, e.PreCallbackRef);
            if (m_L && e.PostCallbackRef != LUA_NOREF) luaL_unref(m_L, LUA_REGISTRYINDEX, e.PostCallbackRef);
        }
        m_PendingNameHooks.erase(it);
        found = true;
    }

    for (auto& [func, list] : m_FunctionHooks) {
        auto rmIt = std::remove_if(list.begin(), list.end(), [&](const LuaHookEntry& e) {
            if (_stricmp(e.FunctionName.c_str(), functionName.c_str()) == 0) {
                if (m_L && e.PreCallbackRef != LUA_NOREF)  luaL_unref(m_L, LUA_REGISTRYINDEX, e.PreCallbackRef);
                if (m_L && e.PostCallbackRef != LUA_NOREF) luaL_unref(m_L, LUA_REGISTRYINDEX, e.PostCallbackRef);
                found = true;
                return true;
            }
            return false;
        });
        list.erase(rmIt, list.end());
    }

    m_HasActiveHooks.store(!m_FunctionHooks.empty(), std::memory_order_release);
    return found;
}

static std::string NormalizeKeyBindName(const std::string& key) {
    std::string norm;
    for (char c : key) {
        if (c != ' ' && c != '_' && c != '-') {
            norm.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(c))));
        }
    }
    return norm;
}

static std::string CanonicalModifierName(const std::string& mod) {
    std::string m = NormalizeKeyBindName(mod);
    if (m == "control" || m == "lcontrol" || m == "rcontrol" ||
        m == "17" || m == "162" || m == "163") return "ctrl";
    if (m == "lshift" || m == "rshift" ||
        m == "16" || m == "160" || m == "161") return "shift";
    if (m == "alt" || m == "lalt" || m == "ralt" || m == "menu" ||
        m == "18" || m == "164" || m == "165") return "alt";
    return m;
}

static void SplitModifierPrefix(std::string& key, bool& ctrl, bool& shift, bool& alt) {
    for (;;) {
        size_t plus = key.find('+');
        if (plus == std::string::npos) break;
        std::string token = CanonicalModifierName(key.substr(0, plus));
        if (token == "ctrl") ctrl = true;
        else if (token == "shift") shift = true;
        else if (token == "alt") alt = true;
        else break;
        key = key.substr(plus + 1);
    }
}

static std::string BuildCompositeKeyName(const std::string& keyName, bool ctrl, bool shift, bool alt) {
    std::string composite;
    if (ctrl)  composite += "ctrl+";
    if (shift) composite += "shift+";
    if (alt)   composite += "alt+";
    composite += NormalizeKeyBindName(keyName);
    return composite;
}

static std::string VkToCanonicalName(int vk) {
    if (vk >= VK_F1 && vk <= VK_F24) return "F" + std::to_string(vk - VK_F1 + 1);
    if (vk >= 'A' && vk <= 'Z') return std::string(1, static_cast<char>(vk));
    if (vk >= '0' && vk <= '9') return std::string(1, static_cast<char>(vk));
    switch (vk) {
        case VK_SPACE:   return "Space";
        case VK_RETURN:  return "Enter";
        case VK_TAB:     return "Tab";
        case VK_ESCAPE:  return "Escape";
        case VK_BACK:    return "Backspace";
        case VK_INSERT:  return "Insert";
        case VK_DELETE:  return "Delete";
        case VK_HOME:    return "Home";
        case VK_END:     return "End";
        case VK_PRIOR:   return "PageUp";
        case VK_NEXT:    return "PageDown";
        case VK_CAPITAL: return "CapsLock";
        case VK_UP:      return "Up";
        case VK_DOWN:    return "Down";
        case VK_LEFT:    return "Left";
        case VK_RIGHT:   return "Right";
        case VK_OEM_3:   return "Tilde";
        case VK_OEM_MINUS: return "Minus";
        case VK_OEM_PLUS:  return "Plus";
        case VK_OEM_4:   return "BracketLeft";
        case VK_OEM_6:   return "BracketRight";
        case VK_OEM_1:   return "Semicolon";
        case VK_OEM_7:   return "Quote";
        case VK_OEM_COMMA: return "Comma";
        case VK_OEM_PERIOD: return "Period";
        case VK_OEM_2:   return "Slash";
        case VK_OEM_5:   return "Backslash";
        default: break;
    }
    return std::to_string(vk);
}

void LuaHookManager::RegisterKeyBindWithModifiers(const std::string& keyName, const std::vector<std::string>& modifierKeys, int callbackRef) {

    bool ctrl = false, shift = false, alt = false;
    for (const std::string& mod : modifierKeys) {
        std::string m = CanonicalModifierName(mod);
        if (m == "ctrl") ctrl = true;
        else if (m == "shift") shift = true;
        else if (m == "alt") alt = true;
    }

    std::string base = NormalizeKeyBindName(keyName);
    if (keyName.find('+') != std::string::npos) SplitModifierPrefix(base, ctrl, shift, alt);

    std::string composite = BuildCompositeKeyName(base, ctrl, shift, alt);
    std::lock_guard<std::mutex> lock(m_KeyBindMutex);
    m_KeyBinds[composite].push_back(callbackRef);
}

bool LuaHookManager::IsKeyBindRegistered(const std::string& keyName) {
    std::lock_guard<std::mutex> lock(m_KeyBindMutex);
    std::string normKey = NormalizeKeyBindName(keyName);
    auto it = m_KeyBinds.find(normKey);
    return it != m_KeyBinds.end() && !it->second.empty();
}

void LuaHookManager::RegisterKeyBind(const std::string& keyName, int callbackRef) {
    std::lock_guard<std::mutex> lock(m_KeyBindMutex);
    m_KeyBinds[NormalizeKeyBindName(keyName)].push_back(callbackRef);
}

void LuaHookManager::OnKeyPress(const std::string& keyName) {
    if (keyName.empty()) return;

    std::string base = NormalizeKeyBindName(keyName);
    bool ctrl = false, shift = false, alt = false;
    if (keyName.find('+') != std::string::npos) SplitModifierPrefix(base, ctrl, shift, alt);
    if (base.empty()) return;

    ctrl  = ctrl  || (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    shift = shift || (GetAsyncKeyState(VK_SHIFT)  & 0x8000) != 0;
    alt   = alt   || (GetAsyncKeyState(VK_MENU)   & 0x8000) != 0;

    const std::string combo = BuildCompositeKeyName(base, ctrl, shift, alt);

    std::vector<int> callbacks;
    std::vector<CppKeyBind> cppCallbacks;
    {
        std::lock_guard<std::mutex> lock(m_KeyBindMutex);

        const int virtualKey = ResolveVirtualKeyCode(base);
        const std::string virtualKeyName =
            virtualKey != 0 ? std::to_string(virtualKey) : std::string();

        auto collectCpp = [&](const std::string& key) {
            if (!cppCallbacks.empty()) return;
            auto it = m_CppKeyBinds.find(key);
            if (it != m_CppKeyBinds.end()) cppCallbacks = it->second;
        };
        collectCpp(combo);
        if (cppCallbacks.empty()) collectCpp(base);
        if (cppCallbacks.empty() && !virtualKeyName.empty()) collectCpp(virtualKeyName);

        auto collectLua = [&](const std::string& key) {
            if (!callbacks.empty()) return;
            auto it = m_KeyBinds.find(key);
            if (it != m_KeyBinds.end()) callbacks = it->second;
        };
        collectLua(combo);
        if (callbacks.empty()) collectLua(base);
        if (callbacks.empty() && !virtualKeyName.empty()) collectLua(virtualKeyName);

        if (callbacks.empty() && cppCallbacks.empty()) return;
    }

    for (const CppKeyBind& bind : cppCallbacks) {
        if (!bind.Callback) continue;
        try {
            bind.Callback(bind.User);
        } catch (...) {
            char buf[512];
            snprintf(buf, sizeof(buf), "[CppMod] C++ keybind error for '%s'", keyName.c_str());
            EngineDiscovery::WriteLogLine(buf);
        }
    }

    if (callbacks.empty()) return;

    auto luaLock = LockLuaForHandOff();
    lua_State* L = luaLock.owns_lock() ? LiveStateUnderLock() : nullptr;
    if (!L) return;

    for (int ref : callbacks) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            char buf[512];
            snprintf(buf, sizeof(buf), "[Lua] Keybind error for '%s': %s", keyName.c_str(), err ? err : "unknown");
            EngineDiscovery::WriteLogLine(buf);
            lua_pop(L, 1);
        }
    }
}

void LuaHookManager::SetFrameSink(FrameSink sink, void* user) {
    if (!sink) {
        m_FrameSink.store(nullptr, std::memory_order_release);
        m_FrameSinkUser.store(nullptr, std::memory_order_release);
        return;
    }
    m_FrameSinkUser.store(user, std::memory_order_release);
    m_FrameSink.store(sink, std::memory_order_release);
}

void LuaHookManager::SetCppProcessEventSink(CppProcessEventSink sink) {
    m_CppProcessEventSink.store(sink, std::memory_order_release);
}

void LuaHookManager::SetCppScriptFunctionSink(CppScriptFunctionSink sink) {
    m_CppScriptFunctionSink.store(sink, std::memory_order_release);
}

uint32_t LuaHookManager::RegisterCppKeyBind(const std::string& keyName,
                                            CppKeyBindCallback callback, void* user) {
    if (keyName.empty() || !callback) return 0;

    bool ctrl = false, shift = false, alt = false;
    std::string base = NormalizeKeyBindName(keyName);
    if (keyName.find('+') != std::string::npos) SplitModifierPrefix(base, ctrl, shift, alt);
    if (base.empty()) return 0;

    const std::string composite = BuildCompositeKeyName(base, ctrl, shift, alt);

    std::lock_guard<std::mutex> lock(m_KeyBindMutex);
    const uint32_t id = m_NextCppKeyBindId++;
    CppKeyBind bind;
    bind.Id = id;
    bind.Callback = callback;
    bind.User = user;
    m_CppKeyBinds[composite].push_back(bind);
    return id;
}

bool LuaHookManager::UnregisterCppKeyBind(uint32_t id) {
    std::lock_guard<std::mutex> lock(m_KeyBindMutex);
    for (auto it = m_CppKeyBinds.begin(); it != m_CppKeyBinds.end(); ++it) {
        auto& binds = it->second;
        const auto found = std::remove_if(binds.begin(), binds.end(),
                                          [id](const CppKeyBind& bind) { return bind.Id == id; });
        if (found != binds.end()) {
            binds.erase(found, binds.end());
            if (binds.empty()) m_CppKeyBinds.erase(it);
            return true;
        }
    }
    return false;
}

void LuaHookManager::UnregisterNewObjectListener(const std::string& className, int callbackRef) {
    std::lock_guard<std::recursive_mutex> lock(m_HookMutex);
    auto it = m_NewObjectListeners.find(className);
    if (it == m_NewObjectListeners.end()) return;
    auto rmIt = std::remove(it->second.begin(), it->second.end(), callbackRef);
    if (rmIt != it->second.end()) {
        it->second.erase(rmIt, it->second.end());
        if (m_L && callbackRef != LUA_NOREF) {
            luaL_unref(m_L, LUA_REGISTRYINDEX, callbackRef);
        }
    }
    if (it->second.empty()) m_NewObjectListeners.erase(it);
}

void LuaHookManager::RegisterNewObjectListener(const std::string& className, int callbackRef) {
    std::lock_guard<std::recursive_mutex> lock(m_HookMutex);
    m_NewObjectListeners[className].push_back(callbackRef);
}

void LuaHookManager::CheckNewObjects() {
    if (m_NewObjectListeners.empty() || !g_State.GUObjectArray) return;

    if (!MayRunModCodeOnThisThread()) {
        ReportModCodeRefusedOffGameThread("a new-object pass");
        return;
    }

    std::unique_lock<std::recursive_mutex> luaLock(LuaManager::Get().GetLuaMutex(), std::try_to_lock);
    lua_State* L = luaLock.owns_lock() ? LiveStateUnderLock() : nullptr;
    if (!L) return;

    int currentCount = EngineDiscovery::GetObjectCount(g_State);
    if (m_LastObjectCount == 0) {
        m_LastObjectCount = currentCount;
        return;
    }

    if (currentCount > m_LastObjectCount) {
        for (int i = m_LastObjectCount; i < currentCount; ++i) {
            void* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
            if (!obj) continue;

            UClass* cls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj));
            if (!cls) continue;

            std::string clsName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cls));
            std::string fullCls = EngineDiscovery::BuildFullName(g_State, static_cast<UObject*>(cls));

            for (const auto& [targetName, listeners] : m_NewObjectListeners) {
                if (_stricmp(clsName.c_str(), targetName.c_str()) == 0 ||
                    _stricmp(fullCls.c_str(), targetName.c_str()) == 0 ||
                    (fullCls.size() >= targetName.size() &&
                     _stricmp(fullCls.c_str() + (fullCls.size() - targetName.size()), targetName.c_str()) == 0))
                {
                    std::vector<int> listenersCopy = listeners;
                    for (int cbRef : listenersCopy) {
                        lua_rawgeti(L, LUA_REGISTRYINDEX, cbRef);
                        PushUObject(L, static_cast<UObject*>(obj));
                        if (lua_pcall(L, 1, 1, 0) == LUA_OK) {

                            if (lua_isboolean(L, -1) && lua_toboolean(L, -1)) {
                                lua_pop(L, 1);
                                UnregisterNewObjectListener(targetName, cbRef);
                                continue;
                            }
                            lua_pop(L, 1);
                        } else {
                            lua_pop(L, 1);
                        }
                    }
                }
            }
        }
        m_LastObjectCount = currentCount;
    }

    EngineDiscovery::DeltaUpdateFunctionCache(g_State);
}

void LuaHookManager::RegisterConsoleCommandHandler(const std::string& commandName, int callbackRef) {
    std::lock_guard<std::recursive_mutex> lock(m_HookMutex);
    std::string lowerCmd = commandName;
    std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), ::tolower);
    m_ConsoleCommandHandlers[lowerCmd].push_back(callbackRef);
}

void LuaHookManager::RegisterGlobalConsoleHandler(int callbackRef, const std::string& commandName) {
    std::lock_guard<std::recursive_mutex> lock(m_HookMutex);
    GlobalConsoleHandler handler;
    handler.CallbackRef = callbackRef;
    if (!commandName.empty()) {
        std::string lowerName = commandName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        handler.CommandName = lowerName;
    }
    m_GlobalConsoleHandlers.push_back(handler);
}

void LuaHookManager::RegisterExecLifecycleHook(const std::string& fnName, int preCallbackRef,
                                               int postCallbackRef, uint32_t* outPreId,
                                               uint32_t* outPostId) {
    std::lock_guard<std::recursive_mutex> lock(m_HookMutex);

    LuaHookEntry entry;
    entry.FunctionName = fnName;
    entry.HookId = m_NextHookId++;
    if (preCallbackRef != LUA_NOREF) entry.PreCallbackRef = preCallbackRef;
    if (postCallbackRef != LUA_NOREF) {
        entry.PostCallbackRef = postCallbackRef;
        entry.PostId = m_NextHookId++;
    }
    if (outPreId)  *outPreId = preCallbackRef != LUA_NOREF ? entry.HookId : 0;
    if (outPostId) *outPostId = postCallbackRef != LUA_NOREF ? entry.PostId : 0;
    m_ExecLifecycleCallbacks[fnName].push_back(entry);

    char msg[320];
    snprintf(msg, sizeof(msg),
             "[LuaHook] (ID:%u) Console-exec hook on '%s' (%s): fires when a command runs "
             "through this runtime's console dispatch; the engine member it names has no "
             "script binding in any supported version",
             entry.HookId, fnName.c_str(), preCallbackRef != LUA_NOREF ? "pre" : "post");
    EngineDiscovery::WriteLogLine(msg);
}

bool LuaHookManager::FireExecLifecycleCallbacks(bool post, const std::string& commandStr) {
    std::lock_guard<std::recursive_mutex> lock(m_HookMutex);
    if (m_ExecLifecycleCallbacks.empty()) return false;

    std::unique_lock<std::recursive_mutex> luaLock(LuaManager::Get().GetLuaMutex(), std::try_to_lock);
    lua_State* L = luaLock.owns_lock() ? LiveStateUnderLock() : nullptr;
    if (!L) return false;

    UObject* pc = ConsoleEnabler::FindPlayerController(g_State);
    UObject* world = EngineDiscovery::GetLiveWorld(g_State);

    bool claimed = false;
    for (auto& [event, list] : m_ExecLifecycleCallbacks) {
        for (const LuaHookEntry& entry : list) {
            const int cbRef = post ? entry.PostCallbackRef : entry.PreCallbackRef;
            if (cbRef == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, cbRef);
            if (!lua_isfunction(L, -1)) {
                lua_pop(L, 1);
                continue;
            }

            int nargs = 0;
            if (_stricmp(event.c_str(), "Exec") == 0) {

                PushUObject(L, pc);
                PushUObject(L, world);
                lua_pushstring(L, commandStr.c_str());
                CompatPushOutputDevice(L);
                nargs = 4;
            } else if (_stricmp(event.c_str(), "CallFunctionByNameWithArguments") == 0) {

                PushUObject(L, pc);
                lua_pushstring(L, commandStr.c_str());
                CompatPushOutputDevice(L);
                PushUObject(L, pc);
                lua_pushboolean(L, 0);
                nargs = 5;
            } else {

                PushUObject(L, pc);
                lua_pushstring(L, commandStr.c_str());
                lua_newtable(L);
                std::istringstream iss(commandStr);
                std::string token;
                int idx = 1;
                while (iss >> token) {
                    lua_pushstring(L, token.c_str());
                    lua_rawseti(L, -2, idx++);
                }
                CompatPushOutputDevice(L);
                PushUObject(L, pc);
                nargs = 5;
            }

            if (lua_pcall(L, nargs, 1, 0) == LUA_OK) {
                if (!post && lua_isboolean(L, -1) && lua_toboolean(L, -1)) claimed = true;
                lua_pop(L, 1);
            } else {
                const char* err = lua_tostring(L, -1);
                std::string logMsg = std::string("[LuaHook] Console-exec hook '") + event +
                                     "' error: " + (err ? err : "unknown");
                EngineDiscovery::WriteLogLine(logMsg.c_str());
                lua_pop(L, 1);
            }
        }
    }
    return claimed;
}

bool LuaHookManager::ExecuteConsoleCommand(const std::string& commandStr) {
    if (commandStr.empty() || !g_State.GUObjectArray) return false;

    std::string trimmed = commandStr;
    size_t spacePos = trimmed.find(' ');
    std::string baseCmd = (spacePos != std::string::npos) ? trimmed.substr(0, spacePos) : trimmed;
    std::string args = (spacePos != std::string::npos) ? trimmed.substr(spacePos + 1) : "";
    std::string lowerBase = baseCmd;
    std::transform(lowerBase.begin(), lowerBase.end(), lowerBase.begin(), ::tolower);

    if (FireExecLifecycleCallbacks(false, commandStr)) {
        FireExecLifecycleCallbacks(true, commandStr);
        return true;
    }

    const bool handled = DispatchConsoleCommand(commandStr, baseCmd, args, lowerBase);
    FireExecLifecycleCallbacks(true, commandStr);
    return handled;
}

bool LuaHookManager::DispatchConsoleCommand(const std::string& commandStr, const std::string& baseCmd,
                                            const std::string& args, const std::string& lowerBase) {
    bool handledByLua = false;
    {
        std::lock_guard<std::recursive_mutex> lock(m_HookMutex);

        std::unique_lock<std::recursive_mutex> luaLock(LuaManager::Get().GetLuaMutex(), std::try_to_lock);
        lua_State* L = luaLock.owns_lock() ? LiveStateUnderLock() : nullptr;

        if (L && !m_GlobalConsoleHandlers.empty()) {
            for (const GlobalConsoleHandler& handler : m_GlobalConsoleHandlers) {

                if (!handler.CommandName.empty() && handler.CommandName != lowerBase) continue;
                lua_rawgeti(L, LUA_REGISTRYINDEX, handler.CallbackRef);
                if (lua_isfunction(L, -1)) {

                    lua_pushstring(L, commandStr.c_str());
                    lua_newtable(L);
                    std::istringstream issG(args);
                    std::string tokG;
                    int idxG = 1;
                    while (issG >> tokG) {
                        lua_pushstring(L, tokG.c_str());
                        lua_rawseti(L, -2, idxG++);
                    }
                    CompatPushOutputDevice(L);
                    if (lua_pcall(L, 3, 1, 0) == LUA_OK) {
                        if (lua_isboolean(L, -1) && lua_toboolean(L, -1)) handledByLua = true;
                        lua_pop(L, 1);
                    } else {
                        const char* err = lua_tostring(L, -1);
                        std::string logMsg = std::string("[LuaHook] Global console handler error: ") + (err ? err : "unknown");
                        EngineDiscovery::WriteLogLine(logMsg.c_str());
                        lua_pop(L, 1);
                    }
                } else {
                    lua_pop(L, 1);
                }
            }
        }

        auto it = m_ConsoleCommandHandlers.find(lowerBase);
        if (it != m_ConsoleCommandHandlers.end() && !it->second.empty()) {
            handledByLua = true;
            for (int cbRef : it->second) {
                if (!L) break;
                lua_rawgeti(L, LUA_REGISTRYINDEX, cbRef);
                if (lua_isfunction(L, -1)) {

                    lua_pushstring(L, commandStr.c_str());
                    lua_newtable(L);
                    std::istringstream iss(args);
                    std::string tok;
                    int idx = 1;
                    while (iss >> tok) {
                        lua_pushstring(L, tok.c_str());
                        lua_rawseti(L, -2, idx++);
                    }
                    CompatPushOutputDevice(L);
                    lua_pushstring(L, args.c_str());
                    if (lua_pcall(L, 4, 0, 0) != LUA_OK) {
                        const char* err = lua_tostring(L, -1);
                        std::string logMsg = std::string("[LuaHook] Custom console command '") + baseCmd + "' error: " + (err ? err : "unknown");
                        EngineDiscovery::WriteLogLine(logMsg.c_str());
                        lua_pop(L, 1);
                    }
                } else {
                    lua_pop(L, 1);
                }
            }
        }
    }
    if (handledByLua) {
        return true;
    }

    std::wstring wcmd(commandStr.begin(), commandStr.end());
    FString fstr = { const_cast<wchar_t*>(wcmd.c_str()), static_cast<int32_t>(wcmd.size() + 1), static_cast<int32_t>(wcmd.size() + 1) };

    UObject* consoleObj = ConsoleEnabler::g_CachedConsoleObject;
    if (!consoleObj) {
        UClass* consoleClass = ConsoleEnabler::FindClassByNameExact(g_State, "Console");
        if (consoleClass) {
            consoleObj = ConsoleEnabler::FindFirstObjectOfClass(g_State, consoleClass, false);
        }
    }
    if (consoleObj) {
        UClass* cClass = EngineDiscovery::GetObjectClass(g_State, consoleObj);
        void* consoleCmdFunc = EngineDiscovery::FindFunctionByName(g_State, static_cast<UStruct*>(cClass), "ConsoleCommand");
        if (consoleCmdFunc) {
            struct {
                FString Cmd;
            } params = { fstr };
            if (EngineDiscovery::CallProcessEvent(g_State, consoleObj, consoleCmdFunc, &params)) {
                return true;
            }
        }
    }

    UObject* pc = ConsoleEnabler::FindPlayerController(g_State);
    if (!pc) {
        UClass* pcClass = nullptr;
        {
            std::lock_guard<std::mutex> lock(*g_State.ReflectionCacheMutex);
            pcClass = g_State.CachedPlayerControllerClass;
        }
        if (pcClass) {
            std::vector<UObject*> pcs = EngineDiscovery::FindObjectsByClassFast(g_State, pcClass);
            for (UObject* inst : pcs) {
                std::string oName = EngineDiscovery::GetObjectNameStr(g_State, inst);
                if (oName.find("Default__") == std::string::npos) {
                    pc = inst;
                    break;
                }
            }
        }
        if (!pc) {
            int count = EngineDiscovery::GetObjectCount(g_State);
            for (int i = 0; i < count; ++i) {
                void* o = EngineDiscovery::GetObjectByIndex(g_State, i);
                if (!o) continue;
                UClass* c = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(o));
                std::string cName = c ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(c)) : "";
                if (cName.find("PlayerController") != std::string::npos || cName.find("_Controller_") != std::string::npos) {
                    std::string oName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(o));
                    if (oName.find("Default__") == std::string::npos) {
                        pc = static_cast<UObject*>(o);
                        if (c) {
                            std::lock_guard<std::mutex> lock(*g_State.ReflectionCacheMutex);
                            g_State.CachedPlayerControllerClass = (UClass*)c;
                            g_State.ClassByName[std::string("PlayerController")] = (UClass*)c;
                        }
                        break;
                    }
                }
            }
        }
    }

    {
        UObject* ksCDO = LuaManager::FindEngineCDO("KismetSystemLibrary");
        if (ksCDO) {
            UClass* ksClass = EngineDiscovery::GetObjectClass(g_State, ksCDO);
            void* ksFunc = nullptr;
            {
                std::lock_guard<std::recursive_mutex> lock(*g_State.IndexMutex);
                auto it = g_State.FunctionByNameOnly.find("ExecuteConsoleCommand");
                if (it != g_State.FunctionByNameOnly.end()) ksFunc = it->second;
            }
            if (!ksFunc && ksClass) {
                ksFunc = EngineDiscovery::FindFunctionByName(g_State, static_cast<UStruct*>(ksClass), "ExecuteConsoleCommand");
            }
            if (ksFunc) {

                void* worldCtx = static_cast<void*>(EngineDiscovery::GetLiveWorld(g_State));
                if (worldCtx && !SafeMem_IsReadable(worldCtx, 8)) worldCtx = nullptr;
                if (!worldCtx) worldCtx = pc;
                if (!worldCtx) worldCtx = g_State.GEngine;
                if (!worldCtx) worldCtx = ksCDO;

                struct {
                    void* WorldContextObject;
                    FString Command;
                    void* SpecificPlayer;
                } ksParams = { worldCtx, fstr, pc };
                if (EngineDiscovery::CallProcessEvent(g_State, ksCDO, ksFunc, &ksParams)) {
                    return true;
                }
            }
        }
    }

    if (pc) {
        UClass* pcClass = EngineDiscovery::GetObjectClass(g_State, pc);
        void* clientCmdFunc = EngineDiscovery::FindFunctionByName(g_State, static_cast<UStruct*>(pcClass), "ClientConsoleCommand");
        if (clientCmdFunc) {
            struct {
                FString Cmd;
            } params = { fstr };
            if (EngineDiscovery::CallProcessEvent(g_State, pc, clientCmdFunc, &params)) {
                return true;
            }
        }

        void* consoleCmdFunc = EngineDiscovery::FindFunctionByName(g_State, static_cast<UStruct*>(pcClass), "ConsoleCommand");
        if (consoleCmdFunc) {
            struct {
                FString Cmd;
                bool bWriteToLog;
            } params = { fstr, true };
            if (EngineDiscovery::CallProcessEvent(g_State, pc, consoleCmdFunc, &params)) {
                return true;
            }
        }
    }

    if (pc) {
        int32_t cmOff = ConsoleEnabler::FindPropertyOffset(g_State, (UStruct*)EngineDiscovery::GetObjectClass(g_State, pc), "CheatManager");
        if (cmOff > 0) {
            void* cmObj = nullptr;
            if (SafeMem_Read(reinterpret_cast<uint8_t*>(pc) + cmOff, &cmObj, sizeof(void*)) && cmObj) {
                UClass* cmClass = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(cmObj));
                void* cmFunc = EngineDiscovery::FindFunctionByName(g_State, static_cast<UStruct*>(cmClass), baseCmd.c_str());
                if (cmFunc) {
                    return EngineDiscovery::CallProcessEvent(g_State, cmObj, cmFunc, nullptr);
                }
            }
        }
    }

    return false;
}

uint32_t LuaHookManager::RegisterBeginPlayHook(int preRef, int postRef) {
    return RegisterHook("ReceiveBeginPlay", preRef, postRef);
}

uint32_t LuaHookManager::RegisterEndPlayHook(int preRef, int postRef) {
    return RegisterHook("ReceiveEndPlay", preRef, postRef);
}

uint32_t LuaHookManager::RegisterLoadMapHook(int preRef, int postRef) {

    uint32_t preId = 0, postId = 0;
    RegisterHookCompat("ReceiveBeginPlay", preRef, postRef, &preId, &postId,
                       true);
    return preId;
}

uint32_t LuaHookManager::RegisterInitGameStateHook(int preRef, int postRef) {

    uint32_t preId = 0, postId = 0;
    RegisterHookCompat("ReceiveBeginPlay", preRef, postRef, &preId, &postId,
                       false, true);

    return preId != 0 ? preId : postId;
}

bool LuaHookManager::CancelTimer(uint32_t timerId) {
    std::lock_guard<std::mutex> lock(m_TimerMutex);
    for (auto& t : m_Timers) {
        if (t.TimerId == timerId && !t.Cancelled) {
            t.Cancelled = true;
            return true;
        }
    }
    return false;
}

uint32_t LuaHookManager::MakeActionHandle() {
    std::lock_guard<std::mutex> lock(m_TimerMutex);

    return m_NextTimerId++;
}

uint32_t LuaHookManager::ExecuteWithDelay(int64_t delayMs, int callbackRef) {
    std::lock_guard<std::mutex> lock(m_TimerMutex);
    LuaTimerEntry t;
    t.TimerId = m_NextTimerId++;
    t.CallbackRef = callbackRef;
    t.NextTriggerTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs > 0 ? delayMs : 0);
    t.IntervalMs = delayMs;
    t.IsLoop = false;
    m_Timers.push_back(t);
    return t.TimerId;
}

uint32_t LuaHookManager::ExecuteWithDelayUsingHandle(uint64_t userHandle, int64_t delayMs, int callbackRef, bool retriggerable) {
    std::lock_guard<std::mutex> lock(m_TimerMutex);

    for (auto& existing : m_Timers) {
        if (existing.TimerId == static_cast<uint32_t>(userHandle) && !existing.Cancelled) {
            if (retriggerable) {
                existing.NextTriggerTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs > 0 ? delayMs : 0);
                existing.Paused = false;
                if (existing.CallbackRef != LUA_NOREF && m_L) {
                    luaL_unref(m_L, LUA_REGISTRYINDEX, existing.CallbackRef);
                }
                existing.CallbackRef = callbackRef;
            }
            return existing.TimerId;
        }
    }

    LuaTimerEntry t;
    t.TimerId = static_cast<uint32_t>(userHandle);
    t.CallbackRef = callbackRef;
    t.NextTriggerTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs > 0 ? delayMs : 0);
    t.IntervalMs = delayMs;
    t.IsLoop = false;
    m_Timers.push_back(t);
    if (static_cast<uint32_t>(userHandle) >= m_NextTimerId) {
        m_NextTimerId = static_cast<uint32_t>(userHandle) + 1;
    }
    return t.TimerId;
}

uint32_t LuaHookManager::LoopAsync(int64_t intervalMs, int callbackRef) {
    std::lock_guard<std::mutex> lock(m_TimerMutex);
    LuaTimerEntry t;
    t.TimerId = m_NextTimerId++;
    t.CallbackRef = callbackRef;
    t.NextTriggerTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(intervalMs > 0 ? intervalMs : 0);
    t.IntervalMs = intervalMs;
    t.IsLoop = true;
    m_Timers.push_back(t);
    return t.TimerId;
}

uint32_t LuaHookManager::ExecuteAfterFrames(int64_t frames, int callbackRef) {
    std::lock_guard<std::mutex> lock(m_TimerMutex);
    LuaTimerEntry t;
    t.TimerId = m_NextTimerId++;
    t.CallbackRef = callbackRef;
    t.IsFrameBased = true;
    t.DelayFrames = frames > 0 ? frames : 1;
    t.FramesRemaining = t.DelayFrames;
    t.IsLoop = false;
    m_Timers.push_back(t);
    return t.TimerId;
}

uint32_t LuaHookManager::LoopAfterFrames(int64_t frames, int callbackRef) {
    std::lock_guard<std::mutex> lock(m_TimerMutex);
    LuaTimerEntry t;
    t.TimerId = m_NextTimerId++;
    t.CallbackRef = callbackRef;
    t.IsFrameBased = true;
    t.DelayFrames = frames > 0 ? frames : 1;
    t.FramesRemaining = t.DelayFrames;
    t.IsLoop = true;
    m_Timers.push_back(t);
    return t.TimerId;
}

static LuaTimerEntry* FindTimerLocked(std::vector<LuaTimerEntry>& timers, uint32_t timerId) {
    for (auto& t : timers) {
        if (t.TimerId == timerId && !t.Cancelled) return &t;
    }
    return nullptr;
}

bool LuaHookManager::PauseTimer(uint32_t timerId) {
    std::lock_guard<std::mutex> lock(m_TimerMutex);
    if (LuaTimerEntry* t = FindTimerLocked(m_Timers, timerId)) {
        if (!t->Paused) {
            if (!t->IsFrameBased) {
                auto now = std::chrono::steady_clock::now();
                if (t->NextTriggerTime > now) {
                    t->IntervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(t->NextTriggerTime - now).count();
                }
            }
            t->Paused = true;
            return true;
        }
    }
    return false;
}

bool LuaHookManager::UnpauseTimer(uint32_t timerId) {
    std::lock_guard<std::mutex> lock(m_TimerMutex);
    if (LuaTimerEntry* t = FindTimerLocked(m_Timers, timerId)) {
        if (t->Paused) {
            if (!t->IsFrameBased) {
                t->NextTriggerTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(t->IntervalMs > 0 ? t->IntervalMs : 0);
            }
            t->Paused = false;
            return true;
        }
    }
    return false;
}

bool LuaHookManager::ResetTimer(uint32_t timerId) {
    std::lock_guard<std::mutex> lock(m_TimerMutex);
    if (LuaTimerEntry* t = FindTimerLocked(m_Timers, timerId)) {
        if (t->IsFrameBased) {
            t->FramesRemaining = t->DelayFrames;
        } else {
            t->NextTriggerTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(t->IntervalMs > 0 ? t->IntervalMs : 0);
        }
        t->Paused = false;
        return true;
    }
    return false;
}

bool LuaHookManager::SetTimerDelay(uint32_t timerId, int64_t newDelayMs) {
    std::lock_guard<std::mutex> lock(m_TimerMutex);
    if (LuaTimerEntry* t = FindTimerLocked(m_Timers, timerId)) {
        if (t->IsFrameBased) {
            t->DelayFrames = newDelayMs;
            t->FramesRemaining = t->DelayFrames;
        } else {
            t->IntervalMs = newDelayMs;
            t->NextTriggerTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(newDelayMs > 0 ? newDelayMs : 0);
        }
        return true;
    }
    return false;
}

bool LuaHookManager::IsValidTimerHandle(uint32_t timerId) {
    std::lock_guard<std::mutex> lock(m_TimerMutex);
    return FindTimerLocked(m_Timers, timerId) != nullptr;
}

bool LuaHookManager::IsTimerActive(uint32_t timerId) {
    std::lock_guard<std::mutex> lock(m_TimerMutex);
    LuaTimerEntry* t = FindTimerLocked(m_Timers, timerId);
    return t && !t->Paused;
}

bool LuaHookManager::IsTimerPaused(uint32_t timerId) {
    std::lock_guard<std::mutex> lock(m_TimerMutex);
    LuaTimerEntry* t = FindTimerLocked(m_Timers, timerId);
    return t && t->Paused;
}

int64_t LuaHookManager::GetTimerTimeRemaining(uint32_t timerId) {
    std::lock_guard<std::mutex> lock(m_TimerMutex);
    LuaTimerEntry* t = FindTimerLocked(m_Timers, timerId);
    if (!t) return -1;
    if (t->IsFrameBased) return t->FramesRemaining;
    if (t->Paused) return t->IntervalMs;
    auto now = std::chrono::steady_clock::now();
    if (t->NextTriggerTime > now) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(t->NextTriggerTime - now).count();
    }
    return 0;
}

int64_t LuaHookManager::GetTimerTimeElapsed(uint32_t timerId) {
    int64_t remaining = GetTimerTimeRemaining(timerId);
    int64_t rate = GetTimerRate(timerId);
    if (remaining < 0 || rate < 0) return -1;
    return rate - remaining;
}

int64_t LuaHookManager::GetTimerRate(uint32_t timerId) {
    std::lock_guard<std::mutex> lock(m_TimerMutex);
    LuaTimerEntry* t = FindTimerLocked(m_Timers, timerId);
    if (!t) return -1;
    return t->IsFrameBased ? t->DelayFrames : t->IntervalMs;
}

int LuaHookManager::CancelAllTimers() {
    std::lock_guard<std::mutex> lock(m_TimerMutex);
    int count = 0;
    for (auto& t : m_Timers) {
        if (!t.Cancelled) {
            t.Cancelled = true;
            ++count;
        }
    }
    return count;
}

void LuaHookManager::ProcessTimers() {

    if (!MayRunModCodeOnThisThread()) {
        ReportModCodeRefusedOffGameThread("a timer tick");
        return;
    }

    std::unique_lock<std::recursive_mutex> luaLock(LuaManager::Get().GetLuaMutex(), std::try_to_lock);
    lua_State* L = luaLock.owns_lock() ? LiveStateUnderLock() : nullptr;

    m_FrameCounter.fetch_add(1, std::memory_order_relaxed);

    EngineDiscovery::AdvanceLiveLookupGeneration(g_State);

    if (!L) return;

    std::vector<LuaTimerEntry> dueTimers;
    auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(m_TimerMutex);
        for (auto& t : m_Timers) {
            if (t.Cancelled || t.Paused) continue;
            if (t.IsFrameBased) {
                if (t.FramesRemaining > 0) --t.FramesRemaining;
                if (t.FramesRemaining <= 0) {
                    dueTimers.push_back(t);
                    if (t.IsLoop) {
                        t.FramesRemaining = t.DelayFrames;
                    } else {
                        t.Cancelled = true;
                    }
                }
            } else if (now >= t.NextTriggerTime) {
                dueTimers.push_back(t);
                if (t.IsLoop) {
                    t.NextTriggerTime = now + std::chrono::milliseconds(t.IntervalMs);
                } else {
                    t.Cancelled = true;
                }
            }
        }
    }

    for (auto& t : dueTimers) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, t.CallbackRef);
        if (lua_pcall(L, 0, 1, 0) == LUA_OK) {

            if (t.IsLoop && lua_isboolean(L, -1) && lua_toboolean(L, -1)) {
                lua_pop(L, 1);
                CancelTimer(t.TimerId);
                continue;
            }
            lua_pop(L, 1);
        } else {
            const char* err = lua_tostring(L, -1);
            if (err) {
                char msg[512];
                snprintf(msg, sizeof(msg), "[Lua] Timer %u callback error: %s", t.TimerId, err);
                EngineDiscovery::WriteLogLine(msg);
            }
            lua_pop(L, 1);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_TimerMutex);
        auto it = std::remove_if(m_Timers.begin(), m_Timers.end(), [&](const LuaTimerEntry& t) {
            if (t.Cancelled) {
                if (t.CallbackRef != LUA_NOREF) {
                    luaL_unref(L, LUA_REGISTRYINDEX, t.CallbackRef);
                }
                return true;
            }
            return false;
        });
        m_Timers.erase(it, m_Timers.end());
    }
}

void LuaHookManager::EnqueueGameThreadTask(int callbackRef) {
    std::lock_guard<std::mutex> lock(m_TaskMutex);
    m_GameThreadQueue.push_back(callbackRef);
}

void LuaHookManager::ProcessGameThreadTasks() {

    if (!MayRunModCodeOnThisThread()) {
        ReportModCodeRefusedOffGameThread("a queued game-thread task");
        return;
    }

    std::unique_lock<std::recursive_mutex> luaLock(LuaManager::Get().GetLuaMutex(), std::try_to_lock);
    lua_State* L = luaLock.owns_lock() ? LiveStateUnderLock() : nullptr;
    if (!L) return;

    std::vector<int> tasks;
    {
        std::lock_guard<std::mutex> lock(m_TaskMutex);
        tasks.swap(m_GameThreadQueue);
    }

    for (int ref : tasks) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            char buf[512];
            snprintf(buf, sizeof(buf), "[Lua] GameThread task error: %s", err ? err : "unknown");
            EngineDiscovery::WriteLogLine(buf);
            lua_pop(L, 1);
        }
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
    }
}

static UObject* ResolveWorldForContext(UObject* context) {
    UObject* cur = context;
    while (cur) {
        UClass* cls = EngineDiscovery::GetObjectClass(g_State, cur);
        if (cls) {
            std::string cName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cls));
            if (cName == "World") {
                return cur;
            }
        }
        cur = EngineDiscovery::GetObjectOuter(g_State, cur);
    }
    return EngineDiscovery::GetLiveWorld(g_State);
}

BeginPlayRideAlong LuaHookManager::PrepareBeginPlayRideAlong(void* objectPtr, std::vector<LuaHookEntry>& entries) {
    BeginPlayRideAlong answer;
    UObject* object = static_cast<UObject*>(objectPtr);

    for (const LuaHookEntry& candidate : entries) {
        if (candidate.IsLoadMapHook) answer.HasLoadMapEntry = true;
        else if (candidate.IsInitGameStateHook) answer.HasGameModeEntry = true;
    }

    if (answer.HasLoadMapEntry) {

        UObject* world = ResolveWorldForContext(object);
        if (world && !IsLevelWorld(world)) world = nullptr;
        const std::string worldName = world
            ? EngineDiscovery::GetObjectNameStr(g_State, world) : std::string();
        for (LuaHookEntry& candidate : entries) {
            if (!candidate.IsLoadMapHook) continue;
            candidate.LoadMapNotify = false;
            if (!world) continue;
            WorldNotice& notice = m_WorldNotices[candidate.HookId];
            if (notice.Recorded && notice.World == world && notice.WorldName == worldName) continue;
            notice.Recorded = true;
            notice.World = world;
            notice.WorldName = worldName;
            candidate.LoadMapNotify = true;
        }
        answer.World = world;
    }

    if (answer.HasGameModeEntry) {

        UObject* world = answer.World ? static_cast<UObject*>(answer.World)
                                      : ResolveWorldForContext(object);
        if (world && !IsLevelWorld(world)) world = nullptr;
        bool bFieldFound = false;
        UObject* authority = world ? ResolveAuthorityGameMode(world, &bFieldFound) : nullptr;
        const bool isGameModeClass = IsGameModeObject(object);
        if (authority) {
            answer.MatchedByAuthorityField = true;
        } else if (isGameModeClass) {

            authority = static_cast<UObject*>(object);
        }
        const bool matchesAuthority = (authority != nullptr && authority == object);
        answer.IsGameModeBeginningPlay = matchesAuthority;
        if (authority) {
            answer.Authority = authority;
            answer.World = world;

            const std::string worldName = world
                ? EngineDiscovery::GetObjectNameStr(g_State, world) : std::string();
            for (LuaHookEntry& candidate : entries) {
                if (!candidate.IsInitGameStateHook) continue;
                candidate.AuthorityNotify = false;
                WorldNotice& notice = m_WorldNotices[candidate.HookId];
                if (notice.Recorded && notice.World == world && notice.WorldName == worldName) continue;
                notice.Recorded = true;
                notice.World = world;
                notice.WorldName = worldName;
                candidate.AuthorityNotify = true;
            }
        }

        static bool s_ReportedUnreadableField = false;
        if (isGameModeClass && world && !bFieldFound && !s_ReportedUnreadableField) {
            s_ReportedUnreadableField = true;
            EngineDiscovery::WriteLogLine(
                "[LuaHook] UWorld::AuthorityGameMode is not readable in this game; the gameplay "
                "authority is recognised by the game mode's class instead");
        }

        static bool s_ReportedForeignGameMode = false;
        if (isGameModeClass && authority != nullptr && authority != object && !s_ReportedForeignGameMode) {
            s_ReportedForeignGameMode = true;
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "[LuaHook] A game mode began play that is not the world's "
                     "UWorld::AuthorityGameMode: %s (world: %s)",
                     EngineDiscovery::GetObjectNameStr(g_State, object).c_str(),
                     world ? EngineDiscovery::GetObjectNameStr(g_State, world).c_str() : "<unknown>");
            EngineDiscovery::WriteLogLine(msg);
        }
    }

    return answer;
}

static bool FunctionCarriesActorEvent(void* ufunction, void* anchor) {
    if (!ufunction || !anchor || !g_State.bInitialized) return false;
    UStruct* declared = reinterpret_cast<UStruct*>(
        EngineDiscovery::GetObjectOuter(g_State, static_cast<UObject*>(ufunction)));
    UStruct* base = reinterpret_cast<UStruct*>(
        EngineDiscovery::GetObjectOuter(g_State, static_cast<UObject*>(anchor)));
    if (!declared || !base) return false;
    if (declared == base) return true;
    return EngineDiscovery::IsStructA(g_State, declared, base);
}

bool LuaHookManager::ClaimBeginPlayEntry(const LuaHookEntry& entry, void* object) {
    if (!entry.IsBeginPlayEventHook) return true;

    const uint64_t engineCall = s_EngineCall.load(std::memory_order_relaxed);
    BeginPlayAnswer& answered = m_BeginPlayAnswers[entry.HookId];
    if (answered.Object == object && answered.EngineCall == engineCall) return false;
    answered.Object = object;
    answered.EngineCall = engineCall;
    return true;
}

static UObject* ContextForEntry(const LuaHookEntry& entry, const BeginPlayRideAlong& ride,
                                void* dispatchedObject) {
    if (entry.IsInitGameStateHook && ride.Authority) {
        return static_cast<UObject*>(ride.Authority);
    }
    return static_cast<UObject*>(dispatchedObject);
}

static void WriteRideAlongNotice(const LuaHookEntry& entry, UObject* object, UObject* world,
                                 bool bMatchedByAuthorityField) {
    char notice[512];
    if (entry.IsLoadMapHook) {
        UClass* carrierClass = EngineDiscovery::GetObjectClass(g_State, object);
        snprintf(notice, sizeof(notice),
                 "[LuaHook] Level begin-play pass reached: %s (first actor: %s)",
                 world ? EngineDiscovery::GetObjectNameStr(g_State, world).c_str() : "<unknown>",
                 carrierClass ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(carrierClass)).c_str() : "<unknown>");
    } else {
        snprintf(notice, sizeof(notice),
                 "[LuaHook] Gameplay authority begin-play: %s (world: %s, matched by %s)",
                 EngineDiscovery::GetObjectNameStr(g_State, object).c_str(),
                 world ? EngineDiscovery::GetObjectNameStr(g_State, world).c_str() : "<unknown>",
                 bMatchedByAuthorityField ? "UWorld::AuthorityGameMode" : "the game mode's class");
    }
    EngineDiscovery::WriteLogLine(notice);
}

void __fastcall LuaHookManager::Hooked_ProcessEvent_Actor(void* object, void* ufunction, void* params) {
    struct Mark {
        bool Previous = s_ArrivedViaActorVtable;
        Mark() { s_ArrivedViaActorVtable = true; }
        ~Mark() { s_ArrivedViaActorVtable = Previous; }
    } mark;
    Hooked_ProcessEvent(object, ufunction, params);
}

void __fastcall LuaHookManager::Hooked_ProcessEvent(void* object, void* ufunction, void* params) {
    auto& mgr = LuaHookManager::Get();

    typedef void (__fastcall* PE_t)(void*, void*, void*);
    PE_t origPE = reinterpret_cast<PE_t>(mgr.m_OriginalProcessEvent);
    if (s_ArrivedViaActorVtable && mgr.m_OriginalActorProcessEvent)
        origPE = reinterpret_cast<PE_t>(mgr.m_OriginalActorProcessEvent);
    if (!origPE) return;

    if (!object || !ufunction) {
        origPE(object, ufunction, params);
        return;
    }

    if (reinterpret_cast<uintptr_t>(ufunction) < 0x10000 ||
        reinterpret_cast<uintptr_t>(object) < 0x10000) {
        return;
    }

    thread_local int s_ProcessEventDepth = 0;
    struct DepthGuard {
        int& d;
        DepthGuard(int& val) : d(val) { ++d; }
        ~DepthGuard() { --d; }
    } depthGuard(s_ProcessEventDepth);

    EngineCallGuard engineCallGuard;

    CppProcessEventSink cppSink = mgr.m_CppProcessEventSink.load(std::memory_order_acquire);
    if (cppSink && !EngineDiscovery::IsLoaderDispatchedCall()) {
        cppSink(object, ufunction, params);
    }

    if (s_ProcessEventDepth == 1 && !EngineDiscovery::IsLoaderDispatchedCall()) {

        if (!mgr.m_GameThreadIdCaptured) {
            std::lock_guard<std::recursive_mutex> captureLock(mgr.m_HookMutex);
            if (!mgr.m_GameThreadIdCaptured) {
                mgr.m_GameThreadId = std::this_thread::get_id();
                mgr.m_GameThreadIdCaptured = true;
            }
        }
        s_LastProcessEventTick.store(GetTickCount64(), std::memory_order_relaxed);
        static uint64_t s_LastTick = 0;
        uint64_t nowTick = GetTickCount64();
        if (nowTick - s_LastTick >= 16) {
            s_LastTick = nowTick;
            bool runFrameSink = false;
            {
                std::unique_lock<std::recursive_mutex> lock(mgr.m_HookMutex, std::try_to_lock);
                if (lock.owns_lock()) {
                    mgr.UpdateFrameTiming();

                    if (mgr.m_L) {
                        mgr.ProcessGameThreadTasks();
                        mgr.ProcessTimers();
                        mgr.CheckNewObjects();

                        static uint64_t s_LastPendingResolveTick = 0;
                        if (nowTick - s_LastPendingResolveTick >= 2000) {
                            s_LastPendingResolveTick = nowTick;
                            mgr.ResolvePendingHooks();
                        }
                    } else {

                        mgr.m_FrameCounter.fetch_add(1, std::memory_order_relaxed);
                        EngineDiscovery::AdvanceLiveLookupGeneration(g_State);
                    }
                    runFrameSink = true;
                }
            }

            FrameSink frameSink = mgr.m_FrameSink.load(std::memory_order_acquire);
            if (runFrameSink && frameSink) {
                frameSink(mgr.m_FrameSinkUser.load(std::memory_order_acquire));
            }
        }
    }

    if (!mgr.m_L) {
        origPE(object, ufunction, params);
        return;
    }

    const bool hasHooks = mgr.m_HasActiveHooks.load(std::memory_order_relaxed);

    if (!hasHooks && mgr.m_ConsoleCommandHandlers.empty() && mgr.m_GlobalConsoleHandlers.empty()) {
        origPE(object, ufunction, params);
        return;
    }

    std::string funcShortName;
    {
        if (ufunction && g_State.UObject_NameOff >= 0) {
            FName fnName = {};
            const void* fnAddr = reinterpret_cast<const uint8_t*>(ufunction) + g_State.UObject_NameOff;
            if (SafeMem_IsReadable(fnAddr, sizeof(FName)) &&
                SafeMem_Read(fnAddr, &fnName, sizeof(FName)) &&
                fnName.ComparisonIndex != 0) {
                funcShortName = EngineDiscovery::GetCachedFName(g_State, static_cast<int32_t>(fnName.ComparisonIndex));
            }
        }
    }
    std::string funcShortNameLower;
    if (!funcShortName.empty()) {
        funcShortNameLower = funcShortName;
        std::transform(funcShortNameLower.begin(), funcShortNameLower.end(), funcShortNameLower.begin(), ::tolower);
    }

    const bool answeredByScriptHook = mgr.m_ScriptHookInstalled && IsInterpretedFunction(ufunction);
    const bool isBeginPlayDispatch = (funcShortNameLower == "receivebeginplay");

    const std::vector<LuaHookEntry>* pEntries = nullptr;
    std::vector<LuaHookEntry> localEntries;

    BeginPlayRideAlong ride;
    if (hasHooks && (!answeredByScriptHook || isBeginPlayDispatch)) {
        std::lock_guard<std::recursive_mutex> lock(mgr.m_HookMutex);
        auto it = mgr.m_FunctionHooks.find(ufunction);
        if (it != mgr.m_FunctionHooks.end() && !it->second.empty()) {
            if (!answeredByScriptHook) {

                for (const LuaHookEntry& candidate : it->second) {
                    if (mgr.ClaimBeginPlayEntry(candidate, object)) localEntries.push_back(candidate);
                }
            } else {

                const bool bAnchorItself = (mgr.m_BeginPlayAnchorFunction == ufunction);
                for (const LuaHookEntry& candidate : it->second) {
                    if (bAnchorItself || candidate.IsLoadMapHook || candidate.IsInitGameStateHook) {
                        if (mgr.ClaimBeginPlayEntry(candidate, object)) localEntries.push_back(candidate);
                    }
                }
            }
            if (!localEntries.empty()) pEntries = &localEntries;
        } else if (!answeredByScriptHook && !funcShortNameLower.empty()) {

            auto itEvt = mgr.m_CustomEvents.find(funcShortNameLower);
            if (itEvt != mgr.m_CustomEvents.end() && !itEvt->second.empty()) {
                localEntries = itEvt->second;
                pEntries = &localEntries;
            }
        }

        if (pEntries == nullptr && funcShortNameLower == "receivebeginplay" && mgr.m_BeginPlayAnchorFunction) {
            auto itAnchor = mgr.m_FunctionHooks.find(mgr.m_BeginPlayAnchorFunction);
            if (itAnchor != mgr.m_FunctionHooks.end()) {
                const bool bActorEventFunction =
                    FunctionCarriesActorEvent(ufunction, mgr.m_BeginPlayAnchorFunction);
                for (const LuaHookEntry& candidate : itAnchor->second) {
                    if (candidate.IsLoadMapHook || candidate.IsInitGameStateHook) {
                        localEntries.push_back(candidate);
                    } else if (bActorEventFunction && mgr.ClaimBeginPlayEntry(candidate, object)) {
                        localEntries.push_back(candidate);
                    }
                }
                if (!localEntries.empty()) pEntries = &localEntries;
            }
        }

        if (pEntries != nullptr) {
            for (const LuaHookEntry& candidate : *pEntries) {
                if (candidate.IsLoadMapHook || candidate.IsInitGameStateHook) {
                    ride = mgr.PrepareBeginPlayRideAlong(object, localEntries);
                    break;
                }
            }
        }
    }

    if (!pEntries && mgr.m_ConsoleCommandHandlers.empty() && mgr.m_GlobalConsoleHandlers.empty()) {
        origPE(object, ufunction, params);
        return;
    }

    bool cancelExecution = false;

    if (!mgr.m_ConsoleCommandHandlers.empty()) {
        static int32_t s_ConsoleCmdNameIdx = -2;
        static int32_t s_ProcessConsoleExecNameIdx = -2;
        if (s_ConsoleCmdNameIdx == -2 && g_State.bInitialized) {
            s_ConsoleCmdNameIdx = EngineDiscovery::ResolveFNameIndex(g_State, "ConsoleCommand");
            s_ProcessConsoleExecNameIdx = EngineDiscovery::ResolveFNameIndex(g_State, "ProcessConsoleExec");
        }

        int32_t funcNameIdx = -1;
        if (ufunction && g_State.UObject_NameOff >= 0) {
            FName fn = {};
            if (SafeMem_Read(reinterpret_cast<const uint8_t*>(ufunction) + g_State.UObject_NameOff, &fn, sizeof(FName))) {
                funcNameIdx = static_cast<int32_t>(fn.ComparisonIndex);
            }
        }

        const bool isConsoleFunc = (funcNameIdx >= 0 && (funcNameIdx == s_ConsoleCmdNameIdx || funcNameIdx == s_ProcessConsoleExecNameIdx));
        if (isConsoleFunc) {
            std::lock_guard<std::recursive_mutex> lock(mgr.m_HookMutex);
            if (params) {

                std::string fullCmd;
                FString fsHeader = {};
                if (SafeMem_IsReadable(params, sizeof(FString)) &&
                    SafeMem_Read(params, &fsHeader, sizeof(FString)) && fsHeader.Data &&
                    fsHeader.ArrayNum > 0 && fsHeader.ArrayNum < 8192 &&
                    SafeMem_IsReadable(fsHeader.Data, static_cast<size_t>(fsHeader.ArrayNum) * sizeof(wchar_t))) {
                    std::vector<wchar_t> wide(static_cast<size_t>(fsHeader.ArrayNum), L'\0');
                    if (SafeMem_Read(fsHeader.Data, wide.data(), static_cast<size_t>(fsHeader.ArrayNum) * sizeof(wchar_t))) {
                        int32_t wideLen = fsHeader.ArrayNum;
                        while (wideLen > 0 && wide[wideLen - 1] == 0) --wideLen;
                        if (wideLen > 0) {
                            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLen, nullptr, 0, nullptr, nullptr);
                            if (utf8Len > 0) {
                                fullCmd.resize(static_cast<size_t>(utf8Len));
                                WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLen, fullCmd.data(), utf8Len, nullptr, nullptr);
                            }
                        }
                    }
                }
                if (!fullCmd.empty()) {
                    std::istringstream iss(fullCmd);
                    std::string cmdName;
                    iss >> cmdName;
                    std::string cmdArgs;
                    std::getline(iss, cmdArgs);
                    if (!cmdArgs.empty() && cmdArgs[0] == ' ') cmdArgs.erase(0, 1);

                    std::string lowerCmd = cmdName;
                    std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), ::tolower);

                    if (!mgr.m_GlobalConsoleHandlers.empty()) {

                        std::unique_lock<std::recursive_mutex> luaLockG(LuaManager::Get().GetLuaMutex(), std::try_to_lock);
                        lua_State* L = luaLockG.owns_lock() ? mgr.LiveStateUnderLock() : nullptr;
                        if (L) {
                            for (const GlobalConsoleHandler& handler : mgr.m_GlobalConsoleHandlers) {

                                if (!handler.CommandName.empty() && handler.CommandName != lowerCmd) continue;
                                lua_rawgeti(L, LUA_REGISTRYINDEX, handler.CallbackRef);
                                if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
                                lua_pushstring(L, fullCmd.c_str());
                                lua_newtable(L);
                                std::istringstream tokenStreamG(cmdArgs);
                                std::string tokenG;
                                int argIdxG = 1;
                                while (tokenStreamG >> tokenG) {
                                    lua_pushstring(L, tokenG.c_str());
                                    lua_rawseti(L, -2, argIdxG++);
                                }
                                CompatPushOutputDevice(L);
                                if (lua_pcall(L, 3, 1, 0) == LUA_OK) {
                                    if (lua_isboolean(L, -1) && lua_toboolean(L, -1)) {
                                        cancelExecution = true;
                                    }
                                    lua_pop(L, 1);
                                } else {
                                    const char* errG = lua_tostring(L, -1);
                                    char bufG[512];
                                    snprintf(bufG, sizeof(bufG), "[LuaHook] Global console handler error: %s", errG ? errG : "unknown");
                                    EngineDiscovery::WriteLogLine(bufG);
                                    lua_pop(L, 1);
                                }
                            }
                        }
                    }

                    auto itCmd = mgr.m_ConsoleCommandHandlers.find(lowerCmd);
                    if (itCmd != mgr.m_ConsoleCommandHandlers.end()) {

                        std::unique_lock<std::recursive_mutex> luaLock(LuaManager::Get().GetLuaMutex(), std::try_to_lock);
                        lua_State* L = luaLock.owns_lock() ? mgr.LiveStateUnderLock() : nullptr;
                        for (int ref : itCmd->second) {
                            if (!L) break;
                            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

                            lua_pushstring(L, fullCmd.c_str());

                            lua_newtable(L);
                            std::istringstream tokenStream(cmdArgs);
                            std::string token;
                            int argIdx = 1;
                            while (tokenStream >> token) {
                                lua_pushstring(L, token.c_str());
                                lua_rawseti(L, -2, argIdx++);
                            }

                            CompatPushOutputDevice(L);
                            lua_pushstring(L, cmdArgs.c_str());

                            if (lua_pcall(L, 4, 1, 0) == LUA_OK) {
                                if (lua_isboolean(L, -1) && lua_toboolean(L, -1)) {
                                    cancelExecution = true;
                                }
                                lua_pop(L, 1);
                            } else {
                                const char* err = lua_tostring(L, -1);
                                char buf[512];
                                snprintf(buf, sizeof(buf), "[Lua] Console command handler error for '%s': %s", cmdName.c_str(), err ? err : "unknown");
                                EngineDiscovery::WriteLogLine(buf);
                                lua_pop(L, 1);
                            }
                        }
                    }
                }
            }
        }
    }

    if (pEntries && !pEntries->empty()) {
        auto luaLock = LockLuaForHandOff();

        lua_State* L = luaLock.owns_lock() ? mgr.LiveStateUnderLock() : nullptr;
        for (const auto& entry : *pEntries) {
            if (!L) break;

            if (entry.IsLoadMapHook && !entry.LoadMapNotify) continue;

            if (entry.IsInitGameStateHook && !entry.AuthorityNotify) continue;
            if (entry.PreCallbackRef != LUA_NOREF) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, entry.PreCallbackRef);

                PushUObject(L, ContextForEntry(entry, ride, object));
                int nArgs = 1 + PushHookParamProxies(L, ufunction, params);
                if (entry.IsLoadMapHook) {
                    if (ride.World) {
                        PushUObject(L, static_cast<UObject*>(ride.World));
                    } else {
                        CompatPushInvalidObject(L);
                    }
                    nArgs++;
                }

                if (lua_pcall(L, nArgs, 1, 0) == LUA_OK) {
                    if (lua_isboolean(L, -1) && lua_toboolean(L, -1)) {
                        cancelExecution = true;
                    }
                    lua_pop(L, 1);
                } else {
                    const char* err = lua_tostring(L, -1);
                    char buf[512];
                    snprintf(buf, sizeof(buf), "[LuaHook] Pre-hook error for '%s': %s", entry.FunctionName.c_str(), err ? err : "unknown");
                    EngineDiscovery::WriteLogLine(buf);
                    lua_pop(L, 1);
                }
            }
        }
    }

    if (!cancelExecution) {
        origPE(object, ufunction, params);
    }

    if (pEntries && !pEntries->empty()) {

        auto luaLock = LockLuaForHandOff();
        lua_State* L = luaLock.owns_lock() ? mgr.LiveStateUnderLock() : nullptr;
        for (const auto& entry : *pEntries) {
            if (!L) break;
            if (entry.IsLoadMapHook && !entry.LoadMapNotify) continue;
            if (entry.IsInitGameStateHook && !entry.AuthorityNotify) continue;
            if (entry.IsLoadMapHook || entry.IsInitGameStateHook) {
                WriteRideAlongNotice(entry, ContextForEntry(entry, ride, object),
                                     static_cast<UObject*>(ride.World), ride.MatchedByAuthorityField);
            }
            if (entry.PostCallbackRef != LUA_NOREF) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, entry.PostCallbackRef);

                PushUObject(L, ContextForEntry(entry, ride, object));
                int nArgs = 1 + PushHookParamProxies(L, ufunction, params);
                if (entry.IsLoadMapHook) {
                    UObject* world = ResolveWorldForContext(static_cast<UObject*>(object));
                    if (world) {
                        PushUObject(L, world);
                    } else {
                        CompatPushInvalidObject(L);
                    }
                    nArgs++;
                }

                if (lua_pcall(L, nArgs, 0, 0) != LUA_OK) {
                    const char* err = lua_tostring(L, -1);
                    char buf[512];
                    snprintf(buf, sizeof(buf), "[LuaHook] Post-hook error for '%s': %s", entry.FunctionName.c_str(), err ? err : "unknown");
                    EngineDiscovery::WriteLogLine(buf);
                    lua_pop(L, 1);
                }
            }
        }
    }
}

void __fastcall LuaHookManager::Hooked_ProcessLocalScriptFunction(void* context, void* frame, void* result) {
    auto& mgr = LuaHookManager::Get();
    typedef void(__fastcall* ProcessLocalScriptFunction_t)(void*, void*, void*);
    ProcessLocalScriptFunction_t original =
        reinterpret_cast<ProcessLocalScriptFunction_t>(mgr.m_OriginalProcessLocalScriptFunction);
    if (!original) return;

    thread_local int s_ScriptDispatchDepth = 0;
    if (s_ScriptDispatchDepth >= 24) {
        original(context, frame, result);
        return;
    }
    struct DepthGuard {
        int& Depth;
        DepthGuard(int& InDepth) : Depth(InDepth) { ++Depth; }
        ~DepthGuard() { --Depth; }
    } depthGuard(s_ScriptDispatchDepth);

    EngineCallGuard engineCallGuard;

    CppScriptFunctionSink cppSink =
        mgr.m_CppScriptFunctionSink.load(std::memory_order_acquire);
    const bool callCppHooks = cppSink != nullptr && context != nullptr && frame != nullptr;
    const bool hasActiveHooks = mgr.m_HasActiveHooks.load(std::memory_order_relaxed);

    if (!hasActiveHooks && !callCppHooks) {
        original(context, frame, result);
        return;
    }

    if (callCppHooks) cppSink(context, frame, result, true);

    original(context, frame, result);

    if (callCppHooks) cppSink(context, frame, result, false);

    if (!hasActiveHooks) return;

    if (!frame || !context) return;

    void* ufunction = nullptr;
    if (!SafeMem_Read(static_cast<const uint8_t*>(frame) + FFrame_NodeOff, &ufunction,
                      sizeof(ufunction)) ||
        reinterpret_cast<uintptr_t>(ufunction) < 0x10000)
        return;

    mgr.DispatchScriptFunctionEntries(context, ufunction, frame);
}

void LuaHookManager::DispatchScriptFunctionEntries(void* context, void* ufunction, void* frame) {
    std::vector<LuaHookEntry> localEntries;
    BeginPlayRideAlong ride;
    {
        std::lock_guard<std::recursive_mutex> lock(m_HookMutex);
        auto it = m_FunctionHooks.find(ufunction);
        if (it != m_FunctionHooks.end() && !it->second.empty()) {
            localEntries = it->second;
        } else if (g_State.UObject_NameOff >= 0 &&
                   (!m_CustomEvents.empty() || m_BeginPlayAnchorFunction != nullptr)) {

            FName fnName = {};
            const void* fnAddr = static_cast<const uint8_t*>(ufunction) + g_State.UObject_NameOff;
            if (SafeMem_IsReadable(fnAddr, sizeof(FName)) &&
                SafeMem_Read(fnAddr, &fnName, sizeof(FName)) && fnName.ComparisonIndex != 0) {
                const int32_t nameIndex = static_cast<int32_t>(fnName.ComparisonIndex);

                if (!m_CustomEvents.empty() && !m_KnownNonEventNameIndexes.count(nameIndex)) {
                    std::string funcShortName =
                        EngineDiscovery::GetCachedFName(g_State, nameIndex);
                    if (!funcShortName.empty()) {
                        std::transform(funcShortName.begin(), funcShortName.end(),
                                       funcShortName.begin(), ::tolower);
                        auto itEvt = m_CustomEvents.find(funcShortName);
                        if (itEvt != m_CustomEvents.end() && !itEvt->second.empty()) {
                            localEntries = itEvt->second;
                        } else {
                            m_KnownNonEventNameIndexes.insert(nameIndex);
                        }
                    }
                }

                if (localEntries.empty() && m_BeginPlayAnchorFunction) {
                    std::string funcShortName =
                        EngineDiscovery::GetCachedFName(g_State, nameIndex);
                    if (!funcShortName.empty()) {
                        std::transform(funcShortName.begin(), funcShortName.end(),
                                       funcShortName.begin(), ::tolower);
                        if (funcShortName == "receivebeginplay") {
                            auto itAnchor = m_FunctionHooks.find(m_BeginPlayAnchorFunction);
                            if (itAnchor != m_FunctionHooks.end()) {

                                const bool bActorEventFunction =
                                    FunctionCarriesActorEvent(ufunction, m_BeginPlayAnchorFunction);
                                for (const LuaHookEntry& candidate : itAnchor->second) {
                                    if (candidate.IsLoadMapHook || candidate.IsInitGameStateHook) {
                                        localEntries.push_back(candidate);
                                    } else if (bActorEventFunction &&
                                               ClaimBeginPlayEntry(candidate, context)) {
                                        localEntries.push_back(candidate);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (!localEntries.empty()) {
            for (const LuaHookEntry& candidate : localEntries) {
                if (candidate.IsLoadMapHook || candidate.IsInitGameStateHook) {
                    ride = PrepareBeginPlayRideAlong(context, localEntries);
                    break;
                }
            }
        }
    }

    if (localEntries.empty()) return;

    void* paramsBase = nullptr;
    SafeMem_Read(static_cast<const uint8_t*>(frame) + FFrame_LocalsOff, &paramsBase,
                 sizeof(paramsBase));

    auto luaLock = LockLuaForHandOff();
    lua_State* L = luaLock.owns_lock() ? LiveStateUnderLock() : nullptr;
    if (!L) return;

    for (const LuaHookEntry& entry : localEntries) {

        if (entry.IsLoadMapHook && !entry.LoadMapNotify) continue;
        if (entry.IsInitGameStateHook && !entry.AuthorityNotify) continue;
        if (entry.IsLoadMapHook || entry.IsInitGameStateHook) {
            WriteRideAlongNotice(entry, ContextForEntry(entry, ride, context),
                                 static_cast<UObject*>(ride.World), ride.MatchedByAuthorityField);
        }

        const int callbacks[2] = { entry.PreCallbackRef, entry.PostCallbackRef };
        for (const int callbackRef : callbacks) {
            if (callbackRef == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, callbackRef);
            if (!lua_isfunction(L, -1)) {
                lua_pop(L, 1);
                continue;
            }

            PushUObject(L, ContextForEntry(entry, ride, context));
            int nArgs = 1 + PushHookParamProxies(L, ufunction, paramsBase, frame);
            if (entry.IsLoadMapHook) {
                if (ride.World) {
                    PushUObject(L, static_cast<UObject*>(ride.World));
                } else {
                    CompatPushInvalidObject(L);
                }
                nArgs++;
            }

            if (lua_pcall(L, nArgs, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                char buf[512];
                snprintf(buf, sizeof(buf), "[LuaHook] Script-function hook error for '%s': %s",
                         entry.FunctionName.c_str(), err ? err : "unknown");
                EngineDiscovery::WriteLogLine(buf);
                lua_pop(L, 1);
            }
        }
    }
}

static int Hook_RegisterHook(lua_State* L) {
    const char* funcName = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int preRef = luaL_ref(L, LUA_REGISTRYINDEX);
    int postRef = LUA_NOREF;
    if (lua_gettop(L) >= 3 && lua_isfunction(L, 3)) {
        lua_pushvalue(L, 3);
        postRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    uint32_t preId = 0, postId = 0;
    LuaHookManager::Get().RegisterHookCompat(funcName, preRef, postRef, &preId, &postId);
    lua_pushinteger(L, preId);
    lua_pushinteger(L, postId);
    return 2;
}

static int Hook_UnregisterHook(lua_State* L) {

    if (lua_isnumber(L, 1)) {
        uint32_t id = static_cast<uint32_t>(lua_tointeger(L, 1));
        bool ok = LuaHookManager::Get().UnregisterHookById(id);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    if (lua_isstring(L, 1)) {
        const char* name = lua_tostring(L, 1);
        uint32_t preId = lua_isinteger(L, 2) ? static_cast<uint32_t>(lua_tointeger(L, 2)) : 0;
        uint32_t postId = lua_isinteger(L, 3) ? static_cast<uint32_t>(lua_tointeger(L, 3)) : 0;
        bool ok = false;
        if (preId != 0) ok = LuaHookManager::Get().UnregisterHookById(preId) || ok;
        if (postId != 0) ok = LuaHookManager::Get().UnregisterHookById(postId) || ok;
        if (!ok && (preId == 0 && postId == 0)) {
            ok = LuaHookManager::Get().UnregisterHookByName(name);
        }
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int Hook_RegisterKeyBind(lua_State* L) {

    int keyVk = 0;
    std::string keyName;
    std::vector<std::string> modifiers;
    int cbIdx = 2;

    auto collectModifiers = [&](int tableIndex) {
        lua_pushnil(L);
        while (lua_next(L, tableIndex) != 0) {
            if (lua_isnumber(L, -1)) {
                modifiers.push_back(std::to_string(lua_tointeger(L, -1)));
            } else if (lua_isstring(L, -1)) {
                modifiers.push_back(lua_tostring(L, -1));
            }
            lua_pop(L, 1);
        }
    };

    if (lua_istable(L, 1)) {
        lua_pushnil(L);
        bool first = true;
        while (lua_next(L, 1) != 0) {
            if (lua_isnumber(L, -1)) {
                int vk = static_cast<int>(lua_tointeger(L, -1));
                if (first) { keyVk = vk; first = false; }
                else modifiers.push_back(std::to_string(vk));
            } else if (lua_isstring(L, -1)) {
                if (first) { keyName = lua_tostring(L, -1); first = false; }
                else modifiers.push_back(lua_tostring(L, -1));
            }
            lua_pop(L, 1);
        }
        cbIdx = 2;
    } else if (lua_isnumber(L, 1)) {
        keyVk = static_cast<int>(lua_tointeger(L, 1));
        if (lua_istable(L, 2)) {
            collectModifiers(2);
            cbIdx = 3;
        }
    } else {
        std::string named = luaL_checkstring(L, 1);
        const int namedVk = ResolveVirtualKeyCode(named);
        if (namedVk != 0) keyVk = namedVk;
        else keyName = named;
        if (lua_istable(L, 2)) {
            collectModifiers(2);
            cbIdx = 3;
        }
    }

    luaL_checktype(L, cbIdx, LUA_TFUNCTION);
    lua_pushvalue(L, cbIdx);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);

    std::string resolved = keyName;
    if (keyVk != 0) resolved = VkToCanonicalName(keyVk);

    LuaHookManager::Get().RegisterKeyBindWithModifiers(resolved, modifiers, cbRef);
    return 0;
}

int ResolveVirtualKeyCode(const std::string& keyName) {
    std::string k = keyName;
    std::transform(k.begin(), k.end(), k.begin(), ::tolower);
    k.erase(std::remove_if(k.begin(), k.end(), [](char c) { return c == ' ' || c == '_' || c == '-'; }), k.end());

    if (k.rfind("f", 0) == 0 && k.size() >= 2) {
        try {
            int fNum = std::stoi(k.substr(1));
            if (fNum >= 1 && fNum <= 24) return VK_F1 + (fNum - 1);
        } catch (...) {}
    }
    if (k.rfind("numpad", 0) == 0 && k.size() >= 7) {
        char digit = k[6];
        if (digit >= '0' && digit <= '9') return VK_NUMPAD0 + (digit - '0');
    }
    if (k.size() == 1) {
        char c = k[0];
        if (c >= 'a' && c <= 'z') return 'A' + (c - 'a');
        if (c >= '0' && c <= '9') return '0' + (c - '0');
        if (c == '`' || c == '~') return VK_OEM_3;
        if (c == '-') return VK_OEM_MINUS;
        if (c == '=' || c == '+') return VK_OEM_PLUS;
        if (c == '[') return VK_OEM_4;
        if (c == ']') return VK_OEM_6;
        if (c == ';') return VK_OEM_1;
        if (c == '\'') return VK_OEM_7;
        if (c == ',') return VK_OEM_COMMA;
        if (c == '.') return VK_OEM_PERIOD;
        if (c == '/') return VK_OEM_2;
        if (c == '\\') return VK_OEM_5;
    }
    if (k == "space" || k == "spacebar") return VK_SPACE;
    if (k == "enter" || k == "return") return VK_RETURN;
    if (k == "tab") return VK_TAB;
    if (k == "escape" || k == "esc") return VK_ESCAPE;
    if (k == "backspace" || k == "back" || k == "bksp") return VK_BACK;
    if (k == "shift" || k == "lshift" || k == "rshift" || k == "left_shift" || k == "right_shift" || k == "leftshift" || k == "rightshift") return VK_SHIFT;
    if (k == "ctrl" || k == "control" || k == "lctrl" || k == "rctrl" || k == "left_control" || k == "right_control" || k == "left_ctrl" || k == "right_ctrl" || k == "leftctrl" || k == "rightctrl") return VK_CONTROL;
    if (k == "alt" || k == "lalt" || k == "ralt" || k == "left_alt" || k == "right_alt" || k == "leftalt" || k == "rightalt") return VK_MENU;
    if (k == "insert" || k == "ins") return VK_INSERT;
    if (k == "delete" || k == "del") return VK_DELETE;
    if (k == "home") return VK_HOME;
    if (k == "end") return VK_END;
    if (k == "pageup" || k == "pgup") return VK_PRIOR;
    if (k == "pagedown" || k == "pgdn") return VK_NEXT;
    if (k == "tilde" || k == "`" || k == "~") return VK_OEM_3;
    if (k == "capslock" || k == "caps") return VK_CAPITAL;
    if (k == "up") return VK_UP;
    if (k == "down") return VK_DOWN;
    if (k == "left") return VK_LEFT;
    if (k == "right") return VK_RIGHT;
    if (k == "minus" || k == "dash") return VK_OEM_MINUS;
    if (k == "plus" || k == "equals" || k == "equal") return VK_OEM_PLUS;
    if (k == "bracketleft" || k == "lbracket") return VK_OEM_4;
    if (k == "bracketright" || k == "rbracket") return VK_OEM_6;
    if (k == "semicolon") return VK_OEM_1;
    if (k == "quote" || k == "apostrophe") return VK_OEM_7;
    if (k == "comma") return VK_OEM_COMMA;
    if (k == "period" || k == "dot") return VK_OEM_PERIOD;
    if (k == "slash") return VK_OEM_2;
    if (k == "backslash") return VK_OEM_5;
    if (k == "numpadadd" || k == "numplus") return VK_ADD;
    if (k == "numpadsubtract" || k == "numminus") return VK_SUBTRACT;
    if (k == "numpadmultiply" || k == "nummul") return VK_MULTIPLY;
    if (k == "numpaddivide" || k == "numdiv") return VK_DIVIDE;
    if (k == "numpaddecimal" || k == "numdot") return VK_DECIMAL;
    if (k == "pause") return VK_PAUSE;
    if (k == "printscreen" || k == "prtscn" || k == "snapshot") return VK_SNAPSHOT;
    if (k == "scrolllock") return VK_SCROLL;
    if (k == "lbutton" || k == "mouse1" || k == "leftclick") return VK_LBUTTON;
    if (k == "rbutton" || k == "mouse2" || k == "rightclick") return VK_RBUTTON;
    if (k == "mbutton" || k == "mouse3" || k == "middleclick") return VK_MBUTTON;
    if (k == "xbutton1" || k == "mouse4") return VK_XBUTTON1;
    if (k == "xbutton2" || k == "mouse5") return VK_XBUTTON2;

    return 0;
}

static void RegisterKeyTable(lua_State* L) {
    lua_newtable(L);
    auto K = [&](const char* name, int vk) { lua_pushinteger(L, vk); lua_setfield(L, -2, name); };
    K("LeftMouseButton", 0x01);   K("RightMouseButton", 0x02);  K("MiddleMouseButton", 0x04);
    K("ThumbMouseButton", 0x05);  K("ThumbMouseButton2", 0x06);
    K("BackSpace", VK_BACK);      K("Tab", VK_TAB);             K("Enter", VK_RETURN);
    K("Shift", VK_SHIFT);         K("LeftShift", VK_LSHIFT);    K("RightShift", VK_RSHIFT);
    K("Control", VK_CONTROL);     K("LeftControl", VK_LCONTROL);K("RightControl", VK_RCONTROL);
    K("Alt", VK_MENU);            K("LeftAlt", VK_LMENU);       K("RightAlt", VK_RMENU);
    K("Pause", VK_PAUSE);         K("CapsLock", VK_CAPITAL);    K("Escape", VK_ESCAPE);
    K("SpaceBar", VK_SPACE);      K("PageUp", VK_PRIOR);        K("PageDown", VK_NEXT);
    K("End", VK_END);             K("Home", VK_HOME);           K("Left", VK_LEFT);
    K("Up", VK_UP);               K("Right", VK_RIGHT);         K("Down", VK_DOWN);
    K("Insert", VK_INSERT);       K("Delete", VK_DELETE);
    K("Zero", '0'); K("One", '1'); K("Two", '2'); K("Three", '3'); K("Four", '4');
    K("Five", '5'); K("Six", '6'); K("Seven", '7'); K("Eight", '8'); K("Nine", '9');
    K("A", 'A'); K("B", 'B'); K("C", 'C'); K("D", 'D'); K("E", 'E'); K("F", 'F');
    K("G", 'G'); K("H", 'H'); K("I", 'I'); K("J", 'J'); K("K", 'K'); K("L", 'L');
    K("M", 'M'); K("N", 'N'); K("O", 'O'); K("P", 'P'); K("Q", 'Q'); K("R", 'R');
    K("S", 'S'); K("T", 'T'); K("U", 'U'); K("V", 'V'); K("W", 'W'); K("X", 'X');
    K("Y", 'Y'); K("Z", 'Z');
    K("NumPadZero", VK_NUMPAD0);  K("NumPadOne", VK_NUMPAD1);   K("NumPadTwo", VK_NUMPAD2);
    K("NumPadThree", VK_NUMPAD3); K("NumPadFour", VK_NUMPAD4);  K("NumPadFive", VK_NUMPAD5);
    K("NumPadSix", VK_NUMPAD6);   K("NumPadSeven", VK_NUMPAD7); K("NumPadEight", VK_NUMPAD8);
    K("NumPadNine", VK_NUMPAD9);  K("Multiply", VK_MULTIPLY);   K("Add", VK_ADD);
    K("Subtract", VK_SUBTRACT);   K("Decimal", VK_DECIMAL);     K("Divide", VK_DIVIDE);
    K("F1", VK_F1); K("F2", VK_F2); K("F3", VK_F3); K("F4", VK_F4); K("F5", VK_F5);
    K("F6", VK_F6); K("F7", VK_F7); K("F8", VK_F8); K("F9", VK_F9); K("F10", VK_F10);
    K("F11", VK_F11); K("F12", VK_F12); K("F13", VK_F13); K("F14", VK_F14); K("F15", VK_F15);
    K("F16", VK_F16); K("F17", VK_F17); K("F18", VK_F18); K("F19", VK_F19); K("F20", VK_F20);
    K("F21", VK_F21); K("F22", VK_F22); K("F23", VK_F23); K("F24", VK_F24);
    K("NumLock", VK_NUMLOCK);     K("ScrollLock", VK_SCROLL);
    K("LeftBracket", VK_OEM_4);   K("RightBracket", VK_OEM_6);  K("Semicolon", VK_OEM_1);
    K("Quote", VK_OEM_7);         K("Comma", VK_OEM_COMMA);     K("Period", VK_OEM_PERIOD);
    K("Slash", VK_OEM_2);         K("Backslash", VK_OEM_5);     K("Tilde", VK_OEM_3);
    K("Equals", VK_OEM_PLUS);     K("Minus", VK_OEM_MINUS);
    K("Gamepad_Left2D", 0x100);   K("Gamepad_LeftX", 0x101);    K("Gamepad_LeftY", 0x102);
    K("Gamepad_Right2D", 0x103);  K("Gamepad_RightX", 0x104);   K("Gamepad_RightY", 0x105);
    K("Gamepad_LeftTriggerAxis", 0x106); K("Gamepad_RightTriggerAxis", 0x107);
    K("Gamepad_LeftThumbstick", 0x108);  K("Gamepad_RightThumbstick", 0x109);
    K("Gamepad_Special_Left", 0x10A);    K("Gamepad_Special_Right", 0x10B);
    K("Gamepad_FaceButton_Bottom", 0x10C); K("Gamepad_FaceButton_Right", 0x10D);
    K("Gamepad_FaceButton_Left", 0x10E);   K("Gamepad_FaceButton_Top", 0x10F);
    K("Gamepad_LeftShoulder", 0x110);      K("Gamepad_RightShoulder", 0x111);
    K("Gamepad_LeftTrigger", 0x112);       K("Gamepad_RightTrigger", 0x113);
    K("Gamepad_DPad_Up", 0x114);    K("Gamepad_DPad_Down", 0x115);
    K("Gamepad_DPad_Right", 0x116); K("Gamepad_DPad_Left", 0x117);
    K("Gamepad_LeftStick_Up", 0x118);    K("Gamepad_LeftStick_Down", 0x119);
    K("Gamepad_LeftStick_Right", 0x11A); K("Gamepad_LeftStick_Left", 0x11B);
    K("Gamepad_RightStick_Up", 0x11C);   K("Gamepad_RightStick_Down", 0x11D);
    K("Gamepad_RightStick_Right", 0x11E);K("Gamepad_RightStick_Left", 0x11F);

    lua_getglobal(L, "Key");
    if (lua_istable(L, -1)) {
        const int dst = lua_gettop(L) - 1;
        const int src = lua_gettop(L);
        lua_pushnil(L);
        while (lua_next(L, src) != 0) {
            lua_pushvalue(L, -2);
            lua_pushvalue(L, -2);
            lua_rawset(L, dst);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    lua_setglobal(L, "Key");
}

static bool CheckVirtualKeyDown(int vk) {
    if (vk == 0) return false;
    if (ConsoleEnabler::IsVirtualKeyHeldWndProc(vk)) return true;
    auto isDown = [](int code) -> bool {
        if (ConsoleEnabler::IsVirtualKeyHeldWndProc(code)) return true;
        return ((GetAsyncKeyState(code) & 0x8000) != 0) || ((GetKeyState(code) & 0x8000) != 0);
    };

    if (isDown(vk)) return true;

    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) {
        return isDown(VK_SHIFT) || isDown(VK_LSHIFT) || isDown(VK_RSHIFT);
    }
    if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL) {
        return isDown(VK_CONTROL) || isDown(VK_LCONTROL) || isDown(VK_RCONTROL);
    }
    if (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU) {
        return isDown(VK_MENU) || isDown(VK_LMENU) || isDown(VK_RMENU);
    }
    return false;
}

static int Hook_IsKeyDown(lua_State* L) {

    int vk = 0;
    if (lua_isnumber(L, 1)) {
        vk = static_cast<int>(lua_tointeger(L, 1));
    } else {
        vk = ResolveVirtualKeyCode(luaL_checkstring(L, 1));
    }
    lua_pushboolean(L, CheckVirtualKeyDown(vk) ? 1 : 0);
    return 1;
}

static int Hook_NotifyOnNewObject(lua_State* L) {
    const char* className = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    LuaHookManager::Get().RegisterNewObjectListener(className, cbRef);
    return 0;
}

static int Hook_RegisterCustomEvent(lua_State* L) {
    const char* eventName = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int preRef = luaL_ref(L, LUA_REGISTRYINDEX);
    int postRef = LUA_NOREF;
    if (lua_gettop(L) >= 3 && lua_isfunction(L, 3)) {
        lua_pushvalue(L, 3);
        postRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    LuaHookManager::Get().RegisterCustomEvent(eventName, preRef, postRef);
    lua_pushinteger(L, preRef);
    lua_pushinteger(L, postRef == LUA_NOREF ? 0 : postRef);
    return 2;
}

static int Hook_UnregisterCustomEvent(lua_State* L) {
    const char* eventName = luaL_checkstring(L, 1);
    int preRef = lua_isinteger(L, 2) ? static_cast<int>(lua_tointeger(L, 2)) : LUA_NOREF;
    int postRef = lua_isinteger(L, 3) ? static_cast<int>(lua_tointeger(L, 3)) : LUA_NOREF;
    bool ok = LuaHookManager::Get().UnregisterCustomEvent(eventName, preRef, postRef);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Hook_IsKeyBindRegistered(lua_State* L) {

    std::string keyName;
    if (lua_isnumber(L, 1)) keyName = VkToCanonicalName(static_cast<int>(lua_tointeger(L, 1)));
    else keyName = luaL_checkstring(L, 1);
    lua_pushboolean(L, LuaHookManager::Get().IsKeyBindRegistered(keyName) ? 1 : 0);
    return 1;
}

static int Hook_ExecuteAsync(lua_State* L) {

    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    LuaHookManager::Get().ExecuteWithDelay(0, cbRef);
    return 0;
}

static int Hook_RegisterConsoleCommandHandler(lua_State* L) {
    const char* cmdName = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    LuaHookManager::Get().RegisterConsoleCommandHandler(cmdName, cbRef);
    return 0;
}

static int Hook_ExecuteConsoleCommand(lua_State* L) {
    const char* cmd = luaL_checkstring(L, 1);
    bool ok = LuaHookManager::Get().ExecuteConsoleCommand(cmd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Hook_RegisterBeginPlayHook(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int preRef = luaL_ref(L, LUA_REGISTRYINDEX);
    int postRef = LUA_NOREF;
    if (lua_gettop(L) >= 2 && lua_isfunction(L, 2)) {
        lua_pushvalue(L, 2);
        postRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    uint32_t id = LuaHookManager::Get().RegisterBeginPlayHook(preRef, postRef);
    lua_pushinteger(L, id);
    return 1;
}

static int Hook_RegisterEndPlayHook(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int preRef = luaL_ref(L, LUA_REGISTRYINDEX);
    int postRef = LUA_NOREF;
    if (lua_gettop(L) >= 2 && lua_isfunction(L, 2)) {
        lua_pushvalue(L, 2);
        postRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    uint32_t id = LuaHookManager::Get().RegisterEndPlayHook(preRef, postRef);
    lua_pushinteger(L, id);
    return 1;
}

static int Hook_RegisterLoadMapHook(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int preRef = luaL_ref(L, LUA_REGISTRYINDEX);
    int postRef = LUA_NOREF;
    if (lua_gettop(L) >= 2 && lua_isfunction(L, 2)) {
        lua_pushvalue(L, 2);
        postRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    uint32_t id = LuaHookManager::Get().RegisterLoadMapHook(preRef, postRef);
    lua_pushinteger(L, id);
    return 1;
}

static int Hook_RegisterInitGameStateHook(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int preRef = luaL_ref(L, LUA_REGISTRYINDEX);
    int postRef = LUA_NOREF;
    if (lua_gettop(L) >= 2 && lua_isfunction(L, 2)) {
        lua_pushvalue(L, 2);
        postRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    uint32_t id = LuaHookManager::Get().RegisterInitGameStateHook(preRef, postRef);
    lua_pushinteger(L, id);
    return 1;
}

static int Hook_ExecuteInGameThread(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    LuaHookManager::Get().EnqueueGameThreadTask(cbRef);
    return 0;
}

static int Hook_ExecuteWithDelay(lua_State* L) {

    int64_t delay = 0;
    int cbIdx = 2;
    uint64_t userHandle = 0;
    if (lua_gettop(L) >= 3 && lua_isinteger(L, 1) && lua_isinteger(L, 2)) {
        userHandle = static_cast<uint64_t>(lua_tointeger(L, 1));
        delay = luaL_checkinteger(L, 2);
        cbIdx = 3;
    } else {
        delay = luaL_checkinteger(L, 1);
    }
    luaL_checktype(L, cbIdx, LUA_TFUNCTION);
    lua_pushvalue(L, cbIdx);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    uint32_t timerId = userHandle != 0
        ? LuaHookManager::Get().ExecuteWithDelayUsingHandle(userHandle, delay, cbRef, false)
        : LuaHookManager::Get().ExecuteWithDelay(delay, cbRef);
    lua_pushinteger(L, timerId);
    return 1;
}

static int Hook_LoopAsync(lua_State* L) {
    int64_t interval = luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    uint32_t timerId = LuaHookManager::Get().LoopAsync(interval, cbRef);
    lua_pushinteger(L, timerId);
    return 1;
}

static int Hook_CancelTimer(lua_State* L) {
    uint32_t timerId = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    bool ok = LuaHookManager::Get().CancelTimer(timerId);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Hook_MakeActionHandle(lua_State* L) {
    lua_pushinteger(L, LuaHookManager::Get().MakeActionHandle());
    return 1;
}

static int Hook_RetriggerableDelay(lua_State* L) {
    uint64_t userHandle = static_cast<uint64_t>(luaL_checkinteger(L, 1));
    int64_t delay = luaL_checkinteger(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    uint32_t timerId = LuaHookManager::Get().ExecuteWithDelayUsingHandle(userHandle, delay, cbRef, true);
    lua_pushinteger(L, timerId);
    return 1;
}

static int Hook_AfterFrames(lua_State* L) {
    int64_t frames = luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    uint32_t timerId = LuaHookManager::Get().ExecuteAfterFrames(frames, cbRef);
    lua_pushinteger(L, timerId);
    return 1;
}

static int Hook_LoopAfterFrames(lua_State* L) {
    int64_t frames = luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    uint32_t timerId = LuaHookManager::Get().LoopAfterFrames(frames, cbRef);
    lua_pushinteger(L, timerId);
    return 1;
}

static int Hook_PauseTimer(lua_State* L) {
    uint32_t timerId = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, LuaHookManager::Get().PauseTimer(timerId) ? 1 : 0);
    return 1;
}

static int Hook_UnpauseTimer(lua_State* L) {
    uint32_t timerId = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, LuaHookManager::Get().UnpauseTimer(timerId) ? 1 : 0);
    return 1;
}

static int Hook_ResetTimer(lua_State* L) {
    uint32_t timerId = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, LuaHookManager::Get().ResetTimer(timerId) ? 1 : 0);
    return 1;
}

static int Hook_SetTimerDelay(lua_State* L) {
    uint32_t timerId = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int64_t delay = static_cast<int64_t>(luaL_checkinteger(L, 2));
    lua_pushboolean(L, LuaHookManager::Get().SetTimerDelay(timerId, delay) ? 1 : 0);
    return 1;
}

static int Hook_IsValidTimerHandle(lua_State* L) {
    uint32_t timerId = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, LuaHookManager::Get().IsValidTimerHandle(timerId) ? 1 : 0);
    return 1;
}

static int Hook_IsTimerActive(lua_State* L) {
    uint32_t timerId = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, LuaHookManager::Get().IsTimerActive(timerId) ? 1 : 0);
    return 1;
}

static int Hook_IsTimerPaused(lua_State* L) {
    uint32_t timerId = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, LuaHookManager::Get().IsTimerPaused(timerId) ? 1 : 0);
    return 1;
}

static int Hook_GetTimerTimeRemaining(lua_State* L) {
    uint32_t timerId = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, LuaHookManager::Get().GetTimerTimeRemaining(timerId));
    return 1;
}

static int Hook_GetTimerTimeElapsed(lua_State* L) {
    uint32_t timerId = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, LuaHookManager::Get().GetTimerTimeElapsed(timerId));
    return 1;
}

static int Hook_GetTimerRate(lua_State* L) {
    uint32_t timerId = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, LuaHookManager::Get().GetTimerRate(timerId));
    return 1;
}

static int Hook_ClearAllDelayedActions(lua_State* L) {
    lua_pushinteger(L, LuaHookManager::Get().CancelAllTimers());
    return 1;
}

void RegisterLuaHookBindings(lua_State* L) {

    lua_pushcfunction(L, Hook_RegisterHook);                 lua_setglobal(L, "RegisterHook");
    lua_pushcfunction(L, Hook_RegisterHook);                 lua_setglobal(L, "Hook");
    lua_pushcfunction(L, Hook_UnregisterHook);               lua_setglobal(L, "UnregisterHook");
    lua_pushcfunction(L, Hook_UnregisterHook);               lua_setglobal(L, "Unhook");

    lua_pushcfunction(L, Hook_RegisterCustomEvent);          lua_setglobal(L, "RegisterCustomEvent");
    lua_pushcfunction(L, Hook_RegisterCustomEvent);          lua_setglobal(L, "HookCustomEvent");
    lua_pushcfunction(L, Hook_UnregisterCustomEvent);        lua_setglobal(L, "UnregisterCustomEvent");
    lua_pushcfunction(L, Hook_UnregisterCustomEvent);        lua_setglobal(L, "UnhookCustomEvent");

    lua_pushcfunction(L, Hook_IsKeyBindRegistered);          lua_setglobal(L, "IsKeyBindRegistered");

    lua_pushcfunction(L, Hook_RegisterKeyBind);              lua_setglobal(L, "RegisterKeyBind");
    lua_pushcfunction(L, Hook_RegisterKeyBind);              lua_setglobal(L, "RegisterKeybind");
    lua_pushcfunction(L, Hook_RegisterKeyBind);              lua_setglobal(L, "BindKey");
    lua_pushcfunction(L, Hook_IsKeyDown);                    lua_setglobal(L, "IsKeyDown");
    lua_pushcfunction(L, Hook_IsKeyDown);                    lua_setglobal(L, "IsKeyPressed");
    lua_pushcfunction(L, Hook_IsKeyDown);                    lua_setglobal(L, "IsKeyHeld");

    lua_pushcfunction(L, Hook_NotifyOnNewObject);            lua_setglobal(L, "NotifyOnNewObject");
    lua_pushcfunction(L, Hook_NotifyOnNewObject);            lua_setglobal(L, "OnNewObject");

    lua_pushcfunction(L, Hook_RegisterConsoleCommandHandler);lua_setglobal(L, "RegisterConsoleCommandHandler");
    lua_pushcfunction(L, Hook_RegisterConsoleCommandHandler);lua_setglobal(L, "RegisterConsoleCommand");
    lua_pushcfunction(L, Hook_RegisterConsoleCommandHandler);lua_setglobal(L, "RegisterCommand");
    lua_pushcfunction(L, Hook_ExecuteConsoleCommand);        lua_setglobal(L, "ExecuteConsoleCommand");
    lua_pushcfunction(L, Hook_ExecuteConsoleCommand);        lua_setglobal(L, "ExecuteCommand");

    lua_pushcfunction(L, Hook_RegisterBeginPlayHook);        lua_setglobal(L, "RegisterBeginPlayHook");
    lua_pushcfunction(L, Hook_RegisterBeginPlayHook);        lua_setglobal(L, "OnBeginPlay");
    lua_pushcfunction(L, Hook_RegisterEndPlayHook);          lua_setglobal(L, "RegisterEndPlayHook");
    lua_pushcfunction(L, Hook_RegisterEndPlayHook);          lua_setglobal(L, "OnEndPlay");
    lua_pushcfunction(L, Hook_RegisterLoadMapHook);          lua_setglobal(L, "RegisterLoadMapHook");
    lua_pushcfunction(L, Hook_RegisterLoadMapHook);          lua_setglobal(L, "OnLoadMap");
    lua_pushcfunction(L, Hook_RegisterInitGameStateHook);    lua_setglobal(L, "RegisterInitGameStateHook");
    lua_pushcfunction(L, Hook_RegisterInitGameStateHook);    lua_setglobal(L, "OnInitGameState");

    lua_pushcfunction(L, Hook_ExecuteInGameThread);          lua_setglobal(L, "ExecuteInGameThread");
    lua_pushcfunction(L, Hook_ExecuteInGameThread);          lua_setglobal(L, "RunOnGameThread");
    lua_pushcfunction(L, Hook_ExecuteInGameThread);          lua_setglobal(L, "NextTick");
    lua_pushcfunction(L, Hook_ExecuteWithDelay);             lua_setglobal(L, "ExecuteWithDelay");
    lua_pushcfunction(L, Hook_ExecuteWithDelay);             lua_setglobal(L, "SetTimeout");
    lua_pushcfunction(L, Hook_LoopAsync);                    lua_setglobal(L, "LoopAsync");
    lua_pushcfunction(L, Hook_LoopAsync);                    lua_setglobal(L, "SetInterval");
    lua_pushcfunction(L, Hook_LoopAsync);                    lua_setglobal(L, "SetTimer");
    lua_pushcfunction(L, Hook_CancelTimer);                  lua_setglobal(L, "CancelTimer");
    lua_pushcfunction(L, Hook_CancelTimer);                  lua_setglobal(L, "ClearTimer");
    lua_pushcfunction(L, Hook_CancelTimer);                  lua_setglobal(L, "CancelDelayedAction");

    lua_pushcfunction(L, Hook_MakeActionHandle);             lua_setglobal(L, "MakeActionHandle");
    lua_pushcfunction(L, Hook_ExecuteWithDelay);             lua_setglobal(L, "ExecuteInGameThreadWithDelay");
    lua_pushcfunction(L, Hook_RetriggerableDelay);           lua_setglobal(L, "RetriggerableExecuteInGameThreadWithDelay");
    lua_pushcfunction(L, Hook_ExecuteWithDelay);             lua_setglobal(L, "ExecuteInGameThreadAfterFrames_DelayStyle");
    lua_pushcfunction(L, Hook_AfterFrames);                  lua_setglobal(L, "ExecuteInGameThreadAfterFrames");
    lua_pushcfunction(L, Hook_LoopAfterFrames);              lua_setglobal(L, "LoopInGameThreadAfterFrames");
    lua_pushcfunction(L, Hook_LoopAsync);                    lua_setglobal(L, "LoopInGameThreadWithDelay");

    lua_pushcfunction(L, Hook_PauseTimer);                   lua_setglobal(L, "PauseDelayedAction");
    lua_pushcfunction(L, Hook_UnpauseTimer);                 lua_setglobal(L, "UnpauseDelayedAction");
    lua_pushcfunction(L, Hook_ResetTimer);                   lua_setglobal(L, "ResetDelayedActionTimer");
    lua_pushcfunction(L, Hook_SetTimerDelay);                lua_setglobal(L, "SetDelayedActionTimer");
    lua_pushcfunction(L, Hook_IsValidTimerHandle);           lua_setglobal(L, "IsValidDelayedActionHandle");
    lua_pushcfunction(L, Hook_IsTimerActive);                lua_setglobal(L, "IsDelayedActionActive");
    lua_pushcfunction(L, Hook_IsTimerPaused);                lua_setglobal(L, "IsDelayedActionPaused");
    lua_pushcfunction(L, Hook_GetTimerTimeRemaining);        lua_setglobal(L, "GetDelayedActionTimeRemaining");
    lua_pushcfunction(L, Hook_GetTimerTimeElapsed);          lua_setglobal(L, "GetDelayedActionTimeElapsed");
    lua_pushcfunction(L, Hook_GetTimerRate);                 lua_setglobal(L, "GetDelayedActionRate");
    lua_pushcfunction(L, Hook_ClearAllDelayedActions);       lua_setglobal(L, "ClearAllDelayedActions");

    lua_pushcfunction(L, Hook_ExecuteAsync);                 lua_setglobal(L, "ExecuteAsync");

    lua_pushcfunction(L, LuaTypeGenerator::Lua_GenerateLuaTypes); lua_setglobal(L, "GenerateLuaTypes");
    lua_pushcfunction(L, LuaTypeGenerator::Lua_DumpAllObjects);   lua_setglobal(L, "DumpAllObjects");

    lua_pushcfunction(L, Compat_ForEachUObjectIndexed);           lua_setglobal(L, "ForEachUObject");
    lua_pushcfunction(L, LuaTypeGenerator::Lua_ForEachUObject);   lua_setglobal(L, "ForEachUObjectFlat");

    RegisterCompatApi(L);
    RegisterCompatParityApi(L);
    RegisterCompatSurface(L);

    RegisterKeyTable(L);
}

}
