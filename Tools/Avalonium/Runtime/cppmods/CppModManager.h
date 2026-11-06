#pragma once

#include "../core/Types.h"
#include "CppModWidgets.h"
#include "sdk/Avalonium/ModApi.h"
#include "sdk/Avalonium/ModBase.hpp"

#include <windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Avalonium::CppMods
{
    struct LoadedMod
    {
        using StartFn = RC::CppUserModBase* (*)();
        using UninstallFn = void (*)(RC::CppUserModBase*);

        std::string Name;
        std::string FolderPath;
        std::string DllPath;
        HMODULE Module = nullptr;
        RC::CppUserModBase* Instance = nullptr;
        StartFn Start = nullptr;
        UninstallFn Uninstall = nullptr;
        DLL_DIRECTORY_COOKIE DllDirectoryCookie = nullptr;
        std::atomic<bool> Alive{true};
    };

    class CppModManager
    {
      public:
        static CppModManager& Get();

        bool Initialize(HMODULE avaloniumModule);
        bool LoadModsFromDirectory(const std::string& modsDirectory);
        void Shutdown(bool processExiting);

        bool IsInitialized() const { return m_Initialized.load(std::memory_order_acquire); }
        bool IsRunning() const { return m_Running.load(std::memory_order_acquire); }

        const std::string& GetModsDirectory() const { return m_ModsDirectory; }
        std::size_t GetLoadedModCount() const { return m_Mods.size(); }

        bool RunOnGameThread(AvaloniumTaskCallback callback, void* userData);
        uint32_t RegisterKeyBind(const std::string& keyName, AvaloniumKeyCallback callback,
                                 void* userData);
        bool UnregisterKeyBind(uint32_t keyId);

        uint32_t BindWidgetClick(void* widget, AvaloniumWidgetCallback callback, void* userData);
        uint32_t BindWidgetEvent(void* widget, const char* eventName,
                                 AvaloniumWidgetCallback callback, void* userData);
        bool UnbindWidgetClick(uint32_t bindingId);

      private:
        CppModManager() = default;
        ~CppModManager() = default;
        CppModManager(const CppModManager&) = delete;
        CppModManager& operator=(const CppModManager&) = delete;

        struct Task
        {
            void* Owner = nullptr;
            AvaloniumTaskCallback Callback = nullptr;
            void* User = nullptr;
        };

        struct KeyBindRecord
        {
            void* Owner = nullptr;
            AvaloniumKeyCallback Callback = nullptr;
            void* User = nullptr;
            std::atomic<bool> Active{true};
        };

        struct WidgetDelegateAttachment
        {
            void* Widget = nullptr;
            void* Property = nullptr;

            int32_t ObjectIndex = -1;
            int32_t ObjectSerial = 0;
            int32_t Offset = 0;
            int32_t Stride = 0;
            bool Multicast = false;
            std::string EventName;
            void* Dispatch = nullptr;
            uint32_t DispatchNameIndex = 0;
            Widgets::ScriptDelegateValue Original;
            uint32_t ActiveBindings = 0;
        };

        struct WidgetClickRecord
        {
            void* Owner = nullptr;
            WidgetDelegateAttachment* Attachment = nullptr;
            AvaloniumWidgetCallback Callback = nullptr;
            void* User = nullptr;
            std::atomic<bool> Active{true};
        };

        static void FrameSinkThunk(void* user);
        static void KeyBindThunk(void* user);
        static void ProcessEventSinkThunk(void* object, void* function, void* params);

        void OnGameThreadFrame();
        bool EnsureHookManager();
        void FireLifecycle();
        void DrainTasks();
        void InvokeLifecycle(RC::CppUserModBase* instance, void (RC::CppUserModBase::*callback)());
        bool IsKnownOwner(void* owner) const;
        void UnregisterKeyBindsForOwner(void* owner);
        void UnbindWidgetClicksForOwner(void* owner);
        void DispatchWidgetClick(void* object, void* function, void* params);
        void RemoveTasksForOwner(void* owner);
        bool CleanupFailedModRegistrations(LoadedMod& mod);

        std::atomic<bool> m_Initialized{false};
        std::atomic<bool> m_Running{false};
        std::atomic<bool> m_LifecycleFired{false};
        std::atomic<bool> m_LoadedOnce{false};
        std::atomic<int> m_FrameInFlight{0};
        std::atomic<int> m_KeyInFlight{0};
        std::atomic<int> m_WidgetClickInFlight{0};
        std::atomic<int> m_WidgetClickCount{0};
        bool m_HookEnsured = false;

        HMODULE m_Module = nullptr;
        std::string m_ModsDirectory;
        std::vector<std::unique_ptr<LoadedMod>> m_Mods;
        std::vector<std::unique_ptr<LoadedMod>> m_RetiredMods;

        std::mutex m_TaskMutex;
        std::deque<Task> m_Tasks;

        std::mutex m_FrameMutex;

        std::mutex m_KeyMutex;
        std::unordered_map<uint32_t, std::unique_ptr<KeyBindRecord>> m_KeyBinds;

        std::mutex m_WidgetMutex;
        std::map<std::pair<void*, int32_t>, std::unique_ptr<WidgetDelegateAttachment>>
            m_WidgetAttachments;
        std::unordered_map<uint32_t, std::unique_ptr<WidgetClickRecord>> m_WidgetClicks;
        std::atomic<uint32_t> m_NextWidgetClickId{1};
    };
}
