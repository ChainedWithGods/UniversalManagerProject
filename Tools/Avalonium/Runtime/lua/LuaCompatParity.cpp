#include "LuaCompatApi.h"
#include "LuaUObject.h"
#include "LuaTypes.h"
#include "LuaHookManager.h"
#include "LuaManager.h"
#include "../core/EngineDiscovery.h"
#include "../core/NameResolution.h"
#include "../core/ObjectIndex.h"
#include "../core/ObjectUtils.h"
#include "../console/ConsoleInternal.h"
#include "../sdk/SDKGenerator.h"
#include "../utils/SafeMemory.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

extern EngineState g_State;

namespace fs = std::filesystem;

namespace LuaEngine {

static std::string ToLowerCopy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

static std::string GetExeDirectory() {
    char buf[MAX_PATH] = {};
    UINT len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return ".";
    std::string p = buf;
    size_t slash = p.find_last_of("\\/");
    return (slash == std::string::npos) ? "." : p.substr(0, slash);
}

static std::string GetModsRoot() {
    std::string modsDir = LuaManager::Get().GetModsDirectory();
    return modsDir.empty() ? GetExeDirectory() : modsDir;
}

static void LogLine(const std::string& msg) {
    EngineDiscovery::WriteLogLine(msg.c_str());
}

static uint32_t ReadObjectFlagsSafe(UObject* obj) {
    if (!obj || g_State.UObject_FlagsOff < 0) return 0;
    int32_t flags = 0;
    if (!SafeMem_Read(reinterpret_cast<uint8_t*>(obj) + g_State.UObject_FlagsOff, &flags, sizeof(flags))) return 0;
    return static_cast<uint32_t>(flags);
}

static constexpr int kFindObjectsMaxResults = 4096;

static const char* INVALID_OBJECT_MT = "Avalonium.InvalidObject";

static int InvalidObject_ReturnFalse(lua_State* L)  { lua_pushboolean(L, 0); return 1; }
static int InvalidObject_ReturnNone(lua_State* L)   { lua_pushstring(L, "None"); return 1; }
static int InvalidObject_ReturnNullAddr(lua_State* L) { lua_pushstring(L, "0x0000000000000000"); return 1; }
static int InvalidObject_ReturnNil(lua_State* L)    { (void)L; lua_pushnil(L); return 1; }
static int InvalidObject_ReturnTypeName(lua_State* L) { lua_pushstring(L, "InvalidObject"); return 1; }

static int InvalidObject_Index(lua_State* L) {

    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }
    if (_stricmp(key, "IsValid") == 0 || _stricmp(key, "IsShown") == 0) {
        lua_pushcfunction(L, InvalidObject_ReturnFalse);
        return 1;
    }
    if (_stricmp(key, "get") == 0 || _stricmp(key, "Get") == 0) {
        lua_pushcfunction(L, [](lua_State* L2) -> int { lua_pushvalue(L2, 1); return 1; });
        return 1;
    }
    if (_stricmp(key, "GetAddress") == 0 || _stricmp(key, "GetAddressString") == 0) {
        lua_pushcfunction(L, InvalidObject_ReturnNullAddr);
        return 1;
    }
    if (_stricmp(key, "GetFullName") == 0 || _stricmp(key, "GetPathName") == 0 ||
        _stricmp(key, "GetName") == 0) {
        lua_pushcfunction(L, InvalidObject_ReturnNone);
        return 1;
    }
    if (_stricmp(key, "GetFName") == 0) {
        lua_pushcfunction(L, [](lua_State* L2) -> int { PushFName(L2, 0, 0); return 1; });
        return 1;
    }
    if (_stricmp(key, "GetClass") == 0 || _stricmp(key, "GetOuter") == 0 || _stricmp(key, "GetWorld") == 0 ||
        _stricmp(key, "GetOutermost") == 0) {
        lua_pushcfunction(L, InvalidObject_ReturnNil);
        return 1;
    }
    if (_stricmp(key, "type") == 0) {
        lua_pushcfunction(L, InvalidObject_ReturnTypeName);
        return 1;
    }
    lua_pushcfunction(L, InvalidObject_ReturnNil);
    return 1;
}

static int InvalidObject_NewIndex(lua_State* L) {
    (void)L;
    return 0;
}

static int InvalidObject_ToString(lua_State* L) {
    lua_pushstring(L, "[InvalidObject]");
    return 1;
}

int CompatPushInvalidObject(lua_State* L) {
    lua_newuserdata(L, 1);
    if (luaL_newmetatable(L, INVALID_OBJECT_MT)) {
        lua_pushcfunction(L, InvalidObject_Index);     lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, InvalidObject_NewIndex);   lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, InvalidObject_ToString);   lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, InvalidObject_ReturnNil);  lua_setfield(L, -2, "__call");
    }
    lua_setmetatable(L, -2);
    return 1;
}

static int Compat_CreateInvalidObject(lua_State* L) {
    return CompatPushInvalidObject(L);
}

static bool ClassMatchesFilter(UObject* obj, UClass* filterClass, const std::string& filterClassNameLower, bool exactClass) {
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    if (filterClass) {
        if (cls == filterClass) return true;
        if (exactClass) return false;

        return cls && ConsoleEnabler::IsObjectA(g_State, obj, filterClass);
    }
    if (!filterClassNameLower.empty()) {
        if (!cls) return false;
        std::string clsName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cls));
        std::string clsFull = EngineDiscovery::BuildFullName(g_State, static_cast<UObject*>(cls));
        return ToLowerCopy(clsName) == filterClassNameLower || ToLowerCopy(clsFull) == filterClassNameLower;
    }
    return true;
}

static bool HasOuterChain(UObject* obj, UObject* outer) {
    if (!outer) return false;
    for (UObject* cur = obj; cur; ) {
        if (cur == outer) return true;
        if (g_State.UObject_OuterOff < 0) break;
        void* nextRaw = nullptr;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(cur) + g_State.UObject_OuterOff, &nextRaw, sizeof(nextRaw))) break;
        cur = static_cast<UObject*>(nextRaw);
        if (!SafeMem_IsReadable(cur, sizeof(void*))) break;
    }
    return false;
}

static bool ObjectNameMatches(UObject* obj, const std::string& nameLower) {
    if (nameLower.empty()) return true;
    std::string objName = EngineDiscovery::GetObjectNameStr(g_State, obj);
    if (ToLowerCopy(objName) == nameLower) return true;
    std::string full = EngineDiscovery::BuildFullName(g_State, obj);
    std::string fullLower = ToLowerCopy(full);
    if (fullLower == nameLower) return true;
    if (fullLower.size() >= nameLower.size() &&
        fullLower.compare(fullLower.size() - nameLower.size(), nameLower.size(), nameLower) == 0) {
        return true;
    }
    return false;
}

static int ScanObjects(lua_State* L, bool firstOnly) {
    UClass* filterClass = nullptr;
    UObject* outerFilter = nullptr;
    std::string filterClassName;
    std::string objectName;
    int64_t flagA = 0;
    int64_t flagB = 0;
    bool exactClass = false;
    std::string pathFilter;

    if (firstOnly) {

        enum { kMaxSlots = 6 };
        std::string slotText[kMaxSlots + 1];
        UObject* slotObj[kMaxSlots + 1] = {};
        bool slotBool[kMaxSlots + 1] = {};
        int64_t numerics[kMaxSlots + 1] = {};
        int numericCount = 0;

        const int slots = lua_gettop(L) > kMaxSlots ? kMaxSlots : lua_gettop(L);
        for (int i = 1; i <= slots; ++i) {
            if (lua_isnumber(L, i) && !lua_isstring(L, i)) {
                if (numericCount < kMaxSlots) numerics[numericCount++] = static_cast<int64_t>(lua_tointeger(L, i));
                continue;
            }
            if (lua_isboolean(L, i)) { slotBool[i] = lua_toboolean(L, i) != 0; continue; }
            if (lua_isuserdata(L, i)) {
                UObject* o = GetUObject(L, i);
                if (o) { slotObj[i] = o; continue; }
            }
            std::string text;
            if (CompatStringFromArg(L, i, text) && !text.empty()) slotText[i] = ToLowerCopy(text);
        }

        int classSlot = 0;
        for (int i = 1; i <= slots; ++i) {
            if (slotObj[i] && ConsoleEnabler::IsClassObject(g_State, slotObj[i])) { classSlot = i; break; }
            if (!slotText[i].empty()) {
                UClass* c = EngineDiscovery::FindClassCached(g_State, slotText[i].c_str());
                if (c) { filterClass = c; filterClassName = slotText[i]; classSlot = i; break; }
            }
        }
        if (classSlot && slotObj[classSlot]) filterClass = static_cast<UClass*>(slotObj[classSlot]);

        std::string firstText, lastText;
        int textCount = 0;
        for (int i = 1; i <= slots; ++i) {
            if (i == classSlot || slotText[i].empty()) continue;
            if (firstText.empty()) firstText = slotText[i];
            lastText = slotText[i];
            ++textCount;
        }
        bool classCameAsObject = classSlot && slotObj[classSlot] != nullptr;
        objectName = (classCameAsObject && textCount >= 2) ? lastText : firstText;
        if (objectName.empty()) objectName = firstText;

        {
            std::string raw;
            for (int i = 1; i <= slots; ++i) {
                if (i == classSlot) continue;
                if (!slotText[i].empty() && slotText[i][0] == '/') { raw = slotText[i]; break; }
            }
            if (!raw.empty()) {
                pathFilter = raw;
                size_t dot = objectName.rfind('.');
                if (dot != std::string::npos && dot + 1 < objectName.size()) objectName = objectName.substr(dot + 1);
            }
        }

        for (int i = 1; i <= slots; ++i) {
            if (i == classSlot || !slotObj[i]) continue;
            if (outerFilter == nullptr) outerFilter = slotObj[i];
        }
        for (int i = 1; i <= slots; ++i) {
            if (slotBool[i]) { exactClass = slotBool[i]; break; }
        }
        if (numericCount >= 1) flagA = numerics[0];
        if (numericCount >= 2) flagB = numerics[1];
    } else {

        int shift = 0;
        if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) shift = 1;
        else if (lua_gettop(L) >= 1 && (lua_isnoneornil(L, 1) || lua_tointeger(L, 1) == 0)) shift = 1;
        int nameSlot = 1 + shift;
        std::string text;
        if (lua_gettop(L) >= nameSlot && CompatStringFromArg(L, nameSlot, text) && !text.empty()) {
            objectName = ToLowerCopy(text);
        }
        int flagSlot = nameSlot + 1;
        if (lua_gettop(L) >= flagSlot && lua_isnumber(L, flagSlot)) flagA = static_cast<int64_t>(lua_tointeger(L, flagSlot));
        if (lua_gettop(L) >= flagSlot + 1 && lua_isnumber(L, flagSlot + 1)) flagB = static_cast<int64_t>(lua_tointeger(L, flagSlot + 1));
    }

    lua_newtable(L);
    int resultIndex = lua_gettop(L);
    int found = 0;
    int maxResults = firstOnly ? 1 : kFindObjectsMaxResults;
    if (!firstOnly && lua_gettop(L) >= 1 && lua_isnumber(L, 1) && lua_tointeger(L, 1) > 0) {
        maxResults = static_cast<int>(lua_tointeger(L, 1));
    }

    constexpr uint64_t kIntFlagPendingKill = 1LL << 29;
    constexpr uint64_t kIntFlagRootSet     = 1LL << 30;
    constexpr uint64_t kIntFlagNative      = 1LL << 25;

    int count = EngineDiscovery::GetObjectCount(g_State);
    for (int i = 0; i < count && found < maxResults; ++i) {
        UObject* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
        if (!obj) continue;

        uint32_t flags = ReadObjectFlagsSafe(obj);
        if (firstOnly) {
            if (flagA) {
                if (flagA & kIntFlagPendingKill) { if (flags & 0x00000100) continue; }
                if (flagA & kIntFlagRootSet)     { if (flags & 0x00000080) continue; }
            }
            if (flagB) {
                bool has = false;
                if (flagB & kIntFlagNative)  has |= (flags & 0x00000004) != 0;
                if (flagB & kIntFlagRootSet) has |= (flags & 0x00000080) != 0;
                if (flagB & ~(kIntFlagNative | kIntFlagRootSet)) has |= flags != 0;
                if (!has) continue;
            }
        } else {
            if (flagA) {
                bool has = false;
                if (flagA & kIntFlagNative)  has |= (flags & 0x00000004) != 0;
                if (flagA & kIntFlagRootSet) has |= (flags & 0x00000080) != 0;
                if (flagA & ~(kIntFlagNative | kIntFlagRootSet)) has |= flags != 0;
                if (!has) continue;
            }
            if (flagB) {
                if (flagB & kIntFlagPendingKill) { if (flags & 0x00000100) continue; }
                if (flagB & kIntFlagRootSet)     { if (flags & 0x00000080) continue; }
            }
        }

        if (!ClassMatchesFilter(obj, filterClass, filterClassName, exactClass)) continue;
        if (outerFilter && !HasOuterChain(obj, outerFilter)) continue;
        if (!ObjectNameMatches(obj, objectName)) continue;
        if (!pathFilter.empty()) {
            std::string full = ToLowerCopy(EngineDiscovery::BuildFullName(g_State, obj));
            if (full.find(pathFilter) == std::string::npos) continue;
        }

        if (firstOnly) {
            lua_pop(L, 1);
            PushUObject(L, obj);
            return 1;
        }
        lua_pushinteger(L, ++found);
        PushUObject(L, obj);
        lua_settable(L, resultIndex);
    }

    if (firstOnly) {
        lua_pop(L, 1);
        lua_pushnil(L);
    }
    return 1;
}

static int Compat_FindObject(lua_State* L) {
    return ScanObjects(L, true);
}

static int Compat_FindObjects(lua_State* L) {
    return ScanObjects(L, false);
}

static int Compat_LoadExport(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    if (!name || !name[0]) { lua_pushnil(L); return 1; }

    std::string modsRoot = GetModsRoot();
    std::vector<std::string> candidates;
    auto tryAdd = [&](const std::string& rel) {
        std::string path = modsRoot + "/" + rel;
        std::error_code ec;
        if (fs::exists(path, ec)) candidates.push_back(path);
    };
    tryAdd("shared/exports/" + std::string(name) + ".lua");
    tryAdd("shared/" + std::string(name) + ".lua");
    tryAdd("exports/" + std::string(name) + ".lua");
    tryAdd(std::string(name) + ".lua");

    if (candidates.empty()) {
        char buf[512];
        snprintf(buf, sizeof(buf), "[Lua] LoadExport: export module '%s' was not found in the mods tree", name);
        LogLine(buf);
        lua_pushnil(L);
        return 1;
    }

    int status = luaL_loadfile(L, candidates.front().c_str());
    if (status != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        char buf[512];
        snprintf(buf, sizeof(buf), "[Lua] LoadExport: failed to compile '%s': %s", candidates.front().c_str(), err ? err : "unknown");
        LogLine(buf);
        lua_pop(L, 1);
        lua_pushnil(L);
        return 1;
    }
    status = lua_pcall(L, 0, 1, 0);
    if (status != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        char buf[512];
        snprintf(buf, sizeof(buf), "[Lua] LoadExport: error running '%s': %s", candidates.front().c_str(), err ? err : "unknown");
        LogLine(buf);
        lua_pop(L, 1);
        lua_pushnil(L);
        return 1;
    }
    return 1;
}

std::string DetectCurrentModName(lua_State* L) {
    lua_Debug ar = {};
    for (int level = 1; level <= 10; ++level) {
        if (!lua_getstack(L, level, &ar)) break;
        if (!lua_getinfo(L, "S", &ar) || !ar.source || ar.source[0] != '@') continue;
        std::string src = ar.source + 1;
        std::replace(src.begin(), src.end(), '\\', '/');
        size_t modsPos = src.find("/Mods/");
        if (modsPos == std::string::npos) continue;
        size_t nameStart = modsPos + 6;
        size_t nameEnd = src.find('/', nameStart);
        if (nameEnd == std::string::npos || nameEnd == nameStart) continue;
        return src.substr(nameStart, nameEnd - nameStart);
    }
    return "";
}

static bool SetModsJsonEnabled(const std::string& modName, bool enabled) {
    std::string modsRoot = GetModsRoot();
    fs::path jsonPath = fs::path(modsRoot) / "mods.json";
    std::error_code ec;
    if (!fs::exists(jsonPath, ec)) return false;

    FILE* f = nullptr;
    if (fopen_s(&f, jsonPath.string().c_str(), "rb") != 0 || !f) return false;
    std::string src;
    char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) src.append(chunk, n);
    fclose(f);

    std::string needle = "\"name\"";
    size_t pos = 0;
    while ((pos = src.find(needle, pos)) != std::string::npos) {
        size_t quoteStart = src.find('"', pos + needle.size());
        if (quoteStart == std::string::npos) break;
        size_t quoteEnd = src.find('"', quoteStart + 1);
        if (quoteEnd == std::string::npos) break;
        std::string foundName = src.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        pos = quoteEnd + 1;
        if (_stricmp(foundName.c_str(), modName.c_str()) != 0) continue;

        size_t objEnd = src.find('}', quoteEnd);
        if (objEnd == std::string::npos) break;
        size_t enabledKey = src.find("\"enabled\"", quoteEnd);
        if (enabledKey == std::string::npos || enabledKey > objEnd) {

            return true;
        }
        size_t valueStart = src.find(':', enabledKey + 9);
        if (valueStart == std::string::npos || valueStart > objEnd) break;
        ++valueStart;
        while (valueStart < src.size() && (src[valueStart] == ' ' || src[valueStart] == '\t')) ++valueStart;
        size_t valueEnd = valueStart;
        while (valueEnd < src.size() && isalpha(static_cast<unsigned char>(src[valueEnd]))) ++valueEnd;

        std::string newValue = enabled ? "true" : "false";
        if (src.compare(valueStart, valueEnd - valueStart, newValue) == 0) return true;
        src.replace(valueStart, valueEnd - valueStart, newValue);

        FILE* out = nullptr;
        if (fopen_s(&out, jsonPath.string().c_str(), "wb") != 0 || !out) return false;
        fwrite(src.data(), 1, src.size(), out);
        fclose(out);
        return true;
    }
    return false;
}

static int DeferredReload(lua_State* L) {
    LuaManager::Get().ReloadMods();
    return 0;
}

static void ScheduleReload(lua_State* L) {

    lua_pushcfunction(L, DeferredReload);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    LuaHookManager::Get().ExecuteWithDelay(50, cbRef);
}

static int Compat_RestartMod(lua_State* L) {
    std::string modName;
    if (lua_isstring(L, 1)) {
        modName = lua_tostring(L, 1);
    } else {
        modName = DetectCurrentModName(L);
    }
    if (modName.empty()) {
        LogLine("[Lua] RestartMod: mod name could not be determined; reloading all mods");
    } else {
        char buf[512];
        snprintf(buf, sizeof(buf), "[Lua] RestartMod: '%s'", modName.c_str());
        LogLine(buf);
    }
    ScheduleReload(L);
    return 0;
}

static int Compat_UninstallMod(lua_State* L) {
    std::string modName;
    if (lua_isstring(L, 1)) {
        modName = lua_tostring(L, 1);
    } else {
        modName = DetectCurrentModName(L);
    }
    if (modName.empty()) {
        LogLine("[Lua] UninstallMod: mod name could not be determined; nothing to disable");
        return 0;
    }
    bool ok = SetModsJsonEnabled(modName, false);
    char buf[512];
    snprintf(buf, sizeof(buf), "[Lua] UninstallMod: '%s' %s", modName.c_str(), ok ? "disabled in mods.json" : "not found in mods.json (left enabled)");
    LogLine(buf);
    ScheduleReload(L);
    return 0;
}

