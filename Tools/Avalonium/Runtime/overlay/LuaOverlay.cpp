#include "LuaOverlay.h"
#include "OverlayDragMe.h"
#include "SystemMetrics.h"
#include "../core/EngineDiscovery.h"
#include "../core/Types.h"
#include "../lua/LuaHookManager.h"
#include "../lua/LuaManager.h"
#include "../utils/SafeMemory.h"
#include "../core/RuntimeConfig.h"
#include <psapi.h>

extern EngineState g_State;

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace menu {
extern bool isOpen;
}

namespace LuaEngine {
namespace Overlay {

static uint32_t ParseColor(lua_State* L, int index, uint32_t defaultColor = 0xFFFFFFFF) {
    if (lua_isnoneornil(L, index)) return defaultColor;
    if (lua_isinteger(L, index)) {
        return static_cast<uint32_t>(lua_tointeger(L, index));
    }
    if (lua_isstring(L, index)) {
        std::string s = lua_tostring(L, index);
        if (!s.empty() && s[0] == '#') s.erase(0, 1);
        try {
            unsigned long val = std::stoul(s, nullptr, 16);
            if (s.size() <= 6) {

                val = 0xFF000000 | (val & 0xFFFFFF);
            }

            uint8_t a = (val >> 24) & 0xFF;
            uint8_t r = (val >> 16) & 0xFF;
            uint8_t g = (val >> 8) & 0xFF;
            uint8_t b = val & 0xFF;
            return IM_COL32(r, g, b, a);
        } catch (...) {}
    } else if (lua_istable(L, index)) {
        lua_rawgeti(L, index, 1); float r = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, index, 2); float g = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, index, 3); float b = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, index, 4); float a = lua_isnoneornil(L, -1) ? 1.0f : static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        if (r <= 1.0f && g <= 1.0f && b <= 1.0f && a <= 1.0f) {
            r *= 255.0f; g *= 255.0f; b *= 255.0f; a *= 255.0f;
        }
        return IM_COL32(static_cast<int>(r), static_cast<int>(g), static_cast<int>(b), static_cast<int>(a));
    }
    return defaultColor;
}

LuaOverlayManager::LuaOverlayManager() {
    m_LastRenderTime = std::chrono::high_resolution_clock::now();
}

LuaOverlayManager::~LuaOverlayManager() {
}

LuaOverlayManager& LuaOverlayManager::Get() {
    static LuaOverlayManager instance;
    return instance;
}

void LuaOverlayManager::ShowNotification(const std::string& title, const std::string& message, float duration, const std::string& typeStr) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    ToastNotification toast;
    toast.Id = m_NextToastId++;

    std::string lowerType = typeStr;
    std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(), ::tolower);
    if (lowerType == "success" || lowerType == "ok") toast.Type = NotificationType::Success;
    else if (lowerType == "warning" || lowerType == "warn") toast.Type = NotificationType::Warning;
    else if (lowerType == "error" || lowerType == "err") toast.Type = NotificationType::Error;
    else toast.Type = NotificationType::Info;

    if (title.empty() && !message.empty()) {
        toast.Title = message;
        toast.Message = "";
    } else if (title.empty() && message.empty()) {
        if (toast.Type == NotificationType::Success) toast.Title = "Success";
        else if (toast.Type == NotificationType::Warning) toast.Title = "Warning";
        else if (toast.Type == NotificationType::Error) toast.Title = "Error";
        else toast.Title = "Notification";
        toast.Message = "";
    } else {
        toast.Title = title;
        toast.Message = message;
    }

    if (duration >= 50.0f) {
        duration /= 1000.0f;
    }
    toast.Duration = (duration > 0.2f) ? duration : 3.5f;
    toast.Elapsed = 0.0f;
    toast.SlideOffset = 60.0f;

    m_Toasts.push_back(toast);

    char logBuf[512];
    snprintf(logBuf, sizeof(logBuf), "[Overlay] Toast (%s): %s %s\n", typeStr.c_str(), toast.Title.c_str(), toast.Message.empty() ? "" : ("— " + toast.Message).c_str());
    EngineDiscovery::WriteLogLine(logBuf);

    OverlayIntegration::Start();
}

void LuaOverlayManager::ClearNotifications() {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    m_Toasts.clear();
}

void LuaOverlayManager::SetStat(const std::string& key, const std::string& value, uint32_t color) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    auto it = m_Stats.find(key);
    if (it != m_Stats.end()) {
        it->second.Value = value;
        if (color != 0) it->second.Color = color;
    } else {
        StatItem item;
        item.Key = key;
        item.Value = value;
        item.Color = color;
        item.Order = m_NextStatOrder++;
        m_Stats[key] = item;
    }
    OverlayIntegration::Start();
}

void LuaOverlayManager::RemoveStat(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    m_Stats.erase(key);
}

void LuaOverlayManager::ClearStats() {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    m_Stats.clear();
}

void LuaOverlayManager::SetStatsVisible(bool visible) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    m_StatsVisible = visible;
    if (visible) {
        OverlayIntegration::Start();
    }
}

void LuaOverlayManager::SetLiveMetricsEnabled(bool enabled) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    m_LiveMetricsEnabled = enabled;
}

void LuaOverlayManager::SetStatsAnchor(const std::string& anchor) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    std::string lower = anchor;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "topleft" || lower == "top_left") m_StatsAnchor = StatsAnchor::TopLeft;
    else if (lower == "bottomleft" || lower == "bottom_left") m_StatsAnchor = StatsAnchor::BottomLeft;
    else if (lower == "bottomright" || lower == "bottom_right") m_StatsAnchor = StatsAnchor::BottomRight;
    else m_StatsAnchor = StatsAnchor::TopRight;
    m_CustomStatsX = -1.0f;
    m_CustomStatsY = -1.0f;
}

void LuaOverlayManager::SetStatsPosition(float x, float y) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    m_CustomStatsX = x;
    m_CustomStatsY = y;
}

void LuaOverlayManager::AddDrawText(const std::string& text, float x, float y, uint32_t color, float fontSize) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    DrawCommand cmd;
    cmd.Type = DrawCmdType::Text;
    cmd.Text = text;
    cmd.X1 = x; cmd.Y1 = y;
    cmd.Color = color;
    cmd.FontSize = fontSize;
    m_FrameDrawCmds.push_back(cmd);
}

void LuaOverlayManager::AddDrawRect(float x, float y, float w, float h, uint32_t color, bool filled, float thickness, float rounding) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    DrawCommand cmd;
    cmd.Type = filled ? DrawCmdType::RectFilled : DrawCmdType::Rect;
    cmd.X1 = x; cmd.Y1 = y;
    cmd.X2 = x + w; cmd.Y2 = y + h;
    cmd.Color = color;
    cmd.Thickness = thickness;
    cmd.Rounding = rounding;
    m_FrameDrawCmds.push_back(cmd);
}

void LuaOverlayManager::AddDrawLine(float x1, float y1, float x2, float y2, uint32_t color, float thickness) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    DrawCommand cmd;
    cmd.Type = DrawCmdType::Line;
    cmd.X1 = x1; cmd.Y1 = y1;
    cmd.X2 = x2; cmd.Y2 = y2;
    cmd.Color = color;
    cmd.Thickness = thickness;
    m_FrameDrawCmds.push_back(cmd);
}

