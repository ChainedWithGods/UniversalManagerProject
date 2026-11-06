#include "OffsetDiscovery.h"
#include "Logger.h"
#include "ObjectArray.h"
#include "NameResolution.h"
#include "ObjectUtils.h"
#include "../utils/SafeMemory.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdarg>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <psapi.h>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace EngineDiscovery {

static bool IsIdentifierishFieldName(const std::string &name) {
  if (name.empty() || name.size() > 256 || name == "None" || name[0] == '<')
    return false;
  bool sawLetterOrDigit = false;
  for (char c : name) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (std::isalnum(u)) {
      sawLetterOrDigit = true;
      continue;
    }
    if (c == '_' || c == '.')
      continue;
    return false;
  }
  return sawLetterOrDigit;
}

bool DiscoverUObjectFlagsOffset(EngineState &state) {

  int32_t count = GetObjectCount(state);
  if (count < 100)
    return false;

  constexpr uint32_t CommonFlagValue =
      0x43;
  constexpr int32_t MinOccurrences =
      100;

  constexpr int32_t kFirstOff = 0x04;
  constexpr int32_t kLastOff  = 0x40;
  constexpr int32_t kStep     = 0x04;
  constexpr int     kSlots    = (kLastOff - kFirstOff) / kStep + 1;
  int32_t hits[kSlots] = {};

  void *objectsPtr = SafeMem_ReadPtr((uint8_t *)state.GUObjectArray + state.TUObjectArray_ObjectsOff);
  void *chunk0 = objectsPtr ? SafeMem_ReadPtr(objectsPtr) : nullptr;
  const int32_t itemSize = state.UObjectArray_ItemSize;
  const int32_t objOff = state.FUObjectItem_ObjectOff;

  for (int i = 0; i < count; i++) {
    UObject *obj = nullptr;
    if (chunk0 && i < 65536) {
      obj = reinterpret_cast<UObject *>(SafeMem_ReadPtr((uint8_t *)chunk0 + i * itemSize + objOff));
    } else {
      obj = GetObjectByIndex(state, i);
    }
    if (!obj || reinterpret_cast<uintptr_t>(obj) < 0x10000)
      continue;

    uint8_t block[kLastOff + 4];
    if (!SafeMem_Read(obj, block, sizeof(block)))
      continue;

    for (int s = 0; s < kSlots; s++) {
      int32_t off = kFirstOff + s * kStep;
      uint32_t value = *(uint32_t *)(block + off);
      if (value == CommonFlagValue)
        hits[s]++;
    }

    if (i >= 150 && (i % 50 == 0 || i == count - 1)) {
      int32_t leaderHits = 0, runnerHits = 0;
      for (int s = 0; s < kSlots; s++) {
        if (hits[s] > leaderHits) {
          runnerHits = leaderHits;
          leaderHits = hits[s];
        } else if (hits[s] > runnerHits) {
          runnerHits = hits[s];
        }
      }
      if (leaderHits >= MinOccurrences && leaderHits > runnerHits * 4) {
        break;
      }
    }
  }

  int    bestSlot  = -1;
  int32_t bestHits = MinOccurrences - 1;
  for (int s = 0; s < kSlots; s++) {
    if (hits[s] > bestHits) {
      bestHits = hits[s];
      bestSlot = s;
    }
  }

  if (bestSlot >= 0) {
    state.UObject_FlagsOff = kFirstOff + bestSlot * kStep;
    Log("[Discovery] Found UObject::Flags at 0x%X (occurrences=%d)",
        state.UObject_FlagsOff, bestHits);
    return true;
  }

  state.UObject_FlagsOff = 0x08;
  Log("[Discovery] UObject::Flags fallback to 0x08");
  return true;
}

