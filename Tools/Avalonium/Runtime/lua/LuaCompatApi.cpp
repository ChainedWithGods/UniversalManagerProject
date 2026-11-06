#include "LuaCompatApi.h"
#include "LuaUObject.h"
#include "LuaTypes.h"
#include "LuaHookManager.h"
#include "LuaManager.h"
#include "../core/EngineDiscovery.h"
#include "../core/NameResolution.h"
#include "../core/ObjectIndex.h"
#include "../core/ObjectUtils.h"
#include "../core/ObjectArray.h"
#include "../console/ConsoleInternal.h"
#include "../utils/SafeMemory.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern EngineState g_State;

namespace LuaEngine {

static int Version_GetMajor(lua_State* L) {
    lua_pushinteger(L, g_State.UEMajor);
    return 1;
}

static int Version_GetMinor(lua_State* L) {
    lua_pushinteger(L, g_State.UEMinor);
    return 1;
}

static int Version_Compare(lua_State* L, bool (*pred)(int64_t a, int64_t b)) {

    int first = (lua_istable(L, 1) || lua_isuserdata(L, 1)) ? 2 : 1;
    int64_t major = static_cast<int64_t>(luaL_checkinteger(L, first));
    int64_t minor = static_cast<int64_t>(luaL_optinteger(L, first + 1, 0));
    int64_t lhs = static_cast<int64_t>(g_State.UEMajor) * 1000 + g_State.UEMinor;
    int64_t rhs = major * 1000 + minor;
    lua_pushboolean(L, pred(lhs, rhs) ? 1 : 0);
    return 1;
}

static int Version_IsEqual(lua_State* L) {
    return Version_Compare(L, [](int64_t a, int64_t b) { return a == b; });
}
static int Version_IsAtLeast(lua_State* L) {
    return Version_Compare(L, [](int64_t a, int64_t b) { return a >= b; });
}
static int Version_IsAtMost(lua_State* L) {
    return Version_Compare(L, [](int64_t a, int64_t b) { return a <= b; });
}
static int Version_IsBelow(lua_State* L) {
    return Version_Compare(L, [](int64_t a, int64_t b) { return a < b; });
}
static int Version_IsAbove(lua_State* L) {
    return Version_Compare(L, [](int64_t a, int64_t b) { return a > b; });
}

static const char* FNAME_MT = "Avalonium.FName";

struct LuaFNameData {
    uint32_t ComparisonIndex = 0;
    uint32_t Number = 0;
};

static int FName_Index_Impl(lua_State* L) {
    {
        auto* d = static_cast<LuaFNameData*>(luaL_testudata(L, 1, FNAME_MT));
        if (!d) { lua_pushnil(L); return 1; }
        const char* k = lua_tostring(L, 2);
        if (!k) { lua_pushnil(L); return 1; }
        if (strcmp(k, "ToString") == 0 || strcmp(k, "get") == 0) {
            lua_pushcfunction(L, [](lua_State* L) -> int {
                auto* d2 = static_cast<LuaFNameData*>(luaL_checkudata(L, 1, FNAME_MT));
                if (!d2) { lua_pushstring(L, ""); return 1; }
                std::string s = EngineDiscovery::GetCachedFName(g_State, static_cast<int32_t>(d2->ComparisonIndex));
                lua_pushstring(L, s.c_str());
                return 1;
            });
            return 1;
        }
        if (strcmp(k, "GetComparisonIndex") == 0) {
            lua_pushcfunction(L, [](lua_State* L) -> int {
                auto* d2 = static_cast<LuaFNameData*>(luaL_checkudata(L, 1, FNAME_MT));
                lua_pushinteger(L, d2 ? static_cast<lua_Integer>(d2->ComparisonIndex) : 0);
                return 1;
            });
            return 1;
        }
        if (strcmp(k, "GetNumber") == 0) {
            lua_pushcfunction(L, [](lua_State* L) -> int {
                auto* d2 = static_cast<LuaFNameData*>(luaL_checkudata(L, 1, FNAME_MT));
                lua_pushinteger(L, d2 ? static_cast<lua_Integer>(d2->Number) : 0);
                return 1;
            });
            return 1;
        }
        if (strcmp(k, "Equals") == 0) {
            lua_pushcfunction(L, [](lua_State* L) -> int {
                auto* d2 = static_cast<LuaFNameData*>(luaL_checkudata(L, 1, FNAME_MT));
                auto* o = static_cast<LuaFNameData*>(luaL_testudata(L, 2, FNAME_MT));
                bool eq = d2 && o && d2->ComparisonIndex == o->ComparisonIndex;
                if (!eq && d2 && lua_isstring(L, 2)) {
                    std::string other = lua_tostring(L, 2);
                    std::string self = EngineDiscovery::GetCachedFName(g_State, static_cast<int32_t>(d2->ComparisonIndex));
                    eq = _stricmp(self.c_str(), other.c_str()) == 0;
                }
                lua_pushboolean(L, eq ? 1 : 0);
                return 1;
            });
            return 1;
        }
        if (strcmp(k, "type") == 0) {
            lua_pushcfunction(L, [](lua_State* L) -> int { lua_pushstring(L, "FName"); return 1; });
            return 1;
        }
        if (strcmp(k, "IsValid") == 0) {
            lua_pushcfunction(L, [](lua_State* L) -> int {
                auto* d2 = static_cast<LuaFNameData*>(luaL_checkudata(L, 1, FNAME_MT));
                lua_pushboolean(L, (d2 && d2->ComparisonIndex != 0) ? 1 : 0);
                return 1;
            });
            return 1;
        }
        lua_pushnil(L);
        return 1;
    }
}

static void EnsureNameMetatable(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, FNAME_MT);
    if (!lua_isnil(L, -1)) return;
    lua_pop(L, 1);

