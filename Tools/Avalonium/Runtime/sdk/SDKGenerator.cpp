#include "SDKGenerator_Impl.h"
#include "SDKGenerator_Containers.h"
#include "SDKGenerator_Bitfields.h"
#include "SDKGenerator_Predefined.h"
#include "SDKGenerator_Cycles.h"
#include "SDKGenerator_Assertions.h"
#include "SDKGenerator_LayoutRules.h"
#include <windows.h>
#include <limits>
namespace SDKGenerator {

    bool DiscoverUEnumNamesOffset(const EngineState& state) {
        if (state.UEnum_NamesOff >= 0) return true;
        if (!state.UEnumClass) return false;

        int32_t count = EngineDiscovery::GetObjectCount(state);
        for (int i = 0; i < std::min(count, 50000); i++) {
            UObject* obj = EngineDiscovery::GetObjectByIndex(state, i);
            if (!obj) continue;

            UClass* cls = EngineDiscovery::GetObjectClass(state, obj);
            if (!cls) continue;

            std::string clsName = GetCachedObjectName(state, cls);
            if (clsName != "Enum" && clsName != "UserDefinedEnum") continue;

            uint8_t enumBuf[0x100];
            if (!SafeMem_Read((uint8_t*)obj, enumBuf, sizeof(enumBuf))) continue;

            for (int off = 0x40; off <= 0xD0; off += 8) {
                uintptr_t dataPtrTagged = *(uintptr_t*)(enumBuf + off);
                bool isTagged = (dataPtrTagged & 1) != 0;
                uintptr_t dataPtr = dataPtrTagged & ~1ULL;

                if (dataPtr < 0x10000 || dataPtr >= 0x7FF000000000ULL) continue;

                if (isTagged) {
                    uintptr_t valPtrTagged = *(uintptr_t*)(enumBuf + off + 8);
                    uintptr_t valPtr = valPtrTagged & ~1ULL;
                    int32_t num = *(int32_t*)(enumBuf + off + 16);

                    if (valPtr >= 0x10000 && valPtr < 0x7FF000000000ULL && num >= 2 && num < 1000) {
                        bool valid = true;
                        int validNames = 0;
                        for (int e = 0; e < std::min(num, 5); e++) {
                            uint8_t nameBuf[16] = {0};
                            if (!SafeMem_Read((uint8_t*)dataPtr + e * state.FName_Size, nameBuf, state.FName_Size)) {
                                valid = false; break;
                            }
                            FName fn;
                            fn.ComparisonIndex = *(uint32_t*)nameBuf;
                            fn.Number = state.FName_NumberOff >= 0 ? *(uint32_t*)(nameBuf + state.FName_NumberOff) : 0;
                            std::string name = EngineDiscovery::FNameToStr(state, fn);
                            if (name.empty() || name[0] == '<') {
                                valid = false; break;
                            }
                            validNames++;
                        }
                        if (valid && validNames >= 2) {
                            const_cast<EngineState&>(state).bIsNewUE5Enum = true;
                            const_cast<EngineState&>(state).UEnum_NamesOff = off;
                            return true;
                        }
                    }
                }

                int32_t num = *(int32_t*)(enumBuf + off + 8);
                int32_t max = *(int32_t*)(enumBuf + off + 12);
                if (num < 2 || num > 1000 || max < num) continue;

                int32_t nameStride = (state.FName_Size + 7) & ~7;
                int32_t entryStride = nameStride + 8;

                bool valid = true;
                int validNames = 0;
                for (int e = 0; e < std::min(num, 5); e++) {
                    uint8_t tmpBuf[32] = {0};
                    if (!SafeMem_Read((uint8_t*)dataPtr + e * entryStride, tmpBuf, entryStride)) {
                        valid = false; break;
                    }
                    FName fn;
                    fn.ComparisonIndex = *(uint32_t*)tmpBuf;
                    fn.Number = state.FName_NumberOff >= 0 ? *(uint32_t*)(tmpBuf + state.FName_NumberOff) : 0;
                    std::string name = EngineDiscovery::FNameToStr(state, fn);
                    if (name.empty() || name[0] == '<') {
                        valid = false; break;
                    }
                    validNames++;
                }

                if (valid && validNames >= 2) {
                    const_cast<EngineState&>(state).bIsNewUE5Enum = false;
                    const_cast<EngineState&>(state).UEnum_NamesOff = off;
                    return true;
                }
            }
        }
        return false;
    }

    bool WriteMetadataManifest(
        const std::string& outputDir,
        const EngineState& state,
        int32_t objectCount,
        int totalFiles,
        int failedFiles,
        int collisionCount,
        const std::vector<MetadataEntry>& metadataEntries,
        const std::map<std::string, PackageSummary>& packageSummaries,
        const std::map<std::string, std::set<std::string>>& packageDependencies) {

        std::vector<const MetadataEntry*> sortedEntries;
        sortedEntries.reserve(metadataEntries.size());
        for (const auto& entry : metadataEntries) sortedEntries.push_back(&entry);
        std::sort(sortedEntries.begin(), sortedEntries.end(), [](const MetadataEntry* lhs, const MetadataEntry* rhs) {
            if (lhs->Kind != rhs->Kind) return lhs->Kind < rhs->Kind;
            if (lhs->PackageName != rhs->PackageName) return lhs->PackageName < rhs->PackageName;
            if (lhs->FullPath != rhs->FullPath) return lhs->FullPath < rhs->FullPath;
            return lhs->CppName < rhs->CppName;
        });

        std::string json;
        json.reserve(8 * 1024 * 1024);
        json.append("{\n");
        json.append("  \"generator\": \"Avalonium SDK\",\n");
        json.append("  \"format_version\": 1,\n");
        json.append("  \"game\": \""); AppendEscapedJson(json, state.GameName); json.append("\",\n");
        json.append("  \"engine\": \"UE"); json.append(std::to_string(state.UEMajor)); json.append("."); json.append(std::to_string(state.UEMinor)); json.append("\",\n");
        json.append("  \"object_count\": "); json.append(std::to_string(objectCount)); json.append(",\n");
        json.append("  \"package_collision_count\": "); json.append(std::to_string(collisionCount)); json.append(",\n");
        json.append("  \"total_files\": "); json.append(std::to_string(totalFiles)); json.append(",\n");
        json.append("  \"failed_files\": "); json.append(std::to_string(failedFiles)); json.append(",\n");
        json.append("  \"packages\": [\n");

        bool firstPackage = true;
        for (const auto& kv : packageSummaries) {
            if (!firstPackage) json.append(",\n");
            firstPackage = false;
            json.append("    {\n");
            json.append("      \"name\": \""); AppendEscapedJson(json, kv.first); json.append("\",\n");
            json.append("      \"enums\": "); json.append(std::to_string(kv.second.Enums)); json.append(",\n");
            json.append("      \"structs\": "); json.append(std::to_string(kv.second.Structs)); json.append(",\n");
            json.append("      \"classes\": "); json.append(std::to_string(kv.second.Classes)); json.append(",\n");
            json.append("      \"functions\": "); json.append(std::to_string(kv.second.Functions)); json.append(",\n");
            json.append("      \"dependencies\": [");
            auto depIt = packageDependencies.find(kv.first);
            if (depIt != packageDependencies.end()) {
                bool firstDep = true;
                for (const std::string& dep : depIt->second) {
                    if (!firstDep) json.append(", ");
                    firstDep = false;
                    json.append("\""); AppendEscapedJson(json, dep); json.append("\"");
                }
            }
            json.append("]\n    }");
        }
        json.append("\n  ],\n");
        json.append("  \"symbols\": [\n");

        bool firstSymbol = true;
        for (const MetadataEntry* entry : sortedEntries) {
            if (!firstSymbol) json.append(",\n");
            firstSymbol = false;
            json.append("    {\n");
            json.append("      \"kind\": \""); AppendEscapedJson(json, entry->Kind); json.append("\",\n");
            json.append("      \"package\": \""); AppendEscapedJson(json, entry->PackageName); json.append("\",\n");
            json.append("      \"name\": \""); AppendEscapedJson(json, entry->Name); json.append("\",\n");
            json.append("      \"cpp_name\": \""); AppendEscapedJson(json, entry->CppName); json.append("\",\n");
            json.append("      \"full_path\": \""); AppendEscapedJson(json, entry->FullPath); json.append("\",\n");
            json.append("      \"super\": \""); AppendEscapedJson(json, entry->SuperName); json.append("\",\n");
            json.append("      \"hash\": \""); AppendEscapedJson(json, entry->SignatureHash); json.append("\",\n");
            json.append("      \"size\": "); json.append(std::to_string(entry->Size)); json.append(",\n");
            json.append("      \"super_size\": "); json.append(std::to_string(entry->SuperSize)); json.append(",\n");
            json.append("      \"own_size\": "); json.append(std::to_string(entry->OwnSize)); json.append(",\n");
            json.append("      \"property_count\": "); json.append(std::to_string(entry->PropertyCount)); json.append(",\n");
            json.append("      \"function_count\": "); json.append(std::to_string(entry->FunctionCount)); json.append(",\n");
            json.append("      \"value_count\": "); json.append(std::to_string(entry->ValueCount)); json.append(",\n");
            json.append("      \"function_flags\": "); json.append(std::to_string(entry->FunctionFlags)); json.append("\n");
            json.append("    }");
        }
        json.append("\n  ]\n");
        json.append("}\n");

        return WriteFile(outputDir, "SDKMetadata.json", json);
    }

