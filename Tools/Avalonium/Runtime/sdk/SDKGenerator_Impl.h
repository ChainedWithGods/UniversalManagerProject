#pragma once
#include "SDKGenerator.h"
#include "SDKValidation.h"
#include "SDKGenerator_Predefined.h"
#include "../core/EngineDiscovery.h"
#include "../core/RuntimeConfig.h"
#include "../usmap/USMapGenerator.h"
#include <windows.h>

extern "C" {
#include "../utils/SafeMemory.h"
}
#include <fstream>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <iomanip>
#include <thread>
#include <atomic>
#include <future>

namespace fs = std::filesystem;

namespace {
    constexpr uint64_t CPF_Edit = 0x0000000000000001ULL;
    constexpr uint64_t CPF_ConstParm = 0x0000000000000002ULL;
    constexpr uint64_t CPF_BlueprintVisible = 0x0000000000000004ULL;
    constexpr uint64_t CPF_ExportObject = 0x0000000000000008ULL;
    constexpr uint64_t CPF_BlueprintReadOnly = 0x0000000000000010ULL;
    constexpr uint64_t CPF_Net = 0x0000000000000020ULL;
    constexpr uint64_t CPF_EditFixedSize = 0x0000000000000040ULL;
    constexpr uint64_t CPF_Parm = 0x0000000000000080ULL;
    constexpr uint64_t CPF_OutParm = 0x0000000000000100ULL;
    constexpr uint64_t CPF_ZeroConstructor = 0x0000000000000200ULL;
    constexpr uint64_t CPF_ReturnParm = 0x0000000000000400ULL;
    constexpr uint64_t CPF_DisableEditOnTemplate = 0x0000000000000800ULL;
    constexpr uint64_t CPF_NonNullable = 0x0000000000001000ULL;
    constexpr uint64_t CPF_Transient = 0x0000000000002000ULL;
    constexpr uint64_t CPF_Config = 0x0000000000004000ULL;
    constexpr uint64_t CPF_RequiredParm = 0x0000000000008000ULL;
    constexpr uint64_t CPF_DisableEditOnInstance = 0x0000000000010000ULL;
    constexpr uint64_t CPF_EditConst = 0x0000000000020000ULL;
    constexpr uint64_t CPF_GlobalConfig = 0x0000000000040000ULL;
    constexpr uint64_t CPF_InstancedReference = 0x0000000000080000ULL;
    constexpr uint64_t CPF_DuplicateTransient = 0x0000000000200000ULL;
    constexpr uint64_t CPF_SaveGame = 0x0000000001000000ULL;
    constexpr uint64_t CPF_NoClear = 0x0000000002000000ULL;
    constexpr uint64_t CPF_ReferenceParm = 0x0000000008000000ULL;
    constexpr uint64_t CPF_BlueprintAssignable = 0x0000000010000000ULL;
    constexpr uint64_t CPF_Deprecated = 0x0000000020000000ULL;
    constexpr uint64_t CPF_IsPlainOldData = 0x0000000040000000ULL;
    constexpr uint64_t CPF_RepSkip = 0x0000000080000000ULL;
    constexpr uint64_t CPF_RepNotify = 0x0000000100000000ULL;
    constexpr uint64_t CPF_Interp = 0x0000000200000000ULL;
    constexpr uint64_t CPF_NonTransactional = 0x0000000400000000ULL;
    constexpr uint64_t CPF_EditorOnly = 0x0000000800000000ULL;
    constexpr uint64_t CPF_NoDestructor = 0x0000001000000000ULL;
    constexpr uint64_t CPF_AutoWeak = 0x0000004000000000ULL;
    constexpr uint64_t CPF_ContainsInstancedReference = 0x0000008000000000ULL;
    constexpr uint64_t CPF_AssetRegistrySearchable = 0x0000010000000000ULL;
    constexpr uint64_t CPF_SimpleDisplay = 0x0000020000000000ULL;
    constexpr uint64_t CPF_AdvancedDisplay = 0x0000040000000000ULL;
    constexpr uint64_t CPF_Protected = 0x0000080000000000ULL;
    constexpr uint64_t CPF_BlueprintCallable = 0x0000100000000000ULL;
    constexpr uint64_t CPF_BlueprintAuthorityOnly = 0x0000200000000000ULL;
    constexpr uint64_t CPF_TextExportTransient = 0x0000400000000000ULL;
    constexpr uint64_t CPF_NonPIEDuplicateTransient = 0x0000800000000000ULL;
    constexpr uint64_t CPF_ExposeOnSpawn = 0x0001000000000000ULL;
    constexpr uint64_t CPF_PersistentInstance = 0x0002000000000000ULL;
    constexpr uint64_t CPF_UObjectWrapper = 0x0004000000000000ULL;
    constexpr uint64_t CPF_HasGetValueTypeHash = 0x0008000000000000ULL;
    constexpr uint64_t CPF_NativeAccessSpecifierPublic = 0x0010000000000000ULL;
    constexpr uint64_t CPF_NativeAccessSpecifierProtected = 0x0020000000000000ULL;
    constexpr uint64_t CPF_NativeAccessSpecifierPrivate = 0x0040000000000000ULL;
    constexpr uint64_t CPF_SkipSerialization = 0x0080000000000000ULL;

