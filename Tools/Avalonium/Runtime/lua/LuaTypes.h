#pragma once

#include "../core/Types.h"

#include <atomic>

struct lua_State;

namespace LuaEngine {

struct LuaCallFrame {
    std::vector<uint64_t> Block;
    std::atomic<int> References{1};

    void Retain() { References.fetch_add(1, std::memory_order_relaxed); }
    void Release() {
        if (References.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this;
    }
};

LuaCallFrame* CreateCallFrame(std::vector<uint64_t>&& block);

struct Vector3D {
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
};

struct Rotator3D {
    float Pitch = 0.0f;
    float Yaw = 0.0f;
    float Roll = 0.0f;
};

struct Transform3D {
    Rotator3D Rotation;
    Vector3D Translation;
    Vector3D Scale3D = { 1.0f, 1.0f, 1.0f };
};

struct LinearColor4D {
    float R = 0.0f;
    float G = 0.0f;
    float B = 0.0f;
    float A = 1.0f;
};

struct LuaTArrayData {
    void* ArrayPtr = nullptr;
    void* InnerProperty = nullptr;
    int32_t ElementSize = 0;
    int32_t Offset = 0;
    char TypeName[64] = {0};
    LuaCallFrame* Frame = nullptr;
};

struct LuaTMapData {
    void* MapPtr = nullptr;
    void* KeyProperty = nullptr;
    void* ValueProperty = nullptr;
    int32_t KeySize = 0;
    int32_t ValueSize = 0;
    char KeyTypeName[64] = {0};
    char ValueTypeName[64] = {0};
    LuaCallFrame* Frame = nullptr;
};

struct LuaTSetData {
    void* SetPtr = nullptr;
    void* ElementProperty = nullptr;
    int32_t ElementSize = 0;
    char ElementTypeName[64] = {0};
    LuaCallFrame* Frame = nullptr;
};

struct LuaFDelegateData {
    void* DelegatePtr = nullptr;
    void* OwnerObject = nullptr;
    char DelegateName[64] = {0};
    bool IsMulticast = false;
};

struct LuaSoftObjectData {
    void* SoftPtr = nullptr;
    char AssetPath[260] = {0};
};

void RegisterLuaTypes(lua_State* L);
void RegisterCompatibilityLayer(lua_State* L);

void PushVector(lua_State* L, float x, float y, float z);
void PushRotator(lua_State* L, float pitch, float yaw, float roll);
void PushTransform(lua_State* L, const Transform3D& transform);
void PushColor(lua_State* L, float r, float g, float b, float a);

void PushTArray(lua_State* L, void* arrayPtr, void* innerProp, const char* typeName,
                int32_t elementSize, LuaCallFrame* frame = nullptr);
void PushTMap(lua_State* L, void* mapPtr, void* keyProp, void* valProp, const char* keyTypeName,
              const char* valTypeName, int32_t keySize, int32_t valSize,
              LuaCallFrame* frame = nullptr);
void PushTSet(lua_State* L, void* setPtr, void* elemProp, const char* elemTypeName, int32_t elemSize,
              LuaCallFrame* frame = nullptr);
void PushDelegate(lua_State* L, void* delegatePtr, bool isMulticast);
void PushDelegate(lua_State* L, void* delegatePtr, void* ownerObj, const char* delName, bool isMulticast);
void PushSoftObject(lua_State* L, void* softPtr, const char* assetPath);

bool GetVector(lua_State* L, int index, Vector3D& outVec);
bool GetRotator(lua_State* L, int index, Rotator3D& outRot);
bool GetColor(lua_State* L, int index, LinearColor4D& outCol);

}
