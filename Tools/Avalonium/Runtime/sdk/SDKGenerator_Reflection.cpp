#include "SDKGenerator_Impl.h"
#include <windows.h>
namespace SDKGenerator {

    std::string ResolveReflectionOutputDir(const std::string& sdkOutputDir) {
        if (sdkOutputDir.empty()) return "Reflection";

        size_t lastSep = sdkOutputDir.find_last_of("/\\");
        std::string baseDir = (lastSep != std::string::npos)
            ? sdkOutputDir.substr(0, lastSep)
            : sdkOutputDir;

        while (!baseDir.empty() && (baseDir.back() == '\\' || baseDir.back() == '/')) {
            baseDir.pop_back();
        }

        if (baseDir.empty()) return "Reflection";
        return baseDir + "\\Reflection";
    }

    bool WriteReflectionDatabase(
        const std::string& outputDir,
        const EngineState& state,
        int32_t objectCount,
        int collisionCount,
        const std::map<std::string, PackageSummary>& packageSummaries,
        const std::map<std::string, std::set<std::string>>& packageDependencies,
        const std::vector<ReflectionEnumRecord>& enums,
        const std::vector<ReflectionTypeRecord>& types,
        const std::vector<ReflectionFunctionRecord>& functions,
        const std::vector<ReflectionPropertyRecord>& properties) {

        std::vector<const ReflectionEnumRecord*> sortedEnums;
        sortedEnums.reserve(enums.size());
        for (const auto& e : enums) sortedEnums.push_back(&e);
        std::sort(sortedEnums.begin(), sortedEnums.end(), [](const ReflectionEnumRecord* lhs, const ReflectionEnumRecord* rhs) {
            if (lhs->PackageName != rhs->PackageName) return lhs->PackageName < rhs->PackageName;
            return lhs->FullPath < rhs->FullPath;
        });

        std::vector<const ReflectionTypeRecord*> sortedTypes;
        sortedTypes.reserve(types.size());
        for (const auto& t : types) sortedTypes.push_back(&t);
        std::sort(sortedTypes.begin(), sortedTypes.end(), [](const ReflectionTypeRecord* lhs, const ReflectionTypeRecord* rhs) {
            if (lhs->Kind != rhs->Kind) return lhs->Kind < rhs->Kind;
            if (lhs->PackageName != rhs->PackageName) return lhs->PackageName < rhs->PackageName;
            return lhs->FullPath < rhs->FullPath;
        });

        std::vector<const ReflectionFunctionRecord*> sortedFunctions;
        sortedFunctions.reserve(functions.size());
        for (const auto& f : functions) sortedFunctions.push_back(&f);
        std::sort(sortedFunctions.begin(), sortedFunctions.end(), [](const ReflectionFunctionRecord* lhs, const ReflectionFunctionRecord* rhs) {
            if (lhs->OwnerCppName != rhs->OwnerCppName) return lhs->OwnerCppName < rhs->OwnerCppName;
            return lhs->FullPath < rhs->FullPath;
        });

        std::vector<const ReflectionPropertyRecord*> sortedProperties;
        sortedProperties.reserve(properties.size());
        for (const auto& p : properties) sortedProperties.push_back(&p);
        std::sort(sortedProperties.begin(), sortedProperties.end(), [](const ReflectionPropertyRecord* lhs, const ReflectionPropertyRecord* rhs) {
            if (lhs->OwnerCppName != rhs->OwnerCppName) return lhs->OwnerCppName < rhs->OwnerCppName;
            if (lhs->Role != rhs->Role) return lhs->Role < rhs->Role;
            if (lhs->Offset != rhs->Offset) return lhs->Offset < rhs->Offset;
            return lhs->Name < rhs->Name;
        });

        std::string json;
        json.reserve(32 * 1024 * 1024);
        json.append("{\n");
        json.append("  \"generator\": \"Avalonium ReflectionDB\",\n");
        json.append("  \"format_version\": 1,\n");
        json.append("  \"game\": \""); AppendEscapedJson(json, state.GameName); json.append("\",\n");
        json.append("  \"engine\": \"UE"); json.append(std::to_string(state.UEMajor)); json.append("."); json.append(std::to_string(state.UEMinor)); json.append("\",\n");
        json.append("  \"output_dir\": \""); AppendEscapedJson(json, outputDir); json.append("\",\n");
        json.append("  \"object_count\": "); json.append(std::to_string(objectCount)); json.append(",\n");
        json.append("  \"package_collision_count\": "); json.append(std::to_string(collisionCount)); json.append(",\n");
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
            json.append("]\n");
            json.append("    }");
        }
        json.append("\n  ],\n");

        json.append("  \"enums\": [\n");
        bool firstEnum = true;
        for (const ReflectionEnumRecord* entry : sortedEnums) {
            if (!firstEnum) json.append(",\n");
            firstEnum = false;
            json.append("    {\n");
            json.append("      \"id\": \""); AppendEscapedJson(json, entry->Id); json.append("\",\n");
            json.append("      \"package\": \""); AppendEscapedJson(json, entry->PackageName); json.append("\",\n");
            json.append("      \"name\": \""); AppendEscapedJson(json, entry->Name); json.append("\",\n");
            json.append("      \"cpp_name\": \""); AppendEscapedJson(json, entry->CppName); json.append("\",\n");
            json.append("      \"full_path\": \""); AppendEscapedJson(json, entry->FullPath); json.append("\",\n");
            json.append("      \"hash\": \""); AppendEscapedJson(json, entry->SignatureHash); json.append("\",\n");
            json.append("      \"values\": [");
            bool firstValue = true;
            for (const auto& value : entry->Values) {
                if (!firstValue) json.append(", ");
                firstValue = false;
                json.append("{ \"name\": \""); AppendEscapedJson(json, value.first);
                json.append("\", \"value\": "); json.append(std::to_string(value.second)); json.append(" }");
            }
            json.append("]\n");
            json.append("    }");
        }
        json.append("\n  ],\n");

        json.append("  \"types\": [\n");
        bool firstType = true;
        for (const ReflectionTypeRecord* entry : sortedTypes) {
            if (!firstType) json.append(",\n");
            firstType = false;
            json.append("    {\n");
            json.append("      \"id\": \""); AppendEscapedJson(json, entry->Id); json.append("\",\n");
            json.append("      \"kind\": \""); AppendEscapedJson(json, entry->Kind); json.append("\",\n");
            json.append("      \"package\": \""); AppendEscapedJson(json, entry->PackageName); json.append("\",\n");
            json.append("      \"name\": \""); AppendEscapedJson(json, entry->Name); json.append("\",\n");
            json.append("      \"cpp_name\": \""); AppendEscapedJson(json, entry->CppName); json.append("\",\n");
            json.append("      \"full_path\": \""); AppendEscapedJson(json, entry->FullPath); json.append("\",\n");
            json.append("      \"super\": \""); AppendEscapedJson(json, entry->SuperName); json.append("\",\n");
            json.append("      \"super_cpp_name\": \""); AppendEscapedJson(json, entry->SuperCppName); json.append("\",\n");
            json.append("      \"hash\": \""); AppendEscapedJson(json, entry->SignatureHash); json.append("\",\n");
            json.append("      \"size\": "); json.append(std::to_string(entry->Size)); json.append(",\n");
            json.append("      \"super_size\": "); json.append(std::to_string(entry->SuperSize)); json.append(",\n");
            json.append("      \"own_size\": "); json.append(std::to_string(entry->OwnSize)); json.append(",\n");
            json.append("      \"property_count\": "); json.append(std::to_string(entry->PropertyCount)); json.append(",\n");
            json.append("      \"function_count\": "); json.append(std::to_string(entry->FunctionCount)); json.append("\n");
            json.append("    }");
        }
        json.append("\n  ],\n");

        json.append("  \"functions\": [\n");
        bool firstFunction = true;
        for (const ReflectionFunctionRecord* entry : sortedFunctions) {
            if (!firstFunction) json.append(",\n");
            firstFunction = false;
            json.append("    {\n");
            json.append("      \"id\": \""); AppendEscapedJson(json, entry->Id); json.append("\",\n");
            json.append("      \"owner_type_id\": \""); AppendEscapedJson(json, entry->OwnerTypeId); json.append("\",\n");
            json.append("      \"owner_kind\": \""); AppendEscapedJson(json, entry->OwnerKind); json.append("\",\n");
            json.append("      \"owner_package\": \""); AppendEscapedJson(json, entry->OwnerPackageName); json.append("\",\n");
            json.append("      \"owner_name\": \""); AppendEscapedJson(json, entry->OwnerName); json.append("\",\n");
            json.append("      \"owner_cpp_name\": \""); AppendEscapedJson(json, entry->OwnerCppName); json.append("\",\n");
            json.append("      \"name\": \""); AppendEscapedJson(json, entry->Name); json.append("\",\n");
            json.append("      \"full_path\": \""); AppendEscapedJson(json, entry->FullPath); json.append("\",\n");
            json.append("      \"return_type\": \""); AppendEscapedJson(json, entry->ReturnType); json.append("\",\n");
            json.append("      \"return_name\": \""); AppendEscapedJson(json, entry->ReturnName); json.append("\",\n");
            json.append("      \"function_flags\": "); json.append(std::to_string(entry->FunctionFlags)); json.append(",\n");
            json.append("      \"parameter_size\": "); json.append(std::to_string(entry->ParameterSize)); json.append(",\n");
            json.append("      \"hash\": \""); AppendEscapedJson(json, entry->SignatureHash); json.append("\",\n");
            json.append("      \"param_count\": "); json.append(std::to_string(static_cast<int32_t>(entry->Params.size()))); json.append("\n");
            json.append("    }");
        }
        json.append("\n  ],\n");