    luaL_newmetatable(L, FNAME_MT);
    lua_pushstring(L, "FNameUserdata");
    lua_setfield(L, -2, "__name");
    lua_pushcfunction(L, FName_Index_Impl);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, [](lua_State* L) -> int {
        auto* d = static_cast<LuaFNameData*>(luaL_checkudata(L, 1, FNAME_MT));
        if (!d) { lua_pushstring(L, ""); return 1; }
        std::string s = EngineDiscovery::GetCachedFName(g_State, static_cast<int32_t>(d->ComparisonIndex));
        if (d->Number > 0) s += "_" + std::to_string(d->Number);
        lua_pushstring(L, s.c_str());
        return 1;
    });
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, [](lua_State* L) -> int {
        auto* a = static_cast<LuaFNameData*>(luaL_testudata(L, 1, FNAME_MT));
        auto* b = static_cast<LuaFNameData*>(luaL_testudata(L, 2, FNAME_MT));
        lua_pushboolean(L, (a && b && a->ComparisonIndex == b->ComparisonIndex) ? 1 : 0);
        return 1;
    });
    lua_setfield(L, -2, "__eq");

    lua_pushcfunction(L, [](lua_State* L) -> int {
        std::string left;
        std::string right;
        if (auto* a = static_cast<LuaFNameData*>(luaL_testudata(L, 1, FNAME_MT))) {
            left = EngineDiscovery::GetCachedFName(g_State, static_cast<int32_t>(a->ComparisonIndex));
            if (a->Number > 0) left += "_" + std::to_string(a->Number);
        } else if (lua_isstring(L, 1) || lua_isnumber(L, 1)) {
            size_t len = 0;
            const char* s = lua_tolstring(L, 1, &len);
            if (s) left.assign(s, len);
        }
        if (auto* b = static_cast<LuaFNameData*>(luaL_testudata(L, 2, FNAME_MT))) {
            right = EngineDiscovery::GetCachedFName(g_State, static_cast<int32_t>(b->ComparisonIndex));
            if (b->Number > 0) right += "_" + std::to_string(b->Number);
        } else if (lua_isstring(L, 2) || lua_isnumber(L, 2)) {
            size_t len = 0;
            const char* s = lua_tolstring(L, 2, &len);
            if (s) right.assign(s, len);
        }
        const std::string joined = left + right;
        lua_pushlstring(L, joined.data(), joined.size());
        return 1;
    });
    lua_setfield(L, -2, "__concat");
}

void PushFName(lua_State* L, uint32_t comparisonIndex, uint32_t number) {
    auto* d = static_cast<LuaFNameData*>(lua_newuserdata(L, sizeof(LuaFNameData)));
    d->ComparisonIndex = comparisonIndex;
    d->Number = number;
    EnsureNameMetatable(L);
    lua_setmetatable(L, -2);
}

static const char* THREADID_MT = "Avalonium.ThreadId";

struct LuaThreadIdData {
    int64_t Id = 0;
};

static int ThreadId_Index_Impl(lua_State* L) {
    auto* d = static_cast<LuaThreadIdData*>(luaL_testudata(L, 1, THREADID_MT));
    const char* k = lua_tostring(L, 2);
    if (!d || !k) { lua_pushnil(L); return 1; }
    if (strcmp(k, "ToString") == 0 || strcmp(k, "get") == 0) {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            auto* d2 = static_cast<LuaThreadIdData*>(luaL_checkudata(L, 1, THREADID_MT));
            lua_pushstring(L, d2 ? std::to_string(d2->Id).c_str() : "0");
            return 1;
        });
        return 1;
    }
    if (strcmp(k, "GetId") == 0) {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            auto* d2 = static_cast<LuaThreadIdData*>(luaL_checkudata(L, 1, THREADID_MT));
            lua_pushinteger(L, d2 ? static_cast<lua_Integer>(d2->Id) : 0);
            return 1;
        });
        return 1;
    }
    if (strcmp(k, "type") == 0) {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            lua_pushstring(L, "ThreadIdUserdata");
            return 1;
        });
        return 1;
    }
    if (strcmp(k, "IsValid") == 0) {
        lua_pushcfunction(L, [](lua_State* L) -> int { lua_pushboolean(L, 1); return 1; });
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static void EnsureThreadIdMetatable(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, THREADID_MT);
    if (!lua_isnil(L, -1)) return;
    lua_pop(L, 1);

    luaL_newmetatable(L, THREADID_MT);
    lua_pushstring(L, "ThreadIdUserdata");
    lua_setfield(L, -2, "__name");
    lua_pushcfunction(L, ThreadId_Index_Impl);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, [](lua_State* L) -> int {
        auto* d = static_cast<LuaThreadIdData*>(luaL_checkudata(L, 1, THREADID_MT));
        lua_pushstring(L, d ? std::to_string(d->Id).c_str() : "0");
        return 1;
    });
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, [](lua_State* L) -> int {
        auto* a = static_cast<LuaThreadIdData*>(luaL_testudata(L, 1, THREADID_MT));
        auto* b = static_cast<LuaThreadIdData*>(luaL_testudata(L, 2, THREADID_MT));
        lua_pushboolean(L, (a && b && a->Id == b->Id) ? 1 : 0);
        return 1;
    });
    lua_setfield(L, -2, "__eq");
}

static int ThreadId_Constructor(lua_State* L) {
    auto* d = static_cast<LuaThreadIdData*>(lua_newuserdata(L, sizeof(LuaThreadIdData)));
    d->Id = static_cast<int64_t>(luaL_optinteger(L, 1, 0));
    EnsureThreadIdMetatable(L);
    lua_setmetatable(L, -2);
    return 1;
}

void PublishCompatTypeRecords(lua_State* L) {
    EnsureNameMetatable(L);
    lua_pushvalue(L, -1);
    lua_setglobal(L, "FNameUserdata");
    lua_setglobal(L, "fname");

    EnsureThreadIdMetatable(L);
    lua_pushvalue(L, -1);
    lua_setglobal(L, "ThreadIdUserdata");
    lua_setglobal(L, "threadid");

    lua_pushcfunction(L, ThreadId_Constructor);
    lua_setglobal(L, "ThreadId");
}