void LuaOverlayManager::AddDrawCircle(float x, float y, float radius, uint32_t color, bool filled, float thickness) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    DrawCommand cmd;
    cmd.Type = filled ? DrawCmdType::CircleFilled : DrawCmdType::Circle;
    cmd.X1 = x; cmd.Y1 = y;
    cmd.Radius = radius;
    cmd.Color = color;
    cmd.Thickness = thickness;
    m_FrameDrawCmds.push_back(cmd);
}

void LuaOverlayManager::AddDrawProgressBar(float x, float y, float w, float h, float fraction, const std::string& text, uint32_t barColor, uint32_t bgColor) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    DrawCommand cmd;
    cmd.Type = DrawCmdType::ProgressBar;
    cmd.X1 = x; cmd.Y1 = y;
    cmd.X2 = x + w; cmd.Y2 = y + h;
    cmd.Fraction = std::clamp(fraction, 0.0f, 1.0f);
    cmd.Text = text;
    cmd.Color = barColor;
    cmd.BgColor = bgColor;
    m_FrameDrawCmds.push_back(cmd);
}

void LuaOverlayManager::RegisterDrawCallback(int callbackRef) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    m_DrawCallbackRefs.push_back(callbackRef);
    OverlayIntegration::Start();
}

void LuaOverlayManager::UnregisterDrawCallback(int callbackRef) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    auto it = std::find(m_DrawCallbackRefs.begin(), m_DrawCallbackRefs.end(), callbackRef);
    if (it != m_DrawCallbackRefs.end()) {
        if (m_L) luaL_unref(m_L, LUA_REGISTRYINDEX, *it);
        m_DrawCallbackRefs.erase(it);
    }
}

void LuaOverlayManager::RegisterWindow(const std::string& title, int callbackRef) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    for (auto& win : m_CustomWindows) {
        if (win.Title == title) {
            if (m_L && win.CallbackRef != -1) luaL_unref(m_L, LUA_REGISTRYINDEX, win.CallbackRef);
            win.CallbackRef = callbackRef;
            return;
        }
    }
    CustomLuaWindow win;
    win.Title = title;
    win.CallbackRef = callbackRef;
    win.IsOpen = true;
    m_CustomWindows.push_back(win);
    OverlayIntegration::Start();
}

void LuaOverlayManager::UnregisterWindow(const std::string& title) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    for (auto it = m_CustomWindows.begin(); it != m_CustomWindows.end(); ++it) {
        if (it->Title == title) {
            if (m_L && it->CallbackRef != -1) luaL_unref(m_L, LUA_REGISTRYINDEX, it->CallbackRef);
            m_CustomWindows.erase(it);
            break;
        }
    }
}

bool LuaOverlayManager::HasVisibleCustomWindows() const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(m_OverlayMutex));
    for (const auto& win : m_CustomWindows) {
        if (win.IsOpen && win.CallbackRef != -1)
            return true;
    }
    return false;
}

bool LuaOverlayManager::ToggleWindow(const std::string& title) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    if (!title.empty()) {
        for (auto& win : m_CustomWindows) {
            if (win.Title == title) {
                win.IsOpen = !win.IsOpen;
                return win.IsOpen;
            }
        }
    } else {
        bool anyOpen = false;
        for (const auto& win : m_CustomWindows) {
            if (win.IsOpen) { anyOpen = true; break; }
        }
        for (auto& win : m_CustomWindows) {
            win.IsOpen = !anyOpen;
        }
        return !anyOpen;
    }
    return false;
}

void LuaOverlayManager::SetWindowOpen(const std::string& title, bool open) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    if (!title.empty()) {
        for (auto& win : m_CustomWindows) {
            if (win.Title == title) {
                win.IsOpen = open;
                return;
            }
        }
    } else {
        for (auto& win : m_CustomWindows) {
            win.IsOpen = open;
        }
    }
}

bool LuaOverlayManager::IsWindowOpen(const std::string& title) const {
    std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(m_OverlayMutex));
    if (!title.empty()) {
        for (const auto& win : m_CustomWindows) {
            if (win.Title == title) return win.IsOpen;
        }
    } else {
        for (const auto& win : m_CustomWindows) {
            if (win.IsOpen) return true;
        }
    }
    return false;
}

void LuaOverlayManager::ClearAll(lua_State* L) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    if (L) {
        for (int ref : m_DrawCallbackRefs) {
            luaL_unref(L, LUA_REGISTRYINDEX, ref);
        }
        for (auto& win : m_CustomWindows) {
            if (win.CallbackRef != -1) luaL_unref(L, LUA_REGISTRYINDEX, win.CallbackRef);
        }
    }
    m_DrawCallbackRefs.clear();
    m_CustomWindows.clear();
    m_Toasts.clear();
    m_Stats.clear();
    m_FrameDrawCmds.clear();
    m_PersistentDrawCmds.clear();
    m_StatsVisible = false;
    m_LiveMetricsEnabled = false;
}

void LuaOverlayManager::Render() {
    if (!ImGui::GetCurrentContext()) return;

    auto now = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(now - m_LastRenderTime).count();
    if (deltaTime <= 0.0f || deltaTime > 0.5f) deltaTime = 0.0166f;
    m_LastRenderTime = now;

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (!drawList) return;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 displaySize = io.DisplaySize;
    if (displaySize.x <= 100.0f || displaySize.y <= 100.0f) return;

    if (m_L && !m_DrawCallbackRefs.empty()) {
        std::vector<int> activeCallbacks;
        {
            std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
            activeCallbacks = m_DrawCallbackRefs;
        }
        std::lock_guard<std::recursive_mutex> luaLock(LuaManager::Get().GetLuaMutex());
        for (int ref : activeCallbacks) {
            lua_rawgeti(m_L, LUA_REGISTRYINDEX, ref);
            if (lua_pcall(m_L, 0, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(m_L, -1);
                char buf[512];
                snprintf(buf, sizeof(buf), "[Overlay] DrawCallback error: %s", err ? err : "unknown");
                EngineDiscovery::WriteLogLine(buf);
                lua_pop(m_L, 1);
            }
        }
    }

    RenderCanvas(drawList);

    if (m_StatsVisible) {
        RenderStatsHUD(drawList, displaySize);
    }

    RenderToasts(drawList, displaySize, deltaTime);

    RenderCustomWindows();
}

void LuaOverlayManager::RenderToasts(ImDrawList* drawList, const ImVec2& displaySize, float deltaTime) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    if (m_Toasts.empty()) return;

    float cardWidth = 330.0f;
    float startY = 32.0f;
    float cardX = displaySize.x - cardWidth - 24.0f;

    for (auto it = m_Toasts.begin(); it != m_Toasts.end();) {
        it->Elapsed += deltaTime;
        if (it->Elapsed >= it->Duration) {
            it = m_Toasts.erase(it);
            continue;
        }

        it->SlideOffset = std::max(0.0f, it->SlideOffset - deltaTime * 300.0f);

        float alpha = 1.0f;
        if (it->Elapsed < 0.25f) {
            alpha = it->Elapsed / 0.25f;
        } else if (it->Elapsed > it->Duration - 0.4f) {
            alpha = (it->Duration - it->Elapsed) / 0.4f;
        }
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        uint32_t accentColor = IM_COL32(52, 152, 219, static_cast<int>(255 * alpha));
        const char* iconStr = "[i]";
        if (it->Type == NotificationType::Success) {
            accentColor = IM_COL32(46, 204, 113, static_cast<int>(255 * alpha));
            iconStr = "[OK]";
        } else if (it->Type == NotificationType::Warning) {
            accentColor = IM_COL32(241, 196, 15, static_cast<int>(255 * alpha));
            iconStr = "[!]";
        } else if (it->Type == NotificationType::Error) {
            accentColor = IM_COL32(231, 76, 60, static_cast<int>(255 * alpha));
            iconStr = "[X]";
        }

        float cardHeight = it->Message.empty() ? 44.0f : 64.0f;
        ImVec2 pMin(cardX + it->SlideOffset, startY);
        ImVec2 pMax(pMin.x + cardWidth, pMin.y + cardHeight);

        uint32_t bgCol = IM_COL32(18, 20, 28, static_cast<int>(235 * alpha));
        uint32_t borderCol = IM_COL32(60, 65, 80, static_cast<int>(180 * alpha));
        drawList->AddRectFilled(pMin, pMax, bgCol, 8.0f);
        drawList->AddRect(pMin, pMax, borderCol, 8.0f, 0, 1.0f);

        drawList->AddRectFilled(pMin, ImVec2(pMin.x + 5.0f, pMax.y), accentColor, 8.0f, ImDrawFlags_RoundCornersLeft);

        std::string titleText = std::string(iconStr) + " " + it->Title;
        uint32_t textCol = IM_COL32(245, 245, 250, static_cast<int>(255 * alpha));
        drawList->AddText(ImVec2(pMin.x + 14.0f, pMin.y + 10.0f), textCol, titleText.c_str());

        if (!it->Message.empty()) {
            uint32_t msgCol = IM_COL32(180, 185, 200, static_cast<int>(240 * alpha));
            drawList->AddText(ImVec2(pMin.x + 14.0f, pMin.y + 32.0f), msgCol, it->Message.c_str());
        }

        float progress = std::clamp(1.0f - (it->Elapsed / it->Duration), 0.0f, 1.0f);
        float barY = pMax.y - 2.5f;
        drawList->AddLine(
            ImVec2(pMin.x + 6.0f, barY),
            ImVec2(pMin.x + 6.0f + (cardWidth - 12.0f) * progress, barY),
            accentColor,
            2.0f
        );

        startY += cardHeight + 10.0f;
        ++it;
    }
}