    constexpr uint32_t FUNC_Final = 0x00000001U;
    constexpr uint32_t FUNC_RequiredAPI = 0x00000002U;
    constexpr uint32_t FUNC_BlueprintAuthorityOnly = 0x00000004U;
    constexpr uint32_t FUNC_BlueprintCosmetic = 0x00000008U;
    constexpr uint32_t FUNC_Net = 0x00000040U;
    constexpr uint32_t FUNC_NetReliable = 0x00000080U;
    constexpr uint32_t FUNC_Native = 0x00000400U;
    constexpr uint32_t FUNC_Event = 0x00000800U;
    constexpr uint32_t FUNC_Static = 0x00002000U;
    constexpr uint32_t FUNC_NetMulticast = 0x00004000U;
    constexpr uint32_t FUNC_MulticastDelegate = 0x00010000U;
    constexpr uint32_t FUNC_Public = 0x00020000U;
    constexpr uint32_t FUNC_Private = 0x00040000U;
    constexpr uint32_t FUNC_Protected = 0x00080000U;
    constexpr uint32_t FUNC_Delegate = 0x00100000U;
    constexpr uint32_t FUNC_NetServer = 0x00200000U;
    constexpr uint32_t FUNC_HasOutParms = 0x00400000U;
    constexpr uint32_t FUNC_HasDefaults = 0x00800000U;
    constexpr uint32_t FUNC_NetClient = 0x01000000U;
    constexpr uint32_t FUNC_BlueprintCallable = 0x04000000U;
    constexpr uint32_t FUNC_BlueprintEvent = 0x08000000U;
    constexpr uint32_t FUNC_BlueprintPure = 0x10000000U;
    constexpr uint32_t FUNC_Const = 0x40000000U;
}

namespace SDKGenerator {

    struct SdkFastMemScope {
        SdkFastMemScope()  { SafeMem_SetFastMode(1); }
        ~SdkFastMemScope() { SafeMem_SetFastMode(0); }
    };

    inline void AppendEscapedJson(std::string& out, const std::string& value) {
        if (value.empty()) return;
        bool needsEscape = false;
        for (unsigned char ch : value) {
            if (ch < 0x20 || ch == '\\' || ch == '"') {
                needsEscape = true;
                break;
            }
        }
        if (!needsEscape) {
            out.append(value);
            return;
        }
        for (unsigned char ch : value) {
            switch (ch) {
            case '\\': out.append("\\\\"); break;
            case '"':  out.append("\\\""); break;
            case '\n': out.append("\\n"); break;
            case '\r': out.append("\\r"); break;
            case '\t': out.append("\\t"); break;
            case '\b': out.append("\\b"); break;
            case '\f': out.append("\\f"); break;
            default:
                if (ch < 0x20) {
                    char hexBuf[8];
                    snprintf(hexBuf, sizeof(hexBuf), "\\u%04x", static_cast<unsigned int>(ch));
                    out.append(hexBuf);
                } else {
                    out.push_back(static_cast<char>(ch));
                }
                break;
            }
        }
    }

