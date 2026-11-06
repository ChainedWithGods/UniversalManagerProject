#include "BlueprintDiscovery.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "../utils/SafeMemory.h"
#include "EngineDiscovery.h"
#include "Logger.h"

namespace EngineDiscovery {

UObject *FindClassDefaultObject(EngineState &state, UClass *cls) {
    if (!cls) return nullptr;

    if (state.UClass_ClassDefaultObjectOff >= 0) {
        void *cdo = nullptr;
        if (SafeMem_Read(reinterpret_cast<uint8_t *>(cls) + state.UClass_ClassDefaultObjectOff,
                         &cdo, sizeof(void *)) && cdo) {
            if (GetObjectClass(state, static_cast<UObject *>(cdo)) == cls) {
                return static_cast<UObject *>(cdo);
            }
        }
    }

    const std::string className = GetObjectNameStr(state, static_cast<UObject *>(cls));
    if (!className.empty()) {
        UObject *byName = FindObjectByNameFast(state, "Default__" + className);
        if (byName && GetObjectClass(state, byName) == cls) return byName;
    }

    if (state.UObject_FlagsOff >= 0) {
        const int32_t count = GetObjectCount(state);
        for (int32_t i = 0; i < count; ++i) {
            UObject *obj = GetObjectByIndex(state, i);
            if (!obj) continue;
            uint32_t flags = 0;
            if (!SafeMem_Read(reinterpret_cast<uint8_t *>(obj) + state.UObject_FlagsOff,
                              &flags, sizeof(flags)))
                continue;
            if ((flags & 0x00000010u) == 0) continue;
            if (GetObjectClass(state, obj) == cls) return obj;
        }
    }

    const std::vector<UObject *> instances = FindObjectsByClassFast(state, cls);
    for (UObject *obj : instances) {
        if (obj && GetObjectClass(state, obj) == cls) return obj;
    }
    return nullptr;
}

namespace {

constexpr uint64_t kCpfParm = 0x0000000000000080ULL;
constexpr uint64_t kCpfOutParm = 0x0000000000000100ULL;
constexpr uint64_t kCpfReturnParm = 0x0000000000000400ULL;

constexpr int32_t kMaxParmsSize = 0xFFFF;

int32_t PropertyTotalSize(const ReflectedMember &p) {

    const int32_t dim = p.ArrayDim > 0 ? p.ArrayDim : 1;
    return p.ElementSize * dim;
}

bool TypeNameIs(const std::string &typeName, const char *what) {
    return typeName.find(what) != std::string::npos;
}

int32_t PropertyOffsetField(const EngineState &state) {
    if (!state.bUseFProperty && state.UProperty_OffsetOff >= 0) {
        return state.UProperty_OffsetOff;
    }
    return state.FProperty_OffsetOff;
}

void ReadPropertyFields(const EngineState &state, void *property,
                        ReflectedMember &out) {
    out.Property = property;

    const int32_t offsetField = PropertyOffsetField(state);
    if (offsetField >= 0) {
        SafeMem_Read(reinterpret_cast<uint8_t *>(property) + offsetField,
                     &out.Offset, 4);
    }
    if (state.FProperty_ElementSizeOff >= 0) {
        SafeMem_Read(reinterpret_cast<uint8_t *>(property) + state.FProperty_ElementSizeOff,
                     &out.ElementSize, 4);
    }
    if (state.FProperty_ArrayDimOff >= 0) {
        SafeMem_Read(reinterpret_cast<uint8_t *>(property) + state.FProperty_ArrayDimOff,
                     &out.ArrayDim, 4);
        if (out.ArrayDim <= 0) out.ArrayDim = 1;
    }
    if (state.FProperty_PropertyFlagsOff >= 0) {
        SafeMem_Read(reinterpret_cast<uint8_t *>(property) + state.FProperty_PropertyFlagsOff,
                     &out.Flags, 8);
    }

    if (TypeNameIs(out.TypeName, "StructProperty") &&
        state.FStructProperty_StructOff >= 0) {
        void *nested = nullptr;
        SafeMem_Read(reinterpret_cast<uint8_t *>(property) + state.FStructProperty_StructOff,
                     &nested, sizeof(void *));
        out.Struct = nested;
    }
}

bool CollectStructProperties(const EngineState &state, UStruct *owner,
                             std::vector<ReflectedMember> &out) {
    if (!owner) return false;

    const bool useFField = state.UStruct_ChildPropsOff >= 0;
    if (!useFField && state.UStruct_ChildrenOff < 0) return false;

    constexpr int kMaxMembers = 8192;

    if (useFField) {
        void *child = GetChildPropsHead(state, owner);
        for (int guard = 0; child && guard < kMaxMembers; ++guard) {
            ReflectedMember p;
            p.Name = GetFFieldName(state, child);
            p.TypeName = GetFFieldTypeName(state, child);
            ReadPropertyFields(state, child, p);
            out.push_back(std::move(p));
            child = GetFFieldNext(state, child);
        }
        return true;
    }

    void *child = GetChildrenHead(state, owner);
    for (int guard = 0; child && guard < kMaxMembers; ++guard) {
        ReflectedMember p;
        UObject *childObj = static_cast<UObject *>(child);
        p.Name = GetObjectNameStr(state, childObj);
        UClass *propClass = GetObjectClass(state, childObj);
        p.TypeName = propClass
                         ? GetObjectNameStr(state, static_cast<UObject *>(propClass))
                         : std::string();
        ReadPropertyFields(state, child, p);
        out.push_back(std::move(p));
        child = GetFieldNext(state, child);
    }
    return true;
}

bool MetaClassChainContains(const EngineState &state, UClass *cls, const char *name,
                            int *depthOut = nullptr) {
    if (!cls) return false;
    UClass *meta = GetObjectClass(state, static_cast<UObject *>(cls));
    UStruct *cursor = static_cast<UStruct *>(meta);
    for (int depth = 0; cursor && depth < 32; ++depth) {
        if (depthOut && *depthOut < depth) *depthOut = depth;
        const std::string n = GetObjectNameStr(state, static_cast<UObject *>(cursor));
        if (_stricmp(n.c_str(), name) == 0) return true;
        cursor = GetSuperStruct(state, cursor);
    }
    return false;
}

int32_t FNameWidth(const EngineState &state) {

    int32_t width = state.FName_Size > 0 ? state.FName_Size : 8;
    if (width < 4) width = 4;
    if (width > 16) width = 16;
    return width;
}

struct NameWriter {
    bool bProbed = false;
    bool bReady = false;
    std::string Detail;
    UObject *LibraryCDO = nullptr;
    void *Function = nullptr;
    int32_t FrameSize = 0;
    int32_t InputOffset = -1;
    int32_t InputSize = 0;
    int32_t ReturnOffset = -1;
    int32_t ReturnSize = 0;
};

std::mutex g_nameWriterMutex;

NameWriter &GetNameWriter(EngineState &state) {
    static NameWriter writer;
    std::lock_guard<std::mutex> lock(g_nameWriterMutex);
    if (writer.bProbed) return writer;
    writer.bProbed = true;

    UClass *libraryClass = FindClassCached(state, "KismetStringLibrary");
    if (!libraryClass) {
        writer.Detail = "UKismetStringLibrary is not loaded, so no name can be created";
        return writer;
    }

    writer.LibraryCDO = FindClassDefaultObject(state, libraryClass);
    if (!writer.LibraryCDO) {
        writer.Detail = "UKismetStringLibrary has no class default object to call";
        return writer;
    }

    writer.Function = FindFunctionByName(state, reinterpret_cast<UStruct *>(libraryClass),
                                         "Conv_StringToName");
    if (!writer.Function) {
        writer.Detail = "UKismetStringLibrary::Conv_StringToName is not reflected on this build";
        return writer;
    }

    std::vector<ReflectedMember> params;
    CollectStructProperties(state, reinterpret_cast<UStruct *>(writer.Function), params);
    writer.FrameSize = GetFunctionFrameSize(state, writer.Function);

    for (const ReflectedMember &p : params) {
        if ((p.Flags & kCpfParm) == 0) continue;
        if ((p.Flags & kCpfReturnParm) != 0) {
            if (p.Offset >= 0) {
                writer.ReturnOffset = p.Offset;
                writer.ReturnSize = PropertyTotalSize(p);
            }
            continue;
        }
        if (writer.InputOffset >= 0) continue;

        const bool nameMatch = _stricmp(p.Name.c_str(), "InString") == 0;
        const bool typeMatch = TypeNameIs(p.TypeName, "StrProperty");
        if ((nameMatch || typeMatch) && p.Offset >= 0) {
            writer.InputOffset = p.Offset;
            writer.InputSize = PropertyTotalSize(p);
        }
    }

    if (writer.InputOffset < 0) {
        writer.Detail = "the string-to-name conversion has no string parameter";
        return writer;
    }
    if (writer.ReturnOffset < 0) {
        writer.Detail = "the string-to-name conversion has no return value";
        return writer;
    }
    if (writer.FrameSize <= 0) {
        writer.Detail = "the string-to-name conversion has no argument frame";
        return writer;
    }

    writer.bReady = true;
    writer.Detail = "UKismetStringLibrary::Conv_StringToName";
    Log("[Blueprint] Name creation available through %s (frame=%d bytes, "
        "argument at %d, return at %d)",
        writer.Detail.c_str(), writer.FrameSize, writer.InputOffset, writer.ReturnOffset);
    return writer;
}

bool WriteKnownFName(EngineState &state, int32_t index, uint8_t *out, int32_t width,
                     uint32_t number = 0) {
    if (!out || width < 4) return false;
    std::memset(out, 0, static_cast<size_t>(width));

    const uint32_t comparisonIndex = static_cast<uint32_t>(index < 0 ? 0 : index);
    std::memcpy(out, &comparisonIndex, 4);

    if (state.FName_NumberOff >= 0 && state.FName_NumberOff + 4 <= width) {
        std::memcpy(out + state.FName_NumberOff, &number, 4);
    }

    if (width >= 12 && state.FName_NumberOff == 4) {
        std::memcpy(out + 8, &comparisonIndex, 4);
    } else if (width >= 12 && state.FName_NumberOff == 8) {
        std::memcpy(out + 4, &comparisonIndex, 4);
    }
    return true;
}

void PublishCreatedName(EngineState &state, const std::string &text,
                        const uint8_t *bytes) {
    uint32_t index = 0;
    std::memcpy(&index, bytes, 4);
    std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);
    state.StringToFNameIndex[text] = static_cast<int32_t>(index);
    state.FNameCache[static_cast<int32_t>(index)] = text;
}

struct EngineFString {
    const wchar_t *Data;
    int32_t Num;
    int32_t Max;
};

bool CreateFNameThroughEngine(EngineState &state, const std::string &text, uint8_t *out,
                              int32_t width, std::string *detail) {
    NameWriter &writer = GetNameWriter(state);
    if (!writer.bReady) {
        if (detail) *detail = writer.Detail;
        return false;
    }

    const int32_t frameSize = writer.FrameSize;
    if (frameSize <= 0 || frameSize > kMaxParmsSize || writer.InputOffset < 0 ||
        writer.ReturnOffset < 0 ||
        writer.InputOffset + static_cast<int32_t>(sizeof(EngineFString)) > frameSize) {
        if (detail) *detail = "the conversion's argument frame did not resolve";
        return false;
    }

    std::vector<uint64_t> frame(static_cast<size_t>((frameSize + 7) / 8) + 1, 0);
    uint8_t *base = reinterpret_cast<uint8_t *>(frame.data());

    std::wstring wideText;
    {
        const int wideChars =
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (wideChars <= 0) {
            if (detail) *detail = "the name text is not representable as UTF-16";
            return false;
        }
        wideText.resize(static_cast<size_t>(wideChars));
        if (MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wideText.data(),
                                wideChars) <= 0) {
            if (detail) *detail = "the name text did not convert to UTF-16";
            return false;
        }
    }