        json.append("  \"properties\": [\n");
        bool firstProperty = true;
        for (const ReflectionPropertyRecord* entry : sortedProperties) {
            if (!firstProperty) json.append(",\n");
            firstProperty = false;
            json.append("    {\n");
            json.append("      \"id\": \""); AppendEscapedJson(json, entry->Id); json.append("\",\n");
            json.append("      \"owner_id\": \""); AppendEscapedJson(json, entry->OwnerId); json.append("\",\n");
            json.append("      \"owner_kind\": \""); AppendEscapedJson(json, entry->OwnerKind); json.append("\",\n");
            json.append("      \"owner_package\": \""); AppendEscapedJson(json, entry->OwnerPackageName); json.append("\",\n");
            json.append("      \"owner_name\": \""); AppendEscapedJson(json, entry->OwnerName); json.append("\",\n");
            json.append("      \"owner_cpp_name\": \""); AppendEscapedJson(json, entry->OwnerCppName); json.append("\",\n");
            json.append("      \"role\": \""); AppendEscapedJson(json, entry->Role); json.append("\",\n");
            json.append("      \"name\": \""); AppendEscapedJson(json, entry->Name); json.append("\",\n");
            json.append("      \"cpp_type\": \""); AppendEscapedJson(json, entry->CppType); json.append("\",\n");
            json.append("      \"raw_type\": \""); AppendEscapedJson(json, entry->RawType); json.append("\"");

            if (!entry->InnerType.empty()) {
                json.append(",\n      \"inner_type\": \""); AppendEscapedJson(json, entry->InnerType); json.append("\"");
            }
            if (!entry->KeyType.empty()) {
                json.append(",\n      \"key_type\": \""); AppendEscapedJson(json, entry->KeyType); json.append("\"");
            }
            if (!entry->ValueType.empty()) {
                json.append(",\n      \"value_type\": \""); AppendEscapedJson(json, entry->ValueType); json.append("\"");
            }
            if (!entry->StructType.empty()) {
                json.append(",\n      \"struct_type\": \""); AppendEscapedJson(json, entry->StructType); json.append("\"");
            }
            if (!entry->EnumType.empty()) {
                json.append(",\n      \"enum_type\": \""); AppendEscapedJson(json, entry->EnumType); json.append("\"");
            }
            if (!entry->ObjectClassType.empty()) {
                json.append(",\n      \"object_class_type\": \""); AppendEscapedJson(json, entry->ObjectClassType); json.append("\"");
            }
            if (!entry->FlagsText.empty() && entry->FlagsText != "None") {
                json.append(",\n      \"flags_text\": \""); AppendEscapedJson(json, entry->FlagsText); json.append("\"");
            }

            json.append(",\n      \"hash\": \""); AppendEscapedJson(json, entry->SignatureHash); json.append("\"");
            json.append(",\n      \"offset\": "); json.append(std::to_string(entry->Offset));
            json.append(",\n      \"size\": "); json.append(std::to_string(entry->Size));
            json.append(",\n      \"element_size\": "); json.append(std::to_string(entry->ElementSize));
            if (entry->ArrayDim > 1) {
                json.append(",\n      \"array_dim\": "); json.append(std::to_string(entry->ArrayDim));
            }
            if (entry->Flags != 0) {
                json.append(",\n      \"flags\": "); json.append(std::to_string(entry->Flags));
            }

            if (entry->IsArray) json.append(",\n      \"is_array\": true");
            if (entry->IsMap) json.append(",\n      \"is_map\": true");
            if (entry->IsSet) json.append(",\n      \"is_set\": true");
            if (entry->IsStruct) json.append(",\n      \"is_struct\": true");
            if (entry->IsEnum) json.append(",\n      \"is_enum\": true");
            if (entry->IsObject) json.append(",\n      \"is_object\": true");
            if (entry->IsBool) json.append(",\n      \"is_bool\": true");
            if (entry->IsString) json.append(",\n      \"is_string\": true");
            if (entry->IsNumeric) json.append(",\n      \"is_numeric\": true");
            if (entry->IsInt) json.append(",\n      \"is_int\": true");
            if (entry->IsFloat) json.append(",\n      \"is_float\": true");
            if (entry->IsDelegate) json.append(",\n      \"is_delegate\": true");
            if (entry->IsInterface) json.append(",\n      \"is_interface\": true");

            if (entry->IsBitfield) {
                json.append(",\n      \"is_bitfield\": true");
                if (entry->BitIndex != 0xFF) {
                    json.append(",\n      \"bit_index\": "); json.append(std::to_string(entry->BitIndex));
                }
                if (entry->FieldMask != 0) {
                    json.append(",\n      \"field_mask\": "); json.append(std::to_string(entry->FieldMask));
                }
            }

            if (entry->Role == "param") {
                if (entry->IsReturnParam) json.append(",\n      \"is_return_param\": true");
                if (entry->IsOutParam) json.append(",\n      \"is_out_param\": true");
                if (entry->IsReferenceParam) json.append(",\n      \"is_reference_param\": true");
                if (entry->IsConstParam) json.append(",\n      \"is_const_param\": true");
            }

            json.append("\n    }");
        }
        json.append("\n  ],\n");

        json.append("  \"indexes\": {\n");
        json.append("    \"package_count\": "); json.append(std::to_string(static_cast<int32_t>(packageSummaries.size()))); json.append(",\n");
        json.append("    \"enum_count\": "); json.append(std::to_string(static_cast<int32_t>(sortedEnums.size()))); json.append(",\n");
        json.append("    \"type_count\": "); json.append(std::to_string(static_cast<int32_t>(sortedTypes.size()))); json.append(",\n");
        json.append("    \"function_count\": "); json.append(std::to_string(static_cast<int32_t>(sortedFunctions.size()))); json.append(",\n");
        json.append("    \"property_count\": "); json.append(std::to_string(static_cast<int32_t>(sortedProperties.size()))); json.append("\n");
        json.append("  }\n");
        json.append("}\n");

