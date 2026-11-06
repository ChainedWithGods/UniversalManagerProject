#include "BuiltinMods.h"
#include "UniversalHUD.h"
#include "UniversalOverlay.h"

namespace LuaEngine {
namespace BuiltinMods {

void RegisterAll(lua_State* L) {
    if (!L) return;

    UniversalHUD::Register(L);

    UniversalOverlay::Register(L);
}

}
}