    const EngineFString argument = {wideText.c_str(),
                                    static_cast<int32_t>(wideText.size()),
                                    static_cast<int32_t>(wideText.size())};
    std::memcpy(base + writer.InputOffset, &argument, sizeof(argument));

    if (!CallProcessEvent(state, writer.LibraryCDO, writer.Function, base)) {
        if (detail) *detail =
            "the engine's string-to-name conversion raised an exception";
        return false;
    }

    int32_t copyWidth = width;
    if (writer.ReturnSize > 0 && copyWidth > writer.ReturnSize) copyWidth = writer.ReturnSize;
    if (copyWidth <= 0 || writer.ReturnOffset + copyWidth > frameSize) {
        if (detail) *detail = "the conversion's return value does not fit its frame";
        return false;
    }
    if (!SafeMem_Read(base + writer.ReturnOffset, out,
                      static_cast<size_t>(copyWidth))) {
        if (detail) *detail = "the conversion's return value was not readable";
        return false;
    }
    return true;
}

}

int32_t GetFunctionFrameSize(const EngineState &state, void *ufunction) {
    if (!ufunction) return 0;

    std::vector<ReflectedMember> params;
    if (!CollectStructProperties(state, reinterpret_cast<UStruct *>(ufunction), params)) {
        return 0;
    }

    int32_t size = 0;
    for (const ReflectedMember &p : params) {
        if ((p.Flags & kCpfParm) == 0) continue;

        if (p.Offset < 0 || p.Offset > kMaxParmsSize) {
            Log("[Blueprint] %s reports a parameter at offset %d, which no "
                "parameter frame can hold; no frame is used",
                GetObjectNameStr(state, reinterpret_cast<UObject *>(ufunction)).c_str(),
                p.Offset);
            return 0;
        }
        const int32_t end = p.Offset + PropertyTotalSize(p);
        if (end < 0 || end > kMaxParmsSize) {
            Log("[Blueprint] %s reports an argument frame of %d bytes; no frame "
                "is used",
                GetObjectNameStr(state, reinterpret_cast<UObject *>(ufunction)).c_str(),
                end);
            return 0;
        }
        if (end > size) size = end;
    }
    return size;
}