        return WriteFile(outputDir, "ReflectionDB.json", json);
    }

    std::string EscapeCppString(const std::string& value) {
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
        for (unsigned char ch : value) {
            switch (ch) {
            case '\\': out.append("\\\\"); break;
            case '"':  out.append("\\\""); break;
            case '\n': out.append("\\n"); break;
            case '\r': out.append("\\r"); break;
            case '\t': out.append("\\t"); break;
            default:
                if (ch < 0x20) {
                    char hexBuf[8];
                    snprintf(hexBuf, sizeof(hexBuf), "\\x%02X", static_cast<unsigned int>(ch));
                    out.append(hexBuf);
                } else {
                    out.push_back(static_cast<char>(ch));
                }
                break;
            }
        }
        return out;
    }

    bool WriteReflectionRuntimeHelpers(
        const std::string& outputDir,
        const std::vector<ReflectionTypeRecord>& types,
        const std::vector<ReflectionFunctionRecord>& functions,
        const std::vector<ReflectionPropertyRecord>& properties) {

        std::vector<const ReflectionTypeRecord*> sortedTypes;
        sortedTypes.reserve(types.size());
        for (const auto& t : types) sortedTypes.push_back(&t);
        std::sort(sortedTypes.begin(), sortedTypes.end(), [](const ReflectionTypeRecord* lhs, const ReflectionTypeRecord* rhs) {
            if (lhs->CppName != rhs->CppName) return lhs->CppName < rhs->CppName;
            return lhs->FullPath < rhs->FullPath;
        });

        std::vector<const ReflectionFunctionRecord*> sortedFunctions;
        sortedFunctions.reserve(functions.size());
        for (const auto& f : functions) sortedFunctions.push_back(&f);
        std::sort(sortedFunctions.begin(), sortedFunctions.end(), [](const ReflectionFunctionRecord* lhs, const ReflectionFunctionRecord* rhs) {
            if (lhs->OwnerCppName != rhs->OwnerCppName) return lhs->OwnerCppName < rhs->OwnerCppName;
            if (lhs->Name != rhs->Name) return lhs->Name < rhs->Name;
            return lhs->FullPath < rhs->FullPath;
        });

        std::vector<const ReflectionPropertyRecord*> sortedProperties;
        sortedProperties.reserve(properties.size());
        for (const auto& p : properties) sortedProperties.push_back(&p);
        std::sort(sortedProperties.begin(), sortedProperties.end(), [](const ReflectionPropertyRecord* lhs, const ReflectionPropertyRecord* rhs) {
            if (lhs->OwnerCppName != rhs->OwnerCppName) return lhs->OwnerCppName < rhs->OwnerCppName;
            if (lhs->Role != rhs->Role) return lhs->Role < rhs->Role;
            if (lhs->Name != rhs->Name) return lhs->Name < rhs->Name;
            return lhs->Offset < rhs->Offset;
        });

        struct RuntimeParameterRef {
            const std::string* OwnerCppName;
            const std::string* FunctionName;
            const SDKProperty* Param;
        };
        std::vector<RuntimeParameterRef> sortedParameters;
        for (const auto& entry : functions) {
            for (const auto& param : entry.Params) {
                sortedParameters.push_back({ &entry.OwnerCppName, &entry.Name, &param });
            }
        }
        std::sort(sortedParameters.begin(), sortedParameters.end(), [](const RuntimeParameterRef& lhs, const RuntimeParameterRef& rhs) {
            if (*lhs.OwnerCppName != *rhs.OwnerCppName) return *lhs.OwnerCppName < *rhs.OwnerCppName;
            if (*lhs.FunctionName != *rhs.FunctionName) return *lhs.FunctionName < *rhs.FunctionName;
            if (lhs.Param->Offset != rhs.Param->Offset) return lhs.Param->Offset < rhs.Param->Offset;
            return lhs.Param->Name < rhs.Param->Name;
        });

        std::ostringstream header;
        header << R"(#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace ReflectionRuntime {

struct TypeRecord {
    const char* Kind;
    const char* CppName;
    const char* FullPath;
    const char* SuperCppName;
    const char* SignatureHash;
    int32_t Size;
    int32_t SuperSize;
    int32_t OwnSize;
    int32_t PropertyCount;
    int32_t FunctionCount;
};

struct FunctionRecord {
    const char* OwnerCppName;
    const char* Name;
    const char* FullPath;
    const char* ReturnType;
    const char* ReturnName;
    const char* SignatureHash;
    uint32_t FunctionFlags;
    int32_t ParameterSize;
    int32_t ParamCount;
};

struct PropertyRecord {
    const char* OwnerCppName;
    const char* Name;
    const char* Role;
    const char* CppType;
    const char* RawType;
    const char* InnerType;
    const char* KeyType;
    const char* ValueType;
    const char* StructType;
    const char* EnumType;
    const char* ObjectClassType;
    const char* FlagsText;
    const char* SignatureHash;
    int32_t Offset;
    int32_t Size;
    int32_t ElementSize;
    int32_t ArrayDim;
    uint64_t Flags;
};

struct ParameterRecord {
    const char* OwnerCppName;
    const char* FunctionName;
    const char* Name;
    const char* CppType;
    const char* RawType;
    const char* InnerType;
    const char* KeyType;
    const char* ValueType;
    const char* StructType;
    const char* EnumType;
    const char* ObjectClassType;
    const char* FlagsText;
    const char* SignatureHash;
    int32_t Offset;
    int32_t Size;
    int32_t ElementSize;
    int32_t ArrayDim;
    uint64_t Flags;
    bool IsReturnParam;
    bool IsOutParam;
    bool IsReferenceParam;
    bool IsConstParam;
};

enum class RuntimeStatus : uint32_t {
    Success = 0,
    InvalidArgument,
    NotFound,
    NullObject,
    NullFunction,
    NullResolver,
    NullInvoker,
    InvalidOffset,
    InvalidSize,
    TypeMismatch
};

struct InvocationContext {
    void* (*ResolveFunction)(void* object, const FunctionRecord* function);
    void (*ProcessEvent)(void* object, void* function, void* params);
};

size_t GetTypeCount();
size_t GetFunctionCount();
size_t GetPropertyCount();
size_t GetParameterCount();
const TypeRecord* GetTypes();
const FunctionRecord* GetFunctions();
const PropertyRecord* GetProperties();
const ParameterRecord* GetParameters();
const TypeRecord* FindTypeByName(const char* cppName);
const TypeRecord* FindTypeByPath(const char* fullPath);
const TypeRecord* FindSuperType(const char* cppName);
const FunctionRecord* FindFunctionByName(const char* ownerCppName, const char* functionName);
const FunctionRecord* FindFunctionByPath(const char* fullPath);
const PropertyRecord* FindPropertyByName(const char* ownerCppName, const char* propertyName, const char* role = nullptr);
const PropertyRecord* FindPropertyByOffset(const char* ownerCppName, int32_t offset, const char* role = "member");
const ParameterRecord* FindParameterByName(const char* ownerCppName, const char* functionName, const char* parameterName);
size_t CountFunctionsForOwner(const char* ownerCppName);
size_t CountPropertiesForOwner(const char* ownerCppName, const char* role = nullptr);
size_t CountParametersForFunction(const char* ownerCppName, const char* functionName, bool includeReturnValue = true);
int32_t GetPropertyOffset(const char* ownerCppName, const char* propertyName, const char* role = "member");

void SetLookupCacheEnabled(bool enabled);
bool IsLookupCacheEnabled();
void ClearLookupCache();

bool ValidateFunctionRecord(const FunctionRecord* function);
bool ValidatePropertyRecord(const PropertyRecord* property);
bool ValidateParameterRecord(const ParameterRecord* parameter, const FunctionRecord* function = nullptr);
RuntimeStatus ValidateInvocation(const FunctionRecord* function, size_t paramsSize);
RuntimeStatus InvokeFunctionChecked(const InvocationContext& context, void* object, const FunctionRecord* function, void* params, size_t paramsSize);
RuntimeStatus InvokeFunctionChecked(const InvocationContext& context, void* object, const char* ownerCppName, const char* functionName, void* params, size_t paramsSize);
const char* GetRuntimeStatusText(RuntimeStatus status);

namespace Detail {
inline bool IsPointerLikeProperty(const PropertyRecord* property) {
    if (!property || !property->RawType) return false;
    return std::strcmp(property->RawType, "ObjectProperty") == 0 ||
           std::strcmp(property->RawType, "ObjectPtrProperty") == 0 ||
           std::strcmp(property->RawType, "ClassProperty") == 0 ||
           std::strcmp(property->RawType, "WeakObjectProperty") == 0 ||
           std::strcmp(property->RawType, "LazyObjectProperty") == 0 ||
           std::strcmp(property->RawType, "SoftObjectProperty") == 0 ||
           std::strcmp(property->RawType, "SoftClassProperty") == 0 ||
           std::strcmp(property->RawType, "InterfaceProperty") == 0;
}

template <typename T>
struct PropertyTypeTraits {
    bool Matches(const PropertyRecord* property) {
        return property &&
               property->ArrayDim == 1 &&
               property->Size == static_cast<int32_t>(sizeof(T));
    }
};

template <>
struct PropertyTypeTraits<bool> {
    bool Matches(const PropertyRecord* property) {
        return property &&
               property->ArrayDim == 1 &&
               property->Size >= 1 &&
               (std::strcmp(property->CppType, "bool") == 0 ||
                std::strcmp(property->RawType, "BoolProperty") == 0);
    }
};

template <>
struct PropertyTypeTraits<int8_t> {
    bool Matches(const PropertyRecord* property) {
        return property &&
               property->ArrayDim == 1 &&
               property->Size == static_cast<int32_t>(sizeof(int8_t)) &&
               (std::strcmp(property->CppType, "int8") == 0 ||
                std::strcmp(property->RawType, "Int8Property") == 0 ||
                std::strcmp(property->RawType, "ByteProperty") == 0);
    }
};

template <>
struct PropertyTypeTraits<uint8_t> {
    bool Matches(const PropertyRecord* property) {
        return property &&
               property->ArrayDim == 1 &&
               property->Size == static_cast<int32_t>(sizeof(uint8_t)) &&
               (std::strcmp(property->CppType, "uint8") == 0 ||
                std::strcmp(property->RawType, "ByteProperty") == 0);
    }
};

template <>
struct PropertyTypeTraits<int16_t> {
    bool Matches(const PropertyRecord* property) {
        return property &&
               property->ArrayDim == 1 &&
               property->Size == static_cast<int32_t>(sizeof(int16_t)) &&
               std::strcmp(property->CppType, "int16") == 0;
    }
};

template <>
struct PropertyTypeTraits<uint16_t> {
    bool Matches(const PropertyRecord* property) {
        return property &&
               property->ArrayDim == 1 &&
               property->Size == static_cast<int32_t>(sizeof(uint16_t)) &&
               std::strcmp(property->CppType, "uint16") == 0;
    }
};

template <>
struct PropertyTypeTraits<int32_t> {
    bool Matches(const PropertyRecord* property) {
        return property &&
               property->ArrayDim == 1 &&
               property->Size == static_cast<int32_t>(sizeof(int32_t)) &&
               (std::strcmp(property->CppType, "int32") == 0 ||
                std::strcmp(property->RawType, "IntProperty") == 0);
    }
};

template <>
struct PropertyTypeTraits<uint32_t> {
    bool Matches(const PropertyRecord* property) {
        return property &&
               property->ArrayDim == 1 &&
               property->Size == static_cast<int32_t>(sizeof(uint32_t)) &&
               (std::strcmp(property->CppType, "uint32") == 0 ||
                std::strcmp(property->RawType, "UInt32Property") == 0);
    }
};

template <>
struct PropertyTypeTraits<int64_t> {
    bool Matches(const PropertyRecord* property) {
        return property &&
               property->ArrayDim == 1 &&
               property->Size == static_cast<int32_t>(sizeof(int64_t)) &&
               (std::strcmp(property->CppType, "int64") == 0 ||
                std::strcmp(property->RawType, "Int64Property") == 0);
    }
};

template <>
struct PropertyTypeTraits<uint64_t> {
    bool Matches(const PropertyRecord* property) {
        return property &&
               property->ArrayDim == 1 &&
               property->Size == static_cast<int32_t>(sizeof(uint64_t)) &&
               (std::strcmp(property->CppType, "uint64") == 0 ||
                std::strcmp(property->RawType, "UInt64Property") == 0);
    }
};

template <>
struct PropertyTypeTraits<float> {
    bool Matches(const PropertyRecord* property) {
        return property &&
               property->ArrayDim == 1 &&
               property->Size == static_cast<int32_t>(sizeof(float)) &&
               (std::strcmp(property->CppType, "float") == 0 ||
                std::strcmp(property->RawType, "FloatProperty") == 0);
    }
};

template <>
struct PropertyTypeTraits<double> {
    bool Matches(const PropertyRecord* property) {
        return property &&
               property->ArrayDim == 1 &&
               property->Size == static_cast<int32_t>(sizeof(double)) &&
               (std::strcmp(property->CppType, "double") == 0 ||
                std::strcmp(property->RawType, "DoubleProperty") == 0);
    }
};

template <typename T>
struct PropertyTypeTraits<T*> {
    bool Matches(const PropertyRecord* property) {
        return property &&
               property->ArrayDim == 1 &&
               property->Size == static_cast<int32_t>(sizeof(void*)) &&
               (IsPointerLikeProperty(property) ||
                (property->CppType && std::strstr(property->CppType, "*") != nullptr));
    }
};
}

template <typename T>
inline bool IsPropertyTypeCompatible(const PropertyRecord* property) {
    typedef typename std::remove_cv<typename std::remove_reference<T>::type>::type CleanType;
    return Detail::PropertyTypeTraits<CleanType>::Matches(property);
}

template <typename T>
inline bool TryGetProperty(const void* object, const PropertyRecord* property, T& outValue) {
    if (!object || !ValidatePropertyRecord(property) || !IsPropertyTypeCompatible<T>(property)) {
        return false;
    }

    std::memcpy(&outValue, static_cast<const uint8_t*>(object) + property->Offset, sizeof(T));
    return true;
}

template <typename T>
inline bool TrySetProperty(void* object, const PropertyRecord* property, const T& value) {
    if (!object || !ValidatePropertyRecord(property) || !IsPropertyTypeCompatible<T>(property)) {
        return false;
    }

    std::memcpy(static_cast<uint8_t*>(object) + property->Offset, &value, sizeof(T));
    return true;
}

template <typename T>
inline bool TryGetPropertyByName(const void* object, const char* ownerCppName, const char* propertyName, T& outValue, const char* role = "member") {
    return TryGetProperty(object, FindPropertyByName(ownerCppName, propertyName, role), outValue);
}

template <typename T>
inline bool TrySetPropertyByName(void* object, const char* ownerCppName, const char* propertyName, const T& value, const char* role = "member") {
    return TrySetProperty(object, FindPropertyByName(ownerCppName, propertyName, role), value);
}

template <typename ParamsT>
inline RuntimeStatus InvokeFunctionChecked(const InvocationContext& context, void* object, const FunctionRecord* function, ParamsT& params) {
    return InvokeFunctionChecked(context, object, function, &params, sizeof(ParamsT));
}

template <typename ParamsT>
inline RuntimeStatus InvokeFunctionChecked(const InvocationContext& context, void* object, const char* ownerCppName, const char* functionName, ParamsT& params) {
    return InvokeFunctionChecked(context, object, ownerCppName, functionName, &params, sizeof(ParamsT));
}

inline bool TryGetBoolProperty(const void* object, const PropertyRecord* property, bool& outValue) { return TryGetProperty(object, property, outValue); }
inline bool TrySetBoolProperty(void* object, const PropertyRecord* property, bool value) { return TrySetProperty(object, property, value); }
inline bool TryGetInt32Property(const void* object, const PropertyRecord* property, int32_t& outValue) { return TryGetProperty(object, property, outValue); }
inline bool TrySetInt32Property(void* object, const PropertyRecord* property, int32_t value) { return TrySetProperty(object, property, value); }
inline bool TryGetInt64Property(const void* object, const PropertyRecord* property, int64_t& outValue) { return TryGetProperty(object, property, outValue); }
inline bool TrySetInt64Property(void* object, const PropertyRecord* property, int64_t value) { return TrySetProperty(object, property, value); }
inline bool TryGetFloatProperty(const void* object, const PropertyRecord* property, float& outValue) { return TryGetProperty(object, property, outValue); }
inline bool TrySetFloatProperty(void* object, const PropertyRecord* property, float value) { return TrySetProperty(object, property, value); }
inline bool TryGetDoubleProperty(const void* object, const PropertyRecord* property, double& outValue) { return TryGetProperty(object, property, outValue); }
inline bool TrySetDoubleProperty(void* object, const PropertyRecord* property, double value) { return TrySetProperty(object, property, value); }
inline bool TryGetObjectProperty(const void* object, const PropertyRecord* property, void*& outValue) { return TryGetProperty(object, property, outValue); }
inline bool TrySetObjectProperty(void* object, const PropertyRecord* property, void* value) { return TrySetProperty(object, property, value); }

}
)";

        std::ostringstream source;
        source << R"(#include "ReflectionHelpers.h"

