#include "USMapGenerator.h"
#include "../core/EngineDiscovery.h"
extern "C" {
#include "../utils/SafeMemory.h"
}
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <set>
#include <map>

static void UsmapLog(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (!EngineDiscovery::IsVerboseLoggingEnabled()) {
        if (strstr(buf, "[USMap] DEBUG:") != nullptr) {
            return;
        }
    }

    EngineDiscovery::WriteSdkLogLine(buf);
}

static void* ReadPtrAt(void* base, int32_t offset) {
    if (!base || offset < 0) return nullptr;
    return SafeMem_ReadPtr((uint8_t*)base + offset);
}

static int32_t ReadI32At(void* base, int32_t offset) {
    if (!base || offset < 0) return 0;
    int32_t val = 0;
    SafeMem_Read((uint8_t*)base + offset, &val, sizeof(val));
    return val;
}

static uint8_t ReadU8At(void* base, int32_t offset) {
    if (!base || offset < 0) return 0;
    uint8_t val = 0;
    SafeMem_Read((uint8_t*)base + offset, &val, 1);
    return val;
}

static void* GetPropertyOwner(const EngineState& state, void* prop) {
    if (!prop) return nullptr;
    if (state.bUseFProperty) {
        return ReadPtrAt(prop, 0x10);
    } else {
        return ReadPtrAt(prop, state.UObject_OuterOff);
    }
}

static UObject* GetOutermost(const EngineState& state, UObject* obj) {
    UObject* outermost = obj;
    UObject* outer = EngineDiscovery::GetObjectOuter(state, obj);
    while (outer) {
        outermost = outer;
        outer = EngineDiscovery::GetObjectOuter(state, outer);
    }
    return outermost;
}

static std::string GetPackagePath(const EngineState& state, UObject* obj) {
    UObject* package = GetOutermost(state, obj);
    if (!package) return "";

    UClass* pkgClass = EngineDiscovery::GetObjectClass(state, package);
    if (!pkgClass) return "";

    std::string pkgClassName = EngineDiscovery::GetObjectNameStr(state, pkgClass);
    if (pkgClassName != "Package") return "";

    return EngineDiscovery::GetObjectNameStr(state, package);
}

namespace USMapDiscovery {

static bool DiscoverUEnumNamesOffset(const EngineState& state) {
    if (state.UEnum_NamesOff >= 0) return true;
    if (!state.UEnumClass) return false;

    int32_t count = EngineDiscovery::GetObjectCount(state);
    for (int i = 0; i < std::min(count, 50000); i++) {
        UObject* obj = EngineDiscovery::GetObjectByIndex(state, i);
        if (!obj) continue;

        UClass* cls = EngineDiscovery::GetObjectClass(state, obj);
        if (!cls) continue;

        std::string clsName = EngineDiscovery::GetObjectNameStr(state, cls);
        if (clsName != "Enum" && clsName != "UserDefinedEnum") continue;

        uint8_t enumBuf[0x100];
        if (!SafeMem_Read(obj, enumBuf, sizeof(enumBuf))) continue;

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
                            valid = false;
                            break;
                        }
                        FName fn;
                        fn.ComparisonIndex = *(uint32_t*)nameBuf;
                        fn.Number = state.FName_NumberOff >= 0 ? *(uint32_t*)(nameBuf + state.FName_NumberOff) : 0;
                        std::string name = EngineDiscovery::FNameToStr(state, fn);
                        if (name.empty() || name[0] == '<') {
                            valid = false;
                            break;
                        }
                        validNames++;
                    }
                    if (valid && validNames >= 2) {
                        const_cast<EngineState&>(state).UEnum_NamesOff = off + 8;

                        const_cast<EngineState&>(state).bIsNewUE5Enum = true;
                        const_cast<EngineState&>(state).UEnum_NamesOff = off;
                        UsmapLog("[USMap] Discovered UEnum::Names (split layout) at 0x%X (num=%d)", off, num);
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
                    valid = false;
                    break;
                }
                FName fn;
                fn.ComparisonIndex = *(uint32_t*)tmpBuf;
                fn.Number = state.FName_NumberOff >= 0 ? *(uint32_t*)(tmpBuf + state.FName_NumberOff) : 0;

                std::string name = EngineDiscovery::FNameToStr(state, fn);
                if (name.empty() || name[0] == '<') {
                    valid = false;
                    break;
                }
                validNames++;
            }

            if (valid && validNames >= 2) {
                const_cast<EngineState&>(state).bIsNewUE5Enum = false;
                const_cast<EngineState&>(state).UEnum_NamesOff = off;
                UsmapLog("[USMap] Discovered UEnum::Names at offset 0x%X (num=%d)", off, num);
                return true;
            }
        }
    }

    UsmapLog("[USMap] Failed to discover UEnum::Names offset");
    return false;
}

