#include "SDKGenerator_Impl.h"
#include "SDKGenerator_Bitfields.h"
#include "SDKGenerator_LayoutRules.h"
#include "../core/ObjectUtils.h"
#include <windows.h>
namespace SDKGenerator {

    uint32_t GetEnumUnderlyingBytesCached(const EngineState& state, void* enumObj) {
        if (!enumObj) return 0;

        GenerationCaches& caches = GetGenerationCaches();
        auto cached = caches.EnumUnderlyingBytes.find(enumObj);
        if (cached != caches.EnumUnderlyingBytes.end()) return cached->second;

        EnumInfo info;
        uint32_t bytes = 0;
        if (ReadEnumValues(state, enumObj, info) && !info.values.empty()) {

            const EnumValueRange range = ComputeEnumValueRange(info.values, info.name);
            bytes = EnumUnderlyingBytes(range.Min, range.Max);
        }

        caches.EnumUnderlyingBytes.emplace(enumObj, bytes);
        return bytes;
    }

    int32_t GetObservedEnumBytes(const EngineState& state, void* enumObj) {
        if (!enumObj) return 0;
        const auto& caches = GetGenerationCaches();
        auto it = caches.EnumPropertyBytes.find(enumObj);
        return (it == caches.EnumPropertyBytes.end()) ? 0 : it->second;
    }

    int32_t ReadStructPropertySize(const EngineState& state, void* structObj) {
        const int32_t off = EngineDiscovery::UStructPropertiesSizeOffset(state);
        if (!structObj || off < 0) return 0;
        int32_t size = 0;
        SafeMem_Read((uint8_t*)structObj + off, &size, 4);
        return size;
    }

    int32_t ReadStructAlignment(const EngineState& state, void* structObj) {
        if (!structObj || state.UStruct_MinAlignmentOff < 0) return 0;

        uint16_t value = 0;
        SafeMem_Read((uint8_t*)structObj + state.UStruct_MinAlignmentOff, &value, 2);
        return IsPlausibleStructAlignment(value) ? static_cast<int32_t>(value) : 0;
    }

    OwnPropertyRange ReadOwnPropertyRange(const EngineState& state, void* classObj) {
        OwnPropertyRange range;
        const auto note = [&range](int32_t offset) {
            if (offset <= 0) return;
            if (range.FirstOffset < 0 || offset < range.FirstOffset) range.FirstOffset = offset;
            if (offset > range.LastOffset) range.LastOffset = offset;
        };

        if (state.bUseFProperty && state.UStruct_ChildPropsOff >= 0) {
            const int offsetOff = state.FProperty_OffsetOff >= 0 ? state.FProperty_OffsetOff : 0x4C;
            for (void* child = EngineDiscovery::GetChildPropsHead(state, (UStruct*)classObj);
                 child; child = EngineDiscovery::GetFFieldNext(state, child)) {
                int32_t offset = 0;
                SafeMem_Read((uint8_t*)child + offsetOff, &offset, 4);
                note(offset);
            }
            return range;
        }

        const int offsetOff = state.UProperty_OffsetOff >= 0 ? state.UProperty_OffsetOff : 0x44;
        for (void* child = EngineDiscovery::GetChildrenHead(state, (UStruct*)classObj);
             child; child = EngineDiscovery::GetFieldNext(state, child)) {
            UClass* propCls = EngineDiscovery::GetObjectClass(state, (UObject*)child);
            if (!propCls) continue;
            if (GetCachedObjectName(state, (UObject*)propCls).find("Property") == std::string::npos) continue;
            int32_t offset = 0;
            SafeMem_Read((uint8_t*)child + offsetOff, &offset, 4);
            note(offset);
        }
        return range;
    }