bool GetReflectedMembers(const EngineState &state, UStruct *owner,
                         std::vector<ReflectedMember> &out) {
    return CollectStructProperties(state, owner, out);
}

namespace {

const ReflectedMember *FindParameter(const std::vector<ReflectedMember> &params,
                                     const char *name) {
    if (!name) return nullptr;
    for (const ReflectedMember &p : params) {
        if ((p.Flags & kCpfParm) == 0) continue;
        if ((p.Flags & kCpfReturnParm) != 0) continue;
        if (_stricmp(p.Name.c_str(), name) == 0) return &p;
    }
    return nullptr;
}

}

bool GetFunctionParameter(EngineState &state, const char *className,
                          const char *functionName, const char *parameterName,
                          ReflectedMember &out) {
    if (!className || !functionName || !parameterName) return false;

    UClass *cls = FindClassCached(state, className);
    if (!cls) return false;
    void *function = FindFunctionByName(state, reinterpret_cast<UStruct *>(cls), functionName);
    if (!function) return false;

    std::vector<ReflectedMember> params;
    if (!CollectStructProperties(state, reinterpret_cast<UStruct *>(function), params)) {
        return false;
    }
    const ReflectedMember *param = FindParameter(params, parameterName);
    if (!param) return false;

    out = *param;
    return true;
}