    void AppendNonReflectedClassMembers(const EngineState& state,
                                        const std::string& packageName, const std::string& className,
                                        int32_t superSize, int32_t liveSize,
                                        std::vector<SDKProperty>& props) {
        struct ExtraMember {
            const char* Name;
            const char* Type;
            int32_t Offset;
            int32_t Size;
        };
        std::vector<ExtraMember> extras;

        const auto hasMember = [&props](const char* name) {
            for (const auto& p : props) if (p.Name == name) return true;
            return false;
        };
        const auto overlaps = [&props](int32_t offset, int32_t size) {
            for (const auto& p : props) {
                if (p.IsBitfield) continue;
                if (offset < p.Offset + p.Size && p.Offset < offset + size) return true;
            }
            return false;
        };

        if (packageName == "CoreUObject" && className == "Enum") {

            if (superSize == 0x30 && !hasMember("Names")) {
                if (liveSize == 0x60)
                    extras.push_back({"Names", "TArray<TPair<FName, int64>>", superSize + 0x10, 0x10});
                else if (liveSize == 0x70)
                    extras.push_back({"Names", "SDK_EnumNameData", superSize + 0x10, 0x18});
            }
        } else if (packageName == "Engine" && className == "DataTable") {

            for (const auto& p : props) {
                if (p.Name == "RowStruct") {
                    if (!hasMember("RowMap"))
                        extras.push_back({"RowMap", "TMap<FName, uint8*>", p.Offset + p.Size, 0x50});
                    break;
                }
            }
        } else if (packageName == "Engine" && className == "Level") {

            const int32_t actorsToOwningWorld =
                (state.UEMajor > 5 || (state.UEMajor == 5 && state.UEMinor >= 8)) ? 0x28 : 0x20;
            for (const auto& p : props) {
                if (p.Name == "OwningWorld") {
                    if (!hasMember("Actors"))
                        extras.push_back({"Actors", "TArray<class AActor*>", p.Offset - actorsToOwningWorld, 0x10});
                    break;
                }
            }
        }

        bool added = false;
        for (const ExtraMember& m : extras) {
            if (m.Offset < superSize || m.Size <= 0 || m.Offset + m.Size > liveSize || overlaps(m.Offset, m.Size)) {
                SdkLog("[SDK] Non-reflected member %s::%s skipped (0x%X..0x%X does not fit the class layout)",
                       className.c_str(), m.Name, m.Offset, m.Offset + m.Size);
                continue;
            }
            SDKProperty prop;
            prop.Type = m.Type;
            prop.Name = m.Name;
            prop.Offset = m.Offset;
            prop.Size = m.Size;
            prop.ElementSize = m.Size;
            prop.ArrayDim = 1;
            prop.FlagsText = "Engine declaration (not reflected)";
            prop.IsSynthetic = true;
            props.push_back(std::move(prop));
            added = true;
        }
        if (added) SortPropertiesForLayout(props);
    }

bool GenerateSDK(const EngineState& state, const std::string& outputDir) {
    SdkFastMemScope fastMem;
    EngineDiscovery::ResetSdkLogTimeBaseline();
    SdkLog("[SDK] ====== Starting Full C++ SDK Generation ======");
    SdkLog("[SDK] Output Folder: %s", outputDir.c_str());

        const std::wstring wideOutputDir = ToWidePath(outputDir);
        if (wideOutputDir.empty()) {
            SdkLog("[SDK] ERROR: Failed to convert output folder to UTF-16: %s", outputDir.c_str());
            return false;
        }

        const fs::path outputDirPath(wideOutputDir);
        if (fs::exists(outputDirPath)) {

            fs::remove_all(outputDirPath);
        }
        fs::create_directories(outputDirPath);

        DiscoverUEnumNamesOffset(state);

        USMapDiscovery::DiscoverPropertySubTypeOffsets(const_cast<EngineState&>(state));
        GetGenerationCaches().Clear();
        GetUniqueTypeNameMap().clear();
        GetTypeNameUseCounts().clear();

        int32_t count = EngineDiscovery::GetObjectCount(state);
        SdkLog("[SDK] Total objects in GUObjectArray: %d", count);

        {

            std::ostringstream basicContent;
            basicContent << "#pragma once\n\n";
            basicContent << "/*\n";
            basicContent << " * SDK generated by Avalonium\n";
            basicContent << " * Created by solo reverse engineer\n";
            basicContent << " * Dont use this SDK for any illegal purpose, dont disappoint me human\n";
            basicContent << " * License: GPLv3\n";
            basicContent << " */\n\n";
            basicContent << "#include <cstdint>\n";
            basicContent << "#include <string>\n";
            basicContent << "#include <unordered_map>\n";
            basicContent << "#ifndef NOMINMAX\n#define NOMINMAX\n#endif\n";
            basicContent << "#ifndef WIN32_LEAN_AND_MEAN\n#define WIN32_LEAN_AND_MEAN\n#endif\n";
            basicContent << "#include <windows.h>\n\n";
            basicContent << "#ifdef PF_MAX\n#undef PF_MAX\n#endif\n\n";
            basicContent << "// Fundamental container and conversion headers\n";
            basicContent << "#include \"UtfN.hpp\"\n";
            basicContent << "#include \"UnrealContainers.hpp\"\n";
            basicContent << "#include \"PropertyFixup.hpp\"\n\n";
            basicContent << "using namespace UC;\n\n";
            basicContent << "// Basic type definitions\n";
            basicContent << "typedef int8_t int8;\n";
            basicContent << "typedef int16_t int16;\n";
            basicContent << "typedef int32_t int32;\n";
            basicContent << "typedef int64_t int64;\n";
            basicContent << "typedef uint8_t uint8;\n";
            basicContent << "typedef uint16_t uint16;\n";
            basicContent << "typedef uint32_t uint32;\n";
            basicContent << "typedef uint64_t uint64;\n\n";
            basicContent << "// Compile-time layout checks: Assertions.inl defines one AVALONIUM_ASSERTS_<Type>\n";
            basicContent << "// macro per class and struct, and every definition invokes it through\n";
            basicContent << "// AVALONIUM_ASSERT_LAYOUT. The file is only read when CHECK_SDK_LAYOUT is\n";
            basicContent << "// defined, so a normal build does not parse it.\n";
            basicContent << "#if defined(CHECK_SDK_LAYOUT)\n";
            basicContent << "#include \"Assertions.inl\"\n";
            basicContent << "#define AVALONIUM_ASSERT_LAYOUT(Type) AVALONIUM_ASSERTS_##Type\n";
            basicContent << "#else\n";
            basicContent << "#define AVALONIUM_ASSERT_LAYOUT(Type)\n";
            basicContent << "#endif\n\n";
            basicContent << "// Core string/name types\n";
            basicContent << "struct FName\n";
            basicContent << "{\n";
            basicContent << "    int32_t ComparisonIndex;\n";
            basicContent << "    int32_t Number;\n\n";
            basicContent << "    inline std::string ToString() const;\n";
            basicContent << "};\n\n";

            basicContent << "struct SDK_EnumNameData\n";
            basicContent << "{\n";
            basicContent << "    void* TaggedNames;\n";
            basicContent << "    void* TaggedValues;\n";
            basicContent << "    int32_t NumValues;\n";
            basicContent << "    int32_t Pad_14;\n";
            basicContent << "};\n\n";
            basicContent << "// Forward declarations\n";
            basicContent << "class UObject;\nclass UField;\nclass UStruct;\nclass UClass;\nclass UFunction;\nclass AActor;\n\n";

            basicContent << R"SDK_RUNTIME(// ============================================================================

#if !defined(AVALONIUM_SDK_API_VERSION)
#define AVALONIUM_SDK_API_VERSION 1u
#endif

#ifndef AVALONIUM_SDK_API_TYPES_DEFINED
#define AVALONIUM_SDK_API_TYPES_DEFINED
typedef struct AvaloniumSdkApi
{
    uint32_t struct_size;
    uint32_t api_version;

    int32_t (*get_engine_major)(void);
    int32_t (*get_engine_minor)(void);

    int32_t (*get_object_count)(void);
    void* (*get_object_by_index)(int32_t index);
    int32_t (*get_object_index)(void* object);
    void* (*get_object_class)(void* object);
    void* (*get_object_outer)(void* object);
    void* (*get_super_struct)(void* object);
    const char* (*get_object_name)(void* object);
    const char* (*get_object_full_name)(void* object);
    const char* (*get_fname_string)(int32_t comparison_index, int32_t number);
    bool (*is_a)(void* object, const char* class_name);

    void* (*find_object)(const char* path);
    void* (*find_class)(const char* name);
    void* (*get_class_default_object)(void* uclass);
    void* (*find_function)(void* uclass, const char* function_name);

    bool (*process_event)(void* object, void* function, void* params);
} AvaloniumSdkApi;
#endif

namespace InSDKUtils
{
    inline uintptr_t GetImageBase()
    {
        static const uintptr_t base = reinterpret_cast<uintptr_t>(::GetModuleHandleW(nullptr));
        return base;
    }

    template<typename FuncType>
    inline FuncType GetVirtualFunction(const void* ObjectInstance, int32 Index)
    {
        void** VTable = *reinterpret_cast<void***>(const_cast<void*>(ObjectInstance));
        return reinterpret_cast<FuncType>(VTable[Index]);
    }
}

namespace SDKHelpers {

inline const AvaloniumSdkApi* Backend()
{
    static const AvaloniumSdkApi* Api = []() -> const AvaloniumSdkApi*
    {
        HMODULE Module = ::GetModuleHandleA("Avalonium.dll");
        if (!Module) Module = ::GetModuleHandleA("Avalonium-Proxied.dll");
        if (!Module) return nullptr;

        using GetApiFn = const AvaloniumSdkApi* (*)(uint32_t);
        const auto GetApi =
            reinterpret_cast<GetApiFn>(::GetProcAddress(Module, "AvaloniumGetSdkApi"));
        if (!GetApi) return nullptr;

        const AvaloniumSdkApi* Table = GetApi(AVALONIUM_SDK_API_VERSION);
        if (!Table || Table->api_version != AVALONIUM_SDK_API_VERSION ||
            Table->struct_size < static_cast<uint32_t>(sizeof(AvaloniumSdkApi)))
        {
            return nullptr;
        }
        return Table;
    }();
    return Api;
}

inline int32_t NumObjects()
{
    const AvaloniumSdkApi* Api = Backend();
    return Api ? Api->get_object_count() : 0;
}

inline UObject* GetObjectByIndex(int32_t index)
{
    const AvaloniumSdkApi* Api = Backend();
    return (Api && index >= 0) ? (UObject*)Api->get_object_by_index(index) : nullptr;
}

inline int32_t GetObjectIndex(const UObject* obj)
{
    const AvaloniumSdkApi* Api = Backend();
    return (Api && obj) ? Api->get_object_index((UObject*)obj) : -1;
}

inline UObject* GetObjectClass(const UObject* obj)
{
    const AvaloniumSdkApi* Api = Backend();
    return (Api && obj) ? (UObject*)Api->get_object_class((UObject*)obj) : nullptr;
}

inline UObject* GetObjectOuter(const UObject* obj)
{
    const AvaloniumSdkApi* Api = Backend();
    return (Api && obj) ? (UObject*)Api->get_object_outer((UObject*)obj) : nullptr;
}

inline bool IsA(const UObject* object, const UClass* target)
{
    const AvaloniumSdkApi* Api = Backend();
    if (!Api || !object || !target) return false;

    void* current = Api->get_object_class((UObject*)object);
    for (int32_t depth = 0; current && depth < 256; ++depth)
    {
        if (current == (const void*)target) return true;
        current = Api->get_super_struct(current);
    }
    return false;
}

inline bool IsChildOf(const UStruct* object, const UStruct* base)
{
    const AvaloniumSdkApi* Api = Backend();
    if (!Api || !object || !base) return false;

    const void* current = object;
    for (int32_t depth = 0; current && depth < 256; ++depth)
    {
        if (current == base) return true;
        current = Api->get_super_struct((UStruct*)current);
    }
    return false;
}

inline std::string FNameToString(FName name)
{
    if (name.ComparisonIndex == 0) return "None";
    const AvaloniumSdkApi* Api = Backend();
    if (!Api) return "<no-runtime>";
    const char* text = Api->get_fname_string(name.ComparisonIndex, name.Number);
    return (text && text[0]) ? std::string(text) : std::string("<unresolved>");
}

inline std::string GetObjectName(const UObject* obj)
{
    const AvaloniumSdkApi* Api = Backend();
    if (!Api || !obj) return std::string();
    const char* text = Api->get_object_name((UObject*)obj);
    return text ? std::string(text) : std::string();
}

inline std::string GetObjectFullName(const UObject* obj)
{
    const AvaloniumSdkApi* Api = Backend();
    if (!Api || !obj) return std::string();

    std::string fullName = GetObjectName(obj);
    for (const UObject* outer = GetObjectOuter(obj); outer; outer = GetObjectOuter(outer))
        fullName = GetObjectName(outer) + "." + fullName;
    return fullName;
}

inline UObject* FindObjectByName(const char* name, const char* className)
{
    const AvaloniumSdkApi* Api = Backend();
    if (!Api || !name) return nullptr;
    UObject* object = (UObject*)Api->find_object(name);
    if (object && className && className[0] && !Api->is_a(object, className)) return nullptr;
    return object;
}

inline UClass* FindClassFast(const char* name)
{
    const AvaloniumSdkApi* Api = Backend();
    return (Api && name) ? (UClass*)Api->find_class(name) : nullptr;
}

inline UFunction* GetFunction(const char* className, const char* funcName)
{
    const AvaloniumSdkApi* Api = Backend();
    if (!Api || !className || !funcName) return nullptr;
    void* objectClass = Api->find_class(className);
    return objectClass ? (UFunction*)Api->find_function(objectClass, funcName) : nullptr;
}

inline UObject* GetDefaultObject(const char* className)
{
    const AvaloniumSdkApi* Api = Backend();
    if (!Api || !className) return nullptr;
    void* objectClass = Api->find_class(className);
    return objectClass ? (UObject*)Api->get_class_default_object(objectClass) : nullptr;
}

inline void ProcessEvent(const UObject* object, UFunction* function, void* params)
{
    const AvaloniumSdkApi* Api = Backend();
    if (Api && object && function)
        Api->process_event((UObject*)object, function, params);
}

struct ReloadableFuncInfo
{
    int32_t Index = -1;
    std::string Name;
    std::string OuterName;
};

inline UFunction* GetCachedFunction(ReloadableFuncInfo& info)
{
    const AvaloniumSdkApi* Api = Backend();
    if (!Api || info.Index < 0) return nullptr;

    UObject* cached = (UObject*)Api->get_object_by_index(info.Index);
    if (!cached) return nullptr;
    if (info.Name != GetObjectName(cached)) return nullptr;

    const UObject* outer = GetObjectOuter(cached);
    if (!outer || info.OuterName != GetObjectName(outer)) return nullptr;
    return (UFunction*)cached;
}

inline void SetCachedFunction(UFunction* function, ReloadableFuncInfo& info)
{
    const AvaloniumSdkApi* Api = Backend();
    if (!Api || !function) return;

    const UObject* object = (const UObject*)function;
    info.Index = Api->get_object_index((UObject*)object);
    info.Name = GetObjectName(object);
    const UObject* outer = GetObjectOuter(object);
    info.OuterName = outer ? GetObjectName(outer) : std::string();
}

struct StaticClassInfo
{
    int32_t Index = -1;
    std::string Name;
};

inline UClass* GetStaticClass(const char* name, StaticClassInfo& info)
{
    const AvaloniumSdkApi* Api = Backend();
    if (!Api || !name) return nullptr;

    if (info.Index >= 0)
    {
        UObject* cached = (UObject*)Api->get_object_by_index(info.Index);
        if (cached && info.Name == GetObjectName(cached))
            return (UClass*)cached;
    }

    UClass* resolved = FindClassFast(name);
    if (resolved)
    {
        info.Index = Api->get_object_index((UObject*)resolved);
        info.Name = GetObjectName((const UObject*)resolved);
    }
    return resolved;
}

}

inline std::string FName::ToString() const {
    return SDKHelpers::FNameToString(*this);
}
)SDK_RUNTIME";

            if (!WriteFile(outputDir, "Basic.h", basicContent.str())) {
                SdkLog("[SDK] ERROR: Failed to write Basic.h, aborting.");
                return false;
            }
            WriteFile(outputDir, "UtfN.hpp", GetUtfNHeaderContent());
            WriteFile(outputDir, "UnrealContainers.hpp", GetUnrealContainersHeaderContent());
        }

