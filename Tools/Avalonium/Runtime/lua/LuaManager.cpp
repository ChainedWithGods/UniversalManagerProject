#include "LuaManager.h"
#include "LuaTypes.h"
#include "LuaUObject.h"
#include "LuaHookManager.h"
#include "../api/IPCEvents.h"
#include "LuaCompatApi.h"
#include "LuaTypeGenerator.h"
#include "BuiltinMods/BuiltinMods.h"
#include "../overlay/LuaOverlay.h"
#include "../core/BlueprintDiscovery.h"
#include "../core/EngineDiscovery.h"
#include "../core/ObjectUtils.h"
#include "../core/RuntimeConfig.h"
#include "../api/UEInternalAPI.h"
#include "../console/ConsoleInternal.h"
#include "../utils/SafeMemory.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <unordered_set>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <unordered_set>
#include <windows.h>
#include <psapi.h>

extern EngineState g_State;

static void* GetPointerFromLua(lua_State* L, int index) {
    if (lua_isnil(L, index) || lua_gettop(L) < index) {
        return nullptr;
    }
    UObject* uobj = LuaEngine::GetUObject(L, index);
    if (uobj) return uobj;

    if (lua_islightuserdata(L, index)) {
        return lua_touserdata(L, index);
    }
    if (lua_isinteger(L, index)) {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(lua_tointeger(L, index)));
    }
    if (lua_isnumber(L, index)) {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(lua_tonumber(L, index)));
    }
    if (lua_istable(L, index)) {
        lua_getfield(L, index, "ptr");
        if (!lua_isnil(L, -1)) {
            void* p = GetPointerFromLua(L, -1);
            lua_pop(L, 1);
            return p;
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "__ptr");
        if (!lua_isnil(L, -1)) {
            void* p = GetPointerFromLua(L, -1);
            lua_pop(L, 1);
            return p;
        }
        lua_pop(L, 1);
    }
    if (lua_isstring(L, index)) {
        const char* str = lua_tostring(L, index);
        if (str && (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))) {
            uintptr_t val = 0;
            std::stringstream ss;
            ss << std::hex << (str + 2);
            ss >> val;
            return reinterpret_cast<void*>(val);
        }
    }
    return nullptr;
}

static bool ClassInheritsFrom(const EngineState& state, UClass* cls, const char* baseName) {
    if (!cls || !baseName) return false;
    UStruct* current = reinterpret_cast<UStruct*>(cls);
    int depth = 0;
    while (current && depth < 64) {
        std::string name = EngineDiscovery::GetObjectNameStr(state, reinterpret_cast<UObject*>(current));
        if (!name.empty()) {

            if (name == baseName) return true;

            if (name.size() > 1 && name.back() != '_') {

                if (name.size() > 1 && (name[0] == 'A' || name[0] == 'U') &&
                    name.substr(1) == baseName) return true;
            }
        }
        UStruct* super = EngineDiscovery::GetSuperStruct(state, current);
        if (super == current) break;
        current = super;
        ++depth;
    }
    return false;
}

namespace {

std::mutex g_IpcTokenMutex;
std::unordered_set<uint64_t> g_IpcTokens;

std::string IpcEscapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string IpcCallStringMethod(lua_State* L, int objectIndex, const char* method) {
    objectIndex = lua_absindex(L, objectIndex);
    lua_getfield(L, objectIndex, method);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return {};
    }
    lua_pushvalue(L, objectIndex);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        lua_pop(L, 1);
        return {};
    }
    std::string out;
    if (lua_isstring(L, -1)) out = lua_tostring(L, -1);
    lua_pop(L, 1);
    return out;
}

std::string IpcObjectClassName(lua_State* L, int objectIndex) {
    objectIndex = lua_absindex(L, objectIndex);
    lua_getfield(L, objectIndex, "GetClass");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return {};
    }
    lua_pushvalue(L, objectIndex);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        lua_pop(L, 1);
        return {};
    }
    std::string name = IpcCallStringMethod(L, -1, "GetName");
    lua_pop(L, 1);
    return name;
}

}

int LuaManager::Lua_IpcEventCallback(lua_State* L) {
    const char* kind = lua_tostring(L, lua_upvalueindex(1));
    const char* filter = lua_tostring(L, lua_upvalueindex(2));
    const uint64_t token = static_cast<uint64_t>(lua_tointeger(L, lua_upvalueindex(3)));

    {
        std::lock_guard<std::mutex> lock(g_IpcTokenMutex);
        if (g_IpcTokens.find(token) == g_IpcTokens.end()) return 0;
    }

    std::string address, name, className;
    if (lua_gettop(L) >= 1) {
        address = IpcCallStringMethod(L, 1, "GetAddressString");
        name = IpcCallStringMethod(L, 1, "GetName");
        className = IpcObjectClassName(L, 1);
    }

    std::ostringstream oss;
    oss << "{\"event\":\"subscribe\",\"token\":" << token
        << ",\"kind\":\"" << IpcEscapeJson(kind ? kind : "") << "\""
        << ",\"filter\":\"" << IpcEscapeJson(filter ? filter : "") << "\""
        << ",\"self\":\"" << IpcEscapeJson(address) << "\""
        << ",\"selfName\":\"" << IpcEscapeJson(name) << "\""
        << ",\"selfClass\":\"" << IpcEscapeJson(className) << "\"}";
    IPCEvents::Push(oss.str());
    return 0;
}

static void PushPointerToLua(lua_State* L, void* ptr) {
    if (!ptr) {
        lua_pushnil(L);
    } else {
        LuaEngine::PushUObject(L, static_cast<UObject*>(ptr));
    }
}

static int Lua_DirectorSet(lua_State* L);
static int Lua_DirectorGet(lua_State* L);
static int Lua_DirectorSlowFor(lua_State* L);
static int Lua_DirectorToggleBulletTime(lua_State* L);
static int Lua_DirectorToggleFreeze(lua_State* L);
static int Lua_Record(lua_State* L);
static int Lua_Watched(lua_State* L);
static int Lua_SetWatchPatterns(lua_State* L);
static int Lua_GetRecords(lua_State* L);

LuaManager::LuaManager() = default;

LuaManager::~LuaManager() {
    Shutdown();
}

LuaManager& LuaManager::Get() {
    static LuaManager instance;
    return instance;
}

bool LuaManager::Initialize(HMODULE hModule) {
    std::lock_guard<std::recursive_mutex> lock(m_LuaMutex);
    if (m_Initialized) {
        return true;
    }

    m_Module = hModule;
    m_L = luaL_newstate();
    if (!m_L) {
        EngineDiscovery::WriteLogLine("[Lua] Failed to create lua_State\n");
        return false;
    }

    luaL_openlibs(m_L);
    EngineDiscovery::WriteLogLine("[Lua] Initializing Lua runtime...\n");

    char dllPath[MAX_PATH] = {};
    if (GetModuleFileNameA(hModule, dllPath, MAX_PATH)) {
        std::filesystem::path p(dllPath);
        std::filesystem::path modsPath = p.parent_path() / "Mods" / "Avalonium";
        m_ModsDirectory = modsPath.string();
    } else {
        m_ModsDirectory = "Mods/Avalonium";
    }

    std::error_code ec;
    std::filesystem::create_directories(m_ModsDirectory, ec);

    std::vector<std::string> sharedSearches;
    {
        const std::filesystem::path shared = std::filesystem::path(m_ModsDirectory) / "shared";
        for (const char* pattern : { "?.lua", "?/?.lua", "?/init.lua" }) {
            std::string p = (shared / pattern).string();
            std::replace(p.begin(), p.end(), '\\', '/');
            sharedSearches.push_back(p);
        }
    }

    lua_getglobal(m_L, "package");
    if (lua_istable(m_L, -1)) {
        lua_getfield(m_L, -1, "path");
        std::string currentPath = lua_tostring(m_L, -1);
        lua_pop(m_L, 1);
        for (const std::string& search : sharedSearches) {
            currentPath += ";" + search;
        }
        lua_pushstring(m_L, currentPath.c_str());
        lua_setfield(m_L, -2, "path");

        lua_getfield(m_L, -1, "cpath");
        const char* existingCpath = lua_tostring(m_L, -1);
        std::string currentCpath = existingCpath ? existingCpath : "";
        lua_pop(m_L, 1);
        const std::filesystem::path modsRoot(m_ModsDirectory);
        for (const char* pattern : { "?.dll", "?/?.dll", "?/scripts/?.dll", "shared/?.dll" }) {
            std::string search = (modsRoot / pattern).string();
            std::replace(search.begin(), search.end(), '\\', '/');
            currentCpath += ";" + search;
        }
        lua_pushstring(m_L, currentCpath.c_str());
        lua_setfield(m_L, -2, "cpath");
    }
    lua_pop(m_L, 1);

    EngineDiscovery::WriteLogLine("[Lua] Registering Lua bindings...\n");
    RegisterBindings();

    EngineDiscovery::WriteLogLine("[Lua] Initializing hook manager...\n");
    LuaEngine::LuaHookManager::Get().Initialize(m_L);

    LuaEngine::BuiltinMods::RegisterAll(m_L);

    m_Initialized = true;
    EngineDiscovery::WriteLogLine("[Lua] Lua runtime initialized successfully\n");
    return true;
}

void LuaManager::Shutdown() {
    std::lock_guard<std::recursive_mutex> lock(m_LuaMutex);
    LuaEngine::LuaHookManager::Get().Shutdown();

    LuaEngine::Overlay::LuaOverlayManager::Get().ClearAll(m_L);

    if (m_L) {
        lua_close(m_L);
        m_L = nullptr;
    }
    m_LoadedMods.clear();
    m_Initialized = false;
}

void LuaManager::RegisterBindings() {

    LuaEngine::RegisterLuaTypes(m_L);

    LuaEngine::RegisterLuaUObject(m_L);

    LuaEngine::RegisterLuaHookBindings(m_L);

    RegisterGlobalHelpers();
    RegisterEngineGlobals();
    RegisterUETable();
    RegisterMemoryBindings();
    LuaEngine::RegisterCompatibilityLayer(m_L);

    LuaEngine::Overlay::RegisterLuaOverlayBindings(m_L);
}

void LuaManager::RegisterGlobalHelpers() {
    lua_pushcfunction(m_L, Lua_Print);
    lua_setglobal(m_L, "print");

    lua_pushcfunction(m_L, Lua_Log);
    lua_setglobal(m_L, "log");

    lua_pushcfunction(m_L, Lua_GetPlayer);
    lua_setglobal(m_L, "GetPlayer");
    lua_pushcfunction(m_L, Lua_GetPlayer);
    lua_setglobal(m_L, "GetPlayerPawn");
    lua_pushcfunction(m_L, Lua_GetPlayer);
    lua_setglobal(m_L, "GetPlayerCharacter");

    lua_pushcfunction(m_L, Lua_GetPlayerController);
    lua_setglobal(m_L, "GetPlayerController");
    lua_pushcfunction(m_L, Lua_GetPlayerController);
    lua_setglobal(m_L, "GetFirstPlayerController");

    lua_pushcfunction(m_L, Lua_GetPlayerCameraManager);
    lua_setglobal(m_L, "GetPlayerCameraManager");
    lua_pushcfunction(m_L, Lua_GetPlayerCameraManager);
    lua_setglobal(m_L, "GetCameraManager");

    lua_pushcfunction(m_L, Lua_GetCameraInfo);
    lua_setglobal(m_L, "GetCameraInfo");

    lua_pushcfunction(m_L, Lua_ProjectWorldToScreen);
    lua_setglobal(m_L, "ProjectWorldToScreen");

    lua_pushcfunction(m_L, Lua_DeprojectScreenToWorld);
    lua_setglobal(m_L, "DeprojectScreenToWorld");

    lua_pushcfunction(m_L, Lua_GetMousePosition);
    lua_setglobal(m_L, "GetMousePosition");

    lua_pushcfunction(m_L, Lua_GetPlayerState);
    lua_setglobal(m_L, "GetPlayerState");

    lua_pushcfunction(m_L, Lua_GetComponentByClass);
    lua_setglobal(m_L, "GetComponentByClass");

    lua_pushcfunction(m_L, Lua_GetAllComponents);
    lua_setglobal(m_L, "GetAllComponents");

    lua_pushcfunction(m_L, Lua_LineTraceSingle);
    lua_setglobal(m_L, "LineTraceSingle");

    lua_pushcfunction(m_L, Lua_GetActorBounds);
    lua_setglobal(m_L, "GetActorBounds");

    lua_pushcfunction(m_L, Lua_GetActorLocation);
    lua_setglobal(m_L, "GetActorLocation");

    lua_pushcfunction(m_L, Lua_SetActorLocation);
    lua_setglobal(m_L, "SetActorLocation");

    lua_pushcfunction(m_L, Lua_GetViewportSize);
    lua_setglobal(m_L, "GetViewportSize");

    lua_pushcfunction(m_L, Lua_GetLocalPlayer);
    lua_setglobal(m_L, "GetLocalPlayer");

    lua_pushcfunction(m_L, Lua_GetGameInstance);
    lua_setglobal(m_L, "GetGameInstance");

    lua_pushcfunction(m_L, Lua_GetGameMode);
    lua_setglobal(m_L, "GetGameMode");

    lua_pushcfunction(m_L, Lua_GetGameState);
    lua_setglobal(m_L, "GetGameState");

    lua_pushcfunction(m_L, Lua_GetAllActorsOfClass);
    lua_setglobal(m_L, "GetAllActorsOfClass");
    lua_pushcfunction(m_L, Lua_GetAllActorsOfClass);
    lua_setglobal(m_L, "GetAllActors");

    lua_pushcfunction(m_L, Lua_GetDistance);
    lua_setglobal(m_L, "GetDistance");

    lua_pushcfunction(m_L, Lua_GetGameplayStatics);
    lua_setglobal(m_L, "GetGameplayStatics");
    lua_pushcfunction(m_L, Lua_GetKismetSystemLibrary);
    lua_setglobal(m_L, "GetKismetSystemLibrary");
    lua_pushcfunction(m_L, Lua_GetKismetMathLibrary);
    lua_setglobal(m_L, "GetKismetMathLibrary");
    lua_pushcfunction(m_L, Lua_GetKismetStringLibrary);
    lua_setglobal(m_L, "GetKismetStringLibrary");
    lua_pushcfunction(m_L, Lua_GetKismetArrayLibrary);
    lua_setglobal(m_L, "GetKismetArrayLibrary");
    lua_pushcfunction(m_L, Lua_GetKismetTextLibrary);
    lua_setglobal(m_L, "GetKismetTextLibrary");
    lua_pushcfunction(m_L, Lua_GetKismetGuidLibrary);
    lua_setglobal(m_L, "GetKismetGuidLibrary");
    lua_pushcfunction(m_L, Lua_GetKismetRenderingLibrary);
    lua_setglobal(m_L, "GetKismetRenderingLibrary");

    lua_pushcfunction(m_L, Lua_GetFrameTime);
    lua_setglobal(m_L, "GetFrameTime");
    lua_pushcfunction(m_L, Lua_GetFPS);
    lua_setglobal(m_L, "GetFPS");
    lua_pushcfunction(m_L, Lua_GetDeltaSeconds);
    lua_setglobal(m_L, "GetDeltaSeconds");
    lua_pushcfunction(m_L, Lua_GetMemoryUsage);
    lua_setglobal(m_L, "GetMemoryUsage");
    lua_pushcfunction(m_L, Lua_ForceGarbageCollection);
    lua_setglobal(m_L, "ForceGarbageCollection");
    lua_pushcfunction(m_L, Lua_ForceGarbageCollection);
    lua_setglobal(m_L, "CollectGarbage");

    lua_pushcfunction(m_L, Lua_GetCVar);
    lua_setglobal(m_L, "GetCVar");
    lua_pushcfunction(m_L, Lua_SetCVar);
    lua_setglobal(m_L, "SetCVar");
    lua_pushcfunction(m_L, Lua_FindCVars);
    lua_setglobal(m_L, "FindCVars");

    lua_newtable(m_L);
    lua_pushcfunction(m_L, Lua_File_Read);      lua_setfield(m_L, -2, "Read");
    lua_pushcfunction(m_L, Lua_File_Write);     lua_setfield(m_L, -2, "Write");
    lua_pushcfunction(m_L, Lua_File_Append);    lua_setfield(m_L, -2, "Append");
    lua_pushcfunction(m_L, Lua_File_Exists);    lua_setfield(m_L, -2, "Exists");
    lua_pushcfunction(m_L, Lua_File_List);      lua_setfield(m_L, -2, "List");
    lua_pushcfunction(m_L, Lua_File_CreateDir); lua_setfield(m_L, -2, "CreateDir");
    lua_pushcfunction(m_L, Lua_File_CreateDir); lua_setfield(m_L, -2, "MakeDir");
    lua_pushcfunction(m_L, Lua_File_Delete);    lua_setfield(m_L, -2, "Delete");
    lua_setglobal(m_L, "File");

    lua_newtable(m_L);
    lua_pushcfunction(m_L, Lua_JSON_Parse);     lua_setfield(m_L, -2, "Parse");
    lua_pushcfunction(m_L, Lua_JSON_Parse);     lua_setfield(m_L, -2, "decode");
    lua_pushcfunction(m_L, Lua_JSON_Stringify); lua_setfield(m_L, -2, "Stringify");
    lua_pushcfunction(m_L, Lua_JSON_Stringify); lua_setfield(m_L, -2, "encode");
    lua_setglobal(m_L, "JSON");

    lua_pushcfunction(m_L, Lua_GetModDirectory);
    lua_setglobal(m_L, "GetModDirectory");
    lua_pushcfunction(m_L, Lua_GetModDirectory);
    lua_setglobal(m_L, "GetModsDirectory");

    lua_pushcfunction(m_L, Lua_GetWorld);
    lua_setglobal(m_L, "GetWorld");

    lua_pushcfunction(m_L, Lua_GetEngine);
    lua_setglobal(m_L, "GetEngine");

    lua_pushcfunction(m_L, Lua_GetObjectCount);
    lua_setglobal(m_L, "GetObjectCount");

    lua_pushcfunction(m_L, Lua_GetObjectByIndex);
    lua_setglobal(m_L, "GetObjectByIndex");

    lua_pushcfunction(m_L, Lua_FindObject);
    lua_setglobal(m_L, "FindObject");

    lua_pushcfunction(m_L, Lua_FindObjects);
    lua_setglobal(m_L, "FindObjects");

    lua_pushcfunction(m_L, Lua_FindClass);
    lua_setglobal(m_L, "FindClass");

    lua_pushcfunction(m_L, Lua_GetWorldActors);
    lua_setglobal(m_L, "GetWorldActors");

    lua_pushcfunction(m_L, Lua_GetObjectName);
    lua_setglobal(m_L, "GetObjectName");

    lua_pushcfunction(m_L, Lua_GetObjectFullName);
    lua_setglobal(m_L, "GetObjectFullName");

    lua_pushcfunction(m_L, Lua_GetObjectClass);
    lua_setglobal(m_L, "GetObjectClass");

    lua_pushcfunction(m_L, Lua_GetSuperClass);
    lua_setglobal(m_L, "GetSuperClass");

    lua_pushcfunction(m_L, Lua_InheritsFrom);
    lua_setglobal(m_L, "InheritsFrom");

    lua_pushcfunction(m_L, Lua_GetClassProperties);
    lua_setglobal(m_L, "GetClassProperties");

    lua_pushcfunction(m_L, Lua_CallFunction);
    lua_setglobal(m_L, "CallFunction");

    lua_pushcfunction(m_L, Lua_ExportProperty);
    lua_setglobal(m_L, "ExportProperty");

    lua_pushcfunction(m_L, Lua_ImportProperty);
    lua_setglobal(m_L, "ImportProperty");

    lua_pushcfunction(m_L, Lua_ReloadMods);
    lua_setglobal(m_L, "ReloadMods");

    lua_pushcfunction(m_L, Lua_ReloadMods);
    lua_setglobal(m_L, "HotReloadMods");
}