void LuaOverlayManager::RenderStatsHUD(ImDrawList* drawList, const ImVec2& displaySize) {
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    if (!m_StatsVisible) return;

    std::vector<StatItem> items;

    if (m_LiveMetricsEnabled) {
        ImGuiIO& io = ImGui::GetIO();
        float fps = io.Framerate;
        float ftMs = io.DeltaTime * 1000.0f;

        static auto s_LastMetricPoll = std::chrono::high_resolution_clock::now();
        static double s_CachedUsedMB = 0.0;
        static double s_CachedTotalMB = 0.0;
        static int s_CachedObjCount = 0;

        auto now = std::chrono::high_resolution_clock::now();
        double pollElapsed = std::chrono::duration<double, std::milli>(now - s_LastMetricPoll).count();
        if (pollElapsed >= 250.0 || s_CachedTotalMB <= 0.0) {
            s_LastMetricPoll = now;
            PROCESS_MEMORY_COUNTERS_EX pmc = {};
            pmc.cb = sizeof(pmc);
            if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
                s_CachedUsedMB = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
            }
            MEMORYSTATUSEX memStatus = {};
            memStatus.dwLength = sizeof(memStatus);
            if (GlobalMemoryStatusEx(&memStatus)) {
                s_CachedTotalMB = static_cast<double>(memStatus.ullTotalPhys) / (1024.0 * 1024.0);
            }
            s_CachedObjCount = EngineDiscovery::GetObjectCount(g_State);
        }

        char buf[64];

        snprintf(buf, sizeof(buf), "%.0f", fps);
        uint32_t fpsCol = (fps >= 55.0f) ? IM_COL32(46, 204, 113, 255) : ((fps >= 28.0f) ? IM_COL32(241, 196, 15, 255) : IM_COL32(231, 76, 60, 255));
        items.push_back({ "FPS", buf, fpsCol, 1 });

        snprintf(buf, sizeof(buf), "%.2f ms", ftMs);
        items.push_back({ "Frame Time", buf, IM_COL32(243, 156, 18, 255), 2 });

        double usedGB = s_CachedUsedMB / 1024.0;
        double totalGB = s_CachedTotalMB / 1024.0;
        if (totalGB > 0.0) {
            snprintf(buf, sizeof(buf), "%.1f / %.1f GB", usedGB, totalGB);
        } else {
            snprintf(buf, sizeof(buf), "%.1f GB", usedGB);
        }
        items.push_back({ "Memory", buf, IM_COL32(52, 152, 219, 255), 3 });

        const SystemMetrics::Snapshot system = SystemMetrics::Poll();
        if (system.CpuUsagePercent >= 0.0f) {
            snprintf(buf, sizeof(buf), "%.0f%%", system.CpuUsagePercent);
            items.push_back({ "CPU", buf, IM_COL32(26, 188, 156, 255), 4 });
        }
        if (system.GpuUsagePercent >= 0.0f) {
            snprintf(buf, sizeof(buf), "%.0f%%", system.GpuUsagePercent);
            items.push_back({ "GPU", buf, IM_COL32(230, 126, 34, 255), 5 });
        }
        if (system.GpuTemperatureC >= 0.0f) {
            snprintf(buf, sizeof(buf), "%.0f C", system.GpuTemperatureC);
            const uint32_t tempColor = (system.GpuTemperatureC < 70.0f) ? IM_COL32(46, 204, 113, 255)
                                     : (system.GpuTemperatureC < 85.0f) ? IM_COL32(241, 196, 15, 255)
                                     : IM_COL32(231, 76, 60, 255);
            items.push_back({ "GPU Temp", buf, tempColor, 6 });
        }

        snprintf(buf, sizeof(buf), "%d", s_CachedObjCount);
        items.push_back({ "Objects", buf, IM_COL32(155, 89, 182, 255), 7 });
    }

    std::vector<StatItem> customItems;
    for (const auto& [_, item] : m_Stats) customItems.push_back(item);
    std::sort(customItems.begin(), customItems.end(), [](const StatItem& a, const StatItem& b) {
        return a.Order < b.Order;
    });
    for (const auto& ci : customItems) {
        items.push_back(ci);
    }

    if (items.empty()) return;

    float padding = 12.0f;
    float lineHeight = 20.0f;
    float hudWidth = 240.0f;
    float hudHeight = padding * 2.0f + 24.0f + items.size() * lineHeight;

    float hudX = 24.0f;
    float hudY = 24.0f;

    if (m_CustomStatsX >= 0.0f && m_CustomStatsY >= 0.0f) {
        hudX = m_CustomStatsX;
        hudY = m_CustomStatsY;
    } else {
        switch (m_StatsAnchor) {
        case StatsAnchor::TopLeft:
            hudX = 24.0f; hudY = 24.0f;
            break;
        case StatsAnchor::TopRight:
            hudX = displaySize.x - hudWidth - 24.0f; hudY = 24.0f;
            break;
        case StatsAnchor::BottomLeft:
            hudX = 24.0f; hudY = displaySize.y - hudHeight - 24.0f;
            break;
        case StatsAnchor::BottomRight:
            hudX = displaySize.x - hudWidth - 24.0f; hudY = displaySize.y - hudHeight - 24.0f;
            break;
        }
    }

    ImVec2 pMin(hudX, hudY);
    ImVec2 pMax(hudX + hudWidth, hudY + hudHeight);

    drawList->AddRectFilled(pMin, pMax, IM_COL32(14, 16, 22, 215), 8.0f);
    drawList->AddRect(pMin, pMax, IM_COL32(50, 55, 70, 200), 8.0f, 0, 1.0f);

    drawList->AddText(ImVec2(pMin.x + padding, pMin.y + padding - 2.0f), IM_COL32(52, 152, 219, 255), "Avalonium Overlay");
    drawList->AddLine(ImVec2(pMin.x + padding, pMin.y + padding + 18.0f), ImVec2(pMax.x - padding, pMin.y + padding + 18.0f), IM_COL32(60, 65, 80, 150), 1.0f);

    float curY = pMin.y + padding + 26.0f;
    for (const auto& item : items) {

        drawList->AddText(ImVec2(pMin.x + padding, curY), IM_COL32(170, 185, 200, 255), item.Key.c_str());

        uint32_t valCol = (item.Color != 0) ? item.Color : IM_COL32(46, 204, 113, 255);
        ImVec2 valSize = ImGui::CalcTextSize(item.Value.c_str());
        float valX = pMax.x - padding - valSize.x;
        drawList->AddText(ImVec2(valX, curY), valCol, item.Value.c_str());

        curY += lineHeight;
    }
}

