#pragma once

#include "../core/Types.h"
#include <cstdint>

struct lua_State;

namespace LuaEngine {

constexpr int COMPAT_API_VERSION_MAJOR = 3;
constexpr int COMPAT_API_VERSION_MINOR = 5;

void RegisterCompatApi(lua_State* L);

void PublishCompatTypeRecords(lua_State* L);

int UObj_HasAllFlags(lua_State* L);
int UObj_HasAnyFlags(lua_State* L);
int UObj_HasAnyInternalFlags(lua_State* L);
int UObj_IsClass(lua_State* L);
int UObj_GetPropertyByName(lua_State* L);
int UObj_SetPropertyByName(lua_State* L);
int UObj_Reflection(lua_State* L);

void PushFName(lua_State* L, uint32_t comparisonIndex, uint32_t number = 0);
int FName_IsValid(lua_State* L);

int GetFNameHandle(lua_State* L, int index, uint32_t* comparisonIndex, uint32_t* number);

void PushFTextRef(lua_State* L, void* textAddr);

void PushFTextValue(lua_State* L, const char* utf8);

bool GetLuaFTextString(lua_State* L, int idx, std::string& out);

int Compat_ForEachUObjectIndexed(lua_State* L);

void RegisterCompatParityApi(lua_State* L);

int CompatPushInvalidObject(lua_State* L);

bool FindCustomPropertyForClass(UClass* cls, const std::string& key, void** outProp,
                                int32_t* outOffset, int32_t* outElementSize, std::string& outTypeName);

int PushCustomPropertyValue(lua_State* L, uint8_t* baseAddr, int32_t offset,
                           int32_t elementSize, const std::string& typeName);

bool SetCustomPropertyValue(lua_State* L, uint8_t* baseAddr, int32_t offset,
                           int32_t elementSize, const std::string& typeName, int valIdx);

void CompatPushWeakObjectPtr(lua_State* L, void* obj);

void CompatPushPropertyWrapper(lua_State* L, void* prop, void* owner);

int Compat_UObj_ForEachProperty(lua_State* L);
int Compat_UObj_ForEachFunction(lua_State* L);

int Compat_UObj_GetFunctionFlags(lua_State* L);
int Compat_UObj_SetFunctionFlags(lua_State* L);

int Compat_UObj_EnumGetNameByValue(lua_State* L);
int Compat_UObj_EnumForEachName(lua_State* L);
int Compat_UObj_EnumGetNameByIndex(lua_State* L);

int Compat_UObj_EnumEditNameAt(lua_State* L);
int Compat_UObj_EnumEditValueAt(lua_State* L);
int Compat_UObj_EnumInsertIntoNames(lua_State* L);
int Compat_UObj_EnumRemoveFromNamesAt(lua_State* L);

int Compat_UObj_DataTableGetRowMap(lua_State* L);
int Compat_UObj_DataTableGetAllRows(lua_State* L);
int Compat_UObj_DataTableGetRowStruct(lua_State* L);
int Compat_UObj_DataTableAddRow(lua_State* L);
int Compat_UObj_DataTableRemoveRow(lua_State* L);
int Compat_UObj_DataTableEmptyTable(lua_State* L);

int Compat_UObj_GetLevel(lua_State* L);

int Compat_UObj_Len(lua_State* L);

int32_t CompatVerifiedFieldNextOffset();

std::string DetectCurrentModName(lua_State* L);

void RegisterCompatSurface(lua_State* L);

void CompatPushOutputDevice(lua_State* L);

bool CompatStringFromArg(lua_State* L, int idx, std::string& out);

bool CompatIsStructFamilyObject(UObject* obj);

int Compat_Surface_Index(lua_State* L);

}
