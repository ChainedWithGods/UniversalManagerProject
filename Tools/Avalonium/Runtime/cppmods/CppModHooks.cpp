#include "CppModHooks.h"

#include "../core/EngineDiscovery.h"
#include "../core/Logger.h"
#include "../core/NameResolution.h"
#include "../utils/InlineHook.h"
#include "../utils/SafeMemory.h"
#include "CppModContext.h"
#include "CppModReflection.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

extern EngineState g_State;

namespace Avalonium::CppMods
{
    namespace
    {
        using NativeFunc = void(__fastcall*)(void*, void*, void*);

        constexpr uint32_t kMaxHookSlots = 128;

        struct HookRecord
        {
            uint32_t Id = 0;
            void* Function = nullptr;
            AvaloniumHookCallback Pre = nullptr;
            AvaloniumHookCallback Post = nullptr;
            AvaloniumVetoCallback Veto = nullptr;
            void* User = nullptr;
            void* Owner = nullptr;
        };

        struct FuncPatch
        {
            void* Target = nullptr;
            uint32_t Slot = 0;
            Avalonium::InlineHook Hook;
            void* Original = nullptr;
            std::vector<HookRecord> Records;
        };

        struct ScriptHookRecord
        {
            uint32_t Id = 0;
            void* Function = nullptr;
            AvaloniumHookCallback Pre = nullptr;
            AvaloniumHookCallback Post = nullptr;
            void* User = nullptr;
            void* Owner = nullptr;

            std::string IdentityKey;
        };

        std::mutex g_Mutex;
        std::array<FuncPatch*, kMaxHookSlots> g_SlotPatches{};
        std::unordered_map<void*, FuncPatch*> g_TargetPatches;
        std::unordered_map<uint32_t, FuncPatch*> g_HookIdPatches;
        std::vector<std::unique_ptr<FuncPatch>> g_Patches;
        std::vector<std::unique_ptr<FuncPatch>> g_Retired;
        std::atomic<uint32_t> g_NextHookId{1};
        std::atomic<int> g_InFlight{0};

        struct RawPatch
        {
            uint32_t Slot = 0;
            Avalonium::InlineHook Hook;
            void* Original = nullptr;
            AvaloniumVetoCallback Veto = nullptr;
            void* User = nullptr;
            void* Owner = nullptr;
        };

        std::array<RawPatch*, kMaxHookSlots> g_RawSlotPatches{};
        std::vector<std::unique_ptr<RawPatch>> g_RawPatches;

        std::mutex g_ScriptMutex;
        std::unordered_map<void*, std::vector<ScriptHookRecord>> g_ScriptHooks;
        std::unordered_map<std::string, std::vector<ScriptHookRecord>> g_ScriptHooksByIdentity;
        std::atomic<uint64_t> g_ScriptHookNameBloom{0};
        std::atomic<uint32_t> g_ScriptHookCount{0};

        uint64_t ScriptHookNameBit(int32_t nameIndex)
        {
            const uint32_t hash = static_cast<uint32_t>(nameIndex) * 2654435761u;
            return uint64_t{1} << (hash & 63u);
        }

        int32_t ObjectNameIndex(UObject* object)
        {
            if (!object || g_State.UObject_NameOff < 0) return 0;
            int32_t index = 0;
            if (!SafeMem_Read(reinterpret_cast<uint8_t*>(object) + g_State.UObject_NameOff +
                                  g_State.FName_CompIdxOff,
                              &index, sizeof(index)))
            {
                return 0;
            }
            return index;
        }

        std::string ScriptHookIdentityKey(const std::string& fullPath, uint32_t flags,
                                          int32_t frameSize)
        {
            return fullPath + "|" + std::to_string(flags) + "|" + std::to_string(frameSize);
        }

        std::string MakeScriptHookIdentity(void* function, std::string& fullPath)
        {
            fullPath.clear();
            if (!function) return {};

            fullPath = EngineDiscovery::BuildPathName(g_State,
                                                      reinterpret_cast<UObject*>(function));
            if (fullPath.empty()) return {};

            const uint32_t flags = GetFunctionFlags(g_State, function);
            const int32_t frameSize = GetFunctionFrameSize(g_State, function);
            return ScriptHookIdentityKey(fullPath, flags, frameSize);
        }

        void LogLine(const std::string& line)
        {
            EngineDiscovery::WriteLogLine(line.c_str());
        }