bool CallStaticFunctionByName(EngineState &state, const char *className,
                              const char *functionName, const ReflectedArgument *args,
                              int32_t argCount, void *result, int32_t resultSize) {
    if (!className || !functionName) return false;

    UClass *cls = FindClassCached(state, className);
    if (!cls) return false;
    UObject *cdo = FindClassDefaultObject(state, cls);
    if (!cdo) return false;
    void *function = FindFunctionByName(state, reinterpret_cast<UStruct *>(cls), functionName);
    if (!function) return false;

    const int32_t frameSize = GetFunctionFrameSize(state, function);
    if (frameSize <= 0) return false;

    std::vector<uint64_t> frame(static_cast<size_t>((frameSize + 7) / 8) + 1, 0);
    uint8_t *base = reinterpret_cast<uint8_t *>(frame.data());

    std::vector<ReflectedMember> params;
    if (!CollectStructProperties(state, reinterpret_cast<UStruct *>(function), params)) {
        return false;
    }

    for (int32_t i = 0; i < (args ? argCount : 0); ++i) {
        const ReflectedArgument &argument = args[i];
        if (!argument.Parameter || !argument.Value || argument.Size <= 0) continue;

        const ReflectedMember *param = FindParameter(params, argument.Parameter);
        if (!param) continue;
        if (param->Offset < 0 || param->Offset + argument.Size > frameSize) continue;
        std::memcpy(base + param->Offset, argument.Value,
                    static_cast<size_t>(argument.Size));
    }

    if (!CallProcessEvent(state, cdo, function, base)) return false;

    if (result) {
        const int32_t returnOffset = GetFunctionReturnValueOffset(state, function);
        if (returnOffset < 0) return false;
        int32_t copy = resultSize > 0 ? resultSize : static_cast<int32_t>(sizeof(void *));
        if (returnOffset + copy > frameSize) return false;
        std::memcpy(result, base + returnOffset, static_cast<size_t>(copy));
    }
    return true;
}

