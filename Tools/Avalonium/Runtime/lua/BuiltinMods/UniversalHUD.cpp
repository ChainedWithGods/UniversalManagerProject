#include "UniversalHUD.h"
#include "../LuaHookManager.h"
#include "../../core/EngineDiscovery.h"
#include "../../core/Types.h"
#include "../../utils/SafeMemory.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <chrono>
#include <windows.h>

extern EngineState g_State;

namespace LuaEngine {
namespace BuiltinMods {
namespace UniversalHUD {

static std::mutex s_HUDMutex;
static bool s_HUDHidden = false;

enum class EUIClassType : uint8_t {
    Unknown = 0,
    None,
    UserWidget,
    HUD,
    WidgetComponent
};

struct ClassReflCache {
    std::unordered_map<UClass*, EUIClassType> classType;
    std::unordered_map<UClass*, void*>   setVisFunc;
    std::unordered_map<UClass*, void*>   setOpacityFunc;
    std::unordered_map<UClass*, int32_t> visibilityOff;
    std::unordered_map<UClass*, int32_t> opacityOff;

    struct HUDProps {
        int32_t showHUDOff = -2;
        uint8_t showHUDMask = 1;
        int32_t showOvOff  = -2;
        uint8_t showOvMask = 1;
    };
    std::unordered_map<UClass*, HUDProps> hudProps;

    void Clear() {
        classType.clear();
        setVisFunc.clear();
        setOpacityFunc.clear();
        visibilityOff.clear();
        opacityOff.clear();
        hudProps.clear();
    }
} s_ClassCache;

struct ActiveWidgetEntry {
    UObject* obj;
    UClass*  cls;
    uint8_t  origVis;
    float    origOpacity;
    int32_t  visOff;
    int32_t  opacityOff;
};

struct ActiveHUDEntry {
    UObject* obj;
    UClass*  cls;
    bool     origShowHUD;
    bool     origShowOvr;
};

struct ActiveCompEntry {
    UObject* obj;
    UClass*  cls;
    bool     origVis;
};

static std::vector<ActiveWidgetEntry> s_ActiveWidgets;
static std::vector<ActiveHUDEntry>    s_ActiveHUDs;
static std::vector<ActiveCompEntry>   s_ActiveComps;

static EUIClassType GetUIClassType(UClass* cls) {
    if (!cls) return EUIClassType::None;
    auto it = s_ClassCache.classType.find(cls);
    if (it != s_ClassCache.classType.end()) return it->second;

    EUIClassType resolved = EUIClassType::None;
    UStruct* cur = static_cast<UStruct*>(cls);
    for (int d = 0; d < 32 && cur; ++d) {
        std::string sName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cur));
        if (_stricmp(sName.c_str(), "UserWidget") == 0 || _stricmp(sName.c_str(), "Widget") == 0) {
            resolved = EUIClassType::UserWidget;
            break;
        }
        if (_stricmp(sName.c_str(), "HUD") == 0) {
            resolved = EUIClassType::HUD;
            break;
        }
        if (_stricmp(sName.c_str(), "WidgetComponent") == 0) {
            resolved = EUIClassType::WidgetComponent;
            break;
        }
        cur = EngineDiscovery::GetSuperStruct(g_State, cur);
    }
    s_ClassCache.classType[cls] = resolved;
    return resolved;
}

static bool FindBoolProp(UClass* cls, const char* propName, int32_t& outOff, uint8_t& outMask) {
    if (!cls || (g_State.UStruct_ChildPropsOff < 0 && g_State.UStruct_ChildrenOff < 0) || g_State.FProperty_OffsetOff < 0) return false;
    for (UStruct* cur = static_cast<UStruct*>(cls); cur;
         cur = EngineDiscovery::GetSuperStruct(g_State, cur)) {
        if (g_State.UStruct_ChildPropsOff >= 0) {
            for (void* c = EngineDiscovery::GetChildPropsHead(g_State, cur); c;
                 c = EngineDiscovery::GetFFieldNext(g_State, c)) {
                if (_stricmp(EngineDiscovery::GetFFieldName(g_State, c).c_str(), propName) == 0) {
                    int32_t off = -1;
                    SafeMem_Read(reinterpret_cast<uint8_t*>(c) + g_State.FProperty_OffsetOff, &off, 4);
                    if (off < 0) return false;
                    uint8_t byteOffset = 0;
                    outMask = 1;
                    if (g_State.FBoolProperty_BaseOff >= 0) {
                        SafeMem_Read(reinterpret_cast<uint8_t*>(c) + g_State.FBoolProperty_BaseOff + 1, &byteOffset, 1);
                        SafeMem_Read(reinterpret_cast<uint8_t*>(c) + g_State.FBoolProperty_BaseOff + 2, &outMask, 1);
                    }
                    if (!outMask) outMask = 1;
                    outOff = off + byteOffset;
                    return true;
                }
            }
        } else if (g_State.UStruct_ChildrenOff >= 0) {
            for (void* c = EngineDiscovery::GetChildrenHead(g_State, cur); c;
                 c = EngineDiscovery::GetFieldNext(g_State, c)) {
                if (_stricmp(EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(c)).c_str(), propName) == 0) {
                    int32_t off = -1;
                    SafeMem_Read(reinterpret_cast<uint8_t*>(c) + g_State.FProperty_OffsetOff, &off, 4);
                    if (off < 0) return false;
                    uint8_t byteOffset = 0;
                    outMask = 1;
                    if (g_State.FBoolProperty_BaseOff >= 0) {
                        SafeMem_Read(reinterpret_cast<uint8_t*>(c) + g_State.FBoolProperty_BaseOff + 1, &byteOffset, 1);
                        SafeMem_Read(reinterpret_cast<uint8_t*>(c) + g_State.FBoolProperty_BaseOff + 2, &outMask, 1);
                    }
                    if (!outMask) outMask = 1;
                    outOff = off + byteOffset;
                    return true;
                }
            }
        }
    }
    return false;
}

