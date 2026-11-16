#include "sdk/Avalonium/ModApi.h"
#include "sdk/Avalonium/SdkApi.h"

#include "CppModContext.h"
#include "CppModAssets.h"
#include "CppModContainers.h"
#include "CppModHooks.h"
#include "CppModImGui.h"
#include "CppModManager.h"
#include "CppModNative.h"
#include "CppModReflection.h"
#include "CppModValue.h"

#include <imgui.h>

#include <d3d11.h>
#include <d3d12.h>

#include "d3d12hook_api.h"

#include "../console/ConsoleInternal.h"
#include "../core/BlueprintDiscovery.h"
#include "../core/EngineDiscovery.h"
#include "../core/FunctionFinder.h"
#include "../core/Logger.h"
#include "../core/NameResolution.h"
#include "../core/ObjectArray.h"
#include "../core/ObjectIndex.h"
#include "../core/ObjectUtils.h"
#include "../core/WorldDiscovery.h"
#include "../lua/LuaHookManager.h"
#include "../utils/SafeMemory.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

extern EngineState g_State;

namespace Avalonium::CppMods
{
    namespace
    {
        thread_local std::string g_NameScratch;

        const char* CopyName(std::string text)
        {
            g_NameScratch = std::move(text);
            return g_NameScratch.c_str();
        }

        void LogLine(const std::string& line)
        {
            EngineDiscovery::WriteLogLine(line.c_str());
        }

        void LogWarningOnce(const char* key, const char* message)
        {
            static std::mutex mutex;
            static std::unordered_set<std::string> seen;
            std::lock_guard<std::mutex> lock(mutex);
            if (seen.insert(key).second) LogLine(message);
        }

        bool EnsureGameThread(const char* what);

        int32_t ApiGetEngineMajor()
        {
            return g_State.UEMajor;
        }

        int32_t ApiGetEngineMinor()
        {
            return g_State.UEMinor;
        }

        const char* ApiGetGameName()
        {
            return g_State.GameName[0] ? g_State.GameName : "Unknown";
        }

        const char* ApiGetRuntimeVersion()
        {
            return "Avalonium C++ Mod API";
        }

        const char* ApiGetModDirectory()
        {
            void* token = CurrentModToken();
            if (!token) return "";
            auto* mod = static_cast<LoadedMod*>(token);
            return CopyName(mod->FolderPath);
        }

        void ApiLogMessage(int32_t level, const char* message)
        {
            if (!message) return;

            const char* prefix = "[CppMod] ";
            switch (level)
            {
            case AVALONIUM_LOG_DEBUG: prefix = "[CppMod] Debug: "; break;
            case AVALONIUM_LOG_WARNING: prefix = "[CppMod] Warning: "; break;
            case AVALONIUM_LOG_ERROR: prefix = "[CppMod] Error: "; break;
            default: break;
            }

            std::string line = prefix;
            line += message;
            LogLine(line);
        }

        bool ApiIsInGameThread()
        {
            return LuaEngine::LuaHookManager::Get().IsInGameThread();
        }

        bool ApiRunOnGameThread(AvaloniumTaskCallback callback, void* userData)
        {
            return CppModManager::Get().RunOnGameThread(callback, userData);
        }

        double ApiGetDeltaSeconds()
        {
            return LuaEngine::LuaHookManager::Get().GetDeltaSeconds();
        }

        uint64_t ApiGetFrameCount()
        {
            return LuaEngine::LuaHookManager::Get().GetFrameCount();
        }

        void* ApiFindObject(const char* path)
        {
            return path ? FindObject(g_State, path) : nullptr;
        }

        void* ApiFindClass(const char* name)
        {
            return name ? FindClass(g_State, name) : nullptr;
        }

        void* ApiFindFirstObjectOfClass(void* objectClass)
        {
            return FindFirstObjectOfClass(g_State, static_cast<UClass*>(objectClass));
        }

        void* ApiGetClassDefaultObject(void* objectClass)
        {
            return GetClassDefaultObject(g_State, static_cast<UClass*>(objectClass));
        }

        int32_t ApiFindObjectsOfClass(void* objectClass, void** outObjects, int32_t maxObjects)
        {
            if (!outObjects || maxObjects <= 0) return 0;
            std::vector<UObject*> matches;
            const int32_t count =
                FindObjectsOfClass(g_State, static_cast<UClass*>(objectClass), matches, maxObjects);
            for (int32_t i = 0; i < count; ++i)
                outObjects[i] = matches[static_cast<size_t>(i)];
            return count;
        }

        int32_t ApiGetObjectCount()
        {
            return EngineDiscovery::GetObjectCount(g_State);
        }

        void* ApiGetObjectByIndex(int32_t index)
        {
            return EngineDiscovery::GetObjectByIndex(g_State, index);
        }

        int32_t ApiGetObjectIndex(void* object)
        {
            if (!object || g_State.UObject_IndexOff < 0) return -1;
            int32_t index = -1;
            if (!SafeMem_Read(reinterpret_cast<uint8_t*>(object) + g_State.UObject_IndexOff,
                              &index, sizeof(index)))
            {
                return -1;
            }
            return index;
        }

        void* ApiConstructObject(void* objectClass, void* outer)
        {
            if (!objectClass) return nullptr;
            if (!EnsureGameThread("construct_object")) return nullptr;

            UClass* cls = static_cast<UClass*>(objectClass);
            UClass* metaClass =
                EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(cls));
            if (!metaClass ||
                !EngineDiscovery::IsStructA(g_State,
                                            reinterpret_cast<UStruct*>(metaClass),
                                            reinterpret_cast<UStruct*>(g_State.UClassClass)))
            {
                LogWarningOnce("construct-not-class",
                               "[CppMod] Warning: construct_object was given a pointer that is "
                               "not a class");
                return nullptr;
            }

            UObject* outerObject = static_cast<UObject*>(outer);
            if (!outerObject)
            {
                outerObject = EngineDiscovery::GetLiveWorld(g_State);
            }
            if (!outerObject)
            {
                LogWarningOnce("construct-no-outer",
                               "[CppMod] Warning: construct_object has no outer and no world "
                               "is live");
                return nullptr;
            }

            UObject* created =
                EngineDiscovery::SpawnObjectThroughEngine(g_State, cls, outerObject);
            if (!created) created = ConsoleEnabler::TrySEHConstruct(g_State, cls, outerObject);
            return created;
        }

        void* ApiGetWorld()
        {
            return EngineDiscovery::GetLiveWorld(g_State);
        }

        bool ApiIsValidObject(void* object)
        {
            return object && SafeMem_IsReadable(object, 0x20) != 0;
        }

        bool ApiIsPendingKill(void* object)
        {
            return object && EngineDiscovery::IsPendingKillObject(
                                 g_State, static_cast<UObject*>(object));
        }

        bool ApiIsModAsset(void* object)
        {
            return object && IsModAsset(g_State, static_cast<UObject*>(object));
        }