void LuaOverlayManager::RenderCanvas(ImDrawList* drawList) {
    std::vector<DrawCommand> cmds;
    {
        std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
        cmds = m_FrameDrawCmds;
        m_FrameDrawCmds.clear();
    }

    for (const auto& cmd : cmds) {
        switch (cmd.Type) {
        case DrawCmdType::Text:
            drawList->AddText(ImVec2(cmd.X1, cmd.Y1), cmd.Color, cmd.Text.c_str());
            break;
        case DrawCmdType::Rect:
            drawList->AddRect(ImVec2(cmd.X1, cmd.Y1), ImVec2(cmd.X2, cmd.Y2), cmd.Color, cmd.Rounding, 0, cmd.Thickness);
            break;
        case DrawCmdType::RectFilled:
            drawList->AddRectFilled(ImVec2(cmd.X1, cmd.Y1), ImVec2(cmd.X2, cmd.Y2), cmd.Color, cmd.Rounding);
            break;
        case DrawCmdType::Line:
            drawList->AddLine(ImVec2(cmd.X1, cmd.Y1), ImVec2(cmd.X2, cmd.Y2), cmd.Color, cmd.Thickness);
            break;
        case DrawCmdType::Circle:
            drawList->AddCircle(ImVec2(cmd.X1, cmd.Y1), cmd.Radius, cmd.Color, 0, cmd.Thickness);
            break;
        case DrawCmdType::CircleFilled:
            drawList->AddCircleFilled(ImVec2(cmd.X1, cmd.Y1), cmd.Radius, cmd.Color, 0);
            break;
        case DrawCmdType::ProgressBar: {
            ImVec2 pMin(cmd.X1, cmd.Y1);
            ImVec2 pMax(cmd.X2, cmd.Y2);
            float w = cmd.X2 - cmd.X1;
            drawList->AddRectFilled(pMin, pMax, cmd.BgColor, 4.0f);
            drawList->AddRectFilled(pMin, ImVec2(cmd.X1 + w * cmd.Fraction, cmd.Y2), cmd.Color, 4.0f);
            drawList->AddRect(pMin, pMax, IM_COL32(80, 85, 100, 200), 4.0f, 0, 1.0f);
            if (!cmd.Text.empty()) {
                ImVec2 tSize = ImGui::CalcTextSize(cmd.Text.c_str());
                float tx = pMin.x + (w - tSize.x) * 0.5f;
                float ty = pMin.y + ((cmd.Y2 - cmd.Y1) - tSize.y) * 0.5f;
                drawList->AddText(ImVec2(tx, ty), IM_COL32(255, 255, 255, 255), cmd.Text.c_str());
            }
            break;
        }
        }
    }
}