        std::map<std::string, int> packageCollisionCounter;
        std::map<std::string, std::string> rawToUniquePackageName;
        std::vector<MetadataEntry> metadataEntries;
        std::map<std::string, PackageSummary> packageSummaries;
        std::vector<ReflectionEnumRecord> reflectionEnums;
        std::vector<ReflectionTypeRecord> reflectionTypes;
        std::vector<ReflectionFunctionRecord> reflectionFunctions;
        std::vector<ReflectionPropertyRecord> reflectionProperties;
        metadataEntries.reserve(static_cast<size_t>(count / 2));
        reflectionEnums.reserve(static_cast<size_t>(count / 32));
        reflectionTypes.reserve(static_cast<size_t>(count / 4));
        reflectionFunctions.reserve(static_cast<size_t>(count / 16));
        reflectionProperties.reserve(static_cast<size_t>(count));
        std::vector<StructAssertionInfo> structAssertions;
        std::map<std::string, std::set<std::string>> packageDefinedClasses;

        std::vector<UObject*> enumObjects;
        std::vector<UObject*> structObjects;
        std::vector<UObject*> classObjects;
        enumObjects.reserve(static_cast<size_t>(count / 16));
        structObjects.reserve(static_cast<size_t>(count / 8));
        classObjects.reserve(static_cast<size_t>(count / 8));

        for (int i = 0; i < count; i++) {
            UObject* obj = EngineDiscovery::GetObjectByIndex(state, i);
            if (!obj) continue;
            UClass* cls = EngineDiscovery::GetObjectClass(state, obj);
            if (!cls) continue;

            if (IsClass(state, cls)) {
                classObjects.push_back(obj);
            } else if (IsScriptStruct(state, cls)) {
                structObjects.push_back(obj);
            } else if (IsEnum(state, cls)) {
                enumObjects.push_back(obj);
            }
        }

        struct ClassLayoutInfo {
            int32_t LiveSize = 0;
            int32_t LayoutEnd = 0;
            int32_t Alignment = 0;
            int32_t FirstOwnOffset = -1;
            int32_t LastOwnOffset = -1;

            bool Packed = false;
            bool EmitAlignas = false;
        };
        int layoutFallbacks = 0;
        const auto computeLayouts = [&](const std::vector<UObject*>& objects,
                                        std::unordered_map<UObject*, ClassLayoutInfo>& layouts,
                                        bool allowUnalignedShrink) {
            std::unordered_map<UObject*, int32_t> firstChildMember;
            for (UObject* obj : objects) {
                if (!obj) continue;
                ClassLayoutInfo info;
                info.LiveSize = ReadStructPropertySize(state, obj);
                info.LayoutEnd = info.LiveSize;
                info.Alignment = ReadStructAlignment(state, obj);
                const OwnPropertyRange own = ReadOwnPropertyRange(state, obj);
                info.FirstOwnOffset = own.FirstOffset;
                info.LastOwnOffset = own.LastOffset;
                layouts.emplace(obj, info);
            }
            for (const auto& entry : layouts) {
                const ClassLayoutInfo& info = entry.second;
                if (info.FirstOwnOffset <= 0) continue;
                UObject* super = (UObject*)EngineDiscovery::GetSuperStruct(state, entry.first);
                if (!super) continue;
                auto it = firstChildMember.find(super);
                if (it == firstChildMember.end() || info.FirstOwnOffset < it->second)
                    firstChildMember[super] = info.FirstOwnOffset;
            }
            int shrunk = 0;
            for (auto& entry : layouts) {
                ClassLayoutInfo& info = entry.second;
                auto it = firstChildMember.find(entry.first);
                if (it == firstChildMember.end()) continue;
                const int32_t candidate = it->second;
                if (candidate <= 0 || candidate >= info.LiveSize) continue;
                const bool fitsOwnMembers = info.LastOwnOffset < candidate;
                const bool alignmentRestoresSize =
                    info.Alignment > 1 && AlignUp(candidate, info.Alignment) == info.LiveSize;

                if (fitsOwnMembers && (allowUnalignedShrink || alignmentRestoresSize)) {
                    info.LayoutEnd = candidate;
                    info.Packed = true;
                    info.EmitAlignas = alignmentRestoresSize;
                    ++shrunk;
                } else {
                    ++layoutFallbacks;
                    SdkLog("[SDK] Layout candidate left at the live size: %s (size 0x%X, first child member 0x%X, last own 0x%X, align 0x%X)",
                           EngineDiscovery::GetObjectNameStr(state, entry.first).c_str(),
                           info.LiveSize, candidate, info.LastOwnOffset, info.Alignment);
                }
            }
            return shrunk;
        };
        std::unordered_map<UObject*, ClassLayoutInfo> classLayouts;
        std::unordered_map<UObject*, ClassLayoutInfo> structLayouts;
        const int shrunkClasses = computeLayouts(classObjects, classLayouts, true);
        const int shrunkStructs = computeLayouts(structObjects, structLayouts, false);
        SdkLog("[SDK] Layout: %d class(es) and %d struct(s) packed below the live size (members inside a base's alignment tail), %d candidate(s) left at the live size",
               shrunkClasses, shrunkStructs, layoutFallbacks);

        std::unordered_map<std::string, int32_t> alignasStructs;