    int32_t ReadPropertyArrayDim(const EngineState& state, void* prop, const char* where) {
        if (!prop || state.FProperty_ArrayDimOff < 0) return 1;

        int32_t asInt32 = 1;
        uint8_t asByte = 1;
        SafeMem_Read((uint8_t*)prop + state.FProperty_ArrayDimOff, &asInt32, 4);
        SafeMem_Read((uint8_t*)prop + state.FProperty_ArrayDimOff, &asByte, 1);

        const ArrayDimRead read = SanitizeArrayDim(asInt32, asByte);
        if (read.Repaired) {
            static bool reported = false;
            if (!reported) {
                reported = true;
                SdkLog("[SDK] FProperty::ArrayDim at 0x%X reads 0x%X, whose upper bytes are not "
                       "part of a dimension (first seen in %s); reading the field as one byte "
                       "and using the low byte for the dimension.",
                       state.FProperty_ArrayDimOff, (uint32_t)asInt32, where ? where : "a property");
            }
        }
        return read.Value;
    }

    bool ReadEnumValues(const EngineState& state, void* enumObj, EnumInfo& info) {
        if (state.UEnum_NamesOff < 0) return false;
        info.name = GetCachedObjectName(state, (UObject*)enumObj);
        if (info.name.empty() || info.name[0] == '<') return false;

        if (state.bIsNewUE5Enum) {
            uintptr_t enumAddr = reinterpret_cast<uintptr_t>(enumObj);
            uintptr_t namesTagPtr, valuesTagPtr;
            int32_t num = 0;
            if (!SafeMem_Read((uint8_t*)enumAddr + state.UEnum_NamesOff, &namesTagPtr, 8)) return false;
            if (!SafeMem_Read((uint8_t*)enumAddr + state.UEnum_NamesOff + 8, &valuesTagPtr, 8)) return false;
            if (!SafeMem_Read((uint8_t*)enumAddr + state.UEnum_NamesOff + 16, &num, 4)) return false;
            if (num < 0 || num > 5000) return false;

            uintptr_t namesPtr = namesTagPtr & ~1ULL;
            uintptr_t valuesPtr = valuesTagPtr & ~1ULL;

            for (int i = 0; i < num; i++) {
                uint8_t nameBuf[16] = {0};
                if (!SafeMem_Read((uint8_t*)namesPtr + i * state.FName_Size, nameBuf, state.FName_Size)) break;
                FName fn;
                fn.ComparisonIndex = *(uint32_t*)nameBuf;
                fn.Number = state.FName_NumberOff >= 0 ? *(uint32_t*)(nameBuf + state.FName_NumberOff) : 0;
                int64_t val;
                if (!SafeMem_Read((uint8_t*)valuesPtr + i * sizeof(int64_t), &val, sizeof(int64_t))) break;
                std::string valueName = EngineDiscovery::FNameToStr(state, fn);
                if (valueName.empty() || valueName == "None") continue;
                size_t colonPos = valueName.find("::");
                if (colonPos != std::string::npos) valueName = valueName.substr(colonPos + 2);
                info.values.push_back({valueName, val});
            }
            return true;
        }

        uint8_t tarrayBuf[16];
        if (!SafeMem_Read((uint8_t*)enumObj + state.UEnum_NamesOff, tarrayBuf, 16)) return false;
        void* data = *(void**)tarrayBuf;
        int32_t num = *(int32_t*)(tarrayBuf + 8);
        if (num < 0 || num > 5000) return false;
        if (num > 0 && !data) return false;

        int32_t nameStride = (state.FName_Size + 7) & ~7;
        int32_t entryStride = nameStride + 8;

        for (int i = 0; i < num; i++) {
            uint8_t entryBuf[32] = {0};
            if (!SafeMem_Read((uint8_t*)data + i * entryStride, entryBuf, entryStride)) break;
            FName fn;
            fn.ComparisonIndex = *(uint32_t*)entryBuf;
            fn.Number = state.FName_NumberOff >= 0 ? *(uint32_t*)(entryBuf + state.FName_NumberOff) : 0;
            int64_t val = *(int64_t*)(entryBuf + nameStride);
            std::string valueName = EngineDiscovery::FNameToStr(state, fn);
            if (valueName.empty() || valueName == "None") continue;
            size_t colonPos = valueName.find("::");
            if (colonPos != std::string::npos) valueName = valueName.substr(colonPos + 2);
            info.values.push_back({valueName, val});
        }
        return true;
    }