static constexpr int kMaxDirectoryDepth = 12;
static constexpr int kMaxDirectoryNodes = 60000;
static constexpr int kMaxDirectoryFiles = 200000;
static int g_DirectoryNodeBudget = 0;
static int g_DirectoryFileBudget = 0;

static const char* DIRECTORY_MT = "Avalonium.GameDirectory";

static bool FindGameProjectDirectories(fs::path& projectDirectory,
                                      fs::path& gameRootDirectory) {
    std::error_code ec;
    fs::path dir = fs::path(GetExeDirectory());
    for (int up = 0; up < 6; ++up) {
        if (dir.empty() || !fs::is_directory(dir, ec)) break;
        if (fs::is_directory(dir / "Content", ec)) {
            projectDirectory = dir;
            fs::path parent = dir.parent_path();
            if (parent.empty() || parent == dir) break;
            gameRootDirectory = parent;
            return true;
        }
        fs::path parent = dir.parent_path();
        if (parent.empty() || parent == dir) break;
        dir = parent;
    }
    return false;
}

static void PushFileEntry(lua_State* L, const fs::path& file) {
    lua_newtable(L);
    std::string name = file.filename().string();
    std::string full = file.string();
    lua_pushlstring(L, name.data(), name.size());
    lua_setfield(L, -2, "__name");
    lua_pushlstring(L, full.data(), full.size());
    lua_setfield(L, -2, "__absolute_path");
}

static int Directory_Index(lua_State* L) {
    if (!lua_isstring(L, 2) || _stricmp(lua_tostring(L, 2), "__files") != 0) {
        lua_pushnil(L);
        return 1;
    }

    lua_getfield(L, 1, "__absolute_path");
    const char* path = lua_tostring(L, -1);
    std::string dirPath = path ? path : "";
    lua_pop(L, 1);

    lua_newtable(L);
    if (!dirPath.empty()) {
        std::error_code ec;
        int idx = 0;
        fs::directory_iterator it(fs::path(dirPath),
                                  fs::directory_options::skip_permission_denied, ec);
        fs::directory_iterator end;
        for (; it != end && !ec; it.increment(ec)) {
            if (g_DirectoryFileBudget <= 0) break;
            std::error_code entryEc;
            if (it->is_directory(entryEc) || entryEc) continue;
            --g_DirectoryFileBudget;
            lua_pushinteger(L, ++idx);
            PushFileEntry(L, it->path());
            lua_settable(L, -3);
        }
    }

    lua_pushvalue(L, -1);
    lua_setfield(L, 1, "__files");
    return 1;
}

static void PushDirectoryMetatable(lua_State* L) {
    if (luaL_newmetatable(L, DIRECTORY_MT)) {
        lua_pushcfunction(L, Directory_Index);
        lua_setfield(L, -2, "__index");
        lua_pushstring(L, "GameDirectory");
        lua_setfield(L, -2, "__name");
    }
    lua_setmetatable(L, -2);
}

static bool PushDirectoryTable(lua_State* L, const fs::path& dir, int depth,
                              const fs::path& projectDirectory);

static bool PushDirectoryTable(lua_State* L, const fs::path& dir, int depth,
                              const fs::path& projectDirectory) {
    if (depth > kMaxDirectoryDepth || g_DirectoryNodeBudget <= 0) return false;
    --g_DirectoryNodeBudget;

    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return false;

    lua_newtable(L);
    lua_pushstring(L, dir.filename().string().c_str());
    lua_setfield(L, -2, "__name");
    lua_pushstring(L, dir.string().c_str());
    lua_setfield(L, -2, "__absolute_path");
    PushDirectoryMetatable(L);

    std::vector<fs::path> subDirs;
    fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec);
    fs::directory_iterator end;
    for (; it != end && !ec; it.increment(ec)) {
        std::error_code entryEc;
        bool isDir = it->is_directory(entryEc);
        if (entryEc) continue;
        if (isDir) subDirs.push_back(it->path());
    }

    int parentIdx = lua_gettop(L);
    lua_newtable(L);
    int dirsIdx = lua_gettop(L);
    int dirIdx = 0;
    for (const fs::path& sub : subDirs) {
        if (!PushDirectoryTable(L, sub, depth + 1, projectDirectory)) continue;
        ++dirIdx;
        lua_pushinteger(L, dirIdx);
        lua_pushvalue(L, -2);
        lua_settable(L, dirsIdx);

        std::string childName = sub.filename().string();
        lua_pushstring(L, childName.c_str());
        lua_pushvalue(L, -2);
        lua_settable(L, parentIdx);

        if (!projectDirectory.empty() && sub == projectDirectory) {
            lua_pushstring(L, "Game");
            lua_pushvalue(L, -2);
            lua_settable(L, parentIdx);
        }

        lua_pop(L, 1);
    }
    lua_setfield(L, parentIdx, "__dirs");
    return true;
}

static int Compat_IterateGameDirectories(lua_State* L) {
    fs::path projectDirectory;
    fs::path gameRootDirectory;
    if (!FindGameProjectDirectories(projectDirectory, gameRootDirectory)) {

        char buf[512];
        snprintf(buf, sizeof(buf),
                 "[Lua] IterateGameDirectories: no Content directory was found above %s, so "
                 "the game directory structure is unknown",
                 GetExeDirectory().c_str());
        LogLine(buf);
        lua_pushnil(L);
        return 1;
    }

    g_DirectoryNodeBudget = kMaxDirectoryNodes;
    g_DirectoryFileBudget = kMaxDirectoryFiles;
    if (!PushDirectoryTable(L, gameRootDirectory, 0, projectDirectory)) {
        lua_pushnil(L);
    }
    return 1;
}

static int Compat_CreateLogicModsDirectory(lua_State* L) {
    fs::path projectDirectory;
    fs::path gameRootDirectory;
    if (!FindGameProjectDirectories(projectDirectory, gameRootDirectory)) {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "[Lua] CreateLogicModsDirectory: no Content directory was found above %s, so "
                 "the game directory structure is unknown",
                 GetExeDirectory().c_str());
        LogLine(buf);
        lua_pushboolean(L, 0);
        return 1;
    }

    std::error_code ec;
    fs::path paksDirectory = projectDirectory / "Content" / "Paks";
    fs::path logicMods = paksDirectory / "LogicMods";
    fs::create_directories(logicMods, ec);
    if (fs::is_directory(logicMods, ec)) {
        char buf[512];
        snprintf(buf, sizeof(buf), "[Lua] Logic-mods directory ready: %s", logicMods.string().c_str());
        LogLine(buf);
        lua_pushboolean(L, 1);
    } else {
        char buf[512];
        snprintf(buf, sizeof(buf), "[Lua] Failed to create logic-mods directory: %s", logicMods.string().c_str());
        LogLine(buf);
        lua_pushboolean(L, 0);
    }
    return 1;
}

struct CustomPropInfo {
    std::string Name;
    std::string TypeName;
    void* BelongsToClass = nullptr;
    std::string BelongsToClassName;
    int32_t Offset = 0;
    int32_t ArrayDim = 1;
    int32_t ElementSize = 8;
    uint64_t Flags = 0;
};

static std::mutex g_CustomPropMutex;
static std::vector<CustomPropInfo> g_CustomProps;
constexpr size_t kMaxCustomProps = 512;

static int32_t DefaultElementSizeForType(const std::string& typeName) {
    if (typeName.find("Int8Property") != std::string::npos || typeName.find("ByteProperty") != std::string::npos ||
        typeName.find("BoolProperty") != std::string::npos) return 1;
    if (typeName.find("Int16Property") != std::string::npos || typeName.find("UInt16Property") != std::string::npos) return 2;
    if (typeName.find("IntProperty") != std::string::npos || typeName.find("UInt32Property") != std::string::npos ||
        typeName.find("FloatProperty") != std::string::npos) return 4;
    if (typeName.find("Int64Property") != std::string::npos || typeName.find("UInt64Property") != std::string::npos ||
        typeName.find("DoubleProperty") != std::string::npos) return 8;
    if (typeName.find("NameProperty") != std::string::npos) return 8;
    if (typeName.find("StrProperty") != std::string::npos) return 16;
    if (typeName.find("TextProperty") != std::string::npos) return 16;
    return 8;
}

static int Compat_RegisterCustomProperty(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    CustomPropInfo info;

    lua_getfield(L, 1, "Name");
    if (lua_isstring(L, -1)) info.Name = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "Type");
    if (lua_isstring(L, -1)) {
        info.TypeName = lua_tostring(L, -1);
    } else if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "Name");
        if (lua_isstring(L, -1)) info.TypeName = lua_tostring(L, -1);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "BelongsToClass");
    if (lua_isuserdata(L, -1)) {
        UObject* cls = GetUObject(L, -1);
        info.BelongsToClass = cls;
        if (cls) info.BelongsToClassName = EngineDiscovery::GetObjectNameStr(g_State, cls);
    } else if (lua_isstring(L, -1)) {
        info.BelongsToClassName = lua_tostring(L, -1);

        UObject* resolved = EngineDiscovery::FindClassCached(g_State, info.BelongsToClassName.c_str());
        info.BelongsToClass = resolved;
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "OffsetInternal");
    if (lua_isnumber(L, -1)) info.Offset = static_cast<int32_t>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "ArrayDim");
    if (lua_isnumber(L, -1)) info.ArrayDim = static_cast<int32_t>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "PropertyFlags");
    if (lua_isnumber(L, -1)) info.Flags = static_cast<uint64_t>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    if (info.Name.empty() || info.TypeName.empty()) {
        return luaL_error(L, "RegisterCustomProperty requires 'Name' and 'Type' fields");
    }
    if (info.BelongsToClass == nullptr && info.BelongsToClassName.empty()) {
        return luaL_error(L, "RegisterCustomProperty requires a 'BelongsToClass' object or class name");
    }
    info.ElementSize = DefaultElementSizeForType(info.TypeName);

    std::lock_guard<std::mutex> lock(g_CustomPropMutex);

    for (auto& existing : g_CustomProps) {
        if (_stricmp(existing.Name.c_str(), info.Name.c_str()) == 0 &&
            ((existing.BelongsToClass && existing.BelongsToClass == info.BelongsToClass) ||
             (!info.BelongsToClass && _stricmp(existing.BelongsToClassName.c_str(), info.BelongsToClassName.c_str()) == 0))) {
            existing = info;
            lua_pushboolean(L, 1);
            return 1;
        }
    }
    if (g_CustomProps.size() >= kMaxCustomProps) {
        g_CustomProps.erase(g_CustomProps.begin());
    }
    g_CustomProps.push_back(info);

    char buf[512];
    snprintf(buf, sizeof(buf), "[Lua] Custom property '%s' (%s @ +0x%X) registered on '%s'",
             info.Name.c_str(), info.TypeName.c_str(), info.Offset,
             info.BelongsToClassName.c_str());
    LogLine(buf);
    lua_pushboolean(L, 1);
    return 1;
}

bool FindCustomPropertyForClass(UClass* cls, const std::string& key, void** outProp,
                                int32_t* outOffset, int32_t* outElementSize, std::string& outTypeName) {
    if (!cls || key.empty()) return false;
    std::lock_guard<std::mutex> lock(g_CustomPropMutex);
    if (g_CustomProps.empty()) return false;

    UStruct* cur = static_cast<UStruct*>(cls);
    while (cur) {
        for (const CustomPropInfo& info : g_CustomProps) {
            bool classMatches = false;
            if (info.BelongsToClass) {
                classMatches = (info.BelongsToClass == static_cast<void*>(cur));
            } else if (!info.BelongsToClassName.empty()) {
                std::string curName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cur));
                std::string curFull = EngineDiscovery::BuildFullName(g_State, static_cast<UObject*>(cur));
                std::string want = ToLowerCopy(info.BelongsToClassName);
                classMatches = ToLowerCopy(curName) == want ||
                               (curFull.size() >= want.size() &&
                                ToLowerCopy(curFull).compare(curFull.size() - want.size(), want.size(), want) == 0);
            }
            if (!classMatches) continue;
            if (_stricmp(info.Name.c_str(), key.c_str()) != 0) continue;

            if (outProp) *outProp = nullptr;
            if (outOffset) *outOffset = info.Offset;
            if (outElementSize) *outElementSize = info.ElementSize;
            outTypeName = info.TypeName;
            return true;
        }
        cur = EngineDiscovery::GetSuperStruct(g_State, cur);
    }
    return false;
}

int PushCustomPropertyValue(lua_State* L, uint8_t* baseAddr, int32_t offset,
                           int32_t elementSize, const std::string& typeName) {
    uint8_t* addr = baseAddr + offset;
    if (!SafeMem_IsReadable(addr, static_cast<size_t>(std::max<int32_t>(elementSize, 1)))) {
        lua_pushnil(L);
        return 1;
    }

    if (typeName.find("BoolProperty") != std::string::npos) {
        uint8_t v = 0;
        if (!SafeMem_Read(addr, &v, 1)) { lua_pushnil(L); return 1; }
        lua_pushboolean(L, v ? 1 : 0);
        return 1;
    }
    if (typeName.find("FloatProperty") != std::string::npos) {
        float v = 0.0f;
        if (!SafeMem_Read(addr, &v, sizeof(v))) { lua_pushnil(L); return 1; }
        lua_pushnumber(L, v);
        return 1;
    }
    if (typeName.find("DoubleProperty") != std::string::npos) {
        double v = 0.0;
        if (!SafeMem_Read(addr, &v, sizeof(v))) { lua_pushnil(L); return 1; }
        lua_pushnumber(L, v);
        return 1;
    }
    if (typeName.find("Int64Property") != std::string::npos || typeName.find("UInt64Property") != std::string::npos) {
        int64_t v = 0;
        if (!SafeMem_Read(addr, &v, sizeof(v))) { lua_pushnil(L); return 1; }
        lua_pushinteger(L, static_cast<lua_Integer>(v));
        return 1;
    }
    if (typeName.find("IntProperty") != std::string::npos || typeName.find("UInt32Property") != std::string::npos ||
        typeName.find("Int16Property") != std::string::npos || typeName.find("UInt16Property") != std::string::npos ||
        typeName.find("Int8Property") != std::string::npos || typeName.find("ByteProperty") != std::string::npos) {
        int32_t v = 0;
        if (elementSize == 1) { uint8_t b = 0; if (!SafeMem_Read(addr, &b, 1)) { lua_pushnil(L); return 1; } v = b; }
        else if (elementSize == 2) { int16_t h = 0; if (!SafeMem_Read(addr, &h, 2)) { lua_pushnil(L); return 1; } v = h; }
        else { if (!SafeMem_Read(addr, &v, 4)) { lua_pushnil(L); return 1; } }
        lua_pushinteger(L, v);
        return 1;
    }
    if (typeName.find("NameProperty") != std::string::npos) {
        uint32_t nameIdx = 0;
        if (!SafeMem_Read(addr, &nameIdx, 4)) { lua_pushnil(L); return 1; }
        std::string s = EngineDiscovery::GetCachedFName(g_State, static_cast<int32_t>(nameIdx));
        lua_pushstring(L, s.c_str());
        return 1;
    }
    if (typeName.find("StrProperty") != std::string::npos || typeName.find("Utf8StrProperty") != std::string::npos ||
        typeName.find("AnsiStrProperty") != std::string::npos) {

        struct { void* Data; int32_t Num; int32_t Max; } header = {};
        if (!SafeMem_Read(addr, &header, sizeof(header)) || !header.Data || header.Num <= 0 || header.Num > 8192) {
            lua_pushstring(L, "");
            return 1;
        }
        std::string out;
        if (typeName.find("AnsiStrProperty") != std::string::npos) {
            std::vector<char> buf(static_cast<size_t>(header.Num) + 1, '\0');
            if (SafeMem_Read(header.Data, buf.data(), static_cast<size_t>(header.Num))) out.assign(buf.data());
        } else {
            std::vector<wchar_t> wide(static_cast<size_t>(header.Num) + 1, L'\0');
            if (SafeMem_Read(header.Data, wide.data(), static_cast<size_t>(header.Num) * sizeof(wchar_t))) {
                int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), header.Num, nullptr, 0, nullptr, nullptr);
                if (utf8Len > 0) {
                    out.resize(static_cast<size_t>(utf8Len));
                    WideCharToMultiByte(CP_UTF8, 0, wide.data(), header.Num, out.data(), utf8Len, nullptr, nullptr);
                }
            }
        }
        lua_pushstring(L, out.c_str());
        return 1;
    }
    if (typeName.find("ObjectProperty") != std::string::npos || typeName.find("ClassProperty") != std::string::npos ||
        typeName.find("ObjectPtrProperty") != std::string::npos || typeName.find("InterfaceProperty") != std::string::npos) {
        void* ptr = nullptr;
        if (!SafeMem_Read(addr, &ptr, sizeof(ptr))) { lua_pushnil(L); return 1; }
        PushUObject(L, static_cast<UObject*>(ptr));
        return 1;
    }
    if (typeName.find("StructProperty") != std::string::npos) {

        lua_newtable(L);
        for (int32_t b = 0; b < elementSize && b < 256; ++b) {
            uint8_t byte = 0;
            if (!SafeMem_Read(addr + b, &byte, 1)) break;
            lua_pushinteger(L, b + 1);
            lua_pushinteger(L, byte);
            lua_settable(L, -3);
        }
        return 1;
    }

    int64_t raw = 0;
    if (!SafeMem_Read(addr, &raw, sizeof(raw))) { lua_pushnil(L); return 1; }
    lua_pushinteger(L, static_cast<lua_Integer>(raw));
    return 1;
}

bool SetCustomPropertyValue(lua_State* L, uint8_t* baseAddr, int32_t offset,
                           int32_t elementSize, const std::string& typeName, int valIdx) {
    uint8_t* addr = baseAddr + offset;
    if (!SafeMem_IsReadable(addr, static_cast<size_t>(std::max<int32_t>(elementSize, 1)))) return false;

    if (typeName.find("BoolProperty") != std::string::npos) {
        uint8_t v = lua_toboolean(L, valIdx) ? 1 : 0;
        return SafeMem_Write(addr, &v, 1);
    }
    if (typeName.find("FloatProperty") != std::string::npos) {
        float v = static_cast<float>(luaL_checknumber(L, valIdx));
        return SafeMem_Write(addr, &v, sizeof(v));
    }
    if (typeName.find("DoubleProperty") != std::string::npos) {
        double v = luaL_checknumber(L, valIdx);
        return SafeMem_Write(addr, &v, sizeof(v));
    }
    if (typeName.find("Int64Property") != std::string::npos || typeName.find("UInt64Property") != std::string::npos) {
        int64_t v = static_cast<int64_t>(luaL_checkinteger(L, valIdx));
        return SafeMem_Write(addr, &v, sizeof(v));
    }
    if (typeName.find("IntProperty") != std::string::npos || typeName.find("UInt32Property") != std::string::npos ||
        typeName.find("Int16Property") != std::string::npos || typeName.find("UInt16Property") != std::string::npos ||
        typeName.find("Int8Property") != std::string::npos || typeName.find("ByteProperty") != std::string::npos) {
        int32_t v = static_cast<int32_t>(luaL_checkinteger(L, valIdx));
        if (elementSize == 1) { uint8_t b = static_cast<uint8_t>(v); return SafeMem_Write(addr, &b, 1); }
        if (elementSize == 2) { int16_t h = static_cast<int16_t>(v); return SafeMem_Write(addr, &h, 2); }
        return SafeMem_Write(addr, &v, 4);
    }
    if (typeName.find("ObjectProperty") != std::string::npos || typeName.find("ClassProperty") != std::string::npos ||
        typeName.find("ObjectPtrProperty") != std::string::npos || typeName.find("InterfaceProperty") != std::string::npos) {
        void* ptr = GetUObject(L, valIdx);
        return SafeMem_Write(addr, &ptr, sizeof(ptr));
    }
    if (typeName.find("NameProperty") != std::string::npos) {
        const char* s = luaL_checkstring(L, valIdx);
        int32_t idx = EngineDiscovery::ResolveFNameIndex(g_State, s);
        uint32_t nameIdx = static_cast<uint32_t>(idx >= 0 ? idx : 0);
        return SafeMem_Write(addr, &nameIdx, 4);
    }
    return false;
}