    inline std::string EscapeJson(const std::string& value) {
        if (value.empty()) return "";
        bool needsEscape = false;
        for (unsigned char ch : value) {
            if (ch < 0x20 || ch == '\\' || ch == '"') {
                needsEscape = true;
                break;
            }
        }
        if (!needsEscape) return value;
        std::string out;
        out.reserve(value.size() + 16);
        AppendEscapedJson(out, value);
        return out;
    }

    std::wstring ToWidePath(const std::string& narrowPath);

    struct SDKProperty {
        std::string Type;
        std::string Name;
        std::string RawType;
        std::string InnerType;
        std::string KeyType;
        std::string ValueType;
        std::string StructType;
        std::string EnumType;
        std::string ObjectClassType;
        int Offset = 0;
        int Size = 0;
        int ElementSize = 0;
        int ArrayDim = 0;
        uint64_t Flags = 0;
        std::string FlagsText;
        std::string SignatureHash;
        uint64_t Fingerprint = 0;
        bool IsArray = false;
        bool IsMap = false;
        bool IsSet = false;
        bool IsStruct = false;
        bool IsEnum = false;
        bool IsObject = false;
        bool IsBitfield = false;
        uint8_t BitIndex = 0xFF;
        uint8_t BitCount = 1;
        uint8_t ByteMask = 0;
        uint8_t FieldMask = 0;

        uint8_t FieldSize = 1;
        bool IsBool = false;
        bool IsString = false;
        bool IsNumeric = false;
        bool IsInt = false;
        bool IsFloat = false;
        bool IsDelegate = false;
        bool IsInterface = false;
        bool IsReturnParam = false;
        bool IsOutParam = false;
        bool IsReferenceParam = false;
        bool IsConstParam = false;

        bool IsLocal = false;

        bool IsSynthetic = false;
    };

    struct SDKFunction {
        std::string Name;
        std::string RawName;
        std::string FullPath;
        std::vector<SDKProperty> Params;
        std::string ReturnType;
        std::string ReturnName;
        std::string FlagsText;
        uint32_t FunctionFlags = 0;
        int32_t ParameterSize = 0;
        std::string SignatureHash;
        uint64_t Fingerprint = 0;
    };

    struct MetadataEntry {
        std::string Kind;
        std::string PackageName;
        std::string Name;
        std::string CppName;
        std::string FullPath;
        std::string SuperName;
        std::string SignatureHash;
        int32_t Size = 0;
        int32_t SuperSize = 0;
        int32_t OwnSize = 0;
        int32_t PropertyCount = 0;
        int32_t FunctionCount = 0;
        int32_t ValueCount = 0;
        uint32_t FunctionFlags = 0;
    };

    struct PackageSummary {
        int Enums = 0;
        int Structs = 0;
        int Classes = 0;
        int Functions = 0;
    };

    struct ReflectionEnumRecord {
        std::string Id;
        std::string PackageName;
        std::string Name;
        std::string CppName;
        std::string FullPath;
        std::string SignatureHash;
        std::vector<std::pair<std::string, int64_t>> Values;
    };

    struct ReflectionTypeRecord {
        std::string Id;
        std::string Kind;
        std::string PackageName;
        std::string Name;
        std::string CppName;
        std::string FullPath;
        std::string SuperName;
        std::string SuperCppName;
        std::string SignatureHash;
        int32_t Size = 0;
        int32_t SuperSize = 0;
        int32_t OwnSize = 0;
        int32_t PropertyCount = 0;
        int32_t FunctionCount = 0;
    };

    struct ReflectionFunctionRecord {
        std::string Id;
        std::string OwnerTypeId;
        std::string OwnerKind;
        std::string OwnerPackageName;
        std::string OwnerName;
        std::string OwnerCppName;
        std::string Name;
        std::string FullPath;
        std::string ReturnType;
        std::string ReturnName;
        uint32_t FunctionFlags = 0;
        int32_t ParameterSize = 0;
        std::string SignatureHash;
        std::vector<SDKProperty> Params;
    };