#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ReflectionRuntime {

)";
        source << "static const TypeRecord kTypes[] = {\n";
        for (const ReflectionTypeRecord* entry : sortedTypes) {
            source << "    { \"" << EscapeCppString(entry->Kind) << "\", "
                   << "\"" << EscapeCppString(entry->CppName) << "\", "
                   << "\"" << EscapeCppString(entry->FullPath) << "\", "
                   << "\"" << EscapeCppString(entry->SuperCppName) << "\", "
                   << "\"" << EscapeCppString(entry->SignatureHash) << "\", "
                   << entry->Size << ", " << entry->SuperSize << ", " << entry->OwnSize << ", "
                   << entry->PropertyCount << ", " << entry->FunctionCount << " },\n";
        }
        source << "};\n\n";

        source << "static const FunctionRecord kFunctions[] = {\n";
        for (const ReflectionFunctionRecord* entry : sortedFunctions) {
            source << "    { \"" << EscapeCppString(entry->OwnerCppName) << "\", "
                   << "\"" << EscapeCppString(entry->Name) << "\", "
                   << "\"" << EscapeCppString(entry->FullPath) << "\", "
                   << "\"" << EscapeCppString(entry->ReturnType) << "\", "
                   << "\"" << EscapeCppString(entry->ReturnName) << "\", "
                   << "\"" << EscapeCppString(entry->SignatureHash) << "\", "
                   << entry->FunctionFlags << ", " << entry->ParameterSize << ", "
                   << static_cast<int32_t>(entry->Params.size()) << " },\n";
        }
        source << "};\n\n";

        source << "static const PropertyRecord kProperties[] = {\n";
        for (const ReflectionPropertyRecord* entry : sortedProperties) {
            source << "    { \"" << EscapeCppString(entry->OwnerCppName) << "\", "
                   << "\"" << EscapeCppString(entry->Name) << "\", "
                   << "\"" << EscapeCppString(entry->Role) << "\", "
                   << "\"" << EscapeCppString(entry->CppType) << "\", "
                   << "\"" << EscapeCppString(entry->RawType) << "\", "
                   << "\"" << EscapeCppString(entry->InnerType) << "\", "
                   << "\"" << EscapeCppString(entry->KeyType) << "\", "
                   << "\"" << EscapeCppString(entry->ValueType) << "\", "
                   << "\"" << EscapeCppString(entry->StructType) << "\", "
                   << "\"" << EscapeCppString(entry->EnumType) << "\", "
                   << "\"" << EscapeCppString(entry->ObjectClassType) << "\", "
                   << "\"" << EscapeCppString(entry->FlagsText) << "\", "
                   << "\"" << EscapeCppString(entry->SignatureHash) << "\", "
                   << entry->Offset << ", " << entry->Size << ", " << entry->ElementSize << ", "
                   << entry->ArrayDim << ", " << entry->Flags << "ULL },\n";
        }
        source << "};\n\n";
        source << "static const ParameterRecord kParameters[] = {\n";
        if (sortedParameters.empty()) {
            source << "    { \"\", \"\", \"\", \"\", \"\", \"\", \"\", \"\", \"\", \"\", \"\", \"\", \"\", 0, 0, 0, 0, 0ULL, false, false, false, false },\n";
        } else {
            for (const auto& entry : sortedParameters) {
                source << "    { \"" << EscapeCppString(*entry.OwnerCppName) << "\", "
                       << "\"" << EscapeCppString(*entry.FunctionName) << "\", "
                       << "\"" << EscapeCppString(entry.Param->Name) << "\", "
                       << "\"" << EscapeCppString(entry.Param->Type) << "\", "
                       << "\"" << EscapeCppString(entry.Param->RawType) << "\", "
                       << "\"" << EscapeCppString(entry.Param->InnerType) << "\", "
                       << "\"" << EscapeCppString(entry.Param->KeyType) << "\", "
                       << "\"" << EscapeCppString(entry.Param->ValueType) << "\", "
                       << "\"" << EscapeCppString(entry.Param->StructType) << "\", "
                       << "\"" << EscapeCppString(entry.Param->EnumType) << "\", "
                       << "\"" << EscapeCppString(entry.Param->ObjectClassType) << "\", "
                       << "\"" << EscapeCppString(entry.Param->FlagsText) << "\", "
                       << "\"" << EscapeCppString(entry.Param->SignatureHash) << "\", "
                       << entry.Param->Offset << ", " << entry.Param->Size << ", " << entry.Param->ElementSize << ", "
                       << entry.Param->ArrayDim << ", " << entry.Param->Flags << "ULL, "
                       << (entry.Param->IsReturnParam ? "true" : "false") << ", "
                       << (entry.Param->IsOutParam ? "true" : "false") << ", "
                       << (entry.Param->IsReferenceParam ? "true" : "false") << ", "
                       << (entry.Param->IsConstParam ? "true" : "false") << " },\n";
            }
        }
        source << "};\n";
        source << "static const size_t kParameterCount = " << sortedParameters.size() << ";\n\n";

        source << R"(namespace {

struct LookupCache {
    bool Enabled = false;
    std::mutex Mutex;
    std::unordered_map<std::string, const TypeRecord*> TypeByName;
    std::unordered_map<std::string, const TypeRecord*> TypeByPath;
    std::unordered_map<std::string, const FunctionRecord*> FunctionByName;
    std::unordered_map<std::string, const FunctionRecord*> FunctionByPath;
    std::unordered_map<std::string, const PropertyRecord*> PropertyByName;
    std::unordered_map<std::string, const PropertyRecord*> PropertyByOffset;
    std::unordered_map<std::string, const ParameterRecord*> ParameterByName;
};

LookupCache& GetLookupCache() {
    static LookupCache cache;
    return cache;
}

std::string MakeKey(const char* first, const char* second = nullptr, const char* third = nullptr) {
    std::string key = first ? first : "";
    key.push_back('\x1f');
    if (second) key += second;
    key.push_back('\x1f');
    if (third) key += third;
    return key;
}

std::string MakeOffsetKey(const char* ownerCppName, int32_t offset, const char* role) {
    return MakeKey(ownerCppName, role ? role : "", std::to_string(offset).c_str());
}

template <typename RecordT>
const RecordT* TryGetCached(std::unordered_map<std::string, const RecordT*>& cacheMap, const std::string& key) {
    LookupCache& cache = GetLookupCache();
    if (!cache.Enabled) return nullptr;

    std::lock_guard<std::mutex> lock(cache.Mutex);
    const auto it = cacheMap.find(key);
    return it != cacheMap.end() ? it->second : nullptr;
}

template <typename RecordT>
void StoreCached(std::unordered_map<std::string, const RecordT*>& cacheMap, const std::string& key, const RecordT* record) {
    LookupCache& cache = GetLookupCache();
    if (!cache.Enabled || !record) return;

    std::lock_guard<std::mutex> lock(cache.Mutex);
    cacheMap[key] = record;
}

int64_t ComputePropertyExtent(const PropertyRecord* property) {
    if (!property) return -1;
    const int64_t arrayDim = property->ArrayDim > 0 ? property->ArrayDim : 1;
    return static_cast<int64_t>(property->Offset) + static_cast<int64_t>(property->Size) * arrayDim;
}

}

