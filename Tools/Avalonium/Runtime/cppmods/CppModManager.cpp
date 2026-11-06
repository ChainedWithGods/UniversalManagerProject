#include "CppModManager.h"

#include "CppModContext.h"
#include "CppModHooks.h"
#include "CppModImGui.h"
#include "CppModWidgets.h"

#include "../core/EngineDiscovery.h"
#include "../core/Logger.h"
#include "../lua/LuaHookManager.h"
#include "../lua/LuaManager.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <thread>
#include <utility>
#include <vector>

extern EngineState g_State;

namespace Avalonium::CppMods
{
    namespace
    {
        void LogLine(const std::string& line)
        {
            EngineDiscovery::WriteLogLine(line.c_str());
        }

        std::string Trim(std::string text)
        {
            const auto first = text.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) return {};
            const auto last = text.find_last_not_of(" \t\r\n");
            return text.substr(first, last - first + 1);
        }

        std::string Lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        bool IsModEnabledByFile(const std::filesystem::path& modDirectory)
        {
            std::error_code ec;
            const std::filesystem::path marker = modDirectory / "enabled.txt";
            if (!std::filesystem::exists(marker, ec)) return true;

            std::ifstream file(marker);
            if (!file.is_open()) return true;

            std::string line;
            if (!std::getline(file, line)) return true;
            line = Lower(Trim(line));
            if (line.empty()) return true;
            return !(line == "0" || line == "false" || line == "no" || line == "disabled" ||
                     line == "off");
        }