void LuaManager::RegisterEngineGlobals() {
    lua_pushcfunction(m_L, Lua_FindFirstOf);
    lua_setglobal(m_L, "FindFirstOf");

    lua_pushcfunction(m_L, Lua_FindAllOf);
    lua_setglobal(m_L, "FindAllOf");

    lua_pushcfunction(m_L, Lua_StaticFindObject);
    lua_setglobal(m_L, "StaticFindObject");

    lua_pushcfunction(m_L, Lua_StaticConstructObject);
    lua_setglobal(m_L, "StaticConstructObject");

    lua_newtable(m_L);
    lua_pushcfunction(m_L, Lua_FindFirstOf); lua_setfield(m_L, -2, "FindFirstOf");
    lua_pushcfunction(m_L, Lua_FindAllOf); lua_setfield(m_L, -2, "FindAllOf");
    lua_pushcfunction(m_L, Lua_StaticFindObject); lua_setfield(m_L, -2, "StaticFindObject");
    lua_pushcfunction(m_L, Lua_FindObjects); lua_setfield(m_L, -2, "FindObjects");
    lua_pushcfunction(m_L, Lua_FindClass); lua_setfield(m_L, -2, "FindClass");
    lua_setglobal(m_L, "UObjectGlobals");

    lua_pushcfunction(m_L, Lua_SpawnActorByClass); lua_setglobal(m_L, "SpawnActorByClass");
    lua_pushcfunction(m_L, Lua_GetAllPawns);       lua_setglobal(m_L, "GetAllPawns");
    lua_pushcfunction(m_L, Lua_WithWorld);         lua_setglobal(m_L, "WithWorld");
    lua_pushcfunction(m_L, Lua_WhenClassSpawns);   lua_setglobal(m_L, "WhenClassSpawns");

    lua_newtable(m_L);
    lua_pushcfunction(m_L, Lua_DirectorSet);               lua_setfield(m_L, -2, "Set");
    lua_pushcfunction(m_L, Lua_DirectorGet);               lua_setfield(m_L, -2, "Get");
    lua_pushcfunction(m_L, Lua_DirectorSlowFor);           lua_setfield(m_L, -2, "SlowFor");
    lua_pushcfunction(m_L, Lua_DirectorToggleBulletTime);  lua_setfield(m_L, -2, "ToggleBulletTime");
    lua_pushcfunction(m_L, Lua_DirectorToggleFreeze);      lua_setfield(m_L, -2, "ToggleFreeze");
    lua_setglobal(m_L, "Director");

    lua_pushcfunction(m_L, Lua_Record);            lua_setglobal(m_L, "Record");
    lua_pushcfunction(m_L, Lua_Watched);           lua_setglobal(m_L, "Watched");
    lua_pushcfunction(m_L, Lua_SetWatchPatterns);  lua_setglobal(m_L, "SetWatchPatterns");
    lua_pushcfunction(m_L, Lua_GetRecords);        lua_setglobal(m_L, "GetRecords");
}

void LuaManager::RegisterUETable() {
    lua_newtable(m_L);

    lua_pushcfunction(m_L, Lua_GetPlayer);
    lua_setfield(m_L, -2, "get_player");

    lua_pushcfunction(m_L, Lua_GetPlayerController);
    lua_setfield(m_L, -2, "get_player_controller");

    lua_pushcfunction(m_L, Lua_GetPlayerCameraManager);
    lua_setfield(m_L, -2, "get_camera_manager");

    lua_pushcfunction(m_L, Lua_GetLocalPlayer);
    lua_setfield(m_L, -2, "get_local_player");

    lua_pushcfunction(m_L, Lua_GetGameInstance);
    lua_setfield(m_L, -2, "get_game_instance");

    lua_pushcfunction(m_L, Lua_GetGameMode);
    lua_setfield(m_L, -2, "get_game_mode");

    lua_pushcfunction(m_L, Lua_GetGameState);
    lua_setfield(m_L, -2, "get_game_state");

    lua_pushcfunction(m_L, Lua_GetAllActorsOfClass);
    lua_setfield(m_L, -2, "get_all_actors");

    lua_pushcfunction(m_L, Lua_GetDistance);
    lua_setfield(m_L, -2, "get_distance");

    lua_pushcfunction(m_L, Lua_GetWorld);
    lua_setfield(m_L, -2, "get_world");

    lua_pushcfunction(m_L, Lua_GetEngine);
    lua_setfield(m_L, -2, "get_engine");

    lua_pushcfunction(m_L, Lua_GetObjectCount);
    lua_setfield(m_L, -2, "get_object_count");

    lua_pushcfunction(m_L, Lua_GetObjectByIndex);
    lua_setfield(m_L, -2, "get_object");

    lua_pushcfunction(m_L, Lua_FindObject);
    lua_setfield(m_L, -2, "find_object");

    lua_pushcfunction(m_L, Lua_FindObjects);
    lua_setfield(m_L, -2, "find_objects");

    lua_pushcfunction(m_L, Lua_FindClass);
    lua_setfield(m_L, -2, "find_class");

    lua_pushcfunction(m_L, Lua_GetWorldActors);
    lua_setfield(m_L, -2, "get_world_actors");

    lua_pushcfunction(m_L, Lua_GetObjectName);
    lua_setfield(m_L, -2, "get_name");

    lua_pushcfunction(m_L, Lua_GetObjectFullName);
    lua_setfield(m_L, -2, "get_full_name");

    lua_pushcfunction(m_L, Lua_GetObjectClass);
    lua_setfield(m_L, -2, "get_class");

    lua_pushcfunction(m_L, Lua_GetSuperClass);
    lua_setfield(m_L, -2, "get_super_class");

    lua_pushcfunction(m_L, Lua_InheritsFrom);
    lua_setfield(m_L, -2, "inherits_from");

    lua_pushcfunction(m_L, Lua_GetClassProperties);
    lua_setfield(m_L, -2, "get_properties");

    lua_pushcfunction(m_L, Lua_CallFunction);
    lua_setfield(m_L, -2, "call");

    lua_pushcfunction(m_L, Lua_ExportProperty);
    lua_setfield(m_L, -2, "export_property");

    lua_pushcfunction(m_L, Lua_ImportProperty);
    lua_setfield(m_L, -2, "import_property");

    lua_pushcfunction(m_L, Lua_GetVersion);
    lua_setfield(m_L, -2, "get_version");

    lua_pushcfunction(m_L, Lua_GetGameName);
    lua_setfield(m_L, -2, "get_game_name");

    lua_pushcfunction(m_L, Lua_GetGlobals);
    lua_setfield(m_L, -2, "get_globals");

    lua_pushcfunction(m_L, Lua_GetGameplayStatics);
    lua_setfield(m_L, -2, "get_gameplay_statics");

    lua_pushcfunction(m_L, Lua_GetKismetSystemLibrary);
    lua_setfield(m_L, -2, "get_kismet_system_library");

    lua_pushcfunction(m_L, Lua_GetKismetMathLibrary);
    lua_setfield(m_L, -2, "get_kismet_math_library");

    lua_pushcfunction(m_L, Lua_GetKismetStringLibrary);
    lua_setfield(m_L, -2, "get_kismet_string_library");

    lua_pushcfunction(m_L, Lua_GetKismetArrayLibrary);
    lua_setfield(m_L, -2, "get_kismet_array_library");

    lua_pushcfunction(m_L, Lua_GetFrameTime);           lua_setfield(m_L, -2, "get_frame_time");
    lua_pushcfunction(m_L, Lua_GetFrameTime);           lua_setfield(m_L, -2, "GetFrameTime");
    lua_pushcfunction(m_L, Lua_GetFPS);                 lua_setfield(m_L, -2, "get_fps");
    lua_pushcfunction(m_L, Lua_GetFPS);                 lua_setfield(m_L, -2, "GetFPS");
    lua_pushcfunction(m_L, Lua_GetDeltaSeconds);        lua_setfield(m_L, -2, "get_delta_seconds");
    lua_pushcfunction(m_L, Lua_GetDeltaSeconds);        lua_setfield(m_L, -2, "GetDeltaSeconds");
    lua_pushcfunction(m_L, Lua_GetMemoryUsage);         lua_setfield(m_L, -2, "get_memory_usage");
    lua_pushcfunction(m_L, Lua_GetMemoryUsage);         lua_setfield(m_L, -2, "GetMemoryUsage");
    lua_pushcfunction(m_L, Lua_ForceGarbageCollection); lua_setfield(m_L, -2, "force_garbage_collection");
    lua_pushcfunction(m_L, Lua_ForceGarbageCollection); lua_setfield(m_L, -2, "ForceGarbageCollection");
    lua_pushcfunction(m_L, Lua_ForceGarbageCollection); lua_setfield(m_L, -2, "collect_garbage");
    lua_pushcfunction(m_L, Lua_ForceGarbageCollection); lua_setfield(m_L, -2, "CollectGarbage");

    lua_setglobal(m_L, "ue");
}

void LuaManager::RegisterMemorySnakeCaseNames(lua_State* L) {
    lua_pushcfunction(L, Lua_ReadInt8); lua_setfield(L, -2, "read_i8");
    lua_pushcfunction(L, Lua_ReadInt16); lua_setfield(L, -2, "read_i16");
    lua_pushcfunction(L, Lua_ReadInt32); lua_setfield(L, -2, "read_i32");
    lua_pushcfunction(L, Lua_ReadInt64); lua_setfield(L, -2, "read_i64");

    lua_pushcfunction(L, Lua_ReadUInt8); lua_setfield(L, -2, "read_u8");
    lua_pushcfunction(L, Lua_ReadUInt16); lua_setfield(L, -2, "read_u16");
    lua_pushcfunction(L, Lua_ReadUInt32); lua_setfield(L, -2, "read_u32");
    lua_pushcfunction(L, Lua_ReadUInt64); lua_setfield(L, -2, "read_u64");

    lua_pushcfunction(L, Lua_ReadFloat); lua_setfield(L, -2, "read_float");
    lua_pushcfunction(L, Lua_ReadDouble); lua_setfield(L, -2, "read_double");
    lua_pushcfunction(L, Lua_ReadPointer); lua_setfield(L, -2, "read_ptr");
    lua_pushcfunction(L, Lua_ReadBool); lua_setfield(L, -2, "read_bool");
    lua_pushcfunction(L, Lua_ReadString); lua_setfield(L, -2, "read_str");
    lua_pushcfunction(L, Lua_ReadFString); lua_setfield(L, -2, "read_fstring");
    lua_pushcfunction(L, Lua_ReadFName); lua_setfield(L, -2, "read_fname");
    lua_pushcfunction(L, Lua_Memory_IsValidPtr); lua_setfield(L, -2, "is_valid_ptr");

    lua_pushcfunction(L, Lua_WriteInt8); lua_setfield(L, -2, "write_i8");
    lua_pushcfunction(L, Lua_WriteInt16); lua_setfield(L, -2, "write_i16");
    lua_pushcfunction(L, Lua_WriteInt32); lua_setfield(L, -2, "write_i32");
    lua_pushcfunction(L, Lua_WriteInt64); lua_setfield(L, -2, "write_i64");

    lua_pushcfunction(L, Lua_WriteUInt8); lua_setfield(L, -2, "write_u8");
    lua_pushcfunction(L, Lua_WriteUInt16); lua_setfield(L, -2, "write_u16");
    lua_pushcfunction(L, Lua_WriteUInt32); lua_setfield(L, -2, "write_u32");
    lua_pushcfunction(L, Lua_WriteUInt64); lua_setfield(L, -2, "write_u64");

    lua_pushcfunction(L, Lua_WriteFloat); lua_setfield(L, -2, "write_float");
    lua_pushcfunction(L, Lua_WriteDouble); lua_setfield(L, -2, "write_double");
    lua_pushcfunction(L, Lua_WritePointer); lua_setfield(L, -2, "write_ptr");
    lua_pushcfunction(L, Lua_WriteBool); lua_setfield(L, -2, "write_bool");
}

void LuaManager::RegisterMemoryBindings() {
    lua_getglobal(m_L, "ue");
    if (!lua_istable(m_L, -1)) {
        lua_pop(m_L, 1);
        lua_newtable(m_L);
        lua_pushvalue(m_L, -1);
        lua_setglobal(m_L, "ue");
    }

    RegisterMemorySnakeCaseNames(m_L);

    lua_pop(m_L, 1);

    lua_newtable(m_L);
    lua_pushcfunction(m_L, Lua_ReadInt8); lua_setfield(m_L, -2, "ReadInt8");
    lua_pushcfunction(m_L, Lua_ReadInt8); lua_setfield(m_L, -2, "ReadByte");
    lua_pushcfunction(m_L, Lua_ReadInt16); lua_setfield(m_L, -2, "ReadInt16");
    lua_pushcfunction(m_L, Lua_ReadInt32); lua_setfield(m_L, -2, "ReadInt32");
    lua_pushcfunction(m_L, Lua_ReadInt64); lua_setfield(m_L, -2, "ReadInt64");
    lua_pushcfunction(m_L, Lua_ReadUInt8); lua_setfield(m_L, -2, "ReadUInt8");
    lua_pushcfunction(m_L, Lua_ReadUInt16); lua_setfield(m_L, -2, "ReadUInt16");
    lua_pushcfunction(m_L, Lua_ReadUInt32); lua_setfield(m_L, -2, "ReadUInt32");
    lua_pushcfunction(m_L, Lua_ReadUInt64); lua_setfield(m_L, -2, "ReadUInt64");
    lua_pushcfunction(m_L, Lua_ReadFloat); lua_setfield(m_L, -2, "ReadFloat");
    lua_pushcfunction(m_L, Lua_ReadDouble); lua_setfield(m_L, -2, "ReadDouble");
    lua_pushcfunction(m_L, Lua_ReadPointer); lua_setfield(m_L, -2, "ReadPointer");
    lua_pushcfunction(m_L, Lua_ReadBool); lua_setfield(m_L, -2, "ReadBool");
    lua_pushcfunction(m_L, Lua_ReadString); lua_setfield(m_L, -2, "ReadString");
    lua_pushcfunction(m_L, Lua_ReadFString); lua_setfield(m_L, -2, "ReadFString");
    lua_pushcfunction(m_L, Lua_ReadFName); lua_setfield(m_L, -2, "ReadFName");

    lua_pushcfunction(m_L, Lua_WriteInt8); lua_setfield(m_L, -2, "WriteInt8");
    lua_pushcfunction(m_L, Lua_WriteInt8); lua_setfield(m_L, -2, "WriteByte");
    lua_pushcfunction(m_L, Lua_WriteInt16); lua_setfield(m_L, -2, "WriteInt16");
    lua_pushcfunction(m_L, Lua_WriteInt32); lua_setfield(m_L, -2, "WriteInt32");
    lua_pushcfunction(m_L, Lua_WriteInt64); lua_setfield(m_L, -2, "WriteInt64");
    lua_pushcfunction(m_L, Lua_WriteUInt8); lua_setfield(m_L, -2, "WriteUInt8");
    lua_pushcfunction(m_L, Lua_WriteUInt16); lua_setfield(m_L, -2, "WriteUInt16");
    lua_pushcfunction(m_L, Lua_WriteUInt32); lua_setfield(m_L, -2, "WriteUInt32");
    lua_pushcfunction(m_L, Lua_WriteUInt64); lua_setfield(m_L, -2, "WriteUInt64");
    lua_pushcfunction(m_L, Lua_WriteFloat); lua_setfield(m_L, -2, "WriteFloat");
    lua_pushcfunction(m_L, Lua_WriteDouble); lua_setfield(m_L, -2, "WriteDouble");
    lua_pushcfunction(m_L, Lua_WritePointer); lua_setfield(m_L, -2, "WritePointer");
    lua_pushcfunction(m_L, Lua_WriteBool); lua_setfield(m_L, -2, "WriteBool");
    lua_pushcfunction(m_L, Lua_Memory_IsValidPtr); lua_setfield(m_L, -2, "IsValidPtr");

    RegisterMemorySnakeCaseNames(m_L);

    lua_setglobal(m_L, "Memory");
}

bool LuaManager::ExecuteString(const std::string& script, std::string& outResult) {
    std::lock_guard<std::recursive_mutex> lock(m_LuaMutex);
    if (!m_L) {
        outResult = "Lua runtime not initialized";
        return false;
    }

    int topBefore = lua_gettop(m_L);
    int status = luaL_loadbuffer(m_L, script.c_str(), script.length(), "chunk");
    if (status != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        outResult = err ? err : "Unknown compilation error";
        lua_pop(m_L, 1);
        return false;
    }

    status = lua_pcall(m_L, 0, LUA_MULTRET, 0);
    if (status != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        outResult = err ? err : "Unknown runtime error";
        lua_pop(m_L, 1);
        return false;
    }

    int numResults = lua_gettop(m_L) - topBefore;
    std::ostringstream ss;
    for (int i = 1; i <= numResults; ++i) {
        int idx = topBefore + i;
        if (i > 1) ss << "\t";
        if (lua_isstring(m_L, idx)) {
            ss << lua_tostring(m_L, idx);
        } else if (lua_isinteger(m_L, idx)) {
            ss << lua_tointeger(m_L, idx);
        } else if (lua_isnumber(m_L, idx)) {
            ss << lua_tonumber(m_L, idx);
        } else if (lua_isboolean(m_L, idx)) {
            ss << (lua_toboolean(m_L, idx) ? "true" : "false");
        } else if (lua_islightuserdata(m_L, idx)) {
            ss << "0x" << std::hex << reinterpret_cast<uintptr_t>(lua_touserdata(m_L, idx));
        } else if (lua_isnil(m_L, idx)) {
            ss << "nil";
        } else {
            ss << lua_typename(m_L, lua_type(m_L, idx));
        }
    }

    if (numResults > 0) {
        lua_pop(m_L, numResults);
        outResult = ss.str();
    } else {
        outResult = "OK";
    }

    return true;
}

