#include "CppModImGui.h"

#include "CppModContext.h"
#include "CppModManager.h"

#include "../core/EngineDiscovery.h"

#include <imgui.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Avalonium::CppMods::ImGuiRender
{
    namespace
    {
        struct Record
        {
            void* Owner = nullptr;
            std::string ModName;
            std::string Name;
            AvaloniumImGuiCallback Callback = nullptr;
            void* User = nullptr;
            std::atomic<bool> Active{true};
            std::atomic<bool> InputActive{false};
        };

        std::mutex g_Mutex;
        std::unordered_map<uint32_t, std::shared_ptr<Record>> g_Records;
        std::atomic<uint32_t> g_NextId{1};
        std::atomic<int> g_InFlight{0};
        std::atomic<int> g_InputActiveCount{0};

        void LogLine(const std::string& line)
        {
            EngineDiscovery::WriteLogLine(line.c_str());
        }
    }

    uint32_t Register(const char* name, AvaloniumImGuiCallback callback, void* userData)
    {
        if (!callback) return 0;

        void* owner = CurrentModToken();
        std::string modName;
        if (owner)
        {
            auto* mod = static_cast<LoadedMod*>(owner);
            if (!mod->Alive.load(std::memory_order_acquire)) return 0;
            modName = mod->Name;
        }

        const std::string recordName = (name && *name) ? name : "unnamed";
        const std::string recordMod = modName;

        auto record = std::make_shared<Record>();
        record->Owner = owner;
        record->ModName = recordMod;
        record->Name = recordName;
        record->Callback = callback;
        record->User = userData;

        const uint32_t id = g_NextId.fetch_add(1, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(g_Mutex);
            g_Records[id] = std::move(record);
        }

        LogLine("[CppMod] ImGui render callback '" + recordName + "' registered (id " +
                std::to_string(id) + ")" +
                (recordMod.empty() ? "" : " for '" + recordMod + "'"));
        return id;
    }

    bool Unregister(uint32_t id)
    {
        std::shared_ptr<Record> record;
        {
            std::lock_guard<std::mutex> lock(g_Mutex);
            auto it = g_Records.find(id);
            if (it == g_Records.end()) return false;
            record = std::move(it->second);
            g_Records.erase(it);
            if (record->InputActive.exchange(false, std::memory_order_acq_rel))
                g_InputActiveCount.fetch_sub(1, std::memory_order_acq_rel);
        }
        record->Active.store(false, std::memory_order_release);
        return true;
    }

    void UnregisterOwner(void* owner)
    {
        std::vector<std::shared_ptr<Record>> removed;
        {
            std::lock_guard<std::mutex> lock(g_Mutex);
            for (auto it = g_Records.begin(); it != g_Records.end();)
            {
                if (it->second->Owner == owner)
                {
                    it->second->Active.store(false, std::memory_order_release);
                    if (it->second->InputActive.exchange(false, std::memory_order_acq_rel))
                        g_InputActiveCount.fetch_sub(1, std::memory_order_acq_rel);
                    removed.push_back(std::move(it->second));
                    it = g_Records.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    bool SetInputActive(uint32_t id, bool active)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        auto it = g_Records.find(id);
        if (it == g_Records.end() || !it->second) return false;

        const bool previous = it->second->InputActive.exchange(active, std::memory_order_acq_rel);
        if (previous != active)
        {
            g_InputActiveCount.fetch_add(active ? 1 : -1, std::memory_order_acq_rel);
        }
        return true;
    }

    bool AnyInputActive()
    {
        return g_InputActiveCount.load(std::memory_order_acquire) > 0;
    }

    void RenderAll()
    {
        std::vector<std::shared_ptr<Record>> snapshot;
        {
            std::lock_guard<std::mutex> lock(g_Mutex);
            if (g_Records.empty()) return;
            snapshot.reserve(g_Records.size());
            for (const auto& entry : g_Records)
            {
                if (entry.second && entry.second->Active.load(std::memory_order_acquire))
                    snapshot.push_back(entry.second);
            }
        }

        for (const std::shared_ptr<Record>& record : snapshot)
        {
            if (!record->Callback) continue;
            g_InFlight.fetch_add(1, std::memory_order_acq_rel);
            try
            {
                record->Callback(record->User);
            }
            catch (const std::exception& e)
            {
                LogLine("[CppMod] ImGui callback '" + record->Name +
                        "' threw an exception: " + e.what());
            }
            catch (...)
            {
                LogLine("[CppMod] ImGui callback '" + record->Name +
                        "' threw an unknown exception");
            }
            g_InFlight.fetch_sub(1, std::memory_order_acq_rel);
        }
    }

    bool WaitForIdle(uint32_t milliseconds)
    {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
        while (g_InFlight.load(std::memory_order_acquire) > 0)
        {
            if (milliseconds == 0 || std::chrono::steady_clock::now() >= deadline) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

    void Shutdown()
    {
        std::unordered_map<uint32_t, std::shared_ptr<Record>> records;
        {
            std::lock_guard<std::mutex> lock(g_Mutex);
            records.swap(g_Records);
            g_InputActiveCount.store(0, std::memory_order_release);
        }
        for (auto& entry : records)
        {
            if (!entry.second) continue;
            entry.second->Active.store(false, std::memory_order_release);
            entry.second->InputActive.store(false, std::memory_order_release);
        }
    }
}