int FName_IsValid(lua_State* L) {
    auto* d = static_cast<LuaFNameData*>(luaL_checkudata(L, 1, FNAME_MT));
    lua_pushboolean(L, (d && d->ComparisonIndex != 0) ? 1 : 0);
    return 1;
}

int GetFNameHandle(lua_State* L, int index, uint32_t* comparisonIndex, uint32_t* number) {
    auto* d = static_cast<LuaFNameData*>(luaL_testudata(L, index, FNAME_MT));
    if (!d) return 0;
    if (comparisonIndex) *comparisonIndex = d->ComparisonIndex;
    if (number) *number = d->Number;
    return 1;
}

static int FName_Constructor(lua_State* L) {

    const char* str = luaL_checkstring(L, 1);
    int32_t idx = str ? EngineDiscovery::ResolveFNameIndex(g_State, str) : -1;
    PushFName(L, idx >= 0 ? static_cast<uint32_t>(idx) : 0);
    return 1;
}

static const char* FTEXT_MT = "Avalonium.FText";

struct LuaFTextData {
    void* TextAddr = nullptr;
    char Value[256] = { 0 };
};

static int FText_Index(lua_State* L) {
    auto* d = static_cast<LuaFTextData*>(luaL_testudata(L, 1, FTEXT_MT));
    const char* k = lua_tostring(L, 2);
    if (!d || !k) { lua_pushnil(L); return 1; }
    if (_stricmp(k, "ToString") == 0 || strcmp(k, "get") == 0) {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            auto* d2 = static_cast<LuaFTextData*>(luaL_checkudata(L, 1, FTEXT_MT));
            std::string out;
            if (d2 && d2->TextAddr) {

                FString fs = {};
                if (SafeMem_Read(static_cast<uint8_t*>(d2->TextAddr) + 8, &fs, sizeof(fs)) && fs.Data) {
                    out = fs.ToStdString();
                }
            } else if (d2) {
                out = d2->Value;
            }
            lua_pushstring(L, out.c_str());
            return 1;
        });
        return 1;
    }
    if (_stricmp(k, "type") == 0) {
        lua_pushcfunction(L, [](lua_State* L) -> int { lua_pushstring(L, "FText"); return 1; });
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static void PushFText(lua_State* L) {
    auto* d = static_cast<LuaFTextData*>(lua_newuserdata(L, sizeof(LuaFTextData)));
    d->TextAddr = nullptr;
    d->Value[0] = '\0';
    if (luaL_newmetatable(L, FTEXT_MT)) {
        lua_pushcfunction(L, FText_Index);      lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, [](lua_State* L) -> int { lua_pushstring(L, "FText"); return 1; });
        lua_setfield(L, -2, "__tostring");
    }
    lua_setmetatable(L, -2);
    return;
}

void PushFTextRef(lua_State* L, void* textAddr) {
    PushFText(L);
    auto* d = static_cast<LuaFTextData*>(lua_touserdata(L, -1));
    if (d) d->TextAddr = textAddr;
}

void PushFTextValue(lua_State* L, const char* utf8) {
    PushFText(L);
    auto* d = static_cast<LuaFTextData*>(lua_touserdata(L, -1));
    if (d && utf8) strncpy_s(d->Value, utf8, _TRUNCATE);
}

bool GetLuaFTextString(lua_State* L, int idx, std::string& out) {
    auto* d = static_cast<LuaFTextData*>(luaL_testudata(L, idx, FTEXT_MT));
    if (!d) return false;
    if (d->TextAddr) {

        FString fs = {};
        if (SafeMem_Read(static_cast<uint8_t*>(d->TextAddr) + 8, &fs, sizeof(fs)) && fs.Data) {
            out = fs.ToStdString();
            return true;
        }
        return false;
    }
    out = d->Value;
    return true;
}

static int FText_Constructor(lua_State* L) {
    const char* str = luaL_optstring(L, 1, "");
    PushFTextValue(L, str);
    return 1;
}

static int FString_Constructor(lua_State* L) {

    const char* str = luaL_optstring(L, 1, "");
    lua_pushstring(L, str);
    return 1;
}

struct KeyEntry { const char* Name; uint32_t Vk; };

