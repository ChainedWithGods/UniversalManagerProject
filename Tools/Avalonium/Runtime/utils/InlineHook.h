#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace Avalonium {

inline constexpr size_t kInlineHookPatchBytes = 14;

class InlineHook {
public:
    InlineHook() = default;
    ~InlineHook();

    InlineHook(const InlineHook&) = delete;
    InlineHook& operator=(const InlineHook&) = delete;

    bool Install(void* target, void* detour, void** originalOut = nullptr, std::string* reason = nullptr);

    void Remove();

    bool Installed() const { return m_Installed; }
    void* Original() const { return m_Trampoline; }
    void* Target() const { return m_Target; }
    void* Detour() const { return m_Detour; }
    size_t StolenBytes() const { return m_StolenBytes; }

    const std::string& Summary() const { return m_Summary; }

    const std::string& PlacementNotes() const { return m_PlacementNotes; }

private:
    void* m_Target = nullptr;
    void* m_Detour = nullptr;
    void* m_Trampoline = nullptr;
    uint8_t m_Saved[kInlineHookPatchBytes] = {};
    size_t m_StolenBytes = 0;
    bool m_Installed = false;
    std::string m_Summary;
    std::string m_PlacementNotes;
};

}