        bool SameText(const std::string& a, const std::string& b)
        {
            return _stricmp(a.c_str(), b.c_str()) == 0;
        }

        UFunction* ResolveFunction(EngineState& state, const std::string& className,
                                   const std::string& functionName, std::string* why)
        {
            if (functionName.empty())
            {
                if (why) *why = "the function name is empty";
                return nullptr;
            }

            if (!className.empty())
            {
                UClass* objectClass = FindClass(state, className);
                if (!objectClass)
                {
                    if (why) *why = "class '" + className + "' was not found";
                    return nullptr;
                }
                UFunction* function = FindFunction(state, objectClass, functionName);
                if (!function && why)
                    *why = "function '" + functionName + "' was not found on class '" + className + "'";
                return function;
            }

            auto indexed = state.FunctionByNameOnly.find(functionName);
            if (indexed != state.FunctionByNameOnly.end()) return indexed->second;

            for (UFunction* function : state.LiveUFunctions)
            {
                if (!function) continue;
                const std::string name =
                    EngineDiscovery::GetObjectNameStr(state, static_cast<UObject*>(function));
                if (SameText(name, functionName)) return function;
            }

            if (why) *why = "function '" + functionName + "' was not found by name";
            return nullptr;
        }

        void Dispatch(uint32_t slot, void* context, void* frame, void* result);

        template <uint32_t Slot>
        void __fastcall SlotDetour(void* context, void* frame, void* result)
        {
            Dispatch(Slot, context, frame, result);
        }

        template <size_t... Index>
        std::array<NativeFunc, sizeof...(Index)> MakeSlotTable(std::index_sequence<Index...>)
        {
            return {&SlotDetour<static_cast<uint32_t>(Index)>...};
        }

        const std::array<NativeFunc, kMaxHookSlots> g_SlotTable =
            MakeSlotTable(std::make_index_sequence<kMaxHookSlots>{});

        void InvokeCallback(AvaloniumHookCallback callback, void* user, void* object,
                            void* function, void* frame, void* result)
        {
            if (!callback) return;
            try
            {
                callback(user, object, function, frame, result);
            }
            catch (...)
            {
                LogLine("[CppMod] A hook callback threw an exception; the engine call was left "
                        "uninterrupted");
            }
        }

        void Dispatch(uint32_t slot, void* context, void* frame, void* result)
        {
            RawPatch* rawPatch = nullptr;
            {
                std::lock_guard<std::mutex> lock(g_Mutex);
                if (slot < kMaxHookSlots) rawPatch = g_RawSlotPatches[slot];
            }
            if (rawPatch && rawPatch->Original)
            {
                NativeFunc rawOriginal = reinterpret_cast<NativeFunc>(rawPatch->Original);
                bool allow = true;
                if (rawPatch->Veto)
                {
                    try
                    {
                        allow = rawPatch->Veto(rawPatch->User, context, nullptr, frame, result);
                    }
                    catch (...)
                    {
                        LogLine("[CppMod] A raw hook veto threw an exception; the engine call "
                                "was left uninterrupted");
                    }
                }
                if (!allow) return;
                rawOriginal(context, frame, result);
                return;
            }

            FuncPatch* patch = nullptr;
            NativeFunc original = nullptr;
            {
                std::lock_guard<std::mutex> lock(g_Mutex);
                if (slot < kMaxHookSlots) patch = g_SlotPatches[slot];
                if (patch) original = reinterpret_cast<NativeFunc>(patch->Original);
            }
            if (!patch || !original) return;

            void* function = nullptr;
            if (frame)
            {

                SafeMem_Read(reinterpret_cast<uint8_t*>(frame) + FFrame_NodeOff,
                             &function, sizeof(function));
            }
            if (!function)
            {
                original(context, frame, result);
                return;
            }

            std::vector<HookRecord> records;
            {
                std::lock_guard<std::mutex> lock(g_Mutex);
                for (const HookRecord& record : patch->Records)
                {
                    if (record.Function == function) records.push_back(record);
                }

                if (!records.empty()) g_InFlight.fetch_add(1, std::memory_order_acq_rel);
            }

            if (records.empty())
            {
                original(context, frame, result);
                return;
            }

            struct InFlightGuard
            {
                ~InFlightGuard() { g_InFlight.fetch_sub(1, std::memory_order_acq_rel); }
            } guard;

            for (const HookRecord& record : records)
            {
                ModContextScope scope(record.Owner);
                InvokeCallback(record.Pre, record.User, context, function, frame, nullptr);
            }

            bool allowed = true;
            for (const HookRecord& record : records)
            {
                if (!record.Veto) continue;
                ModContextScope scope(record.Owner);
                bool allow = true;
                try
                {
                    allow = record.Veto(record.User, context, function, frame, nullptr);
                }
                catch (...)
                {
                    LogLine("[CppMod] A hook veto threw an exception; the engine call was left "
                            "uninterrupted");
                }
                if (!allow)
                {
                    allowed = false;
                    break;
                }
            }

            if (allowed)
            {
                original(context, frame, result);

                for (const HookRecord& record : records)
                {
                    ModContextScope scope(record.Owner);
                    InvokeCallback(record.Post, record.User, context, function, frame, result);
                }
            }
        }
    }

