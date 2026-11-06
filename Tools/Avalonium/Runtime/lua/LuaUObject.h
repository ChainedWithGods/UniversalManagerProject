#pragma once

#include "../core/Types.h"
#include <string>
#include <cstdint>

struct lua_State;

namespace LuaEngine {
struct Vector3D;
struct Rotator3D;
}

namespace LuaEngine {

struct LuaUObjectData {
    UObject* Object = nullptr;
};

struct FoundPropertyEx {
    void* Prop = nullptr;
    std::string TypeName;
    int32_t Offset = -1;
    int32_t ElementSize = 0;
    void* InnerProp = nullptr;
    void* KeyProp = nullptr;
    void* ValueProp = nullptr;
    uint8_t ByteOffset = 0;
    uint8_t ByteMask = 1;
    uint8_t FieldMask = 1;
    bool Found = false;
    uint64_t Flags = 0;
};

void RegisterLuaUObject(lua_State* L);
void PushUObject(lua_State* L, UObject* object);
UObject* GetUObject(lua_State* L, int index);

FoundPropertyEx FindPropertyOnClassEx(UClass* cls, const std::string& propName);

void* FindFunctionOnClassEx(UClass* cls, const std::string& funcName);

FoundPropertyEx DescribePropertyEx(void* prop);

void PushPropertyView(lua_State* L, uint8_t* baseAddr, const FoundPropertyEx& prop, const char* name);

int PushPropertyValueEx(lua_State* L, uint8_t* baseAddr, const FoundPropertyEx& prop);

bool SetPropertyValueEx(lua_State* L, uint8_t* baseAddr, const FoundPropertyEx& prop, int valIdx);

int CallUFunctionDynamic(lua_State* L, UObject* object, void* ufunc, int luaArgStart = 2);

UObject* SpawnActorThroughEngineApi(UObject* world, UClass* actorClass,
                                    const LuaEngine::Vector3D& loc,
                                    const LuaEngine::Rotator3D& rot,
                                    std::string* why);

int32_t CompatRowMapOffset(UObject* dataTable);

void* CompatRowStruct(UObject* dataTable);

int32_t CompatRowMapStride(uint8_t* rowMapData, int32_t count);

void CompatPushRowTable(lua_State* L, void* rowStruct, uint8_t* rowBuf);

}