bool LuaManager::ExecuteStringJson(const std::string& script, std::string& outResult) {
    std::lock_guard<std::recursive_mutex> lock(m_LuaMutex);
    if (!m_L) {
        outResult = "Lua runtime not initialized";
        return false;
    }

    const int topBefore = lua_gettop(m_L);
    int status = luaL_loadbuffer(m_L, script.c_str(), script.length(), "chunk");
    if (status != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        outResult = err ? err : "Unknown compilation error";
        lua_settop(m_L, topBefore);
        return false;
    }

    status = lua_pcall(m_L, 0, LUA_MULTRET, 0);
    if (status != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        outResult = err ? err : "Unknown runtime error";
        lua_settop(m_L, topBefore);
        return false;
    }

    const int resultCount = lua_gettop(m_L) - topBefore;
    int valueIndex = 0;
    if (resultCount == 0) {
        lua_pushnil(m_L);
        valueIndex = lua_gettop(m_L);
    } else if (resultCount == 1) {
        valueIndex = topBefore + 1;
    } else {
        lua_createtable(m_L, resultCount, 0);
        for (int i = 1; i <= resultCount; ++i) {
            lua_pushvalue(m_L, topBefore + i);
            lua_rawseti(m_L, -2, i);
        }
        valueIndex = lua_gettop(m_L);
    }

    lua_getglobal(m_L, "JSON");
    if (!lua_istable(m_L, -1)) {
        outResult = "JSON library not available";
        lua_settop(m_L, topBefore);
        return false;
    }
    lua_getfield(m_L, -1, "Stringify");
    if (!lua_isfunction(m_L, -1)) {
        outResult = "JSON.Stringify not available";
        lua_settop(m_L, topBefore);
        return false;
    }
    lua_remove(m_L, -2);
    lua_pushvalue(m_L, valueIndex);
    if (lua_pcall(m_L, 1, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        outResult = err ? err : "JSON encoding failed";
        lua_settop(m_L, topBefore);
        return false;
    }

    const char* encoded = lua_tostring(m_L, -1);
    outResult = encoded ? encoded : "null";
    lua_settop(m_L, topBefore);
    return true;
}

bool LuaManager::GetPropertyJson(void* object, const std::string& path, std::string& outResult) {
    std::lock_guard<std::recursive_mutex> lock(m_LuaMutex);
    if (!m_L) {
        outResult = "Lua runtime not initialized";
        return false;
    }
    if (!object) {
        outResult = "object pointer is null";
        return false;
    }
    if (path.empty()) {
        outResult = "property path is empty";
        return false;
    }

    const int topBefore = lua_gettop(m_L);
    LuaEngine::PushUObject(m_L, static_cast<UObject*>(object));
    lua_getfield(m_L, -1, "GetProperty");
    if (!lua_isfunction(m_L, -1)) {
        outResult = "UObject wrapper has no GetProperty";
        lua_settop(m_L, topBefore);
        return false;
    }
    lua_insert(m_L, -2);
    lua_pushlstring(m_L, path.data(), path.size());
    if (lua_pcall(m_L, 2, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        outResult = err ? err : "property read failed";
        lua_settop(m_L, topBefore);
        return false;
    }

    lua_getglobal(m_L, "JSON");
    lua_getfield(m_L, -1, "Stringify");
    lua_remove(m_L, -2);
    lua_pushvalue(m_L, -2);
    if (lua_pcall(m_L, 1, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        outResult = err ? err : "JSON encoding failed";
        lua_settop(m_L, topBefore);
        return false;
    }

    const char* encoded = lua_tostring(m_L, -1);
    outResult = encoded ? encoded : "null";
    lua_settop(m_L, topBefore);
    return true;
}

bool LuaManager::SetPropertyJson(void* object, const std::string& path, const std::string& jsonValue, std::string& outResult) {
    std::lock_guard<std::recursive_mutex> lock(m_LuaMutex);
    if (!m_L) {
        outResult = "Lua runtime not initialized";
        return false;
    }
    if (!object) {
        outResult = "object pointer is null";
        return false;
    }
    if (path.empty()) {
        outResult = "property path is empty";
        return false;
    }

    const int topBefore = lua_gettop(m_L);

    lua_getglobal(m_L, "JSON");
    lua_getfield(m_L, -1, "Parse");
    lua_remove(m_L, -2);
    lua_pushlstring(m_L, jsonValue.data(), jsonValue.size());
    if (lua_pcall(m_L, 1, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        outResult = err ? err : "invalid JSON value";
        lua_settop(m_L, topBefore);
        return false;
    }
    if (lua_isnil(m_L, -1)) {
        std::string trimmed = jsonValue;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
        while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t' ||
                                    trimmed.back() == '\r' || trimmed.back() == '\n')) {
            trimmed.pop_back();
        }
        if (trimmed != "null") {
            outResult = "invalid JSON value";
            lua_settop(m_L, topBefore);
            return false;
        }
    }
    const int valueIndex = lua_gettop(m_L);

    LuaEngine::PushUObject(m_L, static_cast<UObject*>(object));
    lua_getfield(m_L, -1, "SetProperty");
    if (!lua_isfunction(m_L, -1)) {
        outResult = "UObject wrapper has no SetProperty";
        lua_settop(m_L, topBefore);
        return false;
    }
    lua_insert(m_L, -2);
    lua_pushlstring(m_L, path.data(), path.size());
    lua_pushvalue(m_L, valueIndex);
    if (lua_pcall(m_L, 3, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        outResult = err ? err : "property write failed";
        lua_settop(m_L, topBefore);
        return false;
    }

    const bool ok = lua_toboolean(m_L, -1) != 0;
    outResult = ok ? "OK" : "property not found or not writable";
    lua_settop(m_L, topBefore);
    return ok;
}

int LuaManager::SubscribeIpcEvent(const std::string& kind, const std::string& filter, uint64_t token) {
    std::lock_guard<std::recursive_mutex> lock(m_LuaMutex);
    if (!m_L) return 0;
    if (kind == "ProcessEvent" && filter.empty()) return 0;

    lua_pushlstring(m_L, kind.data(), kind.size());
    lua_pushlstring(m_L, filter.data(), filter.size());
    lua_pushinteger(m_L, static_cast<lua_Integer>(token));
    lua_pushcclosure(m_L, Lua_IpcEventCallback, 3);
    const int callbackRef = luaL_ref(m_L, LUA_REGISTRYINDEX);

    uint32_t hookId = 0;
    if (kind == "ProcessEvent") {
        hookId = LuaEngine::LuaHookManager::Get().RegisterHook(filter, callbackRef, LUA_NOREF);
    } else if (kind == "BeginPlay") {
        hookId = LuaEngine::LuaHookManager::Get().RegisterBeginPlayHook(callbackRef, LUA_NOREF);
    } else if (kind == "EndPlay") {
        hookId = LuaEngine::LuaHookManager::Get().RegisterEndPlayHook(callbackRef, LUA_NOREF);
    } else if (kind == "LoadMap") {
        hookId = LuaEngine::LuaHookManager::Get().RegisterLoadMapHook(callbackRef, LUA_NOREF);
    }

    if (!hookId) {
        luaL_unref(m_L, LUA_REGISTRYINDEX, callbackRef);
        return 0;
    }

    {
        std::lock_guard<std::mutex> tokenLock(g_IpcTokenMutex);
        g_IpcTokens.insert(token);
    }

    const int id = m_NextIpcSubscription++;
    m_IpcSubscriptions[id] = IpcSubscription{hookId, callbackRef, kind, filter, token};
    return id;
}

bool LuaManager::UnsubscribeIpcEvent(int subscriptionId) {
    std::lock_guard<std::recursive_mutex> lock(m_LuaMutex);
    auto it = m_IpcSubscriptions.find(subscriptionId);
    if (it == m_IpcSubscriptions.end()) return false;

    LuaEngine::LuaHookManager::Get().UnregisterHook(it->second.hookId);
    {
        std::lock_guard<std::mutex> tokenLock(g_IpcTokenMutex);
        g_IpcTokens.erase(it->second.token);
    }
    if (m_L) {
        luaL_unref(m_L, LUA_REGISTRYINDEX, it->second.callbackRef);
    }
    m_IpcSubscriptions.erase(it);
    return true;
}

bool LuaManager::ExecuteFile(const std::string& filePath, std::string& outResult) {
    std::lock_guard<std::recursive_mutex> lock(m_LuaMutex);
    if (!m_L) {
        outResult = "Lua runtime not initialized";
        return false;
    }

    int status = luaL_dofile(m_L, filePath.c_str());
    if (status != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        outResult = err ? err : "Unknown execution error";
        lua_pop(m_L, 1);
        return false;
    }

    outResult = "OK";
    return true;
}

static std::string SkipJsonWhitespace(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
    return {};
}

static std::string ParseJsonString(const std::string& s, size_t& i) {
    if (i >= s.size() || s[i] != '"') return {};
    ++i;
    std::string out;
    while (i < s.size() && s[i] != '"') {
        if (s[i] == '\\' && i + 1 < s.size()) { ++i; }
        out += s[i++];
    }
    if (i < s.size()) ++i;
    return out;
}

static bool ParseModsJson(const std::string& filePath,
                          std::unordered_map<std::string, bool>& enabledMap,
                          std::vector<std::string>& orderedNames)
{
    std::ifstream f(filePath);
    if (!f.is_open()) return false;
    std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    size_t i = 0;

    auto findKey = [&](const std::string& key) -> bool {
        size_t pos = src.find("\"" + key + "\"", i);
        if (pos == std::string::npos) return false;
        i = pos + key.size() + 2;

        while (i < src.size() && (src[i] == ' ' || src[i] == ':' || src[i] == '\t')) ++i;
        return true;
    };

    if (!findKey("mods")) return false;
    if (i >= src.size() || src[i] != '[') return false;
    ++i;

    while (i < src.size()) {
        SkipJsonWhitespace(src, i);
        if (i >= src.size() || src[i] == ']') break;
        if (src[i] == '{') {
            ++i;
            std::string name;
            bool enabled = true;

            while (i < src.size() && src[i] != '}') {
                SkipJsonWhitespace(src, i);
                if (src[i] != '"') { ++i; continue; }
                std::string key = ParseJsonString(src, i);
                SkipJsonWhitespace(src, i);
                if (i < src.size() && src[i] == ':') ++i;
                SkipJsonWhitespace(src, i);

                if (key == "name") {
                    name = ParseJsonString(src, i);
                } else if (key == "enabled") {

                    if (i + 4 <= src.size() && src.substr(i, 4) == "true")  { enabled = true;  i += 4; }
                    else if (i + 5 <= src.size() && src.substr(i, 5) == "false") { enabled = false; i += 5; }
                    else if (i < src.size() && src[i] == '1') { enabled = true;  ++i; }
                    else if (i < src.size() && src[i] == '0') { enabled = false; ++i; }
                    else ++i;
                } else {

                    while (i < src.size() && src[i] != ',' && src[i] != '}') ++i;
                }
                SkipJsonWhitespace(src, i);
                if (i < src.size() && src[i] == ',') ++i;
            }
            if (i < src.size()) ++i;

            if (!name.empty()) {
                enabledMap[name] = enabled;
                orderedNames.push_back(name);
            }
        }
        SkipJsonWhitespace(src, i);
        if (i < src.size() && src[i] == ',') ++i;
    }
    return true;
}

static int Lua_ModRequire(lua_State* L);

void LuaManager::LoadModsFromDirectory(const std::string& modsDirectory) {
    if (!m_Initialized || modsDirectory.empty()) return;

    struct ModLoadingFlag {
        std::atomic<bool>& flag;
        explicit ModLoadingFlag(std::atomic<bool>& f) : flag(f) {
            flag.store(true, std::memory_order_release);
        }
        ~ModLoadingFlag() { flag.store(false, std::memory_order_release); }
    } loadingFlag(m_LoadingMods);

    std::lock_guard<std::recursive_mutex> luaLock(m_LuaMutex);

    m_ModsDirectory = modsDirectory;
    m_LoadedMods.clear();

    std::error_code ec;
    if (!std::filesystem::exists(modsDirectory, ec) || !std::filesystem::is_directory(modsDirectory, ec)) {
        char errBuf[512] = {};
        snprintf(errBuf, sizeof(errBuf), "[Lua] Mods directory does not exist: %s\n", modsDirectory.c_str());
        EngineDiscovery::WriteLogLine(errBuf);
        return;
    }

    std::string gameName = (g_State.GameName[0] ? g_State.GameName : "Default");
    std::filesystem::path sharedTypesFile = std::filesystem::path(modsDirectory) / "shared" / "types" / gameName / "AvaloniumTypes.lua";
    if (!std::filesystem::exists(sharedTypesFile, ec) && g_State.GUObjectArray) {
        std::filesystem::path sharedTypesDir = std::filesystem::path(modsDirectory) / "shared" / "types" / gameName;
        LuaEngine::LuaTypeGenerator::GenerateLuaTypes(sharedTypesDir.string());
    }

    std::unordered_map<std::string, bool> enabledMap;
    std::vector<std::string>              orderedNames;
    bool hasModList = false;
    bool useJsonOrder = false;
    const char* listSource = "mods.json";

    std::filesystem::path modsJsonPath = std::filesystem::path(modsDirectory) / "mods.json";
    std::filesystem::path modsTxtPath  = std::filesystem::path(modsDirectory) / "mods.txt";

    if (std::filesystem::exists(modsJsonPath, ec)) {
        hasModList = ParseModsJson(modsJsonPath.string(), enabledMap, orderedNames);
        useJsonOrder = hasModList;
        listSource = "mods.json";
        if (hasModList) {
            char logBuf[512] = {};
            snprintf(logBuf, sizeof(logBuf), "[Lua] Loaded mod list from mods.json (%zu entries)\n", orderedNames.size());
            EngineDiscovery::WriteLogLine(logBuf);
        }
    }

    if (!hasModList && std::filesystem::exists(modsTxtPath, ec)) {

        std::ifstream in(modsTxtPath);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();

            if (!line.empty() && (line[0] == ';' || line[0] == '#')) continue;
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::string modName = line.substr(0, colon);
                std::string valStr  = line.substr(colon + 1);
                modName.erase(modName.begin(), std::find_if(modName.begin(), modName.end(), [](unsigned char ch) { return !std::isspace(ch); }));
                modName.erase(std::find_if(modName.rbegin(), modName.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), modName.end());
                valStr.erase(valStr.begin(), std::find_if(valStr.begin(), valStr.end(), [](unsigned char ch) { return !std::isspace(ch); }));
                valStr.erase(std::find_if(valStr.rbegin(), valStr.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), valStr.end());
                bool isEnabled = (valStr == "1" || _stricmp(valStr.c_str(), "true") == 0);
                enabledMap[modName] = isEnabled;
                orderedNames.push_back(modName);
                hasModList = true;
            }
        }
        if (hasModList) {
            listSource = "mods.txt";
            char logBuf[512] = {};
            snprintf(logBuf, sizeof(logBuf), "[Lua] Loaded mod list from mods.txt (legacy format, %zu entries)\n", orderedNames.size());
            EngineDiscovery::WriteLogLine(logBuf);
        }
    }

    std::vector<std::filesystem::path> scripts;
    std::vector<std::string> disabledMods;
    std::vector<std::string> missingMods;
    std::unordered_set<std::string> processedModNames;

    auto findMainScript = [&](const std::filesystem::path& modDir) -> std::filesystem::path {
        if (!std::filesystem::is_directory(modDir, ec)) return {};
        std::filesystem::path mainScript = modDir / "scripts" / "main.lua";
        if (std::filesystem::exists(mainScript, ec)) return mainScript;
        mainScript = modDir / "main.lua";
        if (std::filesystem::exists(mainScript, ec)) return mainScript;
        return {};
    };

    if (useJsonOrder) {

        for (const auto& name : orderedNames) {
            processedModNames.insert(name);
            std::filesystem::path modDir = std::filesystem::path(modsDirectory) / name;
            bool dirExists = std::filesystem::is_directory(modDir, ec);

            auto it = enabledMap.find(name);
            bool isEnabled = (it != enabledMap.end()) ? it->second : true;

            if (!isEnabled) {
                disabledMods.push_back(name);
                char logBuf[512] = {};
                if (dirExists) {
                    snprintf(logBuf, sizeof(logBuf), "[Lua] Mod '%s' is disabled in %s (skipped)\n", name.c_str(), listSource);
                } else {
                    snprintf(logBuf, sizeof(logBuf), "[Lua] Mod '%s' is disabled in %s (folder not on disk, skipped)\n", name.c_str(), listSource);
                }
                EngineDiscovery::WriteLogLine(logBuf);
                continue;
            }

            if (!dirExists) {
                missingMods.push_back(name);
                char logBuf[512] = {};
                snprintf(logBuf, sizeof(logBuf), "[Lua] Mod '%s' is enabled in %s, but folder was not found on disk (skipped)\n", name.c_str(), listSource);
                EngineDiscovery::WriteLogLine(logBuf);
                continue;
            }

            std::filesystem::path script = findMainScript(modDir);
            if (!script.empty()) {
                scripts.push_back(script);
            } else {
                char logBuf[512] = {};
                snprintf(logBuf, sizeof(logBuf), "[Lua] Mod '%s' folder exists, but neither 'scripts/main.lua' nor 'main.lua' was found (skipped)\n", name.c_str());
                EngineDiscovery::WriteLogLine(logBuf);
            }
        }

        for (const auto& entry : std::filesystem::directory_iterator(modsDirectory, ec)) {
            if (!entry.is_directory()) {
                if (entry.is_regular_file() && entry.path().extension() == ".lua") {
                    std::string fileName = entry.path().filename().string();
                    if (processedModNames.find(fileName) == processedModNames.end()) {
                        scripts.push_back(entry.path());
                    }
                }
                continue;
            }
            std::string dirName = entry.path().filename().string();
            if (dirName == "shared" || dirName == "Cache") continue;
            if (processedModNames.find(dirName) != processedModNames.end()) continue;

            std::filesystem::path script = findMainScript(entry.path());
            if (!script.empty()) {
                scripts.push_back(script);
                char logBuf[512] = {};
                snprintf(logBuf, sizeof(logBuf), "[Lua] Found unlisted mod '%s' on disk (auto-enabled)\n", dirName.c_str());
                EngineDiscovery::WriteLogLine(logBuf);
            }
        }
    } else {

        std::vector<std::filesystem::path> discoveredDirs;
        for (const auto& entry : std::filesystem::directory_iterator(modsDirectory, ec)) {
            if (!entry.is_directory()) {
                if (entry.is_regular_file() && entry.path().extension() == ".lua")
                    scripts.push_back(entry.path());
                continue;
            }
            std::string dirName = entry.path().filename().string();
            if (dirName == "shared" || dirName == "Cache") continue;

            if (hasModList) {
                auto it = enabledMap.find(dirName);
                if (it != enabledMap.end() && !it->second) {
                    disabledMods.push_back(dirName);
                    char logBuf[512] = {};
                    snprintf(logBuf, sizeof(logBuf), "[Lua] Mod '%s' is disabled in %s (skipped)\n", dirName.c_str(), listSource);
                    EngineDiscovery::WriteLogLine(logBuf);
                    continue;
                }
            }
            discoveredDirs.push_back(entry.path());
        }
        std::sort(discoveredDirs.begin(), discoveredDirs.end());
        for (const auto& dir : discoveredDirs) {
            std::filesystem::path script = findMainScript(dir);
            if (!script.empty()) {
                scripts.push_back(script);
            }
        }
    }

    char summaryBuf[512] = {};
    if (!disabledMods.empty()) {
        snprintf(summaryBuf, sizeof(summaryBuf), "[Lua] Found %zu active mod(s) (%zu disabled in %s) in %s\n",
                 scripts.size(), disabledMods.size(), listSource, modsDirectory.c_str());
        EngineDiscovery::WriteLogLine(summaryBuf);

        if (scripts.empty()) {
            char hintBuf[512] = {};
            snprintf(hintBuf, sizeof(hintBuf), "[Lua] Info: All %zu detected mod(s) are currently disabled in %s. To enable a mod, set \"enabled\": true in %s or toggle it ON in Universal Mananger.\n",
                     disabledMods.size(), listSource, listSource);
            EngineDiscovery::WriteLogLine(hintBuf);
        }
    } else {
        snprintf(summaryBuf, sizeof(summaryBuf), "[Lua] Found %zu active mod(s) in %s\n",
                 scripts.size(), modsDirectory.c_str());
        EngineDiscovery::WriteLogLine(summaryBuf);
    }

    for (const auto& scriptPath : scripts) {
        char logBuf[512] = {};
        std::string modIdentifier = scriptPath.parent_path().filename().string();
        if (modIdentifier == "scripts") {
            modIdentifier = scriptPath.parent_path().parent_path().filename().string();
        } else if (modIdentifier == "Mods") {
            modIdentifier = scriptPath.filename().string();
        }

        lua_newtable(m_L);
        lua_newtable(m_L);
        lua_getglobal(m_L, "_G");
        lua_setfield(m_L, -2, "__index");
        lua_setmetatable(m_L, -2);

        {
            std::filesystem::path modRoot = scriptPath.parent_path();
            const std::string leaf = modRoot.filename().string();
            if (leaf == "scripts" || leaf == "Scripts") {
                modRoot = modRoot.parent_path();
            }
            std::string modRootStr = modRoot.string();
            std::replace(modRootStr.begin(), modRootStr.end(), '\\', '/');
            lua_pushstring(m_L, modRootStr.c_str());
            lua_pushvalue(m_L, -2);
            lua_pushcclosure(m_L, Lua_ModRequire, 2);
            lua_setfield(m_L, -2, "require");
        }

        int status = luaL_loadfile(m_L, scriptPath.string().c_str());
        if (status == LUA_OK) {

            lua_pushvalue(m_L, -2);
            lua_setupvalue(m_L, -2, 1);

            status = lua_pcall(m_L, 0, 0, 0);
            if (status == LUA_OK) {
                m_LoadedMods.push_back(modIdentifier);
                snprintf(logBuf, sizeof(logBuf), "[Lua] Loaded mod: %s (%s)\n", modIdentifier.c_str(), scriptPath.filename().string().c_str());
                EngineDiscovery::WriteLogLine(logBuf);
            } else {
                const char* err = lua_tostring(m_L, -1);
                snprintf(logBuf, sizeof(logBuf), "[Lua] Error running mod %s: %s\n", scriptPath.string().c_str(), err ? err : "Unknown");
                EngineDiscovery::WriteLogLine(logBuf);
                lua_pop(m_L, 1);
            }
        } else {
            const char* err = lua_tostring(m_L, -1);
            snprintf(logBuf, sizeof(logBuf), "[Lua] Failed to compile mod %s: %s\n", scriptPath.string().c_str(), err ? err : "Unknown");
            EngineDiscovery::WriteLogLine(logBuf);
            lua_pop(m_L, 1);
        }
        lua_pop(m_L, 1);
    }
}

static bool ModModulePushCached(lua_State* L, const char* key) {
    lua_getglobal(L, "package");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return false; }
    lua_getfield(L, -1, "loaded");
    if (!lua_istable(L, -1)) { lua_pop(L, 2); return false; }
    lua_getfield(L, -1, key);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 3);
        return false;
    }
    lua_remove(L, -2);
    lua_remove(L, -2);
    return true;
}

static void ModModuleRecordLoaded(lua_State* L, const char* key, int valueIdx, bool hasValue) {
    lua_getglobal(L, "package");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }
    lua_getfield(L, -1, "loaded");
    if (!lua_istable(L, -1)) { lua_pop(L, 2); return; }
    lua_pushstring(L, key);
    if (hasValue) lua_pushvalue(L, valueIdx);
    else lua_pushboolean(L, 1);
    lua_rawset(L, -3);
    lua_pop(L, 2);
}

static int Lua_ModRequire(lua_State* L) {
    if (!lua_isstring(L, 1)) return luaL_error(L, "module name expected");
    const char* name = lua_tostring(L, 1);
    const char* root = lua_tostring(L, lua_upvalueindex(1));

    {
        std::string relative = name ? name : "";
        for (char& c : relative) {
            if (c == '.') c = '/';
        }

        if (root && *root && !relative.empty()) {
            const char* subDirs[] = { "/scripts", "/Scripts", "" };
            const char* suffixes[] = { ".lua", "/init.lua", "/main.lua" };
            for (const char* sub : subDirs) {
                for (const char* suffix : suffixes) {
                    const std::string path = std::string(root) + sub + "/" + relative + suffix;
                    std::error_code ec;
                    if (!std::filesystem::exists(path, ec)) continue;

                    std::string pathKey = "@" + path;
                    for (char& c : pathKey) {
                        if (c == '\\') c = '/';
                    }
                    if (ModModulePushCached(L, pathKey.c_str())) return 1;

                    int status = luaL_loadfile(L, path.c_str());
                    if (status != LUA_OK) {
                        const char* err = lua_tostring(L, -1);
                        EngineDiscovery::WriteLogLine(("[Lua] require('" + std::string(name) +
                            "'): failed to compile " + path + ": " + (err ? err : "unknown") + "\n").c_str());
                        lua_pop(L, 1);
                        continue;
                    }

                    lua_pushvalue(L, lua_upvalueindex(2));
                    lua_setupvalue(L, -2, 1);

                    if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
                        const char* err = lua_tostring(L, -1);
                        EngineDiscovery::WriteLogLine(("[Lua] require('" + std::string(name) +
                            "'): error running " + path + ": " + (err ? err : "unknown") + "\n").c_str());
                        lua_pop(L, 1);
                        continue;
                    }

                    const bool hasValue = !lua_isnil(L, -1);
                    ModModuleRecordLoaded(L, pathKey.c_str(), lua_gettop(L), hasValue);
                    if (!hasValue) {
                        lua_pop(L, 1);
                        lua_pushboolean(L, 1);
                    }
                    return 1;
                }
            }
        }
    }

    const int nargs = lua_gettop(L);
    lua_getglobal(L, "require");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        lua_pushnil(L);
        return 1;
    }
    lua_insert(L, 1);
    if (lua_pcall(L, nargs, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        EngineDiscovery::WriteLogLine(("[Lua] require('" + std::string(name) +
            "'): not found in the mod's tree or the standard search path: " +
            (err ? err : "unknown") + "\n").c_str());
        return lua_error(L);
    }
    return 1;
}