void DiscoverPropertySubTypeOffsets(EngineState& state) {
    if (!state.bCoreClassesFound || state.UStruct_ChildPropsOff < 0) return;

    int32_t propBaseEnd = (state.FProperty_OffsetOff == 0x44) ? 0x60 : 0x68;
    if (state.FBoolProperty_BaseOff > 0) {

        propBaseEnd = state.FBoolProperty_BaseOff;
    }

    struct ClassToScan {
        void* classPtr;
        const char* name;
    };

    std::vector<ClassToScan> classes;
    if (state.AActorClass) classes.push_back({state.AActorClass, "Actor"});
    if (state.UObjectClass) classes.push_back({state.UObjectClass, "Object"});

    void* colorStruct = nullptr;
    void* guidStruct = nullptr;
    int32_t objCount = EngineDiscovery::GetObjectCount(state);
    for (int i = 0; i < std::min(objCount, 20000) && (classes.size() < 20 || !colorStruct || !guidStruct); i++) {
        UObject* obj = EngineDiscovery::GetObjectByIndex(state, i);
        if (!obj) continue;
        UClass* cls = EngineDiscovery::GetObjectClass(state, obj);
        if (!cls) continue;

        std::string clsName = EngineDiscovery::GetObjectNameStr(state, cls);

        if (cls == state.UClassClass) {
            std::string name = EngineDiscovery::GetObjectNameStr(state, obj);
            if (name == "PlayerController" || name == "GameMode" || name == "GameState" ||
                name == "Character" || name == "PlayerState" || name == "HUD" ||
                name == "GameInstance" || name == "DataTable" || name == "Pawn" ||
                name == "StaticMesh" || name == "GameViewportClient" ||
                name == "UserDefinedEnum" || name == "ActorComponent") {
                classes.push_back({obj, name.c_str()});
            }
        }

        if (clsName == "ScriptStruct") {
            std::string sName = EngineDiscovery::GetObjectNameStr(state, obj);
            if (sName == "Color" && !colorStruct) colorStruct = obj;
            if (sName == "Guid" && !guidStruct) guidStruct = obj;
        }
    }

    if (state.UStruct_PropertiesSizeOff < 0 && colorStruct && guidStruct) {
        uint8_t colorBuf[0x80] = {};
        uint8_t guidBuf[0x80] = {};
        if (SafeMem_Read(colorStruct, colorBuf, sizeof(colorBuf)) &&
            SafeMem_Read(guidStruct, guidBuf, sizeof(guidBuf))) {
            for (int off = 0x30; off <= 0x70; off += 4) {
                int32_t colorSize = *(int32_t*)(colorBuf + off);
                int32_t guidSize = *(int32_t*)(guidBuf + off);
                if (colorSize == 4 && guidSize == 16) {
                    state.UStruct_PropertiesSizeOff = off;
                    UsmapLog("[Discovery] Found UStruct::PropertiesSize at offset 0x%X (100%% confidence, verified via FColor=4, FGuid=16)", off);
                    break;
                }
            }
        }
    }
    if (colorStruct || guidStruct) {
        struct KnownOffset { void* structPtr; const char* memberName; int32_t expectedOffset; };
        std::vector<KnownOffset> knownMembers;

        if (colorStruct) {
            knownMembers.push_back({colorStruct, "B", 0x00});
            knownMembers.push_back({colorStruct, "G", 0x01});
        }
        if (guidStruct) {
            knownMembers.push_back({guidStruct, "C", 0x08});
            knownMembers.push_back({guidStruct, "D", 0x0C});
        }

        std::map<int32_t, int> offsetVotes;
        for (auto& km : knownMembers) {
            void* prop = EngineDiscovery::GetChildPropsHead(state, (UStruct*)km.structPtr);
            while (prop) {
                std::string pName = EngineDiscovery::GetFFieldName(state, prop);

                bool nameMatch = (pName == km.memberName);
                if (!nameMatch && pName.size() == 1) {
                    char upper = pName[0] >= 'a' ? (pName[0] - 32) : pName[0];
                    char expected = km.memberName[0];
                    nameMatch = (upper == expected);
                }

                if (nameMatch) {
                    uint8_t buf[0x60];
                    if (SafeMem_Read(prop, buf, sizeof(buf))) {

                        for (int off = 0x38; off <= 0x58; off += 4) {
                            int32_t val = *(int32_t*)(buf + off);
                            if (val == km.expectedOffset) {
                                offsetVotes[off]++;
                            }
                        }
                    }
                    break;
                }
                prop = EngineDiscovery::GetFFieldNext(state, prop);
            }
        }

        int bestVotes = 0;
        int32_t bestOffset = -1;
        for (auto& [off, votes] : offsetVotes) {
            if (votes > bestVotes) {
                bestVotes = votes;
                bestOffset = off;
            }
        }

        if (bestOffset >= 0 && bestVotes >= 2) {
            const int totalChecks = static_cast<int>(knownMembers.size());
            if (bestOffset != state.FProperty_OffsetOff) {
                if (bestVotes == totalChecks) {
                    UsmapLog("[Discovery] Adjusted FProperty::Offset_Internal from 0x%X to 0x%X (100%% confidence, %d/4 verified)",
                             state.FProperty_OffsetOff, bestOffset, bestVotes);
                } else {
                    UsmapLog("[Discovery] Adjusted FProperty::Offset_Internal from 0x%X to 0x%X (votes=%d/%d)",
                             state.FProperty_OffsetOff, bestOffset, bestVotes, totalChecks);
                }
                state.FProperty_OffsetOff = bestOffset;
            } else {
                if (bestVotes == totalChecks) {
                    UsmapLog("[Discovery] Confirmed FProperty::Offset_Internal at 0x%X (100%% confidence, %d/4 verified)",
                             bestOffset, bestVotes);
                } else {
                    UsmapLog("[Discovery] Confirmed FProperty::Offset_Internal at 0x%X (votes=%d/%d)",
                             bestOffset, bestVotes, totalChecks);
                }
            }
        } else {
            UsmapLog("[Discovery] Could not confirm FProperty::Offset_Internal (bestOff=0x%X, votes=%d), keeping 0x%X",
                     bestOffset, bestVotes, state.FProperty_OffsetOff);
        }
    }

    for (auto& cs : classes) {
        void* prop = EngineDiscovery::GetChildPropsHead(state, (UStruct*)cs.classPtr);
        while (prop) {
            std::string typeName = EngineDiscovery::GetFFieldTypeName(state, prop);

            uint8_t propBuf[0x100];
            if (!SafeMem_Read(prop, propBuf, sizeof(propBuf))) {
                prop = EngineDiscovery::GetFFieldNext(state, prop);
                continue;
            }

            if (typeName == "ArrayProperty" && state.FArrayProperty_InnerOff < 0) {

                for (int off = propBaseEnd; off <= 0x90; off += 8) {
                    void* inner = *(void**)(propBuf + off);
                    if (!inner) continue;
                    uintptr_t addr = reinterpret_cast<uintptr_t>(inner);
                    if (addr < 0x10000 || addr >= 0x7FF000000000ULL) continue;

                    std::string innerType = EngineDiscovery::GetFFieldTypeName(state, inner);
                    if (innerType.find("Property") != std::string::npos) {

                        void* innerOwner = GetPropertyOwner(state, inner);
                        if (innerOwner == prop) {
                            state.FArrayProperty_InnerOff = off;
                            UsmapLog("[USMap] FArrayProperty::Inner at 0x%X", off);
                            break;
                        }
                    }
                }
            }

            if (typeName == "StructProperty" && state.FStructProperty_StructOff < 0) {
                for (int off = propBaseEnd; off <= 0x90; off += 8) {
                    void* structPtr = *(void**)(propBuf + off);
                    if (!structPtr) continue;
                    uintptr_t addr = reinterpret_cast<uintptr_t>(structPtr);
                    if (addr < 0x10000 || addr >= 0x7FF000000000ULL) continue;

                    UClass* structCls = EngineDiscovery::GetObjectClass(state, (UObject*)structPtr);
                    if (!structCls) continue;
                    std::string structClsName = EngineDiscovery::GetObjectNameStr(state, structCls);

                    std::string sName = EngineDiscovery::GetObjectNameStr(state, (UObject*)structPtr);
                    if (sName == "Vector" || sName == "Rotator" || sName == "Guid" || sName == "Color" || sName == "Transform" ||
                        structClsName == "ScriptStruct" || structClsName == "Struct" || structClsName == "Class") {
                        if (sName.empty() || sName[0] == '<') continue;

                        state.FStructProperty_StructOff = off;
                        UsmapLog("[USMap] FStructProperty::Struct at 0x%X (-> %s)", off, sName.c_str());
                        break;
                    }
                }
            }

            if ((typeName == "ObjectProperty" || typeName == "ObjectPtrProperty") && state.FObjectProperty_ClassOff < 0) {
                for (int off = propBaseEnd; off <= 0x90; off += 8) {
                    void* classPtr = *(void**)(propBuf + off);
                    if (!classPtr) continue;
                    uintptr_t addr = reinterpret_cast<uintptr_t>(classPtr);
                    if (addr < 0x10000 || addr >= 0x7FF000000000ULL) continue;

                    UClass* pcCls = EngineDiscovery::GetObjectClass(state, (UObject*)classPtr);
                    if (pcCls == state.UClassClass) {
                        state.FObjectProperty_ClassOff = off;
                        std::string cName = EngineDiscovery::GetObjectNameStr(state, (UObject*)classPtr);
                        UsmapLog("[USMap] FObjectProperty::PropertyClass at 0x%X (-> %s)", off, cName.c_str());
                        break;
                    }
                }
            }

            if (typeName == "EnumProperty" && state.FEnumProperty_UnderlyingOff < 0) {
                for (int off = propBaseEnd; off <= 0x90; off += 8) {
                    void* ptr1 = *(void**)(propBuf + off);
                    if (!ptr1) continue;
                    uintptr_t addr1 = reinterpret_cast<uintptr_t>(ptr1);
                    if (addr1 < 0x10000 || addr1 >= 0x7FF000000000ULL) continue;

                    std::string p1Type = EngineDiscovery::GetFFieldTypeName(state, ptr1);
                    if (p1Type.find("Property") != std::string::npos) {

                        state.FEnumProperty_UnderlyingOff = off;
                        UsmapLog("[USMap] FEnumProperty::UnderlyingProp at 0x%X (%s)", off, p1Type.c_str());

                        if (off + 8 <= 0x90) {
                            void* enumPtr = *(void**)(propBuf + off + 8);
                            if (enumPtr) {
                                UClass* enumCls = EngineDiscovery::GetObjectClass(state, (UObject*)enumPtr);
                                if (enumCls) {
                                    std::string ecn = EngineDiscovery::GetObjectNameStr(state, enumCls);
                                    if (ecn == "Enum" || ecn == "UserDefinedEnum") {
                                        state.FEnumProperty_EnumOff = off + 8;
                                        std::string eName = EngineDiscovery::GetObjectNameStr(state, (UObject*)enumPtr);
                                        UsmapLog("[USMap] FEnumProperty::Enum at 0x%X (-> %s)", off + 8, eName.c_str());
                                    }
                                }
                            }
                        }
                        break;
                    }

                    UClass* ptr1Cls = EngineDiscovery::GetObjectClass(state, (UObject*)ptr1);
                    if (ptr1Cls) {
                        std::string clsN = EngineDiscovery::GetObjectNameStr(state, ptr1Cls);
                        if (clsN == "Enum" || clsN == "UserDefinedEnum") {
                            state.FEnumProperty_EnumOff = off;
                            UsmapLog("[USMap] FEnumProperty::Enum at 0x%X (found before underlying)", off);

                            if (off >= 8) {
                                void* underPtr = *(void**)(propBuf + off - 8);
                                if (underPtr) {
                                    std::string uType = EngineDiscovery::GetFFieldTypeName(state, underPtr);
                                    if (uType.find("Property") != std::string::npos) {
                                        state.FEnumProperty_UnderlyingOff = off - 8;
                                        UsmapLog("[USMap] FEnumProperty::UnderlyingProp at 0x%X", off - 8);
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }

            if (typeName == "MapProperty" && state.FMapProperty_KeyPropOff < 0) {
                for (int off = propBaseEnd; off <= 0x88; off += 8) {
                    void* key = *(void**)(propBuf + off);
                    void* val = *(void**)(propBuf + off + 8);
                    if (!key || !val) continue;
                    uintptr_t kAddr = reinterpret_cast<uintptr_t>(key);
                    uintptr_t vAddr = reinterpret_cast<uintptr_t>(val);
                    if (kAddr < 0x10000 || kAddr >= 0x7FF000000000ULL) continue;
                    if (vAddr < 0x10000 || vAddr >= 0x7FF000000000ULL) continue;

                    std::string kType = EngineDiscovery::GetFFieldTypeName(state, key);
                    std::string vType = EngineDiscovery::GetFFieldTypeName(state, val);
                    if (kType.find("Property") != std::string::npos &&
                        vType.find("Property") != std::string::npos) {

                        void* kOwner = GetPropertyOwner(state, key);
                        void* vOwner = GetPropertyOwner(state, val);
                        if (kOwner == prop && vOwner == prop) {
                            state.FMapProperty_KeyPropOff = off;
                            state.FMapProperty_ValuePropOff = off + 8;
                            UsmapLog("[USMap] FMapProperty Key=0x%X (%s) Value=0x%X (%s)",
                                off, kType.c_str(), off + 8, vType.c_str());
                            break;
                        }
                    }
                }
            }

            if (typeName == "SetProperty" && state.FSetProperty_ElementPropOff < 0) {
                for (int off = propBaseEnd; off <= 0x90; off += 8) {
                    void* elem = *(void**)(propBuf + off);
                    if (!elem) continue;
                    uintptr_t eAddr = reinterpret_cast<uintptr_t>(elem);
                    if (eAddr < 0x10000 || eAddr >= 0x7FF000000000ULL) continue;

                    std::string eType = EngineDiscovery::GetFFieldTypeName(state, elem);
                    if (eType.find("Property") != std::string::npos) {
                        void* eOwner = GetPropertyOwner(state, elem);
                        if (eOwner == prop) {
                            state.FSetProperty_ElementPropOff = off;
                            UsmapLog("[USMap] FSetProperty::ElementProp at 0x%X (%s)", off, eType.c_str());
                            break;
                        }
                    }
                }
            }

            if (typeName == "OptionalProperty" && state.FOptionalProperty_ValuePropOff < 0) {
                for (int off = propBaseEnd; off <= 0x90; off += 8) {
                    void* valProp = *(void**)(propBuf + off);
                    if (!valProp) continue;
                    uintptr_t vAddr = reinterpret_cast<uintptr_t>(valProp);
                    if (vAddr < 0x10000 || vAddr >= 0x7FF000000000ULL) continue;

                    std::string vType = EngineDiscovery::GetFFieldTypeName(state, valProp);
                    if (vType.find("Property") != std::string::npos) {
                        void* vOwner = GetPropertyOwner(state, valProp);
                        if (vOwner == prop) {
                            state.FOptionalProperty_ValuePropOff = off;
                            UsmapLog("[USMap] FOptionalProperty::ValueProperty at 0x%X (%s)", off, vType.c_str());
                            break;
                        }
                    }
                }
            }

            if (typeName == "ByteProperty" && state.FByteProperty_EnumOff < 0) {
                for (int off = propBaseEnd; off <= 0x90; off += 8) {
                    void* enumPtr = *(void**)(propBuf + off);
                    if (!enumPtr) continue;
                    uintptr_t addr = reinterpret_cast<uintptr_t>(enumPtr);
                    if (addr < 0x10000 || addr >= 0x7FF000000000ULL) continue;

                    UClass* eCls = EngineDiscovery::GetObjectClass(state, (UObject*)enumPtr);
                    if (eCls) {
                        std::string ecn = EngineDiscovery::GetObjectNameStr(state, eCls);
                        if (ecn == "Enum" || ecn == "UserDefinedEnum") {
                            state.FByteProperty_EnumOff = off;
                            std::string eName = EngineDiscovery::GetObjectNameStr(state, (UObject*)enumPtr);
                            UsmapLog("[USMap] FByteProperty::Enum at 0x%X (-> %s)", off, eName.c_str());
                            break;
                        }
                    }
                }
            }

            if (typeName == "ClassProperty" && state.FClassProperty_MetaClassOff < 0 && state.FObjectProperty_ClassOff >= 0) {

                for (int off = propBaseEnd; off <= 0x98; off += 8) {
                    if (off == state.FObjectProperty_ClassOff) continue;
                    void* metaPtr = *(void**)(propBuf + off);
                    if (!metaPtr) continue;
                    uintptr_t addr = reinterpret_cast<uintptr_t>(metaPtr);
                    if (addr < 0x10000 || addr >= 0x7FF000000000ULL) continue;

                    UClass* mcCls = EngineDiscovery::GetObjectClass(state, (UObject*)metaPtr);
                    if (mcCls == state.UClassClass) {
                        state.FClassProperty_MetaClassOff = off;
                        std::string mcName = EngineDiscovery::GetObjectNameStr(state, (UObject*)metaPtr);
                        UsmapLog("[USMap] FClassProperty::MetaClass at 0x%X (-> %s)", off, mcName.c_str());
                        break;
                    }
                }
            }

            if (typeName == "BoolProperty" && state.FBoolProperty_BaseOff < 0) {

                for (int off = 0x68; off <= 0x80; off++) {
                    uint8_t fieldSize  = propBuf[off];
                    uint8_t byteOffset = propBuf[off + 1];
                    uint8_t byteMask   = propBuf[off + 2];
                    uint8_t fieldMask  = propBuf[off + 3];

                    if (fieldSize >= 1 && fieldSize <= 8 &&
                        byteMask != 0 && fieldMask != 0 &&
                        (byteMask & (byteMask - 1)) == 0 &&
                        (fieldMask & (fieldMask - 1)) == 0) {
                        state.FBoolProperty_BaseOff = off;
                        UsmapLog("[USMap] FBoolProperty base at 0x%X (FieldSize=%d ByteOff=%d ByteMask=0x%X FieldMask=0x%X)",
                            off, fieldSize, byteOffset, byteMask, fieldMask);

                        propBaseEnd = off;
                        break;
                    }
                }
            }

            if (state.FProperty_ArrayDimOff < 0 &&
               (typeName == "IntProperty" || typeName == "FloatProperty" || typeName == "BoolProperty")) {
                int expectedSize = (typeName == "IntProperty" || typeName == "FloatProperty") ? 4 : 1;

                for (int off = 0x30; off <= 0x48; off += 4) {
                    int32_t dim = *(int32_t*)(propBuf + off);
                    int32_t elemSize = *(int32_t*)(propBuf + off + 4);

                    if (dim == 1 && elemSize == expectedSize) {
                        uint64_t flags = *(uint64_t*)(propBuf + off + 8);
                        if (flags != 0) {
                            state.FProperty_ArrayDimOff = off;
                            state.FProperty_ElementSizeOff = off + 4;
                            state.FProperty_PropertyFlagsOff = off + 8;
                            UsmapLog("[USMap] FProperty::ArrayDim at 0x%X, ElementSize at 0x%X (=%d), PropertyFlags at 0x%X",
                                off, off + 4, elemSize, off + 8);
                            break;
                        }
                    }
                }
            }

            prop = EngineDiscovery::GetFFieldNext(state, prop);
        }
    }

    int baseOff = state.FBoolProperty_BaseOff >= 0 ? state.FBoolProperty_BaseOff : ((state.FProperty_OffsetOff == 0x44) ? 0x70 : 0x68);

    if (state.FArrayProperty_InnerOff < 0) {
        if ((state.UEMajor == 5 && state.UEMinor >= 3) || state.UEMajor > 5) {
            state.FArrayProperty_InnerOff = baseOff + 8;
        } else {
            state.FArrayProperty_InnerOff = baseOff;
        }
    } else if (state.FArrayProperty_InnerOff == baseOff && ((state.UEMajor == 5 && state.UEMinor >= 3) || state.UEMajor > 5)) {

        state.FArrayProperty_InnerOff = baseOff + 8;
    }

    if (state.FStructProperty_StructOff < 0)       state.FStructProperty_StructOff = baseOff;
    if (state.FObjectProperty_ClassOff < 0)        state.FObjectProperty_ClassOff = baseOff;
    if (state.FEnumProperty_UnderlyingOff < 0)     state.FEnumProperty_UnderlyingOff = baseOff;
    if (state.FEnumProperty_EnumOff < 0)           state.FEnumProperty_EnumOff = baseOff + 8;
    if (state.FMapProperty_KeyPropOff < 0)        { state.FMapProperty_KeyPropOff = baseOff; state.FMapProperty_ValuePropOff = baseOff + 8; }
    if (state.FSetProperty_ElementPropOff < 0)    state.FSetProperty_ElementPropOff = baseOff;
    if (state.FOptionalProperty_ValuePropOff < 0) state.FOptionalProperty_ValuePropOff = baseOff;
    if (state.FByteProperty_EnumOff < 0)           state.FByteProperty_EnumOff = baseOff;
    if (state.FBoolProperty_BaseOff < 0)           state.FBoolProperty_BaseOff = baseOff;
    if (state.FProperty_ArrayDimOff < 0) {
        if (state.FProperty_OffsetOff == 0x44) {
            state.FProperty_ArrayDimOff = 0x30;
            state.FProperty_ElementSizeOff = 0x34;
            state.FProperty_PropertyFlagsOff = 0x38;
        } else {
            state.FProperty_ArrayDimOff = 0x38;
            state.FProperty_ElementSizeOff = 0x3C;
            state.FProperty_PropertyFlagsOff = 0x40;
        }
    }
    if (state.FClassProperty_MetaClassOff < 0 && state.FObjectProperty_ClassOff >= 0) {
        state.FClassProperty_MetaClassOff = state.FObjectProperty_ClassOff + 8;
    }

    if (state.UClass_ClassFlagsOff < 0) {
        if (state.UEMajor >= 5) {
            state.UClass_ClassFlagsOff = 0xD8;
        } else if (state.UEMinor >= 25) {
            state.UClass_ClassFlagsOff = 0xC8;
        } else {
            state.UClass_ClassFlagsOff = 0xB8;
        }
    }
    if (state.UScriptStruct_StructFlagsOff < 0) {
        if (state.UEMajor >= 5) {
            state.UScriptStruct_StructFlagsOff = 0xC0;
        } else if (state.UEMinor >= 25) {
            state.UScriptStruct_StructFlagsOff = 0xB0;
        } else if (state.UEMinor >= 22) {
            state.UScriptStruct_StructFlagsOff = 0x98;
        } else {
            state.UScriptStruct_StructFlagsOff = 0x88;
        }
    }
    if (state.UEnum_EnumFlagsOff < 0 && state.UEnum_NamesOff >= 0) {
        state.UEnum_EnumFlagsOff = state.bIsNewUE5Enum ? (state.UEnum_NamesOff + 24) : (state.UEnum_NamesOff + 0x14);
    }

    if (state.UStruct_ScriptOff < 0) {
        if (state.UEMajor >= 5 || state.UEMinor >= 25) {
            state.UStruct_ScriptOff = 0x60;
        } else if (state.UEMinor >= 22) {
            state.UStruct_ScriptOff = 0x58;
        } else {
            state.UStruct_ScriptOff = 0x48;
        }
    }
    if (state.UClass_ClassDefaultObjectOff < 0) {
        if (state.UEMajor >= 5) {
            state.UClass_ClassDefaultObjectOff = 0x110;
        } else if (state.UEMinor >= 25) {
            state.UClass_ClassDefaultObjectOff = 0x118;
        } else {
            state.UClass_ClassDefaultObjectOff = 0xF8;
        }
    }
    if (state.UClass_InterfacesOff < 0) {
        if (state.UEMajor >= 5) {
            state.UClass_InterfacesOff = 0x1D0;
        } else if (state.UEMinor >= 25) {
            state.UClass_InterfacesOff = 0x1D8;
        } else if (state.UEMinor >= 22) {
            state.UClass_InterfacesOff = 0x1A8;
        } else {
            state.UClass_InterfacesOff = 0x1F0;
        }
    }
    if (state.UFunction_FlagsOff < 0) {
        if (state.UEMajor >= 5 || state.UEMinor >= 25) {
            state.UFunction_FlagsOff = 0xB0;
        } else if (state.UEMinor >= 22) {
            state.UFunction_FlagsOff = 0x98;
        } else {
            state.UFunction_FlagsOff = 0x88;
        }
    }
    if (state.UFunction_FlagsOff >= 0) {
        if (state.UFunction_NumParmsOff < 0)           state.UFunction_NumParmsOff = state.UFunction_FlagsOff + 4;
        if (state.UFunction_ParmsSizeOff < 0)          state.UFunction_ParmsSizeOff = state.UFunction_FlagsOff + 6;
        if (state.UFunction_ReturnValueOffsetOff < 0)   state.UFunction_ReturnValueOffsetOff = state.UFunction_FlagsOff + 8;
        if (state.UFunction_RPCIdOff < 0)               state.UFunction_RPCIdOff = state.UFunction_FlagsOff + 10;
        if (state.UFunction_RPCResponseIdOff < 0)       state.UFunction_RPCResponseIdOff = state.UFunction_FlagsOff + 12;
        if (state.UFunction_FuncOff < 0)                state.UFunction_FuncOff = state.UFunction_FlagsOff + 0x28;
    }
}

static void DiscoverPropertiesSizeOffset(EngineState& state) {

    EngineDiscovery::ResolveUStructPropertiesSizeOffset(state);
}

}

struct EnumInfo {
    std::string name;
    uint32_t enumFlags = 0;
    std::vector<std::pair<std::string, int64_t>> values;
};

static bool ReadEnumValues(const EngineState& state, void* enumObj, EnumInfo& info) {
    if (state.UEnum_NamesOff < 0) return false;

    info.name = EngineDiscovery::GetObjectNameStr(state, (UObject*)enumObj);
    if (info.name.empty() || info.name[0] == '<') return false;

    info.enumFlags = 0;
    if (state.UEnum_EnumFlagsOff >= 0) {
        SafeMem_Read((uint8_t*)enumObj + state.UEnum_EnumFlagsOff, &info.enumFlags, sizeof(uint32_t));
    }

    if (state.bIsNewUE5Enum) {
        uintptr_t enumPtr = reinterpret_cast<uintptr_t>(enumObj);
        uintptr_t namesTagPtr;
        if (!SafeMem_Read((uint8_t*)enumPtr + state.UEnum_NamesOff, &namesTagPtr, 8)) return false;

        uintptr_t valuesTagPtr;
        if (!SafeMem_Read((uint8_t*)enumPtr + state.UEnum_NamesOff + 8, &valuesTagPtr, 8)) return false;

        int32_t num = 0;
        if (!SafeMem_Read((uint8_t*)enumPtr + state.UEnum_NamesOff + 16, &num, 4)) return false;

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
            if (valueName.empty() || valueName == "None" || valueName[0] == '<') continue;

            size_t colonPos = valueName.find("::");
            if (colonPos != std::string::npos) {
                valueName = valueName.substr(colonPos + 2);
            }

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
        if (valueName.empty() || valueName == "None" || valueName[0] == '<') continue;

        size_t colonPos = valueName.find("::");
        if (colonPos != std::string::npos) {
            valueName = valueName.substr(colonPos + 2);
        }

        info.values.push_back({valueName, val});
    }

    return true;
}

static void SerializePropertyType(const EngineState& state, void* prop,
                                   USMapWriter& data, USMapNameTable& names) {
    if (!prop) {
        data.WriteByte((uint8_t)EPropertyType::Unknown);
        return;
    }

    std::string typeName = EngineDiscovery::GetFFieldTypeName(state, prop);
    EPropertyType mappedType = PropertyNameToUsmapType(typeName);

    if (mappedType == EPropertyType::Unknown) {
        UsmapLog("[USMap] WARNING: Unknown property type '%s' (prop=%p)", typeName.c_str(), prop);
    }

    bool isFakeEnum = false;
    if (mappedType == EPropertyType::ByteProperty && state.FByteProperty_EnumOff >= 0) {
        void* byteEnum = ReadPtrAt(prop, state.FByteProperty_EnumOff);
        if (byteEnum) {
            UClass* beCls = EngineDiscovery::GetObjectClass(state, (UObject*)byteEnum);
            if (beCls) {
                std::string clsN = EngineDiscovery::GetObjectNameStr(state, beCls);
                if (clsN == "Enum" || clsN == "UserDefinedEnum") {
                    isFakeEnum = true;
                }
            }
        }
    }

    if (isFakeEnum) {

        data.WriteByte((uint8_t)EPropertyType::EnumProperty);
        data.WriteByte((uint8_t)EPropertyType::ByteProperty);
        void* byteEnum = ReadPtrAt(prop, state.FByteProperty_EnumOff);
        std::string enumName = EngineDiscovery::GetObjectNameStr(state, (UObject*)byteEnum);
        data.WriteInt32(names.AddName(enumName));
        return;
    }

    data.WriteByte((uint8_t)mappedType);

    switch (mappedType) {
    case EPropertyType::EnumProperty: {

        void* underlying = ReadPtrAt(prop, state.FEnumProperty_UnderlyingOff);
        SerializePropertyType(state, underlying, data, names);

        void* enumPtr = ReadPtrAt(prop, state.FEnumProperty_EnumOff);
        if (enumPtr) {
            std::string enumName = EngineDiscovery::GetObjectNameStr(state, (UObject*)enumPtr);
            data.WriteInt32(names.AddName(enumName));
        } else {
            data.WriteInt32(names.AddName(""));
        }
        break;
    }

    case EPropertyType::StructProperty: {
        void* structPtr = ReadPtrAt(prop, state.FStructProperty_StructOff);
        if (structPtr) {
            std::string structName = EngineDiscovery::GetObjectNameStr(state, (UObject*)structPtr);
            data.WriteInt32(names.AddName(structName));
        } else {
            data.WriteInt32(names.AddName(""));
        }
        break;
    }

    case EPropertyType::ObjectProperty:
    case EPropertyType::WeakObjectProperty:
    case EPropertyType::LazyObjectProperty:
    case EPropertyType::AssetObjectProperty:
    case EPropertyType::SoftObjectProperty:
    case EPropertyType::InterfaceProperty:

        break;

    case EPropertyType::ArrayProperty: {
        void* inner = ReadPtrAt(prop, state.FArrayProperty_InnerOff);
        if (!inner) {
            UsmapLog("[USMap] WARNING: ArrayProperty has null inner property, using IntProperty as fallback");
            data.WriteByte((uint8_t)EPropertyType::IntProperty);
        } else {
            SerializePropertyType(state, inner, data, names);
        }
        break;
    }

    case EPropertyType::SetProperty: {
        void* elem = ReadPtrAt(prop, state.FSetProperty_ElementPropOff);
        if (!elem) {
            UsmapLog("[USMap] WARNING: SetProperty has null element property, using IntProperty as fallback");
            data.WriteByte((uint8_t)EPropertyType::IntProperty);
        } else {
            SerializePropertyType(state, elem, data, names);
        }
        break;
    }

    case EPropertyType::MapProperty: {
        void* key = ReadPtrAt(prop, state.FMapProperty_KeyPropOff);
        void* val = ReadPtrAt(prop, state.FMapProperty_ValuePropOff);
        if (!key || !val) {
            UsmapLog("[USMap] WARNING: MapProperty has null key or value property, using IntProperty as fallback");
            data.WriteByte((uint8_t)EPropertyType::IntProperty);
            data.WriteByte((uint8_t)EPropertyType::IntProperty);
        } else {
            SerializePropertyType(state, key, data, names);
            SerializePropertyType(state, val, data, names);
        }
        break;
    }

    case EPropertyType::OptionalProperty: {

        void* valProp = ReadPtrAt(prop, state.FOptionalProperty_ValuePropOff);

        if (!valProp && state.FArrayProperty_InnerOff >= 0) {
            valProp = ReadPtrAt(prop, state.FArrayProperty_InnerOff);
        }
        if (!valProp) {
            UsmapLog("[USMap] WARNING: OptionalProperty has null value property, using IntProperty as fallback");
            data.WriteByte((uint8_t)EPropertyType::IntProperty);
        } else {
            SerializePropertyType(state, valProp, data, names);
        }
        break;
    }

    default:

        break;
    }
}

struct PropertySerInfo {
    uint16_t schemaIdx;
    uint8_t arrayDim;
    std::string name;
    void* prop;
};

struct FunctionParamInfo {
    int32_t nameIdx;
    uint64_t flags;
    void* prop;
};

struct FunctionSerInfo {
    int32_t nameIdx;
    uint32_t flags;
    uint8_t numParms;
    uint16_t parmsSize;
    uint16_t returnValueOffset;
    uint16_t rpcId;
    uint16_t rpcResponseId;
    uint64_t nativeRVA;
    std::vector<uint8_t> bytecode;
    std::vector<FunctionParamInfo> params;
};

struct StructVmbcInfo {
    std::vector<uint8_t> structBytecode;
    struct FuncBytecodeInfo {
        uint64_t nativeRVA;
        std::vector<uint8_t> bytecode;
    };
    std::vector<FuncBytecodeInfo> funcBytecodes;
};

struct PropMoffInfo {
    int32_t offset;
    int32_t elemSize;
    uint8_t byteOff;
    uint8_t byteMask;
    uint8_t fieldMask;
};

struct StructMoffInfo {
    int32_t propSize;
    int32_t minAlignment;
    std::vector<PropMoffInfo> props;
};

struct ClassDfltInfo {
    int32_t cdoNameIdx;
    uint32_t cdoFlags;
};

struct InterfaceSerInfo {
    int32_t ifaceNameIdx;
    int32_t ptrOffset;
};

struct ClassVmtbInfo {
    std::vector<InterfaceSerInfo> interfaces;
};

static std::vector<FunctionSerInfo> CollectStructFunctions(const EngineState& state, void* ustruct, uintptr_t moduleBase, USMapNameTable& names) {
    std::vector<FunctionSerInfo> funcs;
    if (!ustruct || state.UStruct_ChildrenOff < 0 || state.UField_NextOff < 0) return funcs;

    void* child = EngineDiscovery::GetChildrenHead(state, (UStruct*)ustruct);
    int safetyCount = 0;

    while (child && safetyCount++ < 2000) {
        UClass* childCls = EngineDiscovery::GetObjectClass(state, (UObject*)child);
        std::string clsName = childCls ? EngineDiscovery::GetObjectNameStr(state, childCls) : "";

        if (clsName == "Function" || clsName == "DelegateFunction" || clsName == "SparseDelegateFunction") {
            std::string fnName = EngineDiscovery::GetObjectNameStr(state, (UObject*)child);
            if (!fnName.empty() && fnName[0] != '<') {
                FunctionSerInfo fi;
                fi.nameIdx = names.AddName(fnName);
                fi.flags = 0;
                if (state.UFunction_FlagsOff >= 0) {
                    SafeMem_Read((uint8_t*)child + state.UFunction_FlagsOff, &fi.flags, sizeof(uint32_t));
                }
                fi.numParms = 0;
                if (state.UFunction_NumParmsOff >= 0) {
                    SafeMem_Read((uint8_t*)child + state.UFunction_NumParmsOff, &fi.numParms, 1);
                }
                fi.parmsSize = 0;
                if (state.UFunction_ParmsSizeOff >= 0) {
                    SafeMem_Read((uint8_t*)child + state.UFunction_ParmsSizeOff, &fi.parmsSize, sizeof(uint16_t));
                }
                fi.returnValueOffset = 0;
                if (state.UFunction_ReturnValueOffsetOff >= 0) {
                    SafeMem_Read((uint8_t*)child + state.UFunction_ReturnValueOffsetOff, &fi.returnValueOffset, sizeof(uint16_t));
                }
                fi.rpcId = 0;
                if (state.UFunction_RPCIdOff >= 0) {
                    SafeMem_Read((uint8_t*)child + state.UFunction_RPCIdOff, &fi.rpcId, sizeof(uint16_t));
                }
                fi.rpcResponseId = 0;
                if (state.UFunction_RPCResponseIdOff >= 0) {
                    SafeMem_Read((uint8_t*)child + state.UFunction_RPCResponseIdOff, &fi.rpcResponseId, sizeof(uint16_t));
                }

                fi.nativeRVA = 0;
                if (state.UFunction_FuncOff >= 0) {
                    void* funcPtr = ReadPtrAt(child, state.UFunction_FuncOff);
                    if (funcPtr && (uintptr_t)funcPtr > moduleBase && (uintptr_t)funcPtr < moduleBase + 0x20000000ULL) {
                        fi.nativeRVA = (uint64_t)((uintptr_t)funcPtr - moduleBase);
                    }
                }

                if (state.UStruct_ScriptOff >= 0) {
                    void* scriptData = ReadPtrAt(child, state.UStruct_ScriptOff);
                    int32_t scriptNum = ReadI32At(child, state.UStruct_ScriptOff + 8);
                    if (scriptData && scriptNum > 0 && scriptNum < 100000) {
                        fi.bytecode.resize(scriptNum);
                        if (SafeMem_ReadSpanExact(scriptData, fi.bytecode.data(),
                                                  static_cast<size_t>(scriptNum)) !=
                            static_cast<size_t>(scriptNum)) {
                            fi.bytecode.clear();
                        }
                    }
                }

                void* paramProp = EngineDiscovery::GetChildPropsHead(state, (UStruct*)child);
                int pCount = 0;
                while (paramProp && pCount++ < 100) {
                    std::string pName = EngineDiscovery::GetFFieldName(state, paramProp);
                    std::string pType = EngineDiscovery::GetFFieldTypeName(state, paramProp);
                    if (!pName.empty() && !pType.empty() && pType.find("Property") != std::string::npos) {
                        uint64_t pFlags = 0;
                        if (state.FProperty_PropertyFlagsOff >= 0) {
                            SafeMem_Read((uint8_t*)paramProp + state.FProperty_PropertyFlagsOff, &pFlags, sizeof(uint64_t));
                        }
                        fi.params.push_back({names.AddName(pName), pFlags, paramProp});
                    }
                    paramProp = EngineDiscovery::GetFFieldNext(state, paramProp);
                }

                funcs.push_back(std::move(fi));
            }
        }
        child = EngineDiscovery::GetFieldNext(state, child);
    }
    return funcs;
}

namespace EngineDiscovery {

bool GenerateUSMap(const EngineState& state, const std::string& outputPath) {
    EngineDiscovery::ResetSdkLogTimeBaseline();
    UsmapLog("[USMap] ====== Starting .usmap Generation ======");
    UsmapLog("[USMap] Output: %s", outputPath.c_str());

    if (!state.bInitialized) {
        UsmapLog("[USMap] Error: Engine not initialized");
        return false;
    }

    EngineState& mutableState = const_cast<EngineState&>(state);
    USMapDiscovery::DiscoverUEnumNamesOffset(state);
    USMapDiscovery::DiscoverPropertySubTypeOffsets(mutableState);
    USMapDiscovery::DiscoverPropertiesSizeOffset(mutableState);

    UsmapLog("[USMap] === I discovered some offsets for you ===");
    UsmapLog("[USMap]   FProperty_OffsetOff = 0x%X", state.FProperty_OffsetOff);
    UsmapLog("[USMap]   FProperty_ArrayDimOff = 0x%X", state.FProperty_ArrayDimOff);
    UsmapLog("[USMap]   FProperty_ElementSizeOff = 0x%X", state.FProperty_ElementSizeOff);
    UsmapLog("[USMap]   FProperty_PropertyFlagsOff = 0x%X", state.FProperty_PropertyFlagsOff);
    UsmapLog("[USMap]   FBoolProperty_BaseOff = 0x%X", state.FBoolProperty_BaseOff);
    UsmapLog("[USMap]   FArrayProperty_InnerOff = 0x%X", state.FArrayProperty_InnerOff);
    UsmapLog("[USMap]   FStructProperty_StructOff = 0x%X", state.FStructProperty_StructOff);
    UsmapLog("[USMap]   FObjectProperty_ClassOff = 0x%X", state.FObjectProperty_ClassOff);
    UsmapLog("[USMap]   FEnumProperty_UnderlyingOff = 0x%X", state.FEnumProperty_UnderlyingOff);
    UsmapLog("[USMap]   FEnumProperty_EnumOff = 0x%X", state.FEnumProperty_EnumOff);
    UsmapLog("[USMap]   FMapProperty_KeyPropOff = 0x%X", state.FMapProperty_KeyPropOff);
    UsmapLog("[USMap]   FMapProperty_ValuePropOff = 0x%X", state.FMapProperty_ValuePropOff);
    UsmapLog("[USMap]   FSetProperty_ElementPropOff = 0x%X", state.FSetProperty_ElementPropOff);
    UsmapLog("[USMap]   FByteProperty_EnumOff = 0x%X", state.FByteProperty_EnumOff);
    UsmapLog("[USMap]   FClassProperty_MetaClassOff = 0x%X", state.FClassProperty_MetaClassOff);
    UsmapLog("[USMap]   UStruct_ChildPropsOff = 0x%X", state.UStruct_ChildPropsOff);
    UsmapLog("[USMap]   UStruct_SuperOff = 0x%X", state.UStruct_SuperOff);
    UsmapLog("[USMap]   UClass_ClassDefaultObjectOff = 0x%X", state.UClass_ClassDefaultObjectOff);
    UsmapLog("[USMap]   bUseFProperty = %d", state.bUseFProperty);
    UsmapLog("[USMap] === Nothing to see here anymore, moving along ===");

    USMapNameTable names;
    USMapWriter enumData;
    USMapWriter structData;
    uint32_t numEnums = 0;
    uint32_t numStructs = 0;

    int32_t totalObjects = GetObjectCount(state);
    UsmapLog("[USMap] Total objects: %d", totalObjects);

    UsmapLog("[USMap] Phase 1: Serializing enums...");

    SafeMem_SetFastMode(1);

    std::set<std::string> seenPackagePaths;
    std::vector<int32_t> enumPkgPathIndices;
    std::vector<uint32_t> enumFlagsList;

    for (int i = 0; i < totalObjects; i++) {
        UObject* obj = GetObjectByIndex(state, i);
        if (!obj) continue;

        UClass* cls = GetObjectClass(state, obj);
        if (!cls) continue;

        std::string clsName = GetObjectNameStr(state, cls);
        if (clsName != "Enum" && clsName != "UserDefinedEnum") continue;

        EnumInfo info;
        if (!ReadEnumValues(state, obj, info)) continue;

        std::string pkgPath = GetPackagePath(state, obj);
        int32_t pkgPathIdx = -1;
        if (!pkgPath.empty()) {
            pkgPathIdx = names.AddName(pkgPath);
            seenPackagePaths.insert(pkgPath);
        }
        enumPkgPathIndices.push_back(pkgPathIdx);
        enumFlagsList.push_back(info.enumFlags);

        int32_t enumNameIdx = names.AddName(info.name);
        enumData.WriteInt32(enumNameIdx);

        enumData.WriteUInt16((uint16_t)info.values.size());

        for (auto& [valName, valValue] : info.values) {
            enumData.WriteInt64(valValue);
            int32_t valNameIdx = names.AddName(valName);
            enumData.WriteInt32(valNameIdx);
        }

        numEnums++;
    }

    UsmapLog("[USMap] Serialized %u enums", numEnums);

    UsmapLog("[USMap] Phase 2: Collecting structs and classes...");

    struct StructEntry {
        void* ptr;
        std::string name;
        std::string clsName;
        int32_t pkgPathIdx;
    };

    std::vector<StructEntry> allStructs;
    std::set<void*> seenStructs;

    for (int i = 0; i < totalObjects; i++) {
        UObject* obj = GetObjectByIndex(state, i);
        if (!obj) continue;

        UClass* cls = GetObjectClass(state, obj);
        if (!cls) continue;

        std::string clsName = GetObjectNameStr(state, cls);
        if (clsName != "Class" && clsName != "ScriptStruct" && clsName != "AnimBlueprintGeneratedStruct") continue;

        std::string objName = GetObjectNameStr(state, obj);
        if (objName.empty() || objName[0] == '<') continue;

        if (seenStructs.count(obj)) continue;
        seenStructs.insert(obj);

        std::string pkgPath = GetPackagePath(state, obj);
        int32_t pkgPathIdx = -1;
        if (!pkgPath.empty()) {
            pkgPathIdx = names.AddName(pkgPath);
            seenPackagePaths.insert(pkgPath);
        }

        allStructs.push_back({obj, objName, clsName, pkgPathIdx});
    }

    UsmapLog("[USMap] Found %zu structs/classes to serialize", allStructs.size());
    UsmapLog("[USMap] Found %zu unique package paths", seenPackagePaths.size());

    UsmapLog("[USMap] Phase 3: Serializing structs...");

    struct StructEatrEntry {
        uint8_t type;
        int32_t flags;
        std::vector<uint64_t> propFlags;
    };

    std::vector<int32_t> structPkgPathIndices;
    std::vector<StructEatrEntry> structEatrList;
    std::vector<StructMoffInfo> structMoffList;
    std::vector<std::vector<FunctionSerInfo>> structFuncList;
    std::vector<StructVmbcInfo> structVmbcList;
    std::vector<ClassDfltInfo> classDfltList;
    std::vector<ClassVmtbInfo> classVmtbList;

    uintptr_t moduleBase = (uintptr_t)GetModuleHandleA(NULL);

    for (auto& entry : allStructs) {
        void* ustruct = entry.ptr;
        structPkgPathIndices.push_back(entry.pkgPathIdx);

        uint8_t structType = 0;
        int32_t structFlags = 0;
        if (entry.clsName == "ScriptStruct" || entry.clsName == "AnimBlueprintGeneratedStruct") {
            structType = 1;
            if (state.UScriptStruct_StructFlagsOff >= 0) {
                structFlags = ReadI32At(ustruct, state.UScriptStruct_StructFlagsOff);
            }
        } else if (entry.clsName == "Class") {
            structType = 2;
            if (state.UClass_ClassFlagsOff >= 0) {
                structFlags = ReadI32At(ustruct, state.UClass_ClassFlagsOff);
            }
        }

        int32_t structNameIdx = names.AddName(entry.name);
        structData.WriteInt32(structNameIdx);

        UStruct* super = GetSuperStruct(state, (UStruct*)ustruct);
        if (super) {
            std::string superName = GetObjectNameStr(state, super);

            if (superName.empty() || superName[0] == '<' || superName.size() > 256) {
                structData.WriteInt32(-1);
            } else {
                structData.WriteInt32(names.AddName(superName));
            }
        } else {
            structData.WriteInt32(-1);
        }

        std::vector<PropertySerInfo> props;
        std::vector<uint64_t> propFlagsList;
        void* propField = GetChildPropsHead(state, (UStruct*)ustruct);
        void* prevPropField = nullptr;
        uint16_t totalPropCount = 0;
        uint16_t sequentialIdx = 0;

        bool debugThisStruct = (entry.name == "StaticMesh" || entry.name == "StreamableRenderAsset" ||
                                entry.name == "SoundNodeEnveloper" || entry.name == "SoundNode" ||
                                entry.name == "SimpleConstructionScript" || entry.name == "SCS_Node" ||
                                entry.name == "BlueprintCookedComponentInstancingData");
        if (debugThisStruct) {
            UsmapLog("[USMap] DEBUG: Serializing struct '%s' (super=%s)", entry.name.c_str(),
                     super ? GetObjectNameStr(state, super).c_str() : "None");
        }

        while (propField && props.size() < 1000) {
            std::string propName = GetFFieldName(state, propField);
            if (propName.empty()) {
                prevPropField = propField;
                propField = GetFFieldNext(state, propField);
                continue;
            }

            std::string propTypeName = GetFFieldTypeName(state, propField);

            if (propTypeName.empty() || propTypeName.find("Property") == std::string::npos) {
                if (prevPropField) {
                    UsmapLog("[Intelligence] AUTOCORRECT: Bad FField '%s' (type='%s') discovered after '%s'.",
                             propName.c_str(), propTypeName.c_str(), GetFFieldName(state, prevPropField).c_str());
                    UsmapLog("[Intelligence] AUTOCORRECT: Current FField_NextOff=0x%X is leading to garbage. Launching auto-correction...", state.FField_NextOff);

                    int bestNextOff = -1;
                    void* bestNextProp = nullptr;
                    int bestNameOff = state.FField_NameOff;

                    for (int testOff = 0x18; testOff <= 0x80; testOff += 8) {
                        if (testOff == state.FField_NextOff) continue;

                        void* testNext = SafeMem_ReadPtr((uint8_t*)prevPropField + testOff);
                        if (!testNext || (uintptr_t)testNext < 0x10000 || (uintptr_t)testNext >= 0x7FF000000000ULL) continue;

                        std::string testTypeName = GetFFieldTypeName(state, testNext);
                        if (testTypeName.empty() || testTypeName.find("Property") == std::string::npos) continue;

                        std::string testName = GetFFieldName(state, testNext);

                        if (testName.empty() || testName == "None" || testName[0] == '<') {
                            for (int nOff = 0x20; nOff <= 0x40; nOff += 8) {
                                if (nOff == state.FField_NameOff) continue;
                                FName fn;
                                if (SafeMem_Read((uint8_t*)testNext + nOff, &fn, sizeof(FName))) {
                                    std::string nTest = EngineDiscovery::FNameToStr(state, fn);
                                    if (!nTest.empty() && nTest != "None" && nTest[0] != '<' && nTest.find("Property") == std::string::npos) {
                                        testName = nTest;
                                        bestNameOff = nOff;
                                        break;
                                    }
                                }
                            }
                        }

                        if (!testName.empty() && testName != "None" && testName[0] != '<') {
                            UsmapLog("[Intelligence] AUTOCORRECT: Found valid next property '%s' (%s) at NextOff 0x%X, NameOff 0x%X!",
                                     testName.c_str(), testTypeName.c_str(), testOff, bestNameOff);
                            bestNextOff = testOff;
                            bestNextProp = testNext;
                            break;
                        }
                    }

                    if (bestNextOff > 0) {
                        UsmapLog("[Intelligence] AUTOCORRECT: Fixing FField_NextOff globally from 0x%X to 0x%X.", state.FField_NextOff, bestNextOff);
                        const_cast<EngineState&>(state).FField_NextOff = bestNextOff;
                        if (bestNameOff != state.FField_NameOff) {
                            UsmapLog("[Intelligence] AUTOCORRECT: Fixing FField_NameOff globally from 0x%X to 0x%X.", state.FField_NameOff, bestNameOff);
                            const_cast<EngineState&>(state).FField_NameOff = bestNameOff;
                        }
                        propField = bestNextProp;
                        continue;
                    } else {
                        UsmapLog("[Intelligence] AUTOCORRECT: Failed to find a valid alternative Next pointer. Stopping chain.");
                        break;
                    }
                } else {

                    void* nextField = GetFFieldNext(state, propField);
                    if (nextField && nextField != propField) {
                        prevPropField = propField;
                        propField = nextField;
                        continue;
                    }
                    break;
                }
            }

            uint8_t arrayDim = 1;
            if (state.FProperty_ArrayDimOff >= 0) {
                int32_t dim = ReadI32At(propField, state.FProperty_ArrayDimOff);
                if (dim >= 1 && dim <= 64) arrayDim = (uint8_t)dim;
            }

            uint64_t propertyFlags = 0;
            if (state.FProperty_PropertyFlagsOff >= 0) {
                SafeMem_Read((uint8_t*)propField + state.FProperty_PropertyFlagsOff, &propertyFlags, 8);
                if (propertyFlags & 0x0000000000080000ULL) {
                    if (debugThisStruct) {
                        UsmapLog("[USMap] DEBUG:   (Has EditorOnly flag) '%s' flags=0x%llX",
                                 propName.c_str(), propertyFlags);
                    }
                }
            }
            propFlagsList.push_back(propertyFlags);

            uint16_t schemaIdx = sequentialIdx;

            if (debugThisStruct) {
                UsmapLog("[USMap] DEBUG:   [%d] '%s' type='%s' arrayDim=%d",
                         schemaIdx, propName.c_str(), propTypeName.c_str(), arrayDim);
            }

            props.push_back({schemaIdx, arrayDim, propName, propField});
            sequentialIdx += arrayDim;
            totalPropCount += arrayDim;

            prevPropField = propField;
            propField = GetFFieldNext(state, propField);
        }

        if (debugThisStruct) {
            UsmapLog("[USMap] DEBUG: '%s' totalPropCount=%d serializableCount=%zu",
                     entry.name.c_str(), totalPropCount, props.size());
        }

        structEatrList.push_back({structType, structFlags, std::move(propFlagsList)});

        StructMoffInfo moffInfo;
        moffInfo.propSize = state.UStruct_PropertiesSizeOff >= 0 ? ReadI32At(ustruct, state.UStruct_PropertiesSizeOff) : 0;
        moffInfo.minAlignment = state.UStruct_PropertiesSizeOff >= 0 ? ReadI32At(ustruct, state.UStruct_PropertiesSizeOff + 4) : 0;
        for (const auto& p : props) {
            int32_t offInternal = state.FProperty_OffsetOff >= 0 ? ReadI32At(p.prop, state.FProperty_OffsetOff) : 0;
            int32_t elemSize = state.FProperty_ElementSizeOff >= 0 ? ReadI32At(p.prop, state.FProperty_ElementSizeOff) : 0;
            uint8_t bOff = 0, bMask = 0, fMask = 0;
            std::string typeName = EngineDiscovery::GetFFieldTypeName(state, p.prop);
            if (typeName.find("Bool") != std::string::npos && state.FBoolProperty_BaseOff >= 0) {
                bOff = ReadU8At(p.prop, state.FBoolProperty_BaseOff + 1);
                bMask = ReadU8At(p.prop, state.FBoolProperty_BaseOff + 2);
                fMask = ReadU8At(p.prop, state.FBoolProperty_BaseOff + 3);
            }
            moffInfo.props.push_back({offInternal, elemSize, bOff, bMask, fMask});
        }
        structMoffList.push_back(std::move(moffInfo));

        std::vector<FunctionSerInfo> funcs = CollectStructFunctions(state, ustruct, moduleBase, names);

        StructVmbcInfo vmbcInfo;
        if (state.UStruct_ScriptOff >= 0) {
            void* scriptData = ReadPtrAt(ustruct, state.UStruct_ScriptOff);
            int32_t scriptNum = ReadI32At(ustruct, state.UStruct_ScriptOff + 8);
            if (scriptData && scriptNum > 0 && scriptNum < 500000) {
                vmbcInfo.structBytecode.resize(scriptNum);
                if (SafeMem_ReadSpanExact(scriptData, vmbcInfo.structBytecode.data(),
                                          static_cast<size_t>(scriptNum)) !=
                    static_cast<size_t>(scriptNum)) {
                    vmbcInfo.structBytecode.clear();
                }
            }
        }
        for (const auto& fn : funcs) {
            vmbcInfo.funcBytecodes.push_back({fn.nativeRVA, fn.bytecode});
        }
        structVmbcList.push_back(std::move(vmbcInfo));
        structFuncList.push_back(std::move(funcs));

        int32_t cdoNameIdx = -1;
        uint32_t cdoFlags = 0;
        if (entry.clsName == "Class" && state.UClass_ClassDefaultObjectOff >= 0) {
            void* cdoPtr = ReadPtrAt(ustruct, state.UClass_ClassDefaultObjectOff);
            if (cdoPtr) {
                std::string cdoName = EngineDiscovery::GetObjectNameStr(state, (UObject*)cdoPtr);
                if (!cdoName.empty() && cdoName[0] != '<') {
                    cdoNameIdx = names.AddName(cdoName);
                }
                cdoFlags = (uint32_t)ReadI32At(cdoPtr, state.UObject_FlagsOff);
                if (cdoFlags == 0) cdoFlags = 0x10;
            }
        }
        classDfltList.push_back({cdoNameIdx, cdoFlags});

        ClassVmtbInfo vmtbInfo;
        if (entry.clsName == "Class" && state.UClass_InterfacesOff >= 0) {
            void* ifaceData = ReadPtrAt(ustruct, state.UClass_InterfacesOff);
            int32_t ifaceNum = ReadI32At(ustruct, state.UClass_InterfacesOff + 8);
            if (ifaceData && ifaceNum > 0 && ifaceNum < 100) {
                for (int i = 0; i < ifaceNum; i++) {
                    void* ifaceCls = ReadPtrAt((uint8_t*)ifaceData + i * 16, 0);
                    int32_t ptrOff = ReadI32At((uint8_t*)ifaceData + i * 16, 8);
                    if (ifaceCls) {
                        std::string ifaceName = EngineDiscovery::GetObjectNameStr(state, (UObject*)ifaceCls);
                        if (!ifaceName.empty() && ifaceName[0] != '<') {
                            vmtbInfo.interfaces.push_back({names.AddName(ifaceName), ptrOff});
                        }
                    }
                }
            }
        }
        classVmtbList.push_back(std::move(vmtbInfo));

        structData.WriteUInt16(totalPropCount);

        structData.WriteUInt16((uint16_t)props.size());

        for (auto& p : props) {
            structData.WriteUInt16(p.schemaIdx);
            structData.WriteByte(p.arrayDim);
            structData.WriteInt32(names.AddName(p.name));

            SerializePropertyType(state, p.prop, structData, names);
        }

        numStructs++;
    }

    UsmapLog("[USMap] Serialized %u structs/classes", numStructs);

    UsmapLog("[USMap] Phase 4: Assembling payload...");

    USMapWriter payload;

    names.Serialize(payload);
    UsmapLog("[USMap] Name table: %u names", names.GetCount());

    payload.WriteUInt32(numEnums);
    payload.WriteBytes(enumData.buffer.data(), enumData.buffer.size());

    payload.WriteUInt32(numStructs);
    payload.WriteBytes(structData.buffer.data(), structData.buffer.size());

    payload.WriteUInt32(0x54584543);
    payload.WriteByte(0);
    payload.WriteUInt32(7);

    payload.WriteUInt32(0x48545050);
    uint32_t ppthSize = 1 + 4 + (uint32_t)enumPkgPathIndices.size() * 4 + 4 + (uint32_t)structPkgPathIndices.size() * 4;
    payload.WriteUInt32(ppthSize);
    payload.WriteByte(0);
    payload.WriteUInt32((uint32_t)enumPkgPathIndices.size());
    for (int32_t idx : enumPkgPathIndices) {
        payload.WriteInt32(idx);
    }
    payload.WriteUInt32((uint32_t)structPkgPathIndices.size());
    for (int32_t idx : structPkgPathIndices) {
        payload.WriteInt32(idx);
    }

    payload.WriteUInt32(0x52544145);
    uint32_t eatrStructsSize = 0;
    for (const auto& s : structEatrList) {
        eatrStructsSize += 1 + 4 + 4 + (uint32_t)s.propFlags.size() * 8;
    }
    uint32_t eatrSize = 1 + 4 + (uint32_t)enumFlagsList.size() * 4 + 4 + eatrStructsSize;
    payload.WriteUInt32(eatrSize);
    payload.WriteByte(0);
    payload.WriteUInt32((uint32_t)enumFlagsList.size());
    for (uint32_t f : enumFlagsList) {
        payload.WriteUInt32(f);
    }
    payload.WriteUInt32((uint32_t)structEatrList.size());
    for (const auto& s : structEatrList) {
        payload.WriteByte(s.type);
        payload.WriteInt32(s.flags);
        payload.WriteUInt32((uint32_t)s.propFlags.size());
        for (uint64_t pf : s.propFlags) {
            payload.WriteUInt64(pf);
        }
    }

    payload.WriteUInt32(0x46464F4D);
    uint32_t moffSize = 1 + 4;
    for (const auto& s : structMoffList) {
        moffSize += 4 + 4 + 4 + (uint32_t)s.props.size() * (4 + 4 + 1 + 1 + 1);
    }
    payload.WriteUInt32(moffSize);
    payload.WriteByte(0);
    payload.WriteUInt32((uint32_t)structMoffList.size());
    for (const auto& s : structMoffList) {
        payload.WriteInt32(s.propSize);
        payload.WriteInt32(s.minAlignment);
        payload.WriteUInt32((uint32_t)s.props.size());
        for (const auto& p : s.props) {
            payload.WriteInt32(p.offset);
            payload.WriteInt32(p.elemSize);
            payload.WriteByte(p.byteOff);
            payload.WriteByte(p.byteMask);
            payload.WriteByte(p.fieldMask);
        }
    }

    payload.WriteUInt32(0x434E5546);
    USMapWriter funcWriter;
    funcWriter.WriteByte(0);
    funcWriter.WriteUInt32((uint32_t)structFuncList.size());
    for (const auto& sFuncs : structFuncList) {
        funcWriter.WriteUInt32((uint32_t)sFuncs.size());
        for (const auto& fn : sFuncs) {
            funcWriter.WriteInt32(fn.nameIdx);
            funcWriter.WriteUInt32(fn.flags);
            funcWriter.WriteByte(fn.numParms);
            funcWriter.WriteUInt16(fn.parmsSize);
            funcWriter.WriteUInt16(fn.returnValueOffset);
            funcWriter.WriteUInt16(fn.rpcId);
            funcWriter.WriteUInt16(fn.rpcResponseId);
            funcWriter.WriteUInt32((uint32_t)fn.params.size());
            for (const auto& p : fn.params) {
                funcWriter.WriteInt32(p.nameIdx);
                funcWriter.WriteUInt64(p.flags);
                SerializePropertyType(state, p.prop, funcWriter, names);
            }
        }
    }
    payload.WriteUInt32((uint32_t)funcWriter.buffer.size());
    payload.WriteBytes(funcWriter.buffer.data(), funcWriter.buffer.size());

    payload.WriteUInt32(0x43424D56);
    USMapWriter vmbcWriter;
    vmbcWriter.WriteByte(0);
    vmbcWriter.WriteUInt32((uint32_t)structVmbcList.size());
    for (const auto& vmbc : structVmbcList) {
        vmbcWriter.WriteUInt32((uint32_t)vmbc.structBytecode.size());
        if (!vmbc.structBytecode.empty()) {
            vmbcWriter.WriteBytes(vmbc.structBytecode.data(), vmbc.structBytecode.size());
        }
        vmbcWriter.WriteUInt32((uint32_t)vmbc.funcBytecodes.size());
        for (const auto& fbc : vmbc.funcBytecodes) {
            vmbcWriter.WriteUInt64(fbc.nativeRVA);
            vmbcWriter.WriteUInt32((uint32_t)fbc.bytecode.size());
            if (!fbc.bytecode.empty()) {
                vmbcWriter.WriteBytes(fbc.bytecode.data(), fbc.bytecode.size());
            }
        }
    }
    payload.WriteUInt32((uint32_t)vmbcWriter.buffer.size());
    payload.WriteBytes(vmbcWriter.buffer.data(), vmbcWriter.buffer.size());

    payload.WriteUInt32(0x544C4644);
    uint32_t dfltSize = 1 + 4 + (uint32_t)classDfltList.size() * (4 + 4);
    payload.WriteUInt32(dfltSize);
    payload.WriteByte(0);
    payload.WriteUInt32((uint32_t)classDfltList.size());
    for (const auto& d : classDfltList) {
        payload.WriteInt32(d.cdoNameIdx);
        payload.WriteUInt32(d.cdoFlags);
    }

    payload.WriteUInt32(0x42544D56);
    USMapWriter vmtbWriter;
    vmtbWriter.WriteByte(0);
    vmtbWriter.WriteUInt32((uint32_t)classVmtbList.size());
    for (const auto& v : classVmtbList) {
        vmtbWriter.WriteUInt32((uint32_t)v.interfaces.size());
        for (const auto& iface : v.interfaces) {
            vmtbWriter.WriteInt32(iface.ifaceNameIdx);
            vmtbWriter.WriteInt32(iface.ptrOffset);
        }
    }
    payload.WriteUInt32((uint32_t)vmtbWriter.buffer.size());
    payload.WriteBytes(vmtbWriter.buffer.data(), vmtbWriter.buffer.size());

    UsmapLog("[USMap] CEXT extensions: PPTH=%u, EATR=%u, MOFF=%u, FUNC=%u, VMBC=%u, DFLT=%u, VMTB=%u bytes",
             ppthSize, eatrSize, moffSize, (uint32_t)funcWriter.buffer.size(), (uint32_t)vmbcWriter.buffer.size(), dfltSize, (uint32_t)vmtbWriter.buffer.size());
    UsmapLog("[USMap] Data payload: %zu bytes", payload.buffer.size());

    UsmapLog("[USMap] Phase 5: Writing file...");

    USMapWriter file;

    constexpr uint16_t USMAP_MAGIC = 0x30C4;
    file.WriteUInt16(USMAP_MAGIC);

    enum class EUsmapVersion : uint8_t {
        Initial = 0,
        PackageVersioning = 1,
        LongFName = 2,
        LargeEnums = 3,
        ExplicitEnumValues = 4,
    };
    file.WriteByte((uint8_t)EUsmapVersion::ExplicitEnumValues);

    file.WriteInt32(0);

    file.WriteByte(0x00);

    uint32_t dataSize = (uint32_t)payload.buffer.size();
    file.WriteUInt32(dataSize);
    file.WriteUInt32(dataSize);

    file.WriteBytes(payload.buffer.data(), payload.buffer.size());

    bool ok = file.SaveToFile(outputPath, UsmapLog);
    if (ok) {
        UsmapLog("[USMap] ====== YAY, Be happy; usmap written successfully! ======");
        UsmapLog("[USMap] File: %s (%zu bytes)", outputPath.c_str(), file.buffer.size());
        UsmapLog("[USMap] Names: %u, Enums: %u, Structs: %u", names.GetCount(), numEnums, numStructs);
    } else {
        UsmapLog("[USMap] ERROR: Ugh, damn it, Avalonium failed to write the file for you: %s", outputPath.c_str());
        UsmapLog("[USMap] Buffer size: %zu bytes", file.buffer.size());
    }

    SafeMem_SetFastMode(0);

    return ok;
}

}