bool DiscoverUObjectLayout(EngineState &state) {

  SafeMem_SetFastMode(1);

  int32_t count = GetObjectCount(state);
  if (count < 10)
    return false;

  DiscoverUObjectFlagsOffset(state);

  int validCount = 0;
  for (int i = 0; i < std::min(count, 100); i++) {
    UObject *obj = GetObjectByIndex(state, i);
    if (!obj)
      continue;

    void *cls = SafeMem_ReadPtr((uint8_t *)obj + 0x10);
    if (cls && (uintptr_t)cls > 0x10000 &&
        (uintptr_t)cls < 0xFFFF000000000000ULL) {

      void *vtbl = SafeMem_ReadPtr(cls);
      if (vtbl && (uintptr_t)vtbl > 0x10000) {
        validCount++;
      }
    }
  }

  if (validCount >= 5) {
    state.UObject_ClassOff = 0x10;
    state.UObject_NameOff = 0x18;
    state.UObject_OuterOff = 0x20;
    Log("[Discovery] UObject layout verified (standard): Flags=0x%X Class=0x10 "
        "Name=0x18 Outer=0x20",
        state.UObject_FlagsOff);
    SafeMem_SetFastMode(0);
    return true;
  }

  SafeMem_SetFastMode(0);
  Log("[Discovery] Failed to verify UObject layout");
  return false;
}

static bool NextOffsetYieldsSameOwner(const EngineState &state, void *head,
                                      void *ownerStruct, int32_t nextOff,
                                      std::string *nameOut) {
  if (!head || !ownerStruct || nextOff < 0)
    return false;

  void *next = SafeMem_ReadPtr((uint8_t *)head + nextOff);
  if (!next || (uintptr_t)next < 0x10000 ||
      (uintptr_t)next >= 0x7FFFFFFFFFFFULL)
    return false;

  void *nextCls = SafeMem_ReadPtr((uint8_t *)next + state.UObject_ClassOff);
  if (!nextCls || (uintptr_t)nextCls < 0x10000 ||
      (uintptr_t)nextCls >= 0x7FFFFFFFFFFFULL)
    return false;

  if (nextCls != state.UFunctionClass && nextCls != state.UClassClass &&
      nextCls != state.UStructClass && nextCls != state.UEnumClass) {

    FName clsFn;
    if (!SafeMem_Read((uint8_t *)nextCls + state.UObject_NameOff, &clsFn,
                      sizeof(FName)))
      return false;
    if (FNameToStr(state, clsFn).find("Property") == std::string::npos)
      return false;
  }

  if (state.UObject_OuterOff >= 0 &&
      SafeMem_ReadPtr((uint8_t *)next + state.UObject_OuterOff) != ownerStruct)
    return false;

  FName nextName;
  if (!SafeMem_Read((uint8_t *)next + state.UObject_NameOff, &nextName,
                    sizeof(FName)))
    return false;
  std::string nname = FNameToStr(state, nextName);
  if (nname.empty() || nname == "None" || nname[0] == '<')
    return false;
  if (!IsIdentifierishFieldName(nname))
    return false;

  if (nameOut)
    *nameOut = nname;
  return true;
}