size_t GetTypeCount() { return sizeof(kTypes) / sizeof(kTypes[0]); }
size_t GetFunctionCount() { return sizeof(kFunctions) / sizeof(kFunctions[0]); }
size_t GetPropertyCount() { return sizeof(kProperties) / sizeof(kProperties[0]); }
size_t GetParameterCount() { return kParameterCount; }
const TypeRecord* GetTypes() { return kTypes; }
const FunctionRecord* GetFunctions() { return kFunctions; }
const PropertyRecord* GetProperties() { return kProperties; }
const ParameterRecord* GetParameters() { return kParameterCount ? kParameters : nullptr; }

void SetLookupCacheEnabled(bool enabled) {
    LookupCache& cache = GetLookupCache();
    std::lock_guard<std::mutex> lock(cache.Mutex);
    cache.Enabled = enabled;
    if (!enabled) {
        cache.TypeByName.clear();
        cache.TypeByPath.clear();
        cache.FunctionByName.clear();
        cache.FunctionByPath.clear();
        cache.PropertyByName.clear();
        cache.PropertyByOffset.clear();
        cache.ParameterByName.clear();
    }
}

bool IsLookupCacheEnabled() {
    return GetLookupCache().Enabled;
}

void ClearLookupCache() {
    LookupCache& cache = GetLookupCache();
    std::lock_guard<std::mutex> lock(cache.Mutex);
    cache.TypeByName.clear();
    cache.TypeByPath.clear();
    cache.FunctionByName.clear();
    cache.FunctionByPath.clear();
    cache.PropertyByName.clear();
    cache.PropertyByOffset.clear();
    cache.ParameterByName.clear();
}

const TypeRecord* FindTypeByName(const char* cppName) {
    if (!cppName) return nullptr;

    const std::string key = MakeKey(cppName);
    if (const TypeRecord* cached = TryGetCached(GetLookupCache().TypeByName, key)) {
        return cached;
    }

    for (size_t i = 0; i < GetTypeCount(); ++i) {
        if (std::strcmp(kTypes[i].CppName, cppName) == 0) {
            StoreCached(GetLookupCache().TypeByName, key, &kTypes[i]);
            return &kTypes[i];
        }
    }

    return nullptr;
}

const TypeRecord* FindTypeByPath(const char* fullPath) {
    if (!fullPath) return nullptr;

    const std::string key = MakeKey(fullPath);
    if (const TypeRecord* cached = TryGetCached(GetLookupCache().TypeByPath, key)) {
        return cached;
    }

    for (size_t i = 0; i < GetTypeCount(); ++i) {
        if (std::strcmp(kTypes[i].FullPath, fullPath) == 0) {
            StoreCached(GetLookupCache().TypeByPath, key, &kTypes[i]);
            return &kTypes[i];
        }
    }

    return nullptr;
}

const TypeRecord* FindSuperType(const char* cppName) {
    const TypeRecord* type = FindTypeByName(cppName);
    if (!type || !type->SuperCppName || type->SuperCppName[0] == '\0') return nullptr;
    return FindTypeByName(type->SuperCppName);
}

const FunctionRecord* FindFunctionByName(const char* ownerCppName, const char* functionName) {
    if (!ownerCppName || !functionName) return nullptr;

    const std::string key = MakeKey(ownerCppName, functionName);
    if (const FunctionRecord* cached = TryGetCached(GetLookupCache().FunctionByName, key)) {
        return cached;
    }

    for (size_t i = 0; i < GetFunctionCount(); ++i) {
        if (std::strcmp(kFunctions[i].OwnerCppName, ownerCppName) == 0 &&
            std::strcmp(kFunctions[i].Name, functionName) == 0) {
            StoreCached(GetLookupCache().FunctionByName, key, &kFunctions[i]);
            return &kFunctions[i];
        }
    }

    return nullptr;
}

const FunctionRecord* FindFunctionByPath(const char* fullPath) {
    if (!fullPath) return nullptr;

    const std::string key = MakeKey(fullPath);
    if (const FunctionRecord* cached = TryGetCached(GetLookupCache().FunctionByPath, key)) {
        return cached;
    }

    for (size_t i = 0; i < GetFunctionCount(); ++i) {
        if (std::strcmp(kFunctions[i].FullPath, fullPath) == 0) {
            StoreCached(GetLookupCache().FunctionByPath, key, &kFunctions[i]);
            return &kFunctions[i];
        }
    }

    return nullptr;
}

const PropertyRecord* FindPropertyByName(const char* ownerCppName, const char* propertyName, const char* role) {
    if (!ownerCppName || !propertyName) return nullptr;

    const std::string key = MakeKey(ownerCppName, propertyName, role ? role : "");
    if (const PropertyRecord* cached = TryGetCached(GetLookupCache().PropertyByName, key)) {
        return cached;
    }

    for (size_t i = 0; i < GetPropertyCount(); ++i) {
        if (std::strcmp(kProperties[i].OwnerCppName, ownerCppName) != 0) continue;
        if (std::strcmp(kProperties[i].Name, propertyName) != 0) continue;
        if (role && std::strcmp(kProperties[i].Role, role) != 0) continue;
        StoreCached(GetLookupCache().PropertyByName, key, &kProperties[i]);
        return &kProperties[i];
    }

    return nullptr;
}

const PropertyRecord* FindPropertyByOffset(const char* ownerCppName, int32_t offset, const char* role) {
    if (!ownerCppName) return nullptr;

    const std::string key = MakeOffsetKey(ownerCppName, offset, role ? role : "member");
    if (const PropertyRecord* cached = TryGetCached(GetLookupCache().PropertyByOffset, key)) {
        return cached;
    }

    for (size_t i = 0; i < GetPropertyCount(); ++i) {
        if (std::strcmp(kProperties[i].OwnerCppName, ownerCppName) != 0) continue;
        if (kProperties[i].Offset != offset) continue;
        if (role && std::strcmp(kProperties[i].Role, role) != 0) continue;
        StoreCached(GetLookupCache().PropertyByOffset, key, &kProperties[i]);
        return &kProperties[i];
    }

    return nullptr;
}

const ParameterRecord* FindParameterByName(const char* ownerCppName, const char* functionName, const char* parameterName) {
    if (!ownerCppName || !functionName || !parameterName) return nullptr;

    const std::string key = MakeKey(ownerCppName, functionName, parameterName);
    if (const ParameterRecord* cached = TryGetCached(GetLookupCache().ParameterByName, key)) {
        return cached;
    }

    for (size_t i = 0; i < GetParameterCount(); ++i) {
        if (std::strcmp(kParameters[i].OwnerCppName, ownerCppName) != 0) continue;
        if (std::strcmp(kParameters[i].FunctionName, functionName) != 0) continue;
        if (std::strcmp(kParameters[i].Name, parameterName) != 0) continue;
        StoreCached(GetLookupCache().ParameterByName, key, &kParameters[i]);
        return &kParameters[i];
    }

    return nullptr;
}

size_t CountFunctionsForOwner(const char* ownerCppName) {
    if (!ownerCppName) return 0;

    size_t count = 0;
    for (size_t i = 0; i < GetFunctionCount(); ++i) {
        if (std::strcmp(kFunctions[i].OwnerCppName, ownerCppName) == 0) {
            ++count;
        }
    }

    return count;
}

size_t CountPropertiesForOwner(const char* ownerCppName, const char* role) {
    if (!ownerCppName) return 0;

    size_t count = 0;
    for (size_t i = 0; i < GetPropertyCount(); ++i) {
        if (std::strcmp(kProperties[i].OwnerCppName, ownerCppName) != 0) continue;
        if (role && std::strcmp(kProperties[i].Role, role) != 0) continue;
        ++count;
    }

    return count;
}

size_t CountParametersForFunction(const char* ownerCppName, const char* functionName, bool includeReturnValue) {
    if (!ownerCppName || !functionName) return 0;

    size_t count = 0;
    for (size_t i = 0; i < GetParameterCount(); ++i) {
        if (std::strcmp(kParameters[i].OwnerCppName, ownerCppName) != 0) continue;
        if (std::strcmp(kParameters[i].FunctionName, functionName) != 0) continue;
        if (!includeReturnValue && kParameters[i].IsReturnParam) continue;
        ++count;
    }

    return count;
}

int32_t GetPropertyOffset(const char* ownerCppName, const char* propertyName, const char* role) {
    const PropertyRecord* record = FindPropertyByName(ownerCppName, propertyName, role);
    return record ? record->Offset : -1;
}

bool ValidateParameterRecord(const ParameterRecord* parameter, const FunctionRecord* function) {
    if (!parameter || !parameter->OwnerCppName || !parameter->FunctionName || !parameter->Name) {
        return false;
    }
    if (parameter->Offset < 0 || parameter->Size < 0 || parameter->ArrayDim < 0) {
        return false;
    }

    const FunctionRecord* ownerFunction = function;
    if (!ownerFunction) {
        ownerFunction = FindFunctionByName(parameter->OwnerCppName, parameter->FunctionName);
    }
    if (!ownerFunction) {
        return false;
    }

    const int64_t end = static_cast<int64_t>(parameter->Offset) + static_cast<int64_t>(parameter->Size);
    return end <= static_cast<int64_t>(ownerFunction->ParameterSize);
}

bool ValidateFunctionRecord(const FunctionRecord* function) {
    if (!function || !function->OwnerCppName || !function->Name || function->ParameterSize < 0 || function->ParamCount < 0) {
        return false;
    }

    size_t discoveredCount = 0;
    for (size_t i = 0; i < GetParameterCount(); ++i) {
        if (std::strcmp(kParameters[i].OwnerCppName, function->OwnerCppName) != 0) continue;
        if (std::strcmp(kParameters[i].FunctionName, function->Name) != 0) continue;
        if (!ValidateParameterRecord(&kParameters[i], function)) {
            return false;
        }
        ++discoveredCount;
    }

    return discoveredCount == static_cast<size_t>(function->ParamCount);
}