        SdkLog("[SDK] Phase 1: Collecting enums (%zu candidates)...", enumObjects.size());
        std::map<std::string, PackageStreams> packages;
        std::map<std::string, std::set<std::string>> packageDependencies;

        std::map<std::string, std::vector<PendingTypeDef>> pendingStructs;
        std::map<std::string, std::vector<PendingTypeDef>> pendingClasses;
        int enumCount = 0;

        int shiftedLayoutCount = 0;

        struct PendingEnum {
            std::string PkgName;
            std::string CppName;
            std::string FullPath;
            std::string SignatureHash;
            void* EnumObject = nullptr;
            EnumInfo Info;
            EnumValueRange Range;
        };
        std::vector<PendingEnum> pendingEnums;

        for (UObject* obj : enumObjects) {
            if (!obj) continue;

            EnumInfo info;
            if (!ReadEnumValues(state, obj, info)) continue;

            std::string rawObjectName = GetCachedObjectName(state, obj);
            if (rawObjectName.empty() || rawObjectName.find("Default__") == 0 || rawObjectName.find("SKEL_") == 0 || rawObjectName.find("REINST_") == 0) continue;

            std::string objectName = GetCachedCleanObjectName(state, obj);

            std::string pkgName = GetPackageName(state, obj, packageCollisionCounter, rawToUniquePackageName);
            info.packageName = pkgName;
            const std::string enumCppName = GetUniqueTypeCppName(obj, MakeEnumCppName(CleanClassName(info.name)));
            const std::string enumFullPath = GetCachedFullName(state, obj);
            uint64_t enumHash = 1469598103934665603ULL;
            enumHash = HashStringFNV1a(enumHash, enumCppName);
            enumHash = HashStringFNV1a(enumHash, enumFullPath);
            for (const auto& value : info.values) {
                enumHash = HashStringFNV1a(enumHash, CleanClassName(value.first));
                enumHash = HashUInt64FNV1a(enumHash, static_cast<uint64_t>(value.second));
            }
            const std::string enumSignatureHash = FormatHashHex(enumHash);

            PackageStreams& pkg = packages[pkgName];
            pkg.hasEnums = true;
            pkg.hasStructs = true;
            packageSummaries[pkgName].Enums++;

            const EnumValueRange enumRange = ComputeEnumValueRange(info.values, info.name);
            pendingEnums.push_back({ pkgName, enumCppName, enumFullPath, enumSignatureHash,
                                     obj, info, enumRange });

            metadataEntries.push_back({
                "enum",
                pkgName,
                info.name,
                enumCppName,
                enumFullPath,
                "",
                enumSignatureHash,
                0,
                0,
                0,
                0,
                0,
                static_cast<int32_t>(info.values.size()),
                0
            });
            reflectionEnums.push_back({
                MakeReflectionId("enum", enumFullPath),
                pkgName,
                info.name,
                enumCppName,
                enumFullPath,
                enumSignatureHash,
                info.values
            });
            enumCount++;
        }
        SdkLog("[SDK] Serialized %d enums", enumCount);

        SdkLog("[SDK] Phase 2: Collecting structs (%zu candidates)...", structObjects.size());
        int structCount = 0;

        for (UObject* obj : structObjects) {
            if (!obj) continue;

            std::string rawObjectName = GetCachedObjectName(state, obj);
            if (rawObjectName.empty() || rawObjectName.find("Default__") == 0 || rawObjectName.find("SKEL_") == 0 || rawObjectName.find("REINST_") == 0) continue;

            std::string objectName = GetCachedCleanObjectName(state, obj);

            std::string superName = "";
            std::string superCppName = "";
            UObject* superField = (UObject*)EngineDiscovery::GetSuperStruct(state, obj);
            std::string pkgName = GetPackageName(state, obj, packageCollisionCounter, rawToUniquePackageName);

            if (superField) {
                superName = GetCachedCleanObjectName(state, superField);
                std::string superPkgName = GetPackageName(state, superField, packageCollisionCounter, rawToUniquePackageName);
                if (pkgName != superPkgName) {
                    packageDependencies[pkgName].insert(superPkgName);
                }
                superCppName = GetUniqueTypeCppName(superField, "F" + superName);
            }

            int32_t structSize = ReadStructPropertySize(state, obj);

            int32_t superSize = 0;
            if (superField) superSize = ReadStructPropertySize(state, superField);

            ClassLayoutInfo structLayout;
            {
                auto it = structLayouts.find(obj);
                if (it != structLayouts.end()) structLayout = it->second;
                else {
                    structLayout.LiveSize = structSize;
                    structLayout.LayoutEnd = structSize;
                    structLayout.Alignment = ReadStructAlignment(state, obj);
                }
                if (structLayout.LayoutEnd <= 0 || structLayout.LayoutEnd > structSize)
                    structLayout.LayoutEnd = structSize;
            }

            const int32_t superAlignment = superField ? ReadStructAlignment(state, superField) : 1;
            int32_t layoutBase = superField ? AlignUp(superSize, superAlignment) : 0;
            int32_t propFilterBase = superSize;
            if (superField) {
                auto it = structLayouts.find(superField);
                if (it != structLayouts.end() && it->second.Packed) {
                    layoutBase = it->second.LayoutEnd;
                    propFilterBase = it->second.LayoutEnd;
                }
            }

            std::string safeTypeName = GetUniqueTypeCppName(obj, "F" + objectName);
            const std::string structFullPath = GetCachedFullName(state, obj);

            PackageStreams& pkg = packages[pkgName];
            pkg.hasStructs = true;
            packageSummaries[pkgName].Structs++;

            int32_t ownSize = structSize - superSize;
            TypeDependencyContext& depCtx = GetTypeDependencyContext();
            depCtx.Reset(state, packageCollisionCounter, rawToUniquePackageName, pkgName);
            auto props = CollectProperties(state, obj, propFilterBase);
            depCtx.Finish(packageDependencies);
            {

                std::set<std::string> memberScope;
                memberScope.insert(safeTypeName);
                for (auto& p : props) p.Name = MakeUniqueName(p.Name, memberScope);
            }
            uint64_t structHash = 1469598103934665603ULL;
            structHash = HashStringFNV1a(structHash, safeTypeName);
            structHash = HashStringFNV1a(structHash, structFullPath);
            structHash = HashStringFNV1a(structHash, superName);
            structHash = HashUInt64FNV1a(structHash, static_cast<uint64_t>(structSize));
            for (const SDKProperty& prop : props)
                structHash = HashUInt64FNV1a(structHash, prop.Fingerprint);
            const std::string structSignatureHash = FormatHashHex(structHash);

            std::ostringstream oneStruct;
            const int32_t liveAlignment = ReadStructAlignment(state, obj);

            const int32_t structStorageSize = AlignUp(structSize, liveAlignment);

            const int32_t structBodyEnd = structLayout.Packed ? structLayout.LayoutEnd : structStorageSize;

            const bool packStruct = structLayout.Packed || liveAlignment == 1;

            int32_t effectiveAlignment = structLayout.EmitAlignas ? structLayout.Alignment : 1;
            for (UObject* cur = superField; cur; cur = (UObject*)EngineDiscovery::GetSuperStruct(state, cur)) {
                auto it = structLayouts.find(cur);
                if (it != structLayouts.end() && it->second.EmitAlignas &&
                    it->second.Alignment > effectiveAlignment) {
                    effectiveAlignment = it->second.Alignment;
                }
            }
            const int32_t emittedStructSize = AlignUp(structBodyEnd, effectiveAlignment);
            if (structLayout.EmitAlignas && liveAlignment > 1 && !safeTypeName.empty())
                alignasStructs[safeTypeName] = liveAlignment;
            oneStruct << "// ScriptStruct " << pkgName << "." << objectName << "\n";
            oneStruct << "// Full path: " << structFullPath << "\n";
            oneStruct << "// Super: " << (superName.empty() ? "<none>" : superName) << "\n";
            oneStruct << "// Hash: " << structSignatureHash << "\n";
            oneStruct << "// 0x" << std::hex << ownSize << " (0x" << structSize << " - 0x" << superSize << ")" << std::dec << "\n";
            if (structLayout.EmitAlignas) {
                oneStruct << "// Members end at 0x" << std::hex << structLayout.LayoutEnd
                          << "; alignas(0x" << liveAlignment << ") keeps sizeof at 0x" << emittedStructSize << std::dec << "\n";
            } else if (structLayout.Packed) {
                oneStruct << "// Members end at 0x" << std::hex << structLayout.LayoutEnd
                          << "; packed body is the C++ sizeof (0x" << emittedStructSize << ")" << std::dec << "\n";
            }
            if (packStruct) oneStruct << "#pragma pack(push, 1)\n";
            if (!superCppName.empty()) {
                oneStruct << "struct ";
                if (structLayout.EmitAlignas && liveAlignment > 1)
                    oneStruct << "alignas(0x" << std::hex << liveAlignment << std::dec << ") ";
                oneStruct << safeTypeName << " : public " << superCppName << "\n{\npublic:\n";
            } else {
                oneStruct << "struct ";
                if (structLayout.EmitAlignas && liveAlignment > 1)
                    oneStruct << "alignas(0x" << std::hex << liveAlignment << std::dec << ") ";
                oneStruct << safeTypeName << "\n{\npublic:\n";
            }

            std::vector<int32_t> emittedOffsets;
            WriteMembers(oneStruct, props, layoutBase, structBodyEnd, safeTypeName, &emittedOffsets);

            int32_t layoutShift = 0;
            for (size_t i = 0; i < props.size() && i < emittedOffsets.size(); ++i) {
                if (props[i].IsBitfield) continue;
                if (emittedOffsets[i] != props[i].Offset) {
                    layoutShift = emittedOffsets[i] - props[i].Offset;
                    break;
                }
            }
            oneStruct << "};\n";
            if (packStruct) oneStruct << "#pragma pack(pop)\n";
            oneStruct << "\n";
            if (layoutShift > 0) {
                shiftedLayoutCount++;
                oneStruct << "// Layout: members sit 0x" << std::hex << layoutShift
                          << " bytes later than the game places them (C++ base 0x" << layoutBase
                          << ", engine base 0x" << superSize << ")\n\n" << std::dec;
            }
            oneStruct << "AVALONIUM_ASSERT_LAYOUT(" << safeTypeName << ");\n\n";

            {
                StructAssertionInfo assertInfo;
                assertInfo.CppName = safeTypeName;
                assertInfo.Size = emittedStructSize;
                assertInfo.Alignment = liveAlignment;
                assertInfo.LayoutShift = layoutShift;
                assertInfo.LiveBaseSize = superSize;
                assertInfo.CppBaseSize = layoutBase;
                for (const auto& p : props) {
                    assertInfo.Members.push_back({ p.Name, p.Offset, p.IsBitfield });
                }
                structAssertions.push_back(std::move(assertInfo));
            }

            {
                std::set<std::string> structDeps;
                if (!superCppName.empty()) structDeps.insert(superCppName);
                for (const SDKProperty& prop : props) CollectStructRefsFromType(prop.Type, structDeps);
                structDeps.erase(safeTypeName);
                pendingStructs[pkgName].push_back({ safeTypeName, oneStruct.str(), std::move(structDeps) });
            }

            metadataEntries.push_back({
                "struct",
                pkgName,
                objectName,
                safeTypeName,
                structFullPath,
                superName,
                structSignatureHash,
                structSize,
                superSize,
                ownSize,
                static_cast<int32_t>(props.size()),
                0,
                0,
                0
            });
            const std::string typeId = MakeReflectionId("type", structFullPath);
            reflectionTypes.push_back({
                typeId,
                "struct",
                pkgName,
                objectName,
                safeTypeName,
                structFullPath,
                superName,
                superName.empty() ? "" : "F" + superName,
                structSignatureHash,
                structSize,
                superSize,
                ownSize,
                static_cast<int32_t>(props.size()),
                0
            });
            for (const SDKProperty& prop : props) {
                reflectionProperties.push_back(MakeReflectionPropertyRecord(
                    prop,
                    typeId,
                    "struct",
                    pkgName,
                    objectName,
                    safeTypeName,
                    "member"));
            }
            structCount++;
        }
        SdkLog("[SDK] Serialized %d structs", structCount);