int32_t GetFunctionReturnValueOffset(const EngineState &state, void *ufunction) {
    if (!ufunction) return -1;

    std::vector<ReflectedMember> params;
    if (!CollectStructProperties(state, reinterpret_cast<UStruct *>(ufunction), params)) {
        return -1;
    }
    for (const ReflectedMember &p : params) {
        if ((p.Flags & kCpfParm) == 0) continue;
        if ((p.Flags & kCpfReturnParm) != 0) return p.Offset;
    }
    return -1;
}

bool CanCreateFNames(EngineState &state) { return GetNameWriter(state).bReady; }

bool WriteFNameBytes(EngineState &state, uint32_t index, uint32_t number, void *dest,
                     int32_t width) {
    if (!dest) return false;
    const int32_t useWidth = width > 0 ? width : (state.FName_Size > 0 ? state.FName_Size : 8);
    return WriteKnownFName(state, static_cast<int32_t>(index), static_cast<uint8_t *>(dest),
                           useWidth, number);
}

bool FindOrAddFName(EngineState &state, const std::string &text, FNameBytes &out,
                    std::string *detail) {
    out = FNameBytes{};
    const int32_t width = FNameWidth(state);
    out.Size = width;

    if (text.empty()) {
        if (detail) *detail = "the name is empty";
        return false;
    }

    const int32_t known = ResolveFNameIndex(state, text);
    if (known >= 0) {
        return WriteKnownFName(state, known, out.Bytes, width);
    }

    std::string why;
    if (CreateFNameThroughEngine(state, text, out.Bytes, width, &why)) {
        PublishCreatedName(state, text, out.Bytes);
        return true;
    }

    if (detail) *detail = why;
    return false;
}

int32_t FindOrAddFNameIndex(EngineState &state, const std::string &text,
                            std::string *detail) {
    FNameBytes bytes;
    if (!FindOrAddFName(state, text, bytes, detail)) return -1;
    uint32_t index = 0;
    std::memcpy(&index, bytes.Bytes, 4);
    return static_cast<int32_t>(index);
}