bool ValidatePropertyRecord(const PropertyRecord* property) {
    if (!property || !property->OwnerCppName || !property->Name) {
        return false;
    }
    if (property->Offset < 0 || property->Size <= 0 || property->ArrayDim < 0) {
        return false;
    }

    const TypeRecord* ownerType = FindTypeByName(property->OwnerCppName);
    if (!ownerType || ownerType->Size <= 0) {
        return false;
    }

    return ComputePropertyExtent(property) <= static_cast<int64_t>(ownerType->Size);
}

RuntimeStatus ValidateInvocation(const FunctionRecord* function, size_t paramsSize) {
    if (!function) {
        return RuntimeStatus::NotFound;
    }
    if (!ValidateFunctionRecord(function)) {
        return RuntimeStatus::InvalidArgument;
    }
    if (paramsSize != static_cast<size_t>(function->ParameterSize)) {
        return RuntimeStatus::InvalidSize;
    }

    return RuntimeStatus::Success;
}

RuntimeStatus InvokeFunctionChecked(const InvocationContext& context, void* object, const FunctionRecord* function, void* params, size_t paramsSize) {
    if (!object) {
        return RuntimeStatus::NullObject;
    }
    if (!context.ResolveFunction) {
        return RuntimeStatus::NullResolver;
    }
    if (!context.ProcessEvent) {
        return RuntimeStatus::NullInvoker;
    }

    const RuntimeStatus validation = ValidateInvocation(function, paramsSize);
    if (validation != RuntimeStatus::Success) {
        return validation;
    }
    if (function && function->ParameterSize > 0 && !params) {
        return RuntimeStatus::InvalidArgument;
    }

    void* resolvedFunction = context.ResolveFunction(object, function);
    if (!resolvedFunction) {
        return RuntimeStatus::NullFunction;
    }

    context.ProcessEvent(object, resolvedFunction, params);
    return RuntimeStatus::Success;
}

RuntimeStatus InvokeFunctionChecked(const InvocationContext& context, void* object, const char* ownerCppName, const char* functionName, void* params, size_t paramsSize) {
    return InvokeFunctionChecked(context, object, FindFunctionByName(ownerCppName, functionName), params, paramsSize);
}