int LuaManager::Lua_ReloadMods(lua_State* L) {
    LuaManager::Get().ReloadMods();
    return 0;
}

void LuaManager::ReloadMods() {
    std::lock_guard<std::recursive_mutex> lock(m_LuaMutex);
    if (!m_Initialized || !m_L) {
        return;
    }
    char logBuf[256];
    snprintf(logBuf, sizeof(logBuf), "[Lua] === Hot-reloading all active mods ===\n");
    EngineDiscovery::WriteLogLine(logBuf);

    LuaEngine::LuaHookManager::Get().ClearAllHooks();
    LuaEngine::LuaHookManager::Get().ClearAllTimers();
    LuaEngine::Overlay::LuaOverlayManager::Get().ClearAll(m_L);
    m_LoadedMods.clear();

    LoadModsFromDirectory(m_ModsDirectory);
}

int LuaManager::Lua_FindFirstOf(lua_State* L) {
    const char* className = luaL_checkstring(L, 1);
    if (!className) {
        lua_pushnil(L);
        return 1;
    }

    if (_stricmp(className, "PlayerController") == 0 || _stricmp(className, "APlayerController") == 0) {
        UObject* pc = ConsoleEnabler::FindPlayerController(g_State);
        if (pc) {
            LuaEngine::PushUObject(L, pc);
            return 1;
        }
    }

    if (_stricmp(className, "Character") == 0 || _stricmp(className, "ACharacter") == 0 ||
        _stricmp(className, "Pawn") == 0 || _stricmp(className, "APawn") == 0) {
        UObject* pc = ConsoleEnabler::FindPlayerController(g_State);
        if (pc) {
            UClass* pcClass = EngineDiscovery::GetObjectClass(g_State, pc);
            if (pcClass) {
                int32_t pawnOff = ConsoleEnabler::FindPropertyOffset(g_State, static_cast<UStruct*>(pcClass), "AcknowledgedPawn");
                if (pawnOff < 0) pawnOff = ConsoleEnabler::FindPropertyOffset(g_State, static_cast<UStruct*>(pcClass), "Pawn");
                if (pawnOff < 0) pawnOff = ConsoleEnabler::FindPropertyOffset(g_State, static_cast<UStruct*>(pcClass), "Character");
                if (pawnOff >= 0) {
                    UObject* pawn = static_cast<UObject*>(SafeMem_ReadPtr(reinterpret_cast<uint8_t*>(pc) + pawnOff));
                    if (pawn) {
                        std::string pName = EngineDiscovery::GetObjectNameStr(g_State, pawn);
                        if (pName.find("Default__") == std::string::npos) {
                            LuaEngine::PushUObject(L, pawn);
                            return 1;
                        }
                    }
                }
            }
        }
    }

    std::vector<void*> actors(10000);
    int count = EngineDiscovery::GetWorldActors(g_State, actors.data(), static_cast<int32_t>(actors.size()));
    for (int i = 0; i < count; ++i) {
        UObject* act = static_cast<UObject*>(actors[i]);
        if (!act) continue;

        std::string oName = EngineDiscovery::GetObjectNameStr(g_State, act);
        if (oName.find("Default__") != std::string::npos) continue;

        UClass* c = EngineDiscovery::GetObjectClass(g_State, act);
        if (!c) continue;

        UStruct* cur = static_cast<UStruct*>(c);
        for (int depth = 0; depth < 32 && cur; ++depth) {
            std::string cName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cur));
            if (_stricmp(cName.c_str(), className) == 0 || cName.find(className) != std::string::npos) {
                LuaEngine::PushUObject(L, act);
                return 1;
            }
            cur = EngineDiscovery::GetSuperStruct(g_State, cur);
        }
    }

    UClass* cls = EngineDiscovery::FindObjectByNameFast(g_State, className);
    if (cls && g_State.bIndexBuilt) {
        auto instances = EngineDiscovery::FindObjectsByClassFast(g_State, cls);
        for (UObject* inst : instances) {
            if (!inst) continue;
            std::string oName = EngineDiscovery::GetObjectNameStr(g_State, inst);
            if (oName.find("Default__") == std::string::npos) {
                LuaEngine::PushUObject(L, inst);
                return 1;
            }
        }
    }

    if (g_State.GUObjectArray) {
        int total = EngineDiscovery::GetObjectCount(g_State);
        for (int i = 0; i < total; ++i) {
            void* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
            if (!obj) continue;

            std::string oName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(obj));
            if (oName.find("Default__") != std::string::npos) continue;

            UClass* c = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj));
            if (!c) continue;

            UStruct* cur = static_cast<UStruct*>(c);
            for (int depth = 0; depth < 32 && cur; ++depth) {
                std::string cName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cur));
                if (_stricmp(cName.c_str(), className) == 0 || cName.find(className) != std::string::npos) {
                    LuaEngine::PushUObject(L, static_cast<UObject*>(obj));
                    return 1;
                }
                cur = EngineDiscovery::GetSuperStruct(g_State, cur);
            }
        }
    }

    lua_pushnil(L);
    return 1;
}

int LuaManager::Lua_FindAllOf(lua_State* L) {
    FastMemScope fastMem;
    const char* className = luaL_checkstring(L, 1);

    const int arrayIdx = lua_gettop(L) + 1;
    lua_newtable(L);
    if (!className) return 1;

    UClass* targetClass = ConsoleEnabler::FindClassByNameExact(g_State, className);
    if (!targetClass) {

        std::string withC = std::string(className) + "_C";
        targetClass = ConsoleEnabler::FindClassByNameExact(g_State, withC.c_str());
    }

    bool isGenericActor = false;
    if (_stricmp(className, "Actor") == 0 || _stricmp(className, "AActor") == 0 || targetClass == g_State.AActorClass) {
        isGenericActor = true;
    } else if (!targetClass) {
        targetClass = EngineDiscovery::FindClassCached(g_State, className);
        if (!targetClass) {
            std::string withC = std::string(className) + "_C";
            targetClass = EngineDiscovery::FindClassCached(g_State, withC);
        }
        if (!targetClass && (className[0] == 'A' || className[0] == 'U')) {
            targetClass = EngineDiscovery::FindClassCached(g_State, className + 1);
        }
        if (!targetClass) {
            return 1;
        }
    }

    int tableIdx = 1;

    static thread_local std::vector<void*> s_FindAllBuf(16384);
    int count = EngineDiscovery::GetWorldActors(g_State, s_FindAllBuf.data(), static_cast<int32_t>(s_FindAllBuf.size()));

    for (int i = 0; i < count; ++i) {
        UObject* act = static_cast<UObject*>(s_FindAllBuf[i]);
        if (!act || !SafeMem_IsReadable(act, 32)) continue;

        if (isGenericActor || ConsoleEnabler::IsObjectA(g_State, act, targetClass)) {
            LuaEngine::PushUObject(L, act);
            lua_rawseti(L, arrayIdx, tableIdx++);
        }
    }

    if (tableIdx == 1 && targetClass) {
        auto instances = EngineDiscovery::FindObjectsByClassFast(g_State, targetClass);
        for (UObject* inst : instances) {
            if (!inst || !SafeMem_IsReadable(inst, 32)) continue;

            uint32_t flags = 0;
            SafeMem_Read((uint8_t*)inst + g_State.UObject_FlagsOff, &flags, sizeof(flags));
            if (flags & 0x00000010) continue;
            LuaEngine::PushUObject(L, inst);
            lua_rawseti(L, arrayIdx, tableIdx++);
        }
    }

    return 1;
}

int LuaManager::Lua_StaticFindObject(lua_State* L) {

    if (lua_type(L, 1) == LUA_TSTRING) {
        const char* path = lua_tostring(L, 1);
        if (!path) {
            lua_pushnil(L);
            return 1;
        }

        UObject* obj = EngineDiscovery::ResolveObjectPath(g_State, path);
        if (!obj) {
            obj = EngineDiscovery::FindObjectByNameFast(g_State, path);
        }
        if (!obj && g_State.bIndexBuilt) {
            auto list = EngineDiscovery::FindObjectsByPrefix(g_State, path);
            if (!list.empty()) obj = list.front();
        }

        LuaEngine::PushUObject(L, obj);
        return 1;
    }

    UObject* classArg = LuaEngine::GetUObject(L, 1);
    UObject* outerArg = (lua_gettop(L) >= 2) ? LuaEngine::GetUObject(L, 2) : nullptr;
    const char* name = (lua_gettop(L) >= 3) ? lua_tostring(L, 3) : nullptr;

    if (!name) {
        lua_pushnil(L);
        return 1;
    }

    const bool exactClass = (lua_gettop(L) >= 4) && lua_isboolean(L, 4)
                                ? lua_toboolean(L, 4) != 0
                                : false;

    UClass* requiredClass = nullptr;
    if (classArg && EngineDiscovery::GetObjectClass(g_State, classArg) ==
                        reinterpret_cast<UClass*>(g_State.UClassClass))
        requiredClass = reinterpret_cast<UClass*>(classArg);

    UObject* obj = EngineDiscovery::ResolveObjectPath(g_State, name, requiredClass,
                                                     exactClass, outerArg);

    LuaEngine::PushUObject(L, obj);
    return 1;
}

