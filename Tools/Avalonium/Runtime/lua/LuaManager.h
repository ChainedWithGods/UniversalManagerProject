#pragma once

#include "../core/Types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <cstdint>
#include <mutex>

struct lua_State;

class LuaManager {
public:
    static LuaManager& Get();

    bool Initialize(HMODULE hModule);
    void Shutdown();
    bool IsInitialized() const { return m_Initialized; }

    bool ExecuteString(const std::string& script, std::string& outResult);
    bool ExecuteStringJson(const std::string& script, std::string& outResult);

    bool GetPropertyJson(void* object, const std::string& path, std::string& outResult);
    bool SetPropertyJson(void* object, const std::string& path, const std::string& jsonValue, std::string& outResult);
    bool ExecuteFile(const std::string& filePath, std::string& outResult);

    int SubscribeIpcEvent(const std::string& kind, const std::string& filter, uint64_t token);
    bool UnsubscribeIpcEvent(int subscriptionId);

    void LoadModsFromDirectory(const std::string& modsDirectory);
    void ReloadMods();
    void HotReloadMods() { ReloadMods(); }

    std::string GetModsDirectory() const { return m_ModsDirectory; }
    const std::vector<std::string>& GetLoadedMods() const { return m_LoadedMods; }

    lua_State* GetState() const { return m_L; }
    std::recursive_mutex& GetLuaMutex() { return m_LuaMutex; }

    bool IsLoadingMods() const { return m_LoadingMods.load(std::memory_order_acquire); }

    static UObject* FindEngineCDO(const char* className);

private:
    LuaManager();
    ~LuaManager();

    LuaManager(const LuaManager&) = delete;
    LuaManager& operator=(const LuaManager&) = delete;

    void RegisterBindings();
    void RegisterGlobalHelpers();
    void RegisterEngineGlobals();
    void RegisterUETable();
    void RegisterMemoryBindings();

    static void RegisterMemorySnakeCaseNames(lua_State* L);

    static int Lua_Print(lua_State* L);
    static int Lua_Log(lua_State* L);
    static int Lua_ReloadMods(lua_State* L);

    static int Lua_FindFirstOf(lua_State* L);
    static int Lua_FindAllOf(lua_State* L);
    static int Lua_StaticFindObject(lua_State* L);
    static int Lua_StaticConstructObject(lua_State* L);

    static int Lua_GetPlayer(lua_State* L);
    static int Lua_GetPlayerController(lua_State* L);
    static int Lua_GetPlayerCameraManager(lua_State* L);
    static int Lua_GetCameraInfo(lua_State* L);
    static int Lua_ProjectWorldToScreen(lua_State* L);
    static int Lua_DeprojectScreenToWorld(lua_State* L);
    static int Lua_GetMousePosition(lua_State* L);
    static int Lua_GetPlayerState(lua_State* L);
    static int Lua_GetComponentByClass(lua_State* L);
    static int Lua_GetAllComponents(lua_State* L);
    static int Lua_LineTraceSingle(lua_State* L);
    static int Lua_GetActorBounds(lua_State* L);
    static int Lua_GetActorLocation(lua_State* L);
    static int Lua_SetActorLocation(lua_State* L);
    static int Lua_GetViewportSize(lua_State* L);
    static int Lua_GetLocalPlayer(lua_State* L);
    static int Lua_GetGameInstance(lua_State* L);
    static int Lua_GetGameMode(lua_State* L);
    static int Lua_GetGameState(lua_State* L);
    static int Lua_GetAllActorsOfClass(lua_State* L);
    static int Lua_GetDistance(lua_State* L);

    static int Lua_GetGameplayStatics(lua_State* L);
    static int Lua_GetKismetSystemLibrary(lua_State* L);
    static int Lua_GetKismetMathLibrary(lua_State* L);
    static int Lua_GetKismetStringLibrary(lua_State* L);
    static int Lua_GetKismetArrayLibrary(lua_State* L);
    static int Lua_GetKismetTextLibrary(lua_State* L);
    static int Lua_GetKismetGuidLibrary(lua_State* L);
    static int Lua_GetKismetRenderingLibrary(lua_State* L);

    static int Lua_GetCVar(lua_State* L);
    static int Lua_SetCVar(lua_State* L);
    static int Lua_FindCVars(lua_State* L);