        SdkLog("[SDK] Phase 3: Collecting classes and functions (%zu candidates)...", classObjects.size());
        int classCount = 0;
        int funcCount = 0;
        int paramStructCount = 0;

        for (UObject* obj : classObjects) {
            if (!obj) continue;

            const std::string rawObjectName = GetCachedObjectName(state, obj);
            if (rawObjectName.empty() || rawObjectName.find("Default__") == 0 || rawObjectName.find("SKEL_") == 0 || rawObjectName.find("REINST_") == 0) continue;

            std::string objectName = GetCachedCleanObjectName(state, obj);

            std::string superName = "";
            UObject* superField = (UObject*)EngineDiscovery::GetSuperStruct(state, obj);
            std::string pkgName = GetPackageName(state, obj, packageCollisionCounter, rawToUniquePackageName);

            if (pkgName == "CoreUObject" && IsCorePredefinedClass(rawObjectName)) {
                objectName = GetCorePredefinedCanonicalName(rawObjectName);
            }

            if (superField) {
                const std::string rawSuperName = GetCachedObjectName(state, superField);
                std::string superPkgName = GetPackageName(state, superField, packageCollisionCounter, rawToUniquePackageName);
                if (superPkgName == "CoreUObject" && IsCorePredefinedClass(rawSuperName)) {
                    superName = GetCorePredefinedCanonicalName(rawSuperName);
                } else {
                    superName = GetCachedCleanObjectName(state, superField);
                }
                if (pkgName != superPkgName) {
                    packageDependencies[pkgName].insert(superPkgName);
                }
            }

            int32_t structSize = ReadStructPropertySize(state, obj);

            int32_t superSize = 0;
            if (superField) superSize = ReadStructPropertySize(state, superField);

            ClassLayoutInfo classLayout;
            {
                auto it = classLayouts.find(obj);
                if (it != classLayouts.end()) {
                    classLayout = it->second;
                } else {
                    classLayout.LiveSize = structSize;
                    classLayout.LayoutEnd = structSize;
                    classLayout.Alignment = ReadStructAlignment(state, obj);
                }
                if (classLayout.LayoutEnd <= 0 || classLayout.LayoutEnd > structSize)
                    classLayout.LayoutEnd = structSize;
            }
            int32_t layoutBase = superSize;
            if (superField) {
                auto it = classLayouts.find(superField);
                if (it != classLayouts.end()) layoutBase = it->second.LayoutEnd;
            }

            bool isActor = IsActorDerived(state, obj);
            std::string prefix = isActor ? "A" : "U";
            std::string safeTypeName = GetUniqueTypeCppName(obj, prefix + objectName);
            const std::string classFullPath = GetCachedFullName(state, obj);
            std::string superCppName = "";
            if (!superName.empty()) {
                std::string superPrefix = IsActorDerived(state, superField) ? "A" : "U";
                superCppName = GetUniqueTypeCppName(superField, superPrefix + superName);
            }

            PackageStreams& pkg = packages[pkgName];
            pkg.hasClasses = true;
            packageSummaries[pkgName].Classes++;

            int32_t ownSize = structSize - superSize;
            TypeDependencyContext& depCtx = GetTypeDependencyContext();
            depCtx.Reset(state, packageCollisionCounter, rawToUniquePackageName, pkgName);
            auto props = CollectProperties(state, obj, layoutBase);
            AppendNonReflectedClassMembers(state, pkgName, objectName, superSize, structSize, props);
            auto funcs = CollectFunctions(state, obj, pkgName, objectName);
            depCtx.Finish(packageDependencies);
            int32_t reflectedPropCount = 0;
            for (const SDKProperty& prop : props) {
                if (!prop.IsSynthetic) ++reflectedPropCount;
            }
            {

                std::set<std::string> classScope;
                classScope.insert(safeTypeName);
                classScope.insert("StaticClass");
                for (auto& p : props) p.Name = MakeUniqueName(p.Name, classScope);
                for (auto& fn : funcs) fn.Name = MakeUniqueName(fn.Name, classScope);
            }
            uint64_t classHash = 1469598103934665603ULL;
            classHash = HashStringFNV1a(classHash, safeTypeName);
            classHash = HashStringFNV1a(classHash, classFullPath);
            classHash = HashStringFNV1a(classHash, superName);
            classHash = HashUInt64FNV1a(classHash, static_cast<uint64_t>(structSize));
            for (const SDKProperty& prop : props)
                classHash = HashUInt64FNV1a(classHash, prop.Fingerprint);
            for (const SDKFunction& fn : funcs)
                classHash = HashUInt64FNV1a(classHash, fn.Fingerprint);
            const std::string classSignatureHash = FormatHashHex(classHash);

            packageDefinedClasses[pkgName].insert(safeTypeName);

            const bool isCorePredefined = (pkgName == "CoreUObject" && IsCorePredefinedClass(objectName));
            if (isCorePredefined) {

                const int32_t predefinedBase = superField ? layoutBase : 0;
                const int32_t predefinedSize =
                    classLayout.LayoutEnd > 0 ? classLayout.LayoutEnd : structSize;
                std::string decl =
                    GetPredefinedClassDeclaration(state, objectName, predefinedBase, predefinedSize);
                std::set<std::string> classDeps;
                if (!superCppName.empty()) classDeps.insert(superCppName);
                classDeps.erase(safeTypeName);
                pendingClasses[pkgName].push_back({ safeTypeName, decl, std::move(classDeps) });

                const std::string impl = GetPredefinedClassImplementation(state, objectName);
                if (!impl.empty()) {
                    pkg.functions << "// Predefined methods for " << safeTypeName << "\n";
                    pkg.functions << impl << "\n\n";
                    pkg.hasFunctions = true;
                }

                const std::string typeId = MakeReflectionId("type", classFullPath);
                for (const SDKProperty& prop : props) {
                    if (prop.IsSynthetic) continue;
                    reflectionProperties.push_back(MakeReflectionPropertyRecord(
                        prop,
                        typeId,
                        "class",
                        pkgName,
                        objectName,
                        safeTypeName,
                        "member"));
                }

                metadataEntries.push_back({
                    "class",
                    pkgName,
                    objectName,
                    safeTypeName,
                    classFullPath,
                    superName,
                    classSignatureHash,
                    structSize,
                    superSize,
                    ownSize,
                    static_cast<int32_t>(reflectedPropCount),
                    0,
                    0,
                    0
                });
                reflectionTypes.push_back({
                    typeId,
                    "class",
                    pkgName,
                    objectName,
                    safeTypeName,
                    classFullPath,
                    superName,
                    superCppName,
                    classSignatureHash,
                    structSize,
                    superSize,
                    ownSize,
                    static_cast<int32_t>(reflectedPropCount),
                    0
                });
                classCount++;
                continue;
            }

            std::ostringstream oneClass;
            const int32_t liveClassAlignment = classLayout.Alignment > 0 ? classLayout.Alignment : ReadStructAlignment(state, obj);

            int32_t effectiveAlignment = 1;
            for (UObject* cur = obj; cur; cur = (UObject*)EngineDiscovery::GetSuperStruct(state, cur)) {
                auto it = classLayouts.find(cur);
                if (it != classLayouts.end() && it->second.EmitAlignas &&
                    it->second.Alignment > effectiveAlignment) {
                    effectiveAlignment = it->second.Alignment;
                }
            }
            for (const SDKProperty& member : props) {
                std::string memberType = member.Type;
                if (memberType.compare(0, 7, "struct ") == 0) memberType.erase(0, 7);
                else if (memberType.compare(0, 6, "class ") == 0) memberType.erase(0, 6);
                auto sit = alignasStructs.find(memberType);
                if (sit != alignasStructs.end() && sit->second > effectiveAlignment)
                    effectiveAlignment = sit->second;
            }
            const int32_t emittedClassSize = AlignUp(classLayout.LayoutEnd, effectiveAlignment);
            oneClass << "// Class " << pkgName << "." << objectName << "\n";
            oneClass << "// Full path: " << classFullPath << "\n";
            oneClass << "// Super: " << (superName.empty() ? "<none>" : superName) << "\n";
            oneClass << "// Hash: " << classSignatureHash << "\n";
            oneClass << "// 0x" << std::hex << ownSize << " (0x" << structSize << " - 0x" << superSize << ")" << std::dec << "\n";
            if (classLayout.EmitAlignas) {
                oneClass << "// Members end at 0x" << std::hex << classLayout.LayoutEnd
                         << "; alignas(0x" << liveClassAlignment << ") keeps sizeof at 0x" << structSize << std::dec << "\n";
            } else if (emittedClassSize != structSize) {
                oneClass << "// Packed body ends at 0x" << std::hex << classLayout.LayoutEnd
                         << "; an aligned base rounds sizeof to 0x" << emittedClassSize << std::dec << "\n";
            }

            oneClass << "#pragma pack(push, 1)\n";
            oneClass << "class ";
            if (classLayout.EmitAlignas && liveClassAlignment > 1) {
                oneClass << "alignas(0x" << std::hex << liveClassAlignment << std::dec << ") ";
            }
            if (!superName.empty()) {
                oneClass << safeTypeName << " : public " << superCppName << "\n{\npublic:\n";
            } else {
                oneClass << safeTypeName << "\n{\npublic:\n";
            }

            std::vector<int32_t> emittedOffsets;
            WriteMembers(oneClass, props, layoutBase, classLayout.LayoutEnd, safeTypeName, &emittedOffsets);

            int32_t layoutShift = 0;
            for (size_t i = 0; i < props.size() && i < emittedOffsets.size(); ++i) {
                if (props[i].IsBitfield) continue;
                if (emittedOffsets[i] != props[i].Offset) {
                    layoutShift = emittedOffsets[i] - props[i].Offset;
                    break;
                }
            }
            if (layoutShift > 0) {
                shiftedLayoutCount++;
            }

            {
                StructAssertionInfo assertInfo;
                assertInfo.CppName = safeTypeName;
                assertInfo.Size = emittedClassSize;
                assertInfo.Alignment = liveClassAlignment;
                assertInfo.IsClass = true;
                assertInfo.LayoutShift = layoutShift;
                assertInfo.LiveBaseSize = superSize;
                assertInfo.CppBaseSize = layoutBase;
                for (const auto& p : props) {
                    assertInfo.Members.push_back({ p.Name, p.Offset, p.IsBitfield });
                }
                structAssertions.push_back(std::move(assertInfo));
            }

            const std::string typeId = MakeReflectionId("type", classFullPath);
            for (const SDKProperty& prop : props) {
                if (prop.IsSynthetic) continue;
                reflectionProperties.push_back(MakeReflectionPropertyRecord(
                    prop,
                    typeId,
                    "class",
                    pkgName,
                    objectName,
                    safeTypeName,
                    "member"));
            }

            if (!funcs.empty()) {
                oneClass << "\n";
                pkg.hasFunctions = true;

                for (const auto& fn : funcs) {
                    const std::string functionId = MakeReflectionId("function", fn.FullPath);
                    const bool isStaticFunc = (fn.FunctionFlags & FUNC_Static) != 0;
                    const bool isConstFunc = (fn.FunctionFlags & FUNC_Const) != 0 && !isStaticFunc;
                    const std::string paramStructName = safeTypeName + "_" + fn.Name + "_Params";
                    const std::string lookupClassName = EscapeCppString(rawObjectName);
                    const std::string lookupFuncName = EscapeCppString(fn.RawName.empty() ? fn.Name : fn.RawName);

                    oneClass << "    // " << fn.FullPath << "\n";
                    oneClass << "    // Flags: 0x" << std::hex << fn.FunctionFlags << std::dec
                        << " (" << fn.FlagsText << ")\n";
                    oneClass << "    // Hash: " << fn.SignatureHash << "\n";
                    oneClass << "    ";
                    if (isStaticFunc) oneClass << "static ";
                    oneClass << fn.ReturnType << " " << fn.Name << "(";

                    bool first = true;
                    for (const auto& p : fn.Params) {
                        if (p.IsLocal || p.IsReturnParam || p.Name == "ReturnValue") continue;
                        if (!first) oneClass << ", ";
                        oneClass << RenderFunctionArgument(p);
                        first = false;
                    }
                    oneClass << ")";
                    if (isConstFunc) oneClass << " const";
                    oneClass << ";\n";

                    if (!fn.Params.empty()) {
                        pkg.hasParameters = true;

                        pkg.parameters << "// " << fn.FullPath << "\n";
                        pkg.parameters << "// Flags: 0x" << std::hex << fn.FunctionFlags << std::dec
                            << " (" << fn.FlagsText << ")\n";
                        pkg.parameters << "// Hash: " << fn.SignatureHash << "\n";

                        pkg.parameters << "#pragma pack(push, 1)\n";
                        pkg.parameters << "struct " << paramStructName << "\n{\npublic:\n";
                        WriteMembers(pkg.parameters, fn.Params, 0, fn.ParameterSize, paramStructName);
                        pkg.parameters << "};\n#pragma pack(pop)\n\n";
                        pkg.parameters << "AVALONIUM_ASSERT_LAYOUT(" << paramStructName << ");\n\n";
                        paramStructCount++;

                        if (fn.ParameterSize > 0) {
                            StructAssertionInfo paramAssertInfo;
                            paramAssertInfo.CppName = paramStructName;
                            paramAssertInfo.Size = fn.ParameterSize;
                            for (const auto& p : fn.Params) {
                                paramAssertInfo.Members.push_back({ p.Name, p.Offset, p.IsBitfield });
                            }
                            structAssertions.push_back(std::move(paramAssertInfo));
                        }
                    }

                    pkg.functions << "// " << fn.FullPath << "\n";
                    pkg.functions << "// Flags: 0x" << std::hex << fn.FunctionFlags << std::dec
                        << " (" << fn.FlagsText << ")\n";
                    pkg.functions << "// Hash: " << fn.SignatureHash << "\n";
                    pkg.functions << fn.ReturnType << " " << safeTypeName << "::" << fn.Name << "(";
                    first = true;
                    for (const auto& p : fn.Params) {
                        if (p.IsLocal || p.IsReturnParam || p.Name == "ReturnValue") continue;
                        if (!first) pkg.functions << ", ";
                        pkg.functions << RenderFunctionArgument(p);
                        first = false;
                    }
                    pkg.functions << ")";
                    if (isConstFunc) pkg.functions << " const";
                    pkg.functions << "\n{\n";

                    pkg.functions << "    static SDKHelpers::ReloadableFuncInfo FuncInfo{};\n\n";
                    pkg.functions << "    class UFunction* Func = SDKHelpers::GetCachedFunction(FuncInfo);\n\n";
                    pkg.functions << "    if (Func == nullptr)\n";
                    if (isStaticFunc) {
                        pkg.functions << "        Func = StaticClass() ? StaticClass()->GetFunction(\"" << lookupClassName << "\", \"" << lookupFuncName << "\") : SDKHelpers::GetFunction(\"" << lookupClassName << "\", \"" << lookupFuncName << "\");\n\n";
                    } else {
                        pkg.functions << "        Func = Class ? Class->GetFunction(\"" << lookupClassName << "\", \"" << lookupFuncName << "\") : SDKHelpers::GetFunction(\"" << lookupClassName << "\", \"" << lookupFuncName << "\");\n\n";
                    }
                    pkg.functions << "    SDKHelpers::SetCachedFunction(Func, FuncInfo);\n\n";

                    const std::string callTarget = isStaticFunc
                        ? "SDKHelpers::GetDefaultObject(\"" + lookupClassName + "\")"
                        : "this";

                    if (!fn.Params.empty()) {
                        pkg.functions << "    " << paramStructName << " Parms{};\n";
                        for (const auto& p : fn.Params) {
                            if (p.IsLocal || p.IsReturnParam || p.Name == "ReturnValue") continue;
                            pkg.functions << "    Parms." << p.Name << " = " << p.Name << ";\n";
                        }
                        pkg.functions << "\n    SDKHelpers::ProcessEvent(" << callTarget << ", Func, &Parms);\n";
                        for (const auto& p : fn.Params) {
                            if (p.IsLocal || p.IsReturnParam || p.Name == "ReturnValue") continue;
                            if (p.IsOutParam || p.IsReferenceParam)
                                pkg.functions << "    " << p.Name << " = Parms." << p.Name << ";\n";
                        }
                        if (fn.ReturnType != "void" && !fn.ReturnName.empty()) {
                            pkg.functions << "\n    return Parms." << fn.ReturnName << ";\n";
                        }
                    } else {
                        pkg.functions << "    SDKHelpers::ProcessEvent(" << callTarget << ", Func, nullptr);\n";
                    }
                    pkg.functions << "}\n\n";
                    packageSummaries[pkgName].Functions++;
                    metadataEntries.push_back({
                        "function",
                        pkgName,
                        fn.Name,
                        safeTypeName + "::" + fn.Name,
                        fn.FullPath,
                        safeTypeName,
                        fn.SignatureHash,
                        fn.ParameterSize,
                        0,
                        0,
                        static_cast<int32_t>(fn.Params.size()),
                        0,
                        0,
                        fn.FunctionFlags
                    });
                    reflectionFunctions.push_back({
                        functionId,
                        typeId,
                        "class",
                        pkgName,
                        objectName,
                        safeTypeName,
                        fn.Name,
                        fn.FullPath,
                        fn.ReturnType,
                        fn.ReturnName,
                        fn.FunctionFlags,
                        fn.ParameterSize,
                        fn.SignatureHash,
                        fn.Params
                    });
                    for (const SDKProperty& param : fn.Params) {
                        const std::string role = (param.IsReturnParam || param.Name == "ReturnValue") ? "return" : "param";
                        reflectionProperties.push_back(MakeReflectionPropertyRecord(
                            param,
                            functionId,
                            "function",
                            pkgName,
                            fn.Name,
                            safeTypeName + "::" + fn.Name,
                            role));
                    }
                    funcCount++;
                }
            }

            oneClass << "\n    static class UClass* StaticClass()\n";
            oneClass << "    {\n";
            oneClass << "        static SDKHelpers::StaticClassInfo Info{};\n";
            oneClass << "        return SDKHelpers::GetStaticClass(\"" << EscapeCppString(rawObjectName) << "\", Info);\n";
            oneClass << "    }\n\n";
            oneClass << "    static " << safeTypeName << "* GetDefaultObj()\n";
            oneClass << "    {\n";
            oneClass << "        return reinterpret_cast<" << safeTypeName << "*>(SDKHelpers::GetDefaultObject(\"" << EscapeCppString(rawObjectName) << "\"));\n";
            oneClass << "    }\n";
            oneClass << "};\n#pragma pack(pop)\n\n";
            if (layoutShift > 0) {
                oneClass << "// Layout: members sit 0x" << std::hex << layoutShift
                         << " bytes later than in the game (C++ base 0x" << layoutBase
                         << ", engine 0x" << superSize << ")\n\n" << std::dec;
            }
            oneClass << "AVALONIUM_ASSERT_LAYOUT(" << safeTypeName << ");\n\n";

            {
                std::set<std::string> classDeps;
                if (!superCppName.empty()) classDeps.insert(superCppName);
                classDeps.erase(safeTypeName);
                pendingClasses[pkgName].push_back({ safeTypeName, oneClass.str(), std::move(classDeps) });
            }

            metadataEntries.push_back({
                "class",
                pkgName,
                objectName,
                safeTypeName,
                classFullPath,
                superName,
                classSignatureHash,
                structSize,
                superSize,
                ownSize,
                static_cast<int32_t>(reflectedPropCount),
                static_cast<int32_t>(funcs.size()),
                0,
                0
            });
            reflectionTypes.push_back({
                typeId,
                "class",
                pkgName,
                objectName,
                safeTypeName,
                classFullPath,
                superName,
                superCppName,
                classSignatureHash,
                structSize,
                superSize,
                ownSize,
                static_cast<int32_t>(reflectedPropCount),
                static_cast<int32_t>(funcs.size())
            });

            classCount++;
        }
        SdkLog("[SDK] Serialized %d classes, %d functions, %d parameter structs", classCount, funcCount, paramStructCount);

