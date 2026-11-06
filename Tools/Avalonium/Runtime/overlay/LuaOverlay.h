#pragma once

#include "../../Helpers/Universal-Dear-ImGui-Hook/stdafx.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>

struct lua_State;

namespace LuaEngine {
namespace Overlay {

enum class NotificationType : uint8_t {
    Info = 0,
    Success,
    Warning,
    Error
};

enum class StatsAnchor : uint8_t {
    TopLeft = 0,
    TopRight,
    BottomLeft,
    BottomRight
};

struct ToastNotification {
    uint32_t Id = 0;
    std::string Title;
    std::string Message;
    float Duration = 3.0f;
    float Elapsed = 0.0f;
    NotificationType Type = NotificationType::Info;
    float SlideOffset = 50.0f;
};

struct StatItem {
    std::string Key;
    std::string Value;
    uint32_t Color = 0;
    int Order = 0;
};

enum class DrawCmdType : uint8_t {
    Text = 0,
    Rect,
    RectFilled,
    Line,
    Circle,
    CircleFilled,
    ProgressBar
};

struct DrawCommand {
    DrawCmdType Type;
    float X1 = 0, Y1 = 0, X2 = 0, Y2 = 0;
    float Radius = 0;
    float Thickness = 1.0f;
    float Rounding = 0.0f;
    float Fraction = 0.0f;
    uint32_t Color = 0xFFFFFFFF;
    uint32_t BgColor = 0xFF222222;
    std::string Text;
    float FontSize = 0.0f;
};

struct CustomLuaWindow {
    std::string Title;
    int CallbackRef = -1;
    bool IsOpen = true;
};

class LuaOverlayManager {
public:
    static LuaOverlayManager& Get();

    void Render();

    void ShowNotification(const std::string& title, const std::string& message = "", float duration = 3.5f, const std::string& type = "info");
    void ClearNotifications();

    void SetStat(const std::string& key, const std::string& value, uint32_t color = 0);
    void RemoveStat(const std::string& key);
    void ClearStats();
    void SetStatsVisible(bool visible);
    bool IsStatsVisible() const { return m_StatsVisible; }
    void SetLiveMetricsEnabled(bool enabled);
    bool IsLiveMetricsEnabled() const { return m_LiveMetricsEnabled; }
    void SetStatsAnchor(const std::string& anchor);
    void SetStatsPosition(float x, float y);

    void AddDrawText(const std::string& text, float x, float y, uint32_t color = 0xFFFFFFFF, float fontSize = 0.0f);
    void AddDrawRect(float x, float y, float w, float h, uint32_t color = 0xFFFFFFFF, bool filled = false, float thickness = 1.0f, float rounding = 4.0f);
    void AddDrawLine(float x1, float y1, float x2, float y2, uint32_t color = 0xFFFFFFFF, float thickness = 1.0f);
    void AddDrawCircle(float x, float y, float radius, uint32_t color = 0xFFFFFFFF, bool filled = false, float thickness = 1.0f);
    void AddDrawProgressBar(float x, float y, float w, float h, float fraction, const std::string& text = "", uint32_t barColor = 0xFF2ECC71, uint32_t bgColor = 0x88111111);

    void RegisterDrawCallback(int callbackRef);
    void UnregisterDrawCallback(int callbackRef);

    void RegisterWindow(const std::string& title, int callbackRef);
    void UnregisterWindow(const std::string& title);
    bool HasVisibleCustomWindows() const;
    bool ToggleWindow(const std::string& title = "");
    void SetWindowOpen(const std::string& title, bool open);
    bool IsWindowOpen(const std::string& title) const;
    const std::vector<CustomLuaWindow>& GetCustomWindows() const { return m_CustomWindows; }

    void ClearAll(lua_State* L);

    void SetLuaState(lua_State* L) { m_L = L; }

private:
    LuaOverlayManager();
    ~LuaOverlayManager();

    void RenderToasts(ImDrawList* drawList, const ImVec2& displaySize, float deltaTime);
    void RenderStatsHUD(ImDrawList* drawList, const ImVec2& displaySize);
    void RenderCanvas(ImDrawList* drawList);
    void RenderCustomWindows();

    lua_State* m_L = nullptr;
    std::recursive_mutex m_OverlayMutex;

    uint32_t m_NextToastId = 1;
    std::vector<ToastNotification> m_Toasts;

    bool m_StatsVisible = false;
    bool m_LiveMetricsEnabled = true;
    StatsAnchor m_StatsAnchor = StatsAnchor::TopRight;
    float m_CustomStatsX = -1.0f;
    float m_CustomStatsY = -1.0f;
    int m_NextStatOrder = 1;
    std::unordered_map<std::string, StatItem> m_Stats;

    std::vector<DrawCommand> m_PersistentDrawCmds;
    std::vector<DrawCommand> m_FrameDrawCmds;
    std::vector<int> m_DrawCallbackRefs;

    std::vector<CustomLuaWindow> m_CustomWindows;

    std::chrono::high_resolution_clock::time_point m_LastRenderTime{};
};

void RegisterLuaOverlayBindings(lua_State* L);

}
}