    static int Lua_SpawnActorByClass(lua_State* L);
    static int Lua_GetAllPawns(lua_State* L);
    static int Lua_WithWorld(lua_State* L);
    static int Lua_WhenClassSpawns(lua_State* L);

    static int Lua_File_Read(lua_State* L);
    static int Lua_File_Write(lua_State* L);
    static int Lua_File_Append(lua_State* L);
    static int Lua_File_Exists(lua_State* L);
    static int Lua_File_List(lua_State* L);
    static int Lua_File_CreateDir(lua_State* L);
    static int Lua_File_Delete(lua_State* L);

    static int Lua_JSON_Parse(lua_State* L);
    static int Lua_JSON_Stringify(lua_State* L);

    static int Lua_GetFrameTime(lua_State* L);
    static int Lua_GetFPS(lua_State* L);
    static int Lua_GetDeltaSeconds(lua_State* L);
    static int Lua_GetMemoryUsage(lua_State* L);
    static int Lua_ForceGarbageCollection(lua_State* L);

    static int Lua_GetModDirectory(lua_State* L);

    static int Lua_GetWorld(lua_State* L);
    static int Lua_GetEngine(lua_State* L);
    static int Lua_GetObjectCount(lua_State* L);
    static int Lua_GetObjectByIndex(lua_State* L);
    static int Lua_FindObject(lua_State* L);
    static int Lua_FindObjects(lua_State* L);
    static int Lua_FindClass(lua_State* L);
    static int Lua_GetWorldActors(lua_State* L);
    static int Lua_GetObjectName(lua_State* L);
    static int Lua_GetObjectFullName(lua_State* L);
    static int Lua_GetObjectClass(lua_State* L);
    static int Lua_GetSuperClass(lua_State* L);
    static int Lua_InheritsFrom(lua_State* L);
    static int Lua_GetClassProperties(lua_State* L);
    static int Lua_CallFunction(lua_State* L);
    static int Lua_ExportProperty(lua_State* L);
    static int Lua_ImportProperty(lua_State* L);
    static int Lua_GetVersion(lua_State* L);
    static int Lua_GetGameName(lua_State* L);
    static int Lua_GetGlobals(lua_State* L);
    static int Lua_Memory_IsValidPtr(lua_State* L);

    static int Lua_ReadInt8(lua_State* L);
    static int Lua_ReadInt16(lua_State* L);
    static int Lua_ReadInt32(lua_State* L);
    static int Lua_ReadInt64(lua_State* L);
    static int Lua_ReadUInt8(lua_State* L);
    static int Lua_ReadUInt16(lua_State* L);
    static int Lua_ReadUInt32(lua_State* L);
    static int Lua_ReadUInt64(lua_State* L);
    static int Lua_ReadFloat(lua_State* L);
    static int Lua_ReadDouble(lua_State* L);
    static int Lua_ReadPointer(lua_State* L);
    static int Lua_ReadBool(lua_State* L);
    static int Lua_ReadString(lua_State* L);
    static int Lua_ReadFString(lua_State* L);
    static int Lua_ReadFName(lua_State* L);

    static int Lua_WriteInt8(lua_State* L);
    static int Lua_WriteInt16(lua_State* L);
    static int Lua_WriteInt32(lua_State* L);
    static int Lua_WriteInt64(lua_State* L);
    static int Lua_WriteUInt8(lua_State* L);
    static int Lua_WriteUInt16(lua_State* L);
    static int Lua_WriteUInt32(lua_State* L);
    static int Lua_WriteUInt64(lua_State* L);
    static int Lua_WriteFloat(lua_State* L);
    static int Lua_WriteDouble(lua_State* L);
    static int Lua_WritePointer(lua_State* L);
    static int Lua_WriteBool(lua_State* L);
    static int Lua_IpcEventCallback(lua_State* L);

    struct IpcSubscription {
        uint32_t hookId = 0;
        int callbackRef = 0;
        std::string kind;
        std::string filter;
        uint64_t token = 0;
    };
    std::unordered_map<int, IpcSubscription> m_IpcSubscriptions;
    int m_NextIpcSubscription = 1;

    lua_State* m_L = nullptr;
    HMODULE m_Module = nullptr;
    std::string m_ModsDirectory;
    std::vector<std::string> m_LoadedMods;
    bool m_Initialized = false;
    std::recursive_mutex m_LuaMutex;
    std::atomic<bool> m_LoadingMods{ false };
};