static const KeyEntry kKeyTable[] = {
    { "LEFT_MOUSE_BUTTON", VK_LBUTTON }, { "RIGHT_MOUSE_BUTTON", VK_RBUTTON },
    { "CANCEL", VK_CANCEL }, { "MIDDLE_MOUSE_BUTTON", VK_MBUTTON },
    { "XBUTTON_ONE", VK_XBUTTON1 }, { "XBUTTON_TWO", VK_XBUTTON2 },
    { "BACKSPACE", VK_BACK }, { "TAB", VK_TAB }, { "CLEAR", VK_CLEAR },
    { "RETURN", VK_RETURN }, { "PAUSE", VK_PAUSE }, { "CAPS_LOCK", VK_CAPITAL },
    { "ESCAPE", VK_ESCAPE }, { "SPACE", VK_SPACE },
    { "PAGE_UP", VK_PRIOR }, { "PAGE_DOWN", VK_NEXT },
    { "END", VK_END }, { "HOME", VK_HOME },
    { "LEFT_ARROW", VK_LEFT }, { "UP_ARROW", VK_UP },
    { "RIGHT_ARROW", VK_RIGHT }, { "DOWN_ARROW", VK_DOWN },
    { "SELECT", VK_SELECT }, { "PRINT", VK_PRINT },
    { "EXECUTE", VK_EXECUTE }, { "PRINT_SCREEN", VK_SNAPSHOT },
    { "INS", VK_INSERT }, { "DEL", VK_DELETE }, { "HELP", VK_HELP },
    { "ZERO", '0' }, { "ONE", '1' }, { "TWO", '2' }, { "THREE", '3' }, { "FOUR", '4' },
    { "FIVE", '5' }, { "SIX", '6' }, { "SEVEN", '7' }, { "EIGHT", '8' },
    { "NINE", '9' },
    { "A", 'A' }, { "B", 'B' }, { "C", 'C' }, { "D", 'D' }, { "E", 'E' },
    { "F", 'F' }, { "G", 'G' }, { "H", 'H' }, { "I", 'I' }, { "J", 'J' },
    { "K", 'K' }, { "L", 'L' }, { "M", 'M' }, { "N", 'N' }, { "O", 'O' },
    { "P", 'P' }, { "Q", 'Q' }, { "R", 'R' }, { "S", 'S' }, { "T", 'T' },
    { "U", 'U' }, { "V", 'V' }, { "W", 'W' }, { "X", 'X' }, { "Y", 'Y' },
    { "Z", 'Z' },
    { "LEFT_WIN", VK_LWIN }, { "RIGHT_WIN", VK_RWIN }, { "APPS", VK_APPS },
    { "SLEEP", VK_SLEEP },
    { "NUM_ZERO", VK_NUMPAD0 }, { "NUM_ONE", VK_NUMPAD1 }, { "NUM_TWO", VK_NUMPAD2 },
    { "NUM_THREE", VK_NUMPAD3 }, { "NUM_FOUR", VK_NUMPAD4 }, { "NUM_FIVE", VK_NUMPAD5 },
    { "NUM_SIX", VK_NUMPAD6 }, { "NUM_SEVEN", VK_NUMPAD7 }, { "NUM_EIGHT", VK_NUMPAD8 },
    { "NUM_NINE", VK_NUMPAD9 },
    { "MULTIPLY", VK_MULTIPLY }, { "ADD", VK_ADD }, { "SEPARATOR", VK_SEPARATOR },
    { "SUBTRACT", VK_SUBTRACT }, { "DECIMAL", VK_DECIMAL }, { "DIVIDE", VK_DIVIDE },
    { "F1", VK_F1 }, { "F2", VK_F2 }, { "F3", VK_F3 }, { "F4", VK_F4 },
    { "F5", VK_F5 }, { "F6", VK_F6 }, { "F7", VK_F7 }, { "F8", VK_F8 },
    { "F9", VK_F9 }, { "F10", VK_F10 }, { "F11", VK_F11 }, { "F12", VK_F12 },
    { "F13", VK_F13 }, { "F14", VK_F14 }, { "F15", VK_F15 }, { "F16", VK_F16 },
    { "F17", VK_F17 }, { "F18", VK_F18 }, { "F19", VK_F19 }, { "F20", VK_F20 },
    { "F21", VK_F21 }, { "F22", VK_F22 }, { "F23", VK_F23 }, { "F24", VK_F24 },
    { "NUM_LOCK", VK_NUMLOCK }, { "SCROLL_LOCK", VK_SCROLL },
    { "VOLUME_MUTE", VK_VOLUME_MUTE }, { "VOLUME_DOWN", VK_VOLUME_DOWN },
    { "VOLUME_UP", VK_VOLUME_UP },
    { "MEDIA_NEXT_TRACK", VK_MEDIA_NEXT_TRACK }, { "MEDIA_PREV_TRACK", VK_MEDIA_PREV_TRACK },
    { "MEDIA_STOP", VK_MEDIA_STOP }, { "MEDIA_PLAY_PAUSE", VK_MEDIA_PLAY_PAUSE },
    { "OEM_ONE", VK_OEM_1 }, { "OEM_PLUS", VK_OEM_PLUS }, { "OEM_COMMA", VK_OEM_COMMA },
    { "OEM_MINUS", VK_OEM_MINUS }, { "OEM_PERIOD", VK_OEM_PERIOD },
    { "OEM_TWO", VK_OEM_2 }, { "OEM_THREE", VK_OEM_3 }, { "OEM_FOUR", VK_OEM_4 },
    { "OEM_FIVE", VK_OEM_5 }, { "OEM_SIX", VK_OEM_6 }, { "OEM_SEVEN", VK_OEM_7 },
    { "OEM_EIGHT", VK_OEM_8 }, { "OEM_102", VK_OEM_102 },
    { "ATTN", VK_ATTN }, { "CRSEL", VK_CRSEL }, { "EXSEL", VK_EXSEL },
    { "EREOF", VK_EREOF }, { "PLAY", VK_PLAY }, { "ZOOM", VK_ZOOM }, { "PA1", VK_PA1 },
    { "PACKET", VK_PACKET }, { "OEM_CLEAR", VK_OEM_CLEAR },

    { "IME_HANGUEL", VK_HANGUL }, { "IME_HANGUL", VK_HANGUL }, { "IME_ONTIME", 0x1D },
    { "IME_ON", 0x1D }, { "IME_OFF", 0x1E }, { "IME_KANA", VK_KANA },
    { "IME_JUNJA", VK_JUNJA }, { "IME_HANJA", VK_HANJA }, { "IME_FINAL", VK_FINAL },
    { "IME_KANJI", VK_KANJI },
    { "IME_CONVERT", VK_CONVERT }, { "IME_NONCONVERT", VK_NONCONVERT },
    { "IME_ACCEPT", VK_ACCEPT }, { "IME_MODECHANGE", VK_MODECHANGE },
    { "IME_PROCESS", VK_PROCESSKEY }, { "BROWSER_BACK", VK_BROWSER_BACK },
    { "BROWSER_FORWARD", VK_BROWSER_FORWARD }, { "BROWSER_REFRESH", VK_BROWSER_REFRESH },
    { "BROWSER_STOP", VK_BROWSER_STOP }, { "BROWSER_SEARCH", VK_BROWSER_SEARCH },
    { "BROWSER_FAVORITES", VK_BROWSER_FAVORITES }, { "BROWSER_HOME", VK_BROWSER_HOME },
    { "LAUNCH_MAIL", VK_LAUNCH_MAIL }, { "LAUNCH_MEDIA_SELECT", VK_LAUNCH_MEDIA_SELECT },
    { "LAUNCH_APP1", VK_LAUNCH_APP1 }, { "LAUNCH_APP2", VK_LAUNCH_APP2 },
};