    std::string RenderFieldDeclaration(const SDKProperty& prop, const std::string& name) {
        std::string rendered = prop.Type;
        rendered += " ";
        rendered += name;
        if (prop.ArrayDim > 1) {
            rendered += "[";
            rendered += std::to_string(prop.ArrayDim);
            rendered += "]";
        }
        return rendered;
    }

    std::string RenderFieldDeclaration(const SDKProperty& prop) {
        return RenderFieldDeclaration(prop, prop.Name);
    }

    std::string RenderFunctionArgument(const SDKProperty& prop) {

        std::string rendered;
        if (prop.IsOutParam || prop.IsReferenceParam) {
            rendered += prop.Type;
            rendered += "& ";
            rendered += prop.Name;
        } else {
            rendered += prop.Type;
            rendered += " ";
            rendered += prop.Name;
        }
        return rendered;
    }

    uint64_t BuildPropertyFingerprint(const SDKProperty& prop) {
        if (prop.Fingerprint != 0) return prop.Fingerprint;
        uint64_t hash = 1469598103934665603ULL;
        hash = HashStringFNV1a(hash, prop.Type);
        hash = HashStringFNV1a(hash, prop.Name);
        hash = HashUInt64FNV1a(hash, static_cast<uint64_t>(prop.Offset));
        hash = HashUInt64FNV1a(hash, static_cast<uint64_t>(prop.Size));
        hash = HashUInt64FNV1a(hash, static_cast<uint64_t>(prop.ElementSize));
        hash = HashUInt64FNV1a(hash, static_cast<uint64_t>(prop.ArrayDim));
        hash = HashUInt64FNV1a(hash, prop.Flags);
        return hash;
    }

    uint64_t BuildFunctionFingerprint(const SDKFunction& fn) {
        if (fn.Fingerprint != 0) return fn.Fingerprint;
        uint64_t hash = 1469598103934665603ULL;
        hash = HashStringFNV1a(hash, fn.Name);
        hash = HashStringFNV1a(hash, fn.FullPath);
        hash = HashStringFNV1a(hash, fn.ReturnType);
        hash = HashUInt64FNV1a(hash, fn.FunctionFlags);
        hash = HashUInt64FNV1a(hash, static_cast<uint64_t>(fn.ParameterSize));
        for (const SDKProperty& param : fn.Params)
            hash = HashUInt64FNV1a(hash, param.Fingerprint != 0 ? param.Fingerprint : BuildPropertyFingerprint(param));
        return hash;
    }

    std::string MakeReflectionId(const char* prefix, const std::string& primary, const std::string& secondary) {
        std::string id = prefix;
        id += ":";
        id += primary;
        if (!secondary.empty()) {
            id += ":";
            id += secondary;
        }
        return id;
    }

    std::string ResolveNestedPropertyType(const EngineState& state, void* prop) {
        if (!prop) return "";
        std::string nestedRawType = GetCachedFFieldTypeName(state, prop);
        if (nestedRawType.empty()) return "";
        return PropertyTypeToCpp(state, prop, nestedRawType);
    }

