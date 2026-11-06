#include "ObjectUtils.h"
#include "NameResolution.h"
#include "ObjectArray.h"
#include "Logger.h"
#include "../utils/SafeMemory.h"
#include <cstddef>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace EngineDiscovery {

static std::unordered_map<std::string, void *> s_PropertyTypeClassByIdentity;
static std::mutex s_PropertyTypeClassMutex;

void RecordPropertyTypeClass(const std::string &typeName, void *classInstance) {
  if (typeName.empty() || !classInstance)
    return;
  std::lock_guard<std::mutex> lock(s_PropertyTypeClassMutex);
  s_PropertyTypeClassByIdentity[typeName] = classInstance;
}

void *GetRecordedPropertyTypeClass(const std::string &typeName) {
  std::lock_guard<std::mutex> lock(s_PropertyTypeClassMutex);
  auto it = s_PropertyTypeClassByIdentity.find(typeName);
  return it == s_PropertyTypeClassByIdentity.end() ? nullptr : it->second;
}

UClass *GetObjectClass(const EngineState &state, UObject *obj) {
  if (!obj || !IsReadable((uint8_t *)obj + state.UObject_ClassOff))
    return nullptr;
  return (UClass *)SafeMem_ReadPtr((uint8_t *)obj + state.UObject_ClassOff);
}

UObject *GetObjectOuter(const EngineState &state, UObject *obj) {
  if (!obj || !IsReadable((uint8_t *)obj + state.UObject_OuterOff))
    return nullptr;
  return (UObject *)SafeMem_ReadPtr((uint8_t *)obj + state.UObject_OuterOff);
}

UStruct *GetSuperStruct(const EngineState &state, UStruct *ustruct) {
  if (!ustruct || state.UStruct_SuperOff < 0)
    return nullptr;
  if (!IsReadable((uint8_t *)ustruct + state.UStruct_SuperOff))
    return nullptr;
  return (UStruct *)SafeMem_ReadPtr((uint8_t *)ustruct +
                                    state.UStruct_SuperOff);
}

void *GetChildrenHead(const EngineState &state, UStruct *ustruct) {
  if (!ustruct || state.UStruct_ChildrenOff < 0)
    return nullptr;
  return SafeMem_ReadPtr((uint8_t *)ustruct + state.UStruct_ChildrenOff);
}

void *GetFieldNext(const EngineState &state, void *field) {
  if (!field || state.UField_NextOff < 0)
    return nullptr;
  void *next = SafeMem_ReadPtr((uint8_t *)field + state.UField_NextOff);
  return next;
}

std::string GetFieldName(const EngineState &state, void *field) {
  if (!field)
    return "";
  FName fn;
  if (!SafeMem_Read((uint8_t *)field + state.UObject_NameOff, &fn,
                    sizeof(FName)))
    return "";
  return FNameToStr(state, fn);
}

void *GetChildPropsHead(const EngineState &state, UStruct *ustruct) {
  if (!ustruct || state.UStruct_ChildPropsOff < 0)
    return nullptr;
  return SafeMem_ReadPtr((uint8_t *)ustruct + state.UStruct_ChildPropsOff);
}

void *GetFFieldNext(const EngineState &state, void *ffield) {
  if (!ffield || state.FField_NextOff < 0)
    return nullptr;
  return SafeMem_ReadPtr((uint8_t *)ffield + state.FField_NextOff);
}

std::string GetFFieldName(const EngineState &state, void *ffield) {
  if (!ffield || state.FField_NameOff < 0)
    return "";
  FName fn = {};
  if (state.FName_Size == 4 || state.FName_NumberOff < 0) {
    if (!SafeMem_Read((uint8_t *)ffield + state.FField_NameOff, &fn.ComparisonIndex, 4))
      return "";
    fn.Number = 0;
  } else {
    if (!SafeMem_Read((uint8_t *)ffield + state.FField_NameOff, &fn, sizeof(FName)))
      return "";
  }
  return FNameToStr(state, fn);
}

