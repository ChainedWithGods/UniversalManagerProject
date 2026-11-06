#include "LuaCompatApi.h"
#include "LuaUObject.h"
#include "LuaTypes.h"
#include "LuaHookManager.h"
#include "LuaManager.h"
#include "../core/BlueprintDiscovery.h"
#include "../core/EngineDiscovery.h"
#include "../core/NameResolution.h"
#include "../core/ObjectUtils.h"
#include "../console/ConsoleInternal.h"
#include "../utils/SafeMemory.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

extern EngineState g_State;

namespace LuaEngine {

static void LogLine(const std::string& msg) {
    EngineDiscovery::WriteLogLine(msg.c_str());
}

bool CompatStringFromArg(lua_State* L, int idx, std::string& out) {
    out.clear();
    if (lua_isstring(L, idx)) {
        size_t len = 0;
        const char* s = lua_tolstring(L, idx, &len);
        out.assign(s ? s : "", len);
        return true;
    }
    if (lua_isnumber(L, idx)) {
        lua_pushvalue(L, idx);
        if (luaL_tolstring(L, -1, nullptr)) out = lua_tostring(L, -1);
        lua_pop(L, 2);
        return !out.empty();
    }
    if (!lua_isuserdata(L, idx)) return false;

    UObject* obj = GetUObject(L, idx);
    if (obj) {
        out = EngineDiscovery::GetObjectNameStr(g_State, obj);
        return !out.empty();
    }

    static const char* const accessors[] = { "ToString", "GetName", "GetFullName" };
    int top = lua_gettop(L);
    for (const char* accessor : accessors) {
        lua_pushvalue(L, idx);
        lua_getfield(L, -1, accessor);
        if (lua_isfunction(L, -1)) {
            lua_pushvalue(L, idx);
            if (lua_pcall(L, 1, 1, 0) == LUA_OK && lua_isstring(L, -1)) {
                out = lua_tostring(L, -1);
                lua_settop(L, top);
                if (!out.empty()) return true;
                continue;
            }
            lua_pop(L, 1);
        }
        lua_settop(L, top);
    }

    lua_pushvalue(L, idx);
    if (luaL_tolstring(L, -1, nullptr)) out = lua_tostring(L, -1);
    lua_settop(L, top);
    return !out.empty();
}

static const char* OUTPUTDEVICE_MT = "Avalonium.FOutputDevice";

static int OutputDevice_Log_Impl(lua_State* L) {
    std::string msg;
    int nargs = lua_gettop(L);
    for (int i = 1; i <= nargs; ++i) {
        if (i == 1 && (lua_isnoneornil(L, i) || luaL_testudata(L, i, OUTPUTDEVICE_MT))) continue;
        std::string part;
        if (lua_isstring(L, i)) part = lua_tostring(L, i);
        else if (CompatStringFromArg(L, i, part)) {  }
        else {
            lua_pushvalue(L, i);
            if (luaL_tolstring(L, -1, nullptr)) part = lua_tostring(L, -1);
            lua_pop(L, 2);
        }
        if (!msg.empty()) msg += "\t\t";
        msg += part;
    }

    LogLine("[Lua] " + msg);
    lua_pushboolean(L, 1);
    return 1;
}

static int OutputDevice_Warn_Impl(lua_State* L) {
    std::string msg;
    CompatStringFromArg(L, lua_isnoneornil(L, 2) ? 1 : 2, msg);
    if (msg.empty() && lua_isstring(L, 1)) msg = lua_tostring(L, 1);
    LogLine("[Lua][Warning] " + msg);
    lua_pushboolean(L, 1);
    return 1;
}

static int OutputDevice_IsValid_Impl(lua_State* L) {
    lua_pushboolean(L, 1);
    (void)L;
    return 1;
}

static int OutputDevice_TypeName_Impl(lua_State* L) {
    lua_pushstring(L, "FOutputDevice");
    return 1;
}

static int OutputDevice_Index(lua_State* L) {
    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }
    if (_stricmp(key, "Log") == 0)         { lua_pushcfunction(L, OutputDevice_Log_Impl);   return 1; }
    if (_stricmp(key, "Warn") == 0)        { lua_pushcfunction(L, OutputDevice_Warn_Impl);  return 1; }
    if (_stricmp(key, "IsValid") == 0)     { lua_pushcfunction(L, OutputDevice_IsValid_Impl); return 1; }
    if (_stricmp(key, "type") == 0)        { lua_pushcfunction(L, OutputDevice_TypeName_Impl); return 1; }