static void RegisterKeyTables(lua_State* L) {
    lua_newtable(L);
    for (const KeyEntry& e : kKeyTable) {
        lua_pushinteger(L, e.Vk);
        lua_setfield(L, -2, e.Name);
    }

    lua_pushinteger(L, VK_LBUTTON);  lua_setfield(L, -2, "LEFT_CLICK");
    lua_pushinteger(L, VK_RBUTTON);  lua_setfield(L, -2, "RIGHT_CLICK");
    lua_pushinteger(L, VK_SHIFT);    lua_setfield(L, -2, "SHIFT");
    lua_pushinteger(L, VK_LSHIFT);   lua_setfield(L, -2, "LEFT_SHIFT");
    lua_pushinteger(L, VK_RSHIFT);   lua_setfield(L, -2, "RIGHT_SHIFT");
    lua_pushinteger(L, VK_CONTROL);  lua_setfield(L, -2, "CONTROL");
    lua_pushinteger(L, VK_CONTROL);  lua_setfield(L, -2, "CTRL");
    lua_pushinteger(L, VK_LCONTROL); lua_setfield(L, -2, "LEFT_CONTROL");
    lua_pushinteger(L, VK_RCONTROL); lua_setfield(L, -2, "RIGHT_CONTROL");
    lua_pushinteger(L, VK_MENU);     lua_setfield(L, -2, "ALT");
    lua_pushinteger(L, VK_LMENU);    lua_setfield(L, -2, "LEFT_ALT");
    lua_pushinteger(L, VK_RMENU);    lua_setfield(L, -2, "RIGHT_ALT");
    lua_setglobal(L, "Key");

    lua_newtable(L);
    lua_pushinteger(L, VK_SHIFT);   lua_setfield(L, -2, "SHIFT");
    lua_pushinteger(L, VK_CONTROL); lua_setfield(L, -2, "CONTROL");
    lua_pushinteger(L, VK_MENU);    lua_setfield(L, -2, "ALT");
    lua_pushinteger(L, VK_SHIFT);   lua_setfield(L, -2, "LEFT_SHIFT");
    lua_pushinteger(L, VK_SHIFT);   lua_setfield(L, -2, "RIGHT_SHIFT");
    lua_pushinteger(L, VK_CONTROL); lua_setfield(L, -2, "LEFT_CONTROL");
    lua_pushinteger(L, VK_CONTROL); lua_setfield(L, -2, "RIGHT_CONTROL");
    lua_pushinteger(L, VK_MENU);    lua_setfield(L, -2, "LEFT_ALT");
    lua_pushinteger(L, VK_MENU);    lua_setfield(L, -2, "RIGHT_ALT");
    lua_setglobal(L, "ModifierKey");
}

static void RegisterFlagTables(lua_State* L) {

    const bool ue5 = (g_State.UEMajor >= 5);
    const bool ue56plus = (g_State.UEMajor > 5) || (g_State.UEMajor == 5 && g_State.UEMinor >= 6);
    const bool ue58plus = (g_State.UEMajor > 5) || (g_State.UEMajor == 5 && g_State.UEMinor >= 8);

    struct { const char* Name; uint32_t Value; } objFlags[] = {
        { "RF_NoFlags", 0x00000000 },
        { "RF_Public", 0x00000001 },
        { "RF_Standalone", 0x00000002 },
        { "RF_MarkAsNative", 0x00000004 },
        { "RF_Transactional", 0x00000008 },
        { "RF_ClassDefaultObject", 0x00000010 },
        { "RF_ArchetypeObject", 0x00000020 },
        { "RF_Transient", 0x00000040 },
        { "RF_MarkAsRootSet", 0x00000080 },
        { "RF_TagGarbageTemp", 0x00000100 },
        { "RF_NeedInitialization", 0x00000200 },
        { "RF_NeedLoad", 0x00000400 },
        { "RF_NeedPostLoad", 0x00001000 },
        { "RF_NeedPostLoadSubobjects", 0x00002000 },
        { "RF_NewerVersionExists", 0x00004000 },
        { "RF_BeginDestroyed", 0x00008000 },
        { "RF_FinishDestroyed", 0x00010000 },
        { "RF_BeingRegenerated", 0x00020000 },
        { "RF_DefaultSubObject", 0x00040000 },
        { "RF_WasLoaded", 0x00080000 },
        { "RF_TextExportTransient", 0x00100000 },
        { "RF_LoadCompleted", 0x00200000 },
        { "RF_InheritableComponentTemplate", 0x00400000 },
        { "RF_DuplicateTransient", 0x00800000 },
        { "RF_StrongRefOnFrame", 0x01000000 },
        { "RF_NonPIEDuplicateTransient", 0x02000000 },
        { "RF_WillBeLoaded", 0x08000000 },
        { "RF_HasExternalPackage", 0x10000000 },
    };
    lua_newtable(L);
    for (auto& f : objFlags) {
        lua_pushinteger(L, f.Value);
        lua_setfield(L, -2, f.Name);
    }

    lua_pushinteger(L, 0x00000800);
    lua_setfield(L, -2, "RF_KeepForCooker");
    if (ue56plus) {

        lua_pushinteger(L, 0x00000800); lua_setfield(L, -2, "RF_GeneratePublicHash");
    }
    if (ue56plus) {
        lua_pushinteger(L, 0x04000000); lua_setfield(L, -2, "RF_ImmutableDefaultObject");
    } else {
        lua_pushinteger(L, 0x04000000); lua_setfield(L, -2, "RF_Dynamic");
    }
    if (ue5) {
        lua_pushinteger(L, 0x40000000); lua_setfield(L, -2, "RF_MirroredGarbage");
    }
    if (ue58plus) {
        lua_pushinteger(L, 0x20000000); lua_setfield(L, -2, "RF_MigratingAsset");
        lua_pushinteger(L, 0x80000000); lua_setfield(L, -2, "RF_HasDynamicImports");
    }
    if (ue5) {
        lua_pushinteger(L, 0x80000000); lua_setfield(L, -2, "RF_AllocatedInSharedPage");
    }

    lua_pushinteger(L, ue5 ? 0xFFFFFFFF : 0x1FFFFFFF);
    lua_setfield(L, -2, "RF_AllFlags");
    lua_setglobal(L, "EObjectFlags");

    struct { const char* Name; int32_t Value; } internalFlags[] = {
        { "ReachableInCluster", 1 << 23 },
        { "ClusterRoot", 1 << 24 },
        { "Native", 1 << 25 },
        { "Async", 1 << 26 },
        { "Unreachable", 1 << 28 },
        { "RootSet", 1 << 30 },
    };
    lua_newtable(L);
    for (auto& f : internalFlags) {
        lua_pushinteger(L, f.Value);
        lua_setfield(L, -2, f.Name);
    }
    if (ue5) {

        lua_pushinteger(L, 1 << 22); lua_setfield(L, -2, "AsyncLoadingPhase1");
        lua_pushinteger(L, 1 << 27); lua_setfield(L, -2, "AsyncLoadingPhase2");
        lua_pushinteger(L, 1 << 27); lua_setfield(L, -2, "AsyncLoading");
        lua_pushinteger(L, 1 << 29); lua_setfield(L, -2, "Garbage");
        lua_pushinteger(L, 1 << 29); lua_setfield(L, -2, "PendingKill");
    } else {
        lua_pushinteger(L, 1 << 27); lua_setfield(L, -2, "AsyncLoading");
        lua_pushinteger(L, 1 << 29); lua_setfield(L, -2, "PendingKill");
    }
    lua_pushinteger(L, (1 << 25) | (1 << 26) | (1 << 27));
    lua_setfield(L, -2, "GarbageCollectionKeepFlags");
    lua_pushinteger(L, 0x7F800000);
    lua_setfield(L, -2, "AllFlags");
    lua_setglobal(L, "EInternalObjectFlags");

    lua_newtable(L);
    lua_pushinteger(L, 0); lua_setfield(L, -2, "FNAME_Find");
    lua_pushinteger(L, 1); lua_setfield(L, -2, "FNAME_Add");
    lua_pushinteger(L, 2); lua_setfield(L, -2, "FNAME_Replace_Not_Safe_For_Threading");
    lua_setglobal(L, "EFindName");
}