    struct ReflectionPropertyRecord {
        std::string Id;
        std::string OwnerId;
        std::string OwnerKind;
        std::string OwnerPackageName;
        std::string OwnerName;
        std::string OwnerCppName;
        std::string Name;
        std::string CppType;
        std::string RawType;
        std::string InnerType;
        std::string KeyType;
        std::string ValueType;
        std::string StructType;
        std::string EnumType;
        std::string ObjectClassType;
        std::string FlagsText;
        std::string Role;
        std::string SignatureHash;
        int32_t Offset = 0;
        int32_t Size = 0;
        int32_t ElementSize = 0;
        int32_t ArrayDim = 0;
        uint64_t Flags = 0;
        bool IsArray = false;
        bool IsMap = false;
        bool IsSet = false;
        bool IsStruct = false;
        bool IsEnum = false;
        bool IsObject = false;
        bool IsBitfield = false;
        uint8_t BitIndex = 0xFF;
        uint8_t FieldMask = 0;
        uint8_t FieldSize = 1;
        bool IsBool = false;
        bool IsString = false;
        bool IsNumeric = false;
        bool IsInt = false;
        bool IsFloat = false;
        bool IsDelegate = false;
        bool IsInterface = false;
        bool IsReturnParam = false;
        bool IsOutParam = false;
        bool IsReferenceParam = false;
        bool IsConstParam = false;
    };

    struct ReflectionSnapshotEntry {
        std::string Kind;
        std::string Id;
        std::string SignatureHash;
        std::string OwnerCppName;
        std::string Name;
        std::string Extra;
    };

    struct PackageStreams {
        std::ostringstream enums;
        std::ostringstream structs;
        std::ostringstream classes;
        std::ostringstream functions;
        std::ostringstream parameters;
        bool hasStructs = false;
        bool hasClasses = false;
        bool hasFunctions = false;
        bool hasParameters = false;
        bool hasEnums = false;
    };

    struct PendingTypeDef {
        std::string cppName;
        std::string text;
        std::set<std::string> deps;
    };

    struct EnumInfo {
        std::string name;
        std::string packageName;
        std::vector<std::pair<std::string, int64_t>> values;
    };

    struct GenerationCaches {
        std::unordered_map<const UObject*, std::string> ObjectNames;
        std::unordered_map<const UObject*, std::string> CleanObjectNames;
        std::unordered_map<const UObject*, std::string> FullNames;
        std::unordered_map<const UObject*, std::string> PackageNames;
        std::unordered_map<const void*, std::string> FieldNames;
        std::unordered_map<const void*, std::string> FieldTypeNames;
        std::unordered_map<const void*, std::string> PropertyCppTypes;
        std::unordered_map<const void*, bool> IsActorDerived;
        std::unordered_map<const void*, bool> IsScriptStruct;
        std::unordered_map<const void*, bool> IsClass;
        std::unordered_map<const void*, bool> IsFunction;
        std::unordered_map<const void*, bool> IsEnum;
        std::unordered_map<uint64_t, std::string> PropertyFlagsText;
        std::unordered_map<uint32_t, std::string> FunctionFlagsText;

        std::unordered_map<const void*, uint32_t> EnumUnderlyingBytes;

        std::unordered_map<const void*, int32_t> EnumPropertyBytes;

        int32_t FTextSize = 0;
        int32_t LazyObjectSize = 0;
        void Clear();
    };

    struct TypeDependencyContext {
        const EngineState* state = nullptr;
        std::map<std::string, int>* collisionCounter = nullptr;
        std::map<std::string, std::string>* rawToUnique = nullptr;
        std::string selfPackage;
        std::set<std::string> deps;
        void Reset(const EngineState& s, std::map<std::string, int>& cc,
                   std::map<std::string, std::string>& ru, const std::string& self);
        void Finish(std::map<std::string, std::set<std::string>>& packageDependencies);
    };

