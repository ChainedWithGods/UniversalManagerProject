#include "LuaTypes.h"
#include "LuaUObject.h"
#include "LuaCompatApi.h"
#include "LuaManager.h"
#include "../utils/SafeMemory.h"
#include "../core/EngineDiscovery.h"
#include "../console/ConsoleInternal.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <cmath>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <string>
#include <algorithm>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <unordered_set>

extern EngineState g_State;

namespace LuaEngine {

static const char* VECTOR_MT     = "Avalonium.FVector";
static const char* ROTATOR_MT    = "Avalonium.FRotator";
static const char* TRANSFORM_MT  = "Avalonium.FTransform";
static const char* COLOR_MT      = "Avalonium.FLinearColor";
static const char* TARRAY_MT     = "Avalonium.TArray";
static const char* TMAP_MT       = "Avalonium.TMap";
static const char* TSET_MT       = "Avalonium.TSet";
static const char* DELEGATE_MT   = "Avalonium.FDelegate";
static const char* SOFTOBJECT_MT = "Avalonium.FSoftObjectPath";

LuaCallFrame* CreateCallFrame(std::vector<uint64_t>&& block) {
    auto* frame = new LuaCallFrame();
    frame->Block = std::move(block);
    return frame;
}

void PushVector(lua_State* L, float x, float y, float z);
void PushRotator(lua_State* L, float pitch, float yaw, float roll);
void PushTransform(lua_State* L, const Transform3D& transform);
void PushColor(lua_State* L, float r, float g, float b, float a);
bool GetVector(lua_State* L, int index, Vector3D& outVec);
bool GetRotator(lua_State* L, int index, Rotator3D& outRot);
bool GetColor(lua_State* L, int index, LinearColor4D& outCol);

static int VecLen_Impl(lua_State* L) {
    auto* v = static_cast<Vector3D*>(luaL_checkudata(L, 1, VECTOR_MT));
    lua_pushnumber(L, std::sqrt(v->X * v->X + v->Y * v->Y + v->Z * v->Z));
    return 1;
}

static int VecDist_Impl(lua_State* L) {
    auto* v1 = static_cast<Vector3D*>(luaL_checkudata(L, 1, VECTOR_MT));
    Vector3D v2;
    if (!GetVector(L, 2, v2)) {
        return luaL_error(L, "Distance expects FVector as argument");
    }
    float dx = v1->X - v2.X, dy = v1->Y - v2.Y, dz = v1->Z - v2.Z;
    lua_pushnumber(L, std::sqrt(dx * dx + dy * dy + dz * dz));
    return 1;
}

static int VecNorm_Impl(lua_State* L) {
    auto* v = static_cast<Vector3D*>(luaL_checkudata(L, 1, VECTOR_MT));
    float len = std::sqrt(v->X * v->X + v->Y * v->Y + v->Z * v->Z);
    if (len > 0.0001f) { v->X /= len; v->Y /= len; v->Z /= len; }
    lua_pushvalue(L, 1);
    return 1;
}

static int VecLen2D_Impl(lua_State* L) {
    auto* v = static_cast<Vector3D*>(luaL_checkudata(L, 1, VECTOR_MT));
    lua_pushnumber(L, std::sqrt(v->X * v->X + v->Y * v->Y));
    return 1;
}

static int VecLenSq_Impl(lua_State* L) {
    auto* v = static_cast<Vector3D*>(luaL_checkudata(L, 1, VECTOR_MT));
    lua_pushnumber(L, v->X * v->X + v->Y * v->Y + v->Z * v->Z);
    return 1;
}

static int VecDot_Impl(lua_State* L) {
    auto* v1 = static_cast<Vector3D*>(luaL_checkudata(L, 1, VECTOR_MT));
    Vector3D v2;
    if (!GetVector(L, 2, v2)) return luaL_error(L, "Dot expects FVector as argument");
    lua_pushnumber(L, v1->X * v2.X + v1->Y * v2.Y + v1->Z * v2.Z);
    return 1;
}

static int VecCross_Impl(lua_State* L) {
    auto* v1 = static_cast<Vector3D*>(luaL_checkudata(L, 1, VECTOR_MT));
    Vector3D v2;
    if (!GetVector(L, 2, v2)) return luaL_error(L, "Cross expects FVector as argument");
    PushVector(L,
        v1->Y * v2.Z - v1->Z * v2.Y,
        v1->Z * v2.X - v1->X * v2.Z,
        v1->X * v2.Y - v1->Y * v2.X);
    return 1;
}

static int VecToRot_Impl(lua_State* L) {
    auto* v = static_cast<Vector3D*>(luaL_checkudata(L, 1, VECTOR_MT));
    constexpr float RAD_TO_DEG = 180.0f / 3.1415926535f;
    float yaw = std::atan2(v->Y, v->X) * RAD_TO_DEG;
    float pitch = std::atan2(v->Z, std::sqrt(v->X * v->X + v->Y * v->Y)) * RAD_TO_DEG;
    PushRotator(L, pitch, yaw, 0.0f);
    return 1;
}

static int VecToTable_Impl(lua_State* L) {
    auto* v = static_cast<Vector3D*>(luaL_checkudata(L, 1, VECTOR_MT));
    lua_newtable(L);
    lua_pushnumber(L, v->X); lua_setfield(L, -2, "X");
    lua_pushnumber(L, v->Y); lua_setfield(L, -2, "Y");
    lua_pushnumber(L, v->Z); lua_setfield(L, -2, "Z");
    return 1;
}

static int VecClone_Impl(lua_State* L) {
    auto* v = static_cast<Vector3D*>(luaL_checkudata(L, 1, VECTOR_MT));
    PushVector(L, v->X, v->Y, v->Z);
    return 1;
}

static int Vector_Index(lua_State* L) {
    auto* vec = static_cast<Vector3D*>(luaL_checkudata(L, 1, VECTOR_MT));
    if (lua_isstring(L, 2)) {
        const char* key = lua_tostring(L, 2);
        if (_stricmp(key, "X") == 0 || _stricmp(key, "Pitch") == 0) { lua_pushnumber(L, vec->X); return 1; }
        if (_stricmp(key, "Y") == 0 || _stricmp(key, "Yaw") == 0)   { lua_pushnumber(L, vec->Y); return 1; }
        if (_stricmp(key, "Z") == 0 || _stricmp(key, "Roll") == 0)  { lua_pushnumber(L, vec->Z); return 1; }
        if (_stricmp(key, "Length") == 0 || _stricmp(key, "Size") == 0) {
            lua_pushcfunction(L, VecLen_Impl);  return 1;
        }
        if (_stricmp(key, "Length2D") == 0 || _stricmp(key, "Size2D") == 0) {
            lua_pushcfunction(L, VecLen2D_Impl); return 1;
        }
        if (_stricmp(key, "LengthSquared") == 0 || _stricmp(key, "SizeSquared") == 0) {
            lua_pushcfunction(L, VecLenSq_Impl); return 1;
        }
        if (_stricmp(key, "Distance") == 0) {
            lua_pushcfunction(L, VecDist_Impl); return 1;
        }
        if (_stricmp(key, "Normalize") == 0) {
            lua_pushcfunction(L, VecNorm_Impl); return 1;
        }
        if (_stricmp(key, "Dot") == 0) {
            lua_pushcfunction(L, VecDot_Impl); return 1;
        }
        if (_stricmp(key, "Cross") == 0) {
            lua_pushcfunction(L, VecCross_Impl); return 1;
        }
        if (_stricmp(key, "ToRotator") == 0 || _stricmp(key, "Rotation") == 0) {
            lua_pushcfunction(L, VecToRot_Impl); return 1;
        }
        if (_stricmp(key, "ToTable") == 0) {
            lua_pushcfunction(L, VecToTable_Impl); return 1;
        }
        if (_stricmp(key, "Clone") == 0) {
            lua_pushcfunction(L, VecClone_Impl); return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int Vector_NewIndex(lua_State* L) {
    auto* vec = static_cast<Vector3D*>(luaL_checkudata(L, 1, VECTOR_MT));
    const char* key = luaL_checkstring(L, 2);
    float val = static_cast<float>(luaL_checknumber(L, 3));
    if      (_stricmp(key, "X") == 0 || _stricmp(key, "Pitch") == 0) vec->X = val;
    else if (_stricmp(key, "Y") == 0 || _stricmp(key, "Yaw")   == 0) vec->Y = val;
    else if (_stricmp(key, "Z") == 0 || _stricmp(key, "Roll")  == 0) vec->Z = val;
    return 0;
}

static int Vector_Add(lua_State* L) {
    Vector3D v1, v2;
    if (GetVector(L, 1, v1) && GetVector(L, 2, v2)) {
        PushVector(L, v1.X + v2.X, v1.Y + v2.Y, v1.Z + v2.Z);
        return 1;
    }
    return luaL_error(L, "Attempt to add invalid vector types");
}

static int Vector_Sub(lua_State* L) {
    Vector3D v1, v2;
    if (GetVector(L, 1, v1) && GetVector(L, 2, v2)) {
        PushVector(L, v1.X - v2.X, v1.Y - v2.Y, v1.Z - v2.Z);
        return 1;
    }
    return luaL_error(L, "Attempt to subtract invalid vector types");
}

static int Vector_Mul(lua_State* L) {
    Vector3D v;
    if (GetVector(L, 1, v) && lua_isnumber(L, 2)) {
        float s = static_cast<float>(lua_tonumber(L, 2));
        PushVector(L, v.X * s, v.Y * s, v.Z * s);
        return 1;
    } else if (lua_isnumber(L, 1) && GetVector(L, 2, v)) {
        float s = static_cast<float>(lua_tonumber(L, 1));
        PushVector(L, v.X * s, v.Y * s, v.Z * s);
        return 1;
    }
    return luaL_error(L, "Vector multiplication requires a vector and number");
}

static int Vector_Div(lua_State* L) {
    Vector3D v;
    if (GetVector(L, 1, v) && lua_isnumber(L, 2)) {
        float s = static_cast<float>(lua_tonumber(L, 2));
        if (std::abs(s) > 0.00001f) {
            PushVector(L, v.X / s, v.Y / s, v.Z / s);
            return 1;
        }
    }
    return luaL_error(L, "Invalid vector division");
}

static int Vector_Unm(lua_State* L) {
    Vector3D v;
    if (GetVector(L, 1, v)) {
        PushVector(L, -v.X, -v.Y, -v.Z);
        return 1;
    }
    return luaL_error(L, "Invalid vector unary minus");
}

static int Vector_Eq(lua_State* L) {
    Vector3D v1, v2;
    if (GetVector(L, 1, v1) && GetVector(L, 2, v2)) {
        bool eq = std::abs(v1.X - v2.X) < 0.001f &&
                  std::abs(v1.Y - v2.Y) < 0.001f &&
                  std::abs(v1.Z - v2.Z) < 0.001f;
        lua_pushboolean(L, eq ? 1 : 0);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int Vector_ToString(lua_State* L) {
    auto* vec = static_cast<Vector3D*>(luaL_checkudata(L, 1, VECTOR_MT));
    char buf[128];
    snprintf(buf, sizeof(buf), "FVector(X=%.2f, Y=%.2f, Z=%.2f)", vec->X, vec->Y, vec->Z);
    lua_pushstring(L, buf);
    return 1;
}

static int RotForward_Impl(lua_State* L) {
    auto* r = static_cast<Rotator3D*>(luaL_checkudata(L, 1, ROTATOR_MT));
    constexpr float DEG_TO_RAD = 3.1415926535f / 180.0f;
    float p  = r->Pitch * DEG_TO_RAD;
    float y  = r->Yaw   * DEG_TO_RAD;
    float cp = std::cos(p), sp = std::sin(p);
    float cy = std::cos(y), sy = std::sin(y);
    PushVector(L, cp * cy, cp * sy, sp);
    return 1;
}

static int RotRight_Impl(lua_State* L) {
    auto* r = static_cast<Rotator3D*>(luaL_checkudata(L, 1, ROTATOR_MT));
    constexpr float DEG_TO_RAD = 3.1415926535f / 180.0f;
    float p  = r->Pitch * DEG_TO_RAD;
    float y  = r->Yaw   * DEG_TO_RAD;
    float roll = r->Roll * DEG_TO_RAD;
    float cp = std::cos(p), sp = std::sin(p);
    float cy = std::cos(y), sy = std::sin(y);
    float cr = std::cos(roll), sr = std::sin(roll);
    PushVector(L,
        cy * sp * sr - sy * cr,
        sy * sp * sr + cy * cr,
        -cp * sr);
    return 1;
}

static int RotUp_Impl(lua_State* L) {
    auto* r = static_cast<Rotator3D*>(luaL_checkudata(L, 1, ROTATOR_MT));
    constexpr float DEG_TO_RAD = 3.1415926535f / 180.0f;
    float p  = r->Pitch * DEG_TO_RAD;
    float y  = r->Yaw   * DEG_TO_RAD;
    float roll = r->Roll * DEG_TO_RAD;
    float cp = std::cos(p), sp = std::sin(p);
    float cy = std::cos(y), sy = std::sin(y);
    float cr = std::cos(roll), sr = std::sin(roll);
    PushVector(L,
        -cy * sp * cr - sy * sr,
        -sy * sp * cr + cy * sr,
        cp * cr);
    return 1;
}

static int RotNorm_Impl(lua_State* L) {
    auto* r = static_cast<Rotator3D*>(luaL_checkudata(L, 1, ROTATOR_MT));
    auto normalizeAxis = [](float angle) {
        angle = std::fmod(angle, 360.0f);
        if (angle > 180.0f) angle -= 360.0f;
        else if (angle < -180.0f) angle += 360.0f;
        return angle;
    };
    PushRotator(L, normalizeAxis(r->Pitch), normalizeAxis(r->Yaw), normalizeAxis(r->Roll));
    return 1;
}

static int RotClamp_Impl(lua_State* L) {
    auto* r = static_cast<Rotator3D*>(luaL_checkudata(L, 1, ROTATOR_MT));
    auto clampAxis = [](float angle) {
        angle = std::fmod(angle, 360.0f);
        if (angle < 0.0f) angle += 360.0f;
        return angle;
    };
    PushRotator(L, clampAxis(r->Pitch), clampAxis(r->Yaw), clampAxis(r->Roll));
    return 1;
}

static int RotToTable_Impl(lua_State* L) {
    auto* r = static_cast<Rotator3D*>(luaL_checkudata(L, 1, ROTATOR_MT));
    lua_newtable(L);
    lua_pushnumber(L, r->Pitch); lua_setfield(L, -2, "Pitch");
    lua_pushnumber(L, r->Yaw);   lua_setfield(L, -2, "Yaw");
    lua_pushnumber(L, r->Roll);  lua_setfield(L, -2, "Roll");
    return 1;
}

static int RotClone_Impl(lua_State* L) {
    auto* r = static_cast<Rotator3D*>(luaL_checkudata(L, 1, ROTATOR_MT));
    PushRotator(L, r->Pitch, r->Yaw, r->Roll);
    return 1;
}

static int Rotator_Index(lua_State* L) {
    auto* rot = static_cast<Rotator3D*>(luaL_checkudata(L, 1, ROTATOR_MT));
    if (lua_isstring(L, 2)) {
        const char* key = lua_tostring(L, 2);
        if (_stricmp(key, "Pitch") == 0) { lua_pushnumber(L, rot->Pitch); return 1; }
        if (_stricmp(key, "Yaw")   == 0) { lua_pushnumber(L, rot->Yaw);   return 1; }
        if (_stricmp(key, "Roll")  == 0) { lua_pushnumber(L, rot->Roll);   return 1; }
        if (_stricmp(key, "Vector") == 0 || _stricmp(key, "GetForwardVector") == 0 || _stricmp(key, "ForwardVector") == 0) {
            lua_pushcfunction(L, RotForward_Impl);
            return 1;
        }
        if (_stricmp(key, "GetRightVector") == 0 || _stricmp(key, "RightVector") == 0) {
            lua_pushcfunction(L, RotRight_Impl);
            return 1;
        }
        if (_stricmp(key, "GetUpVector") == 0 || _stricmp(key, "UpVector") == 0) {
            lua_pushcfunction(L, RotUp_Impl);
            return 1;
        }
        if (_stricmp(key, "Normalize") == 0) {
            lua_pushcfunction(L, RotNorm_Impl);
            return 1;
        }
        if (_stricmp(key, "Clamp") == 0) {
            lua_pushcfunction(L, RotClamp_Impl);
            return 1;
        }
        if (_stricmp(key, "ToTable") == 0) {
            lua_pushcfunction(L, RotToTable_Impl);
            return 1;
        }
        if (_stricmp(key, "Clone") == 0) {
            lua_pushcfunction(L, RotClone_Impl);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int Rotator_NewIndex(lua_State* L) {
    auto* rot = static_cast<Rotator3D*>(luaL_checkudata(L, 1, ROTATOR_MT));
    const char* key = luaL_checkstring(L, 2);
    float val = static_cast<float>(luaL_checknumber(L, 3));
    if (_stricmp(key, "Pitch") == 0) rot->Pitch = val;
    else if (_stricmp(key, "Yaw")  == 0) rot->Yaw  = val;
    else if (_stricmp(key, "Roll") == 0) rot->Roll = val;
    return 0;
}

static int Rotator_Add(lua_State* L) {
    Rotator3D r1, r2;
    if (GetRotator(L, 1, r1) && GetRotator(L, 2, r2)) {
        PushRotator(L, r1.Pitch + r2.Pitch, r1.Yaw + r2.Yaw, r1.Roll + r2.Roll);
        return 1;
    }
    return luaL_error(L, "Attempt to add invalid rotator types");
}

static int Rotator_Sub(lua_State* L) {
    Rotator3D r1, r2;
    if (GetRotator(L, 1, r1) && GetRotator(L, 2, r2)) {
        PushRotator(L, r1.Pitch - r2.Pitch, r1.Yaw - r2.Yaw, r1.Roll - r2.Roll);
        return 1;
    }
    return luaL_error(L, "Attempt to subtract invalid rotator types");
}

static int Rotator_ToString(lua_State* L) {
    auto* rot = static_cast<Rotator3D*>(luaL_checkudata(L, 1, ROTATOR_MT));
    char buf[128];
    snprintf(buf, sizeof(buf), "FRotator(P=%.2f, Y=%.2f, R=%.2f)", rot->Pitch, rot->Yaw, rot->Roll);
    lua_pushstring(L, buf);
    return 1;
}

static int TransTransformPos_Impl(lua_State* L) {
    auto* t = static_cast<Transform3D*>(luaL_checkudata(L, 1, TRANSFORM_MT));
    Vector3D v;
    if (!GetVector(L, 2, v)) return luaL_error(L, "TransformPosition expects FVector");
    Vector3D scaled = { v.X * t->Scale3D.X, v.Y * t->Scale3D.Y, v.Z * t->Scale3D.Z };
    constexpr float DEG_TO_RAD = 3.1415926535f / 180.0f;
    float p = t->Rotation.Pitch * DEG_TO_RAD;
    float y = t->Rotation.Yaw * DEG_TO_RAD;
    float r = t->Rotation.Roll * DEG_TO_RAD;
    float cp = std::cos(p), sp = std::sin(p);
    float cy = std::cos(y), sy = std::sin(y);
    float cr = std::cos(r), sr = std::sin(r);

    Vector3D rotated = {
        scaled.X * (cp * cy) + scaled.Y * (cy * sp * sr - sy * cr) + scaled.Z * (-cy * sp * cr - sy * sr),
        scaled.X * (cp * sy) + scaled.Y * (sy * sp * sr + cy * cr) + scaled.Z * (-sy * sp * cr + cy * sr),
        scaled.X * (sp)       + scaled.Y * (-cp * sr)               + scaled.Z * (cp * cr)
    };
    PushVector(L, rotated.X + t->Translation.X, rotated.Y + t->Translation.Y, rotated.Z + t->Translation.Z);
    return 1;
}

static int TransToTable_Impl(lua_State* L) {
    auto* t = static_cast<Transform3D*>(luaL_checkudata(L, 1, TRANSFORM_MT));
    lua_newtable(L);
    PushVector(L, t->Translation.X, t->Translation.Y, t->Translation.Z);
    lua_setfield(L, -2, "Translation");
    PushRotator(L, t->Rotation.Pitch, t->Rotation.Yaw, t->Rotation.Roll);
    lua_setfield(L, -2, "Rotation");
    PushVector(L, t->Scale3D.X, t->Scale3D.Y, t->Scale3D.Z);
    lua_setfield(L, -2, "Scale3D");
    return 1;
}

static int TransClone_Impl(lua_State* L) {
    auto* t = static_cast<Transform3D*>(luaL_checkudata(L, 1, TRANSFORM_MT));
    PushTransform(L, *t);
    return 1;
}

static int Transform_Index(lua_State* L) {
    auto* trans = static_cast<Transform3D*>(luaL_checkudata(L, 1, TRANSFORM_MT));
    if (lua_isstring(L, 2)) {
        const char* key = lua_tostring(L, 2);
        if (_stricmp(key, "Translation") == 0 || _stricmp(key, "Location") == 0) {
            PushVector(L, trans->Translation.X, trans->Translation.Y, trans->Translation.Z);
            return 1;
        }
        if (_stricmp(key, "Rotation") == 0) {
            PushRotator(L, trans->Rotation.Pitch, trans->Rotation.Yaw, trans->Rotation.Roll);
            return 1;
        }
        if (_stricmp(key, "Scale3D") == 0 || _stricmp(key, "Scale") == 0) {
            PushVector(L, trans->Scale3D.X, trans->Scale3D.Y, trans->Scale3D.Z);
            return 1;
        }
        if (_stricmp(key, "TransformPosition") == 0 || _stricmp(key, "TransformLocation") == 0) {
            lua_pushcfunction(L, TransTransformPos_Impl);
            return 1;
        }
        if (_stricmp(key, "ToTable") == 0) {
            lua_pushcfunction(L, TransToTable_Impl);
            return 1;
        }
        if (_stricmp(key, "Clone") == 0) {
            lua_pushcfunction(L, TransClone_Impl);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int Transform_NewIndex(lua_State* L) {
    auto* trans = static_cast<Transform3D*>(luaL_checkudata(L, 1, TRANSFORM_MT));
    const char* key = luaL_checkstring(L, 2);
    if (_stricmp(key, "Translation") == 0 || _stricmp(key, "Location") == 0) {
        GetVector(L, 3, trans->Translation);
    } else if (_stricmp(key, "Rotation") == 0) {
        GetRotator(L, 3, trans->Rotation);
    } else if (_stricmp(key, "Scale3D") == 0 || _stricmp(key, "Scale") == 0) {
        GetVector(L, 3, trans->Scale3D);
    }
    return 0;
}

static int Transform_ToString(lua_State* L) {
    auto* trans = static_cast<Transform3D*>(luaL_checkudata(L, 1, TRANSFORM_MT));
    char buf[256];
    snprintf(buf, sizeof(buf),
             "FTransform(Loc=(%.1f,%.1f,%.1f) Rot=(%.1f,%.1f,%.1f) Scale=(%.1f,%.1f,%.1f))",
             trans->Translation.X, trans->Translation.Y, trans->Translation.Z,
             trans->Rotation.Pitch, trans->Rotation.Yaw, trans->Rotation.Roll,
             trans->Scale3D.X, trans->Scale3D.Y, trans->Scale3D.Z);
    lua_pushstring(L, buf);
    return 1;
}

static int ColorToHex_Impl(lua_State* L) {
    auto* col = static_cast<LinearColor4D*>(luaL_checkudata(L, 1, COLOR_MT));
    auto toByte = [](float f) -> int {
        return std::clamp(static_cast<int>(std::round(f * 255.0f)), 0, 255);
    };
    char buf[16];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", toByte(col->R), toByte(col->G), toByte(col->B), toByte(col->A));
    lua_pushstring(L, buf);
    return 1;
}

static int ColorToTable_Impl(lua_State* L) {
    auto* col = static_cast<LinearColor4D*>(luaL_checkudata(L, 1, COLOR_MT));
    lua_newtable(L);
    lua_pushnumber(L, col->R); lua_setfield(L, -2, "R");
    lua_pushnumber(L, col->G); lua_setfield(L, -2, "G");
    lua_pushnumber(L, col->B); lua_setfield(L, -2, "B");
    lua_pushnumber(L, col->A); lua_setfield(L, -2, "A");
    return 1;
}

static int ColorClone_Impl(lua_State* L) {
    auto* col = static_cast<LinearColor4D*>(luaL_checkudata(L, 1, COLOR_MT));
    PushColor(L, col->R, col->G, col->B, col->A);
    return 1;
}

static int Color_Index(lua_State* L) {
    auto* col = static_cast<LinearColor4D*>(luaL_checkudata(L, 1, COLOR_MT));
    if (lua_isstring(L, 2)) {
        const char* key = lua_tostring(L, 2);
        if (_stricmp(key, "R") == 0) { lua_pushnumber(L, col->R); return 1; }
        if (_stricmp(key, "G") == 0) { lua_pushnumber(L, col->G); return 1; }
        if (_stricmp(key, "B") == 0) { lua_pushnumber(L, col->B); return 1; }
        if (_stricmp(key, "A") == 0) { lua_pushnumber(L, col->A); return 1; }
        if (_stricmp(key, "ToHex") == 0 || _stricmp(key, "Hex") == 0) {
            lua_pushcfunction(L, ColorToHex_Impl);
            return 1;
        }
        if (_stricmp(key, "ToTable") == 0) {
            lua_pushcfunction(L, ColorToTable_Impl);
            return 1;
        }
        if (_stricmp(key, "Clone") == 0) {
            lua_pushcfunction(L, ColorClone_Impl);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int Color_NewIndex(lua_State* L) {
    auto* col = static_cast<LinearColor4D*>(luaL_checkudata(L, 1, COLOR_MT));
    const char* key = luaL_checkstring(L, 2);
    float val = static_cast<float>(luaL_checknumber(L, 3));
    if      (_stricmp(key, "R") == 0) col->R = val;
    else if (_stricmp(key, "G") == 0) col->G = val;
    else if (_stricmp(key, "B") == 0) col->B = val;
    else if (_stricmp(key, "A") == 0) col->A = val;
    return 0;
}

static int Color_Add(lua_State* L) {
    LinearColor4D c1, c2;
    if (GetColor(L, 1, c1) && GetColor(L, 2, c2)) {
        PushColor(L, c1.R + c2.R, c1.G + c2.G, c1.B + c2.B, c1.A + c2.A);
        return 1;
    }
    return luaL_error(L, "Attempt to add invalid color types");
}

static int Color_Sub(lua_State* L) {
    LinearColor4D c1, c2;
    if (GetColor(L, 1, c1) && GetColor(L, 2, c2)) {
        PushColor(L, c1.R - c2.R, c1.G - c2.G, c1.B - c2.B, c1.A - c2.A);
        return 1;
    }
    return luaL_error(L, "Attempt to subtract invalid color types");
}

static int Color_Mul(lua_State* L) {
    LinearColor4D c;
    if (GetColor(L, 1, c) && lua_isnumber(L, 2)) {
        float s = static_cast<float>(lua_tonumber(L, 2));
        PushColor(L, c.R * s, c.G * s, c.B * s, c.A * s);
        return 1;
    }
    return luaL_error(L, "Color multiplication requires a color and number");
}

static int Color_ToString(lua_State* L) {
    auto* col = static_cast<LinearColor4D*>(luaL_checkudata(L, 1, COLOR_MT));
    char buf[128];
    snprintf(buf, sizeof(buf), "FLinearColor(R=%.2f, G=%.2f, B=%.2f, A=%.2f)",
             col->R, col->G, col->B, col->A);
    lua_pushstring(L, buf);
    return 1;
}

static int Math_Lerp(lua_State* L) {
    float a = static_cast<float>(luaL_checknumber(L, 1));
    float b = static_cast<float>(luaL_checknumber(L, 2));
    float alpha = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushnumber(L, a + alpha * (b - a));
    return 1;
}

static int Math_Clamp(lua_State* L) {
    float val = static_cast<float>(luaL_checknumber(L, 1));
    float minVal = static_cast<float>(luaL_checknumber(L, 2));
    float maxVal = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushnumber(L, std::clamp(val, minVal, maxVal));
    return 1;
}

static int Math_VInterpTo(lua_State* L) {
    Vector3D current, target;
    if (GetVector(L, 1, current) && GetVector(L, 2, target)) {
        float dt = static_cast<float>(luaL_checknumber(L, 3));
        float speed = static_cast<float>(luaL_checknumber(L, 4));
        if (speed <= 0.0f) { PushVector(L, target.X, target.Y, target.Z); return 1; }
        Vector3D delta = { target.X - current.X, target.Y - current.Y, target.Z - current.Z };
        float dist = std::sqrt(delta.X * delta.X + delta.Y * delta.Y + delta.Z * delta.Z);
        if (dist < 0.001f) { PushVector(L, target.X, target.Y, target.Z); return 1; }
        float step = speed * dt;
        if (step >= dist) { PushVector(L, target.X, target.Y, target.Z); return 1; }
        float factor = step / dist;
        PushVector(L, current.X + delta.X * factor, current.Y + delta.Y * factor, current.Z + delta.Z * factor);
        return 1;
    }
    return luaL_error(L, "VInterpTo expects (currentVec, targetVec, deltaTime, speed)");
}

static int Math_RInterpTo(lua_State* L) {
    Rotator3D current, target;
    if (GetRotator(L, 1, current) && GetRotator(L, 2, target)) {
        float dt = static_cast<float>(luaL_checknumber(L, 3));
        float speed = static_cast<float>(luaL_checknumber(L, 4));
        auto normAxis = [](float a) {
            a = std::fmod(a, 360.0f);
            if (a > 180.0f) a -= 360.0f;
            else if (a < -180.0f) a += 360.0f;
            return a;
        };
        float dp = normAxis(target.Pitch - current.Pitch);
        float dy = normAxis(target.Yaw - current.Yaw);
        float dr = normAxis(target.Roll - current.Roll);
        float step = speed * dt;
        float p = (std::abs(dp) <= step) ? target.Pitch : (current.Pitch + (dp > 0 ? step : -step));
        float y = (std::abs(dy) <= step) ? target.Yaw   : (current.Yaw   + (dy > 0 ? step : -step));
        float r = (std::abs(dr) <= step) ? target.Roll  : (current.Roll  + (dr > 0 ? step : -step));
        PushRotator(L, p, y, r);
        return 1;
    }
    return luaL_error(L, "RInterpTo expects (currentRot, targetRot, deltaTime, speed)");
}

static int Math_FindLookAtRotation(lua_State* L) {
    Vector3D start, target;
    if (GetVector(L, 1, start) && GetVector(L, 2, target)) {
        Vector3D diff = { target.X - start.X, target.Y - start.Y, target.Z - start.Z };
        constexpr float RAD_TO_DEG = 180.0f / 3.1415926535f;
        float yaw = std::atan2(diff.Y, diff.X) * RAD_TO_DEG;
        float pitch = std::atan2(diff.Z, std::sqrt(diff.X * diff.X + diff.Y * diff.Y)) * RAD_TO_DEG;
        PushRotator(L, pitch, yaw, 0.0f);
        return 1;
    }
    return luaL_error(L, "FindLookAtRotation expects (startVec, targetVec)");
}

static void PushRawTypedValue(lua_State* L, uint8_t* addr, const std::string& typeName, int32_t size) {
    if (!addr) { lua_pushnil(L); return; }

    if (typeName.find("Int8Property") != std::string::npos || typeName.find("ByteProperty") != std::string::npos) {
        uint8_t v = 0; SafeMem_Read(addr, &v, 1); lua_pushinteger(L, v); return;
    }
    if (typeName.find("Int16Property") != std::string::npos || typeName.find("UInt16Property") != std::string::npos) {
        int16_t v = 0; SafeMem_Read(addr, &v, 2); lua_pushinteger(L, v); return;
    }
    if (typeName.find("IntProperty") != std::string::npos || typeName.find("UInt32Property") != std::string::npos) {
        int32_t v = 0; SafeMem_Read(addr, &v, 4); lua_pushinteger(L, v); return;
    }
    if (typeName.find("Int64Property") != std::string::npos || typeName.find("UInt64Property") != std::string::npos) {
        int64_t v = 0; SafeMem_Read(addr, &v, 8); lua_pushinteger(L, v); return;
    }
    if (typeName.find("FloatProperty") != std::string::npos) {
        float v = 0.0f; SafeMem_Read(addr, &v, 4); lua_pushnumber(L, v); return;
    }
    if (typeName.find("DoubleProperty") != std::string::npos) {
        double v = 0.0; SafeMem_Read(addr, &v, 8); lua_pushnumber(L, v); return;
    }
    if (typeName.find("BoolProperty") != std::string::npos) {
        uint8_t v = 0; SafeMem_Read(addr, &v, 1); lua_pushboolean(L, v ? 1 : 0); return;
    }
    if (typeName.find("StrProperty") != std::string::npos) {
        FString fs = {};
        if (SafeMem_Read(addr, &fs, sizeof(fs))) {
            lua_pushstring(L, fs.ToStdString().c_str());
            return;
        }
    }
    if (typeName.find("NameProperty") != std::string::npos) {
        FName fn = {};
        if (SafeMem_Read(addr, &fn, sizeof(fn))) {
            std::string s = EngineDiscovery::GetCachedFName(g_State, fn.ComparisonIndex);
            lua_pushstring(L, s.c_str());
            return;
        }
    }
    if (typeName.find("TextProperty") != std::string::npos) {
        FString fs = {};
        if (SafeMem_Read(addr, &fs, sizeof(fs))) {
            lua_pushstring(L, fs.ToStdString().c_str());
            return;
        }
    }
    if (typeName.find("ObjectProperty") != std::string::npos || typeName.find("ClassProperty") != std::string::npos) {
        void* obj = nullptr; SafeMem_Read(addr, &obj, sizeof(void*));
        PushUObject(L, static_cast<UObject*>(obj));
        return;
    }
    if (typeName.find("StructProperty") != std::string::npos && size == 12) {
        Vector3D v; SafeMem_Read(addr, &v, sizeof(v));
        PushVector(L, v.X, v.Y, v.Z);
        return;
    }

    void* ptr = nullptr;
    if (SafeMem_Read(addr, &ptr, sizeof(void*)) && ptr) {
        lua_pushlightuserdata(L, ptr);
    } else {
        lua_pushnil(L);
    }
}

static std::string ContainerTypeName(const char* stored, void* prop) {
    if (stored && stored[0]) return std::string(stored);
    if (prop) {
        std::string resolved = EngineDiscovery::GetFFieldTypeName(g_State, prop);
        if (!resolved.empty()) return resolved;
    }
    return std::string();
}

static void TArray_PushElement(lua_State* L, LuaTArrayData* arrData, uint8_t* elemAddr, int32_t elemSize) {
    PushRawTypedValue(L, elemAddr, ContainerTypeName(arrData->TypeName, arrData->InnerProperty), elemSize);
}

static void TArray_PushElementView(lua_State* L, LuaTArrayData* arrData, uint8_t* elemAddr,
                                   int32_t elemSize) {
    if (arrData->InnerProperty) {
        FoundPropertyEx elem = DescribePropertyEx(arrData->InnerProperty);
        elem.Offset = 0;
        if (elem.TypeName.empty()) {
            elem.TypeName = ContainerTypeName(arrData->TypeName, arrData->InnerProperty);
        }
        if (elem.ElementSize <= 0) elem.ElementSize = elemSize;
        if (!elem.TypeName.empty()) {
            PushPropertyView(L, elemAddr, elem, nullptr);
            return;
        }
    }
    TArray_PushElement(L, arrData, elemAddr, elemSize);
}

static void ReportEmptyForEach(const LuaTArrayData* arr) {
    static std::mutex reportedMutex;
    static std::unordered_set<const void*> reported;
    std::lock_guard<std::mutex> lock(reportedMutex);
    if (!reported.insert(arr->InnerProperty).second) return;
    std::string msg = "[Lua] TArray:ForEach iterated an empty array (inner=" +
                      ContainerTypeName(arr->TypeName, arr->InnerProperty) + ")";
    EngineDiscovery::WriteLogLine(msg.c_str());
}

static int TArray_ForEach_Impl(lua_State* L) {
    auto* arr = static_cast<LuaTArrayData*>(luaL_checkudata(L, 1, TARRAY_MT));
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (!arr || !arr->ArrayPtr) return 0;

    TArray<uint8_t> rawArray = {};
    if (!SafeMem_Read(arr->ArrayPtr, &rawArray, sizeof(rawArray)) || !rawArray.Data) {
        ReportEmptyForEach(arr);
        return 0;
    }

    const int32_t elemSize = arr->ElementSize > 0 ? arr->ElementSize : static_cast<int32_t>(sizeof(void*));
    const int32_t count = rawArray.ArrayNum;
    if (count == 0) ReportEmptyForEach(arr);
    for (int32_t i = 0; i < count; ++i) {

        TArray<uint8_t> live = {};
        if (!SafeMem_Read(arr->ArrayPtr, &live, sizeof(live)) || !live.Data) break;
        uint8_t* elemAddr = reinterpret_cast<uint8_t*>(live.Data) + (i * elemSize);
        lua_pushvalue(L, 2);
        lua_pushinteger(L, i + 1);
        TArray_PushElementView(L, arr, elemAddr, elemSize);
        if (lua_pcall(L, 2, 1, 0) == LUA_OK) {
            bool stop = lua_isboolean(L, -1) && !lua_toboolean(L, -1);
            lua_pop(L, 1);
            if (stop) break;
        } else {

            const char* err = lua_tostring(L, -1);
            std::string msg = std::string("[Lua] TArray:ForEach callback error: ") + (err ? err : "unknown error");
            EngineDiscovery::WriteLogLine(msg.c_str());
            lua_pop(L, 1);
        }
    }
    return 0;
}

static int TArray_Num_Impl(lua_State* L) {
    auto* arrData = static_cast<LuaTArrayData*>(luaL_checkudata(L, 1, TARRAY_MT));
    if (!arrData || !arrData->ArrayPtr) { lua_pushinteger(L, 0); return 1; }
    TArray<uint8_t> rawArray = {};
    if (SafeMem_Read(arrData->ArrayPtr, &rawArray, sizeof(rawArray))) {
        lua_pushinteger(L, rawArray.ArrayNum);
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

static int TArray_IsValid_Impl(lua_State* L) {
    auto* arrData = static_cast<LuaTArrayData*>(luaL_checkudata(L, 1, TARRAY_MT));
    lua_pushboolean(L, (arrData && arrData->ArrayPtr && SafeMem_IsReadable(arrData->ArrayPtr, sizeof(TArray<void*>))) ? 1 : 0);
    return 1;
}

static int TArray_GetArrayAddress_Impl(lua_State* L) {
    auto* arrData = static_cast<LuaTArrayData*>(luaL_checkudata(L, 1, TARRAY_MT));
    lua_pushlightuserdata(L, arrData ? arrData->ArrayPtr : nullptr);
    return 1;
}

static int TArray_GetArrayDataAddress_Impl(lua_State* L) {
    auto* arrData = static_cast<LuaTArrayData*>(luaL_checkudata(L, 1, TARRAY_MT));
    TArray<uint8_t> rawArray = {};
    if (arrData && arrData->ArrayPtr && SafeMem_Read(arrData->ArrayPtr, &rawArray, sizeof(rawArray))) {
        lua_pushlightuserdata(L, rawArray.Data);
    } else {
        lua_pushlightuserdata(L, nullptr);
    }
    return 1;
}

static int TArray_GetArrayMax_Impl(lua_State* L) {
    auto* arrData = static_cast<LuaTArrayData*>(luaL_checkudata(L, 1, TARRAY_MT));
    TArray<uint8_t> rawArray = {};
    if (arrData && arrData->ArrayPtr && SafeMem_Read(arrData->ArrayPtr, &rawArray, sizeof(rawArray))) {
        lua_pushinteger(L, rawArray.ArrayMax);
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

static int TArray_Empty_Impl(lua_State* L) {
    auto* arrData = static_cast<LuaTArrayData*>(luaL_checkudata(L, 1, TARRAY_MT));
    if (!arrData || !arrData->ArrayPtr) { lua_pushboolean(L, 0); return 1; }

    int32_t zero = 0;
    bool ok = SafeMem_Write(static_cast<uint8_t*>(arrData->ArrayPtr) + sizeof(void*), &zero, sizeof(zero));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int TArray_TypeName_Impl(lua_State* L) {
    lua_pushstring(L, "TArray");
    (void)L;
    return 1;
}

static int TArray_Index(lua_State* L) {
    auto* arrData = static_cast<LuaTArrayData*>(luaL_checkudata(L, 1, TARRAY_MT));
    if (!arrData || !arrData->ArrayPtr) { lua_pushnil(L); return 1; }

    if (lua_isinteger(L, 2)) {
        int idx = static_cast<int>(lua_tointeger(L, 2)) - 1;
        TArray<uint8_t> rawArray = {};
        if (!SafeMem_Read(arrData->ArrayPtr, &rawArray, sizeof(rawArray)) ||
            !rawArray.Data || idx < 0 || idx >= rawArray.ArrayNum)
        {
            lua_pushnil(L); return 1;
        }
        int32_t elemSize = arrData->ElementSize > 0 ? arrData->ElementSize : static_cast<int32_t>(sizeof(void*));
        uint8_t* elemAddr = reinterpret_cast<uint8_t*>(rawArray.Data) + (idx * elemSize);
        TArray_PushElement(L, arrData, elemAddr, elemSize);
        return 1;
    }

    if (lua_isstring(L, 2)) {
        const char* key = lua_tostring(L, 2);
        if (_stricmp(key, "ForEach") == 0) { lua_pushcfunction(L, TArray_ForEach_Impl); return 1; }
        if (_stricmp(key, "Num") == 0 || _stricmp(key, "Count") == 0 || _stricmp(key, "GetArrayNum") == 0) { lua_pushcfunction(L, TArray_Num_Impl); return 1; }
        if (_stricmp(key, "IsValid") == 0) { lua_pushcfunction(L, TArray_IsValid_Impl); return 1; }
        if (_stricmp(key, "Empty") == 0) { lua_pushcfunction(L, TArray_Empty_Impl); return 1; }
        if (_stricmp(key, "type") == 0) { lua_pushcfunction(L, TArray_TypeName_Impl); return 1; }
        if (_stricmp(key, "GetArrayAddress") == 0) { lua_pushcfunction(L, TArray_GetArrayAddress_Impl); return 1; }
        if (_stricmp(key, "GetArrayDataAddress") == 0) { lua_pushcfunction(L, TArray_GetArrayDataAddress_Impl); return 1; }
        if (_stricmp(key, "GetArrayMax") == 0) { lua_pushcfunction(L, TArray_GetArrayMax_Impl); return 1; }
    }

    lua_pushnil(L);
    return 1;
}

static int TArray_ToString(lua_State* L) {
    auto* arrData = static_cast<LuaTArrayData*>(luaL_checkudata(L, 1, TARRAY_MT));
    if (!arrData || !arrData->ArrayPtr) { lua_pushstring(L, "TArray(null)"); return 1; }
    TArray<uint8_t> rawArray = {};
    int num = 0, max = 0;
    if (SafeMem_Read(arrData->ArrayPtr, &rawArray, sizeof(rawArray))) {
        num = rawArray.ArrayNum; max = rawArray.ArrayMax;
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "TArray<%s>[%d/%d] (0x%p)",
             arrData->TypeName[0] ? arrData->TypeName : "void", num, max, arrData->ArrayPtr);
    lua_pushstring(L, buf);
    return 1;
}

struct MapPairLayout {
    int32_t ValueOffset = 0;
    int32_t Stride = 0;
};
struct SetElementLayout {
    int32_t Stride = 0;
};
static MapPairLayout ComputeMapPairLayout(const LuaTMapData* map);
static SetElementLayout ComputeSetElementLayout(const LuaTSetData* set);

struct SparseSlotTable {
    const uint32_t* Words = nullptr;
    int32_t NumBits = 0;
    bool bValid = false;

    bool IsAllocated(int32_t index) const {
        if (!bValid) return true;
        if (index < 0 || index >= NumBits) return false;
        return ((Words[index >> 5] >> (index & 31)) & 1u) != 0;
    }
    int32_t AllocatedCount() const {
        if (!bValid) return NumBits;
        int32_t count = 0;
        for (int32_t i = 0; i < NumBits; ++i)
            if (IsAllocated(i)) ++count;
        return count;
    }
};
static SparseSlotTable ReadSparseSlotTable(const void* sparseArray, int32_t slotCount);

static int TMap_Num_Impl(lua_State* L) {
    auto* map = static_cast<LuaTMapData*>(luaL_checkudata(L, 1, TMAP_MT));
    if (!map || !map->MapPtr) { lua_pushinteger(L, 0); return 1; }
    TArray<uint8_t> rawData = {};
    if (SafeMem_Read(map->MapPtr, &rawData, sizeof(rawData))) {
        lua_pushinteger(L, ReadSparseSlotTable(map->MapPtr, rawData.ArrayNum).AllocatedCount());
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

static int TMap_ForEach_Impl(lua_State* L) {
    auto* map = static_cast<LuaTMapData*>(luaL_checkudata(L, 1, TMAP_MT));
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (!map || !map->MapPtr) return 0;

    TArray<uint8_t> rawData = {};
    if (!SafeMem_Read(map->MapPtr, &rawData, sizeof(rawData)) || !rawData.Data) return 0;

    int32_t keySize = map->KeySize > 0 ? map->KeySize : 8;
    int32_t valSize = map->ValueSize > 0 ? map->ValueSize : 8;

    const MapPairLayout layout = ComputeMapPairLayout(map);
    const SparseSlotTable slots = ReadSparseSlotTable(map->MapPtr, rawData.ArrayNum);
    const std::string keyType = ContainerTypeName(map->KeyTypeName, map->KeyProperty);
    const std::string valType = ContainerTypeName(map->ValueTypeName, map->ValueProperty);

    for (int32_t i = 0; i < rawData.ArrayNum && i < 10000; ++i) {
        if (!slots.IsAllocated(i)) continue;
        uint8_t* pairAddr = reinterpret_cast<uint8_t*>(rawData.Data) + static_cast<size_t>(i) * layout.Stride;
        if (!SafeMem_IsReadable(pairAddr, static_cast<size_t>(layout.Stride))) continue;

        lua_pushvalue(L, 2);
        PushRawTypedValue(L, pairAddr, keyType, keySize);
        PushRawTypedValue(L, pairAddr + layout.ValueOffset, valType, valSize);

        if (lua_pcall(L, 2, 1, 0) == LUA_OK) {
            bool stop = lua_isboolean(L, -1) && !lua_toboolean(L, -1);
            lua_pop(L, 1);
            if (stop) break;
        } else {
            lua_pop(L, 1);
        }
    }
    return 0;
}

static int32_t ContainerAlignUp(int32_t value, int32_t alignment) {
    if (alignment <= 1) return value;
    return ((value + alignment - 1) / alignment) * alignment;
}

struct ContainerStructBuilder {
    int32_t EndOfLastMember = 0;
    int32_t Alignment = 0;

    int32_t AddMember(int32_t memberSize, int32_t memberAlignment) {
        if (memberAlignment < 1) memberAlignment = 1;
        const int32_t offset = ContainerAlignUp(EndOfLastMember, memberAlignment);
        EndOfLastMember = offset + memberSize;
        if (memberAlignment > Alignment) Alignment = memberAlignment;
        return offset;
    }
    int32_t Size() const { return ContainerAlignUp(EndOfLastMember, Alignment); }
};

static int32_t SparseSlotStride(int32_t elementSize, int32_t elementAlignment) {
    constexpr int32_t kFreeListLinkSize = 2 * sizeof(int32_t);
    constexpr int32_t kFreeListLinkAlign = sizeof(int32_t);
    const int32_t alignment = elementAlignment > kFreeListLinkAlign ? elementAlignment : kFreeListLinkAlign;
    const int32_t size = elementSize > kFreeListLinkSize ? elementSize : kFreeListLinkSize;
    return ContainerAlignUp(size, alignment);
}

static int32_t PropertyAlignmentFor(const void* property, int32_t valueSize) {
    return EngineDiscovery::GetPropertyAlignment(g_State, const_cast<void*>(property), valueSize);
}

static MapPairLayout ComputeMapPairLayout(const LuaTMapData* map) {
    const int32_t keySize = map->KeySize > 0 ? map->KeySize : 8;
    const int32_t valueSize = map->ValueSize > 0 ? map->ValueSize : 8;

    ContainerStructBuilder pair;
    pair.AddMember(keySize, PropertyAlignmentFor(map->KeyProperty, keySize));

    MapPairLayout layout;
    layout.ValueOffset = pair.AddMember(valueSize, PropertyAlignmentFor(map->ValueProperty, valueSize));

    ContainerStructBuilder setElement;
    setElement.AddMember(pair.Size(), pair.Alignment);
    setElement.AddMember(static_cast<int32_t>(sizeof(int32_t)), static_cast<int32_t>(sizeof(int32_t)));
    setElement.AddMember(static_cast<int32_t>(sizeof(int32_t)), static_cast<int32_t>(sizeof(int32_t)));
    layout.Stride = SparseSlotStride(setElement.Size(), setElement.Alignment);
    return layout;
}

static SetElementLayout ComputeSetElementLayout(const LuaTSetData* set) {
    const int32_t elementSize = set->ElementSize > 0 ? set->ElementSize : 8;

    ContainerStructBuilder setElement;
    setElement.AddMember(elementSize, PropertyAlignmentFor(set->ElementProperty, elementSize));
    setElement.AddMember(static_cast<int32_t>(sizeof(int32_t)), static_cast<int32_t>(sizeof(int32_t)));
    setElement.AddMember(static_cast<int32_t>(sizeof(int32_t)), static_cast<int32_t>(sizeof(int32_t)));

    SetElementLayout layout;
    layout.Stride = SparseSlotStride(setElement.Size(), setElement.Alignment);
    return layout;
}

static void ReportSparseTableFallback(int32_t slotCount, const char* reason) {
    static bool reported = false;
    if (reported) return;
    reported = true;
    EngineDiscovery::WriteLogLine(
        (std::string("[Lua] A container's allocation bitmap did not validate (slot count ") +
         std::to_string(slotCount) + ", " + reason +
         "); counts and iteration fall back to the slot count, so freed slots may be reported").c_str());
}

static SparseSlotTable ReadSparseSlotTable(const void* sparseArray, int32_t slotCount) {
    SparseSlotTable table;
    table.NumBits = slotCount > 0 ? slotCount : 0;
    if (!sparseArray || slotCount <= 0) return table;

    constexpr int32_t kArrayHeaderSize = static_cast<int32_t>(sizeof(void*) + 2 * sizeof(int32_t));
    constexpr int32_t kInlineWordCount = 4;
    constexpr int32_t kInlineWordBytes = kInlineWordCount * static_cast<int32_t>(sizeof(uint32_t));
    constexpr int32_t kBitArrayInstanceSize = kInlineWordBytes + static_cast<int32_t>(sizeof(void*));
    constexpr int32_t kBitArrayHeaderSize = kBitArrayInstanceSize + 2 * static_cast<int32_t>(sizeof(int32_t));
    constexpr int32_t kNumFreeIndicesOffset = kArrayHeaderSize + kBitArrayHeaderSize + static_cast<int32_t>(sizeof(int32_t));

    uint8_t* base = reinterpret_cast<uint8_t*>(const_cast<void*>(sparseArray));
    uint8_t* bitmap = base + kArrayHeaderSize;

    const void* secondary = nullptr;
    int32_t bitCount = 0;
    const bool readable = SafeMem_Read(bitmap + kInlineWordBytes, &secondary, sizeof(secondary)) &&
                          SafeMem_Read(bitmap + kBitArrayInstanceSize, &bitCount, sizeof(bitCount));
    if (!readable || bitCount != slotCount) {
        ReportSparseTableFallback(slotCount, "the bitmap header did not read as one bit per slot");
        return table;
    }

    int32_t freeSlots = 0;
    if (!SafeMem_Read(base + kNumFreeIndicesOffset, &freeSlots, sizeof(freeSlots)) ||
        freeSlots < 0 || freeSlots > slotCount) {
        ReportSparseTableFallback(slotCount, "the bitmap's free-list count did not read as a slot count");
        return table;
    }

    const size_t wordBytes = static_cast<size_t>((slotCount + 31) / 32) * sizeof(uint32_t);
    const bool inlineHoldsAllWords = wordBytes <= static_cast<size_t>(kInlineWordBytes);
    const uint32_t* inlineWords = inlineHoldsAllWords ? reinterpret_cast<const uint32_t*>(bitmap) : nullptr;

    const uint32_t* candidates[2] = { inlineWords, nullptr };
    int32_t candidateCount = inlineWords ? 1 : 0;
    if (secondary && SafeMem_IsReadable(secondary, wordBytes)) {
        candidates[0] = static_cast<const uint32_t*>(secondary);
        candidates[1] = inlineWords;
        candidateCount = inlineWords ? 2 : 1;
    }

    for (int32_t candidate = 0; candidate < candidateCount; ++candidate) {
        const uint32_t* words = candidates[candidate];
        if (!words || (candidate > 0 && words == candidates[candidate - 1])) continue;

        int32_t usedSlots = 0;
        for (int32_t i = 0; i < bitCount; ++i) {
            if (((words[i >> 5] >> (i & 31)) & 1u) != 0) ++usedSlots;
        }
        if (usedSlots != slotCount - freeSlots) continue;

        table.Words = words;
        table.bValid = true;
        return table;
    }

    ReportSparseTableFallback(slotCount, candidateCount > 0 ? "the bitmap and the free-list count disagreed"
                                                            : "the bitmap's word array was not readable");
    return table;
}

static bool ContainerValuesEqual(lua_State* L, int a, int b) {
    const bool aNil = lua_isnoneornil(L, a);
    const bool bNil = lua_isnoneornil(L, b);
    if (aNil || bNil) {

        return aNil && bNil;
    }
    if (lua_isnumber(L, a) && lua_isnumber(L, b)) {
        return lua_tonumber(L, a) == lua_tonumber(L, b);
    }
    if (lua_isstring(L, a) && lua_isstring(L, b)) {
        size_t la = 0, lb = 0;
        const char* sa = lua_tolstring(L, a, &la);
        const char* sb = lua_tolstring(L, b, &lb);
        return la == lb && memcmp(sa, sb, la) == 0;
    }

    if (lua_islightuserdata(L, a) && lua_islightuserdata(L, b)) {
        return lua_touserdata(L, a) == lua_touserdata(L, b);
    }

    std::string oa, ob;
    bool haveA = false, haveB = false;
    if (lua_isstring(L, a)) { oa = lua_tostring(L, a); haveA = true; }
    else if (lua_isuserdata(L, a)) { haveA = CompatStringFromArg(L, a, oa); }
    if (lua_isstring(L, b)) { ob = lua_tostring(L, b); haveB = true; }
    else if (lua_isuserdata(L, b)) { haveB = CompatStringFromArg(L, b, ob); }
    if (haveA && haveB && !oa.empty() && !ob.empty()) return oa == ob;

    auto render = [L](int idx, std::string& out) -> bool {
        if (!lua_isuserdata(L, idx)) return false;
        int top = lua_gettop(L);
        lua_getmetatable(L, idx);
        bool done = false;
        if (lua_istable(L, -1)) {
            lua_pushliteral(L, "__tostring");
            lua_rawget(L, -2);
            if (lua_isfunction(L, -1)) {
                lua_pushvalue(L, idx);
                if (lua_pcall(L, 1, 1, 0) == LUA_OK && lua_isstring(L, -1)) {
                    out = lua_tostring(L, -1);
                    done = true;
                }
                lua_pop(L, 1);
            }
        }
        lua_settop(L, top);
        return done;
    };
    std::string ra, rb;
    if (!render(a, ra) || !render(b, rb)) return false;
    return ra == rb;
}

static bool TMap_LocatePair(lua_State* L, LuaTMapData* map, int keyIdx, uint8_t** outValue, int32_t* outKeySlotOff) {
    if (outValue) *outValue = nullptr;
    if (!map || !map->MapPtr) return false;
    TArray<uint8_t> rawData = {};
    if (!SafeMem_Read(map->MapPtr, &rawData, sizeof(rawData)) || !rawData.Data) return false;

    int32_t keySize = map->KeySize > 0 ? map->KeySize : static_cast<int32_t>(sizeof(void*));
    int32_t valSize = map->ValueSize > 0 ? map->ValueSize : static_cast<int32_t>(sizeof(void*));
    const MapPairLayout layout = ComputeMapPairLayout(map);
    const SparseSlotTable slots = ReadSparseSlotTable(map->MapPtr, rawData.ArrayNum);
    if (outKeySlotOff) *outKeySlotOff = layout.ValueOffset;
    const std::string keyType = ContainerTypeName(map->KeyTypeName, map->KeyProperty);

    for (int32_t i = 0; i < rawData.ArrayNum && i < 20000; ++i) {
        if (!slots.IsAllocated(i)) continue;
        uint8_t* pair = reinterpret_cast<uint8_t*>(rawData.Data) + static_cast<size_t>(i) * layout.Stride;
        if (!SafeMem_IsReadable(pair, static_cast<size_t>(layout.Stride))) continue;
        int base = lua_gettop(L);
        PushRawTypedValue(L, pair, keyType, keySize);
        bool eq = ContainerValuesEqual(L, -1, keyIdx);
        lua_settop(L, base);
        if (eq) {
            if (outValue) *outValue = pair + layout.ValueOffset;
            return true;
        }
    }
    return false;
}

static int TMap_Find_Impl(lua_State* L) {
    auto* map = static_cast<LuaTMapData*>(luaL_checkudata(L, 1, TMAP_MT));
    uint8_t* value = nullptr;
    if (!TMap_LocatePair(L, map, 2, &value, nullptr) || !value) { lua_pushnil(L); return 1; }
    const std::string valType = ContainerTypeName(map->ValueTypeName, map->ValueProperty);
    PushRawTypedValue(L, value, valType, map->ValueSize > 0 ? map->ValueSize : static_cast<int32_t>(sizeof(void*)));
    return 1;
}

static int TMap_Contains_Impl(lua_State* L) {
    auto* map = static_cast<LuaTMapData*>(luaL_checkudata(L, 1, TMAP_MT));
    lua_pushboolean(L, TMap_LocatePair(L, map, 2, nullptr, nullptr) ? 1 : 0);
    return 1;
}

static int TMap_RefuseMutation(lua_State* L, const char* member) {
    std::string msg = std::string("[Lua] TMap:") + member + " is not available: a script map's sparse array and hash index are engine-owned";
    EngineDiscovery::WriteLogLine(msg.c_str());
    lua_pushboolean(L, 0);
    return 1;
}

static int TMap_TypeName_Impl(lua_State* L) {
    lua_pushstring(L, "TMap");
    (void)L;
    return 1;
}

static int TMap_Index(lua_State* L) {
    auto* map = static_cast<LuaTMapData*>(luaL_checkudata(L, 1, TMAP_MT));
    if (!map || !map->MapPtr) { lua_pushnil(L); return 1; }

    if (lua_isstring(L, 2)) {
        const char* key = lua_tostring(L, 2);
        if (_stricmp(key, "ForEach") == 0) { lua_pushcfunction(L, TMap_ForEach_Impl); return 1; }
        if (_stricmp(key, "Num") == 0 || _stricmp(key, "Count") == 0) { lua_pushcfunction(L, TMap_Num_Impl); return 1; }
        if (_stricmp(key, "Find") == 0) { lua_pushcfunction(L, TMap_Find_Impl); return 1; }
        if (_stricmp(key, "Contains") == 0 || _stricmp(key, "ContainsKey") == 0) { lua_pushcfunction(L, TMap_Contains_Impl); return 1; }
        if (_stricmp(key, "Add") == 0) { lua_pushcclosure(L, [](lua_State* L2) -> int { return TMap_RefuseMutation(L2, "Add"); }, 0); return 1; }
        if (_stricmp(key, "Remove") == 0) { lua_pushcclosure(L, [](lua_State* L2) -> int { return TMap_RefuseMutation(L2, "Remove"); }, 0); return 1; }
        if (_stricmp(key, "Empty") == 0) { lua_pushcclosure(L, [](lua_State* L2) -> int { return TMap_RefuseMutation(L2, "Empty"); }, 0); return 1; }
        if (_stricmp(key, "type") == 0) { lua_pushcfunction(L, TMap_TypeName_Impl); return 1; }
        if (_stricmp(key, "IsValid") == 0) {
            lua_pushboolean(L, (map && map->MapPtr && SafeMem_IsReadable(map->MapPtr, 16)) ? 1 : 0);
            return 1;
        }
    }

    uint8_t* value = nullptr;
    if (TMap_LocatePair(L, map, 2, &value, nullptr) && value) {
        const std::string valType = ContainerTypeName(map->ValueTypeName, map->ValueProperty);
        PushRawTypedValue(L, value, valType, map->ValueSize > 0 ? map->ValueSize : static_cast<int32_t>(sizeof(void*)));
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int TMap_ToString(lua_State* L) {
    auto* map = static_cast<LuaTMapData*>(luaL_checkudata(L, 1, TMAP_MT));
    if (!map || !map->MapPtr) { lua_pushstring(L, "TMap(null)"); return 1; }
    TArray<uint8_t> rawData = {};
    int num = 0;
    if (SafeMem_Read(map->MapPtr, &rawData, sizeof(rawData))) {
        num = ReadSparseSlotTable(map->MapPtr, rawData.ArrayNum).AllocatedCount();
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "TMap<%s, %s>[%d] (0x%p)",
             map->KeyTypeName[0] ? map->KeyTypeName : "Key",
             map->ValueTypeName[0] ? map->ValueTypeName : "Val",
             num, map->MapPtr);
    lua_pushstring(L, buf);
    return 1;
}

static int TSet_Num_Impl(lua_State* L) {
    auto* set = static_cast<LuaTSetData*>(luaL_checkudata(L, 1, TSET_MT));
    if (!set || !set->SetPtr) { lua_pushinteger(L, 0); return 1; }
    TArray<uint8_t> rawData = {};
    if (SafeMem_Read(set->SetPtr, &rawData, sizeof(rawData))) {
        lua_pushinteger(L, ReadSparseSlotTable(set->SetPtr, rawData.ArrayNum).AllocatedCount());
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

static int TSet_ForEach_Impl(lua_State* L) {
    auto* set = static_cast<LuaTSetData*>(luaL_checkudata(L, 1, TSET_MT));
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (!set || !set->SetPtr) return 0;

    TArray<uint8_t> rawData = {};
    if (!SafeMem_Read(set->SetPtr, &rawData, sizeof(rawData)) || !rawData.Data) return 0;

    int32_t elemSize = set->ElementSize > 0 ? set->ElementSize : 8;
    const SetElementLayout layout = ComputeSetElementLayout(set);
    const SparseSlotTable slots = ReadSparseSlotTable(set->SetPtr, rawData.ArrayNum);
    const std::string elemType = ContainerTypeName(set->ElementTypeName, set->ElementProperty);

    for (int i = 0; i < rawData.ArrayNum && i < 10000; ++i) {
        if (!slots.IsAllocated(i)) continue;
        uint8_t* elemAddr = reinterpret_cast<uint8_t*>(rawData.Data) + static_cast<size_t>(i) * layout.Stride;
        if (!SafeMem_IsReadable(elemAddr, static_cast<size_t>(layout.Stride))) continue;

        lua_pushvalue(L, 2);
        PushRawTypedValue(L, elemAddr, elemType, elemSize);

        if (lua_pcall(L, 1, 1, 0) == LUA_OK) {
            bool stop = lua_isboolean(L, -1) && !lua_toboolean(L, -1);
            lua_pop(L, 1);
            if (stop) break;
        } else {
            lua_pop(L, 1);
        }
    }
    return 0;
}

static bool TSet_LocateElement(lua_State* L, LuaTSetData* set, int valueIdx) {
    if (!set || !set->SetPtr) return false;
    TArray<uint8_t> rawData = {};
    if (!SafeMem_Read(set->SetPtr, &rawData, sizeof(rawData)) || !rawData.Data) return false;

    int32_t elemSize = set->ElementSize > 0 ? set->ElementSize : static_cast<int32_t>(sizeof(void*));
    const SetElementLayout layout = ComputeSetElementLayout(set);
    const SparseSlotTable slots = ReadSparseSlotTable(set->SetPtr, rawData.ArrayNum);
    const std::string elemType = ContainerTypeName(set->ElementTypeName, set->ElementProperty);

    for (int32_t i = 0; i < rawData.ArrayNum && i < 20000; ++i) {
        if (!slots.IsAllocated(i)) continue;
        uint8_t* elem = reinterpret_cast<uint8_t*>(rawData.Data) + static_cast<size_t>(i) * layout.Stride;
        if (!SafeMem_IsReadable(elem, static_cast<size_t>(layout.Stride))) continue;
        int base = lua_gettop(L);
        PushRawTypedValue(L, elem, elemType, elemSize);
        bool eq = ContainerValuesEqual(L, -1, valueIdx);
        lua_settop(L, base);
        if (eq) return true;
    }
    return false;
}

static int TSet_Contains_Impl(lua_State* L) {
    auto* set = static_cast<LuaTSetData*>(luaL_checkudata(L, 1, TSET_MT));
    lua_pushboolean(L, TSet_LocateElement(L, set, 2) ? 1 : 0);
    return 1;
}

static int TSet_RefuseMutation(lua_State* L, const char* member) {
    std::string msg = std::string("[Lua] TSet:") + member + " is not available: a script set's sparse array and hash index are engine-owned";
    EngineDiscovery::WriteLogLine(msg.c_str());
    lua_pushboolean(L, 0);
    return 1;
}

static int TSet_TypeName_Impl(lua_State* L) {
    lua_pushstring(L, "TSet");
    (void)L;
    return 1;
}

static int TSet_Index(lua_State* L) {
    auto* set = static_cast<LuaTSetData*>(luaL_checkudata(L, 1, TSET_MT));
    if (!set || !set->SetPtr) { lua_pushnil(L); return 1; }

    if (lua_isstring(L, 2)) {
        const char* key = lua_tostring(L, 2);
        if (_stricmp(key, "ForEach") == 0) { lua_pushcfunction(L, TSet_ForEach_Impl); return 1; }
        if (_stricmp(key, "Num") == 0 || _stricmp(key, "Count") == 0) { lua_pushcfunction(L, TSet_Num_Impl); return 1; }
        if (_stricmp(key, "Contains") == 0) { lua_pushcfunction(L, TSet_Contains_Impl); return 1; }
        if (_stricmp(key, "Add") == 0) { lua_pushcclosure(L, [](lua_State* L2) -> int { return TSet_RefuseMutation(L2, "Add"); }, 0); return 1; }
        if (_stricmp(key, "Remove") == 0) { lua_pushcclosure(L, [](lua_State* L2) -> int { return TSet_RefuseMutation(L2, "Remove"); }, 0); return 1; }
        if (_stricmp(key, "Empty") == 0) { lua_pushcclosure(L, [](lua_State* L2) -> int { return TSet_RefuseMutation(L2, "Empty"); }, 0); return 1; }
        if (_stricmp(key, "type") == 0) { lua_pushcfunction(L, TSet_TypeName_Impl); return 1; }
        if (_stricmp(key, "IsValid") == 0) {
            lua_pushboolean(L, (set && set->SetPtr && SafeMem_IsReadable(set->SetPtr, 16)) ? 1 : 0);
            return 1;
        }
    }

    lua_pushnil(L);
    return 1;
}

static int TSet_ToString(lua_State* L) {
    auto* set = static_cast<LuaTSetData*>(luaL_checkudata(L, 1, TSET_MT));
    if (!set || !set->SetPtr) { lua_pushstring(L, "TSet(null)"); return 1; }
    TArray<uint8_t> rawData = {};
    int num = 0;
    if (SafeMem_Read(set->SetPtr, &rawData, sizeof(rawData))) num = rawData.ArrayNum;
    char buf[128];
    snprintf(buf, sizeof(buf), "TSet<%s>[%d] (0x%p)",
             set->ElementTypeName[0] ? set->ElementTypeName : "Elem", num, set->SetPtr);
    lua_pushstring(L, buf);
    return 1;
}

struct ScriptDelegateEntry {
    void* TargetObject = nullptr;
    FName FunctionName = {};
};

static std::mutex g_DelegateMutex;
static uint32_t g_NextDelegateHandle = 1;

struct LuaDelegateBinding {
    uint32_t Handle = 0;
    int CallbackRef = LUA_NOREF;
    void* DelegatePtr = nullptr;
};

static std::unordered_map<void*, std::vector<LuaDelegateBinding>> g_DelegateBindings;

static int Delegate_Add_Impl(lua_State* L) {
    auto* del = static_cast<LuaFDelegateData*>(luaL_checkudata(L, 1, DELEGATE_MT));
    if (!del || !del->DelegatePtr) { lua_pushnil(L); return 1; }

    if (lua_isfunction(L, 2)) {
        lua_pushvalue(L, 2);
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);
        std::lock_guard<std::mutex> lock(g_DelegateMutex);
        uint32_t handle = g_NextDelegateHandle++;
        g_DelegateBindings[del->DelegatePtr].push_back({ handle, ref, del->DelegatePtr });
        lua_pushinteger(L, handle);
        return 1;
    }

    if (lua_isuserdata(L, 2) || lua_islightuserdata(L, 2)) {
        UObject* targetObj = GetUObject(L, 2);
        if (!targetObj) targetObj = static_cast<UObject*>(lua_touserdata(L, 2));
        const char* funcNameStr = luaL_optstring(L, 3, "");
        if (targetObj && funcNameStr && funcNameStr[0] && del->IsMulticast) {
            int32_t nameIdx = EngineDiscovery::ResolveFNameIndex(g_State, funcNameStr);
            if (nameIdx >= 0) {
                ScriptDelegateEntry entry = { targetObj, { static_cast<uint32_t>(nameIdx), 0 } };
                TArray<ScriptDelegateEntry> invList = {};
                if (SafeMem_Read(del->DelegatePtr, &invList, sizeof(invList))) {
                    if (invList.Data && invList.ArrayNum < invList.ArrayMax) {
                        uint8_t* dst = reinterpret_cast<uint8_t*>(invList.Data) + (invList.ArrayNum * sizeof(ScriptDelegateEntry));
                        DWORD oldProt = 0;
                        if (VirtualProtect(dst, sizeof(ScriptDelegateEntry), PAGE_READWRITE, &oldProt)) {
                            memcpy(dst, &entry, sizeof(ScriptDelegateEntry));
                            int32_t newCount = invList.ArrayNum + 1;
                            memcpy(reinterpret_cast<uint8_t*>(del->DelegatePtr) + 8, &newCount, 4);
                            VirtualProtect(dst, sizeof(ScriptDelegateEntry), oldProt, &oldProt);
                            lua_pushboolean(L, 1);
                            return 1;
                        }
                    }
                }
            }
        }
    }

    lua_pushnil(L);
    return 1;
}

static int Delegate_Remove_Impl(lua_State* L) {
    auto* del = static_cast<LuaFDelegateData*>(luaL_checkudata(L, 1, DELEGATE_MT));
    if (!del || !del->DelegatePtr) { lua_pushboolean(L, 0); return 1; }

    if (lua_isinteger(L, 2) || lua_isnumber(L, 2)) {
        uint32_t handle = static_cast<uint32_t>(lua_tointeger(L, 2));
        std::lock_guard<std::mutex> lock(g_DelegateMutex);
        auto it = g_DelegateBindings.find(del->DelegatePtr);
        if (it != g_DelegateBindings.end()) {
            for (auto bit = it->second.begin(); bit != it->second.end(); ++bit) {
                if (bit->Handle == handle) {
                    luaL_unref(L, LUA_REGISTRYINDEX, bit->CallbackRef);
                    it->second.erase(bit);
                    lua_pushboolean(L, 1);
                    return 1;
                }
            }
        }
    }

    if (lua_isuserdata(L, 2)) {
        UObject* targetObj = GetUObject(L, 2);
        if (targetObj && del->IsMulticast) {
            TArray<ScriptDelegateEntry> invList = {};
            if (SafeMem_Read(del->DelegatePtr, &invList, sizeof(invList)) && invList.Data) {
                for (int i = 0; i < invList.ArrayNum; ++i) {
                    ScriptDelegateEntry entry = {};
                    uint8_t* pEntry = reinterpret_cast<uint8_t*>(invList.Data) + (i * sizeof(ScriptDelegateEntry));
                    if (SafeMem_Read(pEntry, &entry, sizeof(entry)) && entry.TargetObject == targetObj) {
                        DWORD oldProt = 0;
                        if (VirtualProtect(invList.Data, invList.ArrayNum * sizeof(ScriptDelegateEntry), PAGE_READWRITE, &oldProt)) {
                            for (int j = i; j < invList.ArrayNum - 1; ++j) {
                                memcpy(reinterpret_cast<uint8_t*>(invList.Data) + j * sizeof(ScriptDelegateEntry),
                                       reinterpret_cast<uint8_t*>(invList.Data) + (j + 1) * sizeof(ScriptDelegateEntry),
                                       sizeof(ScriptDelegateEntry));
                            }
                            int32_t newCount = invList.ArrayNum - 1;
                            memcpy(reinterpret_cast<uint8_t*>(del->DelegatePtr) + 8, &newCount, 4);
                            VirtualProtect(invList.Data, invList.ArrayNum * sizeof(ScriptDelegateEntry), oldProt, &oldProt);
                            lua_pushboolean(L, 1);
                            return 1;
                        }
                    }
                }
            }
        }
    }

    lua_pushboolean(L, 0);
    return 1;
}

static int Delegate_Clear_Impl(lua_State* L) {
    auto* del = static_cast<LuaFDelegateData*>(luaL_checkudata(L, 1, DELEGATE_MT));
    if (!del || !del->DelegatePtr) return 0;

    {
        std::lock_guard<std::mutex> lock(g_DelegateMutex);
        auto it = g_DelegateBindings.find(del->DelegatePtr);
        if (it != g_DelegateBindings.end()) {
            for (const auto& b : it->second) {
                if (b.CallbackRef != LUA_NOREF) {
                    luaL_unref(L, LUA_REGISTRYINDEX, b.CallbackRef);
                }
            }
            g_DelegateBindings.erase(it);
        }
    }

    if (del->IsMulticast) {
        int32_t zero = 0;
        uint8_t* pNum = reinterpret_cast<uint8_t*>(del->DelegatePtr) + 8;
        DWORD oldProtect = 0;
        if (VirtualProtect(pNum, 4, PAGE_READWRITE, &oldProtect)) {
            memcpy(pNum, &zero, 4);
            VirtualProtect(pNum, 4, oldProtect, &oldProtect);
        }
    }
    return 0;
}

static int Delegate_Broadcast_Impl(lua_State* L) {
    auto* del = static_cast<LuaFDelegateData*>(luaL_checkudata(L, 1, DELEGATE_MT));
    if (!del || !del->DelegatePtr) return 0;

    int numArgs = lua_gettop(L) - 1;

    {
        std::vector<int> callbacks;
        {
            std::lock_guard<std::mutex> lock(g_DelegateMutex);
            auto it = g_DelegateBindings.find(del->DelegatePtr);
            if (it != g_DelegateBindings.end()) {
                for (const auto& b : it->second) {
                    if (b.CallbackRef != LUA_NOREF) callbacks.push_back(b.CallbackRef);
                }
            }
        }

        for (int ref : callbacks) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            for (int i = 2; i <= numArgs + 1; ++i) {
                lua_pushvalue(L, i);
            }
            if (lua_pcall(L, numArgs, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                EngineDiscovery::WriteLogLine(("[Delegate] Error in Lua callback: " + std::string(err ? err : "") + "\n").c_str());
                lua_pop(L, 1);
            }
        }
    }

    if (del->IsMulticast) {
        TArray<ScriptDelegateEntry> invList = {};
        if (SafeMem_Read(del->DelegatePtr, &invList, sizeof(invList)) && invList.Data) {
            for (int i = 0; i < invList.ArrayNum; ++i) {
                ScriptDelegateEntry entry = {};
                if (SafeMem_Read(reinterpret_cast<uint8_t*>(invList.Data) + (i * sizeof(ScriptDelegateEntry)), &entry, sizeof(entry))) {
                    if (entry.TargetObject && SafeMem_IsReadable(entry.TargetObject, 32)) {
                        std::string fn = EngineDiscovery::GetCachedFName(g_State, entry.FunctionName.ComparisonIndex);
                        UClass* cls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(entry.TargetObject));
                        if (cls) {
                            void* ufunc = EngineDiscovery::FindFunctionByName(g_State, static_cast<UStruct*>(cls), fn.c_str());
                            if (ufunc) {
                                EngineDiscovery::CallProcessEvent(g_State, entry.TargetObject, ufunc, nullptr);
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

static int Delegate_Num_Impl(lua_State* L) {
    auto* del = static_cast<LuaFDelegateData*>(luaL_checkudata(L, 1, DELEGATE_MT));
    if (!del || !del->DelegatePtr) { lua_pushinteger(L, 0); return 1; }

    int count = 0;
    {
        std::lock_guard<std::mutex> lock(g_DelegateMutex);
        auto it = g_DelegateBindings.find(del->DelegatePtr);
        if (it != g_DelegateBindings.end()) count += static_cast<int>(it->second.size());
    }

    if (del->IsMulticast) {
        TArray<ScriptDelegateEntry> invList = {};
        if (SafeMem_Read(del->DelegatePtr, &invList, sizeof(invList))) count += invList.ArrayNum;
    }
    lua_pushinteger(L, count);
    return 1;
}

static int Delegate_IsBound_Impl(lua_State* L) {
    Delegate_Num_Impl(L);
    int count = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_pushboolean(L, count > 0 ? 1 : 0);
    return 1;
}

static int Delegate_GetBindings_Impl(lua_State* L) {
    auto* del = static_cast<LuaFDelegateData*>(luaL_checkudata(L, 1, DELEGATE_MT));
    lua_newtable(L);
    if (!del || !del->DelegatePtr) return 1;

    int index = 0;
    {
        std::lock_guard<std::mutex> lock(g_DelegateMutex);
        auto it = g_DelegateBindings.find(del->DelegatePtr);
        if (it != g_DelegateBindings.end()) {
            for (const auto& binding : it->second) {
                lua_newtable(L);
                lua_pushinteger(L, static_cast<lua_Integer>(binding.Handle));
                lua_setfield(L, -2, "Handle");
                if (binding.CallbackRef != LUA_NOREF) {
                    lua_rawgeti(L, LUA_REGISTRYINDEX, binding.CallbackRef);
                    lua_setfield(L, -2, "Callback");
                }
                lua_rawseti(L, -2, ++index);
            }
        }
    }

    if (del->IsMulticast) {
        TArray<ScriptDelegateEntry> invList = {};
        if (SafeMem_Read(del->DelegatePtr, &invList, sizeof(invList)) && invList.Data) {
            for (int i = 0; i < invList.ArrayNum && i < 1024; ++i) {
                ScriptDelegateEntry entry = {};
                uint8_t* entryAddr = reinterpret_cast<uint8_t*>(invList.Data) + (i * sizeof(ScriptDelegateEntry));
                if (!SafeMem_Read(entryAddr, &entry, sizeof(entry))) break;
                if (!entry.TargetObject || !SafeMem_IsReadable(entry.TargetObject, 32)) continue;

                lua_newtable(L);
                PushUObject(L, static_cast<UObject*>(entry.TargetObject));
                lua_setfield(L, -2, "Self");
                lua_pushstring(L, EngineDiscovery::GetCachedFName(g_State, static_cast<int32_t>(entry.FunctionName.ComparisonIndex)).c_str());
                lua_setfield(L, -2, "FunctionName");
                lua_rawseti(L, -2, ++index);
            }
        }
    }
    return 1;
}

static int Delegate_Index(lua_State* L) {
    auto* del = static_cast<LuaFDelegateData*>(luaL_checkudata(L, 1, DELEGATE_MT));
    if (!del || !del->DelegatePtr) { lua_pushnil(L); return 1; }

    if (lua_isstring(L, 2)) {
        const char* key = lua_tostring(L, 2);
        if (_stricmp(key, "Add") == 0 || _stricmp(key, "Bind") == 0) { lua_pushcfunction(L, Delegate_Add_Impl); return 1; }
        if (_stricmp(key, "Remove") == 0 || _stricmp(key, "Unbind") == 0) { lua_pushcfunction(L, Delegate_Remove_Impl); return 1; }
        if (_stricmp(key, "Clear") == 0 || _stricmp(key, "RemoveAll") == 0) { lua_pushcfunction(L, Delegate_Clear_Impl); return 1; }
        if (_stricmp(key, "Broadcast") == 0 || _stricmp(key, "ProcessDelegate") == 0) { lua_pushcfunction(L, Delegate_Broadcast_Impl); return 1; }
        if (_stricmp(key, "Num") == 0 || _stricmp(key, "Count") == 0) { lua_pushcfunction(L, Delegate_Num_Impl); return 1; }
        if (_stricmp(key, "IsBound") == 0) { lua_pushcfunction(L, Delegate_IsBound_Impl); return 1; }
        if (_stricmp(key, "GetBindings") == 0 || _stricmp(key, "Bindings") == 0) { lua_pushcfunction(L, Delegate_GetBindings_Impl); return 1; }
        if (_stricmp(key, "IsValid") == 0) {
            lua_pushboolean(L, (del && del->DelegatePtr && SafeMem_IsReadable(del->DelegatePtr, 16)) ? 1 : 0);
            return 1;
        }
    }

    lua_pushnil(L);
    return 1;
}

static int Delegate_ToString(lua_State* L) {
    auto* del = static_cast<LuaFDelegateData*>(luaL_checkudata(L, 1, DELEGATE_MT));
    if (!del || !del->DelegatePtr) { lua_pushstring(L, "FDelegate(null)"); return 1; }
    char buf[128];
    snprintf(buf, sizeof(buf), "FDelegate%s (0x%p)", del->IsMulticast ? "[Multicast]" : "", del->DelegatePtr);
    lua_pushstring(L, buf);
    return 1;
}

static int Delegate_Constructor(lua_State* L) {
    void* buffer = malloc(32);
    if (buffer) memset(buffer, 0, 32);
    PushDelegate(L, buffer, true);
    return 1;
}

static std::mutex g_SoftTagMutex;
static int32_t g_LastSoftTestedTag = 0;
static std::string g_LastSoftTestedPath;

static void RecordSoftTest(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_SoftTagMutex);
    g_LastSoftTestedPath = path;
    size_t dot = path.rfind('.');
    std::string tag = (dot != std::string::npos) ? path.substr(dot + 1) : path;
    int32_t idx = EngineDiscovery::ResolveFNameIndex(g_State, tag.c_str());
    g_LastSoftTestedTag = idx >= 0 ? idx : 0;
}

static int SoftObject_GetAssetPath_Impl(lua_State* L) {
    auto* soft = static_cast<LuaSoftObjectData*>(luaL_checkudata(L, 1, SOFTOBJECT_MT));
    if (!soft) { lua_pushstring(L, ""); return 1; }
    lua_pushstring(L, soft->AssetPath);
    return 1;
}

static int SoftObject_Get_Impl(lua_State* L) {
    auto* soft = static_cast<LuaSoftObjectData*>(luaL_checkudata(L, 1, SOFTOBJECT_MT));
    if (!soft || !soft->AssetPath[0] || !g_State.GUObjectArray) { lua_pushnil(L); return 1; }

    RecordSoftTest(soft->AssetPath);

    const char* p = soft->AssetPath;
    int count = EngineDiscovery::GetObjectCount(g_State);
    for (int i = 0; i < count; ++i) {
        void* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
        if (!obj) continue;
        std::string fn = EngineDiscovery::BuildFullName(g_State, static_cast<UObject*>(obj));
        if (_stricmp(fn.c_str(), p) == 0 ||
            (fn.size() >= strlen(p) && _stricmp(fn.c_str() + (fn.size() - strlen(p)), p) == 0)) {
            PushUObject(L, static_cast<UObject*>(obj));
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int SoftObject_LoadSynchronous_Impl(lua_State* L) {
    auto* soft = static_cast<LuaSoftObjectData*>(luaL_checkudata(L, 1, SOFTOBJECT_MT));
    if (!soft || !soft->AssetPath[0]) { lua_pushnil(L); return 1; }

    int res = SoftObject_Get_Impl(L);
    if (!lua_isnil(L, -1)) return res;
    lua_pop(L, 1);

    UClass* loaded = ConsoleEnabler::FindClassByNameExact(g_State, soft->AssetPath);
    if (loaded) {
        PushUObject(L, loaded);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

static int SoftObject_IsValid_Impl(lua_State* L) {
    auto* soft = static_cast<LuaSoftObjectData*>(luaL_checkudata(L, 1, SOFTOBJECT_MT));
    lua_pushboolean(L, (soft && soft->AssetPath[0] != '\0') ? 1 : 0);
    return 1;
}

static int SoftObject_IsNull_Impl(lua_State* L) {
    auto* soft = static_cast<LuaSoftObjectData*>(luaL_checkudata(L, 1, SOFTOBJECT_MT));
    lua_pushboolean(L, (!soft || soft->AssetPath[0] == '\0') ? 1 : 0);
    return 1;
}

static int SoftObject_GetWeakPtr_Impl(lua_State* L) {
    auto* soft = static_cast<LuaSoftObjectData*>(luaL_testudata(L, 1, SOFTOBJECT_MT));
    UObject* resolved = nullptr;
    if (soft && soft->AssetPath[0]) {
        SoftObject_Get_Impl(L);
        resolved = GetUObject(L, -1);
        lua_pop(L, 1);
    }
    CompatPushWeakObjectPtr(L, resolved);
    return 1;
}

static int SoftObject_GetTagAtLastTest_Impl(lua_State* L) {
    (void)L;
    std::lock_guard<std::mutex> lock(g_SoftTagMutex);
    PushFName(L, static_cast<uint32_t>(g_LastSoftTestedTag), 0);
    return 1;
}

static int SoftObject_GetSubPathString_Impl(lua_State* L) {
    auto* soft = static_cast<LuaSoftObjectData*>(luaL_checkudata(L, 1, SOFTOBJECT_MT));
    std::string sub;
    if (soft && soft->AssetPath[0]) {
        std::string path = soft->AssetPath;
        size_t tagPos = path.rfind(":");
        if (tagPos != std::string::npos) path = path.substr(0, tagPos);
        size_t objPos = path.find(".");
        if (objPos != std::string::npos) sub = path.substr(objPos + 1);
    }
    lua_pushstring(L, sub.c_str());
    return 1;
}

static int SoftObject_GetObjectID_Impl(lua_State* L) {
    auto* soft = static_cast<LuaSoftObjectData*>(luaL_checkudata(L, 1, SOFTOBJECT_MT));
    void* resolved = soft ? soft->SoftPtr : nullptr;
    UObject* obj = nullptr;
    if (resolved && SafeMem_Read(resolved, &obj, sizeof(obj)) && obj && SafeMem_IsReadable(obj, 32)) {
        lua_pushinteger(L, static_cast<lua_Integer>(reinterpret_cast<uintptr_t>(obj)));
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int SoftObject_TypeName_Impl(lua_State* L) {
    lua_pushstring(L, "FSoftObjectPath");
    (void)L;
    return 1;
}

static int SoftObject_Index(lua_State* L) {
    auto* soft = static_cast<LuaSoftObjectData*>(luaL_checkudata(L, 1, SOFTOBJECT_MT));
    if (!soft) { lua_pushnil(L); return 1; }

    if (lua_isstring(L, 2)) {
        const char* key = lua_tostring(L, 2);
        if (_stricmp(key, "GetAssetPath") == 0 || _stricmp(key, "GetAssetPathName") == 0 || _stricmp(key, "ToString") == 0) {
            lua_pushcfunction(L, SoftObject_GetAssetPath_Impl);
            return 1;
        }
        if (_stricmp(key, "GetSubPathString") == 0 || _stricmp(key, "GetSubPathName") == 0) {
            lua_pushcfunction(L, SoftObject_GetSubPathString_Impl);
            return 1;
        }
        if (_stricmp(key, "GetObjectID") == 0) {
            lua_pushcfunction(L, SoftObject_GetObjectID_Impl);
            return 1;
        }
        if (_stricmp(key, "type") == 0) {
            lua_pushcfunction(L, SoftObject_TypeName_Impl);
            return 1;
        }
        if (_stricmp(key, "Get") == 0 || _stricmp(key, "ResolveObject") == 0) {
            lua_pushcfunction(L, SoftObject_Get_Impl);
            return 1;
        }
        if (_stricmp(key, "Load") == 0 || _stricmp(key, "LoadSynchronous") == 0) {
            lua_pushcfunction(L, SoftObject_LoadSynchronous_Impl);
            return 1;
        }
        if (_stricmp(key, "IsValid") == 0) {
            lua_pushcfunction(L, SoftObject_IsValid_Impl);
            return 1;
        }
        if (_stricmp(key, "IsNull") == 0 || _stricmp(key, "IsEmpty") == 0) {
            lua_pushcfunction(L, SoftObject_IsNull_Impl);
            return 1;
        }
        if (_stricmp(key, "GetWeakPtr") == 0 || _stricmp(key, "GetWeakObjectPtr") == 0) {
            lua_pushcfunction(L, SoftObject_GetWeakPtr_Impl);
            return 1;
        }
        if (_stricmp(key, "GetTagAtLastTest") == 0) {
            lua_pushcfunction(L, SoftObject_GetTagAtLastTest_Impl);
            return 1;
        }
        if (_stricmp(key, "AssetPathName") == 0 || _stricmp(key, "AssetPath") == 0) {
            lua_pushstring(L, soft->AssetPath);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int SoftObject_ToString(lua_State* L) {
    auto* soft = static_cast<LuaSoftObjectData*>(luaL_checkudata(L, 1, SOFTOBJECT_MT));
    if (!soft || !soft->AssetPath[0]) { lua_pushstring(L, "FSoftObjectPath(null)"); return 1; }
    char buf[300];
    snprintf(buf, sizeof(buf), "FSoftObjectPath(\"%s\")", soft->AssetPath);
    lua_pushstring(L, buf);
    return 1;
}

static int SoftObject_Eq(lua_State* L) {
    auto* a = static_cast<LuaSoftObjectData*>(luaL_testudata(L, 1, SOFTOBJECT_MT));
    auto* b = static_cast<LuaSoftObjectData*>(luaL_testudata(L, 2, SOFTOBJECT_MT));
    if (a && b) {
        lua_pushboolean(L, _stricmp(a->AssetPath, b->AssetPath) == 0 ? 1 : 0);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int SoftObject_Constructor(lua_State* L) {
    const char* path = luaL_optstring(L, 1, "");
    PushSoftObject(L, nullptr, path);
    return 1;
}

static int FVector_Constructor(lua_State* L) {
    float x = static_cast<float>(luaL_optnumber(L, 1, 0.0));
    float y = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    float z = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    PushVector(L, x, y, z);
    return 1;
}

static int FRotator_Constructor(lua_State* L) {
    float p = static_cast<float>(luaL_optnumber(L, 1, 0.0));
    float y = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    float r = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    PushRotator(L, p, y, r);
    return 1;
}

static int FTransform_Constructor(lua_State* L) {
    Transform3D t;
    GetRotator(L, 1, t.Rotation);
    GetVector(L, 2, t.Translation);
    if (lua_gettop(L) >= 3) GetVector(L, 3, t.Scale3D);
    PushTransform(L, t);
    return 1;
}

static int FLinearColor_Constructor(lua_State* L) {
    float r = static_cast<float>(luaL_optnumber(L, 1, 0.0));
    float g = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    float b = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    float a = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    PushColor(L, r, g, b, a);
    return 1;
}

void PushVector(lua_State* L, float x, float y, float z) {
    auto* vec = static_cast<Vector3D*>(lua_newuserdata(L, sizeof(Vector3D)));
    vec->X = x; vec->Y = y; vec->Z = z;
    luaL_setmetatable(L, VECTOR_MT);
}

void PushRotator(lua_State* L, float pitch, float yaw, float roll) {
    auto* rot = static_cast<Rotator3D*>(lua_newuserdata(L, sizeof(Rotator3D)));
    rot->Pitch = pitch; rot->Yaw = yaw; rot->Roll = roll;
    luaL_setmetatable(L, ROTATOR_MT);
}

void PushTransform(lua_State* L, const Transform3D& transform) {
    auto* trans = static_cast<Transform3D*>(lua_newuserdata(L, sizeof(Transform3D)));
    *trans = transform;
    luaL_setmetatable(L, TRANSFORM_MT);
}

void PushColor(lua_State* L, float r, float g, float b, float a) {
    auto* col = static_cast<LinearColor4D*>(lua_newuserdata(L, sizeof(LinearColor4D)));
    col->R = r; col->G = g; col->B = b; col->A = a;
    luaL_setmetatable(L, COLOR_MT);
}

void PushTArray(lua_State* L, void* arrayPtr, void* innerProp, const char* typeName,
                int32_t elementSize, LuaCallFrame* frame) {
    auto* arrData = static_cast<LuaTArrayData*>(lua_newuserdata(L, sizeof(LuaTArrayData)));
    arrData->ArrayPtr    = arrayPtr;
    arrData->InnerProperty = innerProp;
    arrData->ElementSize = elementSize;
    arrData->Offset      = 0;
    arrData->Frame       = frame;
    if (typeName) {
        strncpy(arrData->TypeName, typeName, sizeof(arrData->TypeName) - 1);
        arrData->TypeName[sizeof(arrData->TypeName) - 1] = '\0';
    }
    luaL_setmetatable(L, TARRAY_MT);
    if (frame) frame->Retain();
}

void PushTMap(lua_State* L, void* mapPtr, void* keyProp, void* valProp, const char* keyTypeName,
              const char* valTypeName, int32_t keySize, int32_t valSize, LuaCallFrame* frame) {
    auto* mapData = static_cast<LuaTMapData*>(lua_newuserdata(L, sizeof(LuaTMapData)));
    mapData->MapPtr = mapPtr;
    mapData->KeyProperty = keyProp;
    mapData->ValueProperty = valProp;
    mapData->KeySize = keySize;
    mapData->ValueSize = valSize;
    mapData->Frame = frame;
    if (keyTypeName) {
        strncpy(mapData->KeyTypeName, keyTypeName, sizeof(mapData->KeyTypeName) - 1);
        mapData->KeyTypeName[sizeof(mapData->KeyTypeName) - 1] = '\0';
    }
    if (valTypeName) {
        strncpy(mapData->ValueTypeName, valTypeName, sizeof(mapData->ValueTypeName) - 1);
        mapData->ValueTypeName[sizeof(mapData->ValueTypeName) - 1] = '\0';
    }
    luaL_setmetatable(L, TMAP_MT);
    if (frame) frame->Retain();
}

void PushTSet(lua_State* L, void* setPtr, void* elemProp, const char* elemTypeName, int32_t elemSize,
              LuaCallFrame* frame) {
    auto* setData = static_cast<LuaTSetData*>(lua_newuserdata(L, sizeof(LuaTSetData)));
    setData->SetPtr = setPtr;
    setData->ElementProperty = elemProp;
    setData->ElementSize = elemSize;
    setData->Frame = frame;
    if (elemTypeName) {
        strncpy(setData->ElementTypeName, elemTypeName, sizeof(setData->ElementTypeName) - 1);
        setData->ElementTypeName[sizeof(setData->ElementTypeName) - 1] = '\0';
    }
    luaL_setmetatable(L, TSET_MT);
    if (frame) frame->Retain();
}

static int TArray_Gc(lua_State* L) {
    auto* d = static_cast<LuaTArrayData*>(luaL_testudata(L, 1, TARRAY_MT));
    if (d && d->Frame) { LuaCallFrame* frame = d->Frame; d->Frame = nullptr; frame->Release(); }
    return 0;
}

static int TMap_Gc(lua_State* L) {
    auto* d = static_cast<LuaTMapData*>(luaL_testudata(L, 1, TMAP_MT));
    if (d && d->Frame) { LuaCallFrame* frame = d->Frame; d->Frame = nullptr; frame->Release(); }
    return 0;
}

static int TSet_Gc(lua_State* L) {
    auto* d = static_cast<LuaTSetData*>(luaL_testudata(L, 1, TSET_MT));
    if (d && d->Frame) { LuaCallFrame* frame = d->Frame; d->Frame = nullptr; frame->Release(); }
    return 0;
}

void PushDelegate(lua_State* L, void* delegatePtr, bool isMulticast) {
    PushDelegate(L, delegatePtr, nullptr, "", isMulticast);
}

void PushDelegate(lua_State* L, void* delegatePtr, void* ownerObj, const char* delName, bool isMulticast) {
    auto* delData = static_cast<LuaFDelegateData*>(lua_newuserdata(L, sizeof(LuaFDelegateData)));
    delData->DelegatePtr = delegatePtr;
    delData->OwnerObject = ownerObj;
    delData->IsMulticast = isMulticast;
    if (delName) {
        strncpy(delData->DelegateName, delName, sizeof(delData->DelegateName) - 1);
        delData->DelegateName[sizeof(delData->DelegateName) - 1] = '\0';
    } else {
        delData->DelegateName[0] = '\0';
    }
    luaL_setmetatable(L, DELEGATE_MT);
}

void PushSoftObject(lua_State* L, void* softPtr, const char* assetPath) {
    auto* softData = static_cast<LuaSoftObjectData*>(lua_newuserdata(L, sizeof(LuaSoftObjectData)));
    softData->SoftPtr = softPtr;
    if (assetPath) {
        strncpy(softData->AssetPath, assetPath, sizeof(softData->AssetPath) - 1);
        softData->AssetPath[sizeof(softData->AssetPath) - 1] = '\0';
    } else {
        softData->AssetPath[0] = '\0';
    }
    luaL_setmetatable(L, SOFTOBJECT_MT);
}

bool GetVector(lua_State* L, int index, Vector3D& outVec) {
    if (luaL_testudata(L, index, VECTOR_MT)) {
        outVec = *static_cast<Vector3D*>(lua_touserdata(L, index));
        return true;
    }
    if (lua_istable(L, index)) {
        lua_getfield(L, index, "X");
        lua_getfield(L, index, "Y");
        lua_getfield(L, index, "Z");
        if (lua_isnumber(L, -3) && lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
            outVec.X = static_cast<float>(lua_tonumber(L, -3));
            outVec.Y = static_cast<float>(lua_tonumber(L, -2));
            outVec.Z = static_cast<float>(lua_tonumber(L, -1));
            lua_pop(L, 3);
            return true;
        }
        lua_pop(L, 3);
    }
    return false;
}

bool GetRotator(lua_State* L, int index, Rotator3D& outRot) {
    if (luaL_testudata(L, index, ROTATOR_MT)) {
        outRot = *static_cast<Rotator3D*>(lua_touserdata(L, index));
        return true;
    }
    if (lua_istable(L, index)) {
        lua_getfield(L, index, "Pitch");
        lua_getfield(L, index, "Yaw");
        lua_getfield(L, index, "Roll");
        if (lua_isnumber(L, -3) && lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
            outRot.Pitch = static_cast<float>(lua_tonumber(L, -3));
            outRot.Yaw   = static_cast<float>(lua_tonumber(L, -2));
            outRot.Roll  = static_cast<float>(lua_tonumber(L, -1));
            lua_pop(L, 3);
            return true;
        }
        lua_pop(L, 3);
    }
    return false;
}

bool GetColor(lua_State* L, int index, LinearColor4D& outCol) {
    if (luaL_testudata(L, index, COLOR_MT)) {
        outCol = *static_cast<LinearColor4D*>(lua_touserdata(L, index));
        return true;
    }
    return false;
}

void RegisterLuaTypes(lua_State* L) {

    luaL_newmetatable(L, VECTOR_MT);
    lua_pushcfunction(L, Vector_Index);    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, Vector_NewIndex); lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, Vector_Add);      lua_setfield(L, -2, "__add");
    lua_pushcfunction(L, Vector_Sub);      lua_setfield(L, -2, "__sub");
    lua_pushcfunction(L, Vector_Mul);      lua_setfield(L, -2, "__mul");
    lua_pushcfunction(L, Vector_Div);      lua_setfield(L, -2, "__div");
    lua_pushcfunction(L, Vector_Unm);      lua_setfield(L, -2, "__unm");
    lua_pushcfunction(L, Vector_Eq);       lua_setfield(L, -2, "__eq");
    lua_pushcfunction(L, Vector_ToString); lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    luaL_newmetatable(L, ROTATOR_MT);
    lua_pushcfunction(L, Rotator_Index);    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, Rotator_NewIndex); lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, Rotator_Add);      lua_setfield(L, -2, "__add");
    lua_pushcfunction(L, Rotator_Sub);      lua_setfield(L, -2, "__sub");
    lua_pushcfunction(L, Rotator_ToString); lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    luaL_newmetatable(L, TRANSFORM_MT);
    lua_pushcfunction(L, Transform_Index);    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, Transform_NewIndex); lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, Transform_ToString); lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    luaL_newmetatable(L, COLOR_MT);
    lua_pushcfunction(L, Color_Index);    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, Color_NewIndex); lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, Color_Add);      lua_setfield(L, -2, "__add");
    lua_pushcfunction(L, Color_Sub);      lua_setfield(L, -2, "__sub");
    lua_pushcfunction(L, Color_Mul);      lua_setfield(L, -2, "__mul");
    lua_pushcfunction(L, Color_ToString); lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    luaL_newmetatable(L, TARRAY_MT);
    lua_pushcfunction(L, TArray_Num_Impl); lua_setfield(L, -2, "__len");
    lua_pushcfunction(L, TArray_Index);    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, TArray_ToString); lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, TArray_Gc);       lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    luaL_newmetatable(L, TMAP_MT);
    lua_pushcfunction(L, TMap_Num_Impl);  lua_setfield(L, -2, "__len");
    lua_pushcfunction(L, TMap_Index);     lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, TMap_ToString);  lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, TMap_Gc);        lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    luaL_newmetatable(L, TSET_MT);
    lua_pushcfunction(L, TSet_Num_Impl);  lua_setfield(L, -2, "__len");
    lua_pushcfunction(L, TSet_Index);     lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, TSet_ToString);  lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, TSet_Gc);        lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    luaL_newmetatable(L, DELEGATE_MT);
    lua_pushcfunction(L, Delegate_Index);          lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, Delegate_Broadcast_Impl); lua_setfield(L, -2, "__call");
    lua_pushcfunction(L, Delegate_Num_Impl);       lua_setfield(L, -2, "__len");
    lua_pushcfunction(L, Delegate_ToString);       lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    luaL_newmetatable(L, SOFTOBJECT_MT);
    lua_pushcfunction(L, SoftObject_Index);    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, SoftObject_ToString); lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, SoftObject_Eq);       lua_setfield(L, -2, "__eq");
    lua_pop(L, 1);

    lua_pushcfunction(L, FVector_Constructor);      lua_setglobal(L, "FVector");
    lua_pushcfunction(L, FVector_Constructor);      lua_setglobal(L, "Vector");
    lua_pushcfunction(L, FRotator_Constructor);     lua_setglobal(L, "FRotator");
    lua_pushcfunction(L, FRotator_Constructor);     lua_setglobal(L, "Rotator");
    lua_pushcfunction(L, FTransform_Constructor);   lua_setglobal(L, "FTransform");
    lua_pushcfunction(L, FTransform_Constructor);   lua_setglobal(L, "Transform");
    lua_pushcfunction(L, FLinearColor_Constructor); lua_setglobal(L, "FLinearColor");
    lua_pushcfunction(L, FLinearColor_Constructor); lua_setglobal(L, "FColor");
    lua_pushcfunction(L, SoftObject_Constructor);   lua_setglobal(L, "FSoftObjectPath");
    lua_pushcfunction(L, SoftObject_Constructor);   lua_setglobal(L, "SoftObjectPath");
    lua_pushcfunction(L, SoftObject_Constructor);   lua_setglobal(L, "TSoftObjectPtr");
    lua_pushcfunction(L, Delegate_Constructor);     lua_setglobal(L, "FDelegate");
    lua_pushcfunction(L, Delegate_Constructor);     lua_setglobal(L, "Delegate");
    lua_pushcfunction(L, Delegate_Constructor);     lua_setglobal(L, "MulticastDelegate");
    lua_pushcfunction(L, Delegate_Constructor);     lua_setglobal(L, "FMulticastScriptDelegate");

    lua_newtable(L);
    lua_pushcfunction(L, Math_Lerp);               lua_setfield(L, -2, "Lerp");
    lua_pushcfunction(L, Math_Clamp);              lua_setfield(L, -2, "Clamp");
    lua_pushcfunction(L, Math_VInterpTo);          lua_setfield(L, -2, "VInterpTo");
    lua_pushcfunction(L, Math_RInterpTo);          lua_setfield(L, -2, "RInterpTo");
    lua_pushcfunction(L, Math_FindLookAtRotation); lua_setfield(L, -2, "FindLookAtRotation");
    lua_setglobal(L, "Math");
}

static void PushSharedVariablesTable(lua_State* L) {
    lua_getglobal(L, "_SharedVariables");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "_SharedVariables");
    }
}

static void ResolveSharedVariableArgs(lua_State* L, int& nameIdx, int& valueIdx) {
    if ((lua_istable(L, 1) || lua_isfunction(L, 1)) && lua_isstring(L, 2)) {
        nameIdx = 2;
        valueIdx = 3;
    } else {
        nameIdx = 1;
        valueIdx = 2;
    }
}

static int ModRef_SetSharedVariable(lua_State* L) {
    int nameIdx = 1, valueIdx = 2;
    ResolveSharedVariableArgs(L, nameIdx, valueIdx);
    const char* name = luaL_optstring(L, nameIdx, "");
    if (!name || !name[0]) { lua_pushboolean(L, 0); return 1; }

    PushSharedVariablesTable(L);
    lua_pushstring(L, name);
    lua_pushvalue(L, valueIdx);
    lua_settable(L, -3);
    lua_pop(L, 1);
    lua_pushboolean(L, 1);
    return 1;
}

static int ModRef_GetSharedVariable(lua_State* L) {
    int nameIdx = 1, valueIdx = 2;
    ResolveSharedVariableArgs(L, nameIdx, valueIdx);
    (void)valueIdx;
    const char* name = luaL_optstring(L, nameIdx, "");
    if (!name || !name[0]) { lua_pushnil(L); return 1; }

    PushSharedVariablesTable(L);
    lua_pushstring(L, name);
    lua_rawget(L, -2);
    lua_remove(L, -2);
    return 1;
}

static void InstallSharedVariableMethods(lua_State* L, int tableIdx) {
    if (tableIdx < 0) tableIdx = lua_gettop(L) + tableIdx + 1;
    lua_pushcfunction(L, ModRef_SetSharedVariable);
    lua_setfield(L, tableIdx, "SetSharedVariable");
    lua_pushcfunction(L, ModRef_GetSharedVariable);
    lua_setfield(L, tableIdx, "GetSharedVariable");
}

void RegisterCompatibilityLayer(lua_State* L) {

    {
        std::string modsDir = LuaManager::Get().GetModsDirectory();
        if (modsDir.empty()) modsDir = "Mods";
        std::string sharedRoot = modsDir + "/shared";
        std::replace(sharedRoot.begin(), sharedRoot.end(), '\\', '/');
        lua_pushstring(L, sharedRoot.c_str());
        lua_setglobal(L, "__SharedModuleRoot");
    }

    lua_pushcfunction(L, [](lua_State* L) -> int {

        const char* modName = luaL_optstring(L, 1, "");
        if (!modName || !modName[0]) {
            lua_newtable(L);
            InstallSharedVariableMethods(L, -1);
            return 1;
        }

        lua_getglobal(L, "_ModRegistry");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setglobal(L, "_ModRegistry");
        }

        lua_getfield(L, -1, modName);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setfield(L, -3, modName);
        }

        lua_remove(L, -2);
        InstallSharedVariableMethods(L, -1);
        return 1;
    });
    lua_setglobal(L, "ModRef");

    lua_getglobal(L, "ModRef");
    lua_setglobal(L, "GetModRef");

    const char* helperScript = R"lua(
local H = {}
local G = _G

H.GetPlayerController = G.GetPlayerController or G.GetFirstPlayerController
H.GetPlayer = G.GetPlayer or G.GetPlayerCharacter or G.GetPlayerPawn
H.GetPlayerCharacter = H.GetPlayer
H.GetPlayerPawn = H.GetPlayer
H.GetWorld = G.GetWorld
H.GetEngine = G.GetEngine
H.GetGameInstance = G.GetGameInstance
H.GetGameMode = G.GetGameMode
H.GetGameState = G.GetGameState
H.GetCameraManager = G.GetCameraManager or G.GetPlayerCameraManager
H.GetLocalPlayer = G.GetLocalPlayer

H.GetGameplayStatics = G.GetGameplayStatics
H.GetKismetSystemLibrary = G.GetKismetSystemLibrary
H.GetKismetMathLibrary = G.GetKismetMathLibrary
H.GetKismetStringLibrary = G.GetKismetStringLibrary
H.GetKismetArrayLibrary = G.GetKismetArrayLibrary
H.GetKismetTextLibrary = G.GetKismetTextLibrary
H.GetKismetGuidLibrary = G.GetKismetGuidLibrary
H.GetKismetRenderingLibrary = G.GetKismetRenderingLibrary

H.FindFirstOf = G.FindFirstOf
H.FindAllOf = G.FindAllOf
H.FindObject = G.FindObject
H.FindObjects = G.FindObjects
H.StaticFindObject = G.StaticFindObject or G.FindObject
H.StaticClass = G.StaticClass or G.FindClass
H.FindClass = G.FindClass
H.StaticStruct = G.StaticStruct
H.StaticEnum = G.StaticEnum
H.StaticConstructObject = G.StaticConstructObject or G.NewObject
H.ConstructObject = H.StaticConstructObject
H.NewObject = H.StaticConstructObject
H.SpawnActor = G.SpawnActor
H.LoadAsset = G.LoadAsset or G.StaticLoadObject

H.GetDistance = G.GetDistance
H.GetFrameTime = G.GetFrameTime
H.GetFPS = G.GetFPS
H.GetDeltaSeconds = G.GetDeltaSeconds
H.GetMemoryUsage = G.GetMemoryUsage
H.ExecuteConsoleCommand = G.ExecuteConsoleCommand or G.ExecuteCommand
H.ExecuteCommand = H.ExecuteConsoleCommand
H.GetCVar = G.GetCVar
H.SetCVar = G.SetCVar
H.ModRef = G.ModRef

H.Vector = G.Vector
H.FVector = G.FVector
H.Rotator = G.Rotator
H.FRotator = G.FRotator
H.Transform = G.Transform
H.FTransform = G.FTransform
H.Color = G.Color
H.FColor = G.FColor
H.FLinearColor = G.FLinearColor
H.FSoftObjectPath = G.FSoftObjectPath
H.SoftObjectPath = G.SoftObjectPath
H.TSoftObjectPtr = G.TSoftObjectPtr

-- The table above is the floor, not the ceiling: an installation that ships the
-- full helper module in the shared tree gets that module's surface layered on top,
-- so the helpers a script expects are the ones actually loaded while the built-in
-- names still answer when no shared copy is present.
--
-- The overlay runs on first require rather than here, because the shared module is
-- written against the whole surface and would otherwise run before all of it has
-- been registered.
local SharedMerged = false
local function MergeSharedHelpers()
    if SharedMerged then return end
    SharedMerged = true

    local Root = _G.__SharedModuleRoot
    if type(Root) ~= "string" or Root == "" then return end

    local Chunk = loadfile(Root .. "/UEHelpers/UEHelpers.lua")
    if not Chunk then
        Chunk = loadfile(Root .. "/UEHelpers.lua")
    end
    if not Chunk then return end

    local Ok, Shared = pcall(Chunk)
    if not Ok or type(Shared) ~= "table" then return end

    for Key, Value in pairs(Shared) do
        if Value ~= nil then H[Key] = Value end
    end
    _G.__SharedHelpersMerged = true
end

package.preload["UEHelpers"] = function() MergeSharedHelpers() return H end
package.preload["UEHelpers.UEHelpers"] = function() MergeSharedHelpers() return H end
)lua";

    if (luaL_loadbuffer(L, helperScript, strlen(helperScript), "@builtin/UEHelpers.lua") != LUA_OK ||
        lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        EngineDiscovery::WriteLogLine(("[Lua] Compatibility layer setup error: " + std::string(err ? err : "") + "\n").c_str());
        lua_pop(L, 1);
    }
}

}
