#pragma once

#include "Types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace EngineDiscovery {

struct ReflectedMember {
    void *Property = nullptr;
    std::string Name;
    std::string TypeName;
    int32_t Offset = 0;
    int32_t ElementSize = 0;
    int32_t ArrayDim = 1;
    uint64_t Flags = 0;
    void *Struct = nullptr;
};

bool GetReflectedMembers(const EngineState &state, UStruct *owner,
                         std::vector<ReflectedMember> &out);

struct FNameBytes {
    uint8_t Bytes[16] = {};
    int32_t Size = 0;
};

bool FindOrAddFName(EngineState &state, const std::string &text, FNameBytes &out,
                    std::string *detail = nullptr);

int32_t FindOrAddFNameIndex(EngineState &state, const std::string &text,
                            std::string *detail = nullptr);

bool WriteFNameBytes(EngineState &state, uint32_t index, uint32_t number, void *dest,
                     int32_t width);

bool CanCreateFNames(EngineState &state);

int32_t GetFunctionFrameSize(const EngineState &state, void *ufunction);

int32_t GetFunctionReturnValueOffset(const EngineState &state, void *ufunction);

bool GetFunctionParameter(EngineState &state, const char *className,
                          const char *functionName, const char *parameterName,
                          ReflectedMember &out);

struct ReflectedArgument {
    const char *Parameter;
    const void *Value;
    int32_t Size;
};

bool CallStaticFunctionByName(EngineState &state, const char *className,
                              const char *functionName, const ReflectedArgument *args,
                              int32_t argCount, void *result, int32_t resultSize);

UObject *SpawnObjectThroughEngine(EngineState &state, UClass *objectClass, UObject *outer);

struct AssetDataField {
    std::string Name;
    std::string TypeName;
    void *Property = nullptr;
    int32_t Offset = 0;
    int32_t Size = 0;
};

struct AssetRegistryBinding {
    bool bReady = false;
    std::string Detail;

    UObject *HelpersClass = nullptr;
    UObject *HelpersCDO = nullptr;
    UObject *Registry = nullptr;

    UStruct *AssetDataStruct = nullptr;
    std::string AssetDataStructName;
    int32_t AssetDataSize = 0;
    int32_t AssetDataOffset = 0;
    int32_t GetAssetFrameSize = 0;

    void *FnGetAsset = nullptr;
    void *FnGetAssetRegistry = nullptr;
    void *FnGetClass = nullptr;
    void *FnIsValid = nullptr;
    void *FnCreateAssetData = nullptr;

    std::vector<AssetDataField> Fields;

    int32_t OffObjectPath = -1;
    int32_t OffPackageName = -1;
    int32_t OffPackagePath = -1;
    int32_t OffAssetName = -1;
    int32_t OffAssetClass = -1;
    int32_t OffAssetClassPath = -1;
};

const AssetRegistryBinding &GetAssetRegistryBinding(EngineState &state);

bool BuildAssetData(EngineState &state, const AssetRegistryBinding &binding,
                    const std::string &packagePath, const std::string &assetName,
                    uint8_t *buffer, int32_t size, std::string *detail = nullptr);

std::string DescribeAssetRegistryBinding(const AssetRegistryBinding &binding);

UObject *LoadAssetByPath(EngineState &state, const std::string &objectPath,
                         std::string *detail = nullptr);

UObject *FindClassDefaultObject(EngineState &state, UClass *cls);

bool IsBlueprintClass(const EngineState &state, UClass *cls);

UClass *GetBlueprintGeneratedClassObject(EngineState &state);

std::vector<UClass *> FindBlueprintClasses(EngineState &state,
                                           const std::string &superClassName);

}