    bool IsCppKeyword(const std::string& name);
    std::string CleanClassName(std::string name);
    std::string MakeEnumCppName(const std::string& cleanName);
    std::string MakeUniqueName(const std::string& wanted, std::set<std::string>& used);
    bool IsReservedWindowsName(const std::string& name);
    std::string ResolvePackageName(const std::string& rawName, std::map<std::string, int>& collisionCounter, std::map<std::string, std::string>& rawToUnique);
    std::string EscapeJson(const std::string& value);
    void* ReadPtrAt(void* base, int32_t offset);
    GenerationCaches& GetGenerationCaches();
    TypeDependencyContext& GetTypeDependencyContext();
    std::unordered_map<const void*, std::string>& GetUniqueTypeNameMap();
    std::unordered_map<std::string, int>& GetTypeNameUseCounts();
    UObject* GetOutermost(const EngineState& state, UObject* obj);
    uint64_t HashBytesFNV1a(uint64_t hash, const void* data, size_t size);
    uint64_t HashStringFNV1a(uint64_t hash, const std::string& value);
    uint64_t HashUInt64FNV1a(uint64_t hash, uint64_t value);
    std::string FormatHashHex(uint64_t hash);
    std::wstring ToWidePath(const std::string& narrowPath);
    void AppendTag(std::string& output, const char* tag);
    std::string DescribePropertyFlags(uint64_t flags);
    std::string DescribeFunctionFlags(uint32_t flags);
    bool WriteFile(const std::string& outputDir, const std::string& fileName, const std::string& content);
    std::string GetCachedObjectName(const EngineState& state, UObject* obj);
    std::string GetCachedCleanObjectName(const EngineState& state, UObject* obj);
    std::string GetCachedFullName(const EngineState& state, UObject* obj);
    std::string GetCachedFFieldName(const EngineState& state, void* field);
    std::string GetCachedFFieldTypeName(const EngineState& state, void* field);
    std::string DescribePropertyFlagsCached(uint64_t flags);
    std::string DescribeFunctionFlagsCached(uint32_t flags);
    std::string GetPackageName(const EngineState& state, UObject* obj, std::map<std::string, int>& collisionCounter, std::map<std::string, std::string>& rawToUnique);
    void NoteValueTypeDependency(const EngineState& state, const void* referencedObj);
    bool IsActorDerived(const EngineState& state, void* obj);
    bool IsScriptStruct(const EngineState& state, UClass* metaCls);
    bool IsClass(const EngineState& state, UClass* metaCls);
    bool IsFunction(const EngineState& state, UClass* metaCls);
    bool IsEnum(const EngineState& state, UClass* metaCls);
    std::string GetUniqueTypeCppName(const void* obj, const std::string& baseName);

    std::string PropertyTypeToCpp(const EngineState& state, void* prop, const std::string& pType,
                                  int32_t propertyElementSize = -1);
    uint32_t GetEnumUnderlyingBytesCached(const EngineState& state, void* enumObj);
    int32_t GetObservedEnumBytes(const EngineState& state, void* enumObj);

    int32_t ReadStructPropertySize(const EngineState& state, void* structObj);

    int32_t ReadStructAlignment(const EngineState& state, void* structObj);

    struct OwnPropertyRange {
        int32_t FirstOffset = -1;
        int32_t LastOffset = -1;
    };
    OwnPropertyRange ReadOwnPropertyRange(const EngineState& state, void* classObj);

    int32_t ReadPropertyArrayDim(const EngineState& state, void* prop, const char* where);
    bool ReadEnumValues(const EngineState& state, void* enumObj, EnumInfo& info);
    std::string RenderFieldDeclaration(const SDKProperty& prop, const std::string& name);
    std::string RenderFieldDeclaration(const SDKProperty& prop);
    std::string RenderFunctionArgument(const SDKProperty& prop);
    uint64_t BuildPropertyFingerprint(const SDKProperty& prop);
    uint64_t BuildFunctionFingerprint(const SDKFunction& fn);
    std::string MakeReflectionId(const char* prefix, const std::string& primary, const std::string& secondary = "");
    std::string ResolveNestedPropertyType(const EngineState& state, void* prop);
    void PopulatePropertyTraits(const EngineState& state, void* propObj, const std::string& rawType, SDKProperty& prop);
    ReflectionPropertyRecord MakeReflectionPropertyRecord(const SDKProperty& prop, const std::string& ownerId, const std::string& ownerKind, const std::string& ownerPackageName, const std::string& ownerName, const std::string& ownerCppName, const std::string& role);
    std::vector<SDKProperty> ReadFunctionParams(const EngineState& state, void* funcObj);