int LuaManager::Lua_StaticConstructObject(lua_State* L) {
    UObject* clsObj = LuaEngine::GetUObject(L, 1);
    UObject* outerObj = (lua_gettop(L) >= 2) ? LuaEngine::GetUObject(L, 2) : nullptr;

    if (!clsObj) {
        lua_pushnil(L);
        return 1;
    }

    UClass* cls = reinterpret_cast<UClass*>(clsObj);
    UObject* outer = outerObj ? outerObj : static_cast<UObject*>(g_State.GWorld ? g_State.GWorld : (g_State.GWorldPtr ? *g_State.GWorldPtr : nullptr));

    UObject* created = nullptr;
    if (outer) {
        created = EngineDiscovery::SpawnObjectThroughEngine(g_State, cls, outer);
    }

    if (!created && g_State.StaticConstructObject) {
        created = ConsoleEnabler::TrySEHConstruct(g_State, cls, outer);
    }

    if (created) {
        LuaEngine::PushUObject(L, created);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_Print(lua_State* L) {
    int n = lua_gettop(L);
    std::ostringstream ss;
    ss << "[Lua] ";
    for (int i = 1; i <= n; i++) {
        if (i > 1) ss << "\t";
        if (lua_isstring(L, i)) {
            ss << lua_tostring(L, i);
        } else if (lua_isinteger(L, i)) {
            ss << lua_tointeger(L, i);
        } else if (lua_isnumber(L, i)) {
            ss << lua_tonumber(L, i);
        } else if (lua_isboolean(L, i)) {
            ss << (lua_toboolean(L, i) ? "true" : "false");
        } else if (lua_islightuserdata(L, i)) {
            ss << "0x" << std::hex << reinterpret_cast<uintptr_t>(lua_touserdata(L, i));
        } else if (lua_isnil(L, i)) {
            ss << "nil";
        } else {
            ss << lua_typename(L, lua_type(L, i));
        }
    }
    std::string msg = ss.str();

    EngineDiscovery::WriteLogLineEx(LogSession::ClassifyMessage(msg), "%s",
                                    msg.c_str());
    return 0;
}

int LuaManager::Lua_Log(lua_State* L) {
    return Lua_Print(L);
}

int LuaManager::Lua_GetPlayer(lua_State* L) {
    UObject* pc = ConsoleEnabler::FindPlayerController(g_State);
    if (pc && SafeMem_IsReadable(pc, 32)) {
        UClass* pcClass = EngineDiscovery::GetObjectClass(g_State, pc);
        if (pcClass) {
            const char* candidateProps[] = { "AcknowledgedPawn", "Pawn", "Character", "TargetView", "ControlledPawn" };
            for (const char* propName : candidateProps) {
                int32_t off = ConsoleEnabler::FindPropertyOffset(g_State, reinterpret_cast<UStruct*>(pcClass), propName);
                if (off >= 0) {
                    UObject* pawn = reinterpret_cast<UObject*>(SafeMem_ReadPtr(reinterpret_cast<uint8_t*>(pc) + off));
                    if (pawn && SafeMem_IsReadable(pawn, 32)) {
                        std::string pName = EngineDiscovery::GetObjectNameStr(g_State, pawn);
                        if (pName.find("Default__") == std::string::npos) {
                            PushPointerToLua(L, pawn);
                            return 1;
                        }
                    }
                }
            }
        }
    }

    std::vector<void*> actors(4096);
    int32_t actCount = EngineDiscovery::GetWorldActors(g_State, actors.data(), static_cast<int32_t>(actors.size()));
    if (actCount > 0) {
        for (int32_t i = 0; i < actCount; ++i) {
            UObject* act = static_cast<UObject*>(actors[i]);
            if (!act || !SafeMem_IsReadable(act, 32)) continue;

            UClass* actCls = EngineDiscovery::GetObjectClass(g_State, act);
            if (!actCls) continue;

            std::string actClsName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(actCls));
            std::string actName = EngineDiscovery::GetObjectNameStr(g_State, act);
            if (actName.find("Default__") != std::string::npos) continue;

            if (ClassInheritsFrom(g_State, actCls, "Character") || ClassInheritsFrom(g_State, actCls, "Pawn")) {
                int32_t ctrlOff = ConsoleEnabler::FindPropertyOffset(g_State, reinterpret_cast<UStruct*>(actCls), "Controller");
                if (ctrlOff >= 0) {
                    UObject* ctrl = reinterpret_cast<UObject*>(SafeMem_ReadPtr(reinterpret_cast<uint8_t*>(act) + ctrlOff));
                    if (ctrl && SafeMem_IsReadable(ctrl, 32)) {
                        if (ctrl == pc) {
                            PushPointerToLua(L, act);
                            return 1;
                        }
                        UClass* ctrlCls = EngineDiscovery::GetObjectClass(g_State, ctrl);
                        if (ctrlCls && ClassInheritsFrom(g_State, ctrlCls, "PlayerController")) {
                            PushPointerToLua(L, act);
                            return 1;
                        }
                    }
                }
                int32_t inputOff = ConsoleEnabler::FindPropertyOffset(g_State, reinterpret_cast<UStruct*>(actCls), "InputComponent");
                if (inputOff >= 0) {
                    UObject* inputComp = reinterpret_cast<UObject*>(SafeMem_ReadPtr(reinterpret_cast<uint8_t*>(act) + inputOff));
                    if (inputComp && SafeMem_IsReadable(inputComp, 32)) {
                        PushPointerToLua(L, act);
                        return 1;
                    }
                }
            }
        }
    }

    UClass* charClass = ConsoleEnabler::FindClassByNameExact(g_State, "Character");
    if (charClass) {
        std::vector<UObject*> instances = EngineDiscovery::FindObjectsByClassFast(const_cast<EngineState&>(g_State), charClass);
        for (UObject* ch : instances) {
            if (!ch || !SafeMem_IsReadable(ch, 32)) continue;
            std::string chName = EngineDiscovery::GetObjectNameStr(g_State, ch);
            if (chName.find("Default__") != std::string::npos) continue;

            UClass* chCls = EngineDiscovery::GetObjectClass(g_State, ch);
            if (chCls) {
                int32_t ctrlOff = ConsoleEnabler::FindPropertyOffset(g_State, reinterpret_cast<UStruct*>(chCls), "Controller");
                if (ctrlOff >= 0) {
                    UObject* ctrl = reinterpret_cast<UObject*>(SafeMem_ReadPtr(reinterpret_cast<uint8_t*>(ch) + ctrlOff));
                    if (ctrl && SafeMem_IsReadable(ctrl, 32)) {
                        UClass* ctrlCls = EngineDiscovery::GetObjectClass(g_State, ctrl);
                        if (ctrlCls && ClassInheritsFrom(g_State, ctrlCls, "PlayerController")) {
                            PushPointerToLua(L, ch);
                            return 1;
                        }
                    }
                }
            }
        }
        for (UObject* ch : instances) {
            if (!ch || !SafeMem_IsReadable(ch, 32)) continue;
            std::string chName = EngineDiscovery::GetObjectNameStr(g_State, ch);
            if (chName.find("Default__") == std::string::npos) {
                PushPointerToLua(L, ch);
                return 1;
            }
        }
    }

    UClass* pawnClass = ConsoleEnabler::FindClassByNameExact(g_State, "Pawn");
    if (pawnClass) {
        std::vector<UObject*> instances = EngineDiscovery::FindObjectsByClassFast(const_cast<EngineState&>(g_State), pawnClass);
        for (UObject* p : instances) {
            if (!p || !SafeMem_IsReadable(p, 32)) continue;
            std::string pName = EngineDiscovery::GetObjectNameStr(g_State, p);
            if (pName.find("Default__") == std::string::npos) {
                PushPointerToLua(L, p);
                return 1;
            }
        }
    }

    lua_pushnil(L);
    return 1;
}

int LuaManager::Lua_GetPlayerController(lua_State* L) {
    UObject* pc = ConsoleEnabler::FindPlayerController(g_State);
    if (!pc) {
        UClass* pcClass = ConsoleEnabler::FindClassByNameExact(g_State, "PlayerController");
        if (pcClass) {
            pc = ConsoleEnabler::FindFirstObjectOfClass(g_State, pcClass, false);
        }
    }
    PushPointerToLua(L, pc);
    return 1;
}

int LuaManager::Lua_GetPlayerCameraManager(lua_State* L) {
    UObject* pc = ConsoleEnabler::FindPlayerController(g_State);
    if (pc && SafeMem_IsReadable(pc, 32)) {
        UClass* pcClass = EngineDiscovery::GetObjectClass(g_State, pc);
        if (pcClass) {
            int32_t off = ConsoleEnabler::FindPropertyOffset(g_State, reinterpret_cast<UStruct*>(pcClass), "PlayerCameraManager");
            if (off >= 0) {
                UObject* cam = reinterpret_cast<UObject*>(SafeMem_ReadPtr(reinterpret_cast<uint8_t*>(pc) + off));
                if (cam && SafeMem_IsReadable(cam, 32)) {
                    PushPointerToLua(L, cam);
                    return 1;
                }
            }
        }
    }
    UClass* camClass = ConsoleEnabler::FindClassByNameExact(g_State, "PlayerCameraManager");
    if (camClass) {
        UObject* cam = ConsoleEnabler::FindFirstObjectOfClass(g_State, camClass, false);
        if (cam) {
            PushPointerToLua(L, cam);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

int LuaManager::Lua_GetCameraInfo(lua_State* L) {
    UObject* pc = ConsoleEnabler::FindPlayerController(g_State);
    if (!pc) { lua_pushnil(L); return 1; }

    LuaEngine::Vector3D loc = {};
    LuaEngine::Rotator3D rot = {};
    float fov = 90.0f;
    bool haveLoc = false, haveRot = false;

    UClass* pcCls = EngineDiscovery::GetObjectClass(g_State, pc);
    void* fn = LuaEngine::FindFunctionOnClassEx(pcCls, "GetPlayerViewPoint");
    if (!fn) fn = LuaEngine::FindFunctionOnClassEx(pcCls, "K2_GetPlayerViewPoint");
    if (fn) {
        int topBefore = lua_gettop(L);
        int n = LuaEngine::CallUFunctionDynamic(L, pc, fn, 1);
        if (n >= 2) {
            haveLoc = LuaEngine::GetVector(L, -2, loc);
            haveRot = LuaEngine::GetRotator(L, -1, rot);
        }
        lua_settop(L, topBefore);
    }

    UObject* camMgr = nullptr;
    if (pc && SafeMem_IsReadable(pc, 32)) {
        UClass* pcClass = EngineDiscovery::GetObjectClass(g_State, pc);
        if (pcClass) {
            int32_t off = ConsoleEnabler::FindPropertyOffset(g_State, reinterpret_cast<UStruct*>(pcClass), "PlayerCameraManager");
            if (off >= 0) {
                UObject* cam = reinterpret_cast<UObject*>(SafeMem_ReadPtr(reinterpret_cast<uint8_t*>(pc) + off));
                if (cam && SafeMem_IsReadable(cam, 32)) camMgr = cam;
            }
        }
    }
    if (!camMgr) {
        UClass* camClass = ConsoleEnabler::FindClassByNameExact(g_State, "PlayerCameraManager");
        if (camClass) camMgr = ConsoleEnabler::FindFirstObjectOfClass(g_State, camClass, false);
    }

    if (camMgr) {
        UClass* camCls2 = EngineDiscovery::GetObjectClass(g_State, camMgr);
        if (!haveLoc) {
            void* locFn = LuaEngine::FindFunctionOnClassEx(camCls2, "GetCameraLocation");
            if (locFn) {
                uint8_t buf[64] = {};
                if (EngineDiscovery::CallProcessEvent(g_State, camMgr, locFn, buf)) {
                    if (g_State.UEMajor >= 5) {
                        struct DV { double X, Y, Z; } dv = {};
                        SafeMem_Read(buf, &dv, sizeof(dv));
                        loc = { static_cast<float>(dv.X), static_cast<float>(dv.Y), static_cast<float>(dv.Z) };
                    } else {
                        SafeMem_Read(buf, &loc, sizeof(loc));
                    }
                    haveLoc = true;
                }
            }
        }
        if (!haveRot) {
            void* rotFn = LuaEngine::FindFunctionOnClassEx(camCls2, "GetCameraRotation");
            if (rotFn) {
                uint8_t buf[64] = {};
                if (EngineDiscovery::CallProcessEvent(g_State, camMgr, rotFn, buf)) {
                    if (g_State.UEMajor >= 5) {
                        struct DR { double P, Y, R; } dr = {};
                        SafeMem_Read(buf, &dr, sizeof(dr));
                        rot = { static_cast<float>(dr.P), static_cast<float>(dr.Y), static_cast<float>(dr.R) };
                    } else {
                        SafeMem_Read(buf, &rot, sizeof(rot));
                    }
                    haveRot = true;
                }
            }
        }
        void* fovFn = LuaEngine::FindFunctionOnClassEx(camCls2, "GetFOVAngle");
        if (fovFn) {
            uint8_t buf[64] = {};
            if (EngineDiscovery::CallProcessEvent(g_State, camMgr, fovFn, buf)) {
                SafeMem_Read(buf, &fov, sizeof(float));
            }
        }
    }

    if (!haveLoc && !haveRot) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    LuaEngine::PushVector(L, loc.X, loc.Y, loc.Z);
    lua_setfield(L, -2, "Location");
    LuaEngine::PushRotator(L, rot.Pitch, rot.Yaw, rot.Roll);
    lua_setfield(L, -2, "Rotation");
    lua_pushnumber(L, fov);
    lua_setfield(L, -2, "FOV");
    return 1;
}

int LuaManager::Lua_ProjectWorldToScreen(lua_State* L) {
    UObject* pc = ConsoleEnabler::FindPlayerController(g_State);
    if (!pc) { lua_pushnil(L); lua_pushnil(L); lua_pushboolean(L, 0); return 3; }

    UClass* cls = EngineDiscovery::GetObjectClass(g_State, pc);
    void* ufunc = LuaEngine::FindFunctionOnClassEx(cls, "ProjectWorldLocationToScreen");
    if (!ufunc) { lua_pushnil(L); lua_pushnil(L); lua_pushboolean(L, 0); return 3; }

    int topBefore = lua_gettop(L);

    lua_pushnil(L);
    lua_insert(L, 2);

    int n = LuaEngine::CallUFunctionDynamic(L, pc, ufunc, 1);

    bool success = false;
    float sx = 0.0f, sy = 0.0f;
    if (n >= 1) success = lua_toboolean(L, -n);
    if (n >= 2 && (lua_isuserdata(L, -1) || lua_istable(L, -1))) {
        lua_getfield(L, -1, "X"); sx = static_cast<float>(luaL_optnumber(L, -1, 0)); lua_pop(L, 1);
        lua_getfield(L, -1, "Y"); sy = static_cast<float>(luaL_optnumber(L, -1, 0)); lua_pop(L, 1);
    }

    lua_settop(L, topBefore);
    lua_pushnumber(L, sx);
    lua_pushnumber(L, sy);
    lua_pushboolean(L, success ? 1 : 0);
    return 3;
}

int LuaManager::Lua_DeprojectScreenToWorld(lua_State* L) {
    UObject* pc = ConsoleEnabler::FindPlayerController(g_State);
    if (!pc) { lua_pushnil(L); lua_pushnil(L); lua_pushboolean(L, 0); return 3; }

    UClass* cls = EngineDiscovery::GetObjectClass(g_State, pc);
    void* ufunc = LuaEngine::FindFunctionOnClassEx(cls, "DeprojectScreenPositionToWorld");
    if (!ufunc) { lua_pushnil(L); lua_pushnil(L); lua_pushboolean(L, 0); return 3; }

    int n = LuaEngine::CallUFunctionDynamic(L, pc, ufunc, 1);
    if (n < 3) {
        lua_settop(L, lua_gettop(L) - n);
        lua_pushnil(L); lua_pushnil(L); lua_pushboolean(L, 0);
        return 3;
    }

    bool success = lua_toboolean(L, -3);
    LuaEngine::Vector3D worldPos = {};
    LuaEngine::Vector3D worldDir = {};
    LuaEngine::GetVector(L, -2, worldPos);
    LuaEngine::GetVector(L, -1, worldDir);
    lua_settop(L, lua_gettop(L) - n);
    LuaEngine::PushVector(L, worldPos.X, worldPos.Y, worldPos.Z);
    LuaEngine::PushVector(L, worldDir.X, worldDir.Y, worldDir.Z);
    lua_pushboolean(L, success ? 1 : 0);
    return 3;
}

int LuaManager::Lua_GetMousePosition(lua_State* L) {
    UObject* pc = ConsoleEnabler::FindPlayerController(g_State);
    if (!pc) { lua_pushnil(L); lua_pushnil(L); lua_pushboolean(L, 0); return 3; }

    UClass* cls = EngineDiscovery::GetObjectClass(g_State, pc);
    void* ufunc = LuaEngine::FindFunctionOnClassEx(cls, "GetMousePosition");
    if (!ufunc) { lua_pushnil(L); lua_pushnil(L); lua_pushboolean(L, 0); return 3; }

    int n = LuaEngine::CallUFunctionDynamic(L, pc, ufunc, 1);
    if (n < 3) {
        lua_settop(L, lua_gettop(L) - n);
        lua_pushnil(L); lua_pushnil(L); lua_pushboolean(L, 0);
        return 3;
    }

    bool success = lua_toboolean(L, -3);
    float mx = static_cast<float>(luaL_optnumber(L, -2, 0));
    float my = static_cast<float>(luaL_optnumber(L, -1, 0));
    lua_settop(L, lua_gettop(L) - n);
    lua_pushnumber(L, mx);
    lua_pushnumber(L, my);
    lua_pushboolean(L, success ? 1 : 0);
    return 3;
}

int LuaManager::Lua_GetPlayerState(lua_State* L) {
    UObject* pc = ConsoleEnabler::FindPlayerController(g_State);
    if (!pc) { lua_pushnil(L); return 1; }

    UClass* pcClass = EngineDiscovery::GetObjectClass(g_State, pc);
    if (!pcClass) { lua_pushnil(L); return 1; }

    int32_t off = ConsoleEnabler::FindPropertyOffset(g_State, reinterpret_cast<UStruct*>(pcClass), "PlayerState");
    if (off < 0) { lua_pushnil(L); return 1; }

    UObject* ps = reinterpret_cast<UObject*>(SafeMem_ReadPtr(reinterpret_cast<uint8_t*>(pc) + off));
    PushPointerToLua(L, ps);
    return 1;
}

int LuaManager::Lua_GetComponentByClass(lua_State* L) {
    UObject* actor = GetPointerFromLua(L, 1);
    const char* className = luaL_checkstring(L, 2);
    if (!actor || !className || !*className) { lua_pushnil(L); return 1; }

    UClass* compClass = ConsoleEnabler::FindClassByNameExact(g_State, className);
    if (!compClass) {
        std::string withC = std::string(className) + "_C";
        compClass = ConsoleEnabler::FindClassByNameExact(g_State, withC.c_str());
    }
    if (!compClass) { lua_pushnil(L); return 1; }

    UClass* actorCls = EngineDiscovery::GetObjectClass(g_State, actor);
    void* fn = LuaEngine::FindFunctionOnClassEx(actorCls, "GetComponentByClass");
    if (!fn) { lua_pushnil(L); return 1; }

    int topBefore = lua_gettop(L);
    LuaEngine::PushUObject(L, static_cast<UObject*>(compClass));
    lua_insert(L, 2);

    int n = LuaEngine::CallUFunctionDynamic(L, actor, fn, 1);
    if (n < 1) {
        lua_settop(L, topBefore);
        lua_pushnil(L);
        return 1;
    }

    UObject* result = LuaEngine::GetUObject(L, -1);
    lua_settop(L, topBefore);
    PushPointerToLua(L, result);
    return 1;
}

int LuaManager::Lua_GetAllComponents(lua_State* L) {
    UObject* actor = GetPointerFromLua(L, 1);
    if (!actor) { lua_newtable(L); return 1; }

    lua_newtable(L);
    int idx = 1;

    UClass* actorCls = EngineDiscovery::GetObjectClass(g_State, actor);
    if (!actorCls) return 1;

    int32_t rootOff = ConsoleEnabler::FindPropertyOffset(g_State, reinterpret_cast<UStruct*>(actorCls), "RootComponent");
    if (rootOff >= 0) {
        UObject* root = reinterpret_cast<UObject*>(SafeMem_ReadPtr(reinterpret_cast<uint8_t*>(actor) + rootOff));
        if (root) {
            PushPointerToLua(L, root);
            lua_rawseti(L, -2, idx++);
        }
    }

    void* fn = LuaEngine::FindFunctionOnClassEx(actorCls, "GetComponents");
    if (fn) {
        int topBefore = lua_gettop(L);
        int n = LuaEngine::CallUFunctionDynamic(L, actor, fn, 1);
        if (n >= 1) {
            if (lua_istable(L, -1)) {
                lua_pushnil(L);
                while (lua_next(L, -2) != 0) {
                    lua_rawseti(L, -4, idx++);
                }
            }
        }
        lua_settop(L, topBefore);
    }

    return 1;
}

int LuaManager::Lua_GetActorLocation(lua_State* L) {
    UObject* actor = GetPointerFromLua(L, 1);
    if (!actor) { lua_pushnil(L); return 1; }

    UClass* actorCls = EngineDiscovery::GetObjectClass(g_State, actor);
    void* fn = LuaEngine::FindFunctionOnClassEx(actorCls, "K2_GetActorLocation");
    if (!fn) { lua_pushnil(L); return 1; }

    int n = LuaEngine::CallUFunctionDynamic(L, actor, fn, 1);
    if (n >= 1) {
        LuaEngine::Vector3D loc = {};
        if (LuaEngine::GetVector(L, -1, loc)) {
            lua_settop(L, lua_gettop(L) - n);
            LuaEngine::PushVector(L, loc.X, loc.Y, loc.Z);
            return 1;
        }
    }
    lua_settop(L, lua_gettop(L) - n);
    lua_pushnil(L);
    return 1;
}

int LuaManager::Lua_SetActorLocation(lua_State* L) {
    UObject* actor = GetPointerFromLua(L, 1);
    if (!actor) { lua_pushboolean(L, 0); return 1; }

    UClass* actorCls = EngineDiscovery::GetObjectClass(g_State, actor);
    void* fn = LuaEngine::FindFunctionOnClassEx(actorCls, "K2_SetActorLocation");
    if (!fn) { lua_pushboolean(L, 0); return 1; }

    if (lua_gettop(L) < 3 || lua_isnil(L, 3)) {
        lua_pushboolean(L, 0);
        lua_insert(L, 3);
    }

    if (lua_gettop(L) < 4 || lua_isnil(L, 4)) {
        lua_pushboolean(L, 1);
        lua_insert(L, 4);
    }

    int topBefore = lua_gettop(L);
    int n = LuaEngine::CallUFunctionDynamic(L, actor, fn, 2);

    bool success = false;
    if (n >= 1) success = lua_toboolean(L, -n);

    lua_settop(L, topBefore);
    lua_pushboolean(L, success ? 1 : 0);
    return 1;
}

int LuaManager::Lua_GetViewportSize(lua_State* L) {

    ImGuiIO& io = ImGui::GetIO();
    lua_pushnumber(L, static_cast<lua_Number>(io.DisplaySize.x));
    lua_pushnumber(L, static_cast<lua_Number>(io.DisplaySize.y));
    return 2;
}

int LuaManager::Lua_LineTraceSingle(lua_State* L) {
    UObject* gs = FindEngineCDO("GameplayStatics");
    if (!gs) { lua_pushnil(L); lua_pushboolean(L, 0); return 2; }

    UClass* gsCls = EngineDiscovery::GetObjectClass(g_State, gs);
    void* fn = LuaEngine::FindFunctionOnClassEx(gsCls, "LineTraceSingle");
    if (!fn) { lua_pushnil(L); lua_pushboolean(L, 0); return 2; }

    int topBefore = lua_gettop(L);

    UObject* pc = ConsoleEnabler::FindPlayerController(g_State);
    if (pc) {
        LuaEngine::PushUObject(L, pc);
    } else {
        lua_pushnil(L);
    }
    lua_insert(L, 1);

    if (lua_gettop(L) < 4 || lua_isnil(L, 4)) {
        lua_pushnumber(L, 0);
        lua_insert(L, 4);
    }

    if (lua_gettop(L) < 5 || lua_isnil(L, 5)) {
        lua_pushboolean(L, 0);
        lua_insert(L, 5);
    }

    if (lua_gettop(L) < 6 || lua_isnil(L, 6)) {
        lua_pushnil(L);
        lua_insert(L, 6);
    }

    if (lua_gettop(L) < 7 || lua_isnil(L, 7)) {
        lua_pushnumber(L, 0);
        lua_insert(L, 7);
    }

    lua_pushnil(L);
    lua_insert(L, 8);

    if (lua_gettop(L) < 9 || lua_isnil(L, 9)) {
        lua_pushboolean(L, 1);
        lua_insert(L, 9);
    }

    int n = LuaEngine::CallUFunctionDynamic(L, gs, fn, 1);

    bool success = false;
    if (n >= 1) success = lua_toboolean(L, -n);

    if (n >= 2 && (lua_isuserdata(L, -1) || lua_istable(L, -1))) {
        lua_pushvalue(L, -1);
    } else {
        lua_pushnil(L);
    }

    lua_pushboolean(L, success ? 1 : 0);
    lua_replace(L, topBefore + 2);
    lua_replace(L, topBefore + 1);
    lua_settop(L, topBefore + 2);
    return 2;
}

int LuaManager::Lua_GetActorBounds(lua_State* L) {
    UObject* actor = GetPointerFromLua(L, 1);
    if (!actor) { lua_pushnil(L); lua_pushnil(L); return 2; }

    UObject* ksl = FindEngineCDO("KismetSystemLibrary");
    if (!ksl) ksl = FindEngineCDO("GameplayStatics");
    if (ksl) {
        UClass* kslCls = EngineDiscovery::GetObjectClass(g_State, ksl);
        void* fn = LuaEngine::FindFunctionOnClassEx(kslCls, "GetActorBounds");
        if (fn) {
            int topBefore = lua_gettop(L);

            lua_pushnil(L);
            lua_insert(L, 2);

            lua_pushnil(L);
            lua_insert(L, 3);

            int n = LuaEngine::CallUFunctionDynamic(L, ksl, fn, 1);
            if (n >= 2) {
                LuaEngine::Vector3D origin = {}, extent = {};
                if (LuaEngine::GetVector(L, -2, origin) && LuaEngine::GetVector(L, -1, extent)) {
                    lua_settop(L, topBefore);
                    LuaEngine::PushVector(L, origin.X, origin.Y, origin.Z);
                    LuaEngine::PushVector(L, extent.X, extent.Y, extent.Z);
                    return 2;
                }
            }
            lua_settop(L, topBefore);
        }
    }

    UClass* actorCls = EngineDiscovery::GetObjectClass(g_State, actor);
    void* fn = LuaEngine::FindFunctionOnClassEx(actorCls, "GetActorBounds");
    if (fn) {
        int topBefore = lua_gettop(L);

        lua_pushboolean(L, 1);
        lua_insert(L, 1);

        lua_pushnil(L);
        lua_insert(L, 2);

        lua_pushnil(L);
        lua_insert(L, 3);

        int n = LuaEngine::CallUFunctionDynamic(L, actor, fn, 1);
        if (n >= 2) {
            LuaEngine::Vector3D origin = {}, extent = {};
            if (LuaEngine::GetVector(L, -2, origin) && LuaEngine::GetVector(L, -1, extent)) {
                lua_settop(L, topBefore);
                LuaEngine::PushVector(L, origin.X, origin.Y, origin.Z);
                LuaEngine::PushVector(L, extent.X, extent.Y, extent.Z);
                return 2;
            }
        }
        lua_settop(L, topBefore);
    }

    lua_pushnil(L);
    lua_pushnil(L);
    return 2;
}

int LuaManager::Lua_GetLocalPlayer(lua_State* L) {
    UObject* lp = ConsoleEnabler::FindFirstLocalPlayer(g_State);
    PushPointerToLua(L, lp);
    return 1;
}

int LuaManager::Lua_GetGameInstance(lua_State* L) {

    void* liveWorld = static_cast<void*>(EngineDiscovery::GetLiveWorld(g_State));
    if (liveWorld && SafeMem_IsReadable(liveWorld, 32)) {
        UClass* wCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(liveWorld));
        if (wCls) {
            int32_t off = ConsoleEnabler::FindPropertyOffset(g_State, reinterpret_cast<UStruct*>(wCls), "OwningGameInstance");
            if (off >= 0) {
                UObject* gi = reinterpret_cast<UObject*>(SafeMem_ReadPtr(reinterpret_cast<uint8_t*>(liveWorld) + off));
                if (gi && SafeMem_IsReadable(gi, 32)) {
                    PushPointerToLua(L, gi);
                    return 1;
                }
            }
        }
    }
    UClass* giClass = ConsoleEnabler::FindClassByNameExact(g_State, "GameInstance");
    if (giClass) {
        UObject* gi = ConsoleEnabler::FindFirstObjectOfClass(g_State, giClass, false);
        if (gi) {
            PushPointerToLua(L, gi);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

int LuaManager::Lua_GetGameMode(lua_State* L) {
    void* liveWorld = static_cast<void*>(EngineDiscovery::GetLiveWorld(g_State));
    if (liveWorld && SafeMem_IsReadable(liveWorld, 32)) {
        UClass* wCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(liveWorld));
        if (wCls) {
            int32_t off = ConsoleEnabler::FindPropertyOffset(g_State, reinterpret_cast<UStruct*>(wCls), "AuthorityGameMode");
            if (off >= 0) {
                UObject* gm = reinterpret_cast<UObject*>(SafeMem_ReadPtr(reinterpret_cast<uint8_t*>(liveWorld) + off));
                if (gm && SafeMem_IsReadable(gm, 32)) {
                    PushPointerToLua(L, gm);
                    return 1;
                }
            }
        }
    }
    UClass* gmClass = ConsoleEnabler::FindClassByNameExact(g_State, "GameModeBase");
    if (!gmClass) gmClass = ConsoleEnabler::FindClassByNameExact(g_State, "GameMode");
    if (gmClass) {
        UObject* gm = ConsoleEnabler::FindFirstObjectOfClass(g_State, gmClass, false);
        if (gm) {
            PushPointerToLua(L, gm);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

int LuaManager::Lua_GetGameState(lua_State* L) {
    void* liveWorld = static_cast<void*>(EngineDiscovery::GetLiveWorld(g_State));
    if (liveWorld && SafeMem_IsReadable(liveWorld, 32)) {
        UClass* wCls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(liveWorld));
        if (wCls) {
            int32_t off = ConsoleEnabler::FindPropertyOffset(g_State, reinterpret_cast<UStruct*>(wCls), "GameState");
            if (off >= 0) {
                UObject* gs = reinterpret_cast<UObject*>(SafeMem_ReadPtr(reinterpret_cast<uint8_t*>(liveWorld) + off));
                if (gs && SafeMem_IsReadable(gs, 32)) {
                    PushPointerToLua(L, gs);
                    return 1;
                }
            }
        }
    }
    UClass* gsClass = ConsoleEnabler::FindClassByNameExact(g_State, "GameStateBase");
    if (!gsClass) gsClass = ConsoleEnabler::FindClassByNameExact(g_State, "GameState");
    if (gsClass) {
        UObject* gs = ConsoleEnabler::FindFirstObjectOfClass(g_State, gsClass, false);
        if (gs) {
            PushPointerToLua(L, gs);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

int LuaManager::Lua_GetAllActorsOfClass(lua_State* L) {
    FastMemScope fastMem;
    UClass* targetClass = nullptr;
    int maxLimit = 0;

    bool isGenericActor = false;
    if (lua_isstring(L, 1)) {
        const char* clsName = lua_tostring(L, 1);
        if (!clsName || !*clsName || _stricmp(clsName, "Actor") == 0 || _stricmp(clsName, "AActor") == 0) {
            isGenericActor = true;
        } else {
            targetClass = ConsoleEnabler::FindClassByNameExact(g_State, clsName);
            if (!targetClass) {
                std::string withC = std::string(clsName) + "_C";
                targetClass = ConsoleEnabler::FindClassByNameExact(g_State, withC.c_str());
            }
            if (!targetClass) {
                targetClass = EngineDiscovery::FindClassCached(g_State, clsName);
            }
            if (!targetClass) {
                std::string withC = std::string(clsName) + "_C";
                targetClass = EngineDiscovery::FindClassCached(g_State, withC);
            }
            if (!targetClass && (clsName[0] == 'A' || clsName[0] == 'U')) {
                targetClass = EngineDiscovery::FindClassCached(g_State, clsName + 1);
            }
            if (!targetClass) {

                lua_newtable(L);
                return 1;
            }
            if (targetClass == g_State.AActorClass) {
                isGenericActor = true;
            }
        }
    } else if (lua_isuserdata(L, 1)) {
        targetClass = reinterpret_cast<UClass*>(LuaEngine::GetUObject(L, 1));
        if (!targetClass) {
            lua_newtable(L);
            return 1;
        }
        if (targetClass == g_State.AActorClass) {
            isGenericActor = true;
        }
    } else {
        isGenericActor = true;
    }

    if (lua_isinteger(L, 2)) {
        maxLimit = static_cast<int>(lua_tointeger(L, 2));
    }

    static thread_local std::vector<void*> s_ActorsBuf(16384);
    int count = EngineDiscovery::GetWorldActors(g_State, s_ActorsBuf.data(), static_cast<int32_t>(s_ActorsBuf.size()));

    int estCount = (maxLimit > 0 && maxLimit < count) ? maxLimit : count;

    const int arrayIdx = lua_gettop(L) + 1;
    lua_createtable(L, estCount, 0);
    int tableIdx = 1;

    for (int i = 0; i < count; ++i) {
        if (maxLimit > 0 && tableIdx > maxLimit) break;

        UObject* act = static_cast<UObject*>(s_ActorsBuf[i]);
        if (!act || !SafeMem_IsReadable(act, 32)) continue;

        if (isGenericActor || ConsoleEnabler::IsObjectA(g_State, act, targetClass)) {
            PushPointerToLua(L, act);
            lua_rawseti(L, arrayIdx, tableIdx++);
        }
    }
    return 1;
}

int LuaManager::Lua_GetDistance(lua_State* L) {
    LuaEngine::Vector3D v1 = {}, v2 = {};
    bool ok1 = false, ok2 = false;

    if (lua_isuserdata(L, 1)) {
        UObject* o1 = LuaEngine::GetUObject(L, 1);
        if (o1) {
            UClass* cls = EngineDiscovery::GetObjectClass(g_State, o1);
            if (cls) {
                void* fn = EngineDiscovery::FindFunctionByName(g_State, reinterpret_cast<UStruct*>(cls), "K2_GetActorLocation");
                if (fn) {
                    uint8_t buf[64] = {0};
                    if (EngineDiscovery::CallProcessEvent(g_State, o1, fn, buf)) {
                        if (g_State.UEMajor >= 5) {
                            struct DoubleVector3D { double X, Y, Z; } dv = {};
                            SafeMem_Read(buf, &dv, sizeof(dv));
                            v1 = { static_cast<float>(dv.X), static_cast<float>(dv.Y), static_cast<float>(dv.Z) };
                        } else {
                            SafeMem_Read(buf, &v1, sizeof(LuaEngine::Vector3D));
                        }
                        ok1 = true;
                    }
                }
            }
        } else {
            ok1 = LuaEngine::GetVector(L, 1, v1);
        }
    }

    if (lua_isuserdata(L, 2)) {
        UObject* o2 = LuaEngine::GetUObject(L, 2);
        if (o2) {
            UClass* cls = EngineDiscovery::GetObjectClass(g_State, o2);
            if (cls) {
                void* fn = EngineDiscovery::FindFunctionByName(g_State, reinterpret_cast<UStruct*>(cls), "K2_GetActorLocation");
                if (fn) {
                    uint8_t buf[64] = {0};
                    if (EngineDiscovery::CallProcessEvent(g_State, o2, fn, buf)) {
                        if (g_State.UEMajor >= 5) {
                            struct DoubleVector3D { double X, Y, Z; } dv = {};
                            SafeMem_Read(buf, &dv, sizeof(dv));
                            v2 = { static_cast<float>(dv.X), static_cast<float>(dv.Y), static_cast<float>(dv.Z) };
                        } else {
                            SafeMem_Read(buf, &v2, sizeof(LuaEngine::Vector3D));
                        }
                        ok2 = true;
                    }
                }
            }
        } else {
            ok2 = LuaEngine::GetVector(L, 2, v2);
        }
    }

    if (ok1 && ok2) {
        float dx = v1.X - v2.X;
        float dy = v1.Y - v2.Y;
        float dz = v1.Z - v2.Z;
        lua_pushnumber(L, static_cast<lua_Number>(std::sqrt(dx * dx + dy * dy + dz * dz)));
    } else {
        lua_pushnumber(L, 0.0);
    }
    return 1;
}

int LuaManager::Lua_GetWorld(lua_State* L) {

    PushPointerToLua(L, static_cast<void*>(EngineDiscovery::GetLiveWorld(g_State)));
    return 1;
}

int LuaManager::Lua_GetEngine(lua_State* L) {
    if (g_State.GEngine) {
        PushPointerToLua(L, g_State.GEngine);
        return 1;
    }
    if (g_State.bIndexBuilt) {
        auto list = EngineDiscovery::FindObjectsByPrefix(g_State, "GameEngine");
        if (!list.empty() && list.front()) {
            g_State.GEngine = list.front();
            PushPointerToLua(L, list.front());
            return 1;
        }
        auto list2 = EngineDiscovery::FindObjectsByPrefix(g_State, "Engine");
        if (!list2.empty() && list2.front()) {
            g_State.GEngine = list2.front();
            PushPointerToLua(L, list2.front());
            return 1;
        }
    }
    int32_t count = EngineDiscovery::GetObjectCount(g_State);
    for (int32_t i = 0; i < count && i < 100000; ++i) {
        UObject* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
        if (!obj) continue;
        UClass* c = EngineDiscovery::GetObjectClass(g_State, obj);
        if (c) {
            std::string cName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(c));
            if (cName.find("GameEngine") != std::string::npos || cName == "Engine") {
                std::string oName = EngineDiscovery::GetObjectNameStr(g_State, obj);
                if (oName.find("Default__") == std::string::npos) {
                    g_State.GEngine = obj;
                    PushPointerToLua(L, obj);
                    return 1;
                }
            }
        }
    }
    PushPointerToLua(L, nullptr);
    return 1;
}

int LuaManager::Lua_GetObjectCount(lua_State* L) {
    lua_pushinteger(L, EngineDiscovery::GetObjectCount(g_State));
    return 1;
}

int LuaManager::Lua_GetObjectByIndex(lua_State* L) {
    int32_t index = static_cast<int32_t>(luaL_checkinteger(L, 1));
    UObject* obj = EngineDiscovery::GetObjectByIndex(g_State, index);
    PushPointerToLua(L, obj);
    return 1;
}

int LuaManager::Lua_FindObject(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    if (!name) {
        lua_pushnil(L);
        return 1;
    }

    UObject* obj = EngineDiscovery::FindObjectByNameFast(g_State, name);
    if (!obj && g_State.bIndexBuilt) {
        auto list = EngineDiscovery::FindObjectsByPrefix(g_State, name);
        if (!list.empty()) {
            obj = list.front();
        }
    }
    if (!obj && g_State.bIndexBuilt) {
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        for (const auto& pair : g_State.SortedNames) {
            std::string itemLower = pair.first;
            std::transform(itemLower.begin(), itemLower.end(), itemLower.begin(), ::tolower);
            if (itemLower.find(lowerName) != std::string::npos) {
                obj = pair.second;
                break;
            }
        }
    }
    if (!obj && g_State.bIndexBuilt) {
        UClass* dtClass = EngineDiscovery::FindClassCached(g_State, "DataTable");
        if (dtClass) {
            auto tables = EngineDiscovery::FindObjectsByClassFast(g_State, dtClass);
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            for (UObject* t : tables) {
                if (!t || !SafeMem_IsReadable(t, 32)) continue;
                uint32_t flags = 0;
                SafeMem_Read(reinterpret_cast<uint8_t*>(t) + g_State.UObject_FlagsOff, &flags, sizeof(flags));
                if (flags & 0x00000010) continue;
                std::string tName = EngineDiscovery::GetObjectNameStr(g_State, t);
                std::string tLower = tName;
                std::transform(tLower.begin(), tLower.end(), tLower.begin(), ::tolower);
                if (tLower.find(lowerName) != std::string::npos) {
                    obj = t;
                    break;
                }
            }
        }
    }
    PushPointerToLua(L, obj);
    return 1;
}

int LuaManager::Lua_FindObjects(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_newtable(L);
    if (!name) return 1;

    std::vector<UObject*> list;
    if (g_State.bIndexBuilt) {
        list = EngineDiscovery::FindObjectsByNameFast(g_State, name);
        if (list.empty()) {
            list = EngineDiscovery::FindObjectsByPrefix(g_State, name);
        }
    }

    for (size_t i = 0; i < list.size(); ++i) {
        PushPointerToLua(L, list[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}

int LuaManager::Lua_FindClass(lua_State* L) {
    const char* className = luaL_checkstring(L, 1);
    if (!className || !*className) {
        lua_pushnil(L);
        return 1;
    }

    UClass* cls = EngineDiscovery::FindClassCached(g_State, className);
    if (cls) {
        LuaEngine::PushUObject(L, static_cast<UObject*>(cls));
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_GetWorldActors(lua_State* L) {
    std::vector<void*> actors(10000);
    int count = EngineDiscovery::GetWorldActors(g_State, actors.data(), static_cast<int32_t>(actors.size()));

    lua_newtable(L);
    for (int i = 0; i < count; ++i) {
        PushPointerToLua(L, actors[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}

int LuaManager::Lua_GetObjectName(lua_State* L) {
    void* obj = GetPointerFromLua(L, 1);
    if (!obj) {
        lua_pushstring(L, "");
        return 1;
    }
    std::string name = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(obj));
    lua_pushstring(L, name.c_str());
    return 1;
}

int LuaManager::Lua_GetObjectFullName(lua_State* L) {
    void* obj = GetPointerFromLua(L, 1);
    if (!obj) {
        lua_pushstring(L, "");
        return 1;
    }
    std::string fullName = EngineDiscovery::BuildFullName(g_State, static_cast<UObject*>(obj));
    lua_pushstring(L, fullName.c_str());
    return 1;
}

int LuaManager::Lua_GetObjectClass(lua_State* L) {
    void* obj = GetPointerFromLua(L, 1);
    if (!obj) {
        lua_pushnil(L);
        return 1;
    }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj));
    PushPointerToLua(L, cls);
    return 1;
}

int LuaManager::Lua_GetSuperClass(lua_State* L) {
    void* ustruct = GetPointerFromLua(L, 1);
    if (!ustruct) {
        lua_pushnil(L);
        return 1;
    }
    UStruct* super = EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(ustruct));
    PushPointerToLua(L, super);
    return 1;
}

int LuaManager::Lua_InheritsFrom(lua_State* L) {
    void* objOrCls = GetPointerFromLua(L, 1);
    const char* baseName = luaL_checkstring(L, 2);
    if (!objOrCls || !baseName) {
        lua_pushboolean(L, 0);
        return 1;
    }

    UClass* cls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(objOrCls));
    UStruct* current = (cls && cls == g_State.UClassClass) ? static_cast<UStruct*>(objOrCls) : static_cast<UStruct*>(cls);

    bool inherits = false;
    for (int depth = 0; depth < 40 && current; ++depth) {
        std::string name = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(current));
        if (name == baseName) {
            inherits = true;
            break;
        }
        current = EngineDiscovery::GetSuperStruct(g_State, current);
    }

    lua_pushboolean(L, inherits ? 1 : 0);
    return 1;
}

int LuaManager::Lua_GetClassProperties(lua_State* L) {
    void* objOrCls = GetPointerFromLua(L, 1);
    if (!objOrCls) {
        lua_newtable(L);
        return 1;
    }

    UClass* cls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(objOrCls));
    void* targetClass = (cls && cls == g_State.UClassClass) ? objOrCls : cls;

    PropertyInfoEx props[512] = {};
    int count = GetClassProperties(targetClass, props, 512);

    lua_newtable(L);
    for (int i = 0; i < count; ++i) {
        lua_newtable(L);
        lua_pushstring(L, props[i].Name);
        lua_setfield(L, -2, "name");
        lua_pushstring(L, props[i].TypeName);
        lua_setfield(L, -2, "type");
        lua_pushinteger(L, props[i].Offset);
        lua_setfield(L, -2, "offset");
        lua_pushinteger(L, props[i].Size);
        lua_setfield(L, -2, "size");
        lua_pushboolean(L, props[i].bIsArray ? 1 : 0);
        lua_setfield(L, -2, "is_array");
        lua_pushboolean(L, props[i].bIsStruct ? 1 : 0);
        lua_setfield(L, -2, "is_struct");
        lua_pushboolean(L, props[i].bIsEnum ? 1 : 0);
        lua_setfield(L, -2, "is_enum");
        lua_pushboolean(L, props[i].bIsMap ? 1 : 0);
        lua_setfield(L, -2, "is_map");
        lua_pushboolean(L, props[i].bIsBool ? 1 : 0);
        lua_setfield(L, -2, "is_bool");
        lua_pushstring(L, props[i].ClassName);
        lua_setfield(L, -2, "class_name");

        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}

int LuaManager::Lua_CallFunction(lua_State* L) {
    void* obj = GetPointerFromLua(L, 1);
    const char* funcName = luaL_checkstring(L, 2);
    if (!obj || !funcName) {
        lua_pushboolean(L, 0);
        return 1;
    }

    UClass* cls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj));
    if (!cls) {
        lua_pushboolean(L, 0);
        return 1;
    }

    void* ufunc = EngineDiscovery::FindFunctionByName(g_State, static_cast<UStruct*>(cls), funcName);
    if (!ufunc) {
        char err[256];
        snprintf(err, sizeof(err), "[Lua] Function '%s' not found on target object\n", funcName);
        EngineDiscovery::WriteLogLine(err);
        lua_pushboolean(L, 0);
        return 1;
    }

    return LuaEngine::CallUFunctionDynamic(L, static_cast<UObject*>(obj), ufunc);
}

int LuaManager::Lua_ExportProperty(lua_State* L) {
    void* obj = GetPointerFromLua(L, 1);
    const char* propName = luaL_checkstring(L, 2);
    if (!obj || !propName) { lua_pushnil(L); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj));
    if (!cls) { lua_pushnil(L); return 1; }
    LuaEngine::FoundPropertyEx prop = LuaEngine::FindPropertyOnClassEx(cls, propName);
    if (!prop.Found) { lua_pushnil(L); return 1; }
    return LuaEngine::PushPropertyValueEx(L, reinterpret_cast<uint8_t*>(obj), prop);
}

int LuaManager::Lua_ImportProperty(lua_State* L) {
    void* obj = GetPointerFromLua(L, 1);
    const char* propName = luaL_checkstring(L, 2);
    if (!obj || !propName) { lua_pushboolean(L, 0); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj));
    if (!cls) { lua_pushboolean(L, 0); return 1; }
    LuaEngine::FoundPropertyEx prop = LuaEngine::FindPropertyOnClassEx(cls, propName);
    if (!prop.Found) { lua_pushboolean(L, 0); return 1; }
    bool ok = LuaEngine::SetPropertyValueEx(L, reinterpret_cast<uint8_t*>(obj), prop, 3);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_GetVersion(lua_State* L) {
    char buf[64];
    snprintf(buf, sizeof(buf), "UE%d.%d", g_State.UEMajor, g_State.UEMinor);
    lua_pushstring(L, buf);
    return 1;
}

int LuaManager::Lua_GetGameName(lua_State* L) {
    lua_pushstring(L, g_State.GameName[0] ? g_State.GameName : "Unknown");
    return 1;
}

int LuaManager::Lua_GetGlobals(lua_State* L) {
    lua_newtable(L);

    void* liveWorld = static_cast<void*>(EngineDiscovery::GetLiveWorld(g_State));

    PushPointerToLua(L, g_State.GUObjectArray);
    lua_setfield(L, -2, "guobjectarray");

    PushPointerToLua(L, g_State.GNames);
    lua_setfield(L, -2, "gnames");

    PushPointerToLua(L, liveWorld);
    lua_setfield(L, -2, "gworld");

    PushPointerToLua(L, g_State.GEngine);
    lua_setfield(L, -2, "gengine");

    PushPointerToLua(L, g_State.GMalloc);
    lua_setfield(L, -2, "gmalloc");

    PushPointerToLua(L, g_State.GNatives);
    lua_setfield(L, -2, "gnatives");

    return 1;
}

int LuaManager::Lua_ReadInt8(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (!base) { lua_pushnil(L); return 1; }
    int8_t val = 0;
    if (SafeMem_Read(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val))) {
        lua_pushinteger(L, val);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_ReadInt16(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (!base) { lua_pushnil(L); return 1; }
    int16_t val = 0;
    if (SafeMem_Read(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val))) {
        lua_pushinteger(L, val);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_ReadInt32(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (!base) { lua_pushnil(L); return 1; }
    int32_t val = 0;
    if (SafeMem_Read(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val))) {
        lua_pushinteger(L, val);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_ReadInt64(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (!base) { lua_pushnil(L); return 1; }
    int64_t val = 0;
    if (SafeMem_Read(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val))) {
        lua_pushinteger(L, val);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_ReadUInt8(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (!base) { lua_pushnil(L); return 1; }
    uint8_t val = 0;
    if (SafeMem_Read(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val))) {
        lua_pushinteger(L, val);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_ReadUInt16(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (!base) { lua_pushnil(L); return 1; }
    uint16_t val = 0;
    if (SafeMem_Read(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val))) {
        lua_pushinteger(L, val);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_ReadUInt32(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (!base) { lua_pushnil(L); return 1; }
    uint32_t val = 0;
    if (SafeMem_Read(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val))) {
        lua_pushinteger(L, val);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_ReadUInt64(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (!base) { lua_pushnil(L); return 1; }
    uint64_t val = 0;
    if (SafeMem_Read(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val))) {
        lua_pushinteger(L, static_cast<lua_Integer>(val));
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_ReadFloat(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (!base) { lua_pushnil(L); return 1; }
    float val = 0.0f;
    if (SafeMem_Read(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val))) {
        lua_pushnumber(L, val);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_ReadDouble(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (!base) { lua_pushnil(L); return 1; }
    double val = 0.0;
    if (SafeMem_Read(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val))) {
        lua_pushnumber(L, val);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_ReadPointer(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (!base) { lua_pushnil(L); return 1; }
    void* val = nullptr;
    if (SafeMem_Read(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val))) {
        PushPointerToLua(L, val);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_ReadBool(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    uint32_t mask = static_cast<uint32_t>(luaL_optinteger(L, 3, 1));
    if (!base) { lua_pushnil(L); return 1; }
    uint8_t val = 0;
    if (SafeMem_Read(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val))) {
        lua_pushboolean(L, (val & mask) ? 1 : 0);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_ReadString(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    int maxLen = static_cast<int>(luaL_optinteger(L, 3, 256));
    if (!base || maxLen <= 0 || maxLen > 4096) { lua_pushnil(L); return 1; }

    std::vector<char> buf(maxLen + 1, 0);
    if (SafeMem_Read(reinterpret_cast<uint8_t*>(base) + offset, buf.data(), maxLen)) {
        buf[maxLen] = '\0';
        lua_pushstring(L, buf.data());
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_ReadFString(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (!base) { lua_pushnil(L); return 1; }

    FString fstr = {};
    if (SafeMem_Read(reinterpret_cast<uint8_t*>(base) + offset, &fstr, sizeof(fstr))) {
        if (fstr.Data && fstr.ArrayNum > 0 && fstr.ArrayNum < 4096) {
            std::vector<wchar_t> wbuf(fstr.ArrayNum + 1, 0);
            if (SafeMem_Read(fstr.Data, wbuf.data(), fstr.ArrayNum * sizeof(wchar_t))) {
                FString temp = { wbuf.data(), fstr.ArrayNum, fstr.ArrayMax };
                lua_pushstring(L, temp.ToStdString().c_str());
                return 1;
            }
        }
    }
    lua_pushstring(L, "");
    return 1;
}

int LuaManager::Lua_ReadFName(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (!base) { lua_pushnil(L); return 1; }

    FName fname = {};
    if (SafeMem_Read(reinterpret_cast<uint8_t*>(base) + offset, &fname, sizeof(fname))) {
        std::string str = EngineDiscovery::FNameToStr(g_State, fname);
        lua_pushstring(L, str.c_str());
        return 1;
    }
    lua_pushstring(L, "");
    return 1;
}

static bool SafeWriteMemory(void* addr, const void* src, size_t size) {
    DWORD oldProtect = 0;
    if (VirtualProtect(addr, size, PAGE_READWRITE, &oldProtect)) {
        memcpy(addr, src, size);
        VirtualProtect(addr, size, oldProtect, &oldProtect);
        return true;
    }
    return false;
}

int LuaManager::Lua_WriteInt8(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int8_t val = static_cast<int8_t>(luaL_checkinteger(L, 2));
    int offset = static_cast<int>(luaL_optinteger(L, 3, 0));
    if (!base) { lua_pushboolean(L, 0); return 1; }
    bool ok = SafeWriteMemory(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_WriteInt16(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int16_t val = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int offset = static_cast<int>(luaL_optinteger(L, 3, 0));
    if (!base) { lua_pushboolean(L, 0); return 1; }
    bool ok = SafeWriteMemory(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_WriteInt32(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int32_t val = static_cast<int32_t>(luaL_checkinteger(L, 2));
    int offset = static_cast<int>(luaL_optinteger(L, 3, 0));
    if (!base) { lua_pushboolean(L, 0); return 1; }
    bool ok = SafeWriteMemory(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_WriteInt64(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    int64_t val = static_cast<int64_t>(luaL_checkinteger(L, 2));
    int offset = static_cast<int>(luaL_optinteger(L, 3, 0));
    if (!base) { lua_pushboolean(L, 0); return 1; }
    bool ok = SafeWriteMemory(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_WriteUInt8(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    uint8_t val = static_cast<uint8_t>(luaL_checkinteger(L, 2));
    int offset = static_cast<int>(luaL_optinteger(L, 3, 0));
    if (!base) { lua_pushboolean(L, 0); return 1; }
    bool ok = SafeWriteMemory(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_WriteUInt16(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    uint16_t val = static_cast<uint16_t>(luaL_checkinteger(L, 2));
    int offset = static_cast<int>(luaL_optinteger(L, 3, 0));
    if (!base) { lua_pushboolean(L, 0); return 1; }
    bool ok = SafeWriteMemory(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_WriteUInt32(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    uint32_t val = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    int offset = static_cast<int>(luaL_optinteger(L, 3, 0));
    if (!base) { lua_pushboolean(L, 0); return 1; }
    bool ok = SafeWriteMemory(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_WriteUInt64(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    uint64_t val = static_cast<uint64_t>(luaL_checkinteger(L, 2));
    int offset = static_cast<int>(luaL_optinteger(L, 3, 0));
    if (!base) { lua_pushboolean(L, 0); return 1; }
    bool ok = SafeWriteMemory(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_WriteFloat(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    float val = static_cast<float>(luaL_checknumber(L, 2));
    int offset = static_cast<int>(luaL_optinteger(L, 3, 0));
    if (!base) { lua_pushboolean(L, 0); return 1; }
    bool ok = SafeWriteMemory(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_WriteDouble(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    double val = static_cast<double>(luaL_checknumber(L, 2));
    int offset = static_cast<int>(luaL_optinteger(L, 3, 0));
    if (!base) { lua_pushboolean(L, 0); return 1; }
    bool ok = SafeWriteMemory(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_WritePointer(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    void* val = GetPointerFromLua(L, 2);
    int offset = static_cast<int>(luaL_optinteger(L, 3, 0));
    if (!base) { lua_pushboolean(L, 0); return 1; }
    bool ok = SafeWriteMemory(reinterpret_cast<uint8_t*>(base) + offset, &val, sizeof(val));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_WriteBool(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    bool val = lua_toboolean(L, 2) != 0;
    int offset = static_cast<int>(luaL_optinteger(L, 3, 0));
    uint8_t mask = static_cast<uint8_t>(luaL_optinteger(L, 4, 1));
    if (!base) { lua_pushboolean(L, 0); return 1; }

    uint8_t byteVal = 0;
    if (!SafeMem_Read(reinterpret_cast<uint8_t*>(base) + offset, &byteVal, sizeof(byteVal))) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (val) {
        byteVal |= mask;
    } else {
        byteVal &= ~mask;
    }

    bool ok = SafeWriteMemory(reinterpret_cast<uint8_t*>(base) + offset, &byteVal, sizeof(byteVal));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_Memory_IsValidPtr(lua_State* L) {
    void* base = GetPointerFromLua(L, 1);
    bool ok = base && SafeMem_IsReadable(base, 1);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

UObject* LuaManager::FindEngineCDO(const char* className) {
    if (!g_State.GUObjectArray || !className) return nullptr;
    int count = EngineDiscovery::GetObjectCount(g_State);
    UClass* targetClass = nullptr;

    for (int i = 0; i < count; ++i) {
        void* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
        if (!obj) continue;
        std::string n = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(obj));
        if (_stricmp(n.c_str(), className) == 0) {
            UClass* c = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj));
            std::string cName = c ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(c)) : "";
            if (cName == "Class") {
                targetClass = static_cast<UClass*>(obj);
                break;
            }
        }
    }

    if (!targetClass) return nullptr;

    for (int i = 0; i < count; ++i) {
        void* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
        if (obj && EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj)) == targetClass) {
            std::string n = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(obj));
            if (n.find("Default__") != std::string::npos) {
                return static_cast<UObject*>(obj);
            }
        }
    }

    for (int i = 0; i < count; ++i) {
        void* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
        if (obj && EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj)) == targetClass) {
            return static_cast<UObject*>(obj);
        }
    }

    return nullptr;
}

int LuaManager::Lua_GetGameplayStatics(lua_State* L) {
    UObject* cdo = FindEngineCDO("GameplayStatics");
    PushPointerToLua(L, cdo);
    return 1;
}

int LuaManager::Lua_GetKismetSystemLibrary(lua_State* L) {
    UObject* cdo = FindEngineCDO("KismetSystemLibrary");
    PushPointerToLua(L, cdo);
    return 1;
}

int LuaManager::Lua_GetKismetMathLibrary(lua_State* L) {
    UObject* cdo = FindEngineCDO("KismetMathLibrary");
    PushPointerToLua(L, cdo);
    return 1;
}

int LuaManager::Lua_GetKismetStringLibrary(lua_State* L) {
    UObject* cdo = FindEngineCDO("KismetStringLibrary");
    PushPointerToLua(L, cdo);
    return 1;
}

int LuaManager::Lua_GetKismetArrayLibrary(lua_State* L) {
    UObject* cdo = FindEngineCDO("KismetArrayLibrary");
    PushPointerToLua(L, cdo);
    return 1;
}

int LuaManager::Lua_GetKismetTextLibrary(lua_State* L) {
    UObject* cdo = FindEngineCDO("KismetTextLibrary");
    PushPointerToLua(L, cdo);
    return 1;
}

int LuaManager::Lua_GetKismetGuidLibrary(lua_State* L) {
    UObject* cdo = FindEngineCDO("KismetGuidLibrary");
    PushPointerToLua(L, cdo);
    return 1;
}

int LuaManager::Lua_GetKismetRenderingLibrary(lua_State* L) {
    UObject* cdo = FindEngineCDO("KismetRenderingLibrary");
    PushPointerToLua(L, cdo);
    return 1;
}

int LuaManager::Lua_GetCVar(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    if (!name || !*name || !g_State.IConsoleManager) { lua_pushnil(L); return 1; }

    SafeMem_SetFastMode(1);
    int32_t tmapOff = g_State.IConsoleManager_TMapOff > 0 ? g_State.IConsoleManager_TMapOff : 0x08;
    uint8_t* consoleManager = (uint8_t*)g_State.IConsoleManager;
    void* dataPtr = *(void**)(consoleManager + tmapOff + 0x00);
    int32_t arrayNum = *(int32_t*)(consoleManager + tmapOff + 0x08);
    int32_t arrayMax = *(int32_t*)(consoleManager + tmapOff + 0x0C);
    uint8_t* bitArrayBase = consoleManager + tmapOff + 0x10;

    uint32_t* allocationFlags = nullptr;
    uint32_t* heapPtrInline = *(uint32_t**)(bitArrayBase + 0x10);
    int32_t numBitsInline = *(int32_t*)(bitArrayBase + 0x18);
    uint32_t* heapPtrDefault = *(uint32_t**)(bitArrayBase + 0x00);
    int32_t numBitsDefault = *(int32_t*)(bitArrayBase + 0x08);

    auto IsValidHeapPtr = [](uint32_t* ptr) {
        return ptr && (uintptr_t)ptr > 0x10000 && (uintptr_t)ptr < 0x7FFFFFFFFFFFULL && SafeMem_IsReadable(ptr, 16);
    };

    if ((numBitsInline == arrayNum || numBitsInline == arrayMax) && IsValidHeapPtr(heapPtrInline)) {
        allocationFlags = heapPtrInline;
    } else if ((numBitsDefault == arrayNum || numBitsDefault == arrayMax) && IsValidHeapPtr(heapPtrDefault)) {
        allocationFlags = heapPtrDefault;
    } else if (IsValidHeapPtr(heapPtrInline)) {
        allocationFlags = heapPtrInline;
    } else if (IsValidHeapPtr(heapPtrDefault)) {
        allocationFlags = heapPtrDefault;
    } else if (arrayMax <= 128) {
        allocationFlags = (uint32_t*)bitArrayBase;
    }

    if (!allocationFlags || !dataPtr || arrayNum <= 0 || arrayNum > 200000) {
        SafeMem_SetFastMode(0);
        lua_pushnil(L);
        return 1;
    }

    int32_t stride = (g_State.UEMajor >= 5) ? 0x20 : 0x18;
    for (int32_t i = 0; i < arrayNum; ++i) {
        int dwordIndex = i / 32;
        int bitMask = 1 << (i % 32);
        if (dwordIndex > (200000 / 32)) break;

        __try {
            if ((allocationFlags[dwordIndex] & bitMask) != 0) {
                uint8_t* element = (uint8_t*)dataPtr + (i * stride);
                void* strData = *(void**)element;
                int32_t strLen = *(int32_t*)(element + 8);
                if (strData && strLen > 0 && strLen < 128) {
                    char nameBuf[128] = {0};
                    for (int k = 0; k < strLen && k < 127; ++k) {
                        nameBuf[k] = (char)((wchar_t*)strData)[k];
                    }
                    if (_stricmp(nameBuf, name) == 0) {
                        void* consoleObj = *(void**)(element + 0x10);
                        if (consoleObj && SafeMem_IsReadable(consoleObj, 8)) {
                            void** vt = *(void***)consoleObj;
                            if (vt && SafeMem_IsReadable(vt, sizeof(void*) * 14)) {
                                typedef int32_t (*GetInt_t)(const void*);
                                for (int slot = 5; slot <= 12; ++slot) {
                                    void* fn = vt[slot];
                                    if (fn && (uintptr_t)fn > 0x10000 && (uintptr_t)fn < 0x7FFFFFFFFFFFULL) {
                                        __try {
                                            int32_t iv = ((GetInt_t)fn)(consoleObj);
                                            SafeMem_SetFastMode(0);
                                            lua_pushinteger(L, iv);
                                            return 1;
                                        } __except (1) {}
                                    }
                                }
                            }
                        }
                        SafeMem_SetFastMode(0);
                        lua_pushinteger(L, 1);
                        return 1;
                    }
                }
            }
        } __except(1) {}
    }

    SafeMem_SetFastMode(0);
    lua_pushnil(L);
    return 1;
}

int LuaManager::Lua_SetCVar(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    if (!name || !*name) { lua_pushboolean(L, 0); return 1; }

    std::string valStr;
    if (lua_isboolean(L, 2)) valStr = lua_toboolean(L, 2) ? "1" : "0";
    else if (lua_isinteger(L, 2)) valStr = std::to_string(lua_tointeger(L, 2));
    else if (lua_isnumber(L, 2)) valStr = std::to_string(lua_tonumber(L, 2));
    else if (lua_isstring(L, 2)) valStr = lua_tostring(L, 2);
    else valStr = "1";

    std::string cmd = std::string(name) + " " + valStr;
    bool ok = LuaEngine::LuaHookManager::Get().ExecuteConsoleCommand(cmd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_FindCVars(lua_State* L) {

    const int arrayIdx = lua_gettop(L) + 1;
    lua_newtable(L);
    if (!g_State.IConsoleManager) return 1;

    SafeMem_SetFastMode(1);
    int32_t tmapOff = g_State.IConsoleManager_TMapOff > 0 ? g_State.IConsoleManager_TMapOff : 0x08;
    uint8_t* consoleManager = (uint8_t*)g_State.IConsoleManager;
    void* dataPtr = *(void**)(consoleManager + tmapOff + 0x00);
    int32_t arrayNum = *(int32_t*)(consoleManager + tmapOff + 0x08);
    int32_t arrayMax = *(int32_t*)(consoleManager + tmapOff + 0x0C);
    uint8_t* bitArrayBase = consoleManager + tmapOff + 0x10;

    uint32_t* allocationFlags = nullptr;
    uint32_t* heapPtrInline = *(uint32_t**)(bitArrayBase + 0x10);
    int32_t numBitsInline = *(int32_t*)(bitArrayBase + 0x18);
    uint32_t* heapPtrDefault = *(uint32_t**)(bitArrayBase + 0x00);
    int32_t numBitsDefault = *(int32_t*)(bitArrayBase + 0x08);

    auto IsValidHeapPtr = [](uint32_t* ptr) {
        return ptr && (uintptr_t)ptr > 0x10000 && (uintptr_t)ptr < 0x7FFFFFFFFFFFULL && SafeMem_IsReadable(ptr, 16);
    };

    if ((numBitsInline == arrayNum || numBitsInline == arrayMax) && IsValidHeapPtr(heapPtrInline)) {
        allocationFlags = heapPtrInline;
    } else if ((numBitsDefault == arrayNum || numBitsDefault == arrayMax) && IsValidHeapPtr(heapPtrDefault)) {
        allocationFlags = heapPtrDefault;
    } else if (IsValidHeapPtr(heapPtrInline)) {
        allocationFlags = heapPtrInline;
    } else if (IsValidHeapPtr(heapPtrDefault)) {
        allocationFlags = heapPtrDefault;
    } else if (arrayMax <= 128) {
        allocationFlags = (uint32_t*)bitArrayBase;
    }

    if (!allocationFlags || !dataPtr || arrayNum <= 0 || arrayNum > 200000) {
        SafeMem_SetFastMode(0);
        return 1;
    }

    int32_t stride = (g_State.UEMajor >= 5) ? 0x20 : 0x18;
    int tableIdx = 1;

    for (int32_t i = 0; i < arrayNum && i < 10000; ++i) {
        int dwordIndex = i / 32;
        int bitMask = 1 << (i % 32);
        if (dwordIndex > (200000 / 32)) break;

        __try {
            if ((allocationFlags[dwordIndex] & bitMask) != 0) {
                uint8_t* element = (uint8_t*)dataPtr + (i * stride);
                void* strData = *(void**)element;
                int32_t strLen = *(int32_t*)(element + 8);
                if (strData && strLen > 0 && strLen < 128) {
                    char nameBuf[128] = {0};
                    for (int k = 0; k < strLen && k < 127; ++k) {
                        nameBuf[k] = (char)((wchar_t*)strData)[k];
                    }
                    if (nameBuf[0]) {
                        lua_pushstring(L, nameBuf);
                        lua_rawseti(L, arrayIdx, tableIdx++);
                    }
                }
            }
        } __except(1) {}
    }

    SafeMem_SetFastMode(0);
    return 1;
}

int LuaManager::Lua_GetFrameTime(lua_State* L) {

    if (ImGui::GetCurrentContext()) {
        float dtMs = ImGui::GetIO().DeltaTime * 1000.0f;
        if (dtMs > 0.001f && dtMs < 500.0f) {
            lua_pushnumber(L, static_cast<double>(dtMs));
            return 1;
        }
    }

    double ft = LuaEngine::LuaHookManager::Get().GetLastFrameTimeMs();
    if (ft <= 0.0) ft = 16.666;
    lua_pushnumber(L, ft);
    return 1;
}

int LuaManager::Lua_GetFPS(lua_State* L) {

    if (ImGui::GetCurrentContext()) {
        float fps = ImGui::GetIO().Framerate;
        if (fps > 0.1f && fps < 1000.0f) {
            lua_pushnumber(L, static_cast<double>(fps));
            return 1;
        }
    }

    double fps = LuaEngine::LuaHookManager::Get().GetFPS();
    if (fps <= 0.0) fps = 60.0;
    lua_pushnumber(L, fps);
    return 1;
}

int LuaManager::Lua_GetDeltaSeconds(lua_State* L) {

    if (ImGui::GetCurrentContext()) {
        float dt = ImGui::GetIO().DeltaTime;
        if (dt > 0.00001f && dt < 1.0f) {
            lua_pushnumber(L, static_cast<double>(dt));
            return 1;
        }
    }

    double ds = LuaEngine::LuaHookManager::Get().GetDeltaSeconds();
    if (ds <= 0.0) ds = 0.016666;
    lua_pushnumber(L, ds);
    return 1;
}

int LuaManager::Lua_GetMemoryUsage(lua_State* L) {
    double usedMB = 0.0;
    double peakMB = 0.0;
    double availMB = 0.0;
    double privateMB = 0.0;
    double totalMB = 0.0;

    PROCESS_MEMORY_COUNTERS_EX pmc = {};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        usedMB = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
        peakMB = static_cast<double>(pmc.PeakWorkingSetSize) / (1024.0 * 1024.0);
        privateMB = static_cast<double>(pmc.PrivateUsage) / (1024.0 * 1024.0);
    }

    MEMORYSTATUSEX memStatus = {};
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        availMB = static_cast<double>(memStatus.ullAvailPhys) / (1024.0 * 1024.0);
        totalMB = static_cast<double>(memStatus.ullTotalPhys) / (1024.0 * 1024.0);
    }

    lua_newtable(L);

    lua_pushnumber(L, usedMB);    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, peakMB);    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, availMB);   lua_rawseti(L, -2, 3);
    lua_pushnumber(L, privateMB); lua_rawseti(L, -2, 4);
    lua_pushnumber(L, totalMB);   lua_rawseti(L, -2, 5);

    lua_pushnumber(L, usedMB);    lua_setfield(L, -2, "usedMB");
    lua_pushnumber(L, peakMB);    lua_setfield(L, -2, "peakMB");
    lua_pushnumber(L, availMB);   lua_setfield(L, -2, "availMB");
    lua_pushnumber(L, privateMB); lua_setfield(L, -2, "privateMB");
    lua_pushnumber(L, totalMB);   lua_setfield(L, -2, "totalMB");

    return 1;
}

int LuaManager::Lua_ForceGarbageCollection(lua_State* L) {
    bool executed = false;

    UObject* ksCDO = FindEngineCDO("KismetSystemLibrary");
    if (ksCDO) {
        UClass* ksClass = EngineDiscovery::GetObjectClass(g_State, ksCDO);
        void* fn = EngineDiscovery::FindFunctionByName(g_State, static_cast<UStruct*>(ksClass), "CollectGarbage");
        if (fn) {
            EngineDiscovery::CallProcessEvent(g_State, ksCDO, fn, nullptr);
            executed = true;
        }
    }

    if (!executed) {
        LuaEngine::LuaHookManager::Get().ExecuteConsoleCommand("obj gc");
        executed = true;
    }

    char logBuf[256];
    snprintf(logBuf, sizeof(logBuf), "[Lua] ForceGarbageCollection triggered (status: %s)\n", executed ? "success" : "failed");
    EngineDiscovery::WriteLogLine(logBuf);

    lua_pushboolean(L, executed ? 1 : 0);
    return 1;
}

int LuaManager::Lua_File_Read(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        lua_pushnil(L);
        return 1;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string content = buffer.str();
    lua_pushlstring(L, content.data(), content.size());
    return 1;
}

int LuaManager::Lua_File_Write(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    size_t len = 0;
    const char* content = luaL_checklstring(L, 2, &len);
    std::filesystem::path p(path);
    std::error_code ec;
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);
    }
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        lua_pushboolean(L, 0);
        return 1;
    }
    out.write(content, len);
    bool ok = out.good();
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_File_Append(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    size_t len = 0;
    const char* content = luaL_checklstring(L, 2, &len);
    std::filesystem::path p(path);
    std::error_code ec;
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);
    }
    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (!out.is_open()) {
        lua_pushboolean(L, 0);
        return 1;
    }
    out.write(content, len);
    bool ok = out.good();
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_File_Exists(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    bool exists = std::filesystem::exists(path, ec);
    lua_pushboolean(L, exists ? 1 : 0);
    return 1;
}

int LuaManager::Lua_File_List(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    lua_newtable(L);
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_directory(path, ec)) {
        return 1;
    }
    int idx = 1;
    for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
        lua_pushstring(L, entry.path().filename().string().c_str());
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

int LuaManager::Lua_File_CreateDir(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    bool ok = std::filesystem::create_directories(path, ec);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int LuaManager::Lua_File_Delete(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    bool ok = std::filesystem::remove_all(path, ec) > 0;
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static void LuaTableToJsonHelper(lua_State* L, int index, std::stringstream& ss, int depth, bool pretty) {
    int t = lua_type(L, index);
    if (t == LUA_TNIL) {
        ss << "null";
    } else if (t == LUA_TBOOLEAN) {
        ss << (lua_toboolean(L, index) ? "true" : "false");
    } else if (t == LUA_TNUMBER) {
        if (lua_isinteger(L, index)) {
            ss << lua_tointeger(L, index);
        } else {
            ss << lua_tonumber(L, index);
        }
    } else if (t == LUA_TSTRING) {
        const char* str = lua_tostring(L, index);
        ss << "\"";
        for (const char* c = str; *c; ++c) {
            if (*c == '"') ss << "\\\"";
            else if (*c == '\\') ss << "\\\\";
            else if (*c == '\n') ss << "\\n";
            else if (*c == '\r') ss << "\\r";
            else if (*c == '\t') ss << "\\t";
            else ss << *c;
        }
        ss << "\"";
    } else if (t == LUA_TTABLE) {
        if (depth > 32) { ss << "\"{...}\""; return; }
        int absIndex = lua_absindex(L, index);
        size_t rawLen = lua_rawlen(L, absIndex);
        bool isArray = (rawLen > 0);
        auto indent = [](std::stringstream& out, int d, bool p) {
            if (p) {
                out << "\n";
                for (int i = 0; i < d; ++i) out << "  ";
            }
        };

        if (isArray) {
            ss << "[";
            for (size_t i = 1; i <= rawLen; ++i) {
                if (i > 1) ss << ",";
                indent(ss, depth + 1, pretty);
                lua_rawgeti(L, absIndex, static_cast<lua_Integer>(i));
                LuaTableToJsonHelper(L, -1, ss, depth + 1, pretty);
                lua_pop(L, 1);
            }
            if (rawLen > 0) indent(ss, depth, pretty);
            ss << "]";
        } else {
            ss << "{";
            bool first = true;
            lua_pushnil(L);
            while (lua_next(L, absIndex) != 0) {
                if (!first) ss << ",";
                first = false;
                indent(ss, depth + 1, pretty);
                std::string k;
                if (lua_type(L, -2) == LUA_TSTRING) k = lua_tostring(L, -2);
                else if (lua_type(L, -2) == LUA_TNUMBER) k = std::to_string(lua_tointeger(L, -2));
                else k = "key";

                ss << "\"" << k << "\":" << (pretty ? " " : "");
                LuaTableToJsonHelper(L, -1, ss, depth + 1, pretty);
                lua_pop(L, 1);
            }
            if (!first) indent(ss, depth, pretty);
            ss << "}";
        }
    } else {
        ss << "\"[" << lua_typename(L, t) << "]\"";
    }
}

static const char* SkipWhitespaceJson(const char* p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    return p;
}

static const char* ParseJsonValueInternal(lua_State* L, const char* p);

static const char* ParseJsonStringInternal(lua_State* L, const char* p) {
    if (*p != '"') return nullptr;
    ++p;
    std::string s;
    while (*p && *p != '"') {
        if (*p == '\\' && *(p + 1)) {
            ++p;
            if (*p == 'n') s += '\n';
            else if (*p == 'r') s += '\r';
            else if (*p == 't') s += '\t';
            else s += *p;
        } else {
            s += *p;
        }
        ++p;
    }

    if (*p != '"') return nullptr;
    ++p;
    lua_pushlstring(L, s.data(), s.size());
    return p;
}

static const char* ParseJsonNumberInternal(lua_State* L, const char* p) {
    char* end = nullptr;
    double d = std::strtod(p, &end);
    if (end <= p) return nullptr;
    if (std::floor(d) == d && d >= -2147483648.0 && d <= 2147483647.0) {
        lua_pushinteger(L, static_cast<lua_Integer>(d));
    } else {
        lua_pushnumber(L, d);
    }
    return end;
}

static const char* ParseJsonArrayInternal(lua_State* L, const char* p) {
    if (*p != '[') return nullptr;
    ++p;
    lua_newtable(L);
    const int tableIndex = lua_gettop(L);
    int idx = 1;
    p = SkipWhitespaceJson(p);
    if (*p == ']') return p + 1;
    while (true) {
        p = SkipWhitespaceJson(p);
        const char* elementEnd = ParseJsonValueInternal(L, p);
        if (!elementEnd) { lua_pop(L, 1); return nullptr; }
        lua_rawseti(L, tableIndex, idx++);
        p = SkipWhitespaceJson(elementEnd);
        if (*p == ',') { ++p; continue; }
        if (*p == ']') { ++p; break; }

        lua_pop(L, 1);
        return nullptr;
    }
    return p;
}

static const char* ParseJsonObjectInternal(lua_State* L, const char* p) {
    if (*p != '{') return nullptr;
    ++p;
    lua_newtable(L);
    const int tableIndex = lua_gettop(L);
    p = SkipWhitespaceJson(p);
    if (*p == '}') return p + 1;
    while (true) {
        p = SkipWhitespaceJson(p);
        if (*p != '"') { lua_pop(L, 1); return nullptr; }
        std::string key;
        ++p;
        while (*p && *p != '"') {
            if (*p == '\\' && *(p + 1)) { ++p; key += *p; }
            else key += *p;
            ++p;
        }
        if (*p != '"') { lua_pop(L, 1); return nullptr; }
        ++p;
        p = SkipWhitespaceJson(p);
        if (*p != ':') { lua_pop(L, 1); return nullptr; }
        p = SkipWhitespaceJson(p + 1);
        const char* valueEnd = ParseJsonValueInternal(L, p);
        if (!valueEnd) { lua_pop(L, 1); return nullptr; }
        lua_setfield(L, tableIndex, key.c_str());
        p = SkipWhitespaceJson(valueEnd);
        if (*p == ',') { ++p; continue; }

        if (*p == '}') { ++p; break; }
        lua_pop(L, 1);
        return nullptr;
    }
    return p;
}

static const char* ParseJsonValueInternal(lua_State* L, const char* p) {
    p = SkipWhitespaceJson(p);
    if (!*p) return nullptr;
    if (*p == '{') return ParseJsonObjectInternal(L, p);
    if (*p == '[') return ParseJsonArrayInternal(L, p);
    if (*p == '"') return ParseJsonStringInternal(L, p);
    if (strncmp(p, "true", 4) == 0)  { lua_pushboolean(L, 1); return p + 4; }
    if (strncmp(p, "false", 5) == 0) { lua_pushboolean(L, 0); return p + 5; }
    if (strncmp(p, "null", 4) == 0)  { lua_pushnil(L); return p + 4; }

    if ((*p >= '0' && *p <= '9') || *p == '-') return ParseJsonNumberInternal(L, p);
    return nullptr;
}

int LuaManager::Lua_JSON_Parse(lua_State* L) {
    const char* str = luaL_checkstring(L, 1);
    const char* end = ParseJsonValueInternal(L, str);

    if (!end || *SkipWhitespaceJson(end) != '\0') {
        lua_settop(L, 0);
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_JSON_Stringify(lua_State* L) {
    bool pretty = (lua_gettop(L) >= 2) ? (lua_toboolean(L, 2) != 0) : false;
    std::stringstream ss;
    LuaTableToJsonHelper(L, 1, ss, 0, pretty);
    std::string res = ss.str();
    lua_pushlstring(L, res.data(), res.size());
    return 1;
}

int LuaManager::Lua_GetModDirectory(lua_State* L) {
    std::string dir = LuaManager::Get().GetModsDirectory();
    lua_pushstring(L, dir.c_str());
    return 1;
}

int LuaManager::Lua_SpawnActorByClass(lua_State* L) {
    const char* className = luaL_checkstring(L, 1);
    if (!className || !*className) { lua_pushnil(L); return 1; }

    UClass* cls = EngineDiscovery::FindClassCached(g_State, className);
    if (!cls) {
        std::string withC = std::string(className) + "_C";
        cls = EngineDiscovery::FindClassCached(g_State, withC);
    }
    if (!cls && (className[0] == 'A' || className[0] == 'U')) {
        cls = EngineDiscovery::FindClassCached(g_State, className + 1);
    }
    if (!cls) {
        cls = ConsoleEnabler::FindClassByNameExact(g_State, className);
    }
    if (!cls) { lua_pushnil(L); return 1; }

    LuaEngine::Vector3D loc = { 0, 0, 0 };
    LuaEngine::Rotator3D rot = { 0, 0, 0 };
    if (lua_gettop(L) >= 2) LuaEngine::GetVector(L, 2, loc);
    if (lua_gettop(L) >= 3) LuaEngine::GetRotator(L, 3, rot);

    UObject* world = EngineDiscovery::GetLiveWorld(g_State);
    std::string why;
    UObject* actor = LuaEngine::SpawnActorThroughEngineApi(world, cls, loc, rot, &why);
    if (actor) {
        PushPointerToLua(L, actor);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int LuaManager::Lua_GetAllPawns(lua_State* L) {
    const int arrayIdx = lua_gettop(L) + 1;
    lua_newtable(L);

    UClass* targetClass = ConsoleEnabler::FindClassByNameExact(g_State, "Pawn");
    if (!targetClass) targetClass = EngineDiscovery::FindClassCached(g_State, "Pawn");
    if (!targetClass) return 1;

    static thread_local std::vector<void*> s_FindAllBuf(16384);
    int count = EngineDiscovery::GetWorldActors(g_State, s_FindAllBuf.data(), static_cast<int32_t>(s_FindAllBuf.size()));

    int tableIdx = 1;
    for (int i = 0; i < count; ++i) {
        UObject* act = static_cast<UObject*>(s_FindAllBuf[i]);
        if (!act || !SafeMem_IsReadable(act, 32)) continue;
        if (ConsoleEnabler::IsObjectA(g_State, act, targetClass)) {
            PushPointerToLua(L, act);
            lua_rawseti(L, arrayIdx, tableIdx++);
        }
    }

    if (tableIdx == 1 && targetClass) {
        auto instances = EngineDiscovery::FindObjectsByClassFast(g_State, targetClass);
        for (UObject* inst : instances) {
            if (!inst || !SafeMem_IsReadable(inst, 32)) continue;
            uint32_t flags = 0;
            SafeMem_Read((uint8_t*)inst + g_State.UObject_FlagsOff, &flags, sizeof(flags));
            if (flags & 0x00000010) continue;
            PushPointerToLua(L, inst);
            lua_rawseti(L, arrayIdx, tableIdx++);
        }
    }
    return 1;
}

static int Lua_WithWorldWrapper(lua_State* L) {
    UObject* world = EngineDiscovery::GetLiveWorld(g_State);
    if (world && SafeMem_IsReadable(world, 32)) {
        PushPointerToLua(L, world);
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_insert(L, -2);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            char buf[512];
            snprintf(buf, sizeof(buf), "[Lua] WithWorld callback error: %s", err ? err : "unknown");
            EngineDiscovery::WriteLogLine(buf);
            lua_pop(L, 1);
        }
    }
    return 0;
}

int LuaManager::Lua_WithWorld(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, Lua_WithWorldWrapper, 1);
    int wrapperRef = luaL_ref(L, LUA_REGISTRYINDEX);
    LuaEngine::LuaHookManager::Get().EnqueueGameThreadTask(wrapperRef);
    return 0;
}

static int Lua_WhenClassSpawnsPoller(lua_State* L) {
    const char* className = lua_tostring(L, lua_upvalueindex(1));
    if (!className) return 0;

    UClass* targetClass = ConsoleEnabler::FindClassByNameExact(g_State, className);
    if (!targetClass) {
        std::string withC = std::string(className) + "_C";
        targetClass = ConsoleEnabler::FindClassByNameExact(g_State, withC.c_str());
    }
    if (!targetClass) {
        targetClass = EngineDiscovery::FindClassCached(g_State, className);
    }

    UObject* found = nullptr;
    if (targetClass) {
        static thread_local std::vector<void*> s_FindAllBuf(16384);
        int count = EngineDiscovery::GetWorldActors(g_State, s_FindAllBuf.data(), static_cast<int32_t>(s_FindAllBuf.size()));
        for (int i = 0; i < count; ++i) {
            UObject* act = static_cast<UObject*>(s_FindAllBuf[i]);
            if (!act || !SafeMem_IsReadable(act, 32)) continue;
            if (ConsoleEnabler::IsObjectA(g_State, act, targetClass)) {
                found = act;
                break;
            }
        }
        if (!found) {
            auto instances = EngineDiscovery::FindObjectsByClassFast(g_State, targetClass);
            for (UObject* inst : instances) {
                if (!inst || !SafeMem_IsReadable(inst, 32)) continue;
                uint32_t flags = 0;
                SafeMem_Read((uint8_t*)inst + g_State.UObject_FlagsOff, &flags, sizeof(flags));
                if (flags & 0x00000010) continue;
                found = inst;
                break;
            }
        }
    }

    if (found) {
        PushPointerToLua(L, found);
        lua_pushvalue(L, lua_upvalueindex(2));
        lua_insert(L, -2);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            char buf[512];
            snprintf(buf, sizeof(buf), "[Lua] WhenClassSpawns callback error: %s", err ? err : "unknown");
            EngineDiscovery::WriteLogLine(buf);
            lua_pop(L, 1);
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    return 0;
}

int LuaManager::Lua_WhenClassSpawns(lua_State* L) {
    const char* className = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    int64_t interval = 200;
    if (lua_isinteger(L, 3)) interval = lua_tointeger(L, 3);

    lua_pushstring(L, className);
    lua_pushvalue(L, 2);
    lua_pushcclosure(L, Lua_WhenClassSpawnsPoller, 2);
    int pollerRef = luaL_ref(L, LUA_REGISTRYINDEX);
    uint32_t timerId = LuaEngine::LuaHookManager::Get().LoopAsync(interval, pollerRef);
    lua_pushinteger(L, timerId);
    return 1;
}

struct DirectorState {
    float current = 1.0f;
    float restoreTo = 1.0f;
    bool bulletTime = false;
    bool freeze = false;
};

static DirectorState& GetDirectorState() {
    static DirectorState s;
    return s;
}

static float Director_GetTimeDilation() {
    UObject* world = EngineDiscovery::GetLiveWorld(g_State);
    if (!world) return 1.0f;

    static int32_t persistentLevelOff = -1;
    static int32_t worldSettingsOff = -1;
    static int32_t timeDilationOff = -1;

    if (persistentLevelOff < 0) {
        UClass* worldClass = EngineDiscovery::GetObjectClass(g_State, world);
        if (worldClass) {
            persistentLevelOff = ConsoleEnabler::FindPropertyOffset(g_State, (UStruct*)worldClass, "PersistentLevel");
        }
    }
    if (persistentLevelOff < 0) return 1.0f;

    void* level = SafeMem_ReadPtr((uint8_t*)world + persistentLevelOff);
    if (!level) return 1.0f;

    if (worldSettingsOff < 0) {
        UClass* levelClass = ConsoleEnabler::FindClassByNameExact(g_State, "Level");
        if (levelClass) {
            worldSettingsOff = ConsoleEnabler::FindPropertyOffset(g_State, (UStruct*)levelClass, "WorldSettings");
        }
    }
    if (worldSettingsOff < 0) return 1.0f;

    void* worldSettings = SafeMem_ReadPtr((uint8_t*)level + worldSettingsOff);
    if (!worldSettings) return 1.0f;

    if (timeDilationOff < 0) {
        UClass* wsClass = EngineDiscovery::GetObjectClass(g_State, (UObject*)worldSettings);
        if (!wsClass) wsClass = ConsoleEnabler::FindClassByNameExact(g_State, "WorldSettings");
        if (wsClass) {
            timeDilationOff = ConsoleEnabler::FindPropertyOffset(g_State, (UStruct*)wsClass, "TimeDilation");
        }
    }
    if (timeDilationOff < 0) return 1.0f;

    int ok = 0;
    float val = SafeMem_ReadFloat((uint8_t*)worldSettings + timeDilationOff, &ok);
    return ok ? val : 1.0f;
}

static bool Director_SetTimeDilation(float scale) {
    UObject* world = EngineDiscovery::GetLiveWorld(g_State);
    if (!world) return false;

    static int32_t persistentLevelOff = -1;
    static int32_t worldSettingsOff = -1;
    static int32_t timeDilationOff = -1;

    if (persistentLevelOff < 0) {
        UClass* worldClass = EngineDiscovery::GetObjectClass(g_State, world);
        if (worldClass) {
            persistentLevelOff = ConsoleEnabler::FindPropertyOffset(g_State, (UStruct*)worldClass, "PersistentLevel");
        }
    }
    if (persistentLevelOff < 0) return false;

    void* level = SafeMem_ReadPtr((uint8_t*)world + persistentLevelOff);
    if (!level) return false;

    if (worldSettingsOff < 0) {
        UClass* levelClass = ConsoleEnabler::FindClassByNameExact(g_State, "Level");
        if (levelClass) {
            worldSettingsOff = ConsoleEnabler::FindPropertyOffset(g_State, (UStruct*)levelClass, "WorldSettings");
        }
    }
    if (worldSettingsOff < 0) return false;

    void* worldSettings = SafeMem_ReadPtr((uint8_t*)level + worldSettingsOff);
    if (!worldSettings) return false;

    if (timeDilationOff < 0) {
        UClass* wsClass = EngineDiscovery::GetObjectClass(g_State, (UObject*)worldSettings);
        if (!wsClass) wsClass = ConsoleEnabler::FindClassByNameExact(g_State, "WorldSettings");
        if (wsClass) {
            timeDilationOff = ConsoleEnabler::FindPropertyOffset(g_State, (UStruct*)wsClass, "TimeDilation");
        }
    }
    if (timeDilationOff < 0) return false;

    SafeMem_Write((uint8_t*)worldSettings + timeDilationOff, &scale, sizeof(scale));
    return true;
}

static int Lua_DirectorSet(lua_State* L) {
    float scale = static_cast<float>(luaL_checknumber(L, 1));
    bool ok = Director_SetTimeDilation(scale);
    if (ok) GetDirectorState().current = scale;
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_DirectorGet(lua_State* L) {
    lua_pushnumber(L, Director_GetTimeDilation());
    return 1;
}

static int Lua_DirectorSlowForRestore(lua_State* L) {
    float scale      = static_cast<float>(lua_tonumber(L, lua_upvalueindex(1)));
    float restoreTo  = static_cast<float>(lua_tonumber(L, lua_upvalueindex(2)));
    DirectorState& ds = GetDirectorState();
    if (std::fabs(ds.current - scale) < 0.0001f) {
        Director_SetTimeDilation(restoreTo);
        ds.current = restoreTo;
    }
    return 0;
}

static int Lua_DirectorSlowFor(lua_State* L) {
    int64_t ms = luaL_checkinteger(L, 1);
    float scale = static_cast<float>(luaL_optnumber(L, 2, 0.15));
    DirectorState& ds = GetDirectorState();
    float previous = ds.current;
    ds.restoreTo = previous;
    bool ok = Director_SetTimeDilation(scale);
    if (ok) ds.current = scale;

    lua_pushnumber(L, scale);
    lua_pushnumber(L, previous);
    lua_pushcclosure(L, Lua_DirectorSlowForRestore, 2);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    uint64_t handle = LuaEngine::LuaHookManager::Get().MakeActionHandle();
    LuaEngine::LuaHookManager::Get().ExecuteWithDelayUsingHandle(handle, ms, cbRef, false);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_DirectorToggleBulletTime(lua_State* L) {
    float slowScale = static_cast<float>(luaL_optnumber(L, 1, 0.25));
    DirectorState& ds = GetDirectorState();
    ds.bulletTime = !ds.bulletTime;
    if (ds.bulletTime) {
        ds.restoreTo = ds.current;
        bool ok = Director_SetTimeDilation(slowScale);
        if (ok) ds.current = slowScale;
        lua_pushboolean(L, ok ? 1 : 0);
    } else {
        bool ok = Director_SetTimeDilation(ds.restoreTo);
        if (ok) ds.current = ds.restoreTo;
        lua_pushboolean(L, ok ? 1 : 0);
    }
    return 1;
}

static int Lua_DirectorToggleFreeze(lua_State* L) {
    DirectorState& ds = GetDirectorState();
    ds.freeze = !ds.freeze;
    if (ds.freeze) {
        ds.restoreTo = ds.current;
        bool ok = Director_SetTimeDilation(0.0001f);
        if (ok) ds.current = 0.0001f;
        lua_pushboolean(L, ok ? 1 : 0);
    } else {
        bool ok = Director_SetTimeDilation(ds.restoreTo);
        if (ok) ds.current = ds.restoreTo;
        lua_pushboolean(L, ok ? 1 : 0);
    }
    return 1;
}

struct RecordEntry {
    double t;
    std::string kind;
    std::string text;
};

static std::vector<RecordEntry>& GetRecordBuffer() {
    static std::vector<RecordEntry> s;
    return s;
}

static std::vector<std::string>& GetWatchPatterns() {
    static std::vector<std::string> s = { "Enemy", "Boss", "Pickup", "Projectile" };
    return s;
}

static int Lua_Record(lua_State* L) {
    const char* kind = luaL_checkstring(L, 1);
    const char* text = luaL_optstring(L, 2, "");
    auto& buf = GetRecordBuffer();
    if (buf.size() >= 5000) {
        buf.erase(buf.begin());
    }
    buf.push_back({ static_cast<double>(GetTickCount64()) / 1000.0, kind ? kind : "", text ? text : "" });
    return 0;
}

static int Lua_Watched(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    if (!name) { lua_pushnil(L); return 1; }
    for (const auto& pat : GetWatchPatterns()) {
        if (strstr(name, pat.c_str())) {
            lua_pushstring(L, pat.c_str());
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int Lua_SetWatchPatterns(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    auto& patterns = GetWatchPatterns();
    patterns.clear();
    int n = (int)lua_rawlen(L, 1);
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, 1, i);
        const char* s = lua_tostring(L, -1);
        if (s) patterns.emplace_back(s);
        lua_pop(L, 1);
    }
    return 0;
}

static int Lua_GetRecords(lua_State* L) {
    auto& buf = GetRecordBuffer();
    lua_createtable(L, (int)buf.size(), 0);
    for (size_t i = 0; i < buf.size(); ++i) {
        lua_createtable(L, 0, 3);
        lua_pushnumber(L, buf[i].t); lua_setfield(L, -2, "t");
        lua_pushstring(L, buf[i].kind.c_str()); lua_setfield(L, -2, "kind");
        lua_pushstring(L, buf[i].text.c_str()); lua_setfield(L, -2, "text");
        lua_rawseti(L, -2, (int)i + 1);
    }
    return 1;
}