void LuaOverlayManager::RenderCustomWindows() {
    if (!m_L) return;

    std::lock_guard<std::recursive_mutex> luaLock(LuaManager::Get().GetLuaMutex());
    std::lock_guard<std::recursive_mutex> lock(m_OverlayMutex);
    for (auto& win : m_CustomWindows) {
        if (!win.IsOpen || win.CallbackRef == -1) continue;

        ImGui::SetNextWindowSize(ImVec2(360, 280), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(win.Title.c_str(), &win.IsOpen)) {
            lua_rawgeti(m_L, LUA_REGISTRYINDEX, win.CallbackRef);
            if (lua_pcall(m_L, 0, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(m_L, -1);
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Lua Window Error: %s", err ? err : "unknown");
                lua_pop(m_L, 1);
            }
        }
        ImGui::End();
    }
}

static bool IsNotificationTypeString(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return (s == "info" || s == "success" || s == "warning" || s == "error" || s == "ok" || s == "warn" || s == "err");
}

static int Lua_Overlay_Notify(lua_State* L) {
    int top = lua_gettop(L);
    int argOffset = 1;

    if (top >= 1 && lua_istable(L, 1)) {
        argOffset = 2;
    }
    int effectiveArgs = top - argOffset + 1;
    if (effectiveArgs < 1) return 0;

    std::string title;
    std::string msg;
    float duration = 3.5f;
    std::string typeStr = "info";

    if (lua_isstring(L, argOffset)) {
        title = lua_tostring(L, argOffset);
    }

    if (effectiveArgs == 1) {
        msg = "";
    } else if (effectiveArgs == 2) {
        if (lua_type(L, argOffset + 1) == LUA_TNUMBER) {
            duration = static_cast<float>(lua_tonumber(L, argOffset + 1));
        } else if (lua_isstring(L, argOffset + 1)) {
            std::string secondArg = lua_tostring(L, argOffset + 1);
            if (IsNotificationTypeString(secondArg)) {
                typeStr = secondArg;
            } else {
                msg = secondArg;
            }
        }
    } else if (effectiveArgs == 3) {
        if (lua_type(L, argOffset + 1) == LUA_TNUMBER) {
            duration = static_cast<float>(lua_tonumber(L, argOffset + 1));
            if (lua_isstring(L, argOffset + 2)) typeStr = lua_tostring(L, argOffset + 2);
        } else {
            if (lua_isstring(L, argOffset + 1)) msg = lua_tostring(L, argOffset + 1);
            if (lua_type(L, argOffset + 2) == LUA_TNUMBER) {
                duration = static_cast<float>(lua_tonumber(L, argOffset + 2));
            } else if (lua_isstring(L, argOffset + 2)) {
                typeStr = lua_tostring(L, argOffset + 2);
            }
        }
    } else if (effectiveArgs >= 4) {
        if (lua_isstring(L, argOffset + 1)) msg = lua_tostring(L, argOffset + 1);
        if (lua_type(L, argOffset + 2) == LUA_TNUMBER) duration = static_cast<float>(lua_tonumber(L, argOffset + 2));
        if (lua_isstring(L, argOffset + 3)) typeStr = lua_tostring(L, argOffset + 3);
    }

    LuaOverlayManager::Get().ShowNotification(title, msg, duration, typeStr);
    return 0;
}

static int Lua_Overlay_Toast(lua_State* L) {
    int top = lua_gettop(L);
    int argOffset = 1;
    if (top >= 1 && lua_istable(L, 1)) {
        argOffset = 2;
    }
    int effectiveArgs = top - argOffset + 1;
    if (effectiveArgs < 1) return 0;

    std::string msg;
    float duration = 3.0f;
    std::string typeStr = "info";

    if (lua_isstring(L, argOffset)) {
        msg = lua_tostring(L, argOffset);
    }

    if (effectiveArgs == 2) {
        if (lua_type(L, argOffset + 1) == LUA_TNUMBER) {
            duration = static_cast<float>(lua_tonumber(L, argOffset + 1));
        } else if (lua_isstring(L, argOffset + 1)) {
            typeStr = lua_tostring(L, argOffset + 1);
        }
    } else if (effectiveArgs >= 3) {
        if (lua_type(L, argOffset + 1) == LUA_TNUMBER) duration = static_cast<float>(lua_tonumber(L, argOffset + 1));
        if (lua_isstring(L, argOffset + 2)) typeStr = lua_tostring(L, argOffset + 2);
    }

    LuaOverlayManager::Get().ShowNotification(msg, "", duration, typeStr);
    return 0;
}

static int Lua_Overlay_ClearNotifications(lua_State* L) {
    LuaOverlayManager::Get().ClearNotifications();
    return 0;
}

static int Lua_Overlay_SetStat(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    std::string val;
    if (lua_isstring(L, 2)) val = lua_tostring(L, 2);
    else if (lua_isinteger(L, 2)) val = std::to_string(lua_tointeger(L, 2));
    else if (lua_isnumber(L, 2)) {
        char buf[64]; snprintf(buf, sizeof(buf), "%.2f", lua_tonumber(L, 2)); val = buf;
    } else if (lua_isboolean(L, 2)) {
        val = lua_toboolean(L, 2) ? "true" : "false";
    } else {
        const char* s = luaL_tolstring(L, 2, nullptr);
        val = s ? s : "";
        lua_pop(L, 1);
    }

    uint32_t color = ParseColor(L, 3, 0);
    LuaOverlayManager::Get().SetStat(key, val, color);
    return 0;
}

static int Lua_Overlay_RemoveStat(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    LuaOverlayManager::Get().RemoveStat(key);
    return 0;
}

static int Lua_Overlay_ClearStats(lua_State* L) {
    LuaOverlayManager::Get().ClearStats();
    return 0;
}

static int Lua_Overlay_ShowStats(lua_State* L) {
    bool show = (lua_gettop(L) >= 1 && lua_isboolean(L, 1)) ? (lua_toboolean(L, 1) != 0) : true;
    LuaOverlayManager::Get().SetStatsVisible(show);
    return 0;
}

static int Lua_Overlay_IsStatsVisible(lua_State* L) {
    lua_pushboolean(L, LuaOverlayManager::Get().IsStatsVisible() ? 1 : 0);
    return 1;
}

static int Lua_Overlay_SetStatsAnchor(lua_State* L) {
    const char* anchor = luaL_checkstring(L, 1);
    LuaOverlayManager::Get().SetStatsAnchor(anchor);
    return 0;
}

static int Lua_Overlay_SetStatsPosition(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    LuaOverlayManager::Get().SetStatsPosition(x, y);
    return 0;
}

static inline float GetFloatArg(lua_State* L, int index, float defaultVal = 0.0f) {
    return lua_isnumber(L, index) ? static_cast<float>(lua_tonumber(L, index)) : defaultVal;
}

static int Lua_Overlay_DrawText(lua_State* L) {
    const char* text = lua_isstring(L, 1) ? lua_tostring(L, 1) : "";
    float x = GetFloatArg(L, 2, 0.0f);
    float y = GetFloatArg(L, 3, 0.0f);
    uint32_t color = ParseColor(L, 4, 0xFFFFFFFF);
    float fontSize = GetFloatArg(L, 5, 0.0f);
    LuaOverlayManager::Get().AddDrawText(text, x, y, color, fontSize);
    return 0;
}

static int Lua_Overlay_DrawRect(lua_State* L) {
    float x = GetFloatArg(L, 1, 0.0f);
    float y = GetFloatArg(L, 2, 0.0f);
    float w = GetFloatArg(L, 3, 0.0f);
    float h = GetFloatArg(L, 4, 0.0f);
    uint32_t color = ParseColor(L, 5, 0xFFFFFFFF);
    bool filled = (lua_gettop(L) >= 6 && lua_isboolean(L, 6)) ? (lua_toboolean(L, 6) != 0) : false;
    float thickness = GetFloatArg(L, 7, 1.0f);
    float rounding = GetFloatArg(L, 8, 0.0f);
    LuaOverlayManager::Get().AddDrawRect(x, y, w, h, color, filled, thickness, rounding);
    return 0;
}

static int Lua_Overlay_DrawLine(lua_State* L) {
    float x1 = GetFloatArg(L, 1, 0.0f);
    float y1 = GetFloatArg(L, 2, 0.0f);
    float x2 = GetFloatArg(L, 3, 0.0f);
    float y2 = GetFloatArg(L, 4, 0.0f);
    uint32_t color = ParseColor(L, 5, 0xFFFFFFFF);
    float thickness = GetFloatArg(L, 6, 1.0f);
    LuaOverlayManager::Get().AddDrawLine(x1, y1, x2, y2, color, thickness);
    return 0;
}

static int Lua_Overlay_DrawCircle(lua_State* L) {
    float x = GetFloatArg(L, 1, 0.0f);
    float y = GetFloatArg(L, 2, 0.0f);
    float radius = GetFloatArg(L, 3, 0.0f);
    uint32_t color = ParseColor(L, 4, 0xFFFFFFFF);
    bool filled = (lua_gettop(L) >= 5 && lua_isboolean(L, 5)) ? (lua_toboolean(L, 5) != 0) : false;
    float thickness = GetFloatArg(L, 6, 1.0f);
    LuaOverlayManager::Get().AddDrawCircle(x, y, radius, color, filled, thickness);
    return 0;
}

static int Lua_Overlay_DrawProgressBar(lua_State* L) {
    float x = GetFloatArg(L, 1, 0.0f);
    float y = GetFloatArg(L, 2, 0.0f);
    float w = GetFloatArg(L, 3, 0.0f);
    float h = GetFloatArg(L, 4, 0.0f);
    float fraction = GetFloatArg(L, 5, 0.0f);
    const char* text = (lua_gettop(L) >= 6 && lua_isstring(L, 6)) ? lua_tostring(L, 6) : "";
    uint32_t barColor = ParseColor(L, 7, IM_COL32(46, 204, 113, 255));
    uint32_t bgColor = ParseColor(L, 8, IM_COL32(30, 32, 40, 200));
    LuaOverlayManager::Get().AddDrawProgressBar(x, y, w, h, fraction, text, barColor, bgColor);
    return 0;
}

static int Lua_Overlay_RegisterDrawCallback(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    LuaOverlayManager::Get().RegisterDrawCallback(ref);
    lua_pushinteger(L, ref);
    return 1;
}

static int Lua_Overlay_GetViewportSize(lua_State* L) {
    ImGuiIO& io = ImGui::GetIO();
    lua_pushnumber(L, static_cast<lua_Number>(io.DisplaySize.x));
    lua_pushnumber(L, static_cast<lua_Number>(io.DisplaySize.y));
    return 2;
}

static int Lua_Overlay_RegisterWindow(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    LuaOverlayManager::Get().RegisterWindow(title, ref);
    return 0;
}

static int Lua_Overlay_Text(lua_State* L) {
    const char* str = luaL_checkstring(L, 1);
    ImGui::TextUnformatted(str);
    return 0;
}

static int Lua_Overlay_TextColored(lua_State* L) {
    const char* str = luaL_checkstring(L, 1);
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    float a = (lua_gettop(L) >= 5 && lua_isnumber(L, 5)) ? static_cast<float>(lua_tonumber(L, 5)) : 1.0f;
    if (r > 1.0f || g > 1.0f || b > 1.0f) { r /= 255.0f; g /= 255.0f; b /= 255.0f; if (a > 1.0f) a /= 255.0f; }
    ImGui::TextColored(ImVec4(r, g, b, a), "%s", str);
    return 0;
}

static int Lua_Overlay_Button(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    bool clicked = ImGui::Button(label);
    lua_pushboolean(L, clicked ? 1 : 0);
    return 1;
}

static int Lua_Overlay_Checkbox(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    bool val = lua_isboolean(L, 2) && lua_toboolean(L, 2) != 0;
    bool changed = ImGui::Checkbox(label, &val);
    lua_pushboolean(L, val ? 1 : 0);
    lua_pushboolean(L, changed ? 1 : 0);
    return 2;
}

static int Lua_Overlay_SliderFloat(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    float val = static_cast<float>(luaL_checknumber(L, 2));
    float minVal = static_cast<float>(luaL_checknumber(L, 3));
    float maxVal = static_cast<float>(luaL_checknumber(L, 4));
    bool changed = ImGui::SliderFloat(label, &val, minVal, maxVal);
    lua_pushnumber(L, static_cast<double>(val));
    lua_pushboolean(L, changed ? 1 : 0);
    return 2;
}

static int Lua_Overlay_SliderInt(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    int val = static_cast<int>(luaL_checkinteger(L, 2));
    int minVal = static_cast<int>(luaL_checkinteger(L, 3));
    int maxVal = static_cast<int>(luaL_checkinteger(L, 4));
    bool changed = ImGui::SliderInt(label, &val, minVal, maxVal);
    lua_pushinteger(L, val);
    lua_pushboolean(L, changed ? 1 : 0);
    return 2;
}

static int Lua_Overlay_Separator(lua_State* L) {
    ImGui::Separator();
    return 0;
}

static int Lua_Overlay_SameLine(lua_State* L) {
    ImGui::SameLine();
    return 0;
}

static int Lua_Overlay_InputText(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    const char* text = luaL_optstring(L, 2, "");
    int maxLen = static_cast<int>(luaL_optinteger(L, 3, 256));
    if (maxLen < 16) maxLen = 16;
    if (maxLen > 4096) maxLen = 4096;

    std::vector<char> buf(maxLen);
    strncpy_s(buf.data(), maxLen, text, _TRUNCATE);

    bool changed = ImGui::InputText(label, buf.data(), maxLen);

    lua_pushstring(L, buf.data());
    lua_pushboolean(L, changed ? 1 : 0);
    return 2;
}

static int Lua_Overlay_Combo(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    int current = static_cast<int>(luaL_checkinteger(L, 2));
    luaL_checktype(L, 3, LUA_TTABLE);
    int len = static_cast<int>(lua_rawlen(L, 3));
    std::vector<std::string> items;
    items.reserve(len);
    for (int i = 1; i <= len; ++i) {
        lua_rawgeti(L, 3, i);
        const char* s = lua_tostring(L, -1);
        items.emplace_back(s ? s : "");
        lua_pop(L, 1);
    }
    std::string comboStr;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) comboStr += '\0';
        comboStr += items[i];
    }
    comboStr += '\0';
    bool changed = ImGui::Combo(label, &current, comboStr.c_str());
    lua_pushinteger(L, current);
    lua_pushboolean(L, changed ? 1 : 0);
    return 2;
}