    void DispatchScriptFunction(void* context, void* frame, void* result, bool pre)
    {
        if (g_ScriptHookCount.load(std::memory_order_acquire) == 0) return;
        if (!frame) return;

        void* function = nullptr;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(frame) + FFrame_NodeOff, &function,
                          sizeof(function)) ||
            reinterpret_cast<uintptr_t>(function) < 0x10000)
        {
            return;
        }

        const int32_t nameIndex = ObjectNameIndex(reinterpret_cast<UObject*>(function));
        if (nameIndex == 0 ||
            (g_ScriptHookNameBloom.load(std::memory_order_acquire) &
             ScriptHookNameBit(nameIndex)) == 0)
        {
            return;
        }

        std::string fullPath;
        const std::string identity = MakeScriptHookIdentity(function, fullPath);
        if (identity.empty()) return;

        std::vector<ScriptHookRecord> records;
        bool matchedByIdentity = false;
        {
            std::lock_guard<std::mutex> lock(g_ScriptMutex);
            auto it = g_ScriptHooks.find(function);
            if (it != g_ScriptHooks.end())
            {
                for (const ScriptHookRecord& record : it->second)
                {
                    if (record.IdentityKey == identity)
                    {
                        records.push_back(record);
                    }
                }
            }

            if (records.empty() && !identity.empty())
            {
                auto identityIt = g_ScriptHooksByIdentity.find(identity);
                if (identityIt != g_ScriptHooksByIdentity.end() &&
                    !identityIt->second.empty())
                {
                    records = identityIt->second;
                    matchedByIdentity = true;
                }
            }

            if (records.empty()) return;

            g_InFlight.fetch_add(1, std::memory_order_acq_rel);
        }

        if (matchedByIdentity)
        {
            static std::atomic<uint32_t> s_Reported{0};
            if (s_Reported.fetch_add(1, std::memory_order_relaxed) < 4)
            {
                LogLine("[CppMod] Blueprint hook matched by function identity after a "
                        "UFunction pointer change: " + fullPath);
            }
        }

        struct InFlightGuard
        {
            ~InFlightGuard() { g_InFlight.fetch_sub(1, std::memory_order_acq_rel); }
        } guard;

        for (const ScriptHookRecord& record : records)
        {
            ModContextScope scope(record.Owner);
            InvokeCallback(pre ? record.Pre : record.Post, record.User, context, function, frame,
                           pre ? nullptr : result);
        }
    }

    namespace
    {
        bool ValidateHookRequest(EngineState& state, AvaloniumHookCallback pre,
                                 AvaloniumHookCallback post, AvaloniumVetoCallback veto,
                                 std::string* why)
        {
            if (!pre && !post && !veto)
            {
                if (why) *why = "a hook needs a pre callback, a post callback, or a veto";
                return false;
            }
            if (!state.bInitialized)
            {
                if (why) *why = "the engine is not initialized";
                return false;
            }
            if (state.UFunction_FuncOff < 0)
            {
                if (why) *why = "UFunction::Func was not discovered on this build";
                return false;
            }
            return true;
        }

        uint32_t RegisterResolvedHook(EngineState& state, UFunction* function,
                                      AvaloniumHookCallback pre, AvaloniumHookCallback post,
                                      AvaloniumVetoCallback veto, void* userData, void* owner,
                                      std::string* why)
        {
            if (!function)
            {
                if (why) *why = "the function pointer is null";
                return 0;
            }

            const std::string rawFunctionName =
                EngineDiscovery::GetObjectNameStr(state, reinterpret_cast<UObject*>(function));
            UObject* outer =
                EngineDiscovery::GetObjectOuter(state, reinterpret_cast<UObject*>(function));
            const std::string rawClassName =
                outer ? EngineDiscovery::GetObjectNameStr(state, outer) : std::string();
            const std::string functionName =
                rawFunctionName.empty() ? "<function>" : rawFunctionName;
            const std::string className = rawClassName.empty() ? "<global>" : rawClassName;

            void* target = nullptr;
            const bool hasEntry =
                SafeMem_Read(reinterpret_cast<uint8_t*>(function) + state.UFunction_FuncOff,
                             &target, sizeof(target)) &&
                target != nullptr;

            const uint32_t flags = GetFunctionFlags(state, function);
            if ((flags & kFuncNative) == 0)
            {
                if (veto)
                {
                    if (why) *why = "a veto is only supported for native functions";
                    return 0;
                }

                if (!state.ProcessInternal || !hasEntry || target != state.ProcessInternal)
                {
                    if (why)
                        *why = "the function is interpreted but has no script entry on this build";
                    return 0;
                }

                std::string fullPath;
                const std::string identity = MakeScriptHookIdentity(function, fullPath);
                if (identity.empty())
                {
                    if (why)
                        *why = "the function identity (path, flags and frame size) could not be read";
                    return 0;
                }
                const int32_t nameIndex =
                    ObjectNameIndex(reinterpret_cast<UObject*>(function));
                if (nameIndex == 0)
                {
                    if (why) *why = "the function name index could not be read";
                    return 0;
                }

                const uint32_t id = g_NextHookId.fetch_add(1, std::memory_order_relaxed);
                {
                    std::lock_guard<std::mutex> lock(g_ScriptMutex);
                    ScriptHookRecord record;
                    record.Id = id;
                    record.Function = function;
                    record.Pre = pre;
                    record.Post = post;
                    record.User = userData;
                    record.Owner = owner;
                    record.IdentityKey = identity;
                    g_ScriptHooks[function].push_back(record);
                    g_ScriptHooksByIdentity[identity].push_back(record);
                    g_ScriptHookNameBloom.fetch_or(ScriptHookNameBit(nameIndex),
                                                   std::memory_order_relaxed);
                    g_ScriptHookCount.fetch_add(1, std::memory_order_release);
                }

                char line[384];
                std::snprintf(line, sizeof(line),
                              "[CppMod] Blueprint hook %u registered for %s::%s (interpreted)",
                              id, className.c_str(), functionName.c_str());
                LogLine(line);
                return id;
            }

            if (!hasEntry)
            {
                if (why) *why = "the function carries no native entry";
                return 0;
            }

            if ((state.ProcessInternal && target == state.ProcessInternal) ||
                (state.ProcessLocalScriptFunction && target == state.ProcessLocalScriptFunction) ||
                (state.ProcessEvent_UObjectImpl && target == state.ProcessEvent_UObjectImpl) ||
                (state.ProcessEvent_AActorImpl && target == state.ProcessEvent_AActorImpl))
            {
                if (why)
                    *why = "the function's entry is a shared VM entry, not a per-function native entry";
                return 0;
            }

            std::lock_guard<std::mutex> lock(g_Mutex);

            FuncPatch* patch = nullptr;
            auto existing = g_TargetPatches.find(target);
            if (existing != g_TargetPatches.end())
            {
                patch = existing->second;
            }
            else
            {
                uint32_t slot = kMaxHookSlots;
                for (uint32_t i = 0; i < kMaxHookSlots; ++i)
                {
                    if (g_SlotPatches[i] == nullptr)
                    {
                        slot = i;
                        break;
                    }
                }
                if (slot == kMaxHookSlots)
                {
                    if (why) *why = "all native hook slots are in use";
                    return 0;
                }

                auto owned = std::make_unique<FuncPatch>();
                owned->Target = target;
                owned->Slot = slot;

                std::string reason;
                void* original = nullptr;
                if (!owned->Hook.Install(target, reinterpret_cast<void*>(g_SlotTable[slot]),
                                         &original, &reason))
                {
                    if (why) *why = "the native entry could not be patched: " + reason;
                    return 0;
                }

                patch = owned.get();
                patch->Original = original;
                g_SlotPatches[slot] = patch;
                g_TargetPatches[target] = patch;
                g_Patches.push_back(std::move(owned));

                char line[512];
                std::snprintf(line, sizeof(line),
                              "[CppMod] Native hook installed on %s::%s at %p (slot %u)",
                              className.c_str(), functionName.c_str(), target, slot);
                LogLine(line);
            }

            const uint32_t id = g_NextHookId.fetch_add(1, std::memory_order_relaxed);
            HookRecord record;
            record.Id = id;
            record.Function = function;
            record.Pre = pre;
            record.Post = post;
            record.Veto = veto;
            record.User = userData;
            record.Owner = owner;
            patch->Records.push_back(record);
            g_HookIdPatches[id] = patch;

            char line[384];
            std::snprintf(line, sizeof(line), "[CppMod] Hook %u registered for %s::%s", id,
                          className.c_str(), functionName.c_str());
            LogLine(line);
            return id;
        }
    }

    uint32_t RegisterHook(EngineState& state, const std::string& className,
                          const std::string& functionName, AvaloniumHookCallback pre,
                          AvaloniumHookCallback post, void* userData, void* owner,
                          std::string* why)
    {
        if (!ValidateHookRequest(state, pre, post, nullptr, why)) return 0;

        UFunction* function = ResolveFunction(state, className, functionName, why);
        if (!function) return 0;
        return RegisterResolvedHook(state, function, pre, post, nullptr, userData, owner, why);
    }

    uint32_t RegisterHookByFunction(EngineState& state, UFunction* function,
                                    AvaloniumHookCallback pre, AvaloniumHookCallback post,
                                    void* userData, void* owner, std::string* why)
    {
        if (!ValidateHookRequest(state, pre, post, nullptr, why)) return 0;
        if (!function)
        {
            if (why) *why = "the function pointer is null";
            return 0;
        }

        UClass* functionClass =
            EngineDiscovery::GetObjectClass(state, reinterpret_cast<UObject*>(function));
        if (!functionClass || !state.UFunctionClass ||
            !EngineDiscovery::IsStructA(state, reinterpret_cast<UStruct*>(functionClass),
                                        reinterpret_cast<UStruct*>(state.UFunctionClass)))
        {
            if (why) *why = "the pointer is not a live UFunction";
            return 0;
        }

        return RegisterResolvedHook(state, function, pre, post, nullptr, userData, owner, why);
    }

    uint32_t RegisterHookVeto(EngineState& state, const std::string& className,
                              const std::string& functionName, AvaloniumVetoCallback veto,
                              void* userData, void* owner, std::string* why)
    {
        if (!ValidateHookRequest(state, nullptr, nullptr, veto, why)) return 0;

        UFunction* function = ResolveFunction(state, className, functionName, why);
        if (!function) return 0;
        return RegisterResolvedHook(state, function, nullptr, nullptr, veto, userData, owner,
                                    why);
    }

    uint32_t RegisterAddressVeto(EngineState& state, void* target, AvaloniumVetoCallback veto,
                                 void* userData, void* owner, std::string* why)
    {
        if (!target || !veto)
        {
            if (why) *why = "the target or the veto is null";
            return 0;
        }
        if (!state.bInitialized)
        {
            if (why) *why = "the engine is not initialized";
            return 0;
        }

        std::lock_guard<std::mutex> lock(g_Mutex);
        uint32_t slot = kMaxHookSlots;
        for (uint32_t i = 0; i < kMaxHookSlots; ++i)
        {
            if (g_SlotPatches[i] == nullptr && g_RawSlotPatches[i] == nullptr)
            {
                slot = i;
                break;
            }
        }
        if (slot == kMaxHookSlots)
        {
            if (why) *why = "all native hook slots are in use";
            return 0;
        }

        auto owned = std::make_unique<RawPatch>();
        owned->Slot = slot;

        std::string reason;
        void* original = nullptr;
        if (!owned->Hook.Install(target, reinterpret_cast<void*>(g_SlotTable[slot]), &original,
                                 &reason))
        {
            if (why) *why = "the raw entry could not be patched: " + reason;
            return 0;
        }

        owned->Original = original;
        owned->Veto = veto;
        owned->User = userData;
        owned->Owner = owner;
        g_RawSlotPatches[slot] = owned.get();
        g_RawPatches.push_back(std::move(owned));

        char line[384];
        std::snprintf(line, sizeof(line), "[CppMod] Address veto installed at %p (slot %u)",
                      target, slot);
        LogLine(line);
        return g_NextHookId.fetch_add(1, std::memory_order_relaxed);
    }

    bool UnregisterHook(uint32_t hookId)
    {
        void* scriptFunction = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_ScriptMutex);
            for (auto it = g_ScriptHooks.begin(); it != g_ScriptHooks.end(); ++it)
            {
                std::vector<ScriptHookRecord>& records = it->second;
                const auto recordIt =
                    std::find_if(records.begin(), records.end(),
                                 [hookId](const ScriptHookRecord& record)
                                 { return record.Id == hookId; });
                if (recordIt == records.end()) continue;

                const ScriptHookRecord removed = *recordIt;
                scriptFunction = it->first;
                records.erase(recordIt);
                g_ScriptHookCount.fetch_sub(1, std::memory_order_acq_rel);
                if (records.empty()) g_ScriptHooks.erase(it);

                if (!removed.IdentityKey.empty())
                {
                    auto identityIt = g_ScriptHooksByIdentity.find(removed.IdentityKey);
                    if (identityIt != g_ScriptHooksByIdentity.end())
                    {
                        std::vector<ScriptHookRecord>& identityRecords = identityIt->second;
                        identityRecords.erase(
                            std::remove_if(identityRecords.begin(), identityRecords.end(),
                                           [hookId](const ScriptHookRecord& record)
                                           { return record.Id == hookId; }),
                            identityRecords.end());
                        if (identityRecords.empty()) g_ScriptHooksByIdentity.erase(identityIt);
                    }
                }
                break;
            }
        }
        if (scriptFunction)
        {
            char line[256];
            std::snprintf(line, sizeof(line), "[CppMod] Blueprint hook %u removed at %p", hookId,
                          scriptFunction);
            LogLine(line);
            return true;
        }

        std::lock_guard<std::mutex> lock(g_Mutex);

        auto idIt = g_HookIdPatches.find(hookId);
        if (idIt == g_HookIdPatches.end()) return false;

        FuncPatch* patch = idIt->second;
        g_HookIdPatches.erase(idIt);

        patch->Records.erase(
            std::remove_if(patch->Records.begin(), patch->Records.end(),
                           [hookId](const HookRecord& record) { return record.Id == hookId; }),
            patch->Records.end());

        if (patch->Records.empty())
        {
            patch->Hook.Remove();
            g_TargetPatches.erase(patch->Target);

            auto owned = std::find_if(g_Patches.begin(), g_Patches.end(),
                                      [patch](const std::unique_ptr<FuncPatch>& candidate)
                                      { return candidate.get() == patch; });
            if (owned != g_Patches.end())
            {
                g_Retired.push_back(std::move(*owned));
                g_Patches.erase(owned);
            }

            char line[256];
            std::snprintf(line, sizeof(line), "[CppMod] Native hook removed at %p (slot %u)",
                          patch->Target, patch->Slot);
            LogLine(line);
        }

        return true;
    }

    void UnregisterHooksForOwner(void* owner)
    {
        std::vector<uint32_t> ids;
        {
            std::lock_guard<std::mutex> lock(g_ScriptMutex);
            for (const auto& entry : g_ScriptHooks)
            {
                for (const ScriptHookRecord& record : entry.second)
                {
                    if (record.Owner == owner) ids.push_back(record.Id);
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(g_Mutex);
            for (const auto& [id, patch] : g_HookIdPatches)
            {
                for (const HookRecord& record : patch->Records)
                {
                    if (record.Owner == owner)
                    {
                        ids.push_back(id);
                        break;
                    }
                }
            }
        }
        for (uint32_t id : ids) UnregisterHook(id);
    }

    bool WaitForHooksIdle(int timeoutMs)
    {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (g_InFlight.load(std::memory_order_acquire) != 0)
        {
            if (std::chrono::steady_clock::now() >= deadline) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

    void ShutdownHooks()
    {
        std::vector<uint32_t> ids;
        {
            std::lock_guard<std::mutex> lock(g_ScriptMutex);
            for (const auto& entry : g_ScriptHooks)
            {
                for (const ScriptHookRecord& record : entry.second) ids.push_back(record.Id);
            }
        }
        {
            std::lock_guard<std::mutex> lock(g_Mutex);
            ids.reserve(ids.size() + g_HookIdPatches.size());
            for (const auto& entry : g_HookIdPatches) ids.push_back(entry.first);
        }
        for (uint32_t id : ids) UnregisterHook(id);
    }
}
