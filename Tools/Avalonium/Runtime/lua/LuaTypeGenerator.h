#pragma once

#include <string>

struct lua_State;

namespace LuaEngine {

class LuaTypeGenerator {
public:
    static bool GenerateLuaTypes(const std::string& outputDir);
    static bool DumpAllObjects(const std::string& filePath);
    static int Lua_GenerateLuaTypes(lua_State* L);
    static int Lua_DumpAllObjects(lua_State* L);
    static int Lua_ForEachUObject(lua_State* L);
};

}
