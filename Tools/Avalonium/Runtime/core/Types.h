#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>

typedef void UObject;
typedef void UClass;
typedef void UStruct;
typedef void UField;
typedef void UEnum;
typedef void UFunction;
typedef void FProperty;
typedef void UWorld;
typedef void AActor;
typedef void ULevel;

template<typename T>
struct TArray {
    T* Data;
    int32_t ArrayNum;
    int32_t ArrayMax;
};

struct FName {
    uint32_t ComparisonIndex;
    uint32_t Number;
};

struct FString {
    wchar_t* Data;
    int32_t ArrayNum;
    int32_t ArrayMax;

    std::string ToStdString() const {
        if (!Data || ArrayNum <= 0) return "";

        int32_t wideLen = ArrayNum;
        while (wideLen > 0 && Data[wideLen - 1] == 0) {
            --wideLen;
        }
        if (wideLen <= 0) return "";

        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, Data, wideLen, nullptr, 0, nullptr, nullptr);
        if (utf8Len <= 0) return "";

        std::string result(static_cast<size_t>(utf8Len), '\0');
        WideCharToMultiByte(CP_UTF8, 0, Data, wideLen, result.data(), utf8Len, nullptr, nullptr);
        return result;
    }
};

constexpr uint64_t kCpfParm = 0x0000000000000080ULL;
constexpr uint64_t kCpfOutParm = 0x0000000000000100ULL;
constexpr uint64_t kCpfReturnParm = 0x0000000000000400ULL;

constexpr uint32_t kFuncNative = 0x00000400u;

constexpr uint32_t kRfNoFlags             = 0x00000000u;
constexpr uint32_t kRfPublic              = 0x00000001u;
constexpr uint32_t kRfStandalone          = 0x00000002u;
constexpr uint32_t kRfMarkAsNative        = 0x00000004u;
constexpr uint32_t kRfTransactional       = 0x00000008u;
constexpr uint32_t kRfClassDefaultObject  = 0x00000010u;
constexpr uint32_t kRfArchetypeObject     = 0x00000020u;
constexpr uint32_t kRfTransient           = 0x00000040u;

constexpr uint32_t kRfObjectTemplateMask  = kRfClassDefaultObject | kRfArchetypeObject;

constexpr int32_t FFrame_NodeOff = 0x10;
constexpr int32_t FFrame_ObjectOff = 0x18;
constexpr int32_t FFrame_CodeOff = 0x20;
constexpr int32_t FFrame_LocalsOff = 0x28;

constexpr int32_t FFrame_OutParmsOff = 0x78;

constexpr int32_t FFrame_ScanFirstOff = 0x10;
constexpr int32_t FFrame_ScanLastOff = 0x100 - static_cast<int32_t>(sizeof(void*));

struct FOutParmRec {
    void* Property;
    uint8_t* PropAddr;
    FOutParmRec* NextOutParm;
};

typedef void (__fastcall *FName_ToString_t)(const FName* thisName, FString& outString);

typedef void (__fastcall *FName_AppendString_t)(const FName* thisName, FString& outString);

typedef FString (__fastcall *UObject_GetFullName_t)(const UObject* obj, const UObject* stopOuter, uint32_t flags);

typedef FString (__fastcall *UObject_GetName_t)(const UObject* obj);

typedef FString (__fastcall *UObject_GetPathName_t)(const UObject* obj, const UObject* stopOuter);

typedef bool (__fastcall *UObject_IsA_t)(const UObject* obj, const UClass* testClass);

typedef UObject* (__fastcall *StaticFindObject_t)(UClass* classToFind, UObject* package, const wchar_t* name, bool exactClass);

typedef UObject* (__fastcall *StaticLoadObject_t)(UClass* classToFind, UObject* parent, const wchar_t* name, const wchar_t* filename, uint32_t flags, void* sandbox, bool bAllowObjectReconciliation);

typedef UObject* (__fastcall *StaticConstructObject_Internal_t)(const void* Params);