std::string GetFFieldTypeName(const EngineState &state, void *ffield) {
  if (!ffield)
    return "";

  if (!state.bUseFProperty) {

    void *cls = SafeMem_ReadPtr((uint8_t *)ffield + state.UObject_ClassOff);
    if (!cls)
      return "";
    FName fn = {};
    if (state.FName_Size == 4 || state.FName_NumberOff < 0) {
      if (!SafeMem_Read((uint8_t *)cls + state.UObject_NameOff, &fn.ComparisonIndex, 4))
        return "";
      fn.Number = 0;
    } else {
      if (!SafeMem_Read((uint8_t *)cls + state.UObject_NameOff, &fn, sizeof(FName)))
        return "";
    }
    std::string legacyName = FNameToStr(state, fn);
    if (!legacyName.empty() && legacyName != "None" && legacyName[0] != '<') {
      RecordPropertyTypeClass(legacyName, cls);
    }
    return legacyName;
  }

  if (state.FField_ClassOff >= 0 && state.FFieldClass_NameOff >= 0) {
    void *ffclass = SafeMem_ReadPtr((uint8_t *)ffield + state.FField_ClassOff);
    if (ffclass && (uintptr_t)ffclass >= 0x10000 && (uintptr_t)ffclass < 0x7FFFFFFFFFFFULL) {
      FName fn = {};
      bool readOk = false;
      if (state.FName_Size == 4 || state.FName_NumberOff < 0) {
        readOk = SafeMem_Read((uint8_t *)ffclass + state.FFieldClass_NameOff, &fn.ComparisonIndex, 4);
        fn.Number = 0;
      } else {
        readOk = SafeMem_Read((uint8_t *)ffclass + state.FFieldClass_NameOff, &fn, sizeof(FName));
      }
      if (readOk) {
        std::string name = FNameToStr(state, fn);
        if (!name.empty() && name != "None" && name[0] != '<' && name.find("Property") != std::string::npos) {
          RecordPropertyTypeClass(name, ffclass);
          return name;
        }
      }
    }
  }

  static const int candidateClassOffs[] = {0x08, 0x10, 0x00, 0x18};
  static const int candidateNameOffs[] = {0x00, 0x08, 0x04, 0x10, 0x18, 0x20};

  for (int cOff : candidateClassOffs) {
    void *ffclass = SafeMem_ReadPtr((uint8_t *)ffield + cOff);
    if (!ffclass || (uintptr_t)ffclass < 0x10000 || (uintptr_t)ffclass >= 0x7FFFFFFFFFFFULL)
      continue;

    for (int nOff : candidateNameOffs) {
      FName fn = {};
      bool readOk = false;
      if (state.FName_Size == 4 || state.FName_NumberOff < 0) {
        readOk = SafeMem_Read((uint8_t *)ffclass + nOff, &fn.ComparisonIndex, 4);
        fn.Number = 0;
      } else {
        readOk = SafeMem_Read((uint8_t *)ffclass + nOff, &fn, sizeof(FName));
      }

      if (readOk) {
        std::string name = FNameToStr(state, fn);
        if (!name.empty() && name != "None" && name[0] != '<' && name.find("Property") != std::string::npos) {
          auto &mutableState = const_cast<EngineState &>(state);
          mutableState.FField_ClassOff = cOff;
          mutableState.FFieldClass_NameOff = nOff;
          RecordPropertyTypeClass(name, ffclass);
          return name;
        }
      }
    }
  }

  return "";
}