namespace {

std::mutex g_bindingMutex;

AssetRegistryBinding ProbeAssetRegistryBinding(EngineState &state) {
    AssetRegistryBinding binding;

    UClass *helpersClass = FindClassCached(state, "AssetRegistryHelpers");
    if (!helpersClass) {

        UObject *byPath = FindObjectByPath(
            state, "/Script/AssetRegistry.Default__AssetRegistryHelpers");
        if (byPath) helpersClass = GetObjectClass(state, byPath);
    }
    if (!helpersClass) {
        binding.Detail = "UAssetRegistryHelpers is not loaded, so no asset can be loaded";
        return binding;
    }
    binding.HelpersClass = reinterpret_cast<UObject *>(helpersClass);

    binding.HelpersCDO = FindClassDefaultObject(state, helpersClass);
    if (!binding.HelpersCDO) {
        binding.Detail = "UAssetRegistryHelpers has no class default object to call";
        return binding;
    }

    UStruct *helpersStruct = reinterpret_cast<UStruct *>(helpersClass);
    binding.FnGetAsset = FindFunctionByName(state, helpersStruct, "GetAsset");
    binding.FnGetAssetRegistry =
        FindFunctionByName(state, helpersStruct, "GetAssetRegistry");
    binding.FnGetClass = FindFunctionByName(state, helpersStruct, "GetClass");
    binding.FnIsValid = FindFunctionByName(state, helpersStruct, "IsValid");
    binding.FnCreateAssetData =
        FindFunctionByName(state, helpersStruct, "CreateAssetData");

    if (!binding.FnGetAsset) {
        binding.Detail = "UAssetRegistryHelpers::GetAsset is not reflected on this build";
        return binding;
    }

    std::vector<ReflectedMember> params;
    CollectStructProperties(state, reinterpret_cast<UStruct *>(binding.FnGetAsset), params);

    void *structProperty = nullptr;
    int32_t structSize = 0;
    int32_t structOffset = 0;
    for (const ReflectedMember &p : params) {
        if ((p.Flags & kCpfParm) == 0) continue;
        if (!TypeNameIs(p.TypeName, "StructProperty")) continue;
        structProperty = p.Property;
        structSize = p.ElementSize;
        structOffset = p.Offset;
        break;
    }
    if (!structProperty || state.FStructProperty_StructOff < 0) {
        binding.Detail =
            "UAssetRegistryHelpers::GetAsset does not take a reflected structure";
        return binding;
    }

    void *scriptStruct = nullptr;
    SafeMem_Read(reinterpret_cast<uint8_t *>(structProperty) + state.FStructProperty_StructOff,
                 &scriptStruct, sizeof(void *));
    if (!scriptStruct || structSize <= 0) {
        binding.Detail = "the structure GetAsset takes has no reflected definition";
        return binding;
    }
    binding.AssetDataStruct = reinterpret_cast<UStruct *>(scriptStruct);
    binding.AssetDataSize = structSize;
    binding.AssetDataOffset = structOffset;
    binding.GetAssetFrameSize = GetFunctionFrameSize(state, binding.FnGetAsset);
    binding.AssetDataStructName =
        GetObjectNameStr(state, static_cast<UObject *>(scriptStruct));

    std::vector<ReflectedMember> fields;
    CollectStructProperties(state, binding.AssetDataStruct, fields);
    binding.Fields.reserve(fields.size());
    for (const ReflectedMember &f : fields) {
        AssetDataField field;
        field.Name = f.Name;
        field.TypeName = f.TypeName;
        field.Property = f.Property;
        field.Offset = f.Offset;
        field.Size = f.ElementSize;
        binding.Fields.push_back(std::move(field));
    }

    for (const AssetDataField &f : binding.Fields) {
        const std::string &n = f.Name;
        if (_stricmp(n.c_str(), "ObjectPath") == 0) binding.OffObjectPath = f.Offset;
        else if (_stricmp(n.c_str(), "PackageName") == 0) binding.OffPackageName = f.Offset;
        else if (_stricmp(n.c_str(), "PackagePath") == 0) binding.OffPackagePath = f.Offset;
        else if (_stricmp(n.c_str(), "AssetName") == 0) binding.OffAssetName = f.Offset;
        else if (_stricmp(n.c_str(), "AssetClass") == 0) binding.OffAssetClass = f.Offset;
        else if (_stricmp(n.c_str(), "AssetClassPath") == 0) binding.OffAssetClassPath = f.Offset;
    }

    if (binding.FnGetAssetRegistry) {
        const int32_t frameSize = GetFunctionFrameSize(state, binding.FnGetAssetRegistry);
        std::vector<uint64_t> frame(
            static_cast<size_t>((frameSize > 0 ? frameSize : 16) + 7) / 8 + 1, 0);
        const int32_t returnOffset =
            GetFunctionReturnValueOffset(state, binding.FnGetAssetRegistry);
        if (returnOffset >= 0 &&
            CallProcessEvent(state, binding.HelpersCDO, binding.FnGetAssetRegistry,
                             frame.data())) {
            UObject *registry = nullptr;
            SafeMem_Read(reinterpret_cast<uint8_t *>(frame.data()) + returnOffset,
                         &registry, sizeof(void *));
            binding.Registry = registry;
        }
    }

    binding.bReady = true;
    return binding;
}

}