#pragma pack(push, 1)
struct PropertyInfoEx {
    char Name[256];
    char TypeName[256];
    char InnerType[256];
    char KeyType[256];
    char ValueType[256];
    char ClassName[256];
    int32_t Offset;
    int32_t Size;
    int32_t ElementSize;
    int32_t ArrayDim;
    uint64_t PropertyFlags;
    bool bIsArray;
    bool bIsMap;
    bool bIsSet;
    bool bIsStruct;
    bool bIsEnum;
    bool bIsObject;
    bool bIsBool;
    bool bIsString;
};

struct ClassInfoEx {
    char Name[256];
    char FullPath[512];
    char SuperClassName[256];
    void* Address;
    int32_t PropertiesCount;
    int32_t FunctionsCount;
    int32_t Size;
};

struct EnumInfoEx {
    char Name[256];
    char FullPath[512];
    int32_t NumValues;

};
#pragma pack(pop)

constexpr const char* PIPE_NAME = "\\\\.\\pipe\\Avalonium";
constexpr int IOCP_PORT = 20030;
constexpr int MAX_STRING_RESULT = 4096;
constexpr int MAX_OBJECT_COUNT = 100000;

struct FoldedNameHash {
    std::size_t operator()(std::string_view text) const {
        std::size_t hash = 14695981039346656037ull;
        for (char ch : text) {
            unsigned char c = static_cast<unsigned char>(ch);
            if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c - 'A' + 'a');
            hash = (hash ^ c) * 1099511628211ull;
        }
        return hash;
    }
};

struct FoldedNameEquals {
    bool operator()(std::string_view a, std::string_view b) const {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            unsigned char x = static_cast<unsigned char>(a[i]);
            unsigned char y = static_cast<unsigned char>(b[i]);
            if (x >= 'A' && x <= 'Z') x = static_cast<unsigned char>(x - 'A' + 'a');
            if (y >= 'A' && y <= 'Z') y = static_cast<unsigned char>(y - 'A' + 'a');
            if (x != y) return false;
        }
        return true;
    }
};

struct EngineState {

    int UEMajor = 0;
    int UEMinor = 0;

    char GameName[256] = {0};

    HWND GameWindow = nullptr;
    void* OriginalWndProc = nullptr;

    void* GUObjectArray = nullptr;
    void* GNames = nullptr;
    void* GWorld = nullptr;
    void** GWorldPtr = nullptr;
    void* GMalloc = nullptr;

    void* GMallocLocation = nullptr;

    char GMallocName[32] = {};

    int32_t GMallocMallocSlot = -1;
    void* GNatives = nullptr;
    void* GUObjectHashTables = nullptr;
    void* GEngine = nullptr;

    UClass* UObjectClass = nullptr;
    UClass* UClassClass = nullptr;
    UClass* UStructClass = nullptr;
    UClass* AActorClass = nullptr;
    UClass* APawnClass = nullptr;
    UClass* UEnumClass = nullptr;
    UClass* UFunctionClass = nullptr;
    UClass* UPackageClass = nullptr;
    UClass* UWorldClass = nullptr;

    int32_t UObject_VTableOff = 0x00;
    int32_t UObject_FlagsOff = 0x08;
    int32_t UObject_IndexOff = 0x0C;
    int32_t UObject_ClassOff = 0x10;
    int32_t UObject_NameOff = 0x18;
    int32_t UObject_OuterOff = 0x20;

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
    int32_t UEnum_EnumFlagsOff = -1;
    int32_t UClass_ClassFlagsOff = -1;
    int32_t UScriptStruct_StructFlagsOff = -1;
    int32_t UStruct_ScriptOff = -1;
    int32_t UClass_ClassDefaultObjectOff = -1;
    int32_t UClass_InterfacesOff = -1;
    int32_t UFunction_FuncOff = -1;
    int32_t UFunction_NumParmsOff = -1;
    int32_t UFunction_ParmsSizeOff = -1;
    int32_t UFunction_ReturnValueOffsetOff = -1;
    int32_t UFunction_RPCIdOff = -1;
    int32_t UFunction_RPCResponseIdOff = -1;
    bool bIsNewUE5Enum = false;
    bool FName_IsIndirect = false;
    int FName_BlockOffsetBits = 16;