static uintptr_t PropertyTypeClassIdentity(const char* typeName) {
    if (!typeName || !*typeName) return 0;
    if (UClass* cls = EngineDiscovery::FindClassCached(g_State, typeName)) {
        return reinterpret_cast<uintptr_t>(cls);
    }
    return reinterpret_cast<uintptr_t>(
        EngineDiscovery::GetRecordedPropertyTypeClass(typeName));
}

static int PropertyRecord_Index(lua_State* L) {
    if (!lua_isstring(L, 2)) { lua_pushnil(L); return 1; }
    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }

    lua_pushliteral(L, "Name");
    lua_rawget(L, 1);
    const char* typeName = lua_tostring(L, -1);
    lua_pop(L, 1);
    if (!typeName) { lua_pushnil(L); return 1; }

    lua_Integer value = 0;
    if (strcmp(key, "StaticPointer") == 0) {
        value = 0;
    } else if (strcmp(key, "FFieldClassPointer") == 0) {
        value = static_cast<lua_Integer>(PropertyTypeClassIdentity(typeName));
    } else {
        lua_pushnil(L);
        return 1;
    }

    lua_pushvalue(L, 2);
    lua_pushinteger(L, value);
    lua_rawset(L, 1);
    lua_pushinteger(L, value);
    return 1;
}

static int PropertyRecord_NewIndex(lua_State* L) {
    lua_rawset(L, 1);
    return 0;
}

static void RegisterPropertyTypes(lua_State* L) {
    struct { const char* Name; int Size; } types[] = {
        { "ObjectProperty", 8 }, { "ObjectPtrProperty", 8 },
        { "Int8Property", 1 }, { "Int16Property", 2 }, { "IntProperty", 4 },
        { "Int64Property", 8 }, { "ByteProperty", 1 },
        { "UInt16Property", 2 }, { "UInt32Property", 4 }, { "UInt64Property", 8 },
        { "NameProperty", 8 }, { "FloatProperty", 4 },
        { "BoolProperty", 1 }, { "ArrayProperty", 16 }, { "SetProperty", 16 },
        { "MapProperty", 32 }, { "StructProperty", 0 }, { "ClassProperty", 8 },
        { "WeakObjectProperty", 8 }, { "EnumProperty", 1 },
        { "TextProperty", 8 }, { "StrProperty", 16 },
        { "Utf8StrProperty", 16 }, { "AnsiStrProperty", 16 },
        { "InterfaceProperty", 8 }, { "DelegateProperty", 32 },
        { "MulticastDelegateProperty", 16 }, { "MulticastInlineDelegateProperty", 16 },
        { "MulticastSparseDelegateProperty", 8 }, { "SoftObjectProperty", 16 },
        { "SoftClassProperty", 16 }, { "DoubleProperty", 8 },
    };
    lua_newtable(L);
    int idx = 1;
    for (auto& t : types) {
        lua_newtable(L);
        lua_pushstring(L, t.Name);
        lua_setfield(L, -2, "Name");
        lua_pushinteger(L, t.Size);
        lua_setfield(L, -2, "Size");
        if (luaL_newmetatable(L, "Avalonium.PropertyTypeRecord")) {
            lua_pushcfunction(L, PropertyRecord_Index);    lua_setfield(L, -2, "__index");
            lua_pushcfunction(L, PropertyRecord_NewIndex); lua_setfield(L, -2, "__newindex");
            lua_pushstring(L, "PropertyTypeInfo");         lua_setfield(L, -2, "__name");
        }
        lua_setmetatable(L, -2);

        lua_pushvalue(L, -1);
        lua_setfield(L, -3, t.Name);
        lua_rawseti(L, -2, idx++);
    }
    lua_setglobal(L, "PropertyTypes");
}

