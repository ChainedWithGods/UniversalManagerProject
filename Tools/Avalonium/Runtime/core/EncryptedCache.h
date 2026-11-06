#pragma once

#include "Types.h"
#include "PatternScanner.h"
#include <string>
#include <vector>
#include <cstdint>

namespace EncryptedCache {

#pragma pack(push, 1)
struct SnapshotHeader {
    char Magic[8] = {'U', 'D', 'C', 'A', 'C', 'H', 'E', '1'};
    uint32_t Version = 5;
    uint32_t StructSize = 0;

    uint32_t PETimeDateStamp = 0;
    uint32_t PECheckSum = 0;
    uint32_t PEImageSize = 0;
    char GameName[128] = {0};

    int32_t UEMajor = 0;
    int32_t UEMinor = 0;
};

struct SnapshotData {
    SnapshotHeader Header;

    uint64_t GUObjectArray_RVA = 0;
    uint64_t GNames_RVA = 0;
    uint64_t GWorldPtr_RVA = 0;
    uint64_t GMalloc_RVA = 0;
    char GMallocName[32] = {};

    int32_t GMallocMallocSlot = -1;
    uint64_t GNatives_RVA = 0;
    uint64_t GUObjectHashTables_RVA = 0;

    uint64_t IConsoleManagerLocation_RVA = 0;
    uint64_t FNameToString_RVA = 0;
    uint64_t StaticConstructObject_RVA = 0;
    int32_t bStaticConstructTakesParams = 1;

    int32_t UObject_FlagsOff = 0;
    int32_t UObject_IndexOff = 0;
    int32_t UObject_ClassOff = 0;
    int32_t UObject_NameOff = 0;
    int32_t UObject_OuterOff = 0;

    int32_t UStruct_SuperOff = -1;
    int32_t UStruct_ChildrenOff = -1;
    int32_t UStruct_ChildPropsOff = -1;
    int32_t UStruct_PropertiesSizeOff = -1;
    int32_t UStruct_MinAlignmentOff = -1;
    int32_t UField_NextOff = -1;

    int32_t FField_ClassOff = -1;
    int32_t FFieldClass_NameOff = -1;
    int32_t FField_NameOff = -1;
    int32_t FField_NextOff = -1;
    int32_t FProperty_OffsetOff = 0x4C;
    int32_t UProperty_OffsetOff = 0x44;

    int32_t FProperty_ArrayDimOff = -1;
    int32_t FProperty_ElementSizeOff = -1;
    int32_t FProperty_PropertyFlagsOff = -1;

    int32_t FArrayProperty_InnerOff = -1;
    int32_t FStructProperty_StructOff = -1;
    int32_t FObjectProperty_ClassOff = -1;
    int32_t FEnumProperty_UnderlyingOff = -1;
    int32_t FEnumProperty_EnumOff = -1;
    int32_t FMapProperty_KeyPropOff = -1;
    int32_t FMapProperty_ValuePropOff = -1;
    int32_t FSetProperty_ElementPropOff = -1;
    int32_t FOptionalProperty_ValuePropOff = -1;
    int32_t FByteProperty_EnumOff = -1;
    int32_t FBoolProperty_BaseOff = -1;
    int32_t FClassProperty_MetaClassOff = -1;

    int32_t UEnum_NamesOff = -1;
    uint8_t bIsNewUE5Enum = 0;
    uint8_t FName_IsIndirect = 0;
    int32_t FName_BlockOffsetBits = 16;

    int32_t UWorld_PersistentLevelOff = -1;
    int32_t ULevel_ActorsOff = -1;

    int32_t ProcessEvent_VTableIdx = -1;
    int32_t UFunction_FlagsOff = -1;

    uint64_t ProcessInternal_RVA = 0;
    uint64_t ProcessLocalScriptFunction_RVA = 0;
    int32_t UFunction_FuncOff = -1;

    int32_t FUObjectItem_Size = 24;
    int32_t FUObjectItem_ObjectOff = 0;

    int32_t TUObjectArray_ObjectsOff = 0;
    int32_t TUObjectArray_NumElementsOff = 0;
    int32_t TUObjectArray_MaxElementsOff = 0;

    int32_t FName_Size = 8;
    int32_t FName_CompIdxOff = 0;
    int32_t FName_NumberOff = 4;
    uint8_t bUseCasePreservingName = 0;
    uint8_t bUseOutlineNumber = 0;
    uint8_t bUseFProperty = 0;
    uint8_t bUseMaskForFieldOwner = 0;

    int32_t FName_ChunksStart = 0x10;
    int32_t FName_HeaderOff = 0;
    int32_t FName_Stride = 2;
    int32_t FName_LenShift = 6;
    int32_t FName_LenMask = 0x3FF;

    int32_t IConsoleManager_TMapOff = 0x08;
};
#pragma pack(pop)

std::wstring GetCacheFilePathW(HMODULE hModule, const char* gameName, bool createDir = false);

bool TryLoadSnapshot(EngineState &state, HMODULE hModule);

bool SaveSnapshot(const EngineState &state, HMODULE hModule);

}