    void PopulatePropertyTraits(const EngineState& state, void* propObj, const std::string& rawType, SDKProperty& prop) {
        prop.RawType = rawType;
        prop.FlagsText = DescribePropertyFlagsCached(prop.Flags);
        prop.Fingerprint = BuildPropertyFingerprint(prop);
        prop.SignatureHash = FormatHashHex(prop.Fingerprint);

        if (state.bUseFProperty && propObj) {
            if (rawType == "ArrayProperty" && state.FArrayProperty_InnerOff >= 0) {
                prop.InnerType = ResolveNestedPropertyType(state, ReadPtrAt(propObj, state.FArrayProperty_InnerOff));
            } else if (rawType == "SetProperty" && state.FSetProperty_ElementPropOff >= 0) {
                prop.InnerType = ResolveNestedPropertyType(state, ReadPtrAt(propObj, state.FSetProperty_ElementPropOff));
            } else if (rawType == "OptionalProperty" && state.FOptionalProperty_ValuePropOff >= 0) {
                prop.InnerType = ResolveNestedPropertyType(state, ReadPtrAt(propObj, state.FOptionalProperty_ValuePropOff));
            } else if (rawType == "MapProperty") {
                if (state.FMapProperty_KeyPropOff >= 0) {
                    prop.KeyType = ResolveNestedPropertyType(state, ReadPtrAt(propObj, state.FMapProperty_KeyPropOff));
                }
                if (state.FMapProperty_ValuePropOff >= 0) {
                    prop.ValueType = ResolveNestedPropertyType(state, ReadPtrAt(propObj, state.FMapProperty_ValuePropOff));
                }
            }

            if (rawType == "StructProperty" && state.FStructProperty_StructOff >= 0) {
                if (void* structPtr = ReadPtrAt(propObj, state.FStructProperty_StructOff)) {
                    std::string structName = GetCachedCleanObjectName(state, (UObject*)structPtr);
                    if (!structName.empty()) prop.StructType = "F" + structName;
                }
            }

            if (rawType == "EnumProperty" && state.FEnumProperty_EnumOff >= 0) {
                if (void* enumPtr = ReadPtrAt(propObj, state.FEnumProperty_EnumOff)) {
                    std::string enumName = GetCachedCleanObjectName(state, (UObject*)enumPtr);
                    if (!enumName.empty()) prop.EnumType = MakeEnumCppName(enumName);
                }
            } else if (rawType == "ByteProperty" && state.FByteProperty_EnumOff >= 0) {
                if (void* enumPtr = ReadPtrAt(propObj, state.FByteProperty_EnumOff)) {
                    std::string enumName = GetCachedCleanObjectName(state, (UObject*)enumPtr);
                    if (!enumName.empty()) prop.EnumType = MakeEnumCppName(enumName);
                }
            }

            auto resolveObjectClassType = [&](int32_t offset) {
                if (offset < 0) return std::string();
                void* classPtr = ReadPtrAt(propObj, offset);
                if (!classPtr) return std::string();
                std::string className = GetCachedCleanObjectName(state, (UObject*)classPtr);
                if (className.empty()) return std::string();
                return std::string(IsActorDerived(state, classPtr) ? "A" : "U") + className;
            };

            if (rawType == "ObjectProperty" || rawType == "ObjectPtrProperty") {
                prop.ObjectClassType = resolveObjectClassType(state.FObjectProperty_ClassOff);
            } else if (rawType == "ClassProperty" || rawType == "ClassPtrProperty") {
                prop.ObjectClassType = resolveObjectClassType(state.FClassProperty_MetaClassOff);
            }
        }

        prop.IsArray = (rawType == "ArrayProperty");
        prop.IsMap = (rawType == "MapProperty");
        prop.IsSet = (rawType == "SetProperty");
        prop.IsStruct = (rawType == "StructProperty");
        prop.IsEnum = (rawType == "EnumProperty" || (rawType == "ByteProperty" && !prop.EnumType.empty()));
        prop.IsObject = (rawType == "ObjectProperty" || rawType == "ObjectPtrProperty" ||
                         rawType == "ClassProperty" || rawType == "ClassPtrProperty" ||
                         rawType == "SoftObjectProperty" || rawType == "SoftClassProperty" ||
                         rawType == "WeakObjectProperty" || rawType == "LazyObjectProperty");
        prop.IsBool = (rawType == "BoolProperty");
        prop.IsString = (rawType == "StrProperty" || rawType == "NameProperty" ||
                         rawType == "TextProperty" || rawType == "Utf8StrProperty" ||
                         rawType == "AnsiStrProperty");
        prop.IsInt = (rawType == "IntProperty" || rawType == "Int8Property" || rawType == "Int16Property" ||
                      rawType == "Int64Property" || rawType == "UInt16Property" || rawType == "UInt32Property" ||
                      rawType == "UInt64Property" || (rawType == "ByteProperty" && prop.EnumType.empty()));
        prop.IsFloat = (rawType == "FloatProperty" || rawType == "DoubleProperty" || rawType == "LargeWorldCoordinatesRealProperty");
        prop.IsNumeric = prop.IsInt || prop.IsFloat;
        prop.IsDelegate = (rawType == "DelegateProperty" || rawType == "MulticastDelegateProperty" ||
                           rawType == "MulticastInlineDelegateProperty" || rawType == "MulticastSparseDelegateProperty");
        prop.IsInterface = (rawType == "InterfaceProperty");
    }