static uint32_t ReadObjectFlags(UObject* obj) {
    if (!obj || g_State.UObject_FlagsOff < 0) return 0;
    uint32_t flags = 0;
    SafeMem_Read(reinterpret_cast<uint8_t*>(obj) + g_State.UObject_FlagsOff, &flags, 4);
    return flags;
}

int UObj_HasAllFlags(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    uint32_t want = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    lua_pushboolean(L, (ReadObjectFlags(obj) & want) == want ? 1 : 0);
    return 1;
}

int UObj_HasAnyFlags(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    uint32_t want = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    lua_pushboolean(L, (ReadObjectFlags(obj) & want) != 0 ? 1 : 0);
    return 1;
}

int UObj_HasAnyInternalFlags(lua_State* L) {

    UObject* obj = GetUObject(L, 1);
    int32_t want = static_cast<int32_t>(luaL_checkinteger(L, 2));
    if (!obj) { lua_pushboolean(L, 0); return 1; }
    uint32_t flags = ReadObjectFlags(obj);
    bool any = false;
    if (want & (1 << 25)) any |= (flags & 0x00000004) != 0;
    if (want & (1 << 29)) any |= (flags & 0x00000100) != 0;
    if (want & (1 << 30)) any |= (flags & 0x00000080) != 0;
    if (want & (1 << 28)) any |= (flags & (0x00008000 | 0x00010000)) != 0;
    lua_pushboolean(L, any ? 1 : 0);
    return 1;
}

int UObj_IsClass(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    if (!obj) { lua_pushboolean(L, 0); return 1; }

    lua_pushboolean(L, ConsoleEnabler::IsClassObject(g_State, obj) ? 1 : 0);
    return 1;
}

