#pragma once

struct lua_State;

namespace LuaEngine {
namespace BuiltinMods {
namespace UniversalOverlay {

void Register(lua_State* L);

bool ToggleMetrics();
void ShowMetrics(bool show);
bool IsMetricsVisible();

}
}
}
