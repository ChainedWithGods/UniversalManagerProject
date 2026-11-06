#pragma once

struct lua_State;

namespace LuaEngine {
namespace BuiltinMods {

void RegisterAll(lua_State* L);

}
}