const AssetRegistryBinding &GetAssetRegistryBinding(EngineState &state) {
    static AssetRegistryBinding binding;
    static bool probed = false;

    std::lock_guard<std::mutex> lock(g_bindingMutex);
    if (probed) return binding;

    binding = ProbeAssetRegistryBinding(state);
    probed = true;

    if (binding.bReady) {
        Log("[Blueprint] %s", DescribeAssetRegistryBinding(binding).c_str());
    } else {
        Log("[Blueprint] Asset pipeline unavailable: %s", binding.Detail.c_str());
    }
    return binding;
}

std::string DescribeAssetRegistryBinding(const AssetRegistryBinding &binding) {
    if (!binding.bReady) return "asset pipeline unavailable: " + binding.Detail;

    std::string fields;
    for (const AssetDataField &f : binding.Fields) {
        if (!fields.empty()) fields += ", ";
        fields += f.Name + "@" + std::to_string(f.Offset);
    }

    std::string available;
    const struct {
        const char *Name;
        void *Function;
    } functions[] = {
        {"GetAssetRegistry", binding.FnGetAssetRegistry},
        {"CreateAssetData", binding.FnCreateAssetData},
        {"IsValid", binding.FnIsValid},
        {"GetClass", binding.FnGetClass},
        {"GetAsset", binding.FnGetAsset},
    };
    for (const auto &entry : functions) {
        if (!entry.Function) continue;
        if (!available.empty()) available += ", ";
        available += entry.Name;
    }

    return "UAssetRegistryHelpers resolved (functions: " + available + "; " +
           (binding.AssetDataStructName.empty() ? std::string("asset data")
                                                : binding.AssetDataStructName) +
           " " + std::to_string(binding.AssetDataSize) + " bytes [" + fields +
           "]; registry " + (binding.Registry ? "live" : "not resolved") + ")";
}

bool BuildAssetData(EngineState &state, const AssetRegistryBinding &binding,
                    const std::string &packagePath, const std::string &assetName,
                    uint8_t *buffer, int32_t size, std::string *detail) {
    if (!binding.bReady) {
        if (detail) *detail = binding.Detail;
        return false;
    }
    if (!buffer || size < binding.AssetDataSize || packagePath.empty() || assetName.empty()) {
        if (detail) *detail = "the asset data buffer is too small for this build's structure";
        return false;
    }

    std::memset(buffer, 0, static_cast<size_t>(size));

    const std::string objectPathName = packagePath + "." + assetName;
    const size_t lastSlash = packagePath.rfind('/');
    const std::string packageFolder =
        lastSlash == std::string::npos ? std::string() : packagePath.substr(0, lastSlash);

    const struct {
        int32_t Offset;
        const std::string *Text;
    } nameFields[] = {
        {binding.OffObjectPath, &objectPathName},
        {binding.OffPackageName, &packagePath},
        {binding.OffAssetName, &assetName},
        {binding.OffPackagePath, &packageFolder},
    };

    bool wroteAnyPath = false;
    for (const auto &entry : nameFields) {
        if (entry.Offset < 0 || entry.Text->empty()) continue;
        if (entry.Offset + 4 > size) continue;

        FNameBytes name;
        std::string why;
        if (!FindOrAddFName(state, *entry.Text, name, &why)) {
            if (detail) {
                *detail = "the asset path could not be named ('" + *entry.Text + "'): " + why;
            }
            return false;
        }
        const int32_t width = std::min(name.Size, size - entry.Offset);
        if (width > 0) {
            std::memcpy(buffer + entry.Offset, name.Bytes, static_cast<size_t>(width));
            wroteAnyPath = true;
        }
    }

    if (!wroteAnyPath) {
        if (detail) *detail = "this build's asset data carries no path member to fill";
        return false;
    }
    return true;
}