static int32_t GetVisibilityOff(UClass* cls) {
    auto it = s_ClassCache.visibilityOff.find(cls);
    if (it != s_ClassCache.visibilityOff.end()) return it->second;
    if (!cls || g_State.UStruct_ChildPropsOff < 0 || g_State.FProperty_OffsetOff < 0)
        return (s_ClassCache.visibilityOff[cls] = -1);
    for (UStruct* cur = static_cast<UStruct*>(cls); cur;
         cur = EngineDiscovery::GetSuperStruct(g_State, cur)) {
        for (void* c = EngineDiscovery::GetChildPropsHead(g_State, cur); c;
             c = EngineDiscovery::GetFFieldNext(g_State, c)) {
            if (_stricmp(EngineDiscovery::GetFFieldName(g_State, c).c_str(), "Visibility") == 0) {
                int32_t off = -1;
                SafeMem_Read(reinterpret_cast<uint8_t*>(c) + g_State.FProperty_OffsetOff, &off, 4);
                return (s_ClassCache.visibilityOff[cls] = off);
            }
        }
    }
    return (s_ClassCache.visibilityOff[cls] = -1);
}

static int32_t GetRenderOpacityOff(UClass* cls) {
    auto it = s_ClassCache.opacityOff.find(cls);
    if (it != s_ClassCache.opacityOff.end()) return it->second;
    if (!cls || g_State.UStruct_ChildPropsOff < 0 || g_State.FProperty_OffsetOff < 0)
        return (s_ClassCache.opacityOff[cls] = -1);
    for (UStruct* cur = static_cast<UStruct*>(cls); cur;
         cur = EngineDiscovery::GetSuperStruct(g_State, cur)) {
        for (void* c = EngineDiscovery::GetChildPropsHead(g_State, cur); c;
             c = EngineDiscovery::GetFFieldNext(g_State, c)) {
            if (_stricmp(EngineDiscovery::GetFFieldName(g_State, c).c_str(), "RenderOpacity") == 0) {
                int32_t off = -1;
                SafeMem_Read(reinterpret_cast<uint8_t*>(c) + g_State.FProperty_OffsetOff, &off, 4);
                return (s_ClassCache.opacityOff[cls] = off);
            }
        }
    }
    return (s_ClassCache.opacityOff[cls] = -1);
}

static void* GetSetVisFunc(UClass* cls) {
    auto it = s_ClassCache.setVisFunc.find(cls);
    if (it != s_ClassCache.setVisFunc.end()) return it->second;
    void* fn = EngineDiscovery::FindFunctionByName(g_State, static_cast<UStruct*>(cls), "SetVisibility");
    return (s_ClassCache.setVisFunc[cls] = fn);
}

static void* GetSetOpacityFunc(UClass* cls) {
    auto it = s_ClassCache.setOpacityFunc.find(cls);
    if (it != s_ClassCache.setOpacityFunc.end()) return it->second;
    void* fn = EngineDiscovery::FindFunctionByName(g_State, static_cast<UStruct*>(cls), "SetRenderOpacity");
    return (s_ClassCache.setOpacityFunc[cls] = fn);
}