        {
            const GenerationCaches& caches = GetGenerationCaches();
            const int32_t ftextSize = caches.FTextSize > 0 ? caches.FTextSize : 0x18;
            const int32_t lazyObjectSize = caches.LazyObjectSize > 0 ? caches.LazyObjectSize : 0x1C;
            std::string propertyFixup = GetPropertyFixupHeaderContent();
            const auto replacePadding = [&propertyFixup](const char* className, const char* oldPad, int32_t newSize) {
                if (newSize <= 0) return;
                const size_t classPos = propertyFixup.find(className);
                if (classPos == std::string::npos) return;
                const size_t padPos = propertyFixup.find(oldPad, classPos);
                if (padPos == std::string::npos) return;
                std::ostringstream newPad;
                newPad << "uint8_t Padding[0x" << std::hex << newSize << "];";
                propertyFixup.replace(padPos, std::string(oldPad).size(), newPad.str());
            };
            replacePadding("class FText", "uint8_t Padding[0x18];", ftextSize);
            replacePadding("class TLazyObjectPtr", "uint8_t Padding[0x1C];", lazyObjectSize);
            WriteFile(outputDir, "PropertyFixup.hpp", propertyFixup);
        }

        for (const PendingEnum& e : pendingEnums) {
            PackageStreams& pkg = packages[e.PkgName];
            const uint32_t rangeBytes = EnumUnderlyingBytes(e.Range.Min, e.Range.Max);
            const int32_t observedBytes = GetObservedEnumBytes(state, e.EnumObject);
            const bool rangeSigned = e.Range.Min < 0;
            const uint32_t underlyingBytes =
                (observedBytes > 0 && static_cast<uint32_t>(observedBytes) > rangeBytes)
                    ? static_cast<uint32_t>(observedBytes) : rangeBytes;
            const char* underlyingType = EnumUnderlyingTypeNameForBytes(underlyingBytes, rangeSigned);

            const std::string sentinelName = e.Info.name + "_MAX";
            const bool omitSentinel = e.Range.HasSentinel &&
                !ValueFitsUnderlyingType(e.Range.SentinelValue, underlyingBytes, rangeSigned);

            std::vector<std::pair<std::string, int64_t>> emittedValues;
            emittedValues.reserve(e.Info.values.size());
            for (const auto& value : e.Info.values) {
                if (omitSentinel && value.first == sentinelName) continue;
                emittedValues.push_back(value);
            }

            const auto formatEnumValue = [](int64_t value) -> std::string {
                if (value == (std::numeric_limits<int64_t>::min)()) {
                    return "(-9223372036854775807LL - 1LL)";
                }
                return std::to_string(value);
            };

            pkg.enums << "// Enum " << e.PkgName << "." << e.CppName << "\n";
            pkg.enums << "// Full path: " << e.FullPath << "\n";
            pkg.enums << "// Hash: " << e.SignatureHash << "\n";
            pkg.enums << "enum class " << e.CppName << " : " << underlyingType << "\n{\n";
            if (omitSentinel) {
                pkg.enums << "    // " << sentinelName << " (" << e.Range.SentinelValue
                          << ") is UHT metadata and does not fit " << underlyingType << "; omitted\n";
            }
            if (emittedValues.empty()) {
                pkg.enums << "    _Empty = 0\n";
            } else {
                std::set<std::string> usedValueNames;
                for (size_t v = 0; v < emittedValues.size(); v++) {
                    const std::string valueName = MakeUniqueName(CleanClassName(emittedValues[v].first), usedValueNames);
                    pkg.enums << "    " << valueName;
                    pkg.enums << " = " << formatEnumValue(emittedValues[v].second);
                    if (v < emittedValues.size() - 1) pkg.enums << ",";
                    pkg.enums << "\n";
                }
            }
            pkg.enums << "};\n";
            if (omitSentinel) {
                pkg.enums << "constexpr int32_t " << CleanClassName(sentinelName) << " = "
                          << formatEnumValue(e.Range.SentinelValue) << ";\n";
            }
            pkg.enums << "\n";
        }
        SdkLog("[SDK] Phase 4: Sorting dependencies and writing files...");