static int Compat_RegisterConsoleCommandGlobalHandler(lua_State* L) {

    std::string commandName;
    int callbackIndex = -1;
    if (lua_isfunction(L, 1)) {
        callbackIndex = 1;
    } else if (lua_isstring(L, 1) && lua_isfunction(L, 2)) {
        commandName = lua_tostring(L, 1);
        callbackIndex = 2;
    }
    if (callbackIndex < 0) {
        return luaL_error(L,
                          "bad argument #1 to 'RegisterConsoleCommandGlobalHandler' "
                          "(function expected, got %s) -- the form the other runtime uses is "
                          "RegisterConsoleCommandGlobalHandler(commandName, callback)",
                          luaL_typename(L, 1));
    }

    lua_pushvalue(L, callbackIndex);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    LuaHookManager::Get().RegisterGlobalConsoleHandler(cbRef, commandName);
    return 0;
}

static int ExecLifecycleHook_Impl(lua_State* L) {
    const char* fnName = lua_tostring(L, lua_upvalueindex(1));
    int slot = static_cast<int>(lua_tointeger(L, lua_upvalueindex(2)));
    luaL_checktype(L, 1, LUA_TFUNCTION);

    int preRef = LUA_NOREF;
    int postRef = LUA_NOREF;
    lua_pushvalue(L, 1);
    if (slot == 0) preRef = luaL_ref(L, LUA_REGISTRYINDEX);
    else           postRef = luaL_ref(L, LUA_REGISTRYINDEX);

    uint32_t preId = 0;
    uint32_t postId = 0;

    const bool consoleExecEvent = _stricmp(fnName, "ProcessConsoleExec") == 0 ||
                                  _stricmp(fnName, "CallFunctionByNameWithArguments") == 0 ||
                                  _stricmp(fnName, "Exec") == 0;
    if (consoleExecEvent) {
        LuaHookManager::Get().RegisterExecLifecycleHook(fnName, preRef, postRef, &preId, &postId);
    } else {
        LuaHookManager::Get().RegisterHookCompat(fnName, preRef, postRef, &preId, &postId);

        char buf[512];
        snprintf(buf, sizeof(buf), "[LuaHook] Best-effort hook on '%s' (%s): fires when the engine exposes it as a script function",
                 fnName, slot == 0 ? "pre" : "post");
        LogLine(buf);
    }

    lua_pushinteger(L, preId);
    lua_pushinteger(L, postId);
    return 2;
}

enum class LifecycleEvent { BeginPlay, EndPlay, LoadMap, InitGameState };

static int RegisterLifecycleHook(lua_State* L, LifecycleEvent ev, bool asPre) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    int preRef = asPre ? cbRef : LUA_NOREF;
    int postRef = asPre ? LUA_NOREF : cbRef;
    LuaHookManager& mgr = LuaHookManager::Get();
    uint32_t id = 0;
    switch (ev) {
        case LifecycleEvent::BeginPlay:     id = mgr.RegisterBeginPlayHook(preRef, postRef); break;
        case LifecycleEvent::EndPlay:       id = mgr.RegisterEndPlayHook(preRef, postRef); break;
        case LifecycleEvent::LoadMap:       id = mgr.RegisterLoadMapHook(preRef, postRef); break;
        case LifecycleEvent::InitGameState: id = mgr.RegisterInitGameStateHook(preRef, postRef); break;
    }
    lua_pushinteger(L, id);
    return 1;
}

static int Compat_RegisterBeginPlayPreHook(lua_State* L)     { return RegisterLifecycleHook(L, LifecycleEvent::BeginPlay, true); }
static int Compat_RegisterBeginPlayPostHook(lua_State* L)    { return RegisterLifecycleHook(L, LifecycleEvent::BeginPlay, false); }
static int Compat_RegisterEndPlayPreHook(lua_State* L)       { return RegisterLifecycleHook(L, LifecycleEvent::EndPlay, true); }
static int Compat_RegisterEndPlayPostHook(lua_State* L)      { return RegisterLifecycleHook(L, LifecycleEvent::EndPlay, false); }
static int Compat_RegisterLoadMapPreHook(lua_State* L)       { return RegisterLifecycleHook(L, LifecycleEvent::LoadMap, true); }
static int Compat_RegisterLoadMapPostHook(lua_State* L)      { return RegisterLifecycleHook(L, LifecycleEvent::LoadMap, false); }
static int Compat_RegisterInitGameStatePreHook(lua_State* L) { return RegisterLifecycleHook(L, LifecycleEvent::InitGameState, true); }
static int Compat_RegisterInitGameStatePostHook(lua_State* L){ return RegisterLifecycleHook(L, LifecycleEvent::InitGameState, false); }

static int Compat_GenerateSDK(lua_State* L) {
    std::string outDir = luaL_optstring(L, 1, "");
    if (outDir.empty()) outDir = (fs::path(GetModsRoot()) / "SDK").string();
    std::error_code ec;
    fs::create_directories(outDir, ec);
    bool ok = SDKGenerator::GenerateSDK(g_State, outDir);
    char buf[512];
    snprintf(buf, sizeof(buf), "[Lua] SDK generation %s -> %s", ok ? "finished" : "failed", outDir.c_str());
    LogLine(buf);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Compat_GenerateUHTCompatibleHeaders(lua_State* L) {
    std::string outDir = luaL_optstring(L, 1, "");
    if (outDir.empty()) outDir = (fs::path(GetModsRoot()) / "SDK_UHT").string();
    std::error_code ec;
    fs::create_directories(outDir, ec);
    bool ok = SDKGenerator::GenerateSDK(g_State, outDir);
    char buf[512];
    snprintf(buf, sizeof(buf), "[Lua] Header generation %s -> %s", ok ? "finished" : "failed", outDir.c_str());
    LogLine(buf);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Compat_DumpUSMAP(lua_State* L) {
    std::string outPath = luaL_optstring(L, 1, "");
    if (outPath.empty()) outPath = (fs::path(GetModsRoot()) / ".." / "Avalonium.usmap").string();
    bool ok = EngineDiscovery::GenerateUSMap(g_State, outPath);
    char buf[512];
    snprintf(buf, sizeof(buf), "[Lua] USMAP dump %s -> %s", ok ? "finished" : "failed", outPath.c_str());
    LogLine(buf);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Compat_DumpJMAP(lua_State* L) {
    std::string outPath = luaL_optstring(L, 1, "");
    if (outPath.empty()) outPath = (fs::path(GetModsRoot()) / ".." / "Avalonium.jmap").string();

    bool ok = EngineDiscovery::GenerateUSMap(g_State, outPath);
    char buf[512];
    snprintf(buf, sizeof(buf), "[Lua] Reflection map dump %s -> %s", ok ? "finished" : "failed", outPath.c_str());
    LogLine(buf);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Compat_DumpAllActors(lua_State* L) {
    UClass* actorClass = g_State.AActorClass;
    int count = EngineDiscovery::GetObjectCount(g_State);
    int matched = 0;
    constexpr int kMaxLogged = 500;
    LogLine("[Lua] ---- Actor dump begin ----");
    for (int i = 0; i < count; ++i) {
        UObject* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
        if (!obj) continue;
        bool isActor = actorClass ? ConsoleEnabler::IsObjectA(g_State, obj, actorClass) : false;
        if (!isActor) continue;
        ++matched;
        if (matched <= kMaxLogged) {
            UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
            std::string clsName = cls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cls)) : "Unknown";
            std::string full = EngineDiscovery::BuildFullName(g_State, obj);
            char buf[1024];
            snprintf(buf, sizeof(buf), "[Lua]   Actor[%d] %s (%s)", matched, full.c_str(), clsName.c_str());
            LogLine(buf);
        }
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "[Lua] ---- Actor dump end: %d actors ----", matched);
    LogLine(buf);
    lua_pushinteger(L, matched);
    return 1;
}

static int Compat_DumpStaticMeshes(lua_State* L) {
    int count = EngineDiscovery::GetObjectCount(g_State);
    int matched = 0;
    constexpr int kMaxLogged = 500;
    LogLine("[Lua] ---- Static mesh dump begin ----");
    for (int i = 0; i < count; ++i) {
        UObject* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
        if (!obj) continue;
        UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
        if (!cls) continue;
        std::string clsName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cls));
        if (_stricmp(clsName.c_str(), "StaticMesh") != 0) continue;
        ++matched;
        if (matched <= kMaxLogged) {
            std::string full = EngineDiscovery::BuildFullName(g_State, obj);
            char buf[1024];
            snprintf(buf, sizeof(buf), "[Lua]   StaticMesh[%d] %s", matched, full.c_str());
            LogLine(buf);
        }
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "[Lua] ---- Static mesh dump end: %d meshes ----", matched);
    LogLine(buf);
    lua_pushinteger(L, matched);
    return 1;
}

static const char* FQUAT_MT = "Avalonium.FQuat";
static const char* FVEC2_MT = "Avalonium.FVector2D";
static const char* FVEC4_MT = "Avalonium.FVector4";

struct QuatData { float X, Y, Z, W; };
struct Vec2Data { float X, Y; };
struct Vec4Data { float X, Y, Z, W; };

void PushFQuat(lua_State* L, float x, float y, float z, float w) {
    auto* q = static_cast<QuatData*>(lua_newuserdata(L, sizeof(QuatData)));
    q->X = x; q->Y = y; q->Z = z; q->W = w;
    luaL_setmetatable(L, FQUAT_MT);
}

void PushFVector2D(lua_State* L, float x, float y) {
    auto* v = static_cast<Vec2Data*>(lua_newuserdata(L, sizeof(Vec2Data)));
    v->X = x; v->Y = y;
    luaL_setmetatable(L, FVEC2_MT);
}

void PushFVector4(lua_State* L, float x, float y, float z, float w) {
    auto* v = static_cast<Vec4Data*>(lua_newuserdata(L, sizeof(Vec4Data)));
    v->X = x; v->Y = y; v->Z = z; v->W = w;
    luaL_setmetatable(L, FVEC4_MT);
}

bool GetQuatOrVec4Value(lua_State* L, int idx, float outXYZW[4]) {
    if (auto* q = static_cast<QuatData*>(luaL_testudata(L, idx, FQUAT_MT))) {
        outXYZW[0] = q->X; outXYZW[1] = q->Y; outXYZW[2] = q->Z; outXYZW[3] = q->W;
        return true;
    }
    if (auto* v = static_cast<Vec4Data*>(luaL_testudata(L, idx, FVEC4_MT))) {
        outXYZW[0] = v->X; outXYZW[1] = v->Y; outXYZW[2] = v->Z; outXYZW[3] = v->W;
        return true;
    }
    if (lua_istable(L, idx)) {
        bool ok = true;
        const char* keys[4] = { "X", "Y", "Z", "W" };
        for (int i = 0; i < 4; ++i) {
            lua_getfield(L, idx, keys[i]);
            if (lua_isnumber(L, -1)) outXYZW[i] = static_cast<float>(lua_tonumber(L, -1));
            else ok = false;
            lua_pop(L, 1);
        }
        return ok;
    }
    return false;
}

bool GetVec2ValueEx(lua_State* L, int idx, float& x, float& y) {
    if (auto* v = static_cast<Vec2Data*>(luaL_testudata(L, idx, FVEC2_MT))) {
        x = v->X; y = v->Y;
        return true;
    }
    if (lua_istable(L, idx)) {
        bool ok = true;
        lua_getfield(L, idx, "X"); if (lua_isnumber(L, -1)) x = static_cast<float>(lua_tonumber(L, -1)); else ok = false; lua_pop(L, 1);
        lua_getfield(L, idx, "Y"); if (lua_isnumber(L, -1)) y = static_cast<float>(lua_tonumber(L, -1)); else ok = false; lua_pop(L, 1);
        return ok;
    }
    return false;
}

static constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
static constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;

static int Quat_ToRotator(lua_State* L) {
    auto* q = static_cast<QuatData*>(luaL_checkudata(L, 1, FQUAT_MT));

    float pitch = std::asin(std::max(-1.0f, std::min(1.0f, -2.0f * (q->Y * q->Z - q->W * q->X))));
    float yaw   = std::atan2(2.0f * (q->W * q->Y + q->X * q->Z), q->W * q->W - q->X * q->X - q->Y * q->Y + q->Z * q->Z);
    float roll  = std::atan2(2.0f * (q->W * q->Z + q->X * q->Y), q->W * q->W + q->X * q->X - q->Y * q->Y - q->Z * q->Z);
    PushRotator(L, pitch * kRadToDeg, yaw * kRadToDeg, roll * kRadToDeg);
    return 1;
}

static int Quat_Index(lua_State* L) {
    auto* q = static_cast<QuatData*>(luaL_checkudata(L, 1, FQUAT_MT));
    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }
    if (_stricmp(key, "X") == 0) { lua_pushnumber(L, q->X); return 1; }
    if (_stricmp(key, "Y") == 0) { lua_pushnumber(L, q->Y); return 1; }
    if (_stricmp(key, "Z") == 0) { lua_pushnumber(L, q->Z); return 1; }
    if (_stricmp(key, "W") == 0) { lua_pushnumber(L, q->W); return 1; }
    if (_stricmp(key, "ToRotator") == 0 || _stricmp(key, "Rotator") == 0) { lua_pushcfunction(L, Quat_ToRotator); return 1; }
    if (_stricmp(key, "type") == 0) { lua_pushstring(L, "FQuat"); return 1; }
    lua_pushnil(L);
    return 1;
}

static int Quat_NewIndex(lua_State* L) {
    auto* q = static_cast<QuatData*>(luaL_checkudata(L, 1, FQUAT_MT));
    const char* key = luaL_checkstring(L, 2);
    float val = static_cast<float>(luaL_checknumber(L, 3));
    if (_stricmp(key, "X") == 0) q->X = val;
    else if (_stricmp(key, "Y") == 0) q->Y = val;
    else if (_stricmp(key, "Z") == 0) q->Z = val;
    else if (_stricmp(key, "W") == 0) q->W = val;
    return 0;
}

static int Quat_ToString(lua_State* L) {
    auto* q = static_cast<QuatData*>(luaL_checkudata(L, 1, FQUAT_MT));
    char buf[128];
    snprintf(buf, sizeof(buf), "FQuat(%.4f, %.4f, %.4f, %.4f)", q->X, q->Y, q->Z, q->W);
    lua_pushstring(L, buf);
    return 1;
}

static int Quat_Constructor(lua_State* L) {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
    if (lua_gettop(L) >= 4) {
        x = static_cast<float>(luaL_checknumber(L, 1));
        y = static_cast<float>(luaL_checknumber(L, 2));
        z = static_cast<float>(luaL_checknumber(L, 3));
        w = static_cast<float>(luaL_checknumber(L, 4));
    }
    PushFQuat(L, x, y, z, w);
    return 1;
}

static int Vec2_Index(lua_State* L) {
    auto* v = static_cast<Vec2Data*>(luaL_checkudata(L, 1, FVEC2_MT));
    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }
    if (_stricmp(key, "X") == 0) { lua_pushnumber(L, v->X); return 1; }
    if (_stricmp(key, "Y") == 0) { lua_pushnumber(L, v->Y); return 1; }
    if (_stricmp(key, "Length") == 0 || _stricmp(key, "Size") == 0) {
        lua_pushnumber(L, std::sqrt(v->X * v->X + v->Y * v->Y));
        return 1;
    }
    if (_stricmp(key, "type") == 0) { lua_pushstring(L, "FVector2D"); return 1; }
    lua_pushnil(L);
    return 1;
}

static int Vec2_NewIndex(lua_State* L) {
    auto* v = static_cast<Vec2Data*>(luaL_checkudata(L, 1, FVEC2_MT));
    const char* key = luaL_checkstring(L, 2);
    float val = static_cast<float>(luaL_checknumber(L, 3));
    if (_stricmp(key, "X") == 0) v->X = val;
    else if (_stricmp(key, "Y") == 0) v->Y = val;
    return 0;
}

static int Vec2_ToString(lua_State* L) {
    auto* v = static_cast<Vec2Data*>(luaL_checkudata(L, 1, FVEC2_MT));
    char buf[96];
    snprintf(buf, sizeof(buf), "FVector2D(%.4f, %.4f)", v->X, v->Y);
    lua_pushstring(L, buf);
    return 1;
}

static int Vec2_Constructor(lua_State* L) {
    float x = luaL_optnumber(L, 1, 0.0);
    float y = luaL_optnumber(L, 2, 0.0);
    PushFVector2D(L, static_cast<float>(x), static_cast<float>(y));
    return 1;
}

static int Vec4_Index(lua_State* L) {
    auto* v = static_cast<Vec4Data*>(luaL_checkudata(L, 1, FVEC4_MT));
    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }
    if (_stricmp(key, "X") == 0) { lua_pushnumber(L, v->X); return 1; }
    if (_stricmp(key, "Y") == 0) { lua_pushnumber(L, v->Y); return 1; }
    if (_stricmp(key, "Z") == 0) { lua_pushnumber(L, v->Z); return 1; }
    if (_stricmp(key, "W") == 0) { lua_pushnumber(L, v->W); return 1; }
    if (_stricmp(key, "type") == 0) { lua_pushstring(L, "FVector4"); return 1; }
    lua_pushnil(L);
    return 1;
}

static int Vec4_NewIndex(lua_State* L) {
    auto* v = static_cast<Vec4Data*>(luaL_checkudata(L, 1, FVEC4_MT));
    const char* key = luaL_checkstring(L, 2);
    float val = static_cast<float>(luaL_checknumber(L, 3));
    if (_stricmp(key, "X") == 0) v->X = val;
    else if (_stricmp(key, "Y") == 0) v->Y = val;
    else if (_stricmp(key, "Z") == 0) v->Z = val;
    else if (_stricmp(key, "W") == 0) v->W = val;
    return 0;
}

static int Vec4_ToString(lua_State* L) {
    auto* v = static_cast<Vec4Data*>(luaL_checkudata(L, 1, FVEC4_MT));
    char buf[128];
    snprintf(buf, sizeof(buf), "FVector4(%.4f, %.4f, %.4f, %.4f)", v->X, v->Y, v->Z, v->W);
    lua_pushstring(L, buf);
    return 1;
}

static int Vec4_Constructor(lua_State* L) {
    float x = luaL_optnumber(L, 1, 0.0);
    float y = luaL_optnumber(L, 2, 0.0);
    float z = luaL_optnumber(L, 3, 0.0);
    float w = luaL_optnumber(L, 4, 0.0);
    PushFVector4(L, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(w));
    return 1;
}

static UObject* SelfObject(lua_State* L) {
    return GetUObject(L, 1);
}

static std::string ClassNameOf(UObject* obj) {
    if (!obj) return "";
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    return cls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cls)) : std::string();
}

static bool IsEnumObject(UObject* obj) {
    if (!obj) return false;
    if (g_State.UEnumClass && EngineDiscovery::GetObjectClass(g_State, obj) == g_State.UEnumClass) return true;
    return ClassNameOf(obj).find("Enum") != std::string::npos;
}

static bool IsFunctionObject(UObject* obj) {
    if (!obj) return false;
    if (g_State.UFunctionClass && EngineDiscovery::GetObjectClass(g_State, obj) == g_State.UFunctionClass) return true;
    return ClassNameOf(obj).find("Function") != std::string::npos;
}

static bool IsDataTableObject(UObject* obj) {
    return obj && ClassNameOf(obj).find("DataTable") != std::string::npos;
}