UObject *LoadAssetByPath(EngineState &state, const std::string &objectPath,
                         std::string *detail) {
    const AssetRegistryBinding &binding = GetAssetRegistryBinding(state);
    if (!binding.bReady) {
        if (detail) *detail = binding.Detail;
        return nullptr;
    }

    std::string packageName = objectPath;
    std::string assetName;
    const size_t dot = objectPath.rfind('.');
    const size_t slash = objectPath.rfind('/');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        packageName = objectPath.substr(0, dot);
        assetName = objectPath.substr(dot + 1);
    } else if (slash != std::string::npos) {
        assetName = objectPath.substr(slash + 1);
    }

    if (packageName.empty() || assetName.empty()) {
        if (detail) {
            *detail = "an asset path needs a package and an asset name: '" + objectPath + "'";
        }
        return nullptr;
    }

    const int32_t needed = std::max(binding.GetAssetFrameSize,
                                    binding.AssetDataOffset + binding.AssetDataSize) + 8;
    std::vector<uint64_t> frame(static_cast<size_t>((needed + 7) / 8) + 1, 0);
    uint8_t *frameBase = reinterpret_cast<uint8_t *>(frame.data());
    uint8_t *assetData = frameBase + binding.AssetDataOffset;

    if (!BuildAssetData(state, binding, packageName, assetName, assetData,
                        binding.AssetDataSize, detail)) {
        return nullptr;
    }

    const int32_t returnOffset = GetFunctionReturnValueOffset(state, binding.FnGetAsset);
    if (returnOffset < 0) {
        if (detail) *detail = "UAssetRegistryHelpers::GetAsset has no return value";
        return nullptr;
    }

    if (!CallProcessEvent(state, binding.HelpersCDO, binding.FnGetAsset, frameBase)) {
        if (detail) {
            *detail = "the engine's asset lookup raised an exception for '" +
                      packageName + "." + assetName + "'";
        }
        return nullptr;
    }

    UObject *asset = nullptr;
    SafeMem_Read(frameBase + returnOffset, &asset, sizeof(void *));
    if (!asset && detail) {
        *detail = "the registry answered nothing for '" + packageName + "." + assetName + "'";
    }
    return asset;
}

UObject *SpawnObjectThroughEngine(EngineState &state, UClass *objectClass, UObject *outer) {
    if (!objectClass || !outer) return nullptr;

    UObject *created = nullptr;
    const ReflectedArgument args[] = {
        {"ObjectClass", &objectClass, static_cast<int32_t>(sizeof(void *))},
        {"Outer", &outer, static_cast<int32_t>(sizeof(void *))},
    };
    if (!CallStaticFunctionByName(state, "GameplayStatics", "SpawnObject", args, 2, &created,
                                  sizeof(void *))) {
        return nullptr;
    }
    return created;
}

bool IsBlueprintClass(const EngineState &state, UClass *cls) {
    if (!cls) return false;
    if (GetObjectClass(state, reinterpret_cast<UObject *>(cls)) == nullptr) return false;

    return MetaClassChainContains(state, cls, "BlueprintGeneratedClass");
}

UClass *GetBlueprintGeneratedClassObject(EngineState &state) {
    return FindClassCached(state, "BlueprintGeneratedClass");
}

std::vector<UClass *> FindBlueprintClasses(EngineState &state,
                                           const std::string &superClassName) {
    std::vector<UClass *> results;

    UStruct *requiredSuper = nullptr;
    if (!superClassName.empty()) {
        requiredSuper = reinterpret_cast<UStruct *>(FindClassCached(state, superClassName));
        if (!requiredSuper) return results;
    }

    if (!state.UClassClass) return results;
    const std::vector<UObject *> classObjects =
        FindObjectsByClassFast(state, state.UClassClass);

    for (UObject *obj : classObjects) {
        if (!obj) continue;
        UClass *cls = reinterpret_cast<UClass *>(obj);
        if (!IsBlueprintClass(state, cls)) continue;
        if (requiredSuper &&
            !IsStructA(state, reinterpret_cast<UStruct *>(cls), requiredSuper)) {
            continue;
        }
        results.push_back(cls);
    }
    return results;
}

}