int UObj_GetPropertyByName(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    const char* name = luaL_checkstring(L, 2);
    if (!obj || !name) { lua_pushnil(L); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    if (!cls) { lua_pushnil(L); return 1; }
    FoundPropertyEx prop = FindPropertyOnClassEx(cls, name);
    if (!prop.Found) { lua_pushnil(L); return 1; }
    return PushPropertyValueEx(L, reinterpret_cast<uint8_t*>(obj), prop);
}

int UObj_SetPropertyByName(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    const char* name = luaL_checkstring(L, 2);
    if (!obj || !name) { lua_pushboolean(L, 0); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    if (!cls) { lua_pushboolean(L, 0); return 1; }
    FoundPropertyEx prop = FindPropertyOnClassEx(cls, name);
    if (!prop.Found) { lua_pushboolean(L, 0); return 1; }
    bool ok = SetPropertyValueEx(L, reinterpret_cast<uint8_t*>(obj), prop, 3);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int UObj_Reflection(lua_State* L) {
    UObject* obj = GetUObject(L, 1);
    if (!obj) { lua_pushnil(L); return 1; }
    UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
    if (!cls) { lua_pushnil(L); return 1; }

    lua_newtable(L);

    lua_pushvalue(L, 1);
    lua_pushcclosure(L, [](lua_State* L) -> int {

        auto* holder = static_cast<LuaUObjectData*>(lua_touserdata(L, lua_upvalueindex(1)));
        UObject* bound = holder ? holder->Object : nullptr;

        const char* name = nullptr;
        if (lua_isstring(L, 1)) name = lua_tostring(L, 1);
        else if (lua_istable(L, 1) && lua_isstring(L, 2)) name = lua_tostring(L, 2);
        if (!bound || !name) { lua_pushnil(L); return 1; }
        if (!SafeMem_IsReadable(bound, 32)) { lua_pushnil(L); return 1; }

        UClass* owner = EngineDiscovery::GetObjectClass(g_State, bound);
        if (CompatIsStructFamilyObject(bound)) owner = static_cast<UClass*>(bound);
        if (!owner) { lua_pushnil(L); return 1; }
        FoundPropertyEx prop = FindPropertyOnClassEx(owner, name);
        if (!prop.Found || !prop.Prop) { lua_pushnil(L); return 1; }
        CompatPushPropertyWrapper(L, prop.Prop, owner);
        return 1;
    }, 1);
    lua_setfield(L, -2, "GetProperty");

    UStruct* cur = CompatIsStructFamilyObject(obj) ? static_cast<UStruct*>(obj)
                                                  : static_cast<UStruct*>(cls);
    while (cur) {
        if (g_State.UStruct_ChildPropsOff >= 0) {
            void* child = EngineDiscovery::GetChildPropsHead(g_State, cur);
            while (child) {
                std::string name = EngineDiscovery::GetFFieldName(g_State, child);
                std::string typeName = EngineDiscovery::GetFFieldTypeName(g_State, child);
                if (!name.empty() && lua_getfield(L, -1, name.c_str()) == LUA_TNIL) {
                    lua_pop(L, 1);
                    lua_newtable(L);
                    lua_pushstring(L, typeName.c_str()); lua_setfield(L, -2, "Type");
                    lua_pushstring(L, name.c_str());     lua_setfield(L, -2, "Name");
                    lua_setfield(L, -2, name.c_str());
                } else {
                    lua_pop(L, 1);
                }
                child = EngineDiscovery::GetFFieldNext(g_State, child);
            }
        }
        cur = EngineDiscovery::GetSuperStruct(g_State, cur);
    }
    return 1;
}

int Compat_ForEachUObjectIndexed(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    int count = EngineDiscovery::GetObjectCount(g_State);
    for (int i = 0; i < count; ++i) {
        UObject* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
        if (!obj) continue;
        lua_pushvalue(L, 1);
        PushUObject(L, obj);
        lua_pushinteger(L, i / 65536);
        lua_pushinteger(L, i % 65536);
        if (lua_pcall(L, 3, 1, 0) == LUA_OK) {
            bool stop = lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);
            if (stop) break;
        } else {
            const char* err = lua_tostring(L, -1);
            char buf[512];
            snprintf(buf, sizeof(buf), "[Lua] ForEachUObject callback error: %s", err ? err : "unknown");
            EngineDiscovery::WriteLogLine(buf);
            lua_pop(L, 1);
        }
    }
    return 0;
}

static bool IsShortPackageName(const std::string& name) {

    return !name.empty() && name[0] != '/';
}

static int FPackageName_IsShortPackageName(lua_State* L) {
    const char* s = luaL_checkstring(L, 1);
    lua_pushboolean(L, IsShortPackageName(s) ? 1 : 0);
    return 1;
}

static int FPackageName_IsValidLongPackageName(lua_State* L) {
    const char* s = luaL_checkstring(L, 1);
    bool valid = s && s[0] == '/' && strchr(s, '/') != nullptr;
    lua_pushboolean(L, valid ? 1 : 0);
    return 1;
}

static int Compat_GetCurrentThreadId(lua_State* L) {
    lua_pushinteger(L, LuaHookManager::Get().GetCurrentThreadId());
    return 1;
}
static int Compat_GetMainModThreadId(lua_State* L) {
    lua_pushinteger(L, LuaHookManager::Get().GetMainModThreadId());
    return 1;
}
static int Compat_GetGameThreadId(lua_State* L) {
    lua_pushinteger(L, LuaHookManager::Get().GetGameThreadId());
    return 1;
}
static int Compat_GetAsyncThreadId(lua_State* L) {
    lua_pushinteger(L, LuaHookManager::Get().GetAsyncThreadId());
    return 1;
}
static int Compat_IsInGameThread(lua_State* L) {
    lua_pushboolean(L, LuaHookManager::Get().IsInGameThread() ? 1 : 0);
    return 1;
}
static int Compat_IsInMainModThread(lua_State* L) {
    lua_pushboolean(L, LuaHookManager::Get().IsInMainModThread() ? 1 : 0);
    return 1;
}
static int Compat_IsInAsyncThread(lua_State* L) {
    lua_pushboolean(L, LuaHookManager::Get().IsInAsyncThread() ? 1 : 0);
    return 1;
}

void RegisterCompatApi(lua_State* L) {

    lua_newtable(L);
    lua_pushinteger(L, g_State.UEMajor);      lua_setfield(L, -2, "Major");
    lua_pushinteger(L, g_State.UEMinor);      lua_setfield(L, -2, "Minor");
    lua_pushcfunction(L, Version_GetMajor);   lua_setfield(L, -2, "GetMajor");
    lua_pushcfunction(L, Version_GetMinor);   lua_setfield(L, -2, "GetMinor");
    lua_pushcfunction(L, Version_IsEqual);    lua_setfield(L, -2, "IsEqual");
    lua_pushcfunction(L, Version_IsAtLeast);  lua_setfield(L, -2, "IsAtLeast");
    lua_pushcfunction(L, Version_IsAtMost);   lua_setfield(L, -2, "IsAtMost");
    lua_pushcfunction(L, Version_IsBelow);    lua_setfield(L, -2, "IsBelow");
    lua_pushcfunction(L, Version_IsAbove);    lua_setfield(L, -2, "IsAbove");
    lua_setglobal(L, "UnrealVersion");

    lua_pushcfunction(L, FName_Constructor);
    lua_setglobal(L, "FName");
    PushFName(L, 0);
    lua_setglobal(L, "NAME_None");

    lua_pushcfunction(L, FText_Constructor);
    lua_setglobal(L, "FText");
    lua_pushcfunction(L, FString_Constructor);
    lua_setglobal(L, "FString");
    lua_pushcfunction(L, FString_Constructor);
    lua_setglobal(L, "FUtf8String");
    lua_pushcfunction(L, FString_Constructor);
    lua_setglobal(L, "FAnsiString");

    lua_newtable(L);
    lua_pushcfunction(L, FPackageName_IsShortPackageName);   lua_setfield(L, -2, "IsShortPackageName");
    lua_pushcfunction(L, FPackageName_IsValidLongPackageName); lua_setfield(L, -2, "IsValidLongPackageName");
    lua_setglobal(L, "FPackageName");

    RegisterKeyTables(L);
    RegisterFlagTables(L);
    RegisterPropertyTypes(L);

    lua_pushcfunction(L, Compat_GetCurrentThreadId);   lua_setglobal(L, "GetCurrentThreadId");
    lua_pushcfunction(L, Compat_GetMainModThreadId);   lua_setglobal(L, "GetMainModThreadId");
    lua_pushcfunction(L, Compat_GetGameThreadId);      lua_setglobal(L, "GetGameThreadId");
    lua_pushcfunction(L, Compat_GetAsyncThreadId);     lua_setglobal(L, "GetAsyncThreadId");
    lua_pushcfunction(L, Compat_IsInGameThread);       lua_setglobal(L, "IsInGameThread");
    lua_pushcfunction(L, Compat_IsInMainModThread);    lua_setglobal(L, "IsInMainModThread");
    lua_pushcfunction(L, Compat_IsInAsyncThread);      lua_setglobal(L, "IsInAsyncThread");

    lua_pushboolean(L, 1); lua_setglobal(L, "EngineTickAvailable");
    lua_pushboolean(L, 1); lua_setglobal(L, "ProcessEventAvailable");

    lua_newtable(L);
    lua_pushinteger(L, 3); lua_setfield(L, -2, "Major");
    lua_pushinteger(L, 0); lua_setfield(L, -2, "Minor");
    lua_pushinteger(L, 1); lua_setfield(L, -2, "Hotfix");
    lua_pushcfunction(L, [](lua_State* L2) -> int {
        lua_pushinteger(L2, 3); lua_pushinteger(L2, 0); lua_pushinteger(L2, 1);
        return 3;
    });
    lua_setfield(L, -2, "GetVersion");
    lua_pushinteger(L, COMPAT_API_VERSION_MAJOR); lua_setfield(L, -2, "CompatMajor");
    lua_pushinteger(L, COMPAT_API_VERSION_MINOR); lua_setfield(L, -2, "CompatMinor");
    lua_setglobal(L, "RuntimeVersion");

    PublishCompatTypeRecords(L);
}

}