    lua_pushcfunction(L, [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    return 1;
}

static int OutputDevice_ToString(lua_State* L) {
    lua_pushstring(L, "[FOutputDevice]");
    (void)L;
    return 1;
}

void CompatPushOutputDevice(lua_State* L) {
    lua_newuserdata(L, 1);
    luaL_newmetatable(L, OUTPUTDEVICE_MT);

    lua_pushliteral(L, "__index");
    lua_rawget(L, -2);
    const bool needsSetup = !lua_isfunction(L, -1) && !lua_istable(L, -1);
    lua_pop(L, 1);
    if (needsSetup) {
        lua_pushcfunction(L, OutputDevice_Index);     lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, OutputDevice_ToString);  lua_setfield(L, -2, "__tostring");
        lua_pushstring(L, "FOutputDevice");           lua_setfield(L, -2, "__name");
    }

    lua_setmetatable(L, -2);
}

static int Compat_Log(lua_State* L) {
    int nargs = lua_gettop(L);
    std::string msg;
    for (int i = 1; i <= nargs; ++i) {
        std::string part;
        if (lua_isstring(L, i)) part = lua_tostring(L, i);
        else if (!CompatStringFromArg(L, i, part)) {
            lua_pushvalue(L, i);
            if (luaL_tolstring(L, -1, nullptr)) part = lua_tostring(L, -1);
            lua_pop(L, 2);
        }
        if (!msg.empty()) msg += "\t\t";
        msg += part;
    }

    LogLine("[Lua] " + msg);
    return 0;
}

static int Compat_DerefToInt32(lua_State* L) {
    if (lua_gettop(L) != 1 || !lua_isinteger(L, 1)) {
        LogLine("[Lua] 'DerefToInt32' expects a single address argument");
        lua_pushnil(L);
        return 1;
    }
    uintptr_t addr = static_cast<uintptr_t>(lua_tointeger(L, 1));
    int32_t value = 0;
    if (addr < 0x10000 || !SafeMem_Read(reinterpret_cast<void*>(addr), &value, sizeof(value))) {
        LogLine("[Lua] 'DerefToInt32' was given an address that cannot be read");
        lua_pushnil(L);
        return 1;
    }
    if (value == 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, value);
    return 1;
}

static std::string CompatLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

static std::string CompatUpper(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

static int Str_ToString_Impl(lua_State* L) {
    size_t len = 0;
    const char* s = lua_tolstring(L, 1, &len);
    lua_pushlstring(L, s ? s : "", s ? len : 0);
    return 1;
}

static int Str_Len_Impl(lua_State* L) {
    size_t len = 0;
    lua_tolstring(L, 1, &len);
    lua_pushinteger(L, static_cast<lua_Integer>(len));
    return 1;
}

static int Str_IsEmpty_Impl(lua_State* L) {
    size_t len = 0;
    const char* s = lua_tolstring(L, 1, &len);
    bool empty = (s == nullptr) || (len == 0);
    if (!empty) {

        for (size_t i = 0; i < len; ++i) {
            if (!isspace(static_cast<unsigned char>(s[i]))) { empty = false; break; }
            empty = true;
        }
    }
    lua_pushboolean(L, empty ? 1 : 0);
    return 1;
}

static int Str_Find_Impl(lua_State* L) {
    size_t len = 0, slen = 0;
    const char* s = lua_tolstring(L, 1, &len);
    std::string needle;
    if (!CompatStringFromArg(L, 2, needle)) needle = lua_isnoneornil(L, 2) ? std::string() : std::string();
    if (needle.empty()) { lua_pushnil(L); return 1; }
    const char* h = lua_tolstring(L, 2, &slen);
    if (!s || !h || len < slen) { lua_pushnil(L); return 1; }
    for (size_t i = 0; i + slen <= len; ++i) {
        if (memcmp(s + i, h, slen) == 0) {
            lua_pushinteger(L, static_cast<lua_Integer>(i + 1));
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int Str_StartsWith_Impl(lua_State* L) {
    size_t len = 0, plen = 0;
    const char* s = lua_tolstring(L, 1, &len);
    const char* p = lua_tolstring(L, 2, &plen);
    bool ok = s && p && plen <= len && memcmp(s, p, plen) == 0;
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Str_EndsWith_Impl(lua_State* L) {
    size_t len = 0, slen = 0;
    const char* s = lua_tolstring(L, 1, &len);
    const char* sfx = lua_tolstring(L, 2, &slen);
    bool ok = s && sfx && slen <= len && memcmp(s + (len - slen), sfx, slen) == 0;
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Str_ToUpper_Impl(lua_State* L) {
    size_t len = 0;
    const char* s = lua_tolstring(L, 1, &len);
    std::string out = CompatUpper(s ? std::string(s, len) : std::string());
    lua_pushlstring(L, out.data(), out.size());
    return 1;
}

static int Str_ToLower_Impl(lua_State* L) {
    size_t len = 0;
    const char* s = lua_tolstring(L, 1, &len);
    std::string out = CompatLower(s ? std::string(s, len) : std::string());
    lua_pushlstring(L, out.data(), out.size());
    return 1;
}

static int Str_Append_Impl(lua_State* L) {
    size_t len = 0;
    const char* s = lua_tolstring(L, 1, &len);
    std::string out = s ? std::string(s, len) : std::string();
    size_t alen = 0;
    const char* a = lua_tolstring(L, 2, &alen);
    if (a) out.append(a, alen);
    lua_pushlstring(L, out.data(), out.size());
    return 1;
}

static int Str_Concat_Impl(lua_State* L) {
    return Str_Append_Impl(L);
}

static int Str_Clear_Impl(lua_State* L) {
    lua_pushliteral(L, "");
    (void)L;
    return 1;
}

static int Str_TypeName_Impl(lua_State* L) {
    lua_pushstring(L, "FString");
    (void)L;
    return 1;
}

static int Str_IsValid_Impl(lua_State* L) {
    size_t len = 0;
    const char* s = lua_tolstring(L, 1, &len);
    lua_pushboolean(L, (s != nullptr) ? 1 : 0);
    return 1;
}

static int Str_GetFName_Impl(lua_State* L) {
    size_t len = 0;
    const char* s = lua_tolstring(L, 1, &len);
    std::string name = s ? std::string(s, len) : std::string();
    int32_t idx = name.empty() ? -1 : EngineDiscovery::ResolveFNameIndex(g_State, name);
    PushFName(L, idx >= 0 ? static_cast<uint32_t>(idx) : 0);
    return 1;
}

static int Str_GetName_Impl(lua_State* L) {
    return Str_ToString_Impl(L);
}

static int Str_GetFullName_Impl(lua_State* L) {
    return Str_ToString_Impl(L);
}

static int Str_Clone_Impl(lua_State* L) {
    return Str_ToString_Impl(L);
}

static const struct { const char* Name; lua_CFunction Fn; } kStringMembers[] = {
    { "ToString",   Str_ToString_Impl },
    { "Len",        Str_Len_Impl },
    { "Length",     Str_Len_Impl },
    { "IsEmpty",    Str_IsEmpty_Impl },
    { "Find",       Str_Find_Impl },
    { "StartsWith", Str_StartsWith_Impl },
    { "EndsWith",   Str_EndsWith_Impl },
    { "ToUpper",    Str_ToUpper_Impl },
    { "ToLower",    Str_ToLower_Impl },
    { "Append",     Str_Append_Impl },
    { "Concat",     Str_Concat_Impl },
    { "Clear",      Str_Clear_Impl },
    { "Empty",      Str_Clear_Impl },
    { "IsValid",    Str_IsValid_Impl },
    { "Clone",      Str_Clone_Impl },
    { "GetFName",   Str_GetFName_Impl },
    { "GetName",    Str_GetName_Impl },
    { "GetFullName", Str_GetFullName_Impl },
    { "type",       Str_TypeName_Impl },
};

static void InstallStringMembers(lua_State* L) {
    for (const auto& m : kStringMembers) {
        lua_pushcfunction(L, m.Fn);
        lua_setfield(L, -2, m.Name);
    }
}

static void RegisterStringSurface(lua_State* L) {

    const int top = lua_gettop(L);
    bool installed = false;

    lua_pushliteral(L, "");
    if (lua_getmetatable(L, -1)) {
        lua_pushliteral(L, "__index");
        lua_rawget(L, -2);
        if (lua_istable(L, -1)) {
            InstallStringMembers(L);
            installed = true;
        }
    }
    lua_settop(L, top);

    if (!installed) {

        lua_getglobal(L, "string");
        if (lua_istable(L, -1)) {
            InstallStringMembers(L);
            installed = true;
        }
        lua_settop(L, top);
    }

    if (!installed) {
        LogLine("[Lua] String surface NOT installed: no string __index table to extend");
    }
}

bool CompatIsStructFamilyObject(UObject* obj) {
    if (!obj) return false;

    if (g_State.UStructClass &&
        ConsoleEnabler::IsObjectA(g_State, obj, g_State.UStructClass)) {
        return true;
    }

    if (ConsoleEnabler::IsClassObject(g_State, obj)) return true;

    if (g_State.UStruct_SuperOff < 0) return false;

    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    if (!cls) return false;

    static std::mutex s_FamilyMutex;
    static std::unordered_map<UClass*, bool> s_FamilyCache;
    {
        std::lock_guard<std::mutex> lock(s_FamilyMutex);
        auto it = s_FamilyCache.find(cls);
        if (it != s_FamilyCache.end()) return it->second;
    }

    bool family = false;
    UStruct* cur = reinterpret_cast<UStruct*>(cls);
    for (int depth = 0; depth < 32 && cur; ++depth) {

        const std::string name =
            EngineDiscovery::GetObjectNameStr(g_State, reinterpret_cast<UObject*>(cur));
        const bool structName = g_State.bUseCasePreservingName
                                    ? (name == "Struct" || name == "ScriptStruct")
                                    : (_stricmp(name.c_str(), "Struct") == 0 ||
                                       _stricmp(name.c_str(), "ScriptStruct") == 0);
        if (structName) {
            family = true;
            break;
        }
        cur = EngineDiscovery::GetSuperStruct(g_State, cur);
    }

    if (family) {
        static bool s_LoggedFallback = false;
        if (!s_LoggedFallback && !g_State.UStructClass) {
            s_LoggedFallback = true;
            LogLine("[Lua] Struct-family classification used the class-chain fallback "
                    "(the struct core class was not resolved)");
        }
    }

    {
        std::lock_guard<std::mutex> lock(s_FamilyMutex);
        s_FamilyCache[cls] = family;
    }
    return family;
}

static int Surf_TypeName(lua_State* L) {
    lua_pushstring(L, "UObject");
    return 1;
}

static int Surf_GetSuperStruct(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    if (!obj || g_State.UStruct_SuperOff < 0) { lua_pushnil(L); return 1; }

    if (!CompatIsStructFamilyObject(obj) && !ConsoleEnabler::IsClassObject(g_State, obj)) {
        lua_pushnil(L);
        return 1;
    }

    UStruct* sup = EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(obj));
    if (!sup || !SafeMem_IsReadable(sup, 32)) {
        static bool s_LoggedEmptyExtension = false;
        if (!s_LoggedEmptyExtension) {
            s_LoggedEmptyExtension = true;
            LogLine("[Lua] A struct-family object answered no super struct; the "
                    "super-field offset may not be resolved on this build");
        }
        lua_pushnil(L);
        return 1;
    }
    PushUObject(L, static_cast<UObject*>(sup));
    return 1;
}

static int Surf_GetOutermost(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    if (!obj) { lua_pushnil(L); return 1; }
    UObject* top = obj;
    for (int guard = 0; guard < 64; ++guard) {
        UObject* outer = EngineDiscovery::GetObjectOuter(g_State, top);
        if (!outer || outer == top || !SafeMem_IsReadable(outer, 32)) break;
        top = outer;
    }
    PushUObject(L, top);
    return 1;
}

static int Surf_IsMappedToObject(lua_State* L) {
    lua_pushboolean(L, GetUObject(L, 1) ? 1 : 0);
    return 1;
}

static int Surf_IsMappedToProperty(lua_State* L) {
    lua_pushboolean(L, 0);
    (void)L;
    return 1;
}

static void SplitCommand(const std::string& cmd, std::string& word, std::vector<std::string>& args) {
    size_t i = 0;
    while (i < cmd.size() && isspace(static_cast<unsigned char>(cmd[i]))) ++i;
    size_t start = i;
    while (i < cmd.size() && !isspace(static_cast<unsigned char>(cmd[i]))) ++i;
    word = cmd.substr(start, i - start);
    while (i < cmd.size()) {
        while (i < cmd.size() && isspace(static_cast<unsigned char>(cmd[i]))) ++i;
        size_t a = i;
        while (i < cmd.size() && !isspace(static_cast<unsigned char>(cmd[i]))) ++i;
        if (i > a) args.push_back(cmd.substr(a, i - a));
    }
}

static bool LooksNumeric(const std::string& s) {
    if (s.empty()) return false;
    char* end = nullptr;
    strtod(s.c_str(), &end);
    return end && *end == '\0';
}

static int Surf_ProcessConsoleExec(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    std::string cmd;
    if (!CompatStringFromArg(L, 2, cmd) || cmd.empty()) {
        return luaL_error(L, "'ProcessConsoleExec' requires a command string");
    }
    if (!obj) { lua_pushboolean(L, 0); return 1; }

    std::string word;
    std::vector<std::string> args;
    SplitCommand(cmd, word, args);

    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    void* ufunc = (cls && !word.empty()) ? FindFunctionOnClassEx(cls, word) : nullptr;
    if (ufunc) {
        int base = lua_gettop(L);
        for (const std::string& a : args) {
            if (LooksNumeric(a)) lua_pushnumber(L, strtod(a.c_str(), nullptr));
            else lua_pushstring(L, a.c_str());
        }

        CallUFunctionDynamic(L, obj, ufunc, base + 1);
        lua_settop(L, base);
        lua_pushboolean(L, 1);
        return 1;
    }

    bool ok = LuaHookManager::Get().ExecuteConsoleCommand(cmd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

struct SurfaceMember { const char* Name; lua_CFunction Fn; };

static const SurfaceMember kUObjectSurface[] = {
    { "type",                 Surf_TypeName },
    { "GetSuperStruct",       Surf_GetSuperStruct },
    { "GetOutermost",         Surf_GetOutermost },
    { "IsMappedToObject",     Surf_IsMappedToObject },
    { "IsMappedToProperty",   Surf_IsMappedToProperty },
    { "ProcessConsoleExec",   Surf_ProcessConsoleExec },
};

int Compat_Surface_Index(lua_State* L) {
    if (!lua_isstring(L, 2)) return 0;
    const char* key = lua_tostring(L, 2);
    if (!key) return 0;
    for (const SurfaceMember& m : kUObjectSurface) {
        if (_stricmp(key, m.Name) == 0) {
            lua_pushcfunction(L, m.Fn);
            return 1;
        }
    }
    return 0;
}

static void ReportNameCreationFailure(const std::string& why) {
    static std::mutex reportMutex;
    static std::unordered_map<std::string, bool> reported;

    std::lock_guard<std::mutex> lock(reportMutex);
    if (reported[why]) return;
    reported[why] = true;
    Log("[Lua] Cannot create names: %s", why.c_str());
}

static constexpr int32_t kFindNameFind = 0;

static int Surf_FName(lua_State* L) {

    if (lua_isinteger(L, 1)) {
        lua_Integer idx = lua_tointeger(L, 1);
        PushFName(L, static_cast<uint32_t>(idx < 0 ? 0 : idx),
                  static_cast<uint32_t>(luaL_optinteger(L, 3, 0)));
        return 1;
    }

    std::string name;
    if (!CompatStringFromArg(L, 1, name)) name.clear();
    if (name.empty()) {
        PushFName(L, 0);
        return 1;
    }

    const int32_t flag = static_cast<int32_t>(luaL_optinteger(L, 2, 1));
    if (flag == kFindNameFind) {
        const int32_t idx = EngineDiscovery::ResolveFNameIndex(g_State, name);
        PushFName(L, idx >= 0 ? static_cast<uint32_t>(idx) : 0);
        return 1;
    }

    std::string why;
    const int32_t idx = EngineDiscovery::FindOrAddFNameIndex(g_State, name, &why);
    if (idx < 0) {
        ReportNameCreationFailure(why);
        PushFName(L, 0);
        return 1;
    }
    PushFName(L, static_cast<uint32_t>(idx));
    return 1;
}

static int ModSurface_SetSharedVariable(lua_State* L) {

    const char* name = luaL_optstring(L, 2, "");
    if (!name || !name[0]) { lua_pushboolean(L, 0); return 1; }
    lua_getglobal(L, "_SharedVariables");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_newtable(L); lua_pushvalue(L, -1); lua_setglobal(L, "_SharedVariables"); }
    lua_pushstring(L, name);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);
    lua_pop(L, 1);
    lua_pushboolean(L, 1);
    return 1;
}

static int ModSurface_GetSharedVariable(lua_State* L) {
    const char* name = luaL_optstring(L, 2, "");
    if (!name || !name[0]) { lua_pushnil(L); return 1; }
    lua_getglobal(L, "_SharedVariables");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_pushnil(L); return 1; }
    lua_pushstring(L, name);
    lua_rawget(L, -2);
    lua_remove(L, -2);
    return 1;
}

static int ModSurface_OnUnloadAssign(lua_State* L, bool assign) {
    int top = lua_gettop(L);
    int cbIdx = top;
    lua_getglobal(L, "_ModUnloadCallbacks");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_newtable(L); lua_pushvalue(L, -1); lua_setglobal(L, "_ModUnloadCallbacks"); }
    std::string modName = DetectCurrentModName(L);
    if (modName.empty()) modName = "__main__";
    if (!assign) {
        lua_pushstring(L, modName.c_str());
        lua_rawget(L, -2);
        lua_remove(L, -2);
        return 1;
    }
    lua_pushstring(L, modName.c_str());
    lua_pushvalue(L, cbIdx);
    lua_settable(L, -3);
    lua_pop(L, 1);
    lua_pushboolean(L, 1);
    return 1;
}

static int ModSurface_SetOnUnload(lua_State* L) {
    return ModSurface_OnUnloadAssign(L, !lua_isnoneornil(L, 2));
}

static int ModSurface_TypeName(lua_State* L) {
    lua_pushstring(L, "ModRef");
    (void)L;
    return 1;
}

static int ModSurface_Index(lua_State* L) {
    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }
    if (_stricmp(key, "SetSharedVariable") == 0)  { lua_pushcfunction(L, ModSurface_SetSharedVariable); return 1; }
    if (_stricmp(key, "GetSharedVariable") == 0)  { lua_pushcfunction(L, ModSurface_GetSharedVariable); return 1; }
    if (_stricmp(key, "OnUnload") == 0)           { lua_pushcfunction(L, ModSurface_SetOnUnload);       return 1; }
    if (_stricmp(key, "type") == 0)               { lua_pushcfunction(L, ModSurface_TypeName);          return 1; }

    lua_pushvalue(L, 1);
    return 1;
}

static int ModSurface_NewIndex(lua_State* L) {
    const char* key = lua_tostring(L, 2);
    if (key && _stricmp(key, "OnUnload") == 0) {
        return ModSurface_OnUnloadAssign(L, !lua_isnoneornil(L, 3));
    }
    return 0;
}

static void RegisterModRefSurface(lua_State* L) {
    lua_getglobal(L, "ModRef");
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
    if (luaL_newmetatable(L, "Avalonium.ModRefFn")) {
        lua_pushcfunction(L, ModSurface_Index);    lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, ModSurface_NewIndex); lua_setfield(L, -2, "__newindex");
        lua_pushstring(L, "ModRef");               lua_setfield(L, -2, "__name");
    }
    lua_setmetatable(L, -2);
    lua_pop(L, 1);

    lua_getglobal(L, "ModRef");
    lua_setglobal(L, "Mod");
}

static void RegisterVersionSurface(lua_State* L) {

    lua_getglobal(L, "RuntimeVersion");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushinteger(L, COMPAT_API_VERSION_MAJOR);
        lua_setfield(L, -2, "Major");
        lua_pushinteger(L, COMPAT_API_VERSION_MINOR);
        lua_setfield(L, -2, "Hotfix");
    }
    lua_setglobal(L, "Avalonium");
}

void RegisterCompatSurface(lua_State* L) {

    lua_pushcfunction(L, Compat_Log);
    lua_setglobal(L, "Log");
    lua_getglobal(L, "print");
    if (lua_isfunction(L, -1)) {
        lua_setglobal(L, "Print");
    } else {
        lua_pop(L, 1);
        lua_pushcfunction(L, Compat_Log);
        lua_setglobal(L, "Print");
    }

    lua_pushcfunction(L, Compat_DerefToInt32);
    lua_setglobal(L, "DerefToInt32");
    lua_pushcfunction(L, Compat_DerefToInt32);
    lua_setglobal(L, "dereftoint32");

    lua_newtable(L);
    lua_pushinteger(L, 0); lua_setfield(L, -2, "EngineTick");
    lua_pushinteger(L, 1); lua_setfield(L, -2, "ProcessEvent");
    lua_setglobal(L, "EGameThreadMethod");

    lua_pushnil(L);
    lua_setglobal(L, "__OriginalReturnValue");

    RegisterStringSurface(L);
    RegisterModRefSurface(L);
    RegisterVersionSurface(L);

    lua_pushcfunction(L, Surf_FName);
    lua_setglobal(L, "FName");

    lua_newthread(L);
    lua_setglobal(L, "MainThread");
    lua_newthread(L);
    lua_setglobal(L, "HookThread");
    lua_newthread(L);
    lua_setglobal(L, "AsyncThread");

    CompatPushOutputDevice(L);
    lua_setglobal(L, "OutputDeviceRef");

    char buf[256];
    snprintf(buf, sizeof(buf),
             "[Lua] Compat surface v%d.%d: string-api=on, output-device=on, reflection-members=on",
             COMPAT_API_VERSION_MAJOR, COMPAT_API_VERSION_MINOR);
    LogLine(buf);
}

}