        const char* ApiGetObjectName(void* object)
        {
            if (!object) return "";
            return CopyName(EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(object)));
        }

        const char* ApiGetFNameString(int32_t comparisonIndex, int32_t number)
        {
            FName name{};
            name.ComparisonIndex = static_cast<uint32_t>(comparisonIndex);
            name.Number = static_cast<uint32_t>(number);
            return CopyName(EngineDiscovery::FNameToStr(g_State, name));
        }

        const char* ApiGetObjectFullName(void* object)
        {
            if (!object) return "";
            return CopyName(EngineDiscovery::BuildFullName(g_State, static_cast<UObject*>(object)));
        }

        const char* ApiGetObjectPathName(void* object)
        {
            if (!object) return "";
            return CopyName(EngineDiscovery::BuildPathName(g_State, static_cast<UObject*>(object)));
        }

        void* ApiGetObjectClass(void* object)
        {
            return object ? EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(object))
                          : nullptr;
        }

        void* ApiGetObjectOuter(void* object)
        {
            return object ? EngineDiscovery::GetObjectOuter(g_State, static_cast<UObject*>(object))
                          : nullptr;
        }

        void* ApiGetSuperClass(void* objectClass)
        {
            return objectClass
                       ? EngineDiscovery::GetSuperStruct(g_State,
                                                         static_cast<UStruct*>(objectClass))
                       : nullptr;
        }

        bool ApiIsA(void* object, const char* className)
        {
            if (!object || !className || !*className) return false;

            UClass* target = FindClass(g_State, className);
            if (target)
            {
                UClass* objectClass =
                    EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(object));
                return objectClass &&
                       EngineDiscovery::IsStructA(g_State,
                                                  reinterpret_cast<UStruct*>(objectClass),
                                                  reinterpret_cast<UStruct*>(target));
            }

            UClass* current =
                EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(object));
            for (int depth = 0; current && depth < 64; ++depth)
            {
                std::string name =
                    EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(current));
                if (_stricmp(name.c_str(), className) == 0) return true;
                if (name.size() > 2 && name.compare(name.size() - 2, 2, "_C") == 0)
                {
                    name.resize(name.size() - 2);
                    if (_stricmp(name.c_str(), className) == 0) return true;
                }
                current = static_cast<UClass*>(
                    EngineDiscovery::GetSuperStruct(g_State,
                                                    reinterpret_cast<UStruct*>(current)));
            }
            return false;
        }

        void* ApiFindFunction(void* objectClass, const char* functionName)
        {
            return (objectClass && functionName)
                       ? FindFunction(g_State, static_cast<UClass*>(objectClass), functionName)
                       : nullptr;
        }

        int32_t ApiGetClassProperties(void* objectClass, AvaloniumPropertyInfo* outProperties,
                                      int32_t maxProperties)
        {
            return GetClassProperties(g_State, static_cast<UClass*>(objectClass), outProperties,
                                      maxProperties);
        }

        int32_t ApiGetClassFunctions(void* objectClass, AvaloniumFunctionInfo* outFunctions,
                                     int32_t maxFunctions)
        {
            return GetClassFunctions(g_State, static_cast<UClass*>(objectClass), outFunctions,
                                     maxFunctions);
        }

        uint32_t ApiGetFunctionFlags(void* function)
        {
            return GetFunctionFlags(g_State, function);
        }

        int32_t ApiGetFunctionFrameSize(void* function)
        {
            return GetFunctionFrameSize(g_State, function);
        }

        bool ApiGetProperty(void* object, const char* propertyName, AvaloniumValue* outValue)
        {
            if (!outValue) return false;
            SetNoneValue(*outValue);
            if (!object || !propertyName) return false;
            return ReadProperty(g_State, static_cast<UObject*>(object), propertyName, *outValue);
        }

        bool ApiSetProperty(void* object, const char* propertyName, const AvaloniumValue* value)
        {
            if (!object || !propertyName || !value) return false;
            if (!EnsureGameThread("set_property")) return false;
            return WriteProperty(g_State, static_cast<UObject*>(object), propertyName, *value);
        }

        bool ApiGetFunctionArgument(void* function, void* frame, const char* parameterName,
                                    AvaloniumValue* outValue)
        {
            if (!outValue) return false;
            SetNoneValue(*outValue);
            if (!function || !parameterName) return false;
            const bool ok = ReadFunctionArgument(g_State, function, frame, parameterName, *outValue);
            if (ok) StabilizeStructValue(*outValue);
            return ok;
        }

        bool ApiSetFunctionArgument(void* function, void* frame, const char* parameterName,
                                    const AvaloniumValue* value)
        {
            if (!function || !parameterName || !value) return false;
            if (!EnsureGameThread("set_function_argument")) return false;
            return WriteFunctionArgument(g_State, function, frame, parameterName, *value);
        }

        bool ApiGetHookResult(void* function, void* result, AvaloniumValue* outValue)
        {
            if (!outValue) return false;
            SetNoneValue(*outValue);
            if (!function || !result) return false;
            const bool ok = ReadHookResult(g_State, function, result, *outValue);
            if (ok) StabilizeStructValue(*outValue);
            return ok;
        }

        bool EnsureGameThread(const char* what)
        {
            LuaEngine::LuaHookManager& hookManager = LuaEngine::LuaHookManager::Get();
            if (hookManager.GetGameThreadId() == 0)
            {
                LogWarningOnce("game-thread-capture",
                               "[CppMod] Warning: a reflected call was refused because the engine's "
                               "game thread has not been observed yet; queue it with "
                               "RC::RunOnGameThread");
                return false;
            }
            if (!hookManager.IsInGameThread())
            {
                std::string line = "[CppMod] Warning: ";
                line += what;
                line += " was refused off the game thread";
                LogWarningOnce("game-thread-refusal", line.c_str());
                return false;
            }
            return true;
        }

        bool ApiCallFunction(void* object, const char* functionName, const AvaloniumValue* args,
                             int32_t argCount, AvaloniumValue* outResult)
        {
            if (outResult) SetNoneValue(*outResult);
            if (!object || !functionName) return false;
            if (!EnsureGameThread("call_function")) return false;

            AvaloniumValue result{};
            const bool ok = CallFunction(g_State, static_cast<UObject*>(object), functionName,
                                         args, argCount, result);
            if (ok && outResult) *outResult = result;
            return ok;
        }

        bool ApiCallFunctionOut(void* object, const char* functionName,
                                const AvaloniumValue* args, int32_t argCount,
                                const char* const* outNames, AvaloniumValue* outValues,
                                int32_t outCount, AvaloniumValue* outResult)
        {
            if (outResult) SetNoneValue(*outResult);
            if (outValues && outCount > 0)
            {
                for (int32_t i = 0; i < outCount; ++i) SetNoneValue(outValues[i]);
            }
            if (!object || !functionName || outCount < 0) return false;
            if (!EnsureGameThread("call_function_out")) return false;

            AvaloniumValue result{};
            const bool ok = CallFunctionOut(g_State, static_cast<UObject*>(object), functionName,
                                            args, argCount, outNames, outValues, outCount,
                                            result);
            if (ok && outResult) *outResult = result;
            return ok;
        }

        bool ApiProcessEvent(void* object, void* function, void* params)
        {
            if (!object || !function) return false;
            if (!EnsureGameThread("process_event")) return false;
            return ProcessEvent(g_State, static_cast<UObject*>(object),
                                static_cast<UFunction*>(function), params);
        }

        uint32_t ApiRegisterHook(const char* className, const char* functionName,
                                 AvaloniumHookCallback preCallback,
                                 AvaloniumHookCallback postCallback, void* userData)
        {
            if (!functionName) return 0;
            if (!CppModManager::Get().IsRunning()) return 0;

            void* owner = CurrentModToken();
            if (owner)
            {
                auto* mod = static_cast<LoadedMod*>(owner);
                if (!mod->Alive.load(std::memory_order_acquire)) return 0;
            }
            if (!owner)
            {
                LogLine("[CppMod] Warning: a hook was registered outside a mod callback; "
                        "it will not be removed automatically when a mod unloads");
            }

            std::string why;
            const uint32_t id = RegisterHook(g_State, className ? className : "", functionName,
                                             preCallback, postCallback, userData, owner, &why);
            if (!id && !why.empty())
                LogLine("[CppMod] Warning: hook registration failed: " + why);
            return id;
        }

        uint32_t ApiRegisterHookVeto(const char* className, const char* functionName,
                                     AvaloniumVetoCallback vetoCallback, void* userData)
        {
            if (!functionName || !vetoCallback) return 0;
            if (!CppModManager::Get().IsRunning()) return 0;

            void* owner = CurrentModToken();
            if (owner)
            {
                auto* mod = static_cast<LoadedMod*>(owner);
                if (!mod->Alive.load(std::memory_order_acquire)) return 0;
            }
            if (!owner)
            {
                LogLine("[CppMod] Warning: a hook veto was registered outside a mod callback; "
                        "it will not be removed automatically when a mod unloads");
            }

            std::string why;
            const uint32_t id =
                RegisterHookVeto(g_State, className ? className : "", functionName,
                                 vetoCallback, userData, owner, &why);
            if (!id && !why.empty())
                LogLine("[CppMod] Warning: hook veto registration failed: " + why);
            return id;
        }

        uint32_t ApiRegisterAddressVeto(void* target, AvaloniumVetoCallback vetoCallback,
                                        void* userData)
        {
            if (!target || !vetoCallback) return 0;
            if (!CppModManager::Get().IsRunning()) return 0;

            void* owner = CurrentModToken();
            if (owner)
            {
                auto* mod = static_cast<LoadedMod*>(owner);
                if (!mod->Alive.load(std::memory_order_acquire)) return 0;
            }

            std::string why;
            const uint32_t id = RegisterAddressVeto(g_State, target, vetoCallback, userData,
                                                    owner, &why);
            if (!id && !why.empty())
                LogLine("[CppMod] Warning: address veto registration failed: " + why);
            return id;
        }

        uint32_t ApiRegisterHookFunction(void* function, AvaloniumHookCallback preCallback,
                                         AvaloniumHookCallback postCallback, void* userData)
        {
            if (!function) return 0;
            if (!CppModManager::Get().IsRunning()) return 0;

            void* owner = CurrentModToken();
            if (owner)
            {
                auto* mod = static_cast<LoadedMod*>(owner);
                if (!mod->Alive.load(std::memory_order_acquire)) return 0;
            }
            if (!owner)
            {
                LogLine("[CppMod] Warning: a hook was registered outside a mod callback; "
                        "it will not be removed automatically when a mod unloads");
            }

            std::string why;
            const uint32_t id = RegisterHookByFunction(
                g_State, static_cast<UFunction*>(function), preCallback, postCallback, userData,
                owner, &why);
            if (!id && !why.empty())
                LogLine("[CppMod] Warning: hook registration failed: " + why);
            return id;
        }

        bool ApiUnregisterHook(uint32_t hookId)
        {
            return UnregisterHook(hookId);
        }

        uint32_t ApiRegisterKeyBind(const char* keyName, AvaloniumKeyCallback callback,
                                    void* userData)
        {
            if (!keyName) return 0;
            return CppModManager::Get().RegisterKeyBind(keyName, callback, userData);
        }

        bool ApiUnregisterKeyBind(uint32_t keyId)
        {
            return CppModManager::Get().UnregisterKeyBind(keyId);
        }

        uint32_t ApiBindWidgetClick(void* widget, AvaloniumWidgetCallback callback,
                                    void* userData)
        {
            if (!widget || !callback) return 0;
            return CppModManager::Get().BindWidgetClick(widget, callback, userData);
        }

        bool ApiUnbindWidgetClick(uint32_t bindingId)
        {
            return CppModManager::Get().UnbindWidgetClick(bindingId);
        }

        uint32_t ApiBindWidgetEvent(void* widget, const char* eventName,
                                    AvaloniumWidgetCallback callback, void* userData)
        {
            if (!widget || !eventName || !callback) return 0;
            return CppModManager::Get().BindWidgetEvent(widget, eventName, callback, userData);
        }

        int32_t ApiArrayLength(void* object, const char* propertyName)
        {
            if (!object || !propertyName) return -1;
            if (!EnsureGameThread("array_length")) return -1;
            return Containers::ArrayLength(g_State, static_cast<UObject*>(object),
                                           propertyName);
        }

        int32_t ApiArrayElementSize(void* object, const char* propertyName)
        {
            if (!object || !propertyName) return 0;
            if (!EnsureGameThread("array_element_size")) return 0;
            return Containers::ArrayElementSize(g_State, static_cast<UObject*>(object),
                                                propertyName);
        }

        bool ApiArrayGet(void* object, const char* propertyName, int32_t index,
                         AvaloniumValue* outValue)
        {
            if (outValue) SetNoneValue(*outValue);
            if (!object || !propertyName || !outValue) return false;
            if (!EnsureGameThread("array_get")) return false;
            return Containers::ArrayGet(g_State, static_cast<UObject*>(object), propertyName,
                                        index, *outValue);
        }

        bool ApiArraySet(void* object, const char* propertyName, int32_t index,
                         const AvaloniumValue* value)
        {
            if (!object || !propertyName || !value) return false;
            if (!EnsureGameThread("array_set")) return false;
            return Containers::ArraySet(g_State, static_cast<UObject*>(object), propertyName,
                                        index, *value);
        }

        bool ApiArrayAdd(void* object, const char* propertyName, const AvaloniumValue* value)
        {
            if (!object || !propertyName || !value) return false;
            if (!EnsureGameThread("array_add")) return false;
            return Containers::ArrayAdd(g_State, static_cast<UObject*>(object), propertyName,
                                        *value);
        }

        bool ApiArrayRemove(void* object, const char* propertyName, int32_t index)
        {
            if (!object || !propertyName) return false;
            if (!EnsureGameThread("array_remove")) return false;
            return Containers::ArrayRemove(g_State, static_cast<UObject*>(object), propertyName,
                                           index);
        }

        int32_t ApiMapLength(void* object, const char* propertyName)
        {
            if (!object || !propertyName) return -1;
            if (!EnsureGameThread("map_length")) return -1;
            return Containers::MapLength(g_State, static_cast<UObject*>(object), propertyName);
        }

        bool ApiMapGetAt(void* object, const char* propertyName, int32_t index,
                         AvaloniumValue* outKey, AvaloniumValue* outValue)
        {
            if (outKey) SetNoneValue(*outKey);
            if (outValue) SetNoneValue(*outValue);
            if (!object || !propertyName || !outKey || !outValue) return false;
            if (!EnsureGameThread("map_get_at")) return false;
            return Containers::MapGetAt(g_State, static_cast<UObject*>(object), propertyName,
                                        index, *outKey, *outValue);
        }

        bool ApiMapSetValueAt(void* object, const char* propertyName, int32_t index,
                              const AvaloniumValue* value)
        {
            if (!object || !propertyName || !value) return false;
            if (!EnsureGameThread("map_set_value_at")) return false;
            return Containers::MapSetValueAt(g_State, static_cast<UObject*>(object),
                                             propertyName, index, *value);
        }

        int32_t ApiSetLength(void* object, const char* propertyName)
        {
            if (!object || !propertyName) return -1;
            if (!EnsureGameThread("set_length")) return -1;
            return Containers::SetLength(g_State, static_cast<UObject*>(object), propertyName);
        }

        bool ApiSetGetAt(void* object, const char* propertyName, int32_t index,
                         AvaloniumValue* outValue)
        {
            if (outValue) SetNoneValue(*outValue);
            if (!object || !propertyName || !outValue) return false;
            if (!EnsureGameThread("set_get_at")) return false;
            return Containers::SetGetAt(g_State, static_cast<UObject*>(object), propertyName,
                                        index, *outValue);
        }

        bool ApiMapAdd(void* object, const char* propertyName, const AvaloniumValue* key,
                       const AvaloniumValue* value)
        {
            if (!object || !propertyName || !key || !value) return false;
            if (!EnsureGameThread("map_add")) return false;
            return Containers::MapAdd(g_State, static_cast<UObject*>(object), propertyName,
                                      *key, *value);
        }

        bool ApiMapRemove(void* object, const char* propertyName, const AvaloniumValue* key)
        {
            if (!object || !propertyName || !key) return false;
            if (!EnsureGameThread("map_remove")) return false;
            return Containers::MapRemove(g_State, static_cast<UObject*>(object), propertyName,
                                         *key);
        }

        bool ApiMapClear(void* object, const char* propertyName)
        {
            if (!object || !propertyName) return false;
            if (!EnsureGameThread("map_clear")) return false;
            return Containers::MapClear(g_State, static_cast<UObject*>(object), propertyName);
        }

        bool ApiMapContains(void* object, const char* propertyName, const AvaloniumValue* key)
        {
            if (!object || !propertyName || !key) return false;
            if (!EnsureGameThread("map_contains")) return false;
            return Containers::MapContains(g_State, static_cast<UObject*>(object), propertyName,
                                           *key);
        }

        bool ApiMapFind(void* object, const char* propertyName, const AvaloniumValue* key,
                        AvaloniumValue* outValue)
        {
            if (outValue) SetNoneValue(*outValue);
            if (!object || !propertyName || !key || !outValue) return false;
            if (!EnsureGameThread("map_find")) return false;
            if (!Containers::MapFind(g_State, static_cast<UObject*>(object), propertyName,
                                     *key, *outValue))
            {
                return false;
            }
            StabilizeStructValue(*outValue);
            return true;
        }

        bool ApiSetAdd(void* object, const char* propertyName, const AvaloniumValue* value)
        {
            if (!object || !propertyName || !value) return false;
            if (!EnsureGameThread("set_add")) return false;
            return Containers::SetAdd(g_State, static_cast<UObject*>(object), propertyName,
                                      *value);
        }

        bool ApiSetRemove(void* object, const char* propertyName, const AvaloniumValue* value)
        {
            if (!object || !propertyName || !value) return false;
            if (!EnsureGameThread("set_remove")) return false;
            return Containers::SetRemove(g_State, static_cast<UObject*>(object), propertyName,
                                         *value);
        }

        bool ApiSetClear(void* object, const char* propertyName)
        {
            if (!object || !propertyName) return false;
            if (!EnsureGameThread("set_clear")) return false;
            return Containers::SetClear(g_State, static_cast<UObject*>(object), propertyName);
        }

        bool ApiSetContains(void* object, const char* propertyName, const AvaloniumValue* value)
        {
            if (!object || !propertyName || !value) return false;
            if (!EnsureGameThread("set_contains")) return false;
            return Containers::SetContains(g_State, static_cast<UObject*>(object), propertyName,
                                           *value);
        }

        bool ApiGetPropertyPath(void* object, const char* path, AvaloniumValue* outValue)
        {
            if (outValue) SetNoneValue(*outValue);
            if (!object || !path || !outValue) return false;
            if (!EnsureGameThread("get_property_path")) return false;
            if (!Containers::ReadPath(g_State, static_cast<UObject*>(object), path, *outValue))
                return false;
            StabilizeStructValue(*outValue);
            return true;
        }

        bool ApiSetPropertyPath(void* object, const char* path, const AvaloniumValue* value)
        {
            if (!object || !path || !value) return false;
            if (!EnsureGameThread("set_property_path")) return false;
            return Containers::WritePath(g_State, static_cast<UObject*>(object), path, *value);
        }

        void* ApiGetModuleHandle(const char* moduleName)
        {
            return Native::ModuleHandle(moduleName);
        }

        void* ApiFindExport(void* module, const char* symbolName)
        {
            return Native::FindExport(module, symbolName);
        }

        uint64_t ApiFindPattern(void* module, const char* pattern, bool executableOnly)
        {
            return Native::FindPattern(module, pattern, executableOnly);
        }

        uint64_t ApiResolveRelative(uint64_t instruction, int32_t displacementOffset,
                                    int32_t instructionLength)
        {
            return Native::ResolveRelative(instruction, displacementOffset, instructionLength);
        }

        void* ApiGetVtable(void* object)
        {
            return Native::GetVtable(object);
        }

        void* ApiGetVirtualFunction(void* object, int32_t slot)
        {
            return Native::GetVirtualFunction(object, slot);
        }

        void* ApiGetImGuiContext()
        {
            return ImGui::GetCurrentContext();
        }

        void ApiGetImGuiAllocatorFunctions(AvaloniumImGuiAlloc* outAlloc,
                                           AvaloniumImGuiFree* outFree, void** outUserData)
        {
            ImGuiMemAllocFunc allocFunc = nullptr;
            ImGuiMemFreeFunc freeFunc = nullptr;
            void* userData = nullptr;
            ImGui::GetAllocatorFunctions(&allocFunc, &freeFunc, &userData);

            if (outAlloc) *outAlloc = reinterpret_cast<AvaloniumImGuiAlloc>(allocFunc);
            if (outFree) *outFree = reinterpret_cast<AvaloniumImGuiFree>(freeFunc);
            if (outUserData) *outUserData = userData;
        }

        uint32_t ApiRegisterImGuiRender(const char* name, AvaloniumImGuiCallback callback,
                                        void* userData)
        {
            if (!callback) return 0;
            return ImGuiRender::Register(name, callback, userData);
        }

        bool ApiUnregisterImGuiRender(uint32_t id)
        {
            return ImGuiRender::Unregister(id);
        }

        bool ApiSetImGuiInputActive(uint32_t id, bool active)
        {
            return ImGuiRender::SetInputActive(id, active);
        }

        bool ModuleImageSize(HMODULE module, uintptr_t& base, size_t& size)
        {
            if (!module) return false;
            base = reinterpret_cast<uintptr_t>(module);
            auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(module);
            if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
            auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
            if (!nt || nt->Signature != IMAGE_NT_SIGNATURE) return false;
            size = nt->OptionalHeader.SizeOfImage;
            return true;
        }

        ID3D11ShaderResourceView* QueryD3D11ResourceView(uintptr_t candidate)
        {
            ID3D11ShaderResourceView* view = nullptr;
            __try
            {
                auto* unknown = reinterpret_cast<IUnknown*>(candidate);
                if (FAILED(unknown->QueryInterface(__uuidof(ID3D11ShaderResourceView),
                                                   reinterpret_cast<void**>(&view))))
                {
                    view = nullptr;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                view = nullptr;
            }
            return view;
        }

        ID3D12Resource* QueryD3D12Resource(uintptr_t candidate)
        {
            ID3D12Resource* resource = nullptr;
            __try
            {
                auto* unknown = reinterpret_cast<IUnknown*>(candidate);
                if (FAILED(unknown->QueryInterface(__uuidof(ID3D12Resource),
                                                   reinterpret_cast<void**>(&resource))))
                {
                    resource = nullptr;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                resource = nullptr;
            }
            return resource;
        }

        struct TextureClassLayout
        {
            int32_t ImportedSizeOffset = -1;
            int32_t SrgbOffset = -1;
            uint8_t SrgbByteOffset = 0;
            uint8_t SrgbMask = 0;
            int32_t ObjectSize = 0;
        };

        const TextureClassLayout& GetTextureClassLayout(void* texture)
        {
            static std::mutex cacheMutex;
            static std::unordered_map<void*, TextureClassLayout> cache;

            UClass* textureClass =
                EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(texture));
            {
                std::lock_guard<std::mutex> lock(cacheMutex);
                const auto found = cache.find(textureClass);
                if (found != cache.end()) return found->second;
            }

            TextureClassLayout layout;
            PropertyRef property;
            if (FindProperty(g_State, textureClass, "ImportedSize", property))
                layout.ImportedSizeOffset = property.Offset;
            if (FindProperty(g_State, textureClass, "SRGB", property))
            {
                layout.SrgbOffset = property.Offset;
                layout.SrgbByteOffset = property.BoolByteOffset;
                layout.SrgbMask = property.BoolByteMask ? property.BoolByteMask : 1;
            }
            if (g_State.UStruct_PropertiesSizeOff >= 0)
            {
                SafeMem_Read(reinterpret_cast<uint8_t*>(textureClass) +
                                 g_State.UStruct_PropertiesSizeOff,
                             &layout.ObjectSize, sizeof(layout.ObjectSize));
            }

            std::lock_guard<std::mutex> lock(cacheMutex);
            return cache.emplace(textureClass, layout).first->second;
        }

        bool TextureImportedSize(void* texture, const TextureClassLayout& layout, int32_t& sizeX,
                                 int32_t& sizeY)
        {
            sizeX = 0;
            sizeY = 0;
            if (!texture || layout.ImportedSizeOffset < 0) return false;
            if (!SafeMem_Read(reinterpret_cast<uint8_t*>(texture) + layout.ImportedSizeOffset,
                              &sizeX, sizeof(sizeX)) ||
                !SafeMem_Read(reinterpret_cast<uint8_t*>(texture) + layout.ImportedSizeOffset + 4,
                              &sizeY, sizeof(sizeY)))
            {
                return false;
            }
            return sizeX > 0 && sizeY > 0 && sizeX <= 32768 && sizeY <= 32768;
        }

        bool TextureIsSrgb(void* texture, const TextureClassLayout& layout)
        {
            if (!texture || layout.SrgbOffset < 0) return true;
            uint8_t byte = 0;
            if (!SafeMem_Read(reinterpret_cast<uint8_t*>(texture) + layout.SrgbOffset +
                                  layout.SrgbByteOffset,
                              &byte, sizeof(byte)))
            {
                return true;
            }
            return (byte & layout.SrgbMask) != 0;
        }

        bool ModuleRange(HMODULE module, uintptr_t& base, uintptr_t& end)
        {
            base = 0;
            end = 0;
            size_t size = 0;
            if (!ModuleImageSize(module, base, size)) return false;
            end = base + size;
            return true;
        }

        bool InModuleRange(uintptr_t address, uintptr_t base, uintptr_t end)
        {
            return base != 0 && address >= base && address < end;
        }

        uintptr_t ReadPointer(const uint8_t* address)
        {
            uintptr_t value = 0;
            std::memcpy(&value, address, sizeof(value));
            return value;
        }

        bool PlausiblePointer(uintptr_t value)
        {
            return value >= 0x10000 && value <= 0x7FFFFFFFFFFF;
        }

        constexpr uint32_t kRootSetFlag = 1u << 30;

        struct TextureFailureCache
        {
            std::mutex Mutex;
            std::unordered_map<void*, uint64_t> FailedAt;
        };

        TextureFailureCache& TextureFailures()
        {
            static TextureFailureCache cache;
            return cache;
        }

        bool TextureResolutionCoolingDown(void* texture)
        {
            auto& cache = TextureFailures();
            std::lock_guard<std::mutex> lock(cache.Mutex);
            const auto found = cache.FailedAt.find(texture);
            if (found == cache.FailedAt.end()) return false;
            if (GetTickCount64() - found->second < 500) return true;
            cache.FailedAt.erase(found);
            return false;
        }

        void NoteTextureFailure(void* texture)
        {
            auto& cache = TextureFailures();
            std::lock_guard<std::mutex> lock(cache.Mutex);
            if (cache.FailedAt.size() >= 8192) cache.FailedAt.clear();
            cache.FailedAt[texture] = GetTickCount64();
        }

        void NoteTextureSuccess(void* texture)
        {
            auto& cache = TextureFailures();
            std::lock_guard<std::mutex> lock(cache.Mutex);
            cache.FailedAt.erase(texture);
        }

        void RootTextureObject(void* texture)
        {
            EngineDiscovery::SetInternalObjectFlags(
                g_State, static_cast<UObject*>(texture), kRootSetFlag, true);
        }

        bool D3D12ResourceMatchesSize(ID3D12Resource* resource, int32_t wantX, int32_t wantY)
        {
            if (!resource) return false;
            __try
            {
                const D3D12_RESOURCE_DESC desc = resource->GetDesc();
                if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
                    desc.SampleDesc.Count != 1 || desc.MipLevels == 0)
                {
                    return false;
                }
                if (wantX <= 0 || wantY <= 0) return true;
                return desc.Width == static_cast<UINT64>(wantX) &&
                       desc.Height == static_cast<UINT>(wantY);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        bool D3D11ViewMatchesSize(ID3D11ShaderResourceView* view, int32_t wantX, int32_t wantY)
        {
            if (!view) return false;
            if (wantX <= 0 || wantY <= 0) return true;
            __try
            {
                ID3D11Resource* resource = nullptr;
                view->GetResource(&resource);
                if (!resource) return false;
                ID3D11Texture2D* texture = nullptr;
                const HRESULT hr = resource->QueryInterface(__uuidof(ID3D11Texture2D),
                                                            reinterpret_cast<void**>(&texture));
                resource->Release();
                if (FAILED(hr) || !texture) return false;
                D3D11_TEXTURE2D_DESC desc{};
                texture->GetDesc(&desc);
                texture->Release();
                return desc.Width == static_cast<UINT>(wantX) &&
                       desc.Height == static_cast<UINT>(wantY);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        ID3D12Resource* ProbeD3D12Resource(uintptr_t rhi, int32_t wantX, int32_t wantY)
        {
            uintptr_t d3d12Base = 0;
            uintptr_t d3d12End = 0;
            uintptr_t coreBase = 0;
            uintptr_t coreEnd = 0;
            if (!ModuleRange(GetModuleHandleA("d3d12.dll"), d3d12Base, d3d12End)) return nullptr;
            ModuleRange(GetModuleHandleA("d3d12core.dll"), coreBase, coreEnd);

            uint8_t outer[0x300] = {};
            if (!SafeMem_Read(reinterpret_cast<void*>(rhi), outer, sizeof(outer)))
                return nullptr;

            ID3D12Resource* fallback = nullptr;
            for (int outerOffset = 0; outerOffset <= 0x2F8; outerOffset += 8)
            {
                const uintptr_t level1 = ReadPointer(outer + outerOffset);
                if (!PlausiblePointer(level1)) continue;

                uint8_t inner[0x60] = {};
                if (!SafeMem_Read(reinterpret_cast<void*>(level1), inner, sizeof(inner)))
                {
                    continue;
                }

                for (int innerOffset = 0; innerOffset <= 0x58; innerOffset += 8)
                {
                    const uintptr_t candidate = ReadPointer(inner + innerOffset);
                    if (!PlausiblePointer(candidate)) continue;

                    uintptr_t vtable = 0;
                    if (!SafeMem_Read(reinterpret_cast<void*>(candidate), &vtable,
                                      sizeof(vtable)))
                    {
                        continue;
                    }
                    if (!InModuleRange(vtable, d3d12Base, d3d12End) &&
                        !InModuleRange(vtable, coreBase, coreEnd))
                    {
                        continue;
                    }

                    ID3D12Resource* resource = QueryD3D12Resource(candidate);
                    if (!resource) continue;

                    if (D3D12ResourceMatchesSize(resource, wantX, wantY))
                    {
                        if (fallback) fallback->Release();
                        return resource;
                    }
                    if (!fallback) fallback = resource;
                    else resource->Release();
                }
            }
            return fallback;
        }

        ID3D11ShaderResourceView* ProbeD3D11View(uintptr_t rhi, int32_t wantX, int32_t wantY)
        {
            uintptr_t d3d11Base = 0;
            uintptr_t d3d11End = 0;
            if (!ModuleRange(GetModuleHandleA("d3d11.dll"), d3d11Base, d3d11End)) return nullptr;

            uint8_t window[0x200] = {};
            if (!SafeMem_Read(reinterpret_cast<void*>(rhi), window, sizeof(window)))
                return nullptr;

            ID3D11ShaderResourceView* fallback = nullptr;
            for (int offset = 0; offset <= 0x1F8; offset += 8)
            {
                const uintptr_t candidate = ReadPointer(window + offset);
                if (!PlausiblePointer(candidate)) continue;

                uintptr_t vtable = 0;
                if (!SafeMem_Read(reinterpret_cast<void*>(candidate), &vtable, sizeof(vtable)))
                    continue;
                if (!InModuleRange(vtable, d3d11Base, d3d11End)) continue;

                ID3D11ShaderResourceView* view = QueryD3D11ResourceView(candidate);
                if (!view) continue;

                if (D3D11ViewMatchesSize(view, wantX, wantY))
                {
                    if (fallback) fallback->Release();
                    return view;
                }
                if (!fallback) fallback = view;
                else view->Release();
            }
            return fallback;
        }

        struct TextureChainHit
        {
            void* Resource = nullptr;
            void* Rhi = nullptr;
            int32_t ResourceOffset = -1;
            int32_t RhiOffset = -1;
        };

        int32_t TextureScanBound(const TextureClassLayout& layout)
        {
            int32_t bound = layout.ObjectSize;
            if (bound < 0x100 || bound > 0x1000) bound = 0x600;
            return bound;
        }

        bool FindTextureChainD3D12(void* texture, const TextureClassLayout& layout, int32_t wantX,
                                   int32_t wantY, TextureChainHit& hit, ID3D12Resource** out)
        {
            uintptr_t gameBase = 0;
            uintptr_t gameEnd = 0;
            if (!ModuleRange(GetModuleHandle(nullptr), gameBase, gameEnd)) return false;

            const int32_t bound = TextureScanBound(layout);
            std::vector<uint8_t> object(static_cast<size_t>(bound), 0);
            if (!SafeMem_Read(texture, object.data(), bound)) return false;

            for (int32_t resourceOffset = 0x28; resourceOffset + 8 <= bound;
                 resourceOffset += 8)
            {
                const uintptr_t resource = ReadPointer(object.data() + resourceOffset);
                if (!PlausiblePointer(resource)) continue;

                uintptr_t vtable = 0;
                if (!SafeMem_Read(reinterpret_cast<void*>(resource), &vtable, sizeof(vtable)))
                    continue;
                if (!InModuleRange(vtable, gameBase, gameEnd)) continue;

                uint8_t window[0x40] = {};
                if (!SafeMem_Read(reinterpret_cast<void*>(resource), window, sizeof(window)))
                    continue;

                for (int rhiOffset = 0x8; rhiOffset <= 0x38; rhiOffset += 8)
                {
                    const uintptr_t rhi = ReadPointer(window + rhiOffset);
                    if (!PlausiblePointer(rhi)) continue;

                    uintptr_t rhiVtable = 0;
                    if (!SafeMem_Read(reinterpret_cast<void*>(rhi), &rhiVtable,
                                      sizeof(rhiVtable)))
                    {
                        continue;
                    }

                    ID3D12Resource* native = ProbeD3D12Resource(rhi, wantX, wantY);
                    if (!native) continue;

                    hit.Resource = reinterpret_cast<void*>(resource);
                    hit.Rhi = reinterpret_cast<void*>(rhi);
                    hit.ResourceOffset = resourceOffset;
                    hit.RhiOffset = rhiOffset;
                    *out = native;
                    return true;
                }
            }
            return false;
        }

        bool FindTextureChainD3D11(void* texture, const TextureClassLayout& layout, int32_t wantX,
                                   int32_t wantY, TextureChainHit& hit,
                                   ID3D11ShaderResourceView** out)
        {
            uintptr_t gameBase = 0;
            uintptr_t gameEnd = 0;
            if (!ModuleRange(GetModuleHandle(nullptr), gameBase, gameEnd)) return false;

            const int32_t bound = TextureScanBound(layout);
            std::vector<uint8_t> object(static_cast<size_t>(bound), 0);
            if (!SafeMem_Read(texture, object.data(), bound)) return false;

            for (int32_t resourceOffset = 0x28; resourceOffset + 8 <= bound;
                 resourceOffset += 8)
            {
                const uintptr_t resource = ReadPointer(object.data() + resourceOffset);
                if (!PlausiblePointer(resource)) continue;

                uintptr_t vtable = 0;
                if (!SafeMem_Read(reinterpret_cast<void*>(resource), &vtable, sizeof(vtable)))
                    continue;
                if (!InModuleRange(vtable, gameBase, gameEnd)) continue;

                uint8_t window[0x40] = {};
                if (!SafeMem_Read(reinterpret_cast<void*>(resource), window, sizeof(window)))
                    continue;

                for (int rhiOffset = 0x8; rhiOffset <= 0x38; rhiOffset += 8)
                {
                    const uintptr_t rhi = ReadPointer(window + rhiOffset);
                    if (!PlausiblePointer(rhi)) continue;

                    uintptr_t rhiVtable = 0;
                    if (!SafeMem_Read(reinterpret_cast<void*>(rhi), &rhiVtable,
                                      sizeof(rhiVtable)))
                    {
                        continue;
                    }

                    ID3D11ShaderResourceView* view = ProbeD3D11View(rhi, wantX, wantY);
                    if (!view) continue;

                    hit.Resource = reinterpret_cast<void*>(resource);
                    hit.Rhi = reinterpret_cast<void*>(rhi);
                    hit.ResourceOffset = resourceOffset;
                    hit.RhiOffset = rhiOffset;
                    *out = view;
                    return true;
                }
            }
            return false;
        }

        struct CachedTextureView
        {
            void* ResourceObject = nullptr;
            void* RhiTexture = nullptr;
            int32_t ResourceOffset = -1;
            int32_t RhiOffset = -1;
            uint32_t Epoch = 0;
            uint64_t View = 0;
            IUnknown* Hold = nullptr;
        };

        bool CachedChainIsCurrent(void* texture, const CachedTextureView& entry)
        {
            void* resource = nullptr;
            if (entry.ResourceOffset < 0 ||
                !SafeMem_Read(reinterpret_cast<uint8_t*>(texture) + entry.ResourceOffset,
                              &resource, sizeof(resource)) ||
                resource != entry.ResourceObject)
            {
                return false;
            }

            void* rhi = nullptr;
            if (entry.RhiOffset < 0 ||
                !SafeMem_Read(reinterpret_cast<uint8_t*>(entry.ResourceObject) + entry.RhiOffset,
                              &rhi, sizeof(rhi)))
            {
                return false;
            }
            return rhi == entry.RhiTexture;
        }

        void ReleaseCachedTextureView(CachedTextureView& entry, bool freeViewSlot)
        {
            if (entry.Hold)
            {
                entry.Hold->Release();
                entry.Hold = nullptr;
            }
            if (freeViewSlot && entry.View) D3D12Hook_ReleaseTextureView(entry.View);
            entry.View = 0;
        }

        uint64_t ResolveD3D12TextureView(void* texture)
        {
            static std::mutex cacheMutex;
            static std::unordered_map<void*, CachedTextureView> cache;

            const uint32_t epoch = D3D12Hook_TextureEpoch();
            {
                std::lock_guard<std::mutex> lock(cacheMutex);
                const auto found = cache.find(texture);
                if (found != cache.end())
                {
                    if (found->second.Epoch == epoch &&
                        CachedChainIsCurrent(texture, found->second))
                    {
                        return found->second.View;
                    }
                    ReleaseCachedTextureView(found->second, found->second.Epoch == epoch);
                    cache.erase(found);
                }
            }

            if (TextureResolutionCoolingDown(texture)) return 0;

            const TextureClassLayout& layout = GetTextureClassLayout(texture);
            int32_t wantX = 0;
            int32_t wantY = 0;
            TextureImportedSize(texture, layout, wantX, wantY);
            const bool srgb = TextureIsSrgb(texture, layout);

            TextureChainHit hit;
            ID3D12Resource* resource = nullptr;
            if (!FindTextureChainD3D12(texture, layout, wantX, wantY, hit, &resource))
            {
                NoteTextureFailure(texture);
                return 0;
            }

            const uint64_t view = D3D12Hook_CreateTextureView(resource, srgb);
            if (!view)
            {
                resource->Release();
                NoteTextureFailure(texture);
                return 0;
            }

            NoteTextureSuccess(texture);

            CachedTextureView entry;
            entry.ResourceObject = hit.Resource;
            entry.RhiTexture = hit.Rhi;
            entry.ResourceOffset = hit.ResourceOffset;
            entry.RhiOffset = hit.RhiOffset;
            entry.Epoch = epoch;
            entry.View = view;
            entry.Hold = resource;

            std::lock_guard<std::mutex> lock(cacheMutex);
            cache[texture] = entry;
            return view;
        }

        uint64_t ResolveD3D11TextureView(void* texture)
        {
            static std::mutex cacheMutex;
            static std::unordered_map<void*, CachedTextureView> cache;
            {
                std::lock_guard<std::mutex> lock(cacheMutex);
                const auto found = cache.find(texture);
                if (found != cache.end())
                {
                    if (CachedChainIsCurrent(texture, found->second))
                        return found->second.View;
                    ReleaseCachedTextureView(found->second, false);
                    cache.erase(found);
                }
            }

            if (TextureResolutionCoolingDown(texture)) return 0;

            const TextureClassLayout& layout = GetTextureClassLayout(texture);
            int32_t wantX = 0;
            int32_t wantY = 0;
            TextureImportedSize(texture, layout, wantX, wantY);

            TextureChainHit hit;
            ID3D11ShaderResourceView* view = nullptr;
            if (!FindTextureChainD3D11(texture, layout, wantX, wantY, hit, &view))
            {
                NoteTextureFailure(texture);
                return 0;
            }

            NoteTextureSuccess(texture);

            CachedTextureView entry;
            entry.ResourceObject = hit.Resource;
            entry.RhiTexture = hit.Rhi;
            entry.ResourceOffset = hit.ResourceOffset;
            entry.RhiOffset = hit.RhiOffset;
            entry.View = reinterpret_cast<uint64_t>(view);
            entry.Hold = view;

            std::lock_guard<std::mutex> lock(cacheMutex);
            cache[texture] = entry;
            return entry.View;
        }

        uint64_t ApiGetImGuiTexture(void* texture)
        {
            if (!ApiIsValidObject(texture) || !ApiIsA(texture, "Texture2D")) return 0;
            if (!ApiIsPendingKill(texture)) RootTextureObject(texture);
            if (D3D12Hook_TextureBridgeReady()) return ResolveD3D12TextureView(texture);
            return ResolveD3D11TextureView(texture);
        }

        void* ApiGetDataTableRow(void* dataTable, const char* rowName)
        {
            if (!dataTable || !rowName || !*rowName) return nullptr;

            UObject* table = static_cast<UObject*>(dataTable);
            UClass* tableClass = EngineDiscovery::GetObjectClass(g_State, table);
            if (!tableClass) return nullptr;

            PropertyRef rowStructProperty;
            if (!FindProperty(g_State, tableClass, "RowStruct", rowStructProperty))
                return nullptr;
            if (rowStructProperty.Offset <= 0) return nullptr;

            struct RowMapHeader
            {
                void* Data;
                int32_t ArrayNum;
                int32_t ArrayMax;
            };

            RowMapHeader header{};
            const int32_t rowMapOffset =
                rowStructProperty.Offset + static_cast<int32_t>(sizeof(void*));
            if (!SafeMem_Read(reinterpret_cast<uint8_t*>(table) + rowMapOffset, &header,
                              sizeof(header)))
            {
                return nullptr;
            }
            if (!header.Data || header.ArrayNum <= 0 || header.ArrayNum > 50000)
                return nullptr;

            std::string nameText = rowName;
            int32_t nameIndex = EngineDiscovery::ResolveFNameIndex(g_State, nameText);
            uint32_t wantedNumber = 0;
            if (nameIndex < 0)
            {
                size_t end = nameText.size();
                while (end > 0 && nameText[end - 1] >= '0' && nameText[end - 1] <= '9')
                    --end;
                if (end < nameText.size() && end > 0 && nameText[end - 1] == '_')
                {
                    uint64_t external = 0;
                    for (size_t i = end; i < nameText.size(); ++i)
                        external = external * 10 + static_cast<uint64_t>(nameText[i] - '0');
                    const std::string baseName = nameText.substr(0, end - 1);
                    nameIndex = EngineDiscovery::ResolveFNameIndex(g_State, baseName);
                    if (nameIndex >= 0 && external < 0xFFFF)
                        wantedNumber = static_cast<uint32_t>(external + 1);
                }
            }
            if (nameIndex < 0) return nullptr;

            constexpr int32_t kElementStride = 0x18;
            for (int32_t i = 0; i < header.ArrayNum; ++i)
            {
                uint8_t* element = reinterpret_cast<uint8_t*>(header.Data) +
                                   static_cast<size_t>(i) * kElementStride;
                FName name{};
                if (!SafeMem_Read(element, &name, sizeof(name))) continue;
                if (static_cast<int32_t>(name.ComparisonIndex) != nameIndex ||
                    name.Number != wantedNumber)
                {
                    continue;
                }

                void* row = nullptr;
                if (!SafeMem_Read(element + sizeof(FName), &row, sizeof(row))) return nullptr;
                return row;
            }
            return nullptr;
        }
    }

    const AvaloniumModApi g_Api = {
        sizeof(AvaloniumModApi),
        AVALONIUM_MOD_API_VERSION,
        &ApiGetEngineMajor,
        &ApiGetEngineMinor,
        &ApiGetGameName,
        &ApiGetRuntimeVersion,
        &ApiGetModDirectory,
        &ApiLogMessage,
        &ApiIsInGameThread,
        &ApiRunOnGameThread,
        &ApiGetDeltaSeconds,
        &ApiGetFrameCount,
        &ApiFindObject,
        &ApiFindClass,
        &ApiFindFirstObjectOfClass,
        &ApiGetClassDefaultObject,
        &ApiFindObjectsOfClass,
        &ApiGetObjectCount,
        &ApiGetObjectByIndex,
        &ApiIsValidObject,
        &ApiGetObjectName,
        &ApiGetObjectFullName,
        &ApiGetObjectPathName,
        &ApiGetObjectClass,
        &ApiGetObjectOuter,
        &ApiGetSuperClass,
        &ApiIsA,
        &ApiFindFunction,
        &ApiGetClassProperties,
        &ApiGetClassFunctions,
        &ApiGetFunctionFlags,
        &ApiGetFunctionFrameSize,
        &ApiGetProperty,
        &ApiSetProperty,
        &ApiGetFunctionArgument,
        &ApiSetFunctionArgument,
        &ApiGetHookResult,
        &ApiCallFunction,
        &ApiProcessEvent,
        &ApiRegisterHook,
        &ApiUnregisterHook,
        &ApiRegisterKeyBind,
        &ApiUnregisterKeyBind,
        &ApiConstructObject,
        &ApiGetWorld,
        &ApiBindWidgetClick,
        &ApiUnbindWidgetClick,
        &ApiBindWidgetEvent,
        &ApiArrayLength,
        &ApiArrayElementSize,
        &ApiArrayGet,
        &ApiArraySet,
        &ApiArrayAdd,
        &ApiArrayRemove,
        &ApiMapLength,
        &ApiMapGetAt,
        &ApiMapSetValueAt,
        &ApiSetLength,
        &ApiSetGetAt,
        &ApiGetImGuiContext,
        &ApiGetImGuiAllocatorFunctions,
        &ApiRegisterImGuiRender,
        &ApiUnregisterImGuiRender,
        &ApiSetImGuiInputActive,
        &ApiRegisterHookFunction,
        &ApiMapAdd,
        &ApiMapRemove,
        &ApiMapClear,
        &ApiMapContains,
        &ApiMapFind,
        &ApiSetAdd,
        &ApiSetRemove,
        &ApiSetClear,
        &ApiSetContains,
        &ApiGetPropertyPath,
        &ApiSetPropertyPath,
        &ApiGetModuleHandle,
        &ApiFindExport,
        &ApiFindPattern,
        &ApiResolveRelative,
        &ApiGetVtable,
        &ApiGetVirtualFunction,
        &ApiGetImGuiTexture,
        &ApiGetDataTableRow,
        &ApiCallFunctionOut,
        &ApiIsPendingKill,
        &ApiRegisterHookVeto,
        &ApiRegisterAddressVeto,
        &ApiIsModAsset,
    };

    const AvaloniumSdkApi g_SdkApi = {
        sizeof(AvaloniumSdkApi),
        AVALONIUM_SDK_API_VERSION,
        &ApiGetEngineMajor,
        &ApiGetEngineMinor,
        &ApiGetObjectCount,
        &ApiGetObjectByIndex,
        &ApiGetObjectIndex,
        &ApiGetObjectClass,
        &ApiGetObjectOuter,
        &ApiGetSuperClass,
        &ApiGetObjectName,
        &ApiGetObjectFullName,
        &ApiGetFNameString,
        &ApiIsA,
        &ApiFindObject,
        &ApiFindClass,
        &ApiGetClassDefaultObject,
        &ApiFindFunction,
        &ApiProcessEvent,
    };
}

AVALONIUM_MOD_API_ENTRY const AvaloniumModApi* AvaloniumGetModApi(uint32_t requestedVersion)
{
    if (requestedVersion < AVALONIUM_MOD_API_MIN_VERSION ||
        requestedVersion > AVALONIUM_MOD_API_VERSION)
    {
        return nullptr;
    }
    return &Avalonium::CppMods::g_Api;
}

AVALONIUM_SDK_API_ENTRY const AvaloniumSdkApi* AvaloniumGetSdkApi(uint32_t requestedVersion)
{
    if (requestedVersion != AVALONIUM_SDK_API_VERSION)
    {
        return nullptr;
    }
    return &Avalonium::CppMods::g_SdkApi;
}