static int Lua_Overlay_Selectable(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    bool selected = (lua_gettop(L) >= 2 && lua_isboolean(L, 2)) ? (lua_toboolean(L, 2) != 0) : false;
    bool clicked = ImGui::Selectable(label, &selected);
    lua_pushboolean(L, selected ? 1 : 0);
    lua_pushboolean(L, clicked ? 1 : 0);
    return 2;
}

static int Lua_Overlay_InputFloat(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    float val = static_cast<float>(luaL_checknumber(L, 2));
    float step = (lua_gettop(L) >= 3 && lua_isnumber(L, 3)) ? static_cast<float>(lua_tonumber(L, 3)) : 0.0f;
    float stepFast = (lua_gettop(L) >= 4 && lua_isnumber(L, 4)) ? static_cast<float>(lua_tonumber(L, 4)) : 0.0f;
    bool changed = ImGui::InputFloat(label, &val, step, stepFast);
    lua_pushnumber(L, static_cast<lua_Number>(val));
    lua_pushboolean(L, changed ? 1 : 0);
    return 2;
}

static int Lua_Overlay_InputInt(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    int val = static_cast<int>(luaL_checkinteger(L, 2));
    int step = (lua_gettop(L) >= 3 && lua_isinteger(L, 3)) ? static_cast<int>(lua_tointeger(L, 3)) : 1;
    int stepFast = (lua_gettop(L) >= 4 && lua_isinteger(L, 4)) ? static_cast<int>(lua_tointeger(L, 4)) : 100;
    bool changed = ImGui::InputInt(label, &val, step, stepFast);
    lua_pushinteger(L, val);
    lua_pushboolean(L, changed ? 1 : 0);
    return 2;
}

static int Lua_Overlay_RadioButton(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    bool active = (lua_gettop(L) >= 2 && lua_isboolean(L, 2)) ? (lua_toboolean(L, 2) != 0) : false;
    bool clicked = ImGui::RadioButton(label, active);
    lua_pushboolean(L, clicked ? 1 : 0);
    return 1;
}

static int Lua_Overlay_CollapsingHeader(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    bool open = (lua_gettop(L) >= 2 && lua_isboolean(L, 2)) ? (lua_toboolean(L, 2) != 0) : true;
    bool visible = ImGui::CollapsingHeader(label, open ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    lua_pushboolean(L, visible ? 1 : 0);
    return 1;
}

static int Lua_Overlay_TreeNode(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    bool open = ImGui::TreeNode(label);
    lua_pushboolean(L, open ? 1 : 0);
    return 1;
}

static int Lua_Overlay_TreePop(lua_State* L) {
    ImGui::TreePop();
    return 0;
}

static int Lua_Overlay_BeginChild(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    float w = (lua_gettop(L) >= 2 && lua_isnumber(L, 2)) ? static_cast<float>(lua_tonumber(L, 2)) : 0.0f;
    float h = (lua_gettop(L) >= 3 && lua_isnumber(L, 3)) ? static_cast<float>(lua_tonumber(L, 3)) : 0.0f;
    bool open = ImGui::BeginChild(id, ImVec2(w, h));
    lua_pushboolean(L, open ? 1 : 0);
    return 1;
}

static int Lua_Overlay_EndChild(lua_State* L) {
    ImGui::EndChild();
    return 0;
}

static int Lua_Overlay_Indent(lua_State* L) {
    float indent = (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) ? static_cast<float>(lua_tonumber(L, 1)) : 0.0f;
    if (indent > 0.0f) ImGui::Indent(indent);
    else ImGui::Indent();
    return 0;
}

static int Lua_Overlay_Unindent(lua_State* L) {
    float indent = (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) ? static_cast<float>(lua_tonumber(L, 1)) : 0.0f;
    if (indent > 0.0f) ImGui::Unindent(indent);
    else ImGui::Unindent();
    return 0;
}

static int Lua_Overlay_Spacing(lua_State* L) {
    ImGui::Spacing();
    return 0;
}

static int Lua_Overlay_Dummy(lua_State* L) {
    float w = static_cast<float>(luaL_checknumber(L, 1));
    float h = static_cast<float>(luaL_checknumber(L, 2));
    ImGui::Dummy(ImVec2(w, h));
    return 0;
}

static int Lua_Overlay_Tooltip(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", text);
    }
    return 0;
}