static bool LooksLikeStructObject(UObject* obj) {
    if (!obj) return false;
    if (g_State.UStructClass && ConsoleEnabler::IsObjectA(g_State, obj, g_State.UStructClass)) return true;
    std::string c = ClassNameOf(obj);
    if (c == "Class" || c == "ScriptStruct" || c == "Struct" || c == "BlueprintGeneratedClass") return true;
    return c.find("Struct") != std::string::npos;
}

static void* ReadPtrMember(void* base, int32_t off) {
    if (!base || off < 0) return nullptr;
    void* v = nullptr;
    if (!SafeMem_Read(reinterpret_cast<uint8_t*>(base) + off, &v, sizeof(v))) return nullptr;
    return v;
}

static std::string PropNameOf(void* prop) {
    if (!prop) return "";
    std::string n = EngineDiscovery::GetFFieldName(g_State, prop);
    if (n.empty() || n[0] == '<') {
        n = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(prop));
    }
    return n;
}

static std::string PropTypeOf(void* prop) {
    if (!prop) return "";
    std::string t = EngineDiscovery::GetFFieldTypeName(g_State, prop);
    if (t.empty() || t[0] == '<') {
        UClass* cls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(prop));
        if (cls) t = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cls));
    }
    return t;
}

static int32_t PropOffsetOf(void* prop) {
    if (!prop) return -1;
    int32_t off = -1;
    if (g_State.FProperty_OffsetOff >= 0 &&
        SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.FProperty_OffsetOff, &off, 4)) {
        return off;
    }
    off = -1;
    if (g_State.UProperty_OffsetOff >= 0 &&
        SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.UProperty_OffsetOff, &off, 4)) {
        return off;
    }
    return -1;
}

static int32_t PropElementSizeOf(void* prop) {
    int32_t sz = 0;
    if (prop && g_State.FProperty_ElementSizeOff >= 0) {
        SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.FProperty_ElementSizeOff, &sz, 4);
    }
    if (sz <= 0) {

        int32_t legacy = 0;
        if (prop && g_State.FProperty_OffsetOff >= 0 &&
            SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.FProperty_OffsetOff + 8, &legacy, 4) &&
            legacy > 0 && legacy <= 0x10000) {
            sz = legacy;
        }
    }
    return sz;
}

static int32_t PropArrayDimOf(void* prop) {
    int32_t d = 0;
    if (prop && g_State.FProperty_ArrayDimOff >= 0) {
        SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.FProperty_ArrayDimOff, &d, 4);
    }
    return d > 0 ? d : 1;
}

static uint64_t PropFlagsOf(void* prop) {
    uint64_t f = 0;
    if (prop && g_State.FProperty_PropertyFlagsOff >= 0) {
        SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.FProperty_PropertyFlagsOff, &f, 8);
    }
    return f;
}

static void PropBoolBits(void* prop, uint8_t& byteOffset, uint8_t& byteMask,
                         uint8_t& fieldMask, uint8_t& fieldSize) {
    byteOffset = 0; byteMask = 1; fieldMask = 1; fieldSize = 0;
    if (!prop || g_State.FBoolProperty_BaseOff < 0) return;
    SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.FBoolProperty_BaseOff + 0, &fieldSize, 1);
    SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.FBoolProperty_BaseOff + 1, &byteOffset, 1);
    SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.FBoolProperty_BaseOff + 2, &byteMask, 1);
    SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + g_State.FBoolProperty_BaseOff + 3, &fieldMask, 1);
    if (byteMask == 0) byteMask = 1;
    if (fieldMask == 0) fieldMask = byteMask;
}

static void PropRawName(void* prop, uint32_t& comparisonIndex, uint32_t& number) {
    comparisonIndex = 0;
    number = 0;
    int32_t off = g_State.FField_NameOff >= 0 ? g_State.FField_NameOff : g_State.UObject_NameOff;
    if (!prop || off < 0) return;
    FName fn = {};
    if (SafeMem_Read(reinterpret_cast<uint8_t*>(prop) + off, &fn, sizeof(fn))) {
        comparisonIndex = fn.ComparisonIndex;
        number = fn.Number;
    }
}

static std::mutex g_CompatLayoutMutex;

static int32_t g_CompatPropertiesSizeOff = -2;
static int32_t g_CompatFieldNextOff = -2;
static int32_t g_CompatEnumNamesOff = -2;

static int32_t g_CompatEnumFNameStride = 0;
static int32_t g_CompatEnumSplitLayout = -1;

static UObject* FindStructObjectByName(const std::string& name);

static int32_t ReadI32Field(const void* base, int32_t off) {
    int32_t value = 0;
    if (!base || off < 0) return 0;
    if (!SafeMem_Read(reinterpret_cast<const uint8_t*>(base) + off, &value, sizeof(value))) return 0;
    return value;
}

struct CompatStructReferences {
    UObject* Color = nullptr;
    UObject* Guid = nullptr;
    UObject* IntPoint = nullptr;
};

static const CompatStructReferences& CompatStructsOfReference() {

    static CompatStructReferences refs;
    static bool attempted = false;
    if (!attempted) {
        refs.Color = FindStructObjectByName("Color");
        refs.Guid = FindStructObjectByName("Guid");
        refs.IntPoint = FindStructObjectByName("IntPoint");
        attempted = (refs.Color != nullptr) || (refs.Guid != nullptr) || (refs.IntPoint != nullptr);
    }
    return refs;
}

static bool VerifyPropertiesSizeOffset(int32_t off, std::string& evidence) {
    const CompatStructReferences& refs = CompatStructsOfReference();
    struct Reference { UObject* Struct; const char* Display; int32_t Size; };
    const Reference references[] = {
        { refs.Color,    "FColor",    4  },
        { refs.Guid,     "FGuid",     16 },
        { refs.IntPoint, "FIntPoint", 8  },
    };

    if (off < 0) return false;

    int matched = 0;
    std::string detail;
    for (const Reference& ref : references) {
        if (!ref.Struct) continue;
        if (ReadI32Field(ref.Struct, off) != ref.Size) return false;
        ++matched;
        if (!detail.empty()) detail += ", ";
        detail += std::string(ref.Display) + "=" + std::to_string(ref.Size);
    }
    if (matched < 2) return false;
    evidence = detail;
    return true;
}

static int32_t EnsurePropertiesSizeLayout() {
    std::lock_guard<std::mutex> lock(g_CompatLayoutMutex);
    if (g_CompatPropertiesSizeOff != -2) return g_CompatPropertiesSizeOff;

    std::string evidence;
    int32_t shared = g_State.UStruct_PropertiesSizeOff;

    if (shared >= 0 && VerifyPropertiesSizeOffset(shared, evidence)) {
        g_CompatPropertiesSizeOff = shared;
        char buf[288];
        snprintf(buf, sizeof(buf),
                 "[Lua] Compat layout: UStruct::PropertiesSize=0x%X (verified %s, shared by another pipeline)",
                 shared, evidence.c_str());
        LogLine(buf);
        return shared;
    }

    for (int32_t off = 0x28; off <= 0xD8; off += 4) {
        if (!VerifyPropertiesSizeOffset(off, evidence)) continue;
        g_CompatPropertiesSizeOff = off;
        g_State.UStruct_PropertiesSizeOff = off;
        char buf[288];
        snprintf(buf, sizeof(buf),
                 "[Lua] Compat layout: UStruct::PropertiesSize=0x%X (verified %s)",
                 off, evidence.c_str());
        LogLine(buf);
        return off;
    }

    g_CompatPropertiesSizeOff = -1;
    LogLine("[Lua] Compat layout: UStruct::PropertiesSize is unavailable here; struct sizes fall back to field derivation");
    return -1;
}

struct CompatFieldChainProbe {
    int Functions = 0;
    int Properties = 0;
    int Foreign = 0;
    bool Terminated = false;
};

static bool IsPropertyFieldObject(UObject* obj) {
    return obj && ClassNameOf(obj).find("Property") != std::string::npos;
}

static CompatFieldChainProbe ProbeFieldChain(UObject* structObj, int32_t nextOff) {
    CompatFieldChainProbe probe;
    if (!structObj || nextOff < 0) return probe;

    void* child = EngineDiscovery::GetChildrenHead(g_State, reinterpret_cast<UStruct*>(structObj));
    int guard = 0;
    while (child) {
        if (++guard >= 4096) return probe;
        if (!SafeMem_IsReadable(child, sizeof(void*))) return probe;
        UObject* node = static_cast<UObject*>(child);
        if (IsFunctionObject(node)) ++probe.Functions;
        else if (IsPropertyFieldObject(node)) ++probe.Properties;
        else ++probe.Foreign;
        child = SafeMem_ReadPtr(reinterpret_cast<uint8_t*>(child) + nextOff);
    }
    probe.Terminated = true;
    return probe;
}

static int32_t VerifyFieldNextOffset(int& outFunctions, int& outClasses) {
    static const char* kProbeClasses[] = {
        "Actor", "Pawn", "PlayerController", "GameStateBase", "Object", "ActorComponent",
    };

    int32_t best = -1;
    int bestFunctions = 0;
    int bestClasses = 0;

    for (int32_t off = 0x18; off <= 0x48; off += 8) {
        int functions = 0;
        int foreign = 0;
        int classes = 0;
        bool rejected = false;
        for (const char* name : kProbeClasses) {
            UObject* cls = EngineDiscovery::FindClassCached(g_State, name);
            if (!cls) continue;
            CompatFieldChainProbe probe = ProbeFieldChain(cls, off);
            if (!probe.Terminated) { rejected = true; break; }
            functions += probe.Functions;
            foreign += probe.Foreign;
            ++classes;
            if (functions > 4000) break;
        }

        if (rejected || classes < 2 || functions < 20) continue;
        if (functions < foreign * 4) continue;
        if (functions > bestFunctions) {
            best = off;
            bestFunctions = functions;
            bestClasses = classes;
        }
    }

    outFunctions = bestFunctions;
    outClasses = bestClasses;
    return best;
}

int32_t CompatVerifiedFieldNextOffset() {
    std::lock_guard<std::mutex> lock(g_CompatLayoutMutex);
    if (g_CompatFieldNextOff != -2) return g_CompatFieldNextOff;

    int functions = 0;
    int classes = 0;
    int32_t verified = VerifyFieldNextOffset(functions, classes);
    char buf[320];
    if (verified >= 0) {
        g_CompatFieldNextOff = verified;
        snprintf(buf, sizeof(buf),
                 "[Lua] Compat layout: UField::Next=0x%X for class iteration (%d functions across %d probe classes; shared value is 0x%X)",
                 verified, functions, classes, g_State.UField_NextOff);
    } else {
        g_CompatFieldNextOff = g_State.UField_NextOff;
        snprintf(buf, sizeof(buf),
                 "[Lua] Compat layout: UField::Next could not be verified; class iteration falls back to the shared value 0x%X",
                 g_CompatFieldNextOff);
    }
    LogLine(buf);
    return g_CompatFieldNextOff;
}

static void* CompatFieldNext(void* field) {
    if (!field) return nullptr;
    int32_t off = CompatVerifiedFieldNextOffset();
    if (off < 0) return nullptr;
    return SafeMem_ReadPtr(reinterpret_cast<uint8_t*>(field) + off);
}

static int32_t FieldDerivedStructSize(void* structObj) {
    if (!structObj) return 0;

    int32_t maxEnd = 0;
    void* current = structObj;
    for (int depth = 0; current && depth < 64; ++depth) {
        void* prop = EngineDiscovery::GetChildPropsHead(g_State, static_cast<UStruct*>(current));
        if (!prop) prop = EngineDiscovery::GetChildrenHead(g_State, static_cast<UStruct*>(current));
        int guard = 0;
        while (prop && ++guard < 4096) {
            int32_t off = PropOffsetOf(prop);
            int32_t size = PropElementSizeOf(prop);
            if (size <= 0) size = 8;
            int32_t dim = PropArrayDimOf(prop);
            if (dim < 1) dim = 1;
            if (off >= 0) {
                int64_t end = static_cast<int64_t>(off) + static_cast<int64_t>(size) * dim;
                if (end < 0x100000 && end > maxEnd) maxEnd = static_cast<int32_t>(end);
            }
            prop = g_State.bUseFProperty ? EngineDiscovery::GetFFieldNext(g_State, prop)
                                         : CompatFieldNext(prop);
        }
        current = EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(current));
    }
    return maxEnd > 0 ? ((maxEnd + 7) & ~7) : 0;
}

static int32_t StructSizeOf(void* structObj) {
    if (!structObj) return 0;

    int32_t off = EnsurePropertiesSizeLayout();
    int32_t size = 0;
    if (off >= 0 && SafeMem_Read(reinterpret_cast<uint8_t*>(structObj) + off, &size, sizeof(size))) {
        if (size > 0 && size <= 0x100000) return size;
    }
    return FieldDerivedStructSize(structObj);
}

int32_t CompatStructSizeOf(void* structObj) {
    return StructSizeOf(structObj);
}

static bool IsIdentifierishEnumName(const std::string& name) {
    if (name.empty() || name.size() > 256 || name == "None" || name[0] == '<') return false;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) continue;
        if (c == '_' || c == ':') continue;
        return false;
    }
    return true;
}

static int ScoreEnumEntryName(const std::string& enumName, const std::string& entryName) {
    size_t sep = entryName.find("::");
    if (sep == std::string::npos) return 1;
    if (enumName.size() == sep && entryName.compare(0, sep, enumName) == 0) return 4;
    return 2;
}

static int ScoreEnumLayout(void* enumObj, int32_t off, bool splitLayout,
                           std::string& sampleName, int& entryCount,
                           int32_t strideOverride = -1) {
    sampleName.clear();
    entryCount = 0;
    if (!enumObj || off < 0) return 0;

    std::string enumName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(enumObj));

    int32_t nameStride = (strideOverride > 0) ? strideOverride
                       : (g_CompatEnumFNameStride > 0) ? g_CompatEnumFNameStride
                       : (g_State.FName_Size > 0) ? ((g_State.FName_Size + 7) & ~7) : 8;
    int32_t nameSize = nameStride;

    void* names = nullptr;
    void* values = nullptr;
    void* entries = nullptr;
    int32_t count = 0;

    if (splitLayout) {
        uintptr_t namesTag = 0;
        uintptr_t valuesTag = 0;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(enumObj) + off, &namesTag, sizeof(namesTag))) return 0;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(enumObj) + off + 8, &valuesTag, sizeof(valuesTag))) return 0;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(enumObj) + off + 16, &count, sizeof(count))) return 0;
        if (count <= 0 || count > 8192) return 0;
        names = reinterpret_cast<void*>(namesTag & ~static_cast<uintptr_t>(1));
        values = reinterpret_cast<void*>(valuesTag & ~static_cast<uintptr_t>(1));
        if (!SafeMem_IsCanonical(names) || !SafeMem_IsCanonical(values)) return 0;
    } else {
        uint8_t header[16] = {};
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(enumObj) + off, header, sizeof(header))) return 0;
        entries = *reinterpret_cast<void**>(header);
        count = *reinterpret_cast<int32_t*>(header + 8);
        int32_t maxCount = *reinterpret_cast<int32_t*>(header + 12);
        if (!entries || count <= 0 || count > 8192) return 0;
        if (maxCount < count || maxCount > 262144) return 0;
        if (!SafeMem_IsCanonical(entries)) return 0;
    }

    int32_t samples = count < 8 ? count : 8;
    int score = 0;
    for (int32_t i = 0; i < samples; ++i) {
        uint8_t entryBuf[32] = {};
        FName fn = {};
        int64_t value = 0;

        if (splitLayout) {
            if (!SafeMem_Read(reinterpret_cast<uint8_t*>(names) + static_cast<size_t>(i) * nameSize,
                              entryBuf, nameSize)) return 0;
            fn.ComparisonIndex = *reinterpret_cast<uint32_t*>(entryBuf);
            if (g_State.FName_NumberOff >= 0 && g_State.FName_NumberOff + 4 <= nameSize) {
                fn.Number = *reinterpret_cast<uint32_t*>(entryBuf + g_State.FName_NumberOff);
            }
            if (!SafeMem_Read(reinterpret_cast<uint8_t*>(values) + static_cast<size_t>(i) * 8,
                              &value, sizeof(value))) return 0;
        } else {
            int32_t stride = nameStride + 8;
            if (stride > static_cast<int32_t>(sizeof(entryBuf))) return 0;
            if (!SafeMem_Read(reinterpret_cast<uint8_t*>(entries) + static_cast<size_t>(i) * stride,
                              entryBuf, stride)) return 0;
            fn.ComparisonIndex = *reinterpret_cast<uint32_t*>(entryBuf);
            if (g_State.FName_NumberOff >= 0 && g_State.FName_NumberOff + 4 <= nameStride) {
                fn.Number = *reinterpret_cast<uint32_t*>(entryBuf + g_State.FName_NumberOff);
            }
            std::memcpy(&value, entryBuf + nameStride, sizeof(value));
        }

        std::string entryName = EngineDiscovery::FNameToStr(g_State, fn);
        if (!IsIdentifierishEnumName(entryName)) return 0;

        if (value < -0x10000000000LL || value > 0x10000000000LL) return 0;

        score += ScoreEnumEntryName(enumName, entryName);
        if (i == 0) sampleName = entryName;
    }

    entryCount = count;
    return score >= (count < 4 ? count : 4) ? score : 0;
}