    std::vector<SDKProperty> CollectProperties(const EngineState& state, void* obj, int baseLayoutEnd);
    void WriteMembers(std::ostringstream& out, const std::vector<SDKProperty>& props, int superSize, int structSize, const std::string& scopeName = "", std::vector<int32_t>* emittedOffsets = nullptr);
    std::vector<SDKFunction> CollectFunctions(const EngineState& state, void* classObj, const std::string& packageName, const std::string& objectName);
    std::string ResolveReflectionOutputDir(const std::string& sdkOutputDir);
    bool WriteReflectionDatabase( const std::string& outputDir, const EngineState& state, int32_t objectCount, int collisionCount, const std::map<std::string, PackageSummary>& packageSummaries, const std::map<std::string, std::set<std::string>>& packageDependencies, const std::vector<ReflectionEnumRecord>& enums, const std::vector<ReflectionTypeRecord>& types, const std::vector<ReflectionFunctionRecord>& functions, const std::vector<ReflectionPropertyRecord>& properties);
    std::string EscapeCppString(const std::string& value);
    bool WriteReflectionRuntimeHelpers(const std::string& outputDir, const std::vector<ReflectionTypeRecord>& types, const std::vector<ReflectionFunctionRecord>& functions, const std::vector<ReflectionPropertyRecord>& properties);
    bool WriteReflectionGraphs( const std::string& outputDir, const std::map<std::string, PackageSummary>& packageSummaries, const std::map<std::string, std::set<std::string>>& packageDependencies, const std::vector<ReflectionTypeRecord>& types);
    std::vector<ReflectionSnapshotEntry> BuildReflectionSnapshotEntries( const std::vector<ReflectionEnumRecord>& enums, const std::vector<ReflectionTypeRecord>& types, const std::vector<ReflectionFunctionRecord>& functions, const std::vector<ReflectionPropertyRecord>& properties);
    std::string SerializeSnapshotEntry(const ReflectionSnapshotEntry& entry);
    bool ParseSnapshotEntry(const std::string& line, ReflectionSnapshotEntry& entry);
    bool ReadLinesFile(const std::string& path, std::vector<std::string>& lines);
    std::map<std::string, std::string> ParseSnapshotExtra(const std::string& extra);
    std::string GetSnapshotField(const ReflectionSnapshotEntry& entry, const char* key);
    bool LoadSnapshotMap(const std::string& snapshotPath, std::unordered_map<std::string, ReflectionSnapshotEntry>& outEntries);
    std::string DetermineDiffRisk(const char* change, const ReflectionSnapshotEntry* before, const ReflectionSnapshotEntry* after);
    std::string DetermineDiffReason(const char* change, const ReflectionSnapshotEntry* before, const ReflectionSnapshotEntry* after);
    bool WriteReflectionDiffFromSnapshots( const std::string& outputDir, const EngineState& state, const std::unordered_map<std::string, ReflectionSnapshotEntry>& previousById, const std::unordered_map<std::string, ReflectionSnapshotEntry>& currentById, bool hasPreviousSnapshot);
    bool WriteReflectionDiff( const std::string& outputDir, const EngineState& state, const std::vector<ReflectionEnumRecord>& enums, const std::vector<ReflectionTypeRecord>& types, const std::vector<ReflectionFunctionRecord>& functions, const std::vector<ReflectionPropertyRecord>& properties);
    void CollectStructRefsFromType(const std::string& cppType, std::set<std::string>& out);
    std::vector<int> SortTypeDefOrder(const std::vector<PendingTypeDef>& defs);
    std::vector<PendingTypeDef> SortTypeDefs(const std::vector<PendingTypeDef>& defs);
    bool DiscoverUEnumNamesOffset(const EngineState& state);
    bool WriteMetadataManifest( const std::string& outputDir, const EngineState& state, int32_t objectCount, int totalFiles, int failedFiles, int collisionCount, const std::vector<MetadataEntry>& metadataEntries, const std::map<std::string, PackageSummary>& packageSummaries, const std::map<std::string, std::set<std::string>>& packageDependencies);

}