static int Lua_Overlay_IsItemHovered(lua_State* L) {
    lua_pushboolean(L, ImGui::IsItemHovered() ? 1 : 0);
    return 1;
}

static int Lua_Overlay_IsItemClicked(lua_State* L) {
    int button = (lua_gettop(L) >= 1 && lua_isinteger(L, 1)) ? static_cast<int>(lua_tointeger(L, 1)) : 0;
    lua_pushboolean(L, ImGui::IsItemClicked(button) ? 1 : 0);
    return 1;
}

static int Lua_Overlay_ProgressBar(lua_State* L) {
    float fraction = static_cast<float>(luaL_checknumber(L, 1));
    float w = (lua_gettop(L) >= 2 && lua_isnumber(L, 2)) ? static_cast<float>(lua_tonumber(L, 2)) : -1.0f;
    float h = (lua_gettop(L) >= 3 && lua_isnumber(L, 3)) ? static_cast<float>(lua_tonumber(L, 3)) : 0.0f;
    const char* overlay = (lua_gettop(L) >= 4 && lua_isstring(L, 4)) ? lua_tostring(L, 4) : nullptr;
    ImVec2 size(w >= 0.0f ? w : -1.0f, h > 0.0f ? h : 0.0f);
    ImGui::ProgressBar(fraction, size, overlay);
    return 0;
}

static int Lua_Overlay_BeginTable(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    int cols = static_cast<int>(luaL_checkinteger(L, 2));
    int flags = (lua_gettop(L) >= 3 && lua_isinteger(L, 3)) ? static_cast<int>(lua_tointeger(L, 3)) : 0;
    bool open = ImGui::BeginTable(id, cols, flags);
    lua_pushboolean(L, open ? 1 : 0);
    return 1;
}

static int Lua_Overlay_EndTable(lua_State* L) {
    ImGui::EndTable();
    return 0;
}

static int Lua_Overlay_TableNextRow(lua_State* L) {
    int flags = (lua_gettop(L) >= 1 && lua_isinteger(L, 1)) ? static_cast<int>(lua_tointeger(L, 1)) : 0;
    float minH = (lua_gettop(L) >= 2 && lua_isnumber(L, 2)) ? static_cast<float>(lua_tonumber(L, 2)) : 0.0f;
    ImGui::TableNextRow(flags, minH);
    return 0;
}

static int Lua_Overlay_TableNextColumn(lua_State* L) {
    lua_pushboolean(L, ImGui::TableNextColumn() ? 1 : 0);
    return 1;
}

static int Lua_Overlay_TableSetupColumn(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    int flags = (lua_gettop(L) >= 2 && lua_isinteger(L, 2)) ? static_cast<int>(lua_tointeger(L, 2)) : 0;
    float initW = (lua_gettop(L) >= 3 && lua_isnumber(L, 3)) ? static_cast<float>(lua_tonumber(L, 3)) : 0.0f;
    float minW = (lua_gettop(L) >= 4 && lua_isnumber(L, 4)) ? static_cast<float>(lua_tonumber(L, 4)) : 0.0f;
    ImGui::TableSetupColumn(label, flags, initW, minW);
    return 0;
}

static int Lua_Overlay_PushStyleColor(lua_State* L) {
    int idx = static_cast<int>(luaL_checkinteger(L, 1));
    uint32_t color = ParseColor(L, 2, 0xFFFFFFFF);
    ImGui::PushStyleColor(idx, color);
    return 0;
}

static int Lua_Overlay_PopStyleColor(lua_State* L) {
    int count = (lua_gettop(L) >= 1 && lua_isinteger(L, 1)) ? static_cast<int>(lua_tointeger(L, 1)) : 1;
    ImGui::PopStyleColor(count);
    return 0;
}

static int Lua_Overlay_PushStyleVar(lua_State* L) {
    int idx = static_cast<int>(luaL_checkinteger(L, 1));
    if (lua_isnumber(L, 2)) {
        float val = static_cast<float>(lua_tonumber(L, 2));
        ImGui::PushStyleVar(idx, val);
    } else if (lua_istable(L, 2)) {
        lua_rawgeti(L, 2, 1); float x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, 2, 2); float y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        ImGui::PushStyleVar(idx, ImVec2(x, y));
    }
    return 0;
}

static int Lua_Overlay_PopStyleVar(lua_State* L) {
    int count = (lua_gettop(L) >= 1 && lua_isinteger(L, 1)) ? static_cast<int>(lua_tointeger(L, 1)) : 1;
    ImGui::PopStyleVar(count);
    return 0;
}

static int Lua_Overlay_Open(lua_State* L) {
    std::string title = "";
    if (lua_gettop(L) >= 1 && lua_isstring(L, 1)) {
        title = lua_tostring(L, 1);
    }
    if (!LuaOverlayManager::Get().GetCustomWindows().empty() || !title.empty()) {
        LuaOverlayManager::Get().SetWindowOpen(title, true);
        lua_pushboolean(L, 1);
        return 1;
    }
    if (RuntimeConfig::Get().EnableInGameGui)
        menu::isOpen = true;
    lua_pushboolean(L, menu::isOpen ? 1 : 0);
    return 1;
}

static int Lua_Overlay_Close(lua_State* L) {
    std::string title = "";
    if (lua_gettop(L) >= 1 && lua_isstring(L, 1)) {
        title = lua_tostring(L, 1);
    }
    if (!LuaOverlayManager::Get().GetCustomWindows().empty() || !title.empty()) {
        LuaOverlayManager::Get().SetWindowOpen(title, false);
        lua_pushboolean(L, 0);
        return 1;
    }
    menu::isOpen = false;
    lua_pushboolean(L, 0);
    return 1;
}