static int32_t EnsureEnumNamesLayout(void* enumObj, bool& outSplit) {
    std::lock_guard<std::mutex> lock(g_CompatLayoutMutex);
    outSplit = false;
    if (!enumObj) return -1;

    if (g_CompatEnumNamesOff == -2) {
        std::string sample;
        int entries = 0;
        int32_t shared = g_State.UEnum_NamesOff;
        bool sharedSplit = g_State.bIsNewUE5Enum != 0;
        char buf[320];

        if (shared >= 0 && ScoreEnumLayout(enumObj, shared, sharedSplit, sample, entries) > 0) {
            g_CompatEnumNamesOff = shared;
            g_CompatEnumSplitLayout = sharedSplit ? 1 : 0;

            g_CompatEnumFNameStride =
                (g_State.FName_Size > 0) ? ((g_State.FName_Size + 7) & ~7) : 8;
            snprintf(buf, sizeof(buf),
                     "[Lua] Compat layout: UEnum::Names=0x%X (%s layout, verified on %s, %d entries, shared by another pipeline)",
                     shared, sharedSplit ? "split" : "pair", sample.c_str(), entries);
            LogLine(buf);
        } else {

            static const int32_t kPreferred[] = { 0x40, 0x38, 0x48, 0x30, 0x50, 0x58, 0x28, 0x60, 0x68 };
            int bestScore = 0;
            int32_t bestOff = -1;
            bool bestSplit = false;
            std::string bestSample;
            int bestEntries = 0;

            const int32_t sharedStride =
                (g_State.FName_Size > 0) ? ((g_State.FName_Size + 7) & ~7) : 8;
            int32_t bestStride = sharedStride;
            auto tryOffset = [&](int32_t off, bool split, int32_t strideOverride = -1) {
                std::string candidateSample;
                int candidateEntries = 0;
                int score = ScoreEnumLayout(enumObj, off, split, candidateSample, candidateEntries, strideOverride);
                if (score <= 0 || score <= bestScore) return;
                bestStride = (strideOverride > 0) ? strideOverride : sharedStride;
                bestScore = score;
                bestOff = off;
                bestSplit = split;
                bestSample = candidateSample;
                bestEntries = candidateEntries;
            };

            for (int stridePass = 0; stridePass < 2 && bestScore <= 0; ++stridePass) {
                const int32_t strideOverride = stridePass ? 8 : -1;
                for (int pass = 0; pass < 2 && bestScore <= 0; ++pass) {
                    bool split = (pass == 1);
                    for (int32_t off : kPreferred) tryOffset(off, split, strideOverride);
                    for (int32_t off = 0x30; off <= 0xD8 && bestScore <= 0; off += 8) tryOffset(off, split, strideOverride);
                }
            }

            if (bestScore > 0) {
                g_CompatEnumNamesOff = bestOff;
                g_CompatEnumSplitLayout = bestSplit ? 1 : 0;

                g_CompatEnumFNameStride = bestStride;

                if (bestSplit == (g_State.bIsNewUE5Enum != 0)) g_State.UEnum_NamesOff = bestOff;
                snprintf(buf, sizeof(buf),
                         "[Lua] Compat layout: UEnum::Names=0x%X (%s layout, verified on %s, %d entries)",
                         bestOff, bestSplit ? "split" : "pair", bestSample.c_str(), bestEntries);
                LogLine(buf);
            } else {
                g_CompatEnumNamesOff = -1;
                LogLine("[Lua] Compat layout: UEnum::Names could not be verified on this engine layout; enum accessors answer nil");

                {
                    const uint8_t *p = reinterpret_cast<const uint8_t *>(enumObj);

                    int32_t nextOff = (g_CompatFieldNextOff >= 0) ? g_CompatFieldNextOff
                                                                  : g_State.UField_NextOff;
                    if (nextOff < 0) nextOff = 0x20;
                    const int32_t kEnd = 0xE0;
                    for (int32_t off = nextOff; off + 0x18 <= kEnd; off += 8) {
                        uintptr_t namesTag = 0, valuesTag = 0;
                        int32_t cnt = 0;
                        if (!SafeMem_Read(p + off, &namesTag, 8)) continue;
                        if (!SafeMem_Read(p + off + 8, &valuesTag, 8)) continue;
                        if (!SafeMem_Read(p + off + 16, &cnt, 4)) continue;

                        if (cnt > 0 && cnt < 8192 &&
                            SafeMem_IsCanonical(reinterpret_cast<void *>(namesTag & ~static_cast<uintptr_t>(1)))) {
                            uintptr_t namesPtr = namesTag & ~static_cast<uintptr_t>(1);
                            uint8_t nm[16] = {};
                            const char *content = "unreadable";
                            std::string first;
                            if (SafeMem_Read(reinterpret_cast<const void *>(namesPtr), nm, sizeof(nm))) {
                                FName fn = {};
                                fn.ComparisonIndex = *reinterpret_cast<uint32_t *>(nm);
                                if (g_State.FName_NumberOff >= 0)
                                    fn.Number = *reinterpret_cast<uint32_t *>(nm + g_State.FName_NumberOff);
                                first = EngineDiscovery::FNameToStr(g_State, fn);
                                content = first.c_str();
                            }
                            snprintf(buf, sizeof(buf),
                                     "[Lua] Compat layout: UEnum probe @0x%X: names=%p values=%p count=%d first='%s'",
                                     off, reinterpret_cast<const void *>(namesPtr),
                                     reinterpret_cast<const void *>(valuesTag), cnt, content);
                            LogLine(buf);
                        }
                    }

                    snprintf(buf, sizeof(buf), "[Lua] Compat layout: UEnum object class=%s outer=%p",
                             EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject *>(enumObj)).c_str(),
                             SafeMem_ReadPtr(p + g_State.UObject_OuterOff));
                    LogLine(buf);
                }
            }
        }
    }

    if (g_CompatEnumNamesOff < 0) return -1;
    outSplit = (g_CompatEnumSplitLayout == 1);
    return g_CompatEnumNamesOff;
}

FoundPropertyEx DescribePropertyEx(void* prop) {
    FoundPropertyEx fp;
    if (!prop) return fp;

    fp.Prop = prop;
    fp.TypeName = PropTypeOf(prop);
    fp.Offset = PropOffsetOf(prop);
    int32_t elem = PropElementSizeOf(prop);
    fp.ElementSize = elem > 0 ? elem : 8;
    fp.Flags = PropFlagsOf(prop);

    if (g_State.FArrayProperty_InnerOff >= 0 && fp.TypeName.find("ArrayProperty") != std::string::npos) {
        fp.InnerProp = ReadPtrMember(prop, g_State.FArrayProperty_InnerOff);
    }
    if (g_State.FMapProperty_KeyPropOff >= 0 && fp.TypeName.find("MapProperty") != std::string::npos) {
        fp.KeyProp = ReadPtrMember(prop, g_State.FMapProperty_KeyPropOff);
        fp.ValueProp = ReadPtrMember(prop, g_State.FMapProperty_ValuePropOff);
    }
    if (g_State.FSetProperty_ElementPropOff >= 0 && fp.TypeName.find("SetProperty") != std::string::npos) {
        fp.InnerProp = ReadPtrMember(prop, g_State.FSetProperty_ElementPropOff);
    }
    if (g_State.FEnumProperty_UnderlyingOff >= 0 && fp.TypeName.find("EnumProperty") != std::string::npos) {
        fp.InnerProp = ReadPtrMember(prop, g_State.FEnumProperty_UnderlyingOff);
    }
    if (fp.TypeName.find("BoolProperty") != std::string::npos) {
        uint8_t fieldSize = 0;
        PropBoolBits(prop, fp.ByteOffset, fp.ByteMask, fp.FieldMask, fieldSize);
    }
    fp.Found = true;
    return fp;
}

static const char* FPROPERTY_MT = "Avalonium.FProperty";

struct LuaFPropertyData {
    void* Prop = nullptr;
    void* Owner = nullptr;
    void* Instance = nullptr;
};

void CompatPushPropertyWrapper(lua_State* L, void* prop, void* owner) {
    auto* data = static_cast<LuaFPropertyData*>(lua_newuserdata(L, sizeof(LuaFPropertyData)));
    data->Prop = prop;
    data->Owner = owner;
    data->Instance = nullptr;
    luaL_setmetatable(L, FPROPERTY_MT);
}

static LuaFPropertyData* CheckProperty(lua_State* L) {
    auto* p = static_cast<LuaFPropertyData*>(luaL_checkudata(L, 1, FPROPERTY_MT));
    if (!p || !p->Prop || !SafeMem_IsReadable(p->Prop, 8)) {
        luaL_error(L, "property wrapper is stale or invalid");
        return nullptr;
    }
    return p;
}

static int FProp_GetName(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    lua_pushstring(L, PropNameOf(p->Prop).c_str());
    return 1;
}

static int FProp_GetFName(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    uint32_t cmp = 0, num = 0;
    PropRawName(p->Prop, cmp, num);
    if (cmp == 0) {
        int32_t idx = EngineDiscovery::ResolveFNameIndex(g_State, PropNameOf(p->Prop));
        cmp = static_cast<uint32_t>(idx >= 0 ? idx : 0);
    }
    PushFName(L, cmp, num);
    return 1;
}

static int FProp_GetFullName(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    std::string ownerName;
    if (p->Owner && SafeMem_IsReadable(p->Owner, 8)) {
        ownerName = EngineDiscovery::BuildFullName(g_State, static_cast<UObject*>(p->Owner));
    }
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s (%s)", PropNameOf(p->Prop).c_str(), PropTypeOf(p->Prop).c_str());
    std::string out = buf;
    if (!ownerName.empty()) {
        out += " (";
        out += ownerName;
        out += ")";
    }
    lua_pushstring(L, out.c_str());
    return 1;
}

static int FProp_GetClass(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;

    if (g_State.bUseFProperty && g_State.FField_ClassOff >= 0) {
        void* ffc = ReadPtrMember(p->Prop, g_State.FField_ClassOff);
        if (ffc && g_State.FFieldClass_NameOff >= 0) {
            FName fn = {};
            if (SafeMem_Read(reinterpret_cast<uint8_t*>(ffc) + g_State.FFieldClass_NameOff, &fn, sizeof(fn))) {
                lua_pushstring(L, EngineDiscovery::GetCachedFName(g_State, static_cast<int32_t>(fn.ComparisonIndex)).c_str());
                return 1;
            }
        }
    }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(p->Prop));
    if (cls) { PushUObject(L, static_cast<UObject*>(cls)); return 1; }
    lua_pushnil(L);
    return 1;
}

static int FProp_GetStruct(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    void* scriptStruct = ReadPtrMember(p->Prop, g_State.FStructProperty_StructOff);
    if (scriptStruct) { PushUObject(L, static_cast<UObject*>(scriptStruct)); return 1; }
    if (p->Owner) { PushUObject(L, static_cast<UObject*>(p->Owner)); return 1; }
    lua_pushnil(L);
    return 1;
}

static int FProp_GetInner(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    std::string type = PropTypeOf(p->Prop);
    void* inner = nullptr;
    if (type.find("ArrayProperty") != std::string::npos) inner = ReadPtrMember(p->Prop, g_State.FArrayProperty_InnerOff);
    else if (type.find("SetProperty") != std::string::npos) inner = ReadPtrMember(p->Prop, g_State.FSetProperty_ElementPropOff);
    else if (type.find("EnumProperty") != std::string::npos) inner = ReadPtrMember(p->Prop, g_State.FEnumProperty_UnderlyingOff);
    if (inner) { CompatPushPropertyWrapper(L, inner, p->Owner); return 1; }
    lua_pushnil(L);
    return 1;
}

static int FProp_GetEnum(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    void* enumObj = ReadPtrMember(p->Prop, g_State.FEnumProperty_EnumOff);
    if (!enumObj) enumObj = ReadPtrMember(p->Prop, g_State.FByteProperty_EnumOff);
    if (enumObj) { PushUObject(L, static_cast<UObject*>(enumObj)); return 1; }
    lua_pushnil(L);
    return 1;
}

static int FProp_GetPropertyClass(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    void* cls = ReadPtrMember(p->Prop, g_State.FObjectProperty_ClassOff);
    if (!cls) cls = ReadPtrMember(p->Prop, g_State.FClassProperty_MetaClassOff);
    if (cls) { PushUObject(L, static_cast<UObject*>(cls)); return 1; }
    lua_pushnil(L);
    return 1;
}

static int FProp_GetOffset(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    lua_pushinteger(L, PropOffsetOf(p->Prop));
    return 1;
}

static int FProp_GetArrayDim(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    lua_pushinteger(L, PropArrayDimOf(p->Prop));
    return 1;
}

static int FProp_GetSize(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    int32_t size = PropElementSizeOf(p->Prop);
    if (size <= 0) {
        void* scriptStruct = ReadPtrMember(p->Prop, g_State.FStructProperty_StructOff);
        if (scriptStruct) size = StructSizeOf(scriptStruct);
    }
    lua_pushinteger(L, size > 0 ? size : 0);
    return 1;
}

static int FProp_GetFieldSize(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    uint8_t byteOffset = 0, byteMask = 1, fieldMask = 1, fieldSize = 0;
    PropBoolBits(p->Prop, byteOffset, byteMask, fieldMask, fieldSize);
    lua_pushinteger(L, fieldSize);
    return 1;
}

static int FProp_GetByteOffset(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    if (PropTypeOf(p->Prop).find("BoolProperty") == std::string::npos) { lua_pushinteger(L, 0); return 1; }
    uint8_t byteOffset = 0, byteMask = 1, fieldMask = 1, fieldSize = 0;
    PropBoolBits(p->Prop, byteOffset, byteMask, fieldMask, fieldSize);
    lua_pushinteger(L, byteOffset);
    return 1;
}

static int FProp_GetByteMask(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    if (PropTypeOf(p->Prop).find("BoolProperty") == std::string::npos) { lua_pushinteger(L, 0); return 1; }
    uint8_t byteOffset = 0, byteMask = 1, fieldMask = 1, fieldSize = 0;
    PropBoolBits(p->Prop, byteOffset, byteMask, fieldMask, fieldSize);
    lua_pushinteger(L, byteMask);
    return 1;
}

static int FProp_GetFieldMask(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    if (PropTypeOf(p->Prop).find("BoolProperty") == std::string::npos) { lua_pushinteger(L, 0); return 1; }
    uint8_t byteOffset = 0, byteMask = 1, fieldMask = 1, fieldSize = 0;
    PropBoolBits(p->Prop, byteOffset, byteMask, fieldMask, fieldSize);
    lua_pushinteger(L, fieldMask);
    return 1;
}

static int FProp_ContainerPtrToValuePtr(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    lua_Integer raw = lua_isuserdata(L, 2) ? reinterpret_cast<lua_Integer>(GetUObject(L, 2))
                                           : static_cast<lua_Integer>(luaL_checkinteger(L, 2));
    int32_t offset = PropOffsetOf(p->Prop);
    int32_t elemSize = PropElementSizeOf(p->Prop);
    if (elemSize <= 0) elemSize = 1;
    int32_t index = lua_isnumber(L, 3) ? static_cast<int32_t>(lua_tointeger(L, 3)) : 0;
    if (raw == 0 || offset < 0) { lua_pushnil(L); return 1; }
    uintptr_t addr = static_cast<uintptr_t>(raw) + static_cast<uintptr_t>(offset) +
                     static_cast<uintptr_t>(index) * static_cast<uintptr_t>(elemSize);

    if (!SafeMem_IsReadable(reinterpret_cast<void*>(addr), 1)) { lua_pushnil(L); return 1; }
    lua_pushlightuserdata(L, reinterpret_cast<void*>(addr));
    return 1;
}

static void* ResolveValueBase(lua_State* L, LuaFPropertyData* p, int argIdx) {
    if (lua_isuserdata(L, argIdx) || lua_islightuserdata(L, argIdx) || lua_isinteger(L, argIdx)) {
        void* obj = GetUObject(L, argIdx);
        if (obj) return obj;
    }
    return p->Instance;
}

static int FProp_GetValue(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    void* base = ResolveValueBase(L, p, 2);
    if (!base) { lua_pushnil(L); return 1; }
    FoundPropertyEx fp = DescribePropertyEx(p->Prop);
    if (fp.Offset < 0) { lua_pushnil(L); return 1; }
    return PushPropertyValueEx(L, reinterpret_cast<uint8_t*>(base), fp);
}

static int FProp_SetValue(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    void* base = p->Instance;
    int valIdx = 2;
    if (!base) {

        base = lua_isuserdata(L, 2) ? GetUObject(L, 2) : nullptr;
        valIdx = 3;
    } else if (lua_gettop(L) >= 3 && lua_isuserdata(L, 2)) {
        void* explicitBase = GetUObject(L, 2);
        if (explicitBase) { base = explicitBase; valIdx = 3; }
    }
    if (!base) { lua_pushboolean(L, 0); return 1; }
    FoundPropertyEx fp = DescribePropertyEx(p->Prop);
    if (fp.Offset < 0) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, SetPropertyValueEx(L, reinterpret_cast<uint8_t*>(base), fp, valIdx) ? 1 : 0);
    return 1;
}

static int FProp_ImportText(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    const char* text = luaL_optstring(L, 2, "");
    void* base = ResolveValueBase(L, p, 3);
    if (!base || !text) { lua_pushboolean(L, 0); return 1; }

    std::string s = text;
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();

    FoundPropertyEx fp = DescribePropertyEx(p->Prop);
    if (fp.Offset < 0) { lua_pushboolean(L, 0); return 1; }

    bool ok = false;
    if (_stricmp(s.c_str(), "true") == 0 || _stricmp(s.c_str(), "false") == 0) {
        lua_pushboolean(L, _stricmp(s.c_str(), "true") == 0 ? 1 : 0);
        ok = SetPropertyValueEx(L, reinterpret_cast<uint8_t*>(base), fp, lua_gettop(L));
        lua_pop(L, 1);
    } else if (!s.empty() && (isdigit(static_cast<unsigned char>(s[0])) || s[0] == '-' || s[0] == '+' || s[0] == '.')) {
        bool isFloat = s.find('.') != std::string::npos || s.find('e') != std::string::npos || s.find('E') != std::string::npos;
        if (isFloat) {
            lua_pushnumber(L, atof(s.c_str()));
            ok = SetPropertyValueEx(L, reinterpret_cast<uint8_t*>(base), fp, lua_gettop(L));
        } else {
            lua_pushinteger(L, static_cast<lua_Integer>(_strtoi64(s.c_str(), nullptr, 0)));
            ok = SetPropertyValueEx(L, reinterpret_cast<uint8_t*>(base), fp, lua_gettop(L));
        }
        lua_pop(L, 1);
    } else {
        std::string quoted = s;
        if (quoted.size() >= 2 && quoted.front() == '"' && quoted.back() == '"') {
            quoted = quoted.substr(1, quoted.size() - 2);
        }
        lua_pushstring(L, quoted.c_str());
        ok = SetPropertyValueEx(L, reinterpret_cast<uint8_t*>(base), fp, lua_gettop(L));
        lua_pop(L, 1);
    }

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static bool ResolvePropertyTypeName(lua_State* L, int idx, std::string& out) {
    out.clear();
    if (lua_isstring(L, idx)) { out = lua_tostring(L, idx); return !out.empty(); }
    if (lua_istable(L, idx)) {
        lua_getfield(L, idx, "Name");
        if (lua_isstring(L, -1)) out = lua_tostring(L, -1);
        lua_pop(L, 1);
        return !out.empty();
    }
    if (lua_isuserdata(L, idx)) {
        int top = lua_gettop(L);
        lua_getmetatable(L, idx);
        if (lua_istable(L, -1)) {
            lua_pushstring(L, "__index");
            lua_rawget(L, -2);
            if (lua_istable(L, -1)) {
                lua_pushstring(L, "ToString");
                lua_rawget(L, -2);
                if (lua_isfunction(L, -1)) {
                    lua_pushvalue(L, idx);
                    if (lua_pcall(L, 1, 1, 0) == LUA_OK && lua_isstring(L, -1)) out = lua_tostring(L, -1);
                    lua_pop(L, 1);
                }
            }
        }
        lua_settop(L, top);
        return !out.empty();
    }
    return false;
}

static int FProp_IsA(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p) return 0;
    std::string wanted;
    ResolvePropertyTypeName(L, 2, wanted);
    std::string type = PropTypeOf(p->Prop);
    bool match = !wanted.empty() && _stricmp(type.c_str(), wanted.c_str()) == 0;
    if (!match && !wanted.empty()) {

        std::string suffix = "Property";
        if (type.size() > suffix.size() && type.compare(type.size() - suffix.size(), suffix.size(), suffix) == 0) {
            match = _stricmp(type.substr(0, type.size() - suffix.size()).c_str(), wanted.c_str()) == 0;
        }
    }
    lua_pushboolean(L, match ? 1 : 0);
    return 1;
}