        std::set<std::string> cyclicPackages = DetectCyclicPackages(packageDependencies);
        if (!cyclicPackages.empty()) {
            SdkLog("[SDK] Detected %zu packages involved in cyclic dependencies", cyclicPackages.size());
        }

        std::vector<std::string> sortedPackages;
        std::set<std::string> visited;
        std::set<std::string> visiting;

        std::function<void(const std::string&)> topologicalSort = [&](const std::string& node) {
            if (visited.count(node)) return;
            if (visiting.count(node)) return;
            visiting.insert(node);

            for (const auto& dep : packageDependencies[node]) {
                topologicalSort(dep);
            }

            visiting.erase(node);
            visited.insert(node);
            sortedPackages.push_back(node);
        };

        for (const auto& kv : packages) {
            if (!visited.count(kv.first)) {
                topologicalSort(kv.first);
            }
        }

        int failedFiles = 0;
        int collisionCount = 0;
        for (const auto& kv : packageCollisionCounter) {
            if (kv.second > 1) collisionCount += kv.second - 1;
        }
        SdkLog("[SDK] Package name collisions resolved: %d", collisionCount);

        int totalFiles = 5;

        struct WriteTask {
            std::string FileName;
            std::string Content;
            bool IsSDKInclude = false;
            bool IsSDKHeader  = false;
        };
        std::vector<WriteTask> writeTasks;
        writeTasks.reserve(sortedPackages.size() * 4 + 4);

        std::string sdkIncludeList;
        sdkIncludeList.reserve(sortedPackages.size() * 32);

        for (const std::string& pkgName : sortedPackages) {
            if (packages.find(pkgName) == packages.end()) continue;
            auto& pkg = packages[pkgName];

            if (pkg.hasStructs || pkg.hasEnums) {
                std::string fileName = pkgName + "_structs.h";
                std::string content;
                content.reserve(128 * 1024);
                content.append("#pragma once\n\n#include \"Basic.h\"\n\n");
                content.append("// Package: ").append(pkgName).append("\n\n");
                auto depIt = packageDependencies.find(pkgName);
                if (depIt != packageDependencies.end()) {
                    for (const auto& dep : depIt->second) {
                        auto depPkgIt = packages.find(dep);
                        if (depPkgIt != packages.end() && (depPkgIt->second.hasStructs || depPkgIt->second.hasEnums)) {
                            content.append("#include \"").append(dep).append("_structs.h\"\n");
                        }
                    }
                }
                content.append(pkg.enums.str());
                content.append(pkg.structs.str());
                auto pendingStructsIt = pendingStructs.find(pkgName);
                if (pendingStructsIt != pendingStructs.end()) {
                    const auto& defs = pendingStructsIt->second;
                    if (!defs.empty()) {
                        content.append("// Forward declarations\n");
                        for (const auto& d : defs) {
                            content.append("struct ").append(d.cppName).append(";\n");
                        }
                        content.append("\n");
                    }
                    for (int idx : SortTypeDefOrder(defs)) {
                        content.append(defs[idx].text);
                    }
                }
                sdkIncludeList.append("#include \"").append(fileName).append("\"\n");
                writeTasks.push_back({fileName, std::move(content), true, true});
                totalFiles++;
            }

            if (pkg.hasClasses) {
                std::string fileName = pkgName + "_classes.h";
                std::string content;
                content.reserve(256 * 1024);
                content.append("#pragma once\n\n#include \"Basic.h\"\n");
                if (pkg.hasStructs || pkg.hasEnums) {
                    content.append("#include \"").append(pkgName).append("_structs.h\"\n");
                }
                auto depIt = packageDependencies.find(pkgName);
                if (depIt != packageDependencies.end()) {
                    for (const auto& dep : depIt->second) {
                        auto depPkgIt = packages.find(dep);
                        if (depPkgIt != packages.end() && (depPkgIt->second.hasStructs || depPkgIt->second.hasEnums)) {
                            content.append("#include \"").append(dep).append("_structs.h\"\n");
                        }
                    }
                    for (const auto& dep : depIt->second) {
                        auto depPkgIt = packages.find(dep);
                        if (depPkgIt != packages.end() && depPkgIt->second.hasClasses) {

                            if (cyclicPackages.count(pkgName) && cyclicPackages.count(dep)) {
                                continue;
                            }
                            content.append("#include \"").append(dep).append("_classes.h\"\n");
                        }
                    }
                }

                if (cyclicPackages.count(pkgName) && depIt != packageDependencies.end()) {
                    std::set<std::string> forwardClasses;
                    for (const auto& dep : depIt->second) {
                        if (cyclicPackages.count(dep)) {
                            auto clsIt = packageDefinedClasses.find(dep);
                            if (clsIt != packageDefinedClasses.end()) {
                                forwardClasses.insert(clsIt->second.begin(), clsIt->second.end());
                            }
                        }
                    }
                    if (!forwardClasses.empty()) {
                        content.append("\n").append(GenerateForwardDeclarations(forwardClasses, {})).append("\n");
                    }
                }
                content.append("\n// Package: ").append(pkgName).append("\n\n");
                auto pendingClassesIt = pendingClasses.find(pkgName);
                if (pendingClassesIt != pendingClasses.end()) {
                    const auto& defs = pendingClassesIt->second;
                    for (int idx : SortTypeDefOrder(defs)) {
                        content.append(defs[idx].text);
                    }
                }
                sdkIncludeList.append("#include \"").append(fileName).append("\"\n");
                writeTasks.push_back({fileName, std::move(content), true, true});
                totalFiles++;
            }

            if (pkg.hasParameters) {
                std::string fileName = pkgName + "_parameters.h";
                std::string content;
                content.reserve(64 * 1024);
                content.append("#pragma once\n\n#include \"Basic.h\"\n");
                if (pkg.hasClasses) {
                    content.append("#include \"").append(pkgName).append("_classes.h\"\n");
                }
                auto paramDepIt = packageDependencies.find(pkgName);
                if (paramDepIt != packageDependencies.end()) {
                    for (const auto& dep : paramDepIt->second) {
                        auto depPkgIt = packages.find(dep);
                        if (depPkgIt != packages.end() && (depPkgIt->second.hasStructs || depPkgIt->second.hasEnums)) {
                            content.append("#include \"").append(dep).append("_structs.h\"\n");
                        }
                    }
                }
                content.append("\n// Package: ").append(pkgName).append(" - Function Parameters\n\n");
                content.append(pkg.parameters.str());
                writeTasks.push_back({fileName, std::move(content), false, true});
                totalFiles++;
            }

            if (pkg.hasFunctions) {
                std::string fileName = pkgName + "_functions.cpp";
                std::string content;
                content.reserve(128 * 1024);
                content.append("#include \"SDK.h\"\n");
                if (pkg.hasParameters) {
                    content.append("#include \"").append(pkgName).append("_parameters.h\"\n");
                }
                content.append("\n// Package: ").append(pkgName).append(" - Function Implementations\n\n");
                content.append(pkg.functions.str());
                writeTasks.push_back({fileName, std::move(content), false, true});
                totalFiles++;
            }
        }