    ReflectionPropertyRecord MakeReflectionPropertyRecord(
        const SDKProperty& prop,
        const std::string& ownerId,
        const std::string& ownerKind,
        const std::string& ownerPackageName,
        const std::string& ownerName,
        const std::string& ownerCppName,
        const std::string& role) {

        ReflectionPropertyRecord record;
        record.Id = MakeReflectionId("property", ownerId, prop.Name + "@" + std::to_string(prop.Offset));
        record.OwnerId = ownerId;
        record.OwnerKind = ownerKind;
        record.OwnerPackageName = ownerPackageName;
        record.OwnerName = ownerName;
        record.OwnerCppName = ownerCppName;
        record.Name = prop.Name;
        record.CppType = prop.Type;
        record.RawType = prop.RawType;
        record.InnerType = prop.InnerType;
        record.KeyType = prop.KeyType;
        record.ValueType = prop.ValueType;
        record.StructType = prop.StructType;
        record.EnumType = prop.EnumType;
        record.ObjectClassType = prop.ObjectClassType;
        record.FlagsText = prop.FlagsText;
        record.Role = role;
        record.SignatureHash = prop.SignatureHash;
        record.Offset = prop.Offset;
        record.Size = prop.Size;
        record.ElementSize = prop.ElementSize;
        record.ArrayDim = prop.ArrayDim;
        record.Flags = prop.Flags;
        record.IsArray = prop.IsArray;
        record.IsMap = prop.IsMap;
        record.IsSet = prop.IsSet;
        record.IsStruct = prop.IsStruct;
        record.IsEnum = prop.IsEnum;
        record.IsObject = prop.IsObject;
        record.IsBitfield = prop.IsBitfield;
        record.BitIndex = prop.BitIndex;
        record.FieldMask = prop.FieldMask;
        record.IsBool = prop.IsBool;
        record.IsString = prop.IsString;
        record.IsNumeric = prop.IsNumeric;
        record.IsInt = prop.IsInt;
        record.IsFloat = prop.IsFloat;
        record.IsDelegate = prop.IsDelegate;
        record.IsInterface = prop.IsInterface;
        record.IsReturnParam = prop.IsReturnParam;
        record.IsOutParam = prop.IsOutParam;
        record.IsReferenceParam = prop.IsReferenceParam;
        record.IsConstParam = prop.IsConstParam;
        return record;
    }