static int FProp_GetInterfaceClass(lua_State* L) {
    LuaFPropertyData* p = CheckProperty(L);
    if (!p || !p->Prop) { lua_pushnil(L); return 1; }
    if (PropTypeOf(p->Prop).find("InterfaceProperty") == std::string::npos) { lua_pushnil(L); return 1; }

    const uint8_t* base = reinterpret_cast<const uint8_t*>(p->Prop);
    void* ownClass = nullptr;
    void* ownOuter = nullptr;
    if (g_State.UObject_ClassOff >= 0) SafeMem_Read(const_cast<uint8_t*>(base) + g_State.UObject_ClassOff, &ownClass, sizeof(ownClass));
    if (g_State.UObject_OuterOff >= 0) SafeMem_Read(const_cast<uint8_t*>(base) + g_State.UObject_OuterOff, &ownOuter, sizeof(ownOuter));

    for (int32_t off = 0x20; off + sizeof(void*) <= 0x80; off += sizeof(void*)) {
        void* candidate = nullptr;
        if (!SafeMem_Read(const_cast<uint8_t*>(base) + off, &candidate, sizeof(candidate))) continue;
        if (!candidate || candidate == ownClass || candidate == ownOuter) continue;
        if (!SafeMem_IsReadable(candidate, 32)) continue;
        if (!ConsoleEnabler::IsClassObject(g_State, static_cast<UObject*>(candidate))) continue;
        PushUObject(L, static_cast<UObject*>(candidate));
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int FProp_IsValid(lua_State* L) {
    auto* p = static_cast<LuaFPropertyData*>(luaL_testudata(L, 1, FPROPERTY_MT));
    lua_pushboolean(L, (p && p->Prop && SafeMem_IsReadable(p->Prop, 8)) ? 1 : 0);
    return 1;
}

static int FProp_GetOwner(lua_State* L) {
    auto* p = static_cast<LuaFPropertyData*>(luaL_testudata(L, 1, FPROPERTY_MT));
    if (!p || !p->Owner || !SafeMem_IsReadable(p->Owner, 8)) { lua_pushnil(L); return 1; }
    PushUObject(L, static_cast<UObject*>(p->Owner));
    return 1;
}

static int FProp_TypeName(lua_State* L) {
    auto* p = static_cast<LuaFPropertyData*>(luaL_testudata(L, 1, FPROPERTY_MT));
    if (!p || !p->Prop) { lua_pushstring(L, "Property"); return 1; }
    lua_pushstring(L, PropTypeOf(p->Prop).c_str());
    return 1;
}

static int FProp_ToString(lua_State* L) {
    auto* p = static_cast<LuaFPropertyData*>(luaL_testudata(L, 1, FPROPERTY_MT));
    if (!p || !p->Prop) { lua_pushstring(L, "FProperty(null)"); return 1; }
    char buf[512];
    snprintf(buf, sizeof(buf), "FProperty %s <%s> +0x%X (0x%p)",
             PropNameOf(p->Prop).c_str(), PropTypeOf(p->Prop).c_str(), PropOffsetOf(p->Prop), p->Prop);
    lua_pushstring(L, buf);
    return 1;
}

static int FProp_DataKey(lua_State* L, const char* key) {
    auto* p = static_cast<LuaFPropertyData*>(luaL_testudata(L, 1, FPROPERTY_MT));
    if (!p || !p->Prop) return 0;
    const uint64_t flags = PropFlagsOf(p->Prop);

    if (_stricmp(key, "Type") == 0 || _stricmp(key, "TypeName") == 0) {
        lua_pushstring(L, PropTypeOf(p->Prop).c_str()); return 1;
    }
    if (_stricmp(key, "Name") == 0) {
        lua_pushstring(L, PropNameOf(p->Prop).c_str()); return 1;
    }
    if (_stricmp(key, "Offset_Internal") == 0 || _stricmp(key, "Offset") == 0) {
        lua_pushinteger(L, PropOffsetOf(p->Prop)); return 1;
    }
    if (_stricmp(key, "ElementSize") == 0 || _stricmp(key, "Size") == 0) {
        int32_t size = PropElementSizeOf(p->Prop);
        if (size <= 0) {
            void* ss = ReadPtrMember(p->Prop, g_State.FStructProperty_StructOff);
            if (ss) size = StructSizeOf(ss);
        }
        lua_pushinteger(L, size > 0 ? size : 0); return 1;
    }
    if (_stricmp(key, "ArrayDim") == 0) {
        lua_pushinteger(L, PropArrayDimOf(p->Prop)); return 1;
    }
    if (_stricmp(key, "PropertyFlags") == 0) {
        lua_pushinteger(L, static_cast<lua_Integer>(flags)); return 1;
    }
    if (_stricmp(key, "IsOutParam") == 0) {
        lua_pushboolean(L, (flags & 0x0000000000000100) != 0 ? 1 : 0); return 1;
    }
    if (_stricmp(key, "IsArrayLike") == 0) {
        lua_pushboolean(L, ReadPtrMember(p->Prop, g_State.FArrayProperty_InnerOff) != nullptr ? 1 : 0); return 1;
    }
    if (_stricmp(key, "HasValue") == 0) {
        lua_pushboolean(L, (flags & 0x0000000000000100) == 0 ? 1 : 0); return 1;
    }
    if (_stricmp(key, "PropertyAddress") == 0) {
        lua_pushlightuserdata(L, p->Prop); return 1;
    }
    if (_stricmp(key, "ByteOffset") == 0 || _stricmp(key, "ByteMask") == 0 ||
        _stricmp(key, "FieldMask") == 0 || _stricmp(key, "FieldSize") == 0) {
        uint8_t byteOffset = 0, byteMask = 1, fieldMask = 1, fieldSize = 0;
        PropBoolBits(p->Prop, byteOffset, byteMask, fieldMask, fieldSize);
        if (_stricmp(key, "ByteOffset") == 0) lua_pushinteger(L, byteOffset);
        else if (_stricmp(key, "ByteMask") == 0) lua_pushinteger(L, byteMask);
        else if (_stricmp(key, "FieldMask") == 0) lua_pushinteger(L, fieldMask);
        else lua_pushinteger(L, fieldSize);
        return 1;
    }
    return 0;
}

static int FProperty_Index(lua_State* L) {
    auto* p = static_cast<LuaFPropertyData*>(luaL_checkudata(L, 1, FPROPERTY_MT));
    const char* key = lua_tostring(L, 2);
    if (!p || !p->Prop || !key) { lua_pushnil(L); return 1; }

    if (_stricmp(key, "GetFName") == 0)                     { lua_pushcfunction(L, FProp_GetFName); return 1; }
    if (_stricmp(key, "GetName") == 0)                      { lua_pushcfunction(L, FProp_GetName); return 1; }
    if (_stricmp(key, "GetFullName") == 0)                  { lua_pushcfunction(L, FProp_GetFullName); return 1; }
    if (_stricmp(key, "GetClass") == 0)                     { lua_pushcfunction(L, FProp_GetClass); return 1; }
    if (_stricmp(key, "GetStruct") == 0)                    { lua_pushcfunction(L, FProp_GetStruct); return 1; }
    if (_stricmp(key, "GetInner") == 0)                     { lua_pushcfunction(L, FProp_GetInner); return 1; }
    if (_stricmp(key, "GetEnum") == 0)                      { lua_pushcfunction(L, FProp_GetEnum); return 1; }
    if (_stricmp(key, "GetPropertyClass") == 0)             { lua_pushcfunction(L, FProp_GetPropertyClass); return 1; }
    if (_stricmp(key, "GetInterfaceClass") == 0)            { lua_pushcfunction(L, FProp_GetInterfaceClass); return 1; }
    if (_stricmp(key, "GetOffset_Internal") == 0 || _stricmp(key, "GetOffset") == 0) { lua_pushcfunction(L, FProp_GetOffset); return 1; }
    if (_stricmp(key, "GetArrayDim") == 0)                  { lua_pushcfunction(L, FProp_GetArrayDim); return 1; }
    if (_stricmp(key, "GetSize") == 0 || _stricmp(key, "GetElementSize") == 0) { lua_pushcfunction(L, FProp_GetSize); return 1; }
    if (_stricmp(key, "GetFieldSize") == 0)                 { lua_pushcfunction(L, FProp_GetFieldSize); return 1; }
    if (_stricmp(key, "GetByteOffset") == 0)                { lua_pushcfunction(L, FProp_GetByteOffset); return 1; }
    if (_stricmp(key, "GetByteMask") == 0)                  { lua_pushcfunction(L, FProp_GetByteMask); return 1; }
    if (_stricmp(key, "GetFieldMask") == 0)                 { lua_pushcfunction(L, FProp_GetFieldMask); return 1; }
    if (_stricmp(key, "ContainerPtrToValuePtr") == 0)       { lua_pushcfunction(L, FProp_ContainerPtrToValuePtr); return 1; }
    if (_stricmp(key, "GetValue") == 0 || _stricmp(key, "get") == 0) { lua_pushcfunction(L, FProp_GetValue); return 1; }
    if (_stricmp(key, "SetValue") == 0 || _stricmp(key, "set") == 0) { lua_pushcfunction(L, FProp_SetValue); return 1; }
    if (_stricmp(key, "ImportText") == 0)                   { lua_pushcfunction(L, FProp_ImportText); return 1; }
    if (_stricmp(key, "IsA") == 0)                         { lua_pushcfunction(L, FProp_IsA); return 1; }
    if (_stricmp(key, "IsValid") == 0)                     { lua_pushcfunction(L, FProp_IsValid); return 1; }
    if (_stricmp(key, "GetOwner") == 0)                    { lua_pushcfunction(L, FProp_GetOwner); return 1; }
    if (_stricmp(key, "type") == 0)                        { lua_pushcfunction(L, FProp_TypeName); return 1; }

    {
        int handled = FProp_DataKey(L, key);
        if (handled) return handled;
    }

    if (p->Instance) {
        void* scriptStruct = ReadPtrMember(p->Prop, g_State.FStructProperty_StructOff);
        if (scriptStruct) {
            FoundPropertyEx field = FindPropertyOnClassEx(reinterpret_cast<UClass*>(scriptStruct), key);
            if (field.Found && field.Offset >= 0) {
                return PushPropertyValueEx(L, reinterpret_cast<uint8_t*>(p->Instance), field);
            }
        }
    }

    lua_pushnil(L);
    return 1;
}

static int FProperty_NewIndex(lua_State* L) {
    auto* p = static_cast<LuaFPropertyData*>(luaL_checkudata(L, 1, FPROPERTY_MT));
    if (!p || !p->Prop) return 0;
    const char* key = lua_tostring(L, 2);
    if (!key) return 0;

    if (p->Instance) {
        void* scriptStruct = ReadPtrMember(p->Prop, g_State.FStructProperty_StructOff);
        if (scriptStruct) {
            FoundPropertyEx field = FindPropertyOnClassEx(reinterpret_cast<UClass*>(scriptStruct), key);
            if (field.Found && field.Offset >= 0) {
                SetPropertyValueEx(L, reinterpret_cast<uint8_t*>(p->Instance), field, 3);
                return 0;
            }
        }
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "[Lua] property wrapper field '%s' is not writable (wrapper is property metadata, not storage)", key);
    LogLine(buf);
    return 0;
}

static void* CompatEngineAllocate(size_t bytes) {
    if (bytes == 0) return nullptr;

    void* mallocInstance = g_State.GMallocLocation
        ? SafeMem_ReadPtr(g_State.GMallocLocation)
        : g_State.GMalloc;
    if (!mallocInstance) mallocInstance = g_State.GMalloc;
    if (mallocInstance) {
        void* buf = SafeMem_CallMallocVftable(mallocInstance, bytes, 0,
                                              g_State.GMallocMallocSlot);
        if (buf) {
            memset(buf, 0, bytes);
            return buf;
        }
    }
    return nullptr;
}

void* CompatFindScriptStruct(const char* name) {
    if (!name || !*name) return nullptr;
    UObject* byName = FindStructObjectByName(name);
    if (byName) return byName;

    return EngineDiscovery::FindClassCached(g_State, name);
}

static UObject* FindStructObjectByName(const std::string& name) {
    if (name.empty()) return nullptr;

    if (g_State.bIndexBuilt) {
        std::vector<UObject*> matches = EngineDiscovery::FindObjectsByNameFast(g_State, name);
        for (UObject* obj : matches) {
            if (!obj) continue;
            if (ClassNameOf(obj).find("Struct") != std::string::npos) return obj;
        }

    }

    int count = EngineDiscovery::GetObjectCount(g_State);
    for (int i = 0; i < count; ++i) {
        UObject* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
        if (!obj) continue;
        std::string clsName = ClassNameOf(obj);
        if (clsName.find("Struct") == std::string::npos) continue;
        if (_stricmp(EngineDiscovery::GetObjectNameStr(g_State, obj).c_str(), name.c_str()) == 0) return obj;
    }
    return nullptr;
}

struct EnumEntryEx {
    std::string Name;
    int64_t Value = 0;
    uint32_t CmpIndex = 0;
    uint32_t Number = 0;
};

static bool ReadEnumEntriesSafe(void* enumObj, std::vector<EnumEntryEx>& out) {
    out.clear();
    if (!enumObj) return false;

    bool splitLayout = false;
    int32_t namesOff = EnsureEnumNamesLayout(enumObj, splitLayout);
    if (namesOff < 0) return false;

    auto pushEntry = [&out](const std::string& raw, int64_t value, const FName& handle) {
        if (raw.empty() || raw == "None") return;
        std::string name = raw;
        size_t colon = name.find("::");
        if (colon != std::string::npos) name = name.substr(colon + 2);
        if (name.empty()) return;
        out.push_back({ name, value, handle.ComparisonIndex, handle.Number });
    };

    if (splitLayout) {
        uintptr_t enumAddr = reinterpret_cast<uintptr_t>(enumObj);
        uintptr_t namesTag = 0, valuesTag = 0;
        int32_t num = 0;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(enumAddr) + namesOff, &namesTag, 8)) return false;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(enumAddr) + namesOff + 8, &valuesTag, 8)) return false;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(enumAddr) + namesOff + 16, &num, 4)) return false;
        if (num < 0 || num > 8192) return false;

        uintptr_t namesPtr = namesTag & ~static_cast<uintptr_t>(1);
        uintptr_t valuesPtr = valuesTag & ~static_cast<uintptr_t>(1);

        int32_t nameSize = (g_CompatEnumFNameStride > 0) ? g_CompatEnumFNameStride
                                                         : ((g_State.FName_Size > 0) ? g_State.FName_Size : 8);

        for (int32_t i = 0; i < num; ++i) {
            uint8_t nameBuf[16] = {};
            if (!SafeMem_Read(reinterpret_cast<uint8_t*>(namesPtr) + i * nameSize, nameBuf, nameSize)) break;
            FName fn = {};
            fn.ComparisonIndex = *reinterpret_cast<uint32_t*>(nameBuf);
            if (g_State.FName_NumberOff >= 0) {
                fn.Number = *reinterpret_cast<uint32_t*>(nameBuf + g_State.FName_NumberOff);
            }
            int64_t value = 0;
            if (!SafeMem_Read(reinterpret_cast<uint8_t*>(valuesPtr) + i * sizeof(int64_t), &value, sizeof(int64_t))) break;
            pushEntry(EngineDiscovery::FNameToStr(g_State, fn), value, fn);
        }
        return !out.empty();
    }

    uint8_t tarrayBuf[16] = {};
    if (!SafeMem_Read(reinterpret_cast<uint8_t*>(enumObj) + namesOff, tarrayBuf, 16)) return false;
    void* data = *reinterpret_cast<void**>(tarrayBuf);
    int32_t num = *reinterpret_cast<int32_t*>(tarrayBuf + 8);
    if (num < 0 || num > 8192) return false;
    if (num > 0 && !data) return false;

    int32_t nameStride = (g_CompatEnumFNameStride > 0) ? g_CompatEnumFNameStride
                       : ((g_State.FName_Size > 0) ? ((g_State.FName_Size + 7) & ~7) : 8);
    int32_t entryStride = nameStride + 8;

    for (int32_t i = 0; i < num; ++i) {
        uint8_t entryBuf[32] = {};
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(data) + i * entryStride, entryBuf, entryStride)) break;
        FName fn = {};
        fn.ComparisonIndex = *reinterpret_cast<uint32_t*>(entryBuf);
        if (g_State.FName_NumberOff >= 0) {
            fn.Number = *reinterpret_cast<uint32_t*>(entryBuf + g_State.FName_NumberOff);
        }
        int64_t value = *reinterpret_cast<int64_t*>(entryBuf + nameStride);
        pushEntry(EngineDiscovery::FNameToStr(g_State, fn), value, fn);
    }
    return !out.empty();
}

int Compat_UObj_EnumGetNameByValue(lua_State* L) {
    UObject* obj = SelfObject(L);

    if (!IsEnumObject(obj)) { lua_pushnil(L); return 1; }
    lua_Integer wanted = static_cast<lua_Integer>(luaL_optinteger(L, 2, 0));

    std::vector<EnumEntryEx> entries;
    if (!ReadEnumEntriesSafe(obj, entries)) { lua_pushnil(L); return 1; }
    for (const EnumEntryEx& e : entries) {
        if (e.Value == static_cast<int64_t>(wanted)) { PushFName(L, e.CmpIndex, e.Number); return 1; }
    }
    PushFName(L, 0);
    return 1;
}

int Compat_UObj_EnumForEachName(lua_State* L) {
    UObject* obj = SelfObject(L);
    if (!IsEnumObject(obj) || !lua_isfunction(L, 2)) return 0;

    std::vector<EnumEntryEx> entries;
    if (!ReadEnumEntriesSafe(obj, entries)) { lua_pushinteger(L, 0); return 1; }

    int errors = 0;
    for (const EnumEntryEx& e : entries) {
        lua_pushvalue(L, 2);
        PushFName(L, e.CmpIndex, e.Number);
        lua_pushinteger(L, static_cast<lua_Integer>(e.Value));
        if (lua_pcall(L, 2, 1, 0) == LUA_OK) {

            bool stop = lua_isboolean(L, -1) && lua_toboolean(L, -1);
            lua_pop(L, 1);
            if (stop) break;
        } else {
            ++errors;
            lua_pop(L, 1);
        }
    }
    lua_pushinteger(L, errors);
    return 1;
}

int Compat_UObj_EnumGetNameByIndex(lua_State* L) {
    UObject* obj = SelfObject(L);
    if (!IsEnumObject(obj)) { lua_pushnil(L); return 1; }
    lua_Integer index = static_cast<lua_Integer>(luaL_checkinteger(L, 2));

    std::vector<EnumEntryEx> entries;
    if (!ReadEnumEntriesSafe(obj, entries)) { lua_pushnil(L); return 1; }
    if (index < 0 || index >= static_cast<lua_Integer>(entries.size())) { lua_pushnil(L); return 1; }

    const EnumEntryEx& e = entries[static_cast<size_t>(index)];
    PushFName(L, e.CmpIndex, e.Number);
    lua_pushinteger(L, static_cast<lua_Integer>(e.Value));
    return 2;
}

struct EnumArraysEx {
    bool split = false;
    uint8_t* names = nullptr;
    uint8_t* values = nullptr;
    uint8_t* numAddr = nullptr;
    uint8_t* maxAddr = nullptr;
    int32_t count = 0;
    int32_t capacity = 0;
    int32_t nameStride = 8;
    int32_t entryStride = 16;
};

static bool LocateEnumArrays(void* enumObj, EnumArraysEx& out) {
    if (!enumObj) return false;
    bool splitLayout = false;
    int32_t namesOff = EnsureEnumNamesLayout(enumObj, splitLayout);
    if (namesOff < 0) return false;

    out.nameStride = (g_CompatEnumFNameStride > 0) ? g_CompatEnumFNameStride
        : ((g_State.FName_Size > 0) ? ((g_State.FName_Size + 7) & ~7) : 8);
    out.entryStride = out.nameStride + 8;
    const uint8_t* base = reinterpret_cast<const uint8_t*>(enumObj);

    if (splitLayout) {
        uintptr_t namesTag = 0, valuesTag = 0;
        int32_t num = 0;
        if (!SafeMem_Read(const_cast<uint8_t*>(base) + namesOff, &namesTag, 8)) return false;
        if (!SafeMem_Read(const_cast<uint8_t*>(base) + namesOff + 8, &valuesTag, 8)) return false;
        if (!SafeMem_Read(const_cast<uint8_t*>(base) + namesOff + 16, &num, 4)) return false;
        if (num < 0 || num > 8192) return false;
        out.split = true;
        out.names = reinterpret_cast<uint8_t*>(namesTag & ~static_cast<uintptr_t>(1));
        out.values = reinterpret_cast<uint8_t*>(valuesTag & ~static_cast<uintptr_t>(1));
        out.numAddr = const_cast<uint8_t*>(base) + namesOff + 16;
        out.count = num;
        if (!out.names || !out.values) return false;
        if (num > 0 && !SafeMem_IsReadable(out.names, static_cast<size_t>(out.nameStride) * num)) return false;
        if (num > 0 && !SafeMem_IsReadable(out.values, static_cast<size_t>(8) * num)) return false;
        return true;
    }

    void* data = nullptr;
    int32_t num = 0, maxNum = 0;
    if (!SafeMem_Read(const_cast<uint8_t*>(base) + namesOff, &data, 8)) return false;
    if (!SafeMem_Read(const_cast<uint8_t*>(base) + namesOff + 8, &num, 4)) return false;
    if (!SafeMem_Read(const_cast<uint8_t*>(base) + namesOff + 12, &maxNum, 4)) return false;
    if (num < 0 || num > 8192 || maxNum < num) return false;
    out.split = false;
    out.names = static_cast<uint8_t*>(data);
    out.numAddr = const_cast<uint8_t*>(base) + namesOff + 8;
    out.maxAddr = const_cast<uint8_t*>(base) + namesOff + 12;
    out.count = num;
    out.capacity = maxNum;
    if (num > 0 && (!out.names || !SafeMem_IsReadable(out.names, static_cast<size_t>(out.entryStride) * num))) return false;
    return true;
}