        bool ResolveModDll(const std::filesystem::path& modDirectory, const std::string& name,
                           std::filesystem::path& dllDirectoryOut,
                           std::filesystem::path& dllPathOut)
        {
            std::error_code ec;
            const std::filesystem::path dlls = modDirectory / "dlls";
            if (std::filesystem::is_directory(dlls, ec))
            {
                const std::filesystem::path mainDll = dlls / "main.dll";
                if (std::filesystem::is_regular_file(mainDll, ec))
                {
                    dllDirectoryOut = dlls;
                    dllPathOut = mainDll;
                    return true;
                }

                const std::filesystem::path namedDll = dlls / (name + ".dll");
                if (std::filesystem::is_regular_file(namedDll, ec))
                {
                    dllDirectoryOut = dlls;
                    dllPathOut = namedDll;
                    return true;
                }

                LogLine("[CppMod] Mod '" + name +
                        "' has a dlls folder but neither main.dll nor " + name + ".dll");
                return false;
            }

            const std::filesystem::path rootDll = modDirectory / (name + ".dll");
            if (std::filesystem::is_regular_file(rootDll, ec))
            {
                dllDirectoryOut = modDirectory;
                dllPathOut = rootDll;
                return true;
            }

            return false;
        }
    }

    CppModManager& CppModManager::Get()
    {
        static CppModManager manager;
        return manager;
    }

    bool CppModManager::Initialize(HMODULE avaloniumModule)
    {
        if (m_Initialized.load(std::memory_order_acquire)) return true;

        m_Module = avaloniumModule;

        char dllPath[MAX_PATH] = {};
        if (GetModuleFileNameA(avaloniumModule, dllPath, MAX_PATH))
        {
            std::filesystem::path runtimePath(dllPath);
            m_ModsDirectory = (runtimePath.parent_path() / "Mods" / "Avalonium").string();
        }
        else
        {
            m_ModsDirectory = "Mods/Avalonium";
        }

        m_Running.store(true, std::memory_order_release);
        m_Initialized.store(true, std::memory_order_release);
        LogLine("[CppMod] C++ mod runtime initialized");
        return true;
    }

    bool CppModManager::EnsureHookManager()
    {
        if (m_HookEnsured) return LuaEngine::LuaHookManager::Get().IsHookInstalled();
        m_HookEnsured = true;

        LuaEngine::LuaHookManager& hookManager = LuaEngine::LuaHookManager::Get();
        if (!hookManager.IsHookInstalled())
        {
            if (LuaManager::Get().GetState() == nullptr)
            {
                if (!hookManager.Initialize(nullptr))
                {
                    LogLine("[CppMod] Warning: the engine ProcessEvent entry could not be hooked; "
                            "C++ mod lifecycle callbacks will run on the loader thread and "
                            "on_update will not tick");
                }
            }
            else
            {
                LogLine("[CppMod] Warning: Lua owns the ProcessEvent hook but it is not installed; "
                        "C++ mods will not tick");
            }
        }

        return hookManager.IsHookInstalled();
    }

    bool CppModManager::LoadModsFromDirectory(const std::string& modsDirectory)
    {
        if (!m_Initialized.load(std::memory_order_acquire))
        {
            LogLine("[CppMod] LoadModsFromDirectory called before Initialize");
            return false;
        }

        bool expected = false;
        if (!m_LoadedOnce.compare_exchange_strong(expected, true))
        {
            LogLine("[CppMod] C++ mod reload is not supported; the existing mods stay loaded");
            return true;
        }

        const bool explicitDirectory = !modsDirectory.empty();
        m_ModsDirectory = explicitDirectory ? modsDirectory : m_ModsDirectory;
        if (m_ModsDirectory.empty()) m_ModsDirectory = "Mods/Avalonium";

        std::error_code ec;
        if (!std::filesystem::exists(m_ModsDirectory, ec) ||
            !std::filesystem::is_directory(m_ModsDirectory, ec))
        {

            if (explicitDirectory)
                LogLine("[CppMod] Mods directory does not exist: " + m_ModsDirectory);
            return true;
        }

        std::vector<std::filesystem::path> directories;
        for (const auto& entry : std::filesystem::directory_iterator(m_ModsDirectory, ec))
        {
            if (!entry.is_directory(ec)) continue;
            const std::string directoryName = entry.path().filename().string();
            if (directoryName == "shared" || directoryName == "Cache") continue;
            directories.push_back(entry.path());
        }
        std::sort(directories.begin(), directories.end());

        auto abandonMod = [&](std::unique_ptr<LoadedMod>& modPtr, HMODULE module) {
            LoadedMod& mod = *modPtr;
            mod.Alive.store(false, std::memory_order_release);
            const bool idle = CleanupFailedModRegistrations(mod);
            if (!idle)
            {

                LogLine("[CppMod] Mod '" + mod.Name +
                        "' is left loaded because one of its callbacks is still running");
                m_RetiredMods.push_back(std::move(modPtr));
                return;
            }
            if (module) FreeLibrary(module);
            if (mod.DllDirectoryCookie) RemoveDllDirectory(mod.DllDirectoryCookie);
        };

        for (const std::filesystem::path& directory : directories)
        {
            const std::string folderName = directory.filename().string();
            if (!IsModEnabledByFile(directory))
            {
                LogLine("[CppMod] Mod '" + folderName + "' is disabled by enabled.txt");
                continue;
            }

            std::filesystem::path dllDirectory;
            std::filesystem::path dllPath;
            if (!ResolveModDll(directory, folderName, dllDirectory, dllPath))
                continue;

            auto mod = std::make_unique<LoadedMod>();
            mod->Name = folderName;
            mod->FolderPath = directory.string();
            mod->DllPath = dllPath.string();

            if (!dllDirectory.empty())
            {
                mod->DllDirectoryCookie = AddDllDirectory(dllDirectory.c_str());
            }

            ModContextScope loadScope(mod.get());

            HMODULE module = LoadLibraryExW(
                dllPath.c_str(), nullptr,
                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if (!module)
            {
                char line[512];
                std::snprintf(line, sizeof(line),
                              "[CppMod] Failed to load mod '%s' from %s (error %lu)",
                              folderName.c_str(), dllPath.string().c_str(),
                              static_cast<unsigned long>(GetLastError()));
                LogLine(line);
                abandonMod(mod, module);
                continue;
            }
            mod->Module = module;

            using AbiVersionFn = uint32_t (*)();
            auto abiVersion = reinterpret_cast<AbiVersionFn>(
                GetProcAddress(module, "AvaloniumModAbiVersion"));
            if (!abiVersion)
            {
                LogLine("[CppMod] Mod '" + folderName +
                        "' is not an Avalonium native mod (missing AvaloniumModAbiVersion); "
                        "other runtimes' C++ mod binaries are not compatible");
                abandonMod(mod, module);
                continue;
            }

            uint32_t abi = 0;
            {
                ModContextScope scope(mod.get());
                try
                {
                    abi = abiVersion();
                }
                catch (...)
                {
                    abi = 0;
                }
            }
            if (abi != AVALONIUM_MOD_ABI_VERSION)
            {
                char line[512];
                std::snprintf(line, sizeof(line),
                              "[CppMod] Mod '%s' reports mod ABI %u; this runtime accepts ABI %u",
                              folderName.c_str(), abi, AVALONIUM_MOD_ABI_VERSION);
                LogLine(line);
                abandonMod(mod, module);
                continue;
            }

            mod->Start = reinterpret_cast<LoadedMod::StartFn>(
                GetProcAddress(module, "start_mod"));
            mod->Uninstall = reinterpret_cast<LoadedMod::UninstallFn>(
                GetProcAddress(module, "uninstall_mod"));

            if (!mod->Start || !mod->Uninstall)
            {
                LogLine("[CppMod] Mod '" + folderName +
                        "' has no start_mod/uninstall_mod exports; it is not a C++ mod");
                abandonMod(mod, module);
                continue;
            }

            RC::CppUserModBase* instance = nullptr;
            {
                ModContextScope scope(mod.get());
                try
                {
                    instance = mod->Start();
                }
                catch (const std::exception& e)
                {
                    LogLine("[CppMod] Mod '" + folderName + "' threw during start_mod: " + e.what());
                }
                catch (...)
                {
                    LogLine("[CppMod] Mod '" + folderName + "' threw an unknown exception during start_mod");
                }
            }

            if (!instance)
            {
                LogLine("[CppMod] Mod '" + folderName + "' returned no mod object");
                abandonMod(mod, module);
                continue;
            }

            mod->Instance = instance;

            EnsureHookManager();

            const std::string displayName =
                instance->ModName.empty() ? folderName : instance->ModName;
            const std::string version =
                instance->ModVersion.empty() ? "unknown" : instance->ModVersion;
            const std::string authors =
                instance->ModAuthors.empty() ? "unknown" : instance->ModAuthors;

            char line[768];
            std::snprintf(line, sizeof(line),
                          "[CppMod] Loaded '%s' v%s by %s (%s)",
                          displayName.c_str(), version.c_str(), authors.c_str(),
                          dllPath.string().c_str());
            LogLine(line);

            m_Mods.push_back(std::move(mod));
        }

        char summary[384];
        std::snprintf(summary, sizeof(summary), "[CppMod] %zu C++ mod(s) loaded from %s",
                      m_Mods.size(), m_ModsDirectory.c_str());
        LogLine(summary);

        if (m_Mods.empty()) return true;

        LuaEngine::LuaHookManager::Get().SetCppScriptFunctionSink(&DispatchScriptFunction);
        LogLine("[CppMod] Blueprint hook dispatch armed through ProcessLocalScriptFunction");

        if (!LuaEngine::LuaHookManager::Get().IsHookInstalled())
        {
            LogLine("[CppMod] The engine's ProcessEvent entry is not hooked; lifecycle callbacks "
                    "run on the loader thread and on_update is unavailable on this build");
            FireLifecycle();
        }
        else
        {

            LuaEngine::LuaHookManager::Get().SetFrameSink(&CppModManager::FrameSinkThunk, this);
            LuaEngine::LuaHookManager::Get().SetCppProcessEventSink(
                &CppModManager::ProcessEventSinkThunk);
        }
        return true;
    }

    void CppModManager::FrameSinkThunk(void* user)
    {
        static_cast<CppModManager*>(user)->OnGameThreadFrame();
    }

    void CppModManager::OnGameThreadFrame()
    {

        std::lock_guard<std::mutex> frameLock(m_FrameMutex);
        if (!m_Running.load(std::memory_order_acquire)) return;

        m_FrameInFlight.fetch_add(1, std::memory_order_acq_rel);
        struct FrameGuard
        {
            std::atomic<int>& counter;
            ~FrameGuard() { counter.fetch_sub(1, std::memory_order_acq_rel); }
        } guard{m_FrameInFlight};

        if (!m_LifecycleFired.load(std::memory_order_acquire)) FireLifecycle();

        DrainTasks();

        for (const std::unique_ptr<LoadedMod>& mod : m_Mods)
        {
            if (!mod->Instance || !mod->Alive.load(std::memory_order_acquire)) continue;

            ModContextScope scope(mod.get());
            try
            {
                mod->Instance->on_update();
            }
            catch (const std::exception& e)
            {
                static std::atomic<bool> reported{false};
                if (!reported.exchange(true, std::memory_order_relaxed))
                    LogLine("[CppMod] A mod's on_update threw an exception: " + std::string(e.what()));
            }
            catch (...)
            {
                static std::atomic<bool> reported{false};
                if (!reported.exchange(true, std::memory_order_relaxed))
                    LogLine("[CppMod] A mod's on_update threw an unknown exception");
            }
        }
    }

    void CppModManager::FireLifecycle()
    {
        bool expected = false;
        if (!m_LifecycleFired.compare_exchange_strong(expected, true)) return;

        for (const std::unique_ptr<LoadedMod>& mod : m_Mods)
        {
            if (!mod->Instance) continue;
            ModContextScope scope(mod.get());
            InvokeLifecycle(mod->Instance, &RC::CppUserModBase::on_program_start);
        }
        for (const std::unique_ptr<LoadedMod>& mod : m_Mods)
        {
            if (!mod->Instance) continue;
            ModContextScope scope(mod.get());
            InvokeLifecycle(mod->Instance, &RC::CppUserModBase::on_unreal_init);
        }
        for (const std::unique_ptr<LoadedMod>& mod : m_Mods)
        {
            if (!mod->Instance) continue;
            ModContextScope scope(mod.get());
            InvokeLifecycle(mod->Instance, &RC::CppUserModBase::on_cpp_mods_loaded);
        }

        char line[256];
        std::snprintf(line, sizeof(line), "[CppMod] Lifecycle callbacks delivered to %zu mod(s)",
                      m_Mods.size());
        LogLine(line);
    }

    void CppModManager::InvokeLifecycle(RC::CppUserModBase* instance,
                                        void (RC::CppUserModBase::*callback)())
    {
        try
        {
            (instance->*callback)();
        }
        catch (const std::exception& e)
        {
            LogLine("[CppMod] A lifecycle callback threw an exception: " + std::string(e.what()));
        }
        catch (...)
        {
            LogLine("[CppMod] A lifecycle callback threw an unknown exception");
        }
    }

    void CppModManager::DrainTasks()
    {
        std::deque<Task> tasks;
        {
            std::lock_guard<std::mutex> lock(m_TaskMutex);
            tasks.swap(m_Tasks);
        }

        for (const Task& task : tasks)
        {
            if (!task.Callback) continue;
            if (task.Owner && !IsKnownOwner(task.Owner)) continue;

            ModContextScope scope(task.Owner);
            try
            {
                task.Callback(task.User);
            }
            catch (const std::exception& e)
            {
                LogLine("[CppMod] A game-thread task threw an exception: " + std::string(e.what()));
            }
            catch (...)
            {
                LogLine("[CppMod] A game-thread task threw an unknown exception");
            }
        }
    }

    bool CppModManager::IsKnownOwner(void* owner) const
    {
        if (!owner) return true;
        for (const std::unique_ptr<LoadedMod>& mod : m_Mods)
        {
            if (mod.get() == owner && mod->Alive.load(std::memory_order_acquire)) return true;
        }
        return false;
    }

    void CppModManager::UnregisterKeyBindsForOwner(void* owner)
    {
        std::vector<uint32_t> ids;
        {
            std::lock_guard<std::mutex> lock(m_KeyMutex);
            for (const auto& entry : m_KeyBinds)
            {
                if (entry.second->Owner == owner &&
                    entry.second->Active.load(std::memory_order_acquire))
                {
                    ids.push_back(entry.first);
                }
            }
        }
        for (uint32_t id : ids) UnregisterKeyBind(id);
    }

    void CppModManager::RemoveTasksForOwner(void* owner)
    {
        std::lock_guard<std::mutex> lock(m_TaskMutex);
        m_Tasks.erase(std::remove_if(m_Tasks.begin(), m_Tasks.end(),
                                     [owner](const Task& task) { return task.Owner == owner; }),
                      m_Tasks.end());
    }

    bool CppModManager::CleanupFailedModRegistrations(LoadedMod& mod)
    {
        RemoveTasksForOwner(&mod);
        UnregisterKeyBindsForOwner(&mod);
        UnbindWidgetClicksForOwner(&mod);
        ImGuiRender::UnregisterOwner(&mod);
        UnregisterHooksForOwner(&mod);

        for (int i = 0; i < 1000 && m_KeyInFlight.load(std::memory_order_acquire) > 0; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const bool hooksIdle = WaitForHooksIdle(1000);
        return hooksIdle && m_KeyInFlight.load(std::memory_order_acquire) == 0;
    }

    bool CppModManager::RunOnGameThread(AvaloniumTaskCallback callback, void* userData)
    {
        if (!callback || !m_Running.load(std::memory_order_acquire)) return false;

        if (LuaEngine::LuaHookManager::Get().IsInGameThread())
        {
            ModContextScope scope(CurrentModToken());
            try
            {
                callback(userData);
            }
            catch (const std::exception& e)
            {
                LogLine("[CppMod] A game-thread task threw an exception: " + std::string(e.what()));
            }
            catch (...)
            {
                LogLine("[CppMod] A game-thread task threw an unknown exception");
            }
            return true;
        }

        {
            std::lock_guard<std::mutex> lock(m_TaskMutex);
            m_Tasks.push_back(Task{CurrentModToken(), callback, userData});
        }
        return true;
    }

    uint32_t CppModManager::RegisterKeyBind(const std::string& keyName,
                                            AvaloniumKeyCallback callback, void* userData)
    {
        if (!callback || !m_Running.load(std::memory_order_acquire)) return 0;

        void* owner = CurrentModToken();
        if (owner)
        {
            auto* mod = static_cast<LoadedMod*>(owner);
            if (!mod->Alive.load(std::memory_order_acquire)) return 0;
        }

        auto record = std::make_unique<KeyBindRecord>();
        record->Owner = owner;
        record->Callback = callback;
        record->User = userData;

        std::lock_guard<std::mutex> lock(m_KeyMutex);
        const uint32_t id = LuaEngine::LuaHookManager::Get().RegisterCppKeyBind(
            keyName, &CppModManager::KeyBindThunk, record.get());
        if (!id) return 0;

        m_KeyBinds[id] = std::move(record);
        return id;
    }

    bool CppModManager::UnregisterKeyBind(uint32_t keyId)
    {
        {
            std::lock_guard<std::mutex> lock(m_KeyMutex);
            auto it = m_KeyBinds.find(keyId);
            if (it == m_KeyBinds.end() || !it->second->Active.load(std::memory_order_acquire))
                return false;

            it->second->Active.store(false, std::memory_order_release);
        }

        LuaEngine::LuaHookManager::Get().UnregisterCppKeyBind(keyId);
        return true;
    }

    uint32_t CppModManager::BindWidgetClick(void* widget, AvaloniumWidgetCallback callback,
                                            void* userData)
    {
        return BindWidgetEvent(widget, "OnClicked", callback, userData);
    }

    uint32_t CppModManager::BindWidgetEvent(void* widget, const char* eventName,
                                            AvaloniumWidgetCallback callback, void* userData)
    {
        if (!widget || !eventName || !*eventName || !callback ||
            !m_Running.load(std::memory_order_acquire))
        {
            return 0;
        }
        if (!LuaEngine::LuaHookManager::Get().IsInGameThread())
        {
            LogLine("[CppMod] Warning: a widget delegate binding was refused off the game "
                    "thread; queue it with RC::RunOnGameThread");
            return 0;
        }

        Widgets::DelegateInfo delegate;
        std::string why;
        if (!Widgets::FindDelegate(g_State, widget, eventName, delegate, &why))
        {
            LogLine("[CppMod] Warning: binding widget delegate '" + std::string(eventName) +
                    "' failed: " + why);
            return 0;
        }

        int32_t objectIndex = -1;
        int32_t objectSerial = 0;
        if (!Widgets::GetObjectIdentity(g_State, widget, objectIndex, objectSerial))
        {
            LogLine("[CppMod] Warning: binding widget delegate '" + std::string(eventName) +
                    "' failed: the object has no object-array identity");
            return 0;
        }

        std::lock_guard<std::mutex> lock(m_WidgetMutex);

        void* owner = CurrentModToken();
        if (owner)
        {
            auto* mod = static_cast<LoadedMod*>(owner);
            if (!mod->Alive.load(std::memory_order_acquire)) return 0;
        }

        const std::pair<void*, int32_t> key{widget, delegate.Offset};
        auto found = m_WidgetAttachments.find(key);
        if (found != m_WidgetAttachments.end())
        {
            const WidgetDelegateAttachment* existing = found->second.get();
            if (!existing || existing->ObjectIndex != objectIndex ||
                existing->ObjectSerial != objectSerial)
            {

                m_WidgetAttachments.erase(found);
                found = m_WidgetAttachments.end();
            }
        }
        WidgetDelegateAttachment* attachment =
            found != m_WidgetAttachments.end() ? found->second.get() : nullptr;

        if (!attachment)
        {

            std::vector<uint32_t> used;
            for (const auto& entry : m_WidgetAttachments)
            {
                if (entry.second && entry.second->Widget == widget)
                    used.push_back(entry.second->DispatchNameIndex);
            }

            void* dispatch = nullptr;
            uint32_t dispatchName = 0;
            if (!Widgets::PickDispatchFunction(g_State, widget, used, dispatch, dispatchName,
                                               &why))
            {
                LogLine("[CppMod] Warning: binding widget delegate '" +
                        std::string(eventName) + "' failed: " + why);
                return 0;
            }

            auto created = std::make_unique<WidgetDelegateAttachment>();
            created->Widget = widget;
            created->Property = delegate.Property;
            created->ObjectIndex = objectIndex;
            created->ObjectSerial = objectSerial;
            created->Offset = delegate.Offset;
            created->Stride = delegate.Stride;
            created->Multicast = delegate.Multicast;
            created->EventName = eventName;
            created->Dispatch = dispatch;
            created->DispatchNameIndex = dispatchName;

            const bool attached =
                delegate.Multicast
                    ? Widgets::AttachMulticast(g_State, widget, delegate, dispatchName, &why)
                    : Widgets::AttachUnicast(g_State, widget, delegate, dispatchName,
                                             created->Original, &why);
            if (!attached)
            {
                LogLine("[CppMod] Warning: binding widget delegate '" +
                        std::string(eventName) + "' failed: " + why);
                return 0;
            }

            attachment = created.get();
            m_WidgetAttachments.emplace(key, std::move(created));
        }

        auto record = std::make_unique<WidgetClickRecord>();
        record->Owner = owner;
        record->Attachment = attachment;
        record->Callback = callback;
        record->User = userData;

        const uint32_t id = m_NextWidgetClickId.fetch_add(1, std::memory_order_relaxed);
        m_WidgetClicks[id] = std::move(record);
        attachment->ActiveBindings++;
        m_WidgetClickCount.fetch_add(1, std::memory_order_release);
        return id;
    }

    bool CppModManager::UnbindWidgetClick(uint32_t bindingId)
    {
        std::unique_ptr<WidgetClickRecord> record;
        std::unique_ptr<WidgetDelegateAttachment> attachment;
        {
            std::lock_guard<std::mutex> lock(m_WidgetMutex);
            auto it = m_WidgetClicks.find(bindingId);
            if (it == m_WidgetClicks.end()) return false;
            it->second->Active.store(false, std::memory_order_release);
            record = std::move(it->second);
            m_WidgetClicks.erase(it);
            m_WidgetClickCount.fetch_sub(1, std::memory_order_release);

            WidgetDelegateAttachment* target = record->Attachment;
            if (target && target->ActiveBindings > 0) --target->ActiveBindings;
            if (target && target->ActiveBindings == 0)
            {
                for (auto entry = m_WidgetAttachments.begin();
                     entry != m_WidgetAttachments.end(); ++entry)
                {
                    if (entry->second.get() == target)
                    {
                        attachment = std::move(entry->second);
                        m_WidgetAttachments.erase(entry);
                        break;
                    }
                }
            }
        }

        if (!attachment) return true;

        if (attachment->Multicast)
        {
            if (LuaEngine::LuaHookManager::Get().IsInGameThread())
            {
                Widgets::DetachMulticast(g_State, attachment->Widget, Widgets::DelegateInfo{
                    attachment->Property, attachment->Offset, 0, attachment->Stride, true},
                    attachment->DispatchNameIndex);
            }
        }
        else
        {
            Widgets::DetachUnicast(g_State, attachment->Widget, Widgets::DelegateInfo{
                attachment->Property, attachment->Offset, 0, attachment->Stride, false},
                attachment->DispatchNameIndex, attachment->Original);
        }
        return true;
    }

    void CppModManager::UnbindWidgetClicksForOwner(void* owner)
    {
        std::vector<uint32_t> ids;
        {
            std::lock_guard<std::mutex> lock(m_WidgetMutex);
            for (const auto& entry : m_WidgetClicks)
            {
                if (entry.second && entry.second->Owner == owner) ids.push_back(entry.first);
            }
        }
        for (uint32_t id : ids) UnbindWidgetClick(id);
    }

    void CppModManager::ProcessEventSinkThunk(void* object, void* function, void* params)
    {
        Get().DispatchWidgetClick(object, function, params);
    }

    void CppModManager::DispatchWidgetClick(void* object, void* function, void* params)
    {
        if (m_WidgetClickCount.load(std::memory_order_acquire) == 0) return;

        struct PendingCallback
        {
            AvaloniumWidgetCallback Callback = nullptr;
            void* User = nullptr;
            void* Owner = nullptr;
        };

        std::vector<PendingCallback> callbacks;
        Widgets::ScriptDelegateValue original;
        bool forwardOriginal = false;
        bool counted = false;

        {
            std::lock_guard<std::mutex> lock(m_WidgetMutex);
            uint32_t functionNameIndex = 0;
            bool functionNameRead = false;

            for (const auto& entry : m_WidgetAttachments)
            {
                WidgetDelegateAttachment* attachment = entry.second.get();
                if (!attachment || attachment->Widget != object) continue;

                bool matches = attachment->Dispatch == function;
                if (!matches)
                {
                    if (!functionNameRead)
                    {
                        functionNameIndex = Widgets::GetFunctionNameIndex(g_State, function);
                        functionNameRead = true;
                    }
                    matches = functionNameIndex != 0 &&
                              attachment->DispatchNameIndex == functionNameIndex;
                }
                if (!matches) continue;
                if (!Widgets::IsSameObject(g_State, object, attachment->ObjectIndex,
                                           attachment->ObjectSerial))
                {
                    continue;
                }

                for (const auto& binding : m_WidgetClicks)
                {
                    const std::unique_ptr<WidgetClickRecord>& record = binding.second;
                    if (!record || record->Attachment != attachment) continue;
                    if (!record->Active.load(std::memory_order_acquire)) continue;
                    callbacks.push_back({record->Callback, record->User, record->Owner});
                }

                if (!attachment->Multicast && attachment->Original.Bound())
                {
                    original = attachment->Original;
                    forwardOriginal = true;
                }
                break;
            }

            if (!callbacks.empty() || forwardOriginal)
            {
                m_WidgetClickInFlight.fetch_add(1, std::memory_order_acq_rel);
                counted = true;
            }
        }
        if (!counted) return;

        struct ClickGuard
        {
            std::atomic<int>& counter;
            ~ClickGuard() { counter.fetch_sub(1, std::memory_order_acq_rel); }
        } guard{m_WidgetClickInFlight};

        if (forwardOriginal)
        {
            void* originalObject = Widgets::ResolveWeakObject(g_State, original.ObjectIndex,
                                                              original.Serial);
            if (originalObject)
            {
                void* originalFunction = Widgets::ResolveDelegateFunction(
                    g_State, originalObject, original.FunctionNameIndex);
                if (originalFunction)
                {
                    EngineDiscovery::CallProcessEvent(g_State, originalObject, originalFunction,
                                                      params);
                }
            }
        }

        for (const PendingCallback& pending : callbacks)
        {
            if (pending.Owner && !IsKnownOwner(pending.Owner)) continue;

            ModContextScope scope(pending.Owner);
            try
            {
                pending.Callback(pending.User);
            }
            catch (const std::exception& e)
            {
                LogLine("[CppMod] A widget delegate callback threw an exception: " +
                        std::string(e.what()));
            }
            catch (...)
            {
                LogLine("[CppMod] A widget delegate callback threw an unknown exception");
            }
        }
    }

    void CppModManager::KeyBindThunk(void* user)
    {
        auto* record = static_cast<KeyBindRecord*>(user);
        if (!record) return;

        CppModManager& manager = Get();
        {

            std::lock_guard<std::mutex> lock(manager.m_KeyMutex);
            if (!manager.m_Running.load(std::memory_order_acquire)) return;
            if (!record->Active.load(std::memory_order_acquire)) return;
            manager.m_KeyInFlight.fetch_add(1, std::memory_order_acq_rel);
        }
        struct KeyGuard
        {
            std::atomic<int>& counter;
            ~KeyGuard() { counter.fetch_sub(1, std::memory_order_acq_rel); }
        } guard{manager.m_KeyInFlight};

        AvaloniumKeyCallback callback = record->Callback;
        void* userData = record->User;
        ModContextScope scope(record->Owner);

        try
        {
            if (callback) callback(userData);
        }
        catch (const std::exception& e)
        {
            LogLine("[CppMod] A key bind threw an exception: " + std::string(e.what()));
        }
        catch (...)
        {
            LogLine("[CppMod] A key bind threw an unknown exception");
        }
    }

    void CppModManager::Shutdown(bool processExiting)
    {
        if (!m_Initialized.exchange(false)) return;

        m_Running.store(false, std::memory_order_release);
        LuaEngine::LuaHookManager::Get().SetFrameSink(nullptr, nullptr);
        LuaEngine::LuaHookManager::Get().SetCppProcessEventSink(nullptr);
        LuaEngine::LuaHookManager::Get().SetCppScriptFunctionSink(nullptr);

        std::unique_lock<std::mutex> frameLock(m_FrameMutex, std::defer_lock);
        if (!processExiting) frameLock.lock();

        if (!processExiting)
        {
            for (int i = 0; i < 2000 && m_FrameInFlight.load(std::memory_order_acquire) > 0; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::vector<uint32_t> keyIds;
        {
            std::lock_guard<std::mutex> lock(m_KeyMutex);
            keyIds.reserve(m_KeyBinds.size());
            for (const auto& entry : m_KeyBinds) keyIds.push_back(entry.first);
        }
        for (uint32_t id : keyIds) UnregisterKeyBind(id);

        std::vector<uint32_t> widgetIds;
        {
            std::lock_guard<std::mutex> lock(m_WidgetMutex);
            widgetIds.reserve(m_WidgetClicks.size());
            for (const auto& entry : m_WidgetClicks) widgetIds.push_back(entry.first);
        }
        for (uint32_t id : widgetIds) UnbindWidgetClick(id);

        ImGuiRender::Shutdown();

        if (!processExiting)
        {
            for (int i = 0; i < 2000 &&
                            (m_KeyInFlight.load(std::memory_order_acquire) > 0 ||
                             m_WidgetClickInFlight.load(std::memory_order_acquire) > 0 ||
                             !ImGuiRender::WaitForIdle(0));
                 ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        for (const std::unique_ptr<LoadedMod>& mod : m_Mods)
            UnregisterHooksForOwner(mod.get());
        for (const std::unique_ptr<LoadedMod>& mod : m_RetiredMods)
            UnregisterHooksForOwner(mod.get());

        ShutdownHooks();

        if (processExiting)
        {

            LogLine("[CppMod] Process is exiting; hook and key-bind registrations removed and "
                    "mod DLLs left to the OS");
            return;
        }

        const bool hooksIdle = WaitForHooksIdle(2000);

        if (!hooksIdle || !ImGuiRender::WaitForIdle(0) ||
            m_FrameInFlight.load(std::memory_order_acquire) > 0 ||
            m_KeyInFlight.load(std::memory_order_acquire) > 0 ||
            m_WidgetClickInFlight.load(std::memory_order_acquire) > 0)
        {
            LogLine("[CppMod] Warning: a mod callback is still running; mod DLLs are left loaded "
                    "to avoid unloading code that is in use");
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_TaskMutex);
            m_Tasks.clear();
        }

        for (auto it = m_Mods.rbegin(); it != m_Mods.rend(); ++it)
        {
            LoadedMod& mod = **it;
            mod.Alive.store(false, std::memory_order_release);

            ModContextScope scope(&mod);

            if (mod.Instance)
            {
                try
                {
                    mod.Instance->on_unload();
                }
                catch (const std::exception& e)
                {
                    LogLine("[CppMod] Mod '" + mod.Name + "' threw during on_unload: " + e.what());
                }
                catch (...)
                {
                    LogLine("[CppMod] Mod '" + mod.Name + "' threw an unknown exception during on_unload");
                }

                try
                {
                    if (mod.Uninstall) mod.Uninstall(mod.Instance);
                }
                catch (const std::exception& e)
                {
                    LogLine("[CppMod] Mod '" + mod.Name + "' threw during uninstall_mod: " + e.what());
                }
                catch (...)
                {
                    LogLine("[CppMod] Mod '" + mod.Name + "' threw an unknown exception during uninstall_mod");
                }
            }

            if (mod.DllDirectoryCookie) RemoveDllDirectory(mod.DllDirectoryCookie);
            if (mod.Module) FreeLibrary(mod.Module);
            LogLine("[CppMod] Unloaded '" + mod.Name + "'");
        }
        m_Mods.clear();
    }
}