static void SafeWrite(void* addr, const void* src, size_t n) {
    DWORD old = 0;
    if (VirtualProtect(addr, n, PAGE_READWRITE, &old)) { memcpy(addr, src, n); VirtualProtect(addr, n, old, &old); }
}
static bool  ReadBoolBit (uint8_t* p, uint8_t m) { uint8_t v=0; SafeMem_Read(p,&v,1); return (v&m)!=0; }
static void  WriteBoolBit(uint8_t* p, uint8_t m, bool val) {
    uint8_t v=0; SafeMem_Read(p,&v,1); if(val) v|=m; else v&=~m; SafeWrite(p,&v,1);
}
static void SetVisibility(UObject* w, UClass* cls, uint8_t vis) {
    void* fn = GetSetVisFunc(cls);
    if (fn) {
        uint8_t params[4]={vis,0,0,0};
        EngineDiscovery::CallProcessEvent(g_State, w, fn, params);
    }
}
static void SetRenderOpacity(UObject* w, UClass* cls, float opacity) {
    void* fn = GetSetOpacityFunc(cls);
    if (fn) {
        uint8_t params[8]={};
        memcpy(params, &opacity, sizeof(float));
        EngineDiscovery::CallProcessEvent(g_State, w, fn, params);
    }
}
static void SetSceneVisibility(UObject* c, UClass* cls, bool vis) {
    void* fn = GetSetVisFunc(cls); if (!fn) return;
    uint8_t params[8]={}; params[0]=vis?1:0;
    EngineDiscovery::CallProcessEvent(g_State, c, fn, params);
}

static inline bool IsValidLiveObject(UObject* obj) {
    if (!obj || !SafeMem_IsReadable(obj, 24)) return false;
    uint32_t flags = 0;
    if (g_State.UObject_FlagsOff >= 0) {
        SafeMem_Read(reinterpret_cast<uint8_t*>(obj) + g_State.UObject_FlagsOff, &flags, 4);
        if (flags & 0x00000310) return false;
    }
    return true;
}

struct FastMemScope {
    FastMemScope()  { SafeMem_SetFastMode(1); }
    ~FastMemScope() { SafeMem_SetFastMode(0); }
};