    int32_t UWorld_PersistentLevelOff = -1;
    int32_t ULevel_ActorsOff = -1;

    int32_t ProcessEvent_VTableIdx = -1;
    int32_t UFunction_FlagsOff = -1;

    void* ProcessEvent_UObjectImpl = nullptr;
    void* ProcessEvent_AActorImpl = nullptr;

    void* ProcessInternal = nullptr;
    void* ProcessLocalScriptFunction = nullptr;

    FName_ToString_t FNameToString = nullptr;
    UObject_GetFullName_t GetFullName = nullptr;
    StaticConstructObject_Internal_t StaticConstructObject = nullptr;
    bool bStaticConstructTakesParams = true;
    void* IConsoleManager = nullptr;

    void* IConsoleManagerLocation = nullptr;
    int32_t IConsoleManager_TMapOff = 0x08;

    int32_t FUObjectItem_Size = 0x18;
    int32_t FUObjectItem_ObjectOff = 0;

    int32_t TUObjectArray_ObjectsOff = 0;
    int32_t TUObjectArray_NumElementsOff = 0;
    int32_t TUObjectArray_MaxElementsOff = 0;

    int32_t UObjectArray_NumElements = 0;
    int32_t UObjectArray_NumChunks = 0;
    void** UObjectArray_Chunks = nullptr;
    int32_t UObjectArray_ItemSize = 24;

    int32_t UObjectArray_NumElementsPerChunk = 0;

    int32_t FName_Size = 8;
    int32_t FName_CompIdxOff = 0;
    int32_t FName_NumberOff = 4;
    bool bUseCasePreservingName = false;
    bool bUseOutlineNumber = false;

    bool bUseFProperty = false;

    int32_t FName_ChunksStart = 0x10;
    int32_t FName_HeaderOff = 0;
    int32_t FName_Stride = 2;
    int32_t FName_LenShift = 6;
    int32_t FName_LenMask = 0x3FF;

    bool bUseMaskForFieldOwner = false;

    bool bInitialized = false;
    bool bGlobalsFound = false;
    bool bFunctionsFound = false;
    bool bCoreClassesFound = false;
    bool bOffsetsDiscovered = false;
    bool bFNameLayoutDiscovered = false;
    bool bIndexBuilt = false;
    bool bUnsupportedVersion = false;

    std::unordered_map<int32_t, UObject*> NameToFirstObject;
    std::unordered_map<int32_t, std::vector<UObject*>> NameToObjects;
    std::unordered_map<UClass*, std::vector<UObject*>> ClassToInstances;
    std::unordered_map<int32_t, std::string> FNameCache;

    uint64_t LiveLookupGeneration = 0;
    uint64_t LiveLookupCacheStamp = 0;
    std::unordered_map<UClass*, std::vector<UObject*>> LiveClassLookups;

    std::vector<std::vector<char>> FNameThreadArenas;
    std::vector<const char*>      FNameCacheArray;
    std::unordered_map<std::string, int32_t> StringToFNameIndex;

    std::unordered_map<std::string_view, int32_t, FoldedNameHash, FoldedNameEquals> NameTextToIndex;
    std::deque<std::string> FNameExtraNames;

    int32_t FNamePoolHighestChunk = -1;
    int32_t FNamePoolNextSlot = 0;

    std::vector<std::pair<std::string, UObject*>> SortedNames;

    mutable std::shared_ptr<std::recursive_mutex> IndexMutex = std::make_shared<std::recursive_mutex>();

    std::unordered_map<std::string, UFunction*> FunctionByFullKey;

    std::unordered_set<UFunction*> LiveUFunctions;

    std::unordered_map<std::string, UFunction*> FunctionByNameOnly;

    std::unordered_map<std::string, UClass*> ClassByName;

    UClass* CachedPlayerControllerClass = nullptr;

    int32_t FunctionCacheLastCount = 0;

    mutable std::shared_ptr<std::mutex> ReflectionCacheMutex =
        std::make_shared<std::mutex>();

    char LastError[1024] = {0};
};