void DiscoverFPropertyOffsetInternal(EngineState &state) {
  SafeMem_SetFastMode(1);

  if (state.bUseFProperty && state.UStruct_ChildPropsOff >= 0) {
    UObject *colorStruct = nullptr;
    UObject *guidStruct = nullptr;
    const int32_t count = EngineDiscovery::GetObjectCount(state);
    for (int32_t i = 0; i < count; ++i) {
      UObject *obj = EngineDiscovery::GetObjectByIndex(state, i);
      if (!obj)
        continue;

      UClass *cls = EngineDiscovery::GetObjectClass(state, obj);
      if (!cls)
        continue;

      const std::string clsName = EngineDiscovery::GetObjectNameStr(state, cls);
      if (clsName != "ScriptStruct")
        continue;

      const std::string structName = EngineDiscovery::GetObjectNameStr(state, obj);
      if (structName == "Color" && !colorStruct)
        colorStruct = obj;
      if (structName == "Guid" && !guidStruct)
        guidStruct = obj;
      if (colorStruct && guidStruct)
        break;
    }

    struct KnownOffset {
      UObject *StructObject;
      const char *MemberName;
      int32_t ExpectedOffset;
    };

    std::vector<KnownOffset> knownMembers;
    if (colorStruct) {
      knownMembers.push_back({colorStruct, "B", 0x00});
      knownMembers.push_back({colorStruct, "G", 0x01});
    }
    if (guidStruct) {
      knownMembers.push_back({guidStruct, "C", 0x08});
      knownMembers.push_back({guidStruct, "D", 0x0C});
    }

    std::unordered_map<int32_t, int32_t> offsetVotes;
    for (const KnownOffset &known : knownMembers) {
      void *prop = EngineDiscovery::GetChildPropsHead(state, (UStruct *)known.StructObject);
      while (prop) {
        std::string propName = EngineDiscovery::GetFFieldName(state, prop);
        bool nameMatch = (propName == known.MemberName);
        if (!nameMatch && propName.size() == 1) {
          char upper = (propName[0] >= 'a' && propName[0] <= 'z')
                           ? static_cast<char>(propName[0] - ('a' - 'A'))
                           : propName[0];
          nameMatch = (upper == known.MemberName[0]);
        }

        if (nameMatch) {
          uint8_t buf[0x60] = {};
          if (SafeMem_Read(prop, buf, sizeof(buf))) {
            for (int32_t off = 0x38; off <= 0x58; off += 4) {
              int32_t value = *(int32_t *)(buf + off);
              if (value == known.ExpectedOffset) {
                offsetVotes[off]++;
              }
            }
          }
          break;
        }

        prop = EngineDiscovery::GetFFieldNext(state, prop);
      }
    }

    int32_t bestOffset = -1;
    int32_t bestVotes = 0;
    for (const auto &[off, votes] : offsetVotes) {
      if (votes > bestVotes) {
        bestOffset = off;
        bestVotes = votes;
      }
    }

    if (bestOffset >= 0 && bestVotes >= 2) {
      if (bestOffset != state.FProperty_OffsetOff) {
        if (bestVotes == static_cast<int32_t>(knownMembers.size())) {
          Log("[Intelligence] Adjusted FProperty::Offset_Internal from 0x%X to 0x%X (100%% confidence, %d/4 verified)",
              state.FProperty_OffsetOff, bestOffset, bestVotes);
        } else {
          Log("[Intelligence] Adjusted FProperty::Offset_Internal from 0x%X to 0x%X (votes=%d/%zu)",
              state.FProperty_OffsetOff, bestOffset, bestVotes, knownMembers.size());
        }
        state.FProperty_OffsetOff = bestOffset;
      } else {
        if (bestVotes == static_cast<int32_t>(knownMembers.size())) {
          Log("[Discovery] Confirmed FProperty::Offset_Internal at 0x%X (100%% confidence, %d/4 verified)",
              bestOffset, bestVotes);
        } else {
          Log("[Discovery] Confirmed FProperty::Offset_Internal at 0x%X (votes=%d/%zu)",
              bestOffset, bestVotes, knownMembers.size());
        }
      }
    }
  }

  std::unordered_map<int32_t, int32_t> boolBaseVotes;
  std::unordered_map<int32_t, int32_t> arrayDimVotes;

  std::vector<UObject*> scanTargets;
  if (state.AActorClass) scanTargets.push_back((UObject*)state.AActorClass);
  if (state.APawnClass) scanTargets.push_back((UObject*)state.APawnClass);
  if (state.UObjectClass) scanTargets.push_back((UObject*)state.UObjectClass);

  const int32_t totalObjs = EngineDiscovery::GetObjectCount(state);
  for (int32_t i = 0; i < std::min(totalObjs, 20000) && scanTargets.size() < 32; ++i) {
    UObject* obj = EngineDiscovery::GetObjectByIndex(state, i);
    if (!obj) continue;
    UClass* cls = EngineDiscovery::GetObjectClass(state, obj);
    if (cls == state.UClassClass) {
      std::string name = EngineDiscovery::GetObjectNameStr(state, obj);
      if (name == "PlayerController" || name == "Character" || name == "GameMode" ||
          name == "GameState" || name == "HUD" || name == "UserDefinedEnum" ||
          name == "ActorComponent" || name == "GameViewportClient" || name == "Pawn" ||
          name == "MovementComponent" || name == "PrimitiveComponent" || name == "SceneComponent") {
        scanTargets.push_back(obj);
      }
    }
  }

  auto getFieldOwner = [&](void* prop) -> void* {
    if (!prop) return nullptr;
    if (state.bUseFProperty) {
      return SafeMem_ReadPtr((uint8_t*)prop + 0x10);
    } else {
      return SafeMem_ReadPtr((uint8_t*)prop + state.UObject_OuterOff);
    }
  };

  int scannedProps = 0;
  int totalBoolProps = 0;
  for (UObject *target : scanTargets) {
    if (!target) continue;
    UStruct *st = (UStruct *)target;

    if (state.bUseFProperty && state.UStruct_ChildPropsOff >= 0) {
      void *prop = EngineDiscovery::GetChildPropsHead(state, st);
      while (prop && scannedProps < 2000) {
        std::string typeName = EngineDiscovery::GetFFieldTypeName(state, prop);
        uint8_t propBuf[0x100] = {};
        if (SafeMem_Read(prop, propBuf, sizeof(propBuf))) {
          if (typeName.find("BoolProperty") != std::string::npos || typeName == "BoolProperty") {
            totalBoolProps++;
            for (int off = 0x48; off <= 0x88; off++) {
              uint8_t fieldSize  = propBuf[off];
              uint8_t byteOffset = propBuf[off + 1];
              uint8_t byteMask   = propBuf[off + 2];
              uint8_t fieldMask  = propBuf[off + 3];

              if (fieldSize >= 1 && fieldSize <= 8 &&
                  byteOffset <= 7 &&
                  byteMask != 0 && (byteMask & (byteMask - 1)) == 0 &&
                  (fieldMask == 0xFF || fieldMask == byteMask || (fieldMask != 0 && (fieldMask & (fieldMask - 1)) == 0))) {
                boolBaseVotes[off]++;
              }
            }
          }

          if (state.FProperty_OffsetOff >= 0 &&
              (typeName == "IntProperty" || typeName == "FloatProperty" || typeName == "BoolProperty")) {
            int expectedElemSize = (typeName == "IntProperty" || typeName == "FloatProperty") ? 4 : 1;
            for (int off = 0x28; off <= 0x48; off += 4) {
              int32_t dim = *(int32_t *)(propBuf + off);
              int32_t elemSize = *(int32_t *)(propBuf + off + 4);
              uint64_t flags = *(uint64_t *)(propBuf + off + 8);
              if (dim == 1 && elemSize == expectedElemSize && flags != 0) {
                arrayDimVotes[off]++;
              }
            }
          }

          int32_t currentBase = (state.FBoolProperty_BaseOff > 0) ? state.FBoolProperty_BaseOff : 0x70;

          if (typeName == "ArrayProperty" && state.FArrayProperty_InnerOff < 0) {
            for (int off = currentBase; off <= 0x90; off += 8) {
              void* inner = *(void**)(propBuf + off);
              if (!inner || (uintptr_t)inner < 0x10000 || (uintptr_t)inner >= 0x7FFFFFFFFFFFULL) continue;
              std::string innerType = EngineDiscovery::GetFFieldTypeName(state, inner);
              if (innerType.find("Property") != std::string::npos) {
                if (getFieldOwner(inner) == prop) {
                  state.FArrayProperty_InnerOff = off;
                  Log("[Discovery] Found FArrayProperty::Inner at 0x%X (innerType=%s)", off, innerType.c_str());
                  break;
                }
              }
            }
          }

          if (typeName == "StructProperty" && state.FStructProperty_StructOff < 0) {
            for (int off = currentBase; off <= 0x90; off += 8) {
              void* structPtr = *(void**)(propBuf + off);
              if (!structPtr || (uintptr_t)structPtr < 0x10000 || (uintptr_t)structPtr >= 0x7FFFFFFFFFFFULL) continue;
              UClass* sCls = EngineDiscovery::GetObjectClass(state, (UObject*)structPtr);
              if (sCls) {
                std::string sClsName = EngineDiscovery::GetObjectNameStr(state, sCls);
                std::string sName = EngineDiscovery::GetObjectNameStr(state, (UObject*)structPtr);
                if (sClsName == "ScriptStruct" || sClsName == "Struct" || sClsName == "Class" ||
                    sName == "Vector" || sName == "Rotator" || sName == "Guid" || sName == "Color" || sName == "Transform") {
                  state.FStructProperty_StructOff = off;
                  Log("[Discovery] Found FStructProperty::Struct at 0x%X (-> %s)", off, sName.c_str());
                  break;
                }
              }
            }
          }

          if ((typeName == "ObjectProperty" || typeName == "ObjectPtrProperty") && state.FObjectProperty_ClassOff < 0) {
            for (int off = currentBase; off <= 0x90; off += 8) {
              void* classPtr = *(void**)(propBuf + off);
              if (!classPtr || (uintptr_t)classPtr < 0x10000 || (uintptr_t)classPtr >= 0x7FFFFFFFFFFFULL) continue;
              UClass* pcCls = EngineDiscovery::GetObjectClass(state, (UObject*)classPtr);
              if (pcCls == state.UClassClass) {
                state.FObjectProperty_ClassOff = off;
                std::string cName = EngineDiscovery::GetObjectNameStr(state, (UObject*)classPtr);
                Log("[Discovery] Found FObjectProperty::PropertyClass at 0x%X (-> %s)", off, cName.c_str());
                break;
              }
            }
          }

          if (typeName == "ClassProperty" && state.FClassProperty_MetaClassOff < 0 && state.FObjectProperty_ClassOff >= 0) {
            for (int off = currentBase; off <= 0x98; off += 8) {
              if (off == state.FObjectProperty_ClassOff) continue;
              void* metaPtr = *(void**)(propBuf + off);
              if (!metaPtr || (uintptr_t)metaPtr < 0x10000 || (uintptr_t)metaPtr >= 0x7FFFFFFFFFFFULL) continue;
              UClass* mcCls = EngineDiscovery::GetObjectClass(state, (UObject*)metaPtr);
              if (mcCls == state.UClassClass) {
                state.FClassProperty_MetaClassOff = off;
                std::string mcName = EngineDiscovery::GetObjectNameStr(state, (UObject*)metaPtr);
                Log("[Discovery] Found FClassProperty::MetaClass at 0x%X (-> %s)", off, mcName.c_str());
                break;
              }
            }
          }
        }
        scannedProps++;
        prop = EngineDiscovery::GetFFieldNext(state, prop);
      }
    } else if (state.UStruct_ChildrenOff >= 0) {
      void *prop = EngineDiscovery::GetChildrenHead(state, st);
      while (prop && scannedProps < 2000) {
        UClass *pCls = EngineDiscovery::GetObjectClass(state, (UObject *)prop);
        std::string typeName = pCls ? EngineDiscovery::GetObjectNameStr(state, (UObject *)pCls) : "";
        uint8_t propBuf[0x100] = {};
        if (SafeMem_Read(prop, propBuf, sizeof(propBuf))) {
          if (typeName.find("BoolProperty") != std::string::npos || typeName == "BoolProperty") {
            totalBoolProps++;
            for (int off = 0x48; off <= 0x88; off++) {
              uint8_t fieldSize  = propBuf[off];
              uint8_t byteOffset = propBuf[off + 1];
              uint8_t byteMask   = propBuf[off + 2];
              uint8_t fieldMask  = propBuf[off + 3];

              if (fieldSize >= 1 && fieldSize <= 8 &&
                  byteOffset <= 7 &&
                  byteMask != 0 && (byteMask & (byteMask - 1)) == 0 &&
                  (fieldMask == 0xFF || fieldMask == byteMask || (fieldMask != 0 && (fieldMask & (fieldMask - 1)) == 0))) {
                boolBaseVotes[off]++;
              }
            }
          }
        }
        scannedProps++;
        prop = EngineDiscovery::GetFieldNext(state, prop);
      }
    }
  }

  int32_t bestBoolOff = -1;
  int32_t bestBoolVotes = 0;
  for (const auto &[off, votes] : boolBaseVotes) {
    if (votes > bestBoolVotes) {
      bestBoolOff = off;
      bestBoolVotes = votes;
    }
  }

  if (bestBoolOff >= 0 && bestBoolVotes >= 1) {
    state.FBoolProperty_BaseOff = bestBoolOff;

    int confidencePct = (totalBoolProps > 0)
        ? static_cast<int>(bestBoolVotes * 100 / totalBoolProps)
        : 100;
    const char* qualLabel =
        (confidencePct == 100) ? "perfect" :
        (confidencePct >= 95)  ? "strong"  :
        (confidencePct >= 80)  ? "good"    : "partial";
    Log("[Discovery] Found FBoolProperty base at 0x%X "
        "(%d/%d bool-props agree = %d%% %s)",
        bestBoolOff, bestBoolVotes, totalBoolProps,
        confidencePct, qualLabel);
  } else {
    int defaultBase = (state.FProperty_OffsetOff == 0x44) ? 0x68 : 0x70;
    state.FBoolProperty_BaseOff = defaultBase;
    Log("[Discovery] FBoolProperty base fallback to 0x%X", defaultBase);
  }

  int32_t bestDimOff = -1;
  int32_t bestDimVotes = 0;
  for (const auto &[off, votes] : arrayDimVotes) {
    if (votes > bestDimVotes) {
      bestDimOff = off;
      bestDimVotes = votes;
    }
  }

  if (bestDimOff >= 0 && bestDimVotes >= 1) {
    state.FProperty_ArrayDimOff = bestDimOff;
    state.FProperty_ElementSizeOff = bestDimOff + 4;
    state.FProperty_PropertyFlagsOff = bestDimOff + 8;
    Log("[Discovery] FProperty ArrayDim at 0x%X, ElementSize at 0x%X, PropertyFlags at 0x%X",
        state.FProperty_ArrayDimOff, state.FProperty_ElementSizeOff, state.FProperty_PropertyFlagsOff);
  } else {
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

  int32_t propBaseEnd = state.FBoolProperty_BaseOff >= 0 ? state.FBoolProperty_BaseOff : 0x70;
  if (state.FArrayProperty_InnerOff < 0) {

    if ((state.UEMajor == 5 && state.UEMinor >= 3) || state.UEMajor > 5) {
      state.FArrayProperty_InnerOff = propBaseEnd + 8;
    } else {
      state.FArrayProperty_InnerOff = propBaseEnd;
    }
    Log("[Discovery] FArrayProperty_InnerOff set to fallback: 0x%X (UE%d.%d)",
        state.FArrayProperty_InnerOff, state.UEMajor, state.UEMinor);
  }
  if (state.FStructProperty_StructOff < 0)       state.FStructProperty_StructOff = propBaseEnd;
  if (state.FObjectProperty_ClassOff < 0)        state.FObjectProperty_ClassOff = propBaseEnd;
  if (state.FClassProperty_MetaClassOff < 0 && state.FObjectProperty_ClassOff >= 0)
    state.FClassProperty_MetaClassOff = state.FObjectProperty_ClassOff + 8;
  if (state.FEnumProperty_UnderlyingOff < 0)     state.FEnumProperty_UnderlyingOff = propBaseEnd;
  if (state.FEnumProperty_EnumOff < 0)           state.FEnumProperty_EnumOff = propBaseEnd + 8;
  if (state.FMapProperty_KeyPropOff < 0)        { state.FMapProperty_KeyPropOff = propBaseEnd; state.FMapProperty_ValuePropOff = propBaseEnd + 8; }
  if (state.FSetProperty_ElementPropOff < 0)    state.FSetProperty_ElementPropOff = propBaseEnd;
  if (state.FOptionalProperty_ValuePropOff < 0) state.FOptionalProperty_ValuePropOff = propBaseEnd;
  if (state.FByteProperty_EnumOff < 0)           state.FByteProperty_EnumOff = propBaseEnd;

  SafeMem_SetFastMode(0);
}

struct LearnedAlignment {
  int32_t Alignment = 0;
  bool bNarrowedByLayout = false;
};

static std::unordered_map<std::string, LearnedAlignment> s_ClassAlignment;
static std::unordered_map<void *, int32_t> s_StructAlignment;
static std::unordered_map<std::string, bool> s_AlignmentFallbackReported;
static std::mutex s_AlignmentMutex;
static std::mutex s_AlignmentBuildMutex;
static bool s_AlignmentEvidenceBuilt = false;

static const int32_t kAlignmentEvidenceBudget = 150000;

static const int32_t kAlignmentCeiling = static_cast<int32_t>(alignof(std::max_align_t));

static int32_t AlignmentLowBit(int32_t value) {
  if (value <= 0)
    return 0;
  return value & (-value);
}

static int32_t AlignmentClamp(int32_t value) {
  if (value < 1)
    return 1;
  if (value > kAlignmentCeiling)
    return kAlignmentCeiling;
  return value;
}

static int32_t ReadPropertyScalar(const EngineState &state, void *property,
                                  int32_t fieldOffset, int32_t fallback) {
  int32_t value = fallback;
  if (!property || fieldOffset < 0)
    return fallback;
  if (!SafeMem_Read(reinterpret_cast<uint8_t *>(property) + fieldOffset, &value,
                    sizeof(value)))
    return fallback;
  return value;
}

static bool IsStructFamilyClass(const EngineState &state, UClass *cls) {
  if (!cls || !state.UStructClass || state.UStruct_SuperOff < 0)
    return false;
  for (int32_t depth = 0; cls && depth < 64; ++depth) {
    if (cls == state.UStructClass)
      return true;
    if (cls == state.UObjectClass)
      return false;
    cls = reinterpret_cast<UClass *>(SafeMem_ReadPtr(
        reinterpret_cast<uint8_t *>(cls) + state.UStruct_SuperOff));
  }
  return false;
}

struct AlignmentEvidenceWalk {
  const EngineState *State = nullptr;
  int32_t StructsVisited = 0;
  int32_t PropertiesVisited = 0;
  bool bTruncated = false;
};

static void ObservePropertyPlacement(AlignmentEvidenceWalk &walk, void *property) {
  const EngineState &state = *walk.State;
  const std::string typeName = GetFFieldTypeName(state, property);
  if (typeName.empty() || typeName.find("Property") == std::string::npos)
    return;

  const int32_t size = ReadPropertyScalar(state, property, state.FProperty_ElementSizeOff, 0);
  if (size <= 0 || size > (1 << 20))
    return;

  if (state.FStructProperty_StructOff >= 0 &&
      typeName.find("StructProperty") != std::string::npos)
    return;

  const int32_t offsetField = state.bUseFProperty ? state.FProperty_OffsetOff
                                                  : state.UProperty_OffsetOff;
  const int32_t offset = ReadPropertyScalar(state, property, offsetField, 0);

  const int32_t sizeExtent = AlignmentClamp(AlignmentLowBit(size));
  int32_t evidence = sizeExtent;
  bool narrowed = false;
  if (offset > 0) {
    const int32_t offsetExtent = AlignmentClamp(AlignmentLowBit(offset));
    if (offsetExtent < evidence) {
      evidence = offsetExtent;
      narrowed = true;
    }
  }
  if (evidence < 1)
    return;

  std::lock_guard<std::mutex> lock(s_AlignmentMutex);
  LearnedAlignment &learned = s_ClassAlignment[typeName];
  if (learned.Alignment == 0 || evidence < learned.Alignment)
    learned.Alignment = evidence;
  if (narrowed)
    learned.bNarrowedByLayout = true;
}

static bool CollectAlignmentEvidence(UObject *object, int32_t, void *userdata) {
  auto *walk = static_cast<AlignmentEvidenceWalk *>(userdata);
  const EngineState &state = *walk->State;
  if (!object)
    return true;
  if (!IsStructFamilyClass(state, GetObjectClass(state, object)))
    return true;

  UStruct *asStruct = reinterpret_cast<UStruct *>(object);
  walk->StructsVisited++;

  if (state.bUseFProperty && state.UStruct_ChildPropsOff >= 0) {
    for (void *property = GetChildPropsHead(state, asStruct); property;
         property = GetFFieldNext(state, property)) {
      ObservePropertyPlacement(*walk, property);
      if (++walk->PropertiesVisited >= kAlignmentEvidenceBudget) {
        walk->bTruncated = true;
        return false;
      }
    }
    return true;
  }

  for (void *field = GetChildrenHead(state, asStruct); field;
       field = GetFieldNext(state, field)) {
    ObservePropertyPlacement(*walk, field);
    if (++walk->PropertiesVisited >= kAlignmentEvidenceBudget) {
      walk->bTruncated = true;
      return false;
    }
  }
  return true;
}

static void BuildAlignmentEvidence(const EngineState &state) {
  std::lock_guard<std::mutex> build(s_AlignmentBuildMutex);
  if (s_AlignmentEvidenceBuilt)
    return;

  if (state.FProperty_ElementSizeOff < 0 || !state.UStructClass ||
      state.UStruct_SuperOff < 0)
    return;

  AlignmentEvidenceWalk walk;
  walk.State = &state;
  const int32_t objects = ForEachObject(state, CollectAlignmentEvidence, &walk);

  size_t classes = 0;
  {
    std::lock_guard<std::mutex> lock(s_AlignmentMutex);
    classes = s_ClassAlignment.size();
  }
  Log("[Alignment] Derived %zu property-class alignments from %d structures / %d "
      "properties (%d objects%s)",
      classes, walk.StructsVisited, walk.PropertiesVisited, objects,
      walk.bTruncated ? ", walk budget reached" : "");

  int32_t reported = 0;
  {
    std::lock_guard<std::mutex> lock(s_AlignmentMutex);
    for (const auto &entry : s_ClassAlignment) {
      if (entry.second.bNarrowedByLayout || reported >= 16)
        continue;
      Log("[Alignment]   %s stayed at %d: no placement narrowed it",
          entry.first.c_str(), entry.second.Alignment);
      ++reported;
    }
  }
  s_AlignmentEvidenceBuilt = true;
}

static int32_t ResolvePropertyAlignment(const EngineState &state, void *property,
                                        int32_t depth);

static int32_t ResolveStructAlignment(const EngineState &state, void *ustruct,
                                     int32_t depth) {
  if (!ustruct || depth > 8)
    return 1;

  {
    std::lock_guard<std::mutex> lock(s_AlignmentMutex);
    auto it = s_StructAlignment.find(ustruct);
    if (it != s_StructAlignment.end())
      return it->second;
  }

  int32_t alignment = 1;
  UStruct *super = GetSuperStruct(state, reinterpret_cast<UStruct *>(ustruct));
  if (super)
    alignment = ResolveStructAlignment(state, super, depth + 1);

  int32_t resolved = alignment;
  if (state.bUseFProperty && state.UStruct_ChildPropsOff >= 0) {
    for (void *property = GetChildPropsHead(state, reinterpret_cast<UStruct *>(ustruct));
         property; property = GetFFieldNext(state, property)) {
      const int32_t member = ResolvePropertyAlignment(state, property, depth + 1);
      if (member > resolved)
        resolved = member;
    }
  } else {
    for (void *field = GetChildrenHead(state, reinterpret_cast<UStruct *>(ustruct)); field;
         field = GetFieldNext(state, field)) {
      const int32_t member = ResolvePropertyAlignment(state, field, depth + 1);
      if (member > resolved)
        resolved = member;
    }
  }

  resolved = AlignmentClamp(resolved);
  {
    std::lock_guard<std::mutex> lock(s_AlignmentMutex);
    s_StructAlignment[ustruct] = resolved;
  }
  return resolved;
}

static int32_t ResolvePropertyAlignment(const EngineState &state, void *property,
                                        int32_t depth) {
  if (!property || depth > 8)
    return 1;

  const std::string typeName = GetFFieldTypeName(state, property);

  if (typeName.find("StructProperty") != std::string::npos &&
      state.FStructProperty_StructOff >= 0) {
    void *structPtr = SafeMem_ReadPtr(
        reinterpret_cast<uint8_t *>(property) + state.FStructProperty_StructOff);
    if (structPtr)
      return ResolveStructAlignment(state, structPtr, depth + 1);
  }

  if (typeName.find("BoolProperty") != std::string::npos) {

    const int32_t fieldSize = ReadPropertyScalar(state, property, state.FProperty_ElementSizeOff, 0);
    if (fieldSize > 0 && fieldSize <= kAlignmentCeiling)
      return fieldSize;
  }

  if (typeName.find("EnumProperty") != std::string::npos &&
      state.FEnumProperty_UnderlyingOff >= 0) {
    void *underlying = SafeMem_ReadPtr(
        reinterpret_cast<uint8_t *>(property) + state.FEnumProperty_UnderlyingOff);
    if (underlying)
      return ResolvePropertyAlignment(state, underlying, depth + 1);
  }

  std::lock_guard<std::mutex> lock(s_AlignmentMutex);
  auto it = s_ClassAlignment.find(typeName);
  if (it != s_ClassAlignment.end() && it->second.Alignment > 0)
    return it->second.Alignment;
  return 0;
}

int32_t GetPropertyAlignment(const EngineState &state, void *property,
                            int32_t valueSize) {

  const int32_t sizedGuess =
      AlignmentClamp(AlignmentLowBit(valueSize > 0 ? valueSize : static_cast<int32_t>(sizeof(void *))));

  if (!property)
    return sizedGuess;

  BuildAlignmentEvidence(state);
  const int32_t learned = ResolvePropertyAlignment(state, property, 0);
  if (learned > 0)
    return AlignmentClamp(learned);

  const std::string typeName = GetFFieldTypeName(state, property);
  {
    std::lock_guard<std::mutex> lock(s_AlignmentMutex);
    if (typeName.empty() || s_AlignmentFallbackReported.find(typeName) != s_AlignmentFallbackReported.end())
      return sizedGuess;
    s_AlignmentFallbackReported[typeName] = true;
  }
  Log("[Alignment] No placement evidence for %s: using %d for a %d-byte value",
      typeName.empty() ? "<unnamed property>" : typeName.c_str(), sizedGuess,
      valueSize);
  return sizedGuess;
}

namespace {

struct PropertiesSizeProbe {
  const EngineState *state = nullptr;
  UObject *colorStruct = nullptr;
  UObject *guidStruct = nullptr;
};

}

bool ResolveUStructPropertiesSizeOffset(EngineState &state) {
  if (state.UStruct_PropertiesSizeOff >= 0) return true;

  PropertiesSizeProbe probe;
  probe.state = &state;
  ForEachObject(state,
                [](UObject *obj, int32_t idx, void *ud) -> bool {
                  auto *p = (PropertiesSizeProbe *)ud;
                  if (p->colorStruct && p->guidStruct) return false;
                  UClass *cls = GetObjectClass(*p->state, obj);
                  if (!cls) return true;
                  if (GetObjectNameStr(*p->state, (UObject *)cls) != "ScriptStruct")
                    return true;
                  std::string sName = GetObjectNameStr(*p->state, obj);
                  if (sName == "Color" && !p->colorStruct) p->colorStruct = obj;
                  else if (sName == "Guid" && !p->guidStruct) p->guidStruct = obj;
                  return true;
                },
                &probe);

  if (probe.colorStruct && probe.guidStruct) {
    for (int off = 0x30; off <= 0x70; off += 4) {
      int32_t colorSize = 0, guidSize = 0;
      if (!SafeMem_Read((uint8_t *)probe.colorStruct + off, &colorSize, sizeof(int32_t)))
        continue;
      if (!SafeMem_Read((uint8_t *)probe.guidStruct + off, &guidSize, sizeof(int32_t)))
        continue;
      if (colorSize == 4 && guidSize == 16) {
        state.UStruct_PropertiesSizeOff = off;
        Log("[Discovery] UStruct::PropertiesSize at 0x%X (confirmed: FColor=4, FGuid=16)",
            off);
        return true;
      }
    }
    Log("[Discovery] FColor/FGuid found but no offset in 0x30..0xX70 reported "
        "4 and 16; falling back to the declared member order.");
  }

  const int32_t ptr = static_cast<int32_t>(sizeof(void *));
  int32_t base = -1;
  const char *after = nullptr;
  if (state.bUseFProperty && state.UStruct_ChildPropsOff >= 0) {
    base = state.UStruct_ChildPropsOff;
    after = "ChildProperties";
  } else if (state.UStruct_ChildrenOff >= 0) {
    base = state.UStruct_ChildrenOff;
    after = "Children";
  }

  if (base >= 0) {
    state.UStruct_PropertiesSizeOff = base + ptr;
    Log("[Discovery] UStruct::PropertiesSize at 0x%X (UStruct::%s + %d, from the "
        "member order in Public/UObject/Class.h)",
        base + ptr, after, ptr);
    return true;
  }

  Log("[Discovery] UStruct::PropertiesSize unresolved: neither FColor/FGuid nor a "
      "pointer member of UStruct was available. Sizes will read as 0.");
  return false;
}

int32_t UStructPropertiesSizeOffset(const EngineState &state) {
  if (state.UStruct_PropertiesSizeOff >= 0) return state.UStruct_PropertiesSizeOff;

  const int32_t ptr = static_cast<int32_t>(sizeof(void *));
  if (state.bUseFProperty && state.UStruct_ChildPropsOff >= 0)
    return state.UStruct_ChildPropsOff + ptr;
  if (state.UStruct_ChildrenOff >= 0) return state.UStruct_ChildrenOff + ptr;
  return -1;
}

namespace {

bool IsPlausibleMinAlignmentValue(int32_t value) {
  return value == 1 || value == 2 || value == 4 || value == 8 || value == 16 ||
         value == 32;
}

}

bool ResolveUStructMinAlignmentOffset(EngineState &state) {
  if (state.UStruct_MinAlignmentOff >= 0) return true;

  const int32_t sizeOff = UStructPropertiesSizeOffset(state);
  if (sizeOff < 0) {
    Log("[Discovery] UStruct::MinAlignment unresolved: PropertiesSize is not "
        "known yet. Alignment-dependent output will be skipped.");
    return false;
  }

  state.UStruct_MinAlignmentOff = sizeOff + 4;
  Log("[Discovery] UStruct::MinAlignment at 0x%X (declared after PropertiesSize; "
      "int32 through UE5.5, int16 from UE5.6, both read as 16 bits)",
      state.UStruct_MinAlignmentOff);
  return true;
}

void ApplyFFieldLayoutFixups(EngineState &state) {

  if ((state.UEMajor == 4 && state.UEMinor >= 25) || state.UEMajor >= 5) {
    state.bUseFProperty = true;
    Log("[Intelligence] UE4.25+ detected: bUseFProperty=true");
  } else {
    state.bUseFProperty = false;
    Log("[Intelligence] Pre-4.25 detected: bUseFProperty=false");

    if (state.UStruct_ChildrenOff >= 0)
      state.UStruct_ChildPropsOff = state.UStruct_ChildrenOff;
    if (state.UField_NextOff >= 0)
      state.FField_NextOff = state.UField_NextOff;
    state.FField_NameOff = state.UObject_NameOff;

    if (state.UProperty_OffsetOff >= 0)
      state.FProperty_OffsetOff = state.UProperty_OffsetOff;
  }

  if (state.bUseCasePreservingName && state.FField_NameOff > 0) {
    Log("[Discovery] CasePreservingName: FName size=%d (FField offsets already discovered dynamically)",
        state.FName_Size);
  }

  if (state.bUseFProperty) {
    if (state.FField_NextOff == 0x18) {
      state.bUseMaskForFieldOwner = true;
      Log("[Intelligence] UE5.1+ compact FFieldVariant owner detected (NextOff=0x18)");
    } else {
      state.bUseMaskForFieldOwner = false;
      Log("[Discovery] Classic FFieldVariant layout detected (NextOff=0x%X, separate bool member)", state.FField_NextOff);
    }
  }

  ResolveUStructPropertiesSizeOffset(state);

  ResolveUStructMinAlignmentOffset(state);

  ResolveUClassDefaultObjectOffset(state);
}

namespace {

struct CDOProbe {
  const EngineState *state = nullptr;
  int32_t candidateOffset = -1;
  int32_t inspected = 0;
  int32_t agreeing = 0;
};

bool InspectClassCDO(UObject *obj, int32_t, void *userdata) {
  auto *probe = static_cast<CDOProbe *>(userdata);
  const EngineState &state = *probe->state;

  if (!obj) return true;
  UClass *cls = EngineDiscovery::GetObjectClass(state, obj);
  if (!cls || cls != state.UClassClass) return true;

  uintptr_t cdoPtr = 0;
  if (!SafeMem_Read(reinterpret_cast<uint8_t *>(obj) + probe->candidateOffset,
                    &cdoPtr, sizeof(cdoPtr)))
    return true;

  if (!cdoPtr || cdoPtr < 0x10000 || cdoPtr >= 0x7FFFFFFFFFFFULL)
    return true;

  probe->inspected++;

  UClass *cdoClass = EngineDiscovery::GetObjectClass(state, reinterpret_cast<UObject *>(cdoPtr));
  if (cdoClass != reinterpret_cast<UClass *>(obj))
    return true;

  if (state.UObject_FlagsOff >= 0) {
    uint32_t flags = 0;
    if (SafeMem_Read(reinterpret_cast<uint8_t *>(cdoPtr) + state.UObject_FlagsOff,
                     &flags, sizeof(flags))) {
      if ((flags & 0x00000010u) == 0) return true;
    }
  }

  std::string cdoName = EngineDiscovery::GetObjectNameStr(state, reinterpret_cast<UObject *>(cdoPtr));
  if (!cdoName.empty() && cdoName.find("Default__") != 0) {
    return true;
  }

  probe->agreeing++;
  return probe->inspected < 128;
}

}

bool ResolveUClassDefaultObjectOffset(EngineState &state) {
  if (state.UClass_ClassDefaultObjectOff >= 0) return true;

  if (!state.UClassClass || state.UObject_ClassOff < 0) {
    return false;
  }

  const int32_t candidates[] = {
      0x118, 0x110, 0xF8, 0x100, 0x108, 0x120, 0x128, 0x130, 0xE8, 0xF0
  };

  int32_t bestOffset = -1;
  int32_t bestInspected = 0;
  int32_t bestAgreeing = 0;

  for (int32_t candidate : candidates) {
    CDOProbe probe;
    probe.state = &state;
    probe.candidateOffset = candidate;

    ForEachObject(state, InspectClassCDO, &probe);

    if (probe.inspected >= 16 && probe.agreeing == probe.inspected) {
      if (probe.inspected > bestInspected) {
        bestOffset = candidate;
        bestInspected = probe.inspected;
        bestAgreeing = probe.agreeing;
      }
      if (probe.inspected >= 64) break;
    }
  }

  if (bestOffset >= 0) {
    state.UClass_ClassDefaultObjectOff = bestOffset;
    Log("[Discovery] Found UClass::ClassDefaultObject at 0x%X (%d of %d classes agree)",
        bestOffset, bestAgreeing, bestInspected);
    return true;
  }

  int32_t fallback = -1;
  if (state.UEMajor >= 5) {
    fallback = 0x110;
  } else if (state.UEMinor >= 25) {
    fallback = 0x118;
  } else if (state.UEMinor >= 23) {
    fallback = 0xF8;
  }

  if (fallback >= 0) {
    state.UClass_ClassDefaultObjectOff = fallback;
    Log("[Discovery] Found UClass::ClassDefaultObject at 0x%X (version fallback UE%d.%d)",
        fallback, state.UEMajor, state.UEMinor);
    return true;
  }

  Log("[Discovery] UClass::ClassDefaultObject unresolved");
  return false;
}

}