static bool ResolveNameArg(lua_State* L, int idx, uint32_t& outIndex, uint32_t& outNumber) {
    outIndex = 0;
    outNumber = 0;
    if (lua_isinteger(L, idx)) {
        lua_Integer v = lua_tointeger(L, idx);
        outIndex = static_cast<uint32_t>(v < 0 ? 0 : v);
        return true;
    }
    std::string name;
    if (lua_isstring(L, idx)) name = lua_tostring(L, idx);
    if (name.empty() && lua_isuserdata(L, idx)) {

        int top = lua_gettop(L);
        bool ok = false;
        lua_getmetatable(L, idx);
        if (lua_istable(L, -1)) {
            lua_pushstring(L, "__index");
            lua_rawget(L, -2);
            if (lua_istable(L, -1)) {
                lua_pushstring(L, "GetComparisonIndex");
                lua_rawget(L, -2);
                if (lua_isfunction(L, -1)) {
                    lua_pushvalue(L, idx);
                    if (lua_pcall(L, 1, 1, 0) == LUA_OK && lua_isinteger(L, -1)) {
                        outIndex = static_cast<uint32_t>(lua_tointeger(L, -1));
                        ok = true;
                    }
                    lua_pop(L, 1);
                }
            }
        }
        lua_settop(L, top);
        if (ok) {
            if (outIndex == 0) return false;
            return true;
        }

        lua_pushvalue(L, idx);
        if (luaL_tolstring(L, -1, nullptr)) name = lua_tostring(L, -1);
        lua_pop(L, 2);
    }
    if (name.empty()) return false;
    size_t colon = name.rfind("::");
    std::string leaf = (colon == std::string::npos) ? name : name.substr(colon + 2);
    int32_t found = EngineDiscovery::ResolveFNameIndex(g_State, leaf);
    if (found < 0 && colon != std::string::npos) found = EngineDiscovery::ResolveFNameIndex(g_State, name);
    if (found < 0) return false;
    outIndex = static_cast<uint32_t>(found);
    return true;
}

static void WriteEnumNameSlot(lua_State* L, const EnumArraysEx& arr, int32_t slot, uint32_t nameIdx) {
    uint8_t buf[16] = {};
    *reinterpret_cast<uint32_t*>(buf) = nameIdx;
    if (g_State.FName_NumberOff >= 0 && g_State.FName_NumberOff + 4 <= (int)sizeof(buf)) {
        *reinterpret_cast<uint32_t*>(buf + g_State.FName_NumberOff) = 0;
    }
    (void)L;
    SafeMem_Write(arr.names + static_cast<size_t>(slot) * arr.nameStride, buf,
                  static_cast<size_t>(arr.nameStride) < sizeof(buf) ? static_cast<size_t>(arr.nameStride) : sizeof(buf));
}

static void WriteEnumValueSlot(const EnumArraysEx& arr, int32_t slot, int64_t value) {
    if (arr.split) {
        SafeMem_Write(arr.values + static_cast<size_t>(slot) * 8, &value, sizeof(value));
    } else {
        SafeMem_Write(arr.names + static_cast<size_t>(slot) * arr.entryStride + arr.nameStride, &value, sizeof(value));
    }
}

int Compat_UObj_EnumEditNameAt(lua_State* L) {
    UObject* obj = SelfObject(L);
    if (!IsEnumObject(obj)) { lua_pushboolean(L, 0); return 1; }
    lua_Integer index = luaL_checkinteger(L, 2);
    EnumArraysEx arr;
    if (index < 0 || !LocateEnumArrays(obj, arr) || index >= arr.count) { lua_pushboolean(L, 0); return 1; }
    uint32_t nameIdx = 0;
    uint32_t number = 0;
    if (!ResolveNameArg(L, 3, nameIdx, number)) { lua_pushboolean(L, 0); return 1; }
    WriteEnumNameSlot(L, arr, static_cast<int32_t>(index), nameIdx);
    lua_pushboolean(L, 1);
    return 1;
}

int Compat_UObj_EnumEditValueAt(lua_State* L) {
    UObject* obj = SelfObject(L);
    if (!IsEnumObject(obj)) { lua_pushboolean(L, 0); return 1; }
    lua_Integer index = luaL_checkinteger(L, 2);
    EnumArraysEx arr;
    if (index < 0 || !LocateEnumArrays(obj, arr) || index >= arr.count) { lua_pushboolean(L, 0); return 1; }
    int64_t value = static_cast<int64_t>(luaL_optinteger(L, 3, 0));
    WriteEnumValueSlot(arr, static_cast<int32_t>(index), value);
    lua_pushboolean(L, 1);
    return 1;
}

int Compat_UObj_EnumInsertIntoNames(lua_State* L) {
    UObject* obj = SelfObject(L);
    if (!IsEnumObject(obj)) { lua_pushboolean(L, 0); return 1; }

    uint32_t nameIdx = 0;
    uint32_t number = 0;
    if (!ResolveNameArg(L, 2, nameIdx, number)) { lua_pushboolean(L, 0); return 1; }
    int64_t value = static_cast<int64_t>(luaL_checkinteger(L, 3));
    lua_Integer index = luaL_checkinteger(L, 4);

    if (lua_toboolean(L, 5) != 0) {
        LogLine("[Lua] UEnum:InsertIntoNames refused: renumbering the following "
                "entries' values is not implemented; pass false or omit the flag");
        lua_pushboolean(L, 0);
        return 1;
    }

    EnumArraysEx arr;
    if (!LocateEnumArrays(obj, arr) || index < 0 || index > arr.count) { lua_pushboolean(L, 0); return 1; }

    if (arr.split || arr.capacity <= arr.count || arr.count >= 8191) {
        LogLine("[Lua] UEnum:InsertIntoNames refused: the name array has no verified spare capacity on this engine layout");
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!SafeMem_IsReadable(arr.names, static_cast<size_t>(arr.capacity) * arr.entryStride)) {
        LogLine("[Lua] UEnum:InsertIntoNames refused: the name array allocation is not fully writable");
        lua_pushboolean(L, 0);
        return 1;
    }

    int32_t slot = static_cast<int32_t>(index);

    memmove(arr.names + static_cast<size_t>(slot + 1) * arr.entryStride,
            arr.names + static_cast<size_t>(slot) * arr.entryStride,
            static_cast<size_t>(arr.count - slot) * arr.entryStride);
    uint8_t entryBuf[32] = {};
    *reinterpret_cast<uint32_t*>(entryBuf) = nameIdx;
    if (g_State.FName_NumberOff >= 0) *reinterpret_cast<uint32_t*>(entryBuf + g_State.FName_NumberOff) = number;
    *reinterpret_cast<int64_t*>(entryBuf + arr.nameStride) = value;
    SafeMem_Write(arr.names + static_cast<size_t>(slot) * arr.entryStride, entryBuf, static_cast<size_t>(arr.entryStride));
    int32_t newCount = arr.count + 1;
    SafeMem_Write(arr.numAddr, &newCount, 4);
    lua_pushboolean(L, 1);
    return 1;
}

int Compat_UObj_EnumRemoveFromNamesAt(lua_State* L) {
    UObject* obj = SelfObject(L);
    if (!IsEnumObject(obj)) { lua_pushboolean(L, 0); return 1; }
    lua_Integer index = luaL_checkinteger(L, 2);
    lua_Integer countToRemove = luaL_optinteger(L, 3, 1);
    EnumArraysEx arr;
    if (!LocateEnumArrays(obj, arr) || index < 0 || countToRemove < 1) { lua_pushboolean(L, 0); return 1; }

    int32_t slot = static_cast<int32_t>(index);
    int32_t remove = static_cast<int32_t>(countToRemove);
    if (slot >= arr.count) { lua_pushboolean(L, 0); return 1; }
    if (slot + remove > arr.count) remove = arr.count - slot;

    int32_t tail = arr.count - slot - remove;
    if (tail > 0 && SafeMem_IsReadable(arr.names, static_cast<size_t>(arr.capacity ? arr.capacity : arr.count)
                                              * (arr.split ? arr.nameStride : arr.entryStride))) {
        if (!arr.split) {
            memmove(arr.names + static_cast<size_t>(slot) * arr.entryStride,
                    arr.names + static_cast<size_t>(slot + remove) * arr.entryStride,
                    static_cast<size_t>(tail) * arr.entryStride);
        } else {
            memmove(arr.names + static_cast<size_t>(slot) * arr.nameStride,
                    arr.names + static_cast<size_t>(slot + remove) * arr.nameStride,
                    static_cast<size_t>(tail) * arr.nameStride);
            memmove(arr.values + static_cast<size_t>(slot) * 8,
                    arr.values + static_cast<size_t>(slot + remove) * 8,
                    static_cast<size_t>(tail) * 8);
        }
    }
    int32_t newCount = arr.count - remove;
    SafeMem_Write(arr.numAddr, &newCount, 4);
    lua_pushboolean(L, 1);
    return 1;
}

int Compat_UObj_GetFunctionFlags(lua_State* L) {
    UObject* obj = SelfObject(L);
    if (!IsFunctionObject(obj) || g_State.UFunction_FlagsOff < 0) { lua_pushnil(L); return 1; }
    uint32_t flags = 0;
    if (!SafeMem_Read(reinterpret_cast<uint8_t*>(obj) + g_State.UFunction_FlagsOff, &flags, 4)) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, static_cast<lua_Integer>(flags));
    return 1;
}

int Compat_UObj_SetFunctionFlags(lua_State* L) {
    UObject* obj = SelfObject(L);
    if (!IsFunctionObject(obj) || g_State.UFunction_FlagsOff < 0) { lua_pushboolean(L, 0); return 1; }
    lua_Integer flags = static_cast<lua_Integer>(luaL_optinteger(L, 2, 0));
    uint32_t value = static_cast<uint32_t>(flags);
    lua_pushboolean(L, SafeMem_Write(reinterpret_cast<uint8_t*>(obj) + g_State.UFunction_FlagsOff, &value, 4) ? 1 : 0);
    return 1;
}

static bool InvokeIterationCallback(lua_State* L, int fnIdx, int* errors) {

    lua_pushvalue(L, fnIdx);
    lua_insert(L, -2);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        ++(*errors);
        lua_pop(L, 1);
        return false;
    }
    bool stop = lua_isboolean(L, -1) && lua_toboolean(L, -1);
    lua_pop(L, 1);
    return stop;
}

int Compat_UObj_ForEachProperty(lua_State* L) {
    UObject* obj = SelfObject(L);
    if (!obj || !lua_isfunction(L, 2)) return 0;
    if (!LooksLikeStructObject(obj) || g_State.UStruct_ChildPropsOff < 0) {
        lua_pushinteger(L, 0);
        return 1;
    }

    int errors = 0;
    bool stopped = false;
    void* current = obj;
    for (int depth = 0; current && depth < 64 && !stopped; ++depth) {
        void* child = EngineDiscovery::GetChildPropsHead(g_State, static_cast<UStruct*>(current));
        int guard = 0;
        while (child && ++guard < 8192) {
            CompatPushPropertyWrapper(L, child, current);
            stopped = InvokeIterationCallback(L, 2, &errors);
            if (stopped) break;

            child = g_State.bUseFProperty ? EngineDiscovery::GetFFieldNext(g_State, child)
                                         : CompatFieldNext(child);
        }
        current = EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(current));
    }
    lua_pushinteger(L, errors);
    return 1;
}

int Compat_UObj_ForEachFunction(lua_State* L) {
    UObject* obj = SelfObject(L);
    if (!obj || !lua_isfunction(L, 2)) return 0;
    if (!LooksLikeStructObject(obj) || g_State.UStruct_ChildrenOff < 0) {
        lua_pushinteger(L, 0);
        return 1;
    }

    int errors = 0;
    bool stopped = false;
    void* current = obj;
    for (int depth = 0; current && depth < 64 && !stopped; ++depth) {
        void* child = EngineDiscovery::GetChildrenHead(g_State, static_cast<UStruct*>(current));
        int guard = 0;
        while (child && ++guard < 8192) {
            if (IsFunctionObject(static_cast<UObject*>(child))) {
                PushUObject(L, static_cast<UObject*>(child));
                stopped = InvokeIterationCallback(L, 2, &errors);
                if (stopped) break;
            }
            child = CompatFieldNext(child);
        }
        current = EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(current));
    }
    lua_pushinteger(L, errors);
    return 1;
}

struct DataTableLayout {
    bool Valid = false;
    int32_t RowMapOffset = -1;
    void* RowStruct = nullptr;
    int32_t Stride = 0;
    uint8_t* PairData = nullptr;
    int32_t ArrayNum = 0;
    int32_t ArrayMax = 0;
};

static DataTableLayout ReadDataTableLayout(UObject* dt) {
    DataTableLayout layout;
    if (!IsDataTableObject(dt)) return layout;

    layout.RowMapOffset = CompatRowMapOffset(dt);
    if (layout.RowMapOffset < 0) return layout;

    TArray<uint8_t> mapData = {};
    if (!SafeMem_Read(reinterpret_cast<uint8_t*>(dt) + layout.RowMapOffset, &mapData, sizeof(mapData))) return layout;
    if (!mapData.Data || mapData.ArrayNum < 0) return layout;

    layout.RowStruct = CompatRowStruct(dt);
    layout.Stride = CompatRowMapStride(reinterpret_cast<uint8_t*>(mapData.Data), mapData.ArrayNum);
    layout.PairData = reinterpret_cast<uint8_t*>(mapData.Data);
    layout.ArrayNum = mapData.ArrayNum;
    layout.ArrayMax = mapData.ArrayMax;
    layout.Valid = layout.Stride > 0;
    return layout;
}

int Compat_UObj_DataTableGetRowMap(lua_State* L) {
    UObject* dt = SelfObject(L);
    if (!IsDataTableObject(dt)) { lua_pushnil(L); return 1; }

    DataTableLayout layout = ReadDataTableLayout(dt);
    if (!layout.Valid) { lua_newtable(L); return 1; }

    lua_newtable(L);
    for (int32_t i = 0; i < layout.ArrayNum && i < 10000; ++i) {
        uint8_t* pairAddr = layout.PairData + static_cast<size_t>(i) * layout.Stride;
        if (!SafeMem_IsReadable(pairAddr, sizeof(FName) + sizeof(void*))) continue;

        FName rowName = {};
        SafeMem_Read(pairAddr, &rowName, sizeof(rowName));
        std::string nameStr = EngineDiscovery::FNameToStr(g_State, rowName);
        if (nameStr.empty() || nameStr == "None") continue;

        uint8_t* rowBuf = nullptr;
        SafeMem_Read(pairAddr + sizeof(FName), &rowBuf, sizeof(void*));
        if (!rowBuf || !SafeMem_IsReadable(rowBuf, 4)) continue;

        lua_pushstring(L, nameStr.c_str());
        CompatPushRowTable(L, layout.RowStruct, rowBuf);
        lua_settable(L, -3);
    }
    return 1;
}

int Compat_UObj_DataTableGetAllRows(lua_State* L) {
    UObject* dt = SelfObject(L);
    if (!IsDataTableObject(dt)) { lua_pushnil(L); return 1; }

    DataTableLayout layout = ReadDataTableLayout(dt);
    lua_newtable(L);
    if (!layout.Valid) return 1;

    int outIndex = 0;
    for (int32_t i = 0; i < layout.ArrayNum && i < 10000; ++i) {
        uint8_t* pairAddr = layout.PairData + static_cast<size_t>(i) * layout.Stride;
        if (!SafeMem_IsReadable(pairAddr, sizeof(FName) + sizeof(void*))) continue;

        uint8_t* rowBuf = nullptr;
        SafeMem_Read(pairAddr + sizeof(FName), &rowBuf, sizeof(void*));
        if (!rowBuf || !SafeMem_IsReadable(rowBuf, 4)) continue;

        FName rowName = {};
        SafeMem_Read(pairAddr, &rowName, sizeof(rowName));
        std::string nameStr = EngineDiscovery::FNameToStr(g_State, rowName);
        if (nameStr.empty() || nameStr == "None") continue;

        CompatPushRowTable(L, layout.RowStruct, rowBuf);
        lua_pushstring(L, nameStr.c_str());
        lua_setfield(L, -2, "RowName");
        lua_rawseti(L, -2, ++outIndex);
    }
    return 1;
}

int Compat_UObj_DataTableGetRowStruct(lua_State* L) {
    UObject* dt = SelfObject(L);
    if (!IsDataTableObject(dt)) { lua_pushnil(L); return 1; }
    void* rowStruct = CompatRowStruct(dt);
    if (!rowStruct) { lua_pushnil(L); return 1; }
    PushUObject(L, static_cast<UObject*>(rowStruct));
    return 1;
}

static bool ShouldDeferContainerWrite() {
    LuaHookManager& mgr = LuaHookManager::Get();
    return mgr.IsHookInstalled() && !mgr.IsInGameThread();
}

static bool WriteRowFields(lua_State* L, int fieldsIdx, void* rowStruct, uint8_t* rowBuf) {
    if (!lua_istable(L, fieldsIdx) || !rowStruct || !rowBuf) return false;
    bool wrote = false;
    lua_pushnil(L);
    while (lua_next(L, fieldsIdx) != 0) {
        if (lua_isstring(L, -2)) {
            const char* fieldName = lua_tostring(L, -2);
            FoundPropertyEx fp = FindPropertyOnClassEx(reinterpret_cast<UClass*>(rowStruct), fieldName);
            if (fp.Found && fp.Offset >= 0) {
                if (SetPropertyValueEx(L, rowBuf, fp, lua_gettop(L))) wrote = true;
            }
        }
        lua_pop(L, 1);
    }
    return wrote;
}

static uint8_t* FindRowBuffer(DataTableLayout& layout, const std::string& rowName) {
    if (!layout.Valid) return nullptr;
    for (int32_t i = 0; i < layout.ArrayNum && i < 10000; ++i) {
        uint8_t* pairAddr = layout.PairData + static_cast<size_t>(i) * layout.Stride;
        if (!SafeMem_IsReadable(pairAddr, sizeof(FName) + sizeof(void*))) continue;

        uint8_t* rowBuf = nullptr;
        SafeMem_Read(pairAddr + sizeof(FName), &rowBuf, sizeof(void*));
        if (!rowBuf || !SafeMem_IsReadable(rowBuf, 4)) continue;

        FName name = {};
        SafeMem_Read(pairAddr, &name, sizeof(name));
        std::string candidate = EngineDiscovery::FNameToStr(g_State, name);
        if (_stricmp(candidate.c_str(), rowName.c_str()) != 0) continue;
        return rowBuf;
    }
    return nullptr;
}