        {
            std::string assertionsContent;
            assertionsContent.reserve(512 * 1024);
            assertionsContent.append("#pragma once\n\n");
            assertionsContent.append("/*\n");
            assertionsContent.append(" * SDK Compile-Time Layout Assertions\n");
            assertionsContent.append(" * Generated by Avalonium\n");
            assertionsContent.append(" */\n\n");
            assertionsContent.append("#include <cstddef>\n\n");
            assertionsContent.append("// Included by Basic.h. Each block below defines the macro that\n");
            assertionsContent.append("// AVALONIUM_ASSERT_LAYOUT(<Type>) expands next to that type's definition,\n");
            assertionsContent.append("// so the checks only run when CHECK_SDK_LAYOUT is defined.\n\n");
            for (const auto& info : structAssertions) {
                assertionsContent.append(GenerateStructAssertions(info));
            }
            writeTasks.push_back({"Assertions.inl", std::move(assertionsContent), false, true});
            totalFiles++;
        }

        {
            std::string sdkH;
            sdkH.reserve(64 * 1024);
            sdkH.append("#pragma once\n\n");
            sdkH.append("/*\n");
            sdkH.append(" * SDK generated by Avalonium\n");
            sdkH.append(" * Created by solo reverse engineer\n");
            sdkH.append(" * Dont use this SDK for any illegal purpose, dont disappoint me human\n");
            sdkH.append(" * License: GPLv3\n");
            sdkH.append(" */\n\n");
            sdkH.append("#include \"Basic.h\"\n\n");
            sdkH.append(sdkIncludeList);
            writeTasks.push_back({"SDK.h", std::move(sdkH), false, false});
        }

        std::vector<std::future<bool>> futures;
        futures.reserve(writeTasks.size());
        for (auto& task : writeTasks) {

            const std::string* dir = &outputDir;
            const std::string* fn  = &task.FileName;
            const std::string* ct  = &task.Content;
            futures.push_back(std::async(std::launch::async, [dir, fn, ct]() -> bool {
                return WriteFile(*dir, *fn, *ct);
            }));
        }

        for (size_t i = 0; i < futures.size(); ++i) {
            if (!futures[i].get()) {
                SdkLog("[SDK] ERROR: Failed to write file: %s", writeTasks[i].FileName.c_str());
                failedFiles++;
            }
        }
        if (WriteMetadataManifest(outputDir, state, count, totalFiles + 1, failedFiles, collisionCount, metadataEntries, packageSummaries, packageDependencies)) {
            totalFiles++;
        } else {
            SdkLog("[SDK] ERROR: Failed to write SDKMetadata.json!");
            failedFiles++;
        }

        const std::string reflectionOutputDir = ResolveReflectionOutputDir(outputDir);
        if (WriteReflectionDatabase(reflectionOutputDir, state, count, collisionCount, packageSummaries, packageDependencies,
                                    reflectionEnums, reflectionTypes, reflectionFunctions, reflectionProperties)) {
            SdkLog("[SDK] ReflectionDB written to: %s", reflectionOutputDir.c_str());
        } else {
            SdkLog("[SDK] WARNING: Failed to write ReflectionDB.json to: %s", reflectionOutputDir.c_str());
        }
        if (WriteReflectionRuntimeHelpers(reflectionOutputDir, reflectionTypes, reflectionFunctions, reflectionProperties)) {
            SdkLog("[SDK] Reflection runtime helpers written to: %s\\Runtime", reflectionOutputDir.c_str());
        } else {
            SdkLog("[SDK] WARNING: Failed to write reflection runtime helpers to: %s\\Runtime", reflectionOutputDir.c_str());
        }
        if (WriteReflectionGraphs(reflectionOutputDir, packageSummaries, packageDependencies, reflectionTypes)) {
            SdkLog("[SDK] Reflection graph files written to: %s", reflectionOutputDir.c_str());
        } else {
            SdkLog("[SDK] WARNING: Failed to write reflection graph files to: %s", reflectionOutputDir.c_str());
        }
        if (WriteReflectionDiff(reflectionOutputDir, state, reflectionEnums, reflectionTypes, reflectionFunctions, reflectionProperties)) {
            SdkLog("[SDK] Reflection diff files written to: %s\\Diff", reflectionOutputDir.c_str());
        } else {
            SdkLog("[SDK] WARNING: Failed to write reflection diff files to: %s\\Diff", reflectionOutputDir.c_str());
        }

        SdkLog("[SDK] ====== Full C++ SDK generated successfully! ======");
        SdkLog("[SDK] Packages: %d", (int)packages.size());
        SdkLog("[SDK] Enums: %d, Structs: %d, Classes: %d", enumCount, structCount, classCount);
        SdkLog("[SDK] Functions: %d, Parameter Structs: %d", funcCount, paramStructCount);
        if (shiftedLayoutCount > 0) {
            SdkLog("[SDK] Layout: %d type(s) do not match the engine byte-for-byte; their layout "
                   "assertions will fail, so fix the emission rather than the assertion", shiftedLayoutCount);
        }
        SdkLog("[SDK] Metadata Symbols: %d", static_cast<int>(metadataEntries.size()));
        SdkLog("[SDK] Reflection Records: enums=%d, types=%d, functions=%d, properties=%d",
               static_cast<int>(reflectionEnums.size()),
               static_cast<int>(reflectionTypes.size()),
               static_cast<int>(reflectionFunctions.size()),
               static_cast<int>(reflectionProperties.size()));
        SdkLog("[SDK] Total Files: %d (failed: %d)", totalFiles, failedFiles);

        const RuntimeFeatureConfig& runtimeConfig = RuntimeConfig::Load(nullptr);

        if (runtimeConfig.GenerateBuildSystem && !runtimeConfig.BuildScriptPath.empty()) {
            SdkLog("[SDK] Generating build system using python script...");

            std::string cmd = "python \"" + runtimeConfig.BuildScriptPath + "\" \"" + outputDir + "\" --game-name \"" + state.GameName + "\"";
            const std::wstring wideCmd = ToWidePath(cmd);
            std::vector<wchar_t> mutableCmd(wideCmd.begin(), wideCmd.end());
            mutableCmd.push_back(L'\0');

            STARTUPINFOW si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));

            if (!wideCmd.empty() && CreateProcessW(NULL, mutableCmd.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {

                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                SdkLog("[SDK] Build system generation launched for game: %s", state.GameName);
            } else {
                SdkLog("[SDK] WARNING: Failed to launch python script for build generation. Error: %lu", GetLastError());
            }
        }

        if (runtimeConfig.EnableSDKLibclangGeneration) {
            SdkLog("[SDK] Launching libclang AST generation and verification...");
            SDKValidation::StartSDKGenerationThread(
                outputDir,
                state.GameName,
                runtimeConfig.LibclangPath);
        }

        if (runtimeConfig.EnableSDKValidation) {
            SdkLog("[SDK] Starting post-dump SDK validation against live memory...");
            SDKValidation::StartSDKValidationThread(state, outputDir);
        }

        return true;
    }

    bool GenerateReflectionKit(const EngineState& state, const std::string& outputRootDir) {
        const std::wstring wideOutputRootDir = ToWidePath(outputRootDir);
        if (wideOutputRootDir.empty()) {
            SdkLog("[SDK] ERROR: Failed to convert reflection kit root to UTF-16: %s", outputRootDir.c_str());
            return false;
        }

        std::error_code ec;
        fs::create_directories(fs::path(wideOutputRootDir), ec);

        return GenerateSDK(state, outputRootDir + "\\SDK");
    }

    bool GenerateReflectionDiff(const EngineState& state, const std::string& outputDir,
                                const std::string& previousSnapshotPath, const std::string& currentSnapshotPath) {
        if (outputDir.empty() || previousSnapshotPath.empty() || currentSnapshotPath.empty()) {
            SdkLog("[SDK] ERROR: GenerateReflectionDiff received empty path(s)");
            return false;
        }

        std::unordered_map<std::string, ReflectionSnapshotEntry> previousById;
        std::unordered_map<std::string, ReflectionSnapshotEntry> currentById;
        if (!LoadSnapshotMap(previousSnapshotPath, previousById)) {
            SdkLog("[SDK] ERROR: Failed to load previous reflection snapshot: %s", previousSnapshotPath.c_str());
            return false;
        }
        if (!LoadSnapshotMap(currentSnapshotPath, currentById)) {
            SdkLog("[SDK] ERROR: Failed to load current reflection snapshot: %s", currentSnapshotPath.c_str());
            return false;
        }

        const std::wstring wideOutputDir = ToWidePath(outputDir);
        if (wideOutputDir.empty()) {
            SdkLog("[SDK] ERROR: Failed to convert diff output dir to UTF-16: %s", outputDir.c_str());
            return false;
        }

        std::error_code ec;
        fs::create_directories(fs::path(wideOutputDir), ec);
        return WriteReflectionDiffFromSnapshots(outputDir, state, previousById, currentById, true);
    }

}