const char* GetRuntimeStatusText(RuntimeStatus status) {
    switch (status) {
    case RuntimeStatus::Success: return "Success";
    case RuntimeStatus::InvalidArgument: return "InvalidArgument";
    case RuntimeStatus::NotFound: return "NotFound";
    case RuntimeStatus::NullObject: return "NullObject";
    case RuntimeStatus::NullFunction: return "NullFunction";
    case RuntimeStatus::NullResolver: return "NullResolver";
    case RuntimeStatus::NullInvoker: return "NullInvoker";
    case RuntimeStatus::InvalidOffset: return "InvalidOffset";
    case RuntimeStatus::InvalidSize: return "InvalidSize";
    case RuntimeStatus::TypeMismatch: return "TypeMismatch";
    default: return "Unknown";
    }
}

}
)";

        if (!WriteFile(outputDir, "Runtime\\ReflectionHelpers.h", header.str())) {
            return false;
        }
        return WriteFile(outputDir, "Runtime\\ReflectionHelpers.cpp", source.str());
    }

    bool WriteReflectionGraphs(
        const std::string& outputDir,
        const std::map<std::string, PackageSummary>& packageSummaries,
        const std::map<std::string, std::set<std::string>>& packageDependencies,
        const std::vector<ReflectionTypeRecord>& types) {

        std::vector<const ReflectionTypeRecord*> sortedTypes;
        sortedTypes.reserve(types.size());
        for (const auto& t : types) sortedTypes.push_back(&t);
        std::sort(sortedTypes.begin(), sortedTypes.end(), [](const ReflectionTypeRecord* lhs, const ReflectionTypeRecord* rhs) {
            if (lhs->PackageName != rhs->PackageName) return lhs->PackageName < rhs->PackageName;
            return lhs->CppName < rhs->CppName;
        });

        std::string inheritance;
        inheritance.reserve(2 * 1024 * 1024);
        inheritance.append("{\n  \"types\": [\n");
        bool firstType = true;
        for (const ReflectionTypeRecord* entry : sortedTypes) {
            if (!firstType) inheritance.append(",\n");
            firstType = false;
            inheritance.append("    {\n");
            inheritance.append("      \"id\": \""); AppendEscapedJson(inheritance, entry->Id); inheritance.append("\",\n");
            inheritance.append("      \"kind\": \""); AppendEscapedJson(inheritance, entry->Kind); inheritance.append("\",\n");
            inheritance.append("      \"package\": \""); AppendEscapedJson(inheritance, entry->PackageName); inheritance.append("\",\n");
            inheritance.append("      \"cpp_name\": \""); AppendEscapedJson(inheritance, entry->CppName); inheritance.append("\",\n");
            inheritance.append("      \"super_cpp_name\": \""); AppendEscapedJson(inheritance, entry->SuperCppName); inheritance.append("\"\n");
            inheritance.append("    }");
        }
        inheritance.append("\n  ]\n}\n");

        std::string packageGraph;
        packageGraph.reserve(512 * 1024);
        packageGraph.append("{\n  \"packages\": [\n");
        bool firstPackage = true;
        for (const auto& kv : packageSummaries) {
            if (!firstPackage) packageGraph.append(",\n");
            firstPackage = false;
            packageGraph.append("    {\n");
            packageGraph.append("      \"name\": \""); AppendEscapedJson(packageGraph, kv.first); packageGraph.append("\",\n");
            packageGraph.append("      \"dependency_count\": ");
            auto depIt = packageDependencies.find(kv.first);
            packageGraph.append(std::to_string(depIt == packageDependencies.end() ? 0 : static_cast<int>(depIt->second.size())));
            packageGraph.append(",\n      \"dependencies\": [");
            if (depIt != packageDependencies.end()) {
                bool firstDep = true;
                for (const std::string& dep : depIt->second) {
                    if (!firstDep) packageGraph.append(", ");
                    firstDep = false;
                    packageGraph.append("\""); AppendEscapedJson(packageGraph, dep); packageGraph.append("\"");
                }
            }
            packageGraph.append("]\n    }");
        }
        packageGraph.append("\n  ]\n}\n");

        if (!WriteFile(outputDir, "Inheritance.json", inheritance)) {
            return false;
        }
        return WriteFile(outputDir, "PackageGraph.json", packageGraph);
    }

    std::vector<ReflectionSnapshotEntry> BuildReflectionSnapshotEntries(
        const std::vector<ReflectionEnumRecord>& enums,
        const std::vector<ReflectionTypeRecord>& types,
        const std::vector<ReflectionFunctionRecord>& functions,
        const std::vector<ReflectionPropertyRecord>& properties) {

        std::vector<ReflectionSnapshotEntry> entries;
        entries.reserve(enums.size() + types.size() + functions.size() + properties.size());

        for (const ReflectionEnumRecord& entry : enums) {
            entries.push_back({"enum", entry.Id, entry.SignatureHash, entry.PackageName, entry.CppName, entry.FullPath});
        }
        for (const ReflectionTypeRecord& entry : types) {
            std::string extra;
            extra.reserve(64 + entry.SuperCppName.size());
            extra.append("super=").append(entry.SuperCppName)
                 .append("|size=").append(std::to_string(entry.Size))
                 .append("|own_size=").append(std::to_string(entry.OwnSize))
                 .append("|props=").append(std::to_string(entry.PropertyCount))
                 .append("|funcs=").append(std::to_string(entry.FunctionCount));
            entries.push_back({
                entry.Kind,
                entry.Id,
                entry.SignatureHash,
                entry.PackageName,
                entry.CppName,
                std::move(extra)
            });
        }
        for (const ReflectionFunctionRecord& entry : functions) {
            std::string extra;
            extra.reserve(64 + entry.ReturnType.size());
            extra.append("return=").append(entry.ReturnType)
                 .append("|param_size=").append(std::to_string(entry.ParameterSize))
                 .append("|param_count=").append(std::to_string(static_cast<int32_t>(entry.Params.size())))
                 .append("|flags=").append(std::to_string(entry.FunctionFlags));
            entries.push_back({
                "function",
                entry.Id,
                entry.SignatureHash,
                entry.OwnerCppName,
                entry.Name,
                std::move(extra)
            });
        }
        for (const ReflectionPropertyRecord& entry : properties) {
            std::string extra;
            extra.reserve(64 + entry.Role.size() + entry.CppType.size() + entry.RawType.size());
            extra.append("role=").append(entry.Role)
                 .append("|cpp_type=").append(entry.CppType)
                 .append("|raw_type=").append(entry.RawType)
                 .append("|offset=").append(std::to_string(entry.Offset))
                 .append("|size=").append(std::to_string(entry.Size))
                 .append("|flags=").append(std::to_string(entry.Flags));
            entries.push_back({
                "property",
                entry.Id,
                entry.SignatureHash,
                entry.OwnerCppName,
                entry.Name,
                std::move(extra)
            });
        }

        std::vector<size_t> indices(entries.size());
        for (size_t i = 0; i < indices.size(); ++i) indices[i] = i;
        std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
            if (entries[a].Kind != entries[b].Kind) return entries[a].Kind < entries[b].Kind;
            return entries[a].Id < entries[b].Id;
        });

        std::vector<ReflectionSnapshotEntry> sorted;
        sorted.reserve(entries.size());
        for (size_t idx : indices) {
            sorted.push_back(std::move(entries[idx]));
        }
        return sorted;
    }

    std::string SerializeSnapshotEntry(const ReflectionSnapshotEntry& entry) {
        return entry.Kind + "\t" + entry.Id + "\t" + entry.SignatureHash + "\t" +
               entry.OwnerCppName + "\t" + entry.Name + "\t" + entry.Extra;
    }

    bool ParseSnapshotEntry(const std::string& line, ReflectionSnapshotEntry& entry) {
        std::vector<std::string> parts;
        size_t start = 0;
        while (true) {
            size_t pos = line.find('\t', start);
            if (pos == std::string::npos) {
                parts.push_back(line.substr(start));
                break;
            }
            parts.push_back(line.substr(start, pos - start));
            start = pos + 1;
        }
        if (parts.size() < 6) return false;
        entry.Kind = parts[0];
        entry.Id = parts[1];
        entry.SignatureHash = parts[2];
        entry.OwnerCppName = parts[3];
        entry.Name = parts[4];
        entry.Extra = parts[5];
        return true;
    }

    bool ReadLinesFile(const std::string& path, std::vector<std::string>& lines) {
        const std::wstring widePath = ToWidePath(path);
        if (widePath.empty()) return false;
        std::ifstream in(fs::path(widePath), std::ios::binary);
        if (!in.is_open()) return false;

        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
        return true;
    }

    std::map<std::string, std::string> ParseSnapshotExtra(const std::string& extra) {
        std::map<std::string, std::string> fields;
        size_t start = 0;
        while (start <= extra.size()) {
            size_t sep = extra.find('|', start);
            std::string token = (sep == std::string::npos) ? extra.substr(start) : extra.substr(start, sep - start);
            size_t eq = token.find('=');
            if (eq != std::string::npos) {
                fields[token.substr(0, eq)] = token.substr(eq + 1);
            }
            if (sep == std::string::npos) break;
            start = sep + 1;
        }
        return fields;
    }

    std::string GetSnapshotField(const ReflectionSnapshotEntry& entry, const char* key) {
        const size_t keyLen = strlen(key);
        size_t pos = 0;
        const size_t extraSize = entry.Extra.size();
        while (pos < extraSize) {
            if ((pos == 0 || entry.Extra[pos - 1] == '|') &&
                pos + keyLen < extraSize &&
                entry.Extra[pos + keyLen] == '=' &&
                entry.Extra.compare(pos, keyLen, key) == 0) {
                size_t valStart = pos + keyLen + 1;
                size_t valEnd = entry.Extra.find('|', valStart);
                if (valEnd == std::string::npos) valEnd = extraSize;
                return entry.Extra.substr(valStart, valEnd - valStart);
            }
            pos = entry.Extra.find('|', pos);
            if (pos == std::string::npos) break;
            pos++;
        }
        return {};
    }

    bool LoadSnapshotMap(const std::string& snapshotPath, std::unordered_map<std::string, ReflectionSnapshotEntry>& outEntries) {
        std::vector<std::string> lines;
        if (!ReadLinesFile(snapshotPath, lines)) return false;
        outEntries.reserve(lines.size());
        for (const std::string& line : lines) {
            if (line.empty() || line[0] == '#') continue;
            ReflectionSnapshotEntry entry;
            if (ParseSnapshotEntry(line, entry)) {
                outEntries.emplace(entry.Id, std::move(entry));
            }
        }
        return true;
    }

    std::string DetermineDiffRisk(
        const char* change,
        const ReflectionSnapshotEntry* before,
        const ReflectionSnapshotEntry* after) {

        const ReflectionSnapshotEntry* effective = after ? after : before;
        if (!effective) return "low";
        if (strcmp(change, "removed") == 0) {
            if (effective->Kind == "class" || effective->Kind == "struct" || effective->Kind == "function") return "critical";
            if (effective->Kind == "property") return "high";
            return "medium";
        }
        if (strcmp(change, "added") == 0) {
            if (effective->Kind == "property" || effective->Kind == "function") return "low";
            return "medium";
        }
        if (!before || !after) return "low";

        if (effective->Kind == "property") {
            if (GetSnapshotField(*before, "offset") != GetSnapshotField(*after, "offset")) return "critical";
            if (GetSnapshotField(*before, "cpp_type") != GetSnapshotField(*after, "cpp_type")) return "high";
            if (GetSnapshotField(*before, "size") != GetSnapshotField(*after, "size")) return "high";
            return "medium";
        }
        if (effective->Kind == "function") {
            if (GetSnapshotField(*before, "return") != GetSnapshotField(*after, "return")) return "critical";
            if (GetSnapshotField(*before, "param_count") != GetSnapshotField(*after, "param_count")) return "high";
            if (GetSnapshotField(*before, "param_size") != GetSnapshotField(*after, "param_size")) return "high";
            return "medium";
        }
        if (effective->Kind == "class" || effective->Kind == "struct") {
            if (GetSnapshotField(*before, "super") != GetSnapshotField(*after, "super")) return "critical";
            if (GetSnapshotField(*before, "size") != GetSnapshotField(*after, "size")) return "high";
            if (GetSnapshotField(*before, "props") != GetSnapshotField(*after, "props") ||
                GetSnapshotField(*before, "funcs") != GetSnapshotField(*after, "funcs")) return "medium";
            return "medium";
        }
        return "low";
    }

    std::string DetermineDiffReason(
        const char* change,
        const ReflectionSnapshotEntry* before,
        const ReflectionSnapshotEntry* after) {

        const ReflectionSnapshotEntry* effective = after ? after : before;
        if (!effective) return "unknown";
        if (strcmp(change, "added") == 0) return "new symbol";
        if (strcmp(change, "removed") == 0) return "symbol removed";
        if (!before || !after) return "unknown";

        if (effective->Kind == "property") {
            if (GetSnapshotField(*before, "offset") != GetSnapshotField(*after, "offset")) return "offset changed";
            if (GetSnapshotField(*before, "cpp_type") != GetSnapshotField(*after, "cpp_type")) return "property type changed";
            if (GetSnapshotField(*before, "size") != GetSnapshotField(*after, "size")) return "property size changed";
            if (GetSnapshotField(*before, "flags") != GetSnapshotField(*after, "flags")) return "property flags changed";
            return "property metadata changed";
        }
        if (effective->Kind == "function") {
            if (GetSnapshotField(*before, "return") != GetSnapshotField(*after, "return")) return "return type changed";
            if (GetSnapshotField(*before, "param_count") != GetSnapshotField(*after, "param_count")) return "parameter count changed";
            if (GetSnapshotField(*before, "param_size") != GetSnapshotField(*after, "param_size")) return "parameter layout changed";
            if (GetSnapshotField(*before, "flags") != GetSnapshotField(*after, "flags")) return "function flags changed";
            return "function signature changed";
        }
        if (effective->Kind == "class" || effective->Kind == "struct") {
            if (GetSnapshotField(*before, "super") != GetSnapshotField(*after, "super")) return "inheritance changed";
            if (GetSnapshotField(*before, "size") != GetSnapshotField(*after, "size")) return "type size changed";
            if (GetSnapshotField(*before, "props") != GetSnapshotField(*after, "props")) return "property count changed";
            if (GetSnapshotField(*before, "funcs") != GetSnapshotField(*after, "funcs")) return "function count changed";
            return "type metadata changed";
        }
        return "enum values or metadata changed";
    }

    bool WriteReflectionDiffFromSnapshots(
        const std::string& outputDir,
        const EngineState& state,
        const std::unordered_map<std::string, ReflectionSnapshotEntry>& previousById,
        const std::unordered_map<std::string, ReflectionSnapshotEntry>& currentById,
        bool hasPreviousSnapshot) {

        struct DiffEntry {
            const char* Change = nullptr;
            const ReflectionSnapshotEntry* Before = nullptr;
            const ReflectionSnapshotEntry* After = nullptr;
        };
        std::vector<DiffEntry> diffs;
        diffs.reserve(std::max(previousById.size(), currentById.size()));

        for (const auto& kv : currentById) {
            auto prevIt = previousById.find(kv.first);
            if (prevIt == previousById.end()) {
                diffs.push_back({"added", nullptr, &kv.second});
            } else if (prevIt->second.SignatureHash != kv.second.SignatureHash) {
                diffs.push_back({"changed", &prevIt->second, &kv.second});
            }
        }
        for (const auto& kv : previousById) {
            if (currentById.find(kv.first) == currentById.end()) {
                diffs.push_back({"removed", &kv.second, nullptr});
            }
        }

        std::sort(diffs.begin(), diffs.end(), [](const DiffEntry& lhs, const DiffEntry& rhs) {
            const std::string& lhsKind = lhs.After ? lhs.After->Kind : lhs.Before->Kind;
            const std::string& rhsKind = rhs.After ? rhs.After->Kind : rhs.Before->Kind;
            if (lhsKind != rhsKind) return lhsKind < rhsKind;
            const std::string& lhsId = lhs.After ? lhs.After->Id : lhs.Before->Id;
            const std::string& rhsId = rhs.After ? rhs.After->Id : rhs.Before->Id;
            return lhsId < rhsId;
        });

        struct KindSummary {
            int Added = 0;
            int Removed = 0;
            int Changed = 0;
        };

        std::map<std::string, KindSummary> kindSummaries;
        int addedCount = 0;
        int removedCount = 0;
        int changedCount = 0;
        for (const DiffEntry& entry : diffs) {
            const std::string& kind = entry.After ? entry.After->Kind : entry.Before->Kind;
            KindSummary& summary = kindSummaries[kind];
            if (strcmp(entry.Change, "added") == 0) {
                ++addedCount;
                ++summary.Added;
            } else if (strcmp(entry.Change, "removed") == 0) {
                ++removedCount;
                ++summary.Removed;
            } else if (strcmp(entry.Change, "changed") == 0) {
                ++changedCount;
                ++summary.Changed;
            }
        }

        std::string json;
        json.reserve(32 * 1024 * 1024);
        json.append("{\n");
        json.append("  \"generator\": \"Avalonium ReflectionDiff\",\n");
        json.append("  \"format_version\": 2,\n");
        json.append("  \"game\": \""); AppendEscapedJson(json, state.GameName); json.append("\",\n");
        json.append("  \"engine\": \"UE"); json.append(std::to_string(state.UEMajor)); json.append("."); json.append(std::to_string(state.UEMinor)); json.append("\",\n");
        json.append("  \"has_previous_snapshot\": "); json.append(hasPreviousSnapshot ? "true" : "false"); json.append(",\n");
        json.append("  \"summary\": {\n");
        json.append("    \"previous_symbol_count\": "); json.append(std::to_string(previousById.size())); json.append(",\n");
        json.append("    \"current_symbol_count\": "); json.append(std::to_string(currentById.size())); json.append(",\n");
        json.append("    \"added\": "); json.append(std::to_string(addedCount)); json.append(",\n");
        json.append("    \"removed\": "); json.append(std::to_string(removedCount)); json.append(",\n");
        json.append("    \"changed\": "); json.append(std::to_string(changedCount)); json.append("\n");
        json.append("  },\n");
        json.append("  \"per_kind_summary\": [\n");
        bool firstKind = true;
        for (const auto& kv : kindSummaries) {
            if (!firstKind) json.append(",\n");
            firstKind = false;
            json.append("    {\n");
            json.append("      \"kind\": \""); AppendEscapedJson(json, kv.first); json.append("\",\n");
            json.append("      \"added\": "); json.append(std::to_string(kv.second.Added)); json.append(",\n");
            json.append("      \"removed\": "); json.append(std::to_string(kv.second.Removed)); json.append(",\n");
            json.append("      \"changed\": "); json.append(std::to_string(kv.second.Changed)); json.append("\n");
            json.append("    }");
        }
        json.append("\n  ],\n");
        json.append("  \"changes\": [\n");

        bool first = true;
        for (const DiffEntry& entry : diffs) {
            if (!first) json.append(",\n");
            first = false;
            const ReflectionSnapshotEntry& effective = entry.After ? *entry.After : *entry.Before;
            json.append("    {\n");
            json.append("      \"change\": \""); AppendEscapedJson(json, entry.Change); json.append("\",\n");
            json.append("      \"kind\": \""); AppendEscapedJson(json, effective.Kind); json.append("\",\n");
            json.append("      \"risk\": \""); AppendEscapedJson(json, DetermineDiffRisk(entry.Change, entry.Before, entry.After)); json.append("\",\n");
            json.append("      \"reason\": \""); AppendEscapedJson(json, DetermineDiffReason(entry.Change, entry.Before, entry.After)); json.append("\",\n");
            json.append("      \"id\": \""); AppendEscapedJson(json, effective.Id); json.append("\",\n");
            json.append("      \"owner\": \""); AppendEscapedJson(json, effective.OwnerCppName); json.append("\",\n");
            json.append("      \"name\": \""); AppendEscapedJson(json, effective.Name); json.append("\",\n");
            json.append("      \"extra\": \""); AppendEscapedJson(json, effective.Extra); json.append("\",\n");
            json.append("      \"before_hash\": \""); AppendEscapedJson(json, entry.Before ? entry.Before->SignatureHash : ""); json.append("\",\n");
            json.append("      \"after_hash\": \""); AppendEscapedJson(json, entry.After ? entry.After->SignatureHash : ""); json.append("\"\n");
            json.append("    }");
        }
        json.append("\n  ]\n");
        json.append("}\n");

        return WriteFile(outputDir, "Diff\\ReflectionDiff.json", json);
    }

    bool WriteReflectionDiff(
        const std::string& outputDir,
        const EngineState& state,
        const std::vector<ReflectionEnumRecord>& enums,
        const std::vector<ReflectionTypeRecord>& types,
        const std::vector<ReflectionFunctionRecord>& functions,
        const std::vector<ReflectionPropertyRecord>& properties) {

        const std::vector<ReflectionSnapshotEntry> currentEntries = BuildReflectionSnapshotEntries(enums, types, functions, properties);
        const std::string snapshotPath = outputDir + "\\Diff\\ReflectionSnapshot.tsv";
        std::unordered_map<std::string, ReflectionSnapshotEntry> previousById;
        const bool hasPreviousSnapshot = LoadSnapshotMap(snapshotPath, previousById);

        std::unordered_map<std::string_view, const ReflectionSnapshotEntry*> currentById;
        currentById.reserve(currentEntries.size());
        for (const auto& entry : currentEntries) {
            currentById.emplace(std::string_view(entry.Id), &entry);
        }

        struct DiffEntry {
            const char* Change = nullptr;
            const ReflectionSnapshotEntry* Before = nullptr;
            const ReflectionSnapshotEntry* After = nullptr;
        };
        std::vector<DiffEntry> diffs;
        diffs.reserve(std::max(previousById.size(), currentEntries.size()));

        for (const auto& entry : currentEntries) {
            auto prevIt = previousById.find(entry.Id);
            if (prevIt == previousById.end()) {
                diffs.push_back({"added", nullptr, &entry});
            } else if (prevIt->second.SignatureHash != entry.SignatureHash) {
                diffs.push_back({"changed", &prevIt->second, &entry});
            }
        }
        for (const auto& kv : previousById) {
            if (currentById.find(std::string_view(kv.first)) == currentById.end()) {
                diffs.push_back({"removed", &kv.second, nullptr});
            }
        }

        std::sort(diffs.begin(), diffs.end(), [](const DiffEntry& lhs, const DiffEntry& rhs) {
            const std::string& lhsKind = lhs.After ? lhs.After->Kind : lhs.Before->Kind;
            const std::string& rhsKind = rhs.After ? rhs.After->Kind : rhs.Before->Kind;
            if (lhsKind != rhsKind) return lhsKind < rhsKind;
            const std::string& lhsId = lhs.After ? lhs.After->Id : lhs.Before->Id;
            const std::string& rhsId = rhs.After ? rhs.After->Id : rhs.Before->Id;
            return lhsId < rhsId;
        });

        struct KindSummary {
            int Added = 0;
            int Removed = 0;
            int Changed = 0;
        };

        std::map<std::string, KindSummary> kindSummaries;
        int addedCount = 0;
        int removedCount = 0;
        int changedCount = 0;
        for (const DiffEntry& entry : diffs) {
            const std::string& kind = entry.After ? entry.After->Kind : entry.Before->Kind;
            KindSummary& summary = kindSummaries[kind];
            if (strcmp(entry.Change, "added") == 0) {
                ++addedCount;
                ++summary.Added;
            } else if (strcmp(entry.Change, "removed") == 0) {
                ++removedCount;
                ++summary.Removed;
            } else if (strcmp(entry.Change, "changed") == 0) {
                ++changedCount;
                ++summary.Changed;
            }
        }

        std::string json;
        json.reserve(32 * 1024 * 1024);
        json.append("{\n");
        json.append("  \"generator\": \"Avalonium ReflectionDiff\",\n");
        json.append("  \"format_version\": 2,\n");
        json.append("  \"game\": \""); AppendEscapedJson(json, state.GameName); json.append("\",\n");
        json.append("  \"engine\": \"UE"); json.append(std::to_string(state.UEMajor)); json.append("."); json.append(std::to_string(state.UEMinor)); json.append("\",\n");
        json.append("  \"has_previous_snapshot\": "); json.append(hasPreviousSnapshot ? "true" : "false"); json.append(",\n");
        json.append("  \"summary\": {\n");
        json.append("    \"previous_symbol_count\": "); json.append(std::to_string(previousById.size())); json.append(",\n");
        json.append("    \"current_symbol_count\": "); json.append(std::to_string(currentEntries.size())); json.append(",\n");
        json.append("    \"added\": "); json.append(std::to_string(addedCount)); json.append(",\n");
        json.append("    \"removed\": "); json.append(std::to_string(removedCount)); json.append(",\n");
        json.append("    \"changed\": "); json.append(std::to_string(changedCount)); json.append("\n");
        json.append("  },\n");
        json.append("  \"per_kind_summary\": [\n");
        bool firstKind = true;
        for (const auto& kv : kindSummaries) {
            if (!firstKind) json.append(",\n");
            firstKind = false;
            json.append("    {\n");
            json.append("      \"kind\": \""); AppendEscapedJson(json, kv.first); json.append("\",\n");
            json.append("      \"added\": "); json.append(std::to_string(kv.second.Added)); json.append(",\n");
            json.append("      \"removed\": "); json.append(std::to_string(kv.second.Removed)); json.append(",\n");
            json.append("      \"changed\": "); json.append(std::to_string(kv.second.Changed)); json.append("\n");
            json.append("    }");
        }
        json.append("\n  ],\n");
        json.append("  \"changes\": [\n");

        bool first = true;
        for (const DiffEntry& entry : diffs) {
            if (!first) json.append(",\n");
            first = false;
            const ReflectionSnapshotEntry& effective = entry.After ? *entry.After : *entry.Before;
            json.append("    {\n");
            json.append("      \"change\": \""); AppendEscapedJson(json, entry.Change); json.append("\",\n");
            json.append("      \"kind\": \""); AppendEscapedJson(json, effective.Kind); json.append("\",\n");
            json.append("      \"risk\": \""); AppendEscapedJson(json, DetermineDiffRisk(entry.Change, entry.Before, entry.After)); json.append("\",\n");
            json.append("      \"reason\": \""); AppendEscapedJson(json, DetermineDiffReason(entry.Change, entry.Before, entry.After)); json.append("\",\n");
            json.append("      \"id\": \""); AppendEscapedJson(json, effective.Id); json.append("\",\n");
            json.append("      \"owner\": \""); AppendEscapedJson(json, effective.OwnerCppName); json.append("\",\n");
            json.append("      \"name\": \""); AppendEscapedJson(json, effective.Name); json.append("\",\n");
            json.append("      \"extra\": \""); AppendEscapedJson(json, effective.Extra); json.append("\",\n");
            json.append("      \"before_hash\": \""); AppendEscapedJson(json, entry.Before ? entry.Before->SignatureHash : ""); json.append("\",\n");
            json.append("      \"after_hash\": \""); AppendEscapedJson(json, entry.After ? entry.After->SignatureHash : ""); json.append("\"\n");
            json.append("    }");
        }
        json.append("\n  ]\n");
        json.append("}\n");

        if (!WriteFile(outputDir, "Diff\\ReflectionDiff.json", json)) {
            return false;
        }

        std::string snapshot;
        snapshot.reserve(16 * 1024 * 1024);
        snapshot.append("# kind\tid\thash\towner\tname\textra\n");
        for (const ReflectionSnapshotEntry& entry : currentEntries) {
            snapshot.append(SerializeSnapshotEntry(entry));
            snapshot.push_back('\n');
        }
        return WriteFile(outputDir, "Diff\\ReflectionSnapshot.tsv", snapshot);
    }

}