    std::vector<SDKProperty> ReadFunctionParams(const EngineState& state, void* funcObj) {
        std::vector<SDKProperty> params;
        params.reserve(8);

        if (state.bUseFProperty && state.UStruct_ChildPropsOff >= 0) {
            void* child = EngineDiscovery::GetChildPropsHead(state, (UStruct*)funcObj);
            while (child) {
                std::string pName = CleanClassName(GetCachedFFieldName(state, child));
                std::string pType = GetCachedFFieldTypeName(state, child);

                int offset = 0;
                int offsetOff = state.FProperty_OffsetOff >= 0 ? state.FProperty_OffsetOff : 0x4C;
                SafeMem_Read((uint8_t*)child + offsetOff, &offset, 4);

                int elementSize = 0;
                int sizeOff = state.FProperty_ElementSizeOff >= 0 ? state.FProperty_ElementSizeOff : 0x3C;
                SafeMem_Read((uint8_t*)child + sizeOff, &elementSize, 4);

                uint64_t propFlags = 0;
                if (state.FProperty_PropertyFlagsOff >= 0)
                    SafeMem_Read((uint8_t*)child + state.FProperty_PropertyFlagsOff, &propFlags, 8);

                const bool isLocal = state.FProperty_PropertyFlagsOff >= 0 && (propFlags & CPF_Parm) == 0;

                int arrayDim = ReadPropertyArrayDim(state, child, "the parameters of a function");
                arrayDim = std::max(arrayDim, 1);

                std::string cppType = PropertyTypeToCpp(state, child, pType, elementSize);
                if (elementSize > 0) {
                    if (pType == "TextProperty") GetGenerationCaches().FTextSize = elementSize;
                    else if (pType == "LazyObjectProperty") GetGenerationCaches().LazyObjectSize = elementSize;
                }

                if (pName.empty()) {
                    pName = "Param_" + std::to_string(offset);
                }

                SDKProperty p;
                p.Type = cppType;
                p.Name = pName;
                p.Offset = offset;
                p.Size = std::max(elementSize * arrayDim, elementSize);
                p.ElementSize = elementSize;
                p.ArrayDim = arrayDim;
                p.Flags = propFlags;
                if (pType == "BoolProperty") {
                    ExtractBoolPropertyBitfields(state, child, p);
                } else {
                    p.IsBitfield = false;
                }
                p.IsLocal = isLocal;
                p.IsReturnParam = (propFlags & CPF_ReturnParm) != 0;
                p.IsOutParam = (propFlags & CPF_OutParm) != 0;
                p.IsReferenceParam = (propFlags & CPF_ReferenceParm) != 0;
                p.IsConstParam = (propFlags & CPF_ConstParm) != 0;
                PopulatePropertyTraits(state, child, pType, p);
                params.push_back(p);

                child = EngineDiscovery::GetFFieldNext(state, child);
            }
        }

        std::set<std::string> usedParamNames;
        usedParamNames.insert("Parms");
        usedParamNames.insert("Func");
        usedParamNames.insert("Clss");
        usedParamNames.insert("Target");
        for (auto& p : params) {
            p.Name = MakeUniqueName(p.Name, usedParamNames);
        }
        return params;
    }

    std::vector<SDKProperty> CollectProperties(const EngineState& state, void* obj, int baseLayoutEnd) {
        std::vector<SDKProperty> props;
        props.reserve(16);

        if (state.bUseFProperty && state.UStruct_ChildPropsOff >= 0 && state.FField_NameOff >= 0) {
            void* child = EngineDiscovery::GetChildPropsHead(state, (UStruct*)obj);
            while (child) {
                std::string pName = CleanClassName(GetCachedFFieldName(state, child));
                std::string pType = GetCachedFFieldTypeName(state, child);

                int offset = 0;
                int offsetOff = state.FProperty_OffsetOff >= 0 ? state.FProperty_OffsetOff : 0x4C;
                SafeMem_Read((uint8_t*)child + offsetOff, &offset, 4);

                int elementSize = 0;
                int sizeOff = state.FProperty_ElementSizeOff >= 0 ? state.FProperty_ElementSizeOff : 0x3C;
                SafeMem_Read((uint8_t*)child + sizeOff, &elementSize, 4);

                int arrayDim = ReadPropertyArrayDim(state, child, "a member of a struct or class");
                arrayDim = std::max(arrayDim, 1);

                uint64_t propFlags = 0;
                if (state.FProperty_PropertyFlagsOff >= 0)
                    SafeMem_Read((uint8_t*)child + state.FProperty_PropertyFlagsOff, &propFlags, 8);

                std::string cppType = PropertyTypeToCpp(state, child, pType, elementSize);
                if (elementSize > 0) {
                    if (pType == "TextProperty") GetGenerationCaches().FTextSize = elementSize;
                    else if (pType == "LazyObjectProperty") GetGenerationCaches().LazyObjectSize = elementSize;
                }

                if (offset >= baseLayoutEnd && pName.length() > 0) {
                    SDKProperty prop;
                    prop.Type = cppType;
                    prop.Name = pName;
                    prop.Offset = offset;
                    prop.Size = std::max(elementSize * arrayDim, elementSize);
                    prop.ElementSize = elementSize;
                    prop.ArrayDim = arrayDim;
                    prop.Flags = propFlags;
                    if (pType == "BoolProperty") {
                        ExtractBoolPropertyBitfields(state, child, prop);
                    } else {
                        prop.IsBitfield = false;
                    }
                    PopulatePropertyTraits(state, child, pType, prop);
                    props.push_back(prop);
                }
                child = EngineDiscovery::GetFFieldNext(state, child);
            }
        } else {

            void* child = EngineDiscovery::GetChildrenHead(state, (UStruct*)obj);
            while (child) {
                UClass* propCls = EngineDiscovery::GetObjectClass(state, (UObject*)child);
                if (propCls) {
                    std::string clsName = GetCachedObjectName(state, (UObject*)propCls);
                    if (clsName.find("Property") != std::string::npos) {
                        std::string pName = GetCachedCleanObjectName(state, (UObject*)child);
                        int offset = 0;
                        int offsetOff = state.UProperty_OffsetOff >= 0 ? state.UProperty_OffsetOff : 0x44;
                        SafeMem_Read((uint8_t*)child + offsetOff, &offset, 4);
                        std::string cppType = PropertyTypeToCpp(state, child, clsName);
                        if (offset >= baseLayoutEnd && pName.length() > 0) {
                            SDKProperty prop;
                            prop.Type = cppType;
                            prop.Name = pName;
                            prop.Offset = offset;
                            prop.Size = 4;
                            prop.ElementSize = 4;
                            prop.ArrayDim = 1;
                            prop.Flags = 0;
                            if (clsName == "BoolProperty") {
                                ExtractBoolPropertyBitfields(state, child, prop);
                            } else {
                                prop.IsBitfield = false;
                            }
                            PopulatePropertyTraits(state, child, clsName, prop);
                            props.push_back(prop);
                        }
                    }
                }
                child = EngineDiscovery::GetFieldNext(state, child);
            }
        }

        SortPropertiesForLayout(props);
        return props;
    }