bool ToggleAllHUD(bool hide) {
    auto tStart = std::chrono::high_resolution_clock::now();

    FastMemScope fastScope;
    std::lock_guard<std::mutex> lock(s_HUDMutex);
    s_HUDHidden = hide;

    if (!g_State.GUObjectArray) return false;

    int updatedCount = 0;

    if (hide) {
        s_ActiveWidgets.clear();
        s_ActiveHUDs.clear();
        s_ActiveComps.clear();

        const int totalCount = EngineDiscovery::GetObjectCount(g_State);

        for (int i = 0; i < totalCount; ++i) {
            void* raw = EngineDiscovery::GetObjectByIndex(g_State, i);
            if (!raw) continue;

            UObject* obj = static_cast<UObject*>(raw);
            if (!IsValidLiveObject(obj)) continue;

            UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
            if (!cls) continue;

            EUIClassType uiType = GetUIClassType(cls);
            if (uiType == EUIClassType::None) continue;

            if (uiType == EUIClassType::UserWidget) {
                int32_t visOff = GetVisibilityOff(cls);
                int32_t opOff = GetRenderOpacityOff(cls);
                uint8_t curVis = 0;
                float curOpacity = 1.0f;
                if (visOff >= 0) SafeMem_Read(reinterpret_cast<uint8_t*>(obj) + visOff, &curVis, 1);
                if (opOff >= 0) SafeMem_Read(reinterpret_cast<uint8_t*>(obj) + opOff, &curOpacity, sizeof(float));

                s_ActiveWidgets.push_back({ obj, cls, curVis, curOpacity, visOff, opOff });

                if (curVis == 0 || curVis == 3 || curVis == 4 || curOpacity > 0.01f) {
                    SetVisibility(obj, cls, 1);
                    SetRenderOpacity(obj, cls, 0.0f);
                    if (visOff >= 0) {
                        uint8_t collapsed = 1;
                        SafeWrite(reinterpret_cast<uint8_t*>(obj) + visOff, &collapsed, 1);
                    }
                    if (opOff >= 0) {
                        float zero = 0.0f;
                        SafeWrite(reinterpret_cast<uint8_t*>(obj) + opOff, &zero, sizeof(float));
                    }
                    ++updatedCount;
                }
                continue;
            }

            if (uiType == EUIClassType::HUD) {
                auto& hp = s_ClassCache.hudProps[cls];
                if (hp.showHUDOff == -2) {
                    FindBoolProp(cls, "bShowHUD",      hp.showHUDOff, hp.showHUDMask);
                    FindBoolProp(cls, "bShowOverlays", hp.showOvOff,  hp.showOvMask);
                }

                bool origHUD = true;
                bool origOvr = true;
                if (hp.showHUDOff >= 0) {
                    uint8_t* p = reinterpret_cast<uint8_t*>(obj) + hp.showHUDOff;
                    origHUD = ReadBoolBit(p, hp.showHUDMask);
                    WriteBoolBit(p, hp.showHUDMask, false);
                }
                if (hp.showOvOff >= 0) {
                    uint8_t* p = reinterpret_cast<uint8_t*>(obj) + hp.showOvOff;
                    origOvr = ReadBoolBit(p, hp.showOvMask);
                    WriteBoolBit(p, hp.showOvMask, false);
                }
                s_ActiveHUDs.push_back({ obj, cls, origHUD, origOvr });
                ++updatedCount;
                continue;
            }

            if (uiType == EUIClassType::WidgetComponent) {
                s_ActiveComps.push_back({ obj, cls, true });
                SetSceneVisibility(obj, cls, false);
                ++updatedCount;
            }
        }
    } else {
        for (auto& e : s_ActiveHUDs) {
            if (!IsValidLiveObject(e.obj)) continue;
            auto& hp = s_ClassCache.hudProps[e.cls];
            if (hp.showHUDOff >= 0) {
                uint8_t* p = reinterpret_cast<uint8_t*>(e.obj) + hp.showHUDOff;
                WriteBoolBit(p, hp.showHUDMask, e.origShowHUD);
            }
            if (hp.showOvOff >= 0) {
                uint8_t* p = reinterpret_cast<uint8_t*>(e.obj) + hp.showOvOff;
                WriteBoolBit(p, hp.showOvMask, e.origShowOvr);
            }
            ++updatedCount;
        }

        for (auto& e : s_ActiveWidgets) {
            if (!IsValidLiveObject(e.obj)) continue;
            SetVisibility(e.obj, e.cls, e.origVis);
            SetRenderOpacity(e.obj, e.cls, e.origOpacity);
            if (e.visOff >= 0) {
                SafeWrite(reinterpret_cast<uint8_t*>(e.obj) + e.visOff, &e.origVis, 1);
            }
            if (e.opacityOff >= 0) {
                SafeWrite(reinterpret_cast<uint8_t*>(e.obj) + e.opacityOff, &e.origOpacity, sizeof(float));
            }
            ++updatedCount;
        }

        for (auto& e : s_ActiveComps) {
            if (!IsValidLiveObject(e.obj)) continue;
            SetSceneVisibility(e.obj, e.cls, e.origVis);
            ++updatedCount;
        }

        s_ActiveWidgets.clear();
        s_ActiveHUDs.clear();
        s_ActiveComps.clear();
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    char logBuf[256];
    snprintf(logBuf, sizeof(logBuf),
        "[UniversalHUD] ToggleAllHUD(%s): %d objects processed in %.2fms\n",
        hide ? "hide" : "show", updatedCount, ms);
    EngineDiscovery::WriteLogLine(logBuf);
    EngineDiscovery::WriteLogLine(logBuf);

    return true;
}

bool IsHUDHidden() {
    std::lock_guard<std::mutex> lock(s_HUDMutex);
    return s_HUDHidden;
}

static int Lua_ToggleHUD(lua_State* L) {
    bool newState = (lua_gettop(L) >= 1 && lua_isboolean(L, 1))
                    ? lua_toboolean(L, 1) != 0 : !IsHUDHidden();
    bool ok = ToggleAllHUD(newState);
    lua_pushboolean(L, ok && newState ? 1 : 0);
    return 1;
}
static int Lua_HideAllHUD(lua_State* L) { lua_pushboolean(L, ToggleAllHUD(true)  ? 1 : 0); return 1; }
static int Lua_ShowAllHUD(lua_State* L) { lua_pushboolean(L, ToggleAllHUD(false) ? 1 : 0); return 1; }
static int Lua_IsHUDHidden(lua_State* L) { lua_pushboolean(L, IsHUDHidden() ? 1 : 0); return 1; }

static int Lua_RefreshHUDCache(lua_State* L) {
    std::lock_guard<std::mutex> lock(s_HUDMutex);
    s_ClassCache.Clear();
    s_ActiveWidgets.clear();
    s_ActiveHUDs.clear();
    s_ActiveComps.clear();
    EngineDiscovery::WriteLogLine("[UniversalHUD] HUD cache cleared\n");
    lua_pushboolean(L, 1);
    return 1;
}

void Register(lua_State* L) {
    if (!L) return;
    lua_register(L, "ToggleHUD",         Lua_ToggleHUD);
    lua_register(L, "HideAllHUD",         Lua_HideAllHUD);
    lua_register(L, "ShowAllHUD",         Lua_ShowAllHUD);
    lua_register(L, "IsHUDHidden",        Lua_IsHUDHidden);
    lua_register(L, "RefreshHUDCache",    Lua_RefreshHUDCache);
    EngineDiscovery::WriteLogLine("[UniversalHUD] Registered: ToggleHUD, HideAllHUD, ShowAllHUD, IsHUDHidden, RefreshHUDCache\n");
}

}
}
}
