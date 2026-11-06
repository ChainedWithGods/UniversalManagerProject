#include "UniversalOverlay.h"
#include "../../overlay/LuaOverlay.h"
#include "../../core/EngineDiscovery.h"
#include <algorithm>
#include <string>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

namespace LuaEngine {
namespace BuiltinMods {
namespace UniversalOverlay {

bool ToggleMetrics() {
    bool next = !Overlay::LuaOverlayManager::Get().IsStatsVisible();
    Overlay::LuaOverlayManager::Get().SetStatsVisible(next);
    return next;
}

void ShowMetrics(bool show) {
    Overlay::LuaOverlayManager::Get().SetStatsVisible(show);
}

bool IsMetricsVisible() {
    return Overlay::LuaOverlayManager::Get().IsStatsVisible();
}

static int Lua_ToggleMetrics(lua_State* L) {
    bool newState = false;
    if (lua_gettop(L) >= 1 && lua_isboolean(L, 1)) {
        bool show = (lua_toboolean(L, 1) != 0);
        ShowMetrics(show);
        newState = show;
    } else {
        newState = ToggleMetrics();
    }
    lua_pushboolean(L, newState ? 1 : 0);
    return 1;
}

static int Lua_ShowMetrics(lua_State* L) {
    bool show = (lua_gettop(L) >= 1 && lua_isboolean(L, 1)) ? (lua_toboolean(L, 1) != 0) : true;
    ShowMetrics(show);
    return 0;
}

static int Lua_HideMetrics(lua_State* L) {
    ShowMetrics(false);
    return 0;
}

static int Lua_IsMetricsVisible(lua_State* L) {
    lua_pushboolean(L, IsMetricsVisible() ? 1 : 0);
    return 1;
}

static bool IsNotificationTypeString(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return (s == "info" || s == "success" || s == "warning" || s == "error" || s == "ok" || s == "warn" || s == "err");
}

static int Lua_GlobalNotify(lua_State* L) {
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

    Overlay::LuaOverlayManager::Get().ShowNotification(title, msg, duration, typeStr);
    return 0;
}

static int Lua_GlobalToast(lua_State* L) {
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

    Overlay::LuaOverlayManager::Get().ShowNotification(msg, "", duration, typeStr);
    return 0;
}

void Register(lua_State* L) {
    if (!L) return;

    lua_pushcfunction(L, Lua_ToggleMetrics);   lua_setglobal(L, "ToggleMetrics");
    lua_pushcfunction(L, Lua_ToggleMetrics);   lua_setglobal(L, "ToggleMetricsHUD");
    lua_pushcfunction(L, Lua_ShowMetrics);     lua_setglobal(L, "ShowMetrics");
    lua_pushcfunction(L, Lua_ShowMetrics);     lua_setglobal(L, "ShowMetricsHUD");
    lua_pushcfunction(L, Lua_HideMetrics);     lua_setglobal(L, "HideMetrics");
    lua_pushcfunction(L, Lua_HideMetrics);     lua_setglobal(L, "HideMetricsHUD");
    lua_pushcfunction(L, Lua_IsMetricsVisible); lua_setglobal(L, "IsMetricsVisible");
    lua_pushcfunction(L, Lua_IsMetricsVisible); lua_setglobal(L, "IsMetricsHUDVisible");

    lua_pushcfunction(L, Lua_GlobalNotify);    lua_setglobal(L, "Notify");
    lua_pushcfunction(L, Lua_GlobalToast);     lua_setglobal(L, "Toast");

    EngineDiscovery::WriteLogLine("[UniversalOverlay] Registered built-in mod globals: ToggleMetrics, ShowMetrics, HideMetrics, IsMetricsVisible, Notify, Toast\n");
}

}
}
}
