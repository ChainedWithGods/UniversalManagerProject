#include "sdk/Avalonium/ModApi.h"
#include "sdk/Avalonium/SdkApi.h"

#include "CppModContext.h"
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
                outerObject = g_State.GWorld
                                  ? static_cast<UObject*>(g_State.GWorld)
                                  : (g_State.GWorldPtr
                                         ? static_cast<UObject*>(*g_State.GWorldPtr)
                                         : nullptr);
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
            if (g_State.GWorld) return g_State.GWorld;
            if (g_State.GWorldPtr) return *g_State.GWorldPtr;
            return nullptr;
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

        ID3D11ShaderResourceView* FindD3D11TextureView(void* texture)
        {
            static std::mutex cacheMutex;
            static std::unordered_map<void*, ID3D11ShaderResourceView*> cache;
            {
                std::lock_guard<std::mutex> lock(cacheMutex);
                const auto found = cache.find(texture);
                if (found != cache.end()) return found->second;
            }

            uintptr_t d3d11Base = 0;
            uintptr_t gameBase = 0;
            size_t d3d11Size = 0;
            size_t gameSize = 0;
            if (!ModuleImageSize(GetModuleHandleA("d3d11.dll"), d3d11Base, d3d11Size) ||
                !ModuleImageSize(GetModuleHandleA(nullptr), gameBase, gameSize))
            {
                return nullptr;
            }

            std::vector<std::pair<uintptr_t, int>> pending;
            std::unordered_set<uintptr_t> visited;
            pending.emplace_back(reinterpret_cast<uintptr_t>(texture), 0);
            size_t budget = 192;
            while (!pending.empty() && budget-- > 0)
            {
                const auto [address, depth] = pending.back();
                pending.pop_back();
                if (depth > 3 || !visited.insert(address).second) continue;

                uint8_t buffer[0x100] = {};
                if (!SafeMem_Read(reinterpret_cast<void*>(address), buffer, sizeof(buffer)))
                    continue;

                for (int offset = 0x8; offset <= 0xF8; offset += 8)
                {
                    uintptr_t candidate = 0;
                    std::memcpy(&candidate, buffer + offset, sizeof(candidate));
                    if (candidate < 0x10000 || candidate > 0x7FFFFFFFFFFF) continue;

                    uintptr_t vtable = 0;
                    if (!SafeMem_Read(reinterpret_cast<void*>(candidate), &vtable,
                                      sizeof(vtable)))
                        continue;

                    if (vtable >= d3d11Base && vtable < d3d11Base + d3d11Size)
                    {
                        if (ID3D11ShaderResourceView* view =
                                QueryD3D11ResourceView(candidate))
                        {
                            std::lock_guard<std::mutex> lock(cacheMutex);
                            cache[texture] = view;
                            return view;
                        }
                    }
                    else if (depth < 3 && vtable >= gameBase && vtable < gameBase + gameSize)
                    {
                        pending.emplace_back(candidate, depth + 1);
                    }
                }
            }
            return nullptr;
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

        uint64_t FindD3D12TextureView(void* texture)
        {
            struct CacheEntry
            {
                uint64_t View = 0;
                uint32_t Epoch = 0;
            };
            static std::mutex cacheMutex;
            static std::unordered_map<void*, CacheEntry> cache;

            const uint32_t epoch = D3D12Hook_TextureEpoch();
            {
                std::lock_guard<std::mutex> lock(cacheMutex);
                const auto found = cache.find(texture);
                if (found != cache.end() && found->second.Epoch == epoch)
                    return found->second.View;
            }

            uintptr_t d3d12Base = 0;
            uintptr_t d3d12CoreBase = 0;
            uintptr_t gameBase = 0;
            size_t d3d12Size = 0;
            size_t d3d12CoreSize = 0;
            size_t gameSize = 0;
            if (!ModuleImageSize(GetModuleHandleA("d3d12.dll"), d3d12Base, d3d12Size) ||
                !ModuleImageSize(GetModuleHandle(nullptr), gameBase, gameSize))
            {
                return 0;
            }
            ModuleImageSize(GetModuleHandleA("d3d12core.dll"), d3d12CoreBase,
                            d3d12CoreSize);

            std::vector<std::pair<uintptr_t, int>> pending;
            std::unordered_set<uintptr_t> visited;
            pending.emplace_back(reinterpret_cast<uintptr_t>(texture), 0);
            size_t cursor = 0;
            size_t budget = 2048;
            while (cursor < pending.size() && budget-- > 0)
            {
                const auto [address, depth] = pending[cursor++];
                if (depth > 3 || !visited.insert(address).second) continue;

                std::vector<uint8_t> buffer(0x300, 0);
                if (!SafeMem_Read(reinterpret_cast<void*>(address), buffer.data(),
                                  static_cast<int32_t>(buffer.size())))
                {
                    continue;
                }

                for (int offset = 0x8; offset <= 0x2F8; offset += 8)
                {
                    uintptr_t candidate = 0;
                    std::memcpy(&candidate, buffer.data() + offset, sizeof(candidate));
                    if (candidate < 0x10000 || candidate > 0x7FFFFFFFFFFF) continue;

                    uintptr_t vtable = 0;
                    if (!SafeMem_Read(reinterpret_cast<void*>(candidate), &vtable,
                                      sizeof(vtable)))
                    {
                        continue;
                    }

                    const bool d3d12Object =
                        (d3d12Base && vtable >= d3d12Base &&
                         vtable < d3d12Base + d3d12Size) ||
                        (d3d12CoreBase && vtable >= d3d12CoreBase &&
                         vtable < d3d12CoreBase + d3d12CoreSize);
                    if (d3d12Object)
                    {
                        ID3D12Resource* resource = QueryD3D12Resource(candidate);
                        if (!resource) continue;

                        const uint64_t view = D3D12Hook_CreateTextureView(resource);
                        resource->Release();
                        if (!view) continue;

                        {
                            std::lock_guard<std::mutex> lock(cacheMutex);
                            cache[texture] = CacheEntry{view, epoch};
                        }
                        return view;
                    }
                    if (depth < 3 && vtable >= gameBase && vtable < gameBase + gameSize)
                    {
                        pending.emplace_back(candidate, depth + 1);
                    }
                }
            }
            return 0;
        }

        uint64_t ApiGetImGuiTexture(void* texture)
        {
            if (!ApiIsValidObject(texture) || !ApiIsA(texture, "Texture2D")) return 0;
            if (D3D12Hook_TextureBridgeReady()) return FindD3D12TextureView(texture);
            return reinterpret_cast<uint64_t>(FindD3D11TextureView(texture));
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