    void WriteMembers(std::ostringstream& out, const std::vector<SDKProperty>& props, int superSize, int structSize,
                      const std::string& scopeName, std::vector<int32_t>* emittedOffsets) {
        WriteMembersAccurate(out, props, superSize, structSize, scopeName, emittedOffsets);
    }

    std::vector<SDKFunction> CollectFunctions(const EngineState& state, void* classObj,
                                                      const std::string& packageName, const std::string& objectName) {
        std::vector<SDKFunction> funcs;
        funcs.reserve(16);
        void* child = EngineDiscovery::GetChildrenHead(state, (UStruct*)classObj);
        while (child) {
            UClass* cCls = EngineDiscovery::GetObjectClass(state, (UObject*)child);
            if (cCls) {
                if (IsFunction(state, cCls)) {
                    std::string funcName = GetCachedCleanObjectName(state, (UObject*)child);
                    if (!funcName.empty()) {
                        SDKFunction fn;
                        fn.Name = funcName;
                        fn.RawName = GetCachedObjectName(state, (UObject*)child);
                        fn.FullPath = GetCachedFullName(state, (UObject*)child);
                        fn.Params = ReadFunctionParams(state, child);

                        fn.ReturnType = "void";
                        fn.ReturnName = "";

                        fn.ParameterSize = ReadStructPropertySize(state, child);

                        if (state.UFunction_FlagsOff >= 0)
                            SafeMem_Read((uint8_t*)child + state.UFunction_FlagsOff, &fn.FunctionFlags, 4);
                        fn.FlagsText = DescribeFunctionFlagsCached(fn.FunctionFlags);

                        for (auto& p : fn.Params) {
                            if (p.IsReturnParam || p.Name == "ReturnValue") {
                                fn.ReturnType = p.Type;
                                fn.ReturnName = p.Name;
                            }
                        }
                        fn.Fingerprint = BuildFunctionFingerprint(fn);
                        fn.SignatureHash = FormatHashHex(fn.Fingerprint);
                        funcs.push_back(fn);
                    }
                }
            }
            child = EngineDiscovery::GetFieldNext(state, child);
        }
        return funcs;
    }

}
