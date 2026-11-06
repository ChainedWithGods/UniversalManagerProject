#pragma once

struct lua_State;

namespace LuaEngine {
namespace BuiltinMods {
namespace UniversalHUD {

bool ToggleAllHUD(bool hide);
bool IsHUDHidden();

void Register(lua_State* L);

}
}
}