bool DiscoverUStructLayout(EngineState &state) {
  SafeMem_SetFastMode(1);
#pragma region debug-point backrooms-init-crash-ustruct
  Log("[Discovery][Debug] UStruct layout: begin (count=%d SuperOff=0x%X "
      "ChildrenOff=0x%X ChildPropsOff=0x%X)",
      GetObjectCount(state), state.UStruct_SuperOff, state.UStruct_ChildrenOff,
      state.UStruct_ChildPropsOff);
#pragma endregion

  int32_t count = GetObjectCount(state);
  if (count < 10) {
    SafeMem_SetFastMode(0);
    return false;
  }

  UClass *classClass = state.UClassClass;
  for (int pass = 0; pass < 2 && !classClass; pass++) {
#pragma region debug-point backrooms-init-crash-ustruct
    Log("[Discovery][Debug] UStruct layout: metaclass scan pass=%d", pass);
#pragma endregion

    for (int32_t i = 0; i < count && i < 100000; i++) {
      UObject *obj = GetObjectByIndex(state, i);
      if (!obj)
        continue;
      UClass *cls = GetObjectClass(state, obj);

      bool match = false;
      if (pass == 0)
        match = (cls == obj);
      else
        match = true;

      if (match) {
        std::string name = GetObjectNameStr(state, obj);
        if (name == "Class" || name == "UClass" || name == "ClassPrivate") {
          classClass = obj;
          state.UClassClass = (UClass *)classClass;
          Log("[Discovery] Found 'Class' metaclass at index %d: %p (name: %s)",
              i, obj, name.c_str());
          break;
        }
      }
    }
  }

  if (!classClass) {
    Log("[Intelligence] Attempting auto-correction for core class discovery...");
    return false;
  }
#pragma region debug-point backrooms-init-crash-ustruct
  Log("[Discovery][Debug] UStruct layout: class metaclass=%p", classClass);
#pragma endregion

  if (state.UStruct_SuperOff < 0) {
#pragma region debug-point backrooms-init-crash-ustruct
    Log("[Discovery][Debug] UStruct layout: scanning SuperStruct on class=%p",
        classClass);
#pragma endregion
    uint8_t classBuf[0x100];
    if (!SafeMem_Read(classClass, classBuf, sizeof(classBuf)))
      return false;

    for (int off = 0x28; off <= 0x80; off += 8) {
      void *superCandidate = *(void **)(classBuf + off);
      if (!superCandidate || (uintptr_t)superCandidate < 0x10000)
        continue;
      if ((uintptr_t)superCandidate >= 0xFFFF000000000000ULL)
        continue;

      if (!IsReadable(superCandidate, state.UObject_ClassOff + (int)sizeof(void *)))
        continue;

      void *superCls = nullptr;
      if (!SafeMem_Read((uint8_t *)superCandidate + state.UObject_ClassOff,
                        &superCls, sizeof(superCls))) {
        continue;
      }
      if (superCls == classClass) {

        std::string superName =
            GetObjectNameStr(state, (UObject *)superCandidate);
        if (!superName.empty() && superName != "<invalid>" &&
            superName != "<unknown>") {
          state.UStruct_SuperOff = off;
          if (superName == "<unresolved>") {
            Log("[Discovery] Found SuperStruct offset: 0x%X", off);
          } else {
            Log("[Discovery] Found SuperStruct offset: 0x%X (super='%s')", off,
                superName.c_str());
          }
          break;
        }
      }
    }
  }
#pragma region debug-point backrooms-init-crash-ustruct
  Log("[Discovery][Debug] UStruct layout: after SuperStruct scan "
      "(SuperOff=0x%X)",
      state.UStruct_SuperOff);
#pragma endregion

  if (state.UClassClass && state.UStruct_SuperOff >= 0) {
    UClass *fromClass = reinterpret_cast<UClass *>(SafeMem_ReadPtr(
        reinterpret_cast<uint8_t *>(state.UClassClass) + state.UStruct_SuperOff));
    UClass *fromFunction =
        state.UFunctionClass
            ? reinterpret_cast<UClass *>(SafeMem_ReadPtr(
                  reinterpret_cast<uint8_t *>(state.UFunctionClass) + state.UStruct_SuperOff))
            : nullptr;

    const bool fromClassIsAClass =
        fromClass && GetObjectClass(state, reinterpret_cast<UObject *>(fromClass)) ==
                         state.UClassClass;
    const bool agree = fromClassIsAClass && (!fromFunction || fromFunction == fromClass);

    if (agree) {
      if (state.UStructClass && state.UStructClass != fromClass) {
        Log("[Discovery] UStruct corrected: UClass and UFunction both derive from '%s' (%p), "
            "while the core-class name search had resolved '%s' (%p)",
            GetObjectNameStr(state, (UObject *)fromClass).c_str(), fromClass,
            GetObjectNameStr(state, (UObject *)state.UStructClass).c_str(),
            state.UStructClass);
      } else if (!state.UStructClass) {
        Log("[Discovery] UStruct resolved from UClass's own super field: '%s' (%p)",
            GetObjectNameStr(state, (UObject *)fromClass).c_str(), fromClass);
      }
      state.UStructClass = fromClass;
    } else if (fromClass && fromFunction && fromFunction != fromClass) {
      Log("[Discovery] UStruct left as resolved by name: UClass's super (%p) and "
          "UFunction's super (%p) disagree", fromClass, fromFunction);
    }
  }

  UObject *candidates[] = {
      (UObject *)state.AActorClass,    (UObject *)state.APawnClass,
      (UObject *)state.UObjectClass,   classClass,
      (UObject *)state.UFunctionClass,
  };

  for (auto *candidate : candidates) {
    if (!candidate)
      continue;
#pragma region debug-point backrooms-init-crash-ustruct
    Log("[Discovery][Debug] UStruct layout: candidate=%p "
        "(ChildrenOff=0x%X ChildPropsOff=0x%X)",
        candidate, state.UStruct_ChildrenOff, state.UStruct_ChildPropsOff);
#pragma endregion
    if (state.UStruct_ChildrenOff >= 0 && state.UStruct_ChildPropsOff >= 0)
      break;

    uint8_t candBuf[0x100];
    if (!SafeMem_Read(candidate, candBuf, sizeof(candBuf)))
      continue;

    for (int off = state.UStruct_SuperOff + 8; off <= 0xA0; off += 8) {
      void *children = *(void **)(candBuf + off);
      if (!children)
        continue;
      if ((uintptr_t)children < 0x10000 ||
          (uintptr_t)children >= 0x7FFFFFFFFFFFULL)
        continue;
#pragma region debug-point backrooms-init-crash-ustruct
      Log("[Discovery][Debug] UStruct layout: probing candidate=%p off=0x%X "
          "children=%p",
          candidate, off, children);
#pragma endregion

      uint8_t childBuf[0x40];
      if (!SafeMem_Read(children, childBuf, sizeof(childBuf)))
        continue;

      void *childVtbl = *(void **)childBuf;
#pragma region debug-point backrooms-init-crash-ustruct
      Log("[Discovery][Debug] UStruct layout: child header candidate=%p "
          "off=0x%X vtbl=%p",
          candidate, off, childVtbl);
#pragma endregion

      bool isUField = false;
      bool isFField = false;

      void *childCls = *(void **)(childBuf + 0x10);
      if (childCls && (uintptr_t)childCls > 0x10000 &&
          (uintptr_t)childCls < 0x7FFFFFFFFFFFULL) {

        if (childCls == state.UFunctionClass || childCls == state.UClassClass ||
            childCls == state.UStructClass || childCls == state.UEnumClass) {
          isUField = true;
        } else {

          FName clsName;
          if (SafeMem_Read((uint8_t *)childCls + state.UObject_NameOff,
                           &clsName, sizeof(FName))) {
            std::string cn = FNameToStr(state, clsName);
            if (!cn.empty() && cn.find("Property") != std::string::npos) {
              isUField = true;
            }
          }
        }
      }

      if (!isUField) {
#pragma region debug-point backrooms-init-crash-ustruct
        Log("[Discovery][Debug] UStruct layout: entering FField probe "
            "candidate=%p off=0x%X child=%p",
            candidate, off, children);
#pragma endregion

        for (int ownerOff = 0x10; ownerOff <= 0x28; ownerOff += 8) {
          void *owner = *(void **)(childBuf + ownerOff);
          if (owner == candidate) {
            Log("[Discovery] FField candidate at off=0x%X: owner found at 0x%X",
                off, ownerOff);

            for (int nameOff = 0x18; nameOff <= 0x40; nameOff += 8) {
              if (nameOff == ownerOff)
                continue;
              FName testFn;
              if (!SafeMem_Read((uint8_t *)children + nameOff, &testFn,
                                sizeof(FName)))
                continue;
              std::string testName = FNameToStr(state, testFn);
              if (!testName.empty() && testName != "None" &&
                  testName[0] != '<' && testName.length() > 1) {

                if (testName == "Object" || testName == "Actor" ||
                    testName == "Class" || testName == "Pawn" ||
                    testName == "Struct" || testName == "Function")
                  continue;
                Log("[Discovery] FField candidate: valid name '%s' at 0x%X",
                    testName.c_str(), nameOff);
                isFField = true;
                break;
              }
            }
            if (isFField)
              break;
          }
        }

        if (!isFField) {
          for (int nameOff = 0x20; nameOff <= 0x50; nameOff += 8) {
            FName testFn;
            if (!SafeMem_Read((uint8_t *)children + nameOff, &testFn,
                              sizeof(FName)))
              continue;
            std::string testName = FNameToStr(state, testFn);
            if (testName.empty() || testName == "None" || testName[0] == '<' ||
                testName.length() <= 2)
              continue;

            if (testName.find("Property") != std::string::npos)
              continue;

            if (testName == "Object" || testName == "Actor" ||
                testName == "Class" || testName == "Pawn" ||
                testName == "Struct" || testName == "Function")
              continue;

            for (int nextOff = 0x18; nextOff <= 0x50; nextOff += 8) {
              if (nextOff == nameOff)
                continue;
              void *nextPtr = *(void **)(childBuf + nextOff);
              if (!nextPtr)
                continue;
              if ((uintptr_t)nextPtr < 0x10000 ||
                  (uintptr_t)nextPtr >= 0x7FFFFFFFFFFFULL)
                continue;

              FName nextFn;
              if (!SafeMem_Read((uint8_t *)nextPtr + nameOff, &nextFn,
                                sizeof(FName)))
                continue;
              std::string nextName = FNameToStr(state, nextFn);
              if (nextName.empty() || nextName == "None" || nextName[0] == '<')
                continue;

              Log("[Discovery] FField chain found: nameOff=0x%X ('%s'), "
                  "nextOff=0x%X ('%s')",
                  nameOff, testName.c_str(), nextOff, nextName.c_str());
              isFField = true;
              break;
            }
            if (isFField)
              break;
          }
        }
      }

      if (isUField && state.UStruct_ChildrenOff < 0) {
#pragma region debug-point backrooms-init-crash-ustruct
        Log("[Discovery][Debug] UStruct layout: validating UField candidate=%p "
            "off=0x%X child=%p",
            candidate, off, children);
#pragma endregion

        FName childName;
        if (!SafeMem_Read((uint8_t *)children + state.UObject_NameOff,
                          &childName, sizeof(FName)))
          continue;
        std::string name = FNameToStr(state, childName);
        if (name.empty() || name == "None" || name[0] == '<')
          continue;

        std::string nextFieldName;
        int chosenNextOff = -1;
        const int adjacentToSuper =
            (state.UStruct_SuperOff >= 8) ? state.UStruct_SuperOff - 8 : -1;

        if (NextOffsetYieldsSameOwner(state, children, candidate, adjacentToSuper,
                                      &nextFieldName)) {
          chosenNextOff = adjacentToSuper;
        }
        for (int nextOff = 0x20; chosenNextOff < 0 && nextOff <= 0x48;
             nextOff += 8) {
          if (nextOff == adjacentToSuper)
            continue;
          if (NextOffsetYieldsSameOwner(state, children, candidate, nextOff,
                                        &nextFieldName)) {
            chosenNextOff = nextOff;
          }
        }

        if (chosenNextOff >= 0) {
          state.UStruct_ChildrenOff = off;
          state.UField_NextOff = chosenNextOff;
          Log("[Discovery] Found UField Children: off=0x%X first='%s' "
              "NextOff=0x%X next='%s'",
              off, name.c_str(), chosenNextOff, nextFieldName.c_str());
        }
      }

      if (isFField && state.UStruct_ChildPropsOff < 0) {
#pragma region debug-point backrooms-init-crash-ustruct
        Log("[Discovery][Debug] UStruct layout: validating FField candidate=%p "
            "off=0x%X child=%p",
            candidate, off, children);
#pragma endregion

        int ffNameOff = -1;
        int ffNextOff = -1;

        constexpr int32_t kFieldNameScanFirst = 0x20;
        constexpr int32_t kFieldNameScanLast = 0x50;
        constexpr int32_t kFieldNameScanStep = 8;

        for (int noff = kFieldNameScanFirst; noff <= kFieldNameScanLast;
             noff += kFieldNameScanStep) {
          FName fn;
          if (!SafeMem_Read((uint8_t *)children + noff, &fn, sizeof(FName)))
            continue;
          std::string nm = FNameToStr(state, fn);
          if (nm.empty() || nm == "None" || nm[0] == '<' || nm.length() <= 2)
            continue;
          if (!IsIdentifierishFieldName(nm))
            continue;

          if (nm.find("Property") != std::string::npos)
            continue;
          if (nm == "Object" || nm == "Actor" || nm == "Class" ||
              nm == "Pawn" || nm == "Struct" || nm == "Function" ||
              nm == "Package" || nm == "Enum")
            continue;
          ffNameOff = noff;
          break;
        }

        if (ffNameOff < 0) {

          Log("[Discovery] FField ChildProperties: no identifier-shaped name offset "
              "in 0x%X..0x%X for candidate off=0x%X",
              kFieldNameScanFirst, kFieldNameScanLast, off);
          continue;
        }

        for (int noff = 0x18; noff <= 0x80; noff += 8) {
          if (noff == ffNameOff)
            continue;
          void *nextCandidate = *(void **)(childBuf + noff);
          if (!nextCandidate)
            continue;
          if ((uintptr_t)nextCandidate < 0x10000 ||
              (uintptr_t)nextCandidate >= 0x7FFFFFFFFFFFULL)
            continue;

          FName nextName;
          if (!SafeMem_Read((uint8_t *)nextCandidate + ffNameOff, &nextName,
                            sizeof(FName)))
            continue;
          std::string nextNm = FNameToStr(state, nextName);
          if (!nextNm.empty() && nextNm != "None" && nextNm[0] != '<' &&
              IsIdentifierishFieldName(nextNm)) {

            std::string typeName = GetFFieldTypeName(state, nextCandidate);
            if (typeName.empty()) {
                Log("[Intelligence] AUTOCORRECT: FField offset candidate 0x%X points to inner/garbage property (type='%s', name='%s').", noff, typeName.c_str(), nextNm.c_str());
                Log("[Intelligence] AUTOCORRECT: Rejecting 0x%X launching smart scan to find the true pointer...", noff);
                continue;
            }

            if (noff > 0x18) {
                Log("[Intelligence] AUTOCORRECT SUCCESS: Locked onto true valid pointer at 0x%X (found '%s' of type '%s') Layout resolved dynamically", noff, nextNm.c_str(), typeName.c_str());
            }

            ffNextOff = noff;
            break;
          }
        }

        FName ffName;
        SafeMem_Read((uint8_t *)children + ffNameOff, &ffName, sizeof(FName));
        std::string name = FNameToStr(state, ffName);

        state.UStruct_ChildPropsOff = off;
        state.FField_NameOff = ffNameOff;
        state.FField_NextOff =
            ffNextOff >= 0 ? ffNextOff : 0x18;
        Log("[Discovery] Found FField ChildProperties: off=0x%X first='%s' "
            "NameOff=0x%X NextOff=0x%X",
            off, name.c_str(), ffNameOff, state.FField_NextOff);
      }
    }
  }

  state.bOffsetsDiscovered = (state.UStruct_SuperOff >= 0);
#pragma region debug-point backrooms-init-crash-ustruct
  Log("[Discovery][Debug] UStruct layout: end (OffsetsDiscovered=%d "
      "SuperOff=0x%X ChildrenOff=0x%X ChildPropsOff=0x%X FFieldNameOff=0x%X "
      "FFieldNextOff=0x%X)",
      state.bOffsetsDiscovered ? 1 : 0, state.UStruct_SuperOff,
      state.UStruct_ChildrenOff, state.UStruct_ChildPropsOff,
      state.FField_NameOff, state.FField_NextOff);
#pragma endregion
  return state.bOffsetsDiscovered;
}
bool DiscoverOffsets(EngineState &state) {
  if (state.UObjectArray_Chunks == nullptr || state.UObjectArray_NumElements <= 0) {
    if (!DiscoverTUObjectArrayLayout(state)) {
      Log("[Discovery] Failed to discover TUObjectArray layout");
      return false;
    }
  }
  if (!DiscoverUObjectLayout(state)) {
    Log("[Discovery] Failed to discover UObject layout");
    return false;
  }
  DiscoverFNameLayout(state);
  SafeMem_SetFastMode(0);
  return true;
}

}