static int Lua_Overlay_SetMenuKey(lua_State* L) {
    const char* keyName = luaL_checkstring(L, 1);
    int vk = LuaEngine::ResolveVirtualKeyCode(keyName);
    if (vk > 0) {
        globals::openMenuKey = vk;
        lua_pushboolean(L, 1);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int Lua_Overlay_IsOpen(lua_State* L) {
    std::string title = "";
    if (lua_gettop(L) >= 1 && lua_isstring(L, 1)) {
        title = lua_tostring(L, 1);
    }
    if (!LuaOverlayManager::Get().GetCustomWindows().empty() || !title.empty()) {
        bool res = LuaOverlayManager::Get().IsWindowOpen(title);
        lua_pushboolean(L, res ? 1 : 0);
        return 1;
    }
    lua_pushboolean(L, menu::isOpen ? 1 : 0);
    return 1;
}

static int Lua_Overlay_Toggle(lua_State* L) {
    std::string title = "";
    if (lua_gettop(L) >= 1 && lua_isstring(L, 1)) {
        title = lua_tostring(L, 1);
    }
    if (!LuaOverlayManager::Get().GetCustomWindows().empty() || !title.empty()) {
        bool res = LuaOverlayManager::Get().ToggleWindow(title);
        lua_pushboolean(L, res ? 1 : 0);
        return 1;
    }
    if (RuntimeConfig::Get().EnableInGameGui) {
        menu::isOpen = !menu::isOpen;
    }
    lua_pushboolean(L, menu::isOpen ? 1 : 0);
    return 1;
}

static int Lua_Overlay_ToggleMenu(lua_State* L) {
    if (RuntimeConfig::Get().EnableInGameGui) {
        if (lua_gettop(L) >= 1 && lua_isboolean(L, 1)) {
            menu::isOpen = (lua_toboolean(L, 1) != 0);
        } else {
            menu::isOpen = !menu::isOpen;
        }
    }
    lua_pushboolean(L, menu::isOpen ? 1 : 0);
    return 1;
}

static int OverlayMemberThunk(lua_State* L) {
    int argCount = lua_gettop(L);
    if (argCount >= 1 && lua_rawequal(L, 1, lua_upvalueindex(1))) {
        lua_remove(L, 1);
        --argCount;
    }
    lua_pushvalue(L, lua_upvalueindex(2));
    lua_insert(L, 1);
    lua_call(L, argCount, LUA_MULTRET);
    return lua_gettop(L);
}

static void SetOverlayMember(lua_State* L, lua_CFunction fn, const char* name) {
    lua_pushvalue(L, -1);
    lua_pushcfunction(L, fn);
    lua_pushcclosure(L, OverlayMemberThunk, 2);
    lua_setfield(L, -2, name);
}

void RegisterLuaOverlayBindings(lua_State* L) {
    if (!L) return;
    LuaOverlayManager::Get().SetLuaState(L);

    lua_newtable(L);

    SetOverlayMember(L, Lua_Overlay_Notify,             "Notify");
    SetOverlayMember(L, Lua_Overlay_Toast,              "Toast");
    SetOverlayMember(L, Lua_Overlay_ClearNotifications, "ClearNotifications");

    SetOverlayMember(L, Lua_Overlay_SetStat,            "SetStat");
    SetOverlayMember(L, Lua_Overlay_RemoveStat,         "RemoveStat");
    SetOverlayMember(L, Lua_Overlay_ClearStats,         "ClearStats");
    SetOverlayMember(L, Lua_Overlay_ShowStats,          "ShowStats");
    SetOverlayMember(L, Lua_Overlay_IsStatsVisible,     "IsStatsVisible");
    SetOverlayMember(L, Lua_Overlay_SetStatsAnchor,     "SetStatsAnchor");
    SetOverlayMember(L, Lua_Overlay_SetStatsPosition,   "SetStatsPosition");

    SetOverlayMember(L, Lua_Overlay_DrawText,           "DrawText");
    SetOverlayMember(L, Lua_Overlay_DrawRect,           "DrawRect");
    SetOverlayMember(L, Lua_Overlay_DrawLine,           "DrawLine");
    SetOverlayMember(L, Lua_Overlay_DrawCircle,         "DrawCircle");
    SetOverlayMember(L, Lua_Overlay_DrawProgressBar,    "DrawProgressBar");
    SetOverlayMember(L, Lua_Overlay_RegisterDrawCallback, "RegisterDrawCallback");
    SetOverlayMember(L, Lua_Overlay_GetViewportSize,    "GetViewportSize");

    SetOverlayMember(L, Lua_Overlay_RegisterWindow,     "RegisterWindow");
    SetOverlayMember(L, Lua_Overlay_Text,               "Text");
    SetOverlayMember(L, Lua_Overlay_TextColored,        "TextColored");
    SetOverlayMember(L, Lua_Overlay_Button,             "Button");
    SetOverlayMember(L, Lua_Overlay_Checkbox,           "Checkbox");
    SetOverlayMember(L, Lua_Overlay_SliderFloat,        "SliderFloat");
    SetOverlayMember(L, Lua_Overlay_SliderInt,          "SliderInt");
    SetOverlayMember(L, Lua_Overlay_Separator,          "Separator");
    SetOverlayMember(L, Lua_Overlay_SameLine,           "SameLine");
    SetOverlayMember(L, Lua_Overlay_InputText,          "InputText");
    SetOverlayMember(L, Lua_Overlay_Combo,              "Combo");
    SetOverlayMember(L, Lua_Overlay_Selectable,         "Selectable");
    SetOverlayMember(L, Lua_Overlay_InputFloat,         "InputFloat");
    SetOverlayMember(L, Lua_Overlay_InputInt,           "InputInt");
    SetOverlayMember(L, Lua_Overlay_RadioButton,        "RadioButton");
    SetOverlayMember(L, Lua_Overlay_CollapsingHeader,   "CollapsingHeader");
    SetOverlayMember(L, Lua_Overlay_TreeNode,           "TreeNode");
    SetOverlayMember(L, Lua_Overlay_TreePop,            "TreePop");
    SetOverlayMember(L, Lua_Overlay_BeginChild,         "BeginChild");
    SetOverlayMember(L, Lua_Overlay_EndChild,           "EndChild");
    SetOverlayMember(L, Lua_Overlay_Indent,             "Indent");
    SetOverlayMember(L, Lua_Overlay_Unindent,           "Unindent");
    SetOverlayMember(L, Lua_Overlay_Spacing,            "Spacing");
    SetOverlayMember(L, Lua_Overlay_Dummy,              "Dummy");
    SetOverlayMember(L, Lua_Overlay_Tooltip,            "Tooltip");
    SetOverlayMember(L, Lua_Overlay_IsItemHovered,      "IsItemHovered");
    SetOverlayMember(L, Lua_Overlay_IsItemClicked,      "IsItemClicked");
    SetOverlayMember(L, Lua_Overlay_ProgressBar,        "ProgressBar");
    SetOverlayMember(L, Lua_Overlay_BeginTable,         "BeginTable");
    SetOverlayMember(L, Lua_Overlay_EndTable,           "EndTable");
    SetOverlayMember(L, Lua_Overlay_TableNextRow,       "TableNextRow");
    SetOverlayMember(L, Lua_Overlay_TableNextColumn,    "TableNextColumn");
    SetOverlayMember(L, Lua_Overlay_TableSetupColumn,   "TableSetupColumn");
    SetOverlayMember(L, Lua_Overlay_PushStyleColor,     "PushStyleColor");
    SetOverlayMember(L, Lua_Overlay_PopStyleColor,      "PopStyleColor");
    SetOverlayMember(L, Lua_Overlay_PushStyleVar,       "PushStyleVar");
    SetOverlayMember(L, Lua_Overlay_PopStyleVar,        "PopStyleVar");
    SetOverlayMember(L, Lua_Overlay_IsOpen,             "IsOpen");
    SetOverlayMember(L, Lua_Overlay_Open,               "Open");
    SetOverlayMember(L, Lua_Overlay_Close,              "Close");
    SetOverlayMember(L, Lua_Overlay_Toggle,             "Toggle");
    SetOverlayMember(L, Lua_Overlay_ToggleMenu,         "ToggleMenu");
    SetOverlayMember(L, Lua_Overlay_SetMenuKey,         "SetMenuKey");

    lua_setglobal(L, "Overlay");

    EngineDiscovery::WriteLogLine("[Overlay] Registered In-Game Overlay Lua API (Toasts, Stats HUD, Canvas, Mod Windows)\n");
}

}
}