static bool AddRowInline(lua_State* L, UObject* dt, const char* rowName, int fieldsIdx) {
    if (!dt || !rowName || !rowName[0]) return false;

    DataTableLayout layout = ReadDataTableLayout(dt);
    if (!layout.Valid) {
        LogLine("[Lua] DataTable AddRow: row map layout unavailable on this engine build");
        return false;
    }
    if (!layout.RowStruct) {
        LogLine("[Lua] DataTable AddRow: row struct not resolvable");
        return false;
    }

    uint8_t* existing = FindRowBuffer(layout, rowName);
    if (existing) {
        if (!WriteRowFields(L, fieldsIdx, layout.RowStruct, existing)) return false;
        LogLine("[Lua] DataTable AddRow: updated existing row fields");
        return true;
    }

    if (layout.ArrayNum >= layout.ArrayMax) {
        LogLine("[Lua] DataTable AddRow refused: element array has no spare capacity (no engine reallocation attempted)");
        return false;
    }

    int32_t rowSize = StructSizeOf(layout.RowStruct);
    if (rowSize <= 0 || rowSize > 0x100000) {
        LogLine("[Lua] DataTable AddRow refused: row size unavailable");
        return false;
    }

    void* rowBuf = CompatEngineAllocate(static_cast<size_t>(rowSize));
    if (!rowBuf) {
        LogLine("[Lua] DataTable AddRow refused: engine allocation failed");
        return false;
    }

    int32_t nameIdx = EngineDiscovery::ResolveFNameIndex(g_State, rowName);
    if (nameIdx < 0) return false;

    FName key = { static_cast<uint32_t>(nameIdx), 0 };
    uint8_t* pairAddr = layout.PairData + static_cast<size_t>(layout.ArrayNum) * layout.Stride;
    if (!SafeMem_IsReadable(pairAddr, layout.Stride)) {
        LogLine("[Lua] DataTable AddRow refused: target slot is not readable");
        return false;
    }

    DWORD oldProt = 0;
    if (!VirtualProtect(pairAddr, static_cast<SIZE_T>(layout.Stride), PAGE_READWRITE, &oldProt)) {
        LogLine("[Lua] DataTable AddRow refused: slot is not writable");
        return false;
    }

    bool ok = SafeMem_Write(pairAddr, &key, sizeof(FName));
    if (ok) ok = SafeMem_Write(pairAddr + sizeof(FName), &rowBuf, sizeof(void*));
    if (ok) {
        int32_t newCount = layout.ArrayNum + 1;
        uint8_t* countAddr = reinterpret_cast<uint8_t*>(dt) + layout.RowMapOffset + 8;
        ok = SafeMem_Write(countAddr, &newCount, 4);
    }
    VirtualProtect(pairAddr, static_cast<SIZE_T>(layout.Stride), oldProt, &oldProt);

    if (!ok) {
        LogLine("[Lua] DataTable AddRow failed while writing the new row");
        return false;
    }

    if (lua_istable(L, fieldsIdx)) {
        WriteRowFields(L, fieldsIdx, layout.RowStruct, static_cast<uint8_t*>(rowBuf));
    }

    LogLine("[Lua] DataTable AddRow: appended a row (element array only; the engine's hash index is not rebuilt)");
    return true;
}

static bool RemoveRowInline(UObject* dt, const char* rowName) {
    if (!dt || !rowName || !rowName[0]) return false;

    DataTableLayout layout = ReadDataTableLayout(dt);
    if (!layout.Valid) return false;

    for (int32_t i = 0; i < layout.ArrayNum && i < 10000; ++i) {
        uint8_t* pairAddr = layout.PairData + static_cast<size_t>(i) * layout.Stride;
        if (!SafeMem_IsReadable(pairAddr, sizeof(FName) + sizeof(void*))) continue;
        FName name = {};
        SafeMem_Read(pairAddr, &name, sizeof(name));
        std::string candidate = EngineDiscovery::GetCachedFName(g_State, static_cast<int32_t>(name.ComparisonIndex));
        if (_stricmp(candidate.c_str(), rowName) != 0) continue;

        int32_t remaining = layout.ArrayNum - i - 1;
        size_t moveBytes = static_cast<size_t>(remaining) * layout.Stride;
        uint8_t* nextPair = pairAddr + layout.Stride;

        DWORD oldProt = 0;
        if (!VirtualProtect(pairAddr, static_cast<SIZE_T>(layout.Stride) + moveBytes, PAGE_READWRITE, &oldProt)) return false;

        bool ok = true;
        if (moveBytes > 0) {
            std::vector<uint8_t> buffer(moveBytes);
            if (SafeMem_ReadSpanExact(nextPair, buffer.data(), moveBytes) == moveBytes) {
                ok = SafeMem_Write(pairAddr, buffer.data(), moveBytes);
            } else {
                ok = false;
            }
        }
        if (ok) {
            int32_t newCount = layout.ArrayNum - 1;
            uint8_t* countAddr = reinterpret_cast<uint8_t*>(dt) + layout.RowMapOffset + 8;
            ok = SafeMem_Write(countAddr, &newCount, 4);
        }
        VirtualProtect(pairAddr, static_cast<SIZE_T>(layout.Stride) + moveBytes, oldProt, &oldProt);
        return ok;
    }
    return false;
}

static bool EmptyTableInline(UObject* dt) {
    if (!dt) return false;
    DataTableLayout layout = ReadDataTableLayout(dt);
    if (!layout.Valid) return false;

    uint8_t* countAddr = reinterpret_cast<uint8_t*>(dt) + layout.RowMapOffset + 8;
    int32_t zero = 0;
    DWORD oldProt = 0;
    if (!VirtualProtect(countAddr, 4, PAGE_READWRITE, &oldProt)) return false;
    bool ok = SafeMem_Write(countAddr, &zero, 4);
    VirtualProtect(countAddr, 4, oldProt, &oldProt);
    return ok;
}

static int DataTable_AddRow_Deferred(lua_State* L) {
    UObject* dt = GetUObject(L, lua_upvalueindex(1));
    const char* rowName = lua_tostring(L, lua_upvalueindex(2));
    bool ok = AddRowInline(L, dt, rowName, lua_upvalueindex(3));
    char buf[512];
    snprintf(buf, sizeof(buf), "[Lua] DataTable AddRow (deferred): %s", ok ? "applied" : "not applied");
    LogLine(buf);
    return 0;
}

static int DataTable_RemoveRow_Deferred(lua_State* L) {
    UObject* dt = GetUObject(L, lua_upvalueindex(1));
    const char* rowName = lua_tostring(L, lua_upvalueindex(2));
    bool ok = RemoveRowInline(dt, rowName);
    char buf[512];
    snprintf(buf, sizeof(buf), "[Lua] DataTable RemoveRow (deferred): %s", ok ? "applied" : "not applied");
    LogLine(buf);
    return 0;
}

static int DataTable_EmptyTable_Deferred(lua_State* L) {
    UObject* dt = GetUObject(L, lua_upvalueindex(1));
    bool ok = EmptyTableInline(dt);
    char buf[512];
    snprintf(buf, sizeof(buf), "[Lua] DataTable EmptyTable (deferred): %s", ok ? "applied" : "not applied");
    LogLine(buf);
    return 0;
}

static bool EnqueueGameThreadClosure(lua_State* L, lua_CFunction fn, int nargsFromBase) {

    lua_pushcclosure(L, fn, nargsFromBase);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    LuaHookManager::Get().EnqueueGameThreadTask(ref);
    return true;
}

int Compat_UObj_DataTableAddRow(lua_State* L) {
    UObject* dt = SelfObject(L);
    if (!IsDataTableObject(dt) || !lua_isstring(L, 2)) { lua_pushboolean(L, 0); return 1; }

    if (ShouldDeferContainerWrite()) {
        lua_pushvalue(L, 1);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        EnqueueGameThreadClosure(L, DataTable_AddRow_Deferred, 3);
        lua_pushboolean(L, 1);
        return 1;
    }

    bool ok = AddRowInline(L, dt, lua_tostring(L, 2), 3);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int Compat_UObj_DataTableRemoveRow(lua_State* L) {
    UObject* dt = SelfObject(L);
    if (!IsDataTableObject(dt) || !lua_isstring(L, 2)) { lua_pushboolean(L, 0); return 1; }

    if (ShouldDeferContainerWrite()) {
        lua_pushvalue(L, 1);
        lua_pushvalue(L, 2);
        EnqueueGameThreadClosure(L, DataTable_RemoveRow_Deferred, 2);
        lua_pushboolean(L, 1);
        return 1;
    }

    lua_pushboolean(L, RemoveRowInline(dt, lua_tostring(L, 2)) ? 1 : 0);
    return 1;
}

int Compat_UObj_DataTableEmptyTable(lua_State* L) {
    UObject* dt = SelfObject(L);
    if (!IsDataTableObject(dt)) { lua_pushboolean(L, 0); return 1; }

    if (ShouldDeferContainerWrite()) {
        lua_pushvalue(L, 1);
        EnqueueGameThreadClosure(L, DataTable_EmptyTable_Deferred, 1);
        lua_pushboolean(L, 1);
        return 1;
    }

    lua_pushboolean(L, EmptyTableInline(dt) ? 1 : 0);
    return 1;
}

int Compat_UObj_GetLevel(lua_State* L) {
    UObject* obj = SelfObject(L);
    if (!obj || g_State.UObject_OuterOff < 0) { lua_pushnil(L); return 1; }

    void* current = obj;
    for (int depth = 0; current && depth < 64; ++depth) {
        std::string clsName = ClassNameOf(static_cast<UObject*>(current));
        if (clsName == "Level" || clsName == "ULevel") {
            PushUObject(L, static_cast<UObject*>(current));
            return 1;
        }
        void* next = nullptr;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(current) + g_State.UObject_OuterOff, &next, sizeof(void*))) break;
        if (!next || !SafeMem_IsReadable(next, 8)) break;
        current = next;
    }
    lua_pushnil(L);
    return 1;
}

int Compat_UObj_Len(lua_State* L) {
    UObject* obj = SelfObject(L);
    if (!obj) { lua_pushinteger(L, 0); return 1; }

    if (IsDataTableObject(obj)) {
        DataTableLayout layout = ReadDataTableLayout(obj);
        lua_pushinteger(L, layout.Valid ? layout.ArrayNum : 0);
        return 1;
    }

    lua_pushinteger(L, 0);
    return 1;
}

static const char* FWEAKPTR_MT = "Avalonium.FWeakObjectPtr";

struct LuaWeakPtrData {
    void* Object = nullptr;
};

static bool IsObjectLiveBestEffort(void* obj) {
    if (!obj || !SafeMem_IsReadable(obj, 0x28)) return false;
    if (g_State.UObject_FlagsOff >= 0) {
        uint32_t flags = 0;
        if (SafeMem_Read(reinterpret_cast<uint8_t*>(obj) + g_State.UObject_FlagsOff, &flags, 4)) {
            if (flags & 0x00000100) return false;
        }
    }
    return EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj)) != nullptr;
}

void CompatPushWeakObjectPtr(lua_State* L, void* obj) {
    auto* data = static_cast<LuaWeakPtrData*>(lua_newuserdata(L, sizeof(LuaWeakPtrData)));
    data->Object = obj;
    luaL_setmetatable(L, FWEAKPTR_MT);
}

static int WeakPtr_Get(lua_State* L) {
    auto* data = static_cast<LuaWeakPtrData*>(luaL_checkudata(L, 1, FWEAKPTR_MT));
    if (!data || !IsObjectLiveBestEffort(data->Object)) { lua_pushnil(L); return 1; }
    PushUObject(L, static_cast<UObject*>(data->Object));
    return 1;
}

static int WeakPtr_IsValid(lua_State* L) {
    auto* data = static_cast<LuaWeakPtrData*>(luaL_testudata(L, 1, FWEAKPTR_MT));
    lua_pushboolean(L, (data && IsObjectLiveBestEffort(data->Object)) ? 1 : 0);
    return 1;
}

static int WeakPtr_TypeName(lua_State* L) {
    (void)L;
    lua_pushstring(L, "FWeakObjectPtr");
    return 1;
}

static int WeakPtr_ToString(lua_State* L) {
    auto* data = static_cast<LuaWeakPtrData*>(luaL_testudata(L, 1, FWEAKPTR_MT));
    char buf[128];
    snprintf(buf, sizeof(buf), "FWeakObjectPtr(0x%p)%s", data ? data->Object : nullptr,
             (data && IsObjectLiveBestEffort(data->Object)) ? " valid" : " stale");
    lua_pushstring(L, buf);
    return 1;
}

static int WeakPtr_Index(lua_State* L) {
    auto* data = static_cast<LuaWeakPtrData*>(luaL_checkudata(L, 1, FWEAKPTR_MT));
    const char* key = lua_tostring(L, 2);
    if (!data || !key) { lua_pushnil(L); return 1; }
    if (_stricmp(key, "Get") == 0 || _stricmp(key, "get") == 0) { lua_pushcfunction(L, WeakPtr_Get); return 1; }
    if (_stricmp(key, "IsValid") == 0) { lua_pushcfunction(L, WeakPtr_IsValid); return 1; }
    if (_stricmp(key, "type") == 0) { lua_pushcfunction(L, WeakPtr_TypeName); return 1; }
    lua_pushnil(L);
    return 1;
}

static int WeakPtr_Constructor(lua_State* L) {
    void* obj = GetUObject(L, 1);
    if (!obj && lua_islightuserdata(L, 1)) obj = lua_touserdata(L, 1);
    if (!obj) {

        auto* existing = static_cast<LuaWeakPtrData*>(luaL_testudata(L, 1, FWEAKPTR_MT));
        if (existing) obj = existing->Object;
    }
    if (!obj && lua_istable(L, 1)) {

        lua_getfield(L, 1, "Object");
        obj = GetUObject(L, -1);
        lua_pop(L, 1);
    }
    CompatPushWeakObjectPtr(L, obj);
    return 1;
}

template <typename Fn>
static void Bind(lua_State* L, Fn fn, const char* name) {
    lua_pushcfunction(L, fn);
    lua_setglobal(L, name);
}

static void BindLifecycleHook(lua_State* L, const char* fnName, const char* preGlobal, const char* postGlobal) {
    lua_pushstring(L, fnName);
    lua_pushinteger(L, 0);
    lua_pushcclosure(L, ExecLifecycleHook_Impl, 2);
    lua_setglobal(L, preGlobal);

    lua_pushstring(L, fnName);
    lua_pushinteger(L, 1);
    lua_pushcclosure(L, ExecLifecycleHook_Impl, 2);
    lua_setglobal(L, postGlobal);
}

void RegisterCompatParityApi(lua_State* L) {

    Bind(L, Compat_CreateInvalidObject, "CreateInvalidObject");
    Bind(L, Compat_FindObject, "FindObject");
    Bind(L, Compat_FindObjects, "FindObjects");

    Bind(L, Compat_LoadExport, "LoadExport");

    Bind(L, Compat_LoadExport, "loadexport");

    Bind(L, Compat_RestartMod, "RestartMod");
    Bind(L, Compat_RestartMod, "RestartCurrentMod");
    Bind(L, Compat_UninstallMod, "UninstallMod");
    Bind(L, Compat_UninstallMod, "UninstallCurrentMod");

    Bind(L, Compat_IterateGameDirectories, "IterateGameDirectories");
    Bind(L, Compat_CreateLogicModsDirectory, "CreateLogicModsDirectory");

    Bind(L, Compat_RegisterCustomProperty, "RegisterCustomProperty");

    Bind(L, Compat_RegisterConsoleCommandGlobalHandler, "RegisterConsoleCommandGlobalHandler");

    BindLifecycleHook(L, "ProcessConsoleExec", "RegisterProcessConsoleExecPreHook", "RegisterProcessConsoleExecPostHook");
    BindLifecycleHook(L, "CallFunctionByNameWithArguments", "RegisterCallFunctionByNameWithArgumentsPreHook", "RegisterCallFunctionByNameWithArgumentsPostHook");
    BindLifecycleHook(L, "Exec", "RegisterULocalPlayerExecPreHook", "RegisterULocalPlayerExecPostHook");

    Bind(L, Compat_RegisterBeginPlayPreHook, "RegisterBeginPlayPreHook");
    Bind(L, Compat_RegisterBeginPlayPostHook, "RegisterBeginPlayPostHook");
    Bind(L, Compat_RegisterEndPlayPreHook, "RegisterEndPlayPreHook");
    Bind(L, Compat_RegisterEndPlayPostHook, "RegisterEndPlayPostHook");
    Bind(L, Compat_RegisterLoadMapPreHook, "RegisterLoadMapPreHook");
    Bind(L, Compat_RegisterLoadMapPostHook, "RegisterLoadMapPostHook");
    Bind(L, Compat_RegisterInitGameStatePreHook, "RegisterInitGameStatePreHook");
    Bind(L, Compat_RegisterInitGameStatePostHook, "RegisterInitGameStatePostHook");

    Bind(L, Compat_GenerateSDK, "GenerateSDK");
    Bind(L, Compat_GenerateUHTCompatibleHeaders, "GenerateUHTCompatibleHeaders");
    Bind(L, Compat_DumpUSMAP, "DumpUSMAP");
    Bind(L, Compat_DumpJMAP, "DumpJMAP");
    Bind(L, Compat_DumpAllActors, "DumpAllActors");
    Bind(L, Compat_DumpStaticMeshes, "DumpStaticMeshes");

    Bind(L, Quat_Constructor, "FQuat");
    Bind(L, Vec2_Constructor, "FVector2D");
    Bind(L, Vec4_Constructor, "FVector4");

    Bind(L, WeakPtr_Constructor, "FWeakObjectPtr");
    Bind(L, WeakPtr_Constructor, "WeakObjectPtr");

    lua_getglobal(L, "RegisterKeyBind");
    if (lua_isfunction(L, -1)) {
        lua_setglobal(L, "RegisterKeyBindAsync");
    } else {
        lua_pop(L, 1);
    }

    if (luaL_newmetatable(L, FQUAT_MT)) {
        lua_pushcfunction(L, Quat_Index);     lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, Quat_NewIndex);  lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, Quat_ToString);  lua_setfield(L, -2, "__tostring");
        lua_pushstring(L, "FQuat");           lua_setfield(L, -2, "__name");
    }
    lua_pop(L, 1);

    if (luaL_newmetatable(L, FVEC2_MT)) {
        lua_pushcfunction(L, Vec2_Index);     lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, Vec2_NewIndex);  lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, Vec2_ToString);  lua_setfield(L, -2, "__tostring");
        lua_pushstring(L, "FVector2D");       lua_setfield(L, -2, "__name");
    }
    lua_pop(L, 1);

    if (luaL_newmetatable(L, FVEC4_MT)) {
        lua_pushcfunction(L, Vec4_Index);     lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, Vec4_NewIndex);  lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, Vec4_ToString);  lua_setfield(L, -2, "__tostring");
        lua_pushstring(L, "FVector4");        lua_setfield(L, -2, "__name");
    }
    lua_pop(L, 1);

    if (luaL_newmetatable(L, FPROPERTY_MT)) {
        lua_pushcfunction(L, FProperty_Index);    lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, FProperty_NewIndex); lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, FProp_ToString);     lua_setfield(L, -2, "__tostring");
        lua_pushstring(L, "FProperty");           lua_setfield(L, -2, "__name");
    }
    lua_pop(L, 1);

    if (luaL_newmetatable(L, FWEAKPTR_MT)) {
        lua_pushcfunction(L, WeakPtr_Index);     lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, WeakPtr_ToString);  lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, WeakPtr_Get);       lua_setfield(L, -2, "__call");
        lua_pushstring(L, "FWeakObjectPtr");     lua_setfield(L, -2, "__name");
    }
    lua_pop(L, 1);

    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "[Lua] Compat surface v%d.%d: wrappers=on, datatable-mutation=on, weak-objects=on",
                 COMPAT_API_VERSION_MAJOR, COMPAT_API_VERSION_MINOR);
        LogLine(buf);
    }
}

}
