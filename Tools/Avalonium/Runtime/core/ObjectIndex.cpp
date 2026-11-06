#include "ObjectIndex.h"
#include "Logger.h"
#include "ObjectArray.h"
#include "NameResolution.h"
#include "ObjectUtils.h"
#include "FunctionFinder.h"
#include "BlueprintDiscovery.h"
#include "../utils/SafeMemory.h"
#include <algorithm>
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

void BuildObjectIndex(EngineState &state) {
  if (!state.GUObjectArray)
    return;

  Log("[Discovery] Building object index...");
  auto startTime = std::chrono::high_resolution_clock::now();

  int32_t count = GetObjectCount(state);
  if (count <= 0) return;

  std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);

  void *objectsPtr = SafeMem_ReadPtr((uint8_t *)state.GUObjectArray + state.TUObjectArray_ObjectsOff);
  if (!objectsPtr) return;

  int32_t itemSize = state.UObjectArray_ItemSize > 0 ? state.UObjectArray_ItemSize : 24;
  int32_t objOff = state.FUObjectItem_ObjectOff >= 0 ? state.FUObjectItem_ObjectOff : 0;
  int32_t nameOff = state.UObject_NameOff;
  int32_t classOff = state.UObject_ClassOff;
  int32_t elementsPerChunk = GetElementsPerChunk(state);
  int32_t numChunks = (count + elementsPerChunk - 1) / elementsPerChunk;

  unsigned int numThreads = std::thread::hardware_concurrency();
  if (numThreads == 0) numThreads = 4;
  if (numThreads < 2) numThreads = 2;
  if (numThreads > 16) numThreads = 16;

  struct ObjItem {
    int32_t nameIdx;
    UClass *cls;
    UObject *obj;
  };

  std::vector<std::vector<ObjItem>> threadItems(numThreads);
  for (auto &vec : threadItems) {
    vec.reserve((count / numThreads) + 4096);
  }

  std::vector<std::thread> threads;
  threads.reserve(numThreads);

  for (unsigned int t = 0; t < numThreads; t++) {
    threads.emplace_back([&threadItems, objectsPtr, itemSize, objOff, nameOff, classOff, elementsPerChunk, numChunks, count, numThreads, t]() {
      SafeMem_SetFastMode(1);
      auto &localVec = threadItems[t];

      for (int32_t c = (int32_t)t; c < numChunks; c += (int32_t)numThreads) {
        void *chunk = SafeMem_ReadPtr((uint8_t *)objectsPtr + c * sizeof(void *));
        if (!chunk) continue;

        int32_t chunkStartIdx = c * elementsPerChunk;
        int32_t countInChunk = std::min(elementsPerChunk, count - chunkStartIdx);
        uint8_t *chunkBytes = (uint8_t *)chunk;

        for (int32_t i = 0; i < countInChunk; i++) {
          UObject *obj = *(UObject **)(chunkBytes + i * itemSize + objOff);
          if (!obj || (uintptr_t)obj < 0x10000 || (uintptr_t)obj >= 0x800000000000ULL)
            continue;

          int32_t nameIdx = -1;
          if (nameOff >= 0) {
            nameIdx = ((const FName *)((const uint8_t *)obj + nameOff))->ComparisonIndex;
          }

          UClass *cls = nullptr;
          if (classOff >= 0) {
            cls = *(UClass **)((const uint8_t *)obj + classOff);
            if ((uintptr_t)cls < 0x10000 || (uintptr_t)cls >= 0x800000000000ULL)
              cls = nullptr;
          }

          localVec.push_back({nameIdx, cls, obj});
        }
      }
      SafeMem_SetFastMode(0);
    });
  }

  for (auto &th : threads) {
    if (th.joinable()) th.join();
  }

  state.NameToFirstObject.clear();
  state.NameToFirstObject.reserve(count / 2);

  state.ClassToInstances.clear();
  state.ClassToInstances.reserve(10000);

  int32_t indexed = 0;
  for (const auto &vec : threadItems) {
    indexed += static_cast<int32_t>(vec.size());
    for (const auto &item : vec) {
      if (item.nameIdx >= 0) {
        state.NameToFirstObject.try_emplace(item.nameIdx, item.obj);
      }
      if (item.cls) {
        state.ClassToInstances[item.cls].push_back(item.obj);
      }
    }
  }

  state.bIndexBuilt = true;

  auto endTime = std::chrono::high_resolution_clock::now();
  auto ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime)
          .count();

  Log("[Discovery] Indexed %d/%d objects in %lldms (NumElementsPerChunk=%d, chunks=%d)",
      indexed, count, ms, GetElementsPerChunk(state),
      (count + GetElementsPerChunk(state) - 1) / GetElementsPerChunk(state));
  Log("[Discovery] Unique names: %zu, Classes: %zu",
      state.NameToFirstObject.size(), state.ClassToInstances.size());
}

UObject *FindObjectByNameFast(EngineState &state, const std::string &name) {

  int32_t nameIdx = ResolveFNameIndex(state, name);
  if (nameIdx < 0)
    return nullptr;

  std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);
  auto it = state.NameToFirstObject.find(nameIdx);
  if (it != state.NameToFirstObject.end()) {
    return it->second;
  }
  return nullptr;
}

static uint64_t LiveLookupStamp(EngineState &state) {
  const int32_t count = GetObjectCount(state);
  const uint32_t held = static_cast<uint32_t>(count > 0 ? count : 0);

  const uint64_t genBucket = state.LiveLookupGeneration / 60;
  return (genBucket << 32) | static_cast<uint64_t>(held);
}

static void SyncLiveLookupCache(EngineState &state) {
  const uint64_t stamp = LiveLookupStamp(state);
  if (state.LiveLookupCacheStamp == stamp)
    return;

  state.LiveLookupCacheStamp = stamp;
  state.LiveClassLookups.clear();

  state.NameToObjects.clear();
}

std::vector<UObject *> FindObjectsByNameFast(EngineState &state, const std::string &name) {
  int32_t nameIdx = ResolveFNameIndex(state, name);
  if (nameIdx < 0)
    return {};

  std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);
  SyncLiveLookupCache(state);
  auto it = state.NameToObjects.find(nameIdx);
  if (it != state.NameToObjects.end()) {
    return it->second;
  }

  std::vector<UObject *> results;
  int32_t count = GetObjectCount(state);
  int32_t itemSize = state.UObjectArray_ItemSize > 0 ? state.UObjectArray_ItemSize : 24;
  int32_t objOff = state.FUObjectItem_ObjectOff >= 0 ? state.FUObjectItem_ObjectOff : 0;
  int32_t elementsPerChunk = GetElementsPerChunk(state);
  int32_t numChunks = (count + elementsPerChunk - 1) / elementsPerChunk;
  void *objectsPtr = SafeMem_ReadPtr((uint8_t *)state.GUObjectArray + state.TUObjectArray_ObjectsOff);

  if (objectsPtr && state.UObject_NameOff >= 0) {
    SafeMem_SetFastMode(1);
    for (int32_t c = 0; c < numChunks; c++) {
      void *chunk = SafeMem_ReadPtr((uint8_t *)objectsPtr + c * sizeof(void *));
      if (!chunk) continue;
      int32_t chunkStartIdx = c * elementsPerChunk;
      int32_t countInChunk = std::min(elementsPerChunk, count - chunkStartIdx);
      uint8_t *chunkBytes = (uint8_t *)chunk;
      for (int32_t i = 0; i < countInChunk; i++) {
        UObject *obj = *(UObject **)(chunkBytes + i * itemSize + objOff);
        if (!obj || (uintptr_t)obj < 0x10000 || (uintptr_t)obj >= 0x800000000000ULL) continue;
        int32_t objNameIdx = ((const FName *)((const uint8_t *)obj + state.UObject_NameOff))->ComparisonIndex;
        if (objNameIdx == nameIdx) {
          results.push_back(obj);
        }
      }
    }
    SafeMem_SetFastMode(0);
  }

  state.NameToObjects[nameIdx] = results;
  return results;
}

void AdvanceLiveLookupGeneration(EngineState &state) {
  if (!state.IndexMutex) return;

  std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);
  ++state.LiveLookupGeneration;
}

std::vector<UObject *> FindLiveObjectsByClass(EngineState &state, UClass *cls) {
  if (!cls)
    return {};

  std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);
  SyncLiveLookupCache(state);

  auto held = state.LiveClassLookups.find(cls);
  if (held != state.LiveClassLookups.end())
    return held->second;

  std::vector<UObject *> results;
  const int32_t total = GetObjectCount(state);

  int32_t itemSize = state.UObjectArray_ItemSize > 0 ? state.UObjectArray_ItemSize : 24;
  int32_t objOff = state.FUObjectItem_ObjectOff >= 0 ? state.FUObjectItem_ObjectOff : 0;
  int32_t elementsPerChunk = GetElementsPerChunk(state);
  int32_t numChunks = (total + elementsPerChunk - 1) / elementsPerChunk;
  void *objectsPtr = SafeMem_ReadPtr((uint8_t *)state.GUObjectArray + state.TUObjectArray_ObjectsOff);

  SafeMem_SetFastMode(1);
  std::unordered_map<UClass *, bool> classMatches;
  classMatches.reserve(512);
  auto matchesClass = [&](UClass *objCls) -> bool {
    if (!objCls) return false;
    if (objCls == cls) return true;
    auto it = classMatches.find(objCls);
    if (it != classMatches.end()) return it->second;
    bool res = IsStructA(state, (UStruct *)objCls, (UStruct *)cls);
    classMatches[objCls] = res;
    return res;
  };

  const int32_t clsOff = state.UObject_ClassOff;
  if (objectsPtr) {
    for (int32_t c = 0; c < numChunks; c++) {
      void *chunk = SafeMem_ReadPtr((uint8_t *)objectsPtr + c * sizeof(void *));
      if (!chunk) continue;
      int32_t chunkStartIdx = c * elementsPerChunk;
      int32_t countInChunk = std::min(elementsPerChunk, total - chunkStartIdx);
      uint8_t *chunkBytes = (uint8_t *)chunk;
      for (int32_t i = 0; i < countInChunk; i++) {
        UObject *obj = *(UObject **)(chunkBytes + i * itemSize + objOff);
        if (!obj || (uintptr_t)obj < 0x10000 || (uintptr_t)obj >= 0x800000000000ULL) continue;

        UClass *objCls = (clsOff >= 0) ? *(UClass **)((uint8_t *)obj + clsOff) : GetObjectClass(state, obj);
        if (matchesClass(objCls)) {
          results.push_back(obj);
        }
      }
    }
  } else {
    for (int32_t i = 0; i < total; ++i) {
      UObject *obj = GetObjectByIndex(state, i);
      if (!obj) continue;
      UClass *objCls = (clsOff >= 0) ? *(UClass **)((uint8_t *)obj + clsOff) : GetObjectClass(state, obj);
      if (matchesClass(objCls)) {
        results.push_back(obj);
      }
    }
  }
  SafeMem_SetFastMode(0);

  state.LiveClassLookups[cls] = results;
  return results;
}

std::vector<UObject *> FindObjectsByClassFast(EngineState &state, UClass *cls) {

  return FindLiveObjectsByClass(state, cls);
}

static inline bool IsAnyClass(const EngineState &state, UObject *obj) {
  if (!obj || !state.UClassClass) return false;
  if (obj == (UObject*)state.UClassClass) return true;
  UClass *cls = GetObjectClass(state, obj);
  if (!cls) return false;
  if (cls == state.UClassClass) return true;
  return IsStructA(state, (UStruct*)cls, (UStruct*)state.UClassClass);
}

UClass *FindClassFast(EngineState &state, const std::string &name) {
  int32_t nameIdx = ResolveFNameIndex(state, name);
  if (nameIdx < 0)
    return nullptr;

  std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);
  auto it = state.NameToObjects.find(nameIdx);
  if (it == state.NameToObjects.end())
    return nullptr;

  for (UObject *obj : it->second) {
    if (IsAnyClass(state, obj)) {
      return (UClass *)obj;
    }
  }

  return nullptr;
}

std::string GetCachedFName(EngineState &state, int32_t nameIdx) {
  {
    std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);
    auto it = state.FNameCache.find(nameIdx);
    if (it != state.FNameCache.end()) {
      return it->second;
    }
  }

  FName fn;
  fn.ComparisonIndex = nameIdx;
  fn.Number = 0;
  std::string resolved = FNameToStr(state, fn);

  std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);
  state.FNameCache[nameIdx] = resolved;
  return resolved;
}

int32_t ResolveFNameIndex(EngineState &state, const std::string &name) {
  if (name.empty()) return -1;

  {
    std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);
    auto exact = state.StringToFNameIndex.find(name);
    if (exact != state.StringToFNameIndex.end())
      return exact->second;
  }

  const std::string_view query(name);
  {
    std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);
    auto it = state.NameTextToIndex.find(query);
    if (it != state.NameTextToIndex.end()) {
      state.StringToFNameIndex[name] = it->second;
      return it->second;
    }
  }

  RefreshFNamePoolDelta(state);

  std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);
  auto it = state.NameTextToIndex.find(query);
  if (it != state.NameTextToIndex.end()) {
    state.StringToFNameIndex[name] = it->second;
    return it->second;
  }
  return -1;
}

std::vector<UObject *> FindObjectsByPrefix(EngineState &state,
                                           const std::string &prefix) {
  std::vector<UObject *> results;
  if (!state.bIndexBuilt || prefix.empty())
    return results;

  std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);

  if (state.SortedNames.empty()) {
    state.SortedNames.reserve(state.NameToFirstObject.size());
    for (const auto &pair : state.NameToFirstObject) {
      int32_t nameIdx = pair.first;
      UObject *obj = pair.second;
      if (nameIdx >= 0 && static_cast<size_t>(nameIdx) < state.FNameCacheArray.size()) {
        const char *ptr = state.FNameCacheArray[nameIdx];
        if (ptr && ptr[0]) {
          state.SortedNames.emplace_back(ptr, obj);
        }
      }
    }
    std::sort(state.SortedNames.begin(), state.SortedNames.end(),
              [](const std::pair<std::string, UObject *> &a,
                 const std::pair<std::string, UObject *> &b) {
                return a.first < b.first;
              });
  }

  auto lower = std::lower_bound(
      state.SortedNames.begin(), state.SortedNames.end(), prefix,
      [](const std::pair<std::string, UObject *> &elem,
         const std::string &val) { return elem.first < val; });

  for (auto it = lower; it != state.SortedNames.end(); ++it) {
    if (it->first.compare(0, prefix.size(), prefix) != 0)
      break;
    results.push_back(it->second);
  }

  return results;
}

namespace {
inline const char* FastGetObjectName(const EngineState &state, UObject *obj) {
  if (!obj || state.UObject_NameOff < 0)
    return nullptr;
  const FName *namePtr = (const FName *)((const uint8_t *)obj + state.UObject_NameOff);
  int32_t idx = namePtr->ComparisonIndex;
  if (idx >= 0 && (size_t)idx < state.FNameCacheArray.size()) {
    return state.FNameCacheArray[idx];
  }
  return nullptr;
}

inline bool NameCharEquals(char a, char b) {
  return tolower(static_cast<unsigned char>(a)) ==
         tolower(static_cast<unsigned char>(b));
}

inline bool NameIEquals(const std::string &a, const std::string &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (!NameCharEquals(a[i], b[i]))
      return false;
  }
  return true;
}

struct NameParts {
  std::string plain;
  long number = 0;
};

inline NameParts SplitNameNumber(const std::string &raw) {
  NameParts parts;
  parts.plain = raw;
  const size_t us = raw.rfind('_');
  if (us == std::string::npos || us == 0 || us + 1 >= raw.size())
    return parts;
  long value = 0;
  for (size_t i = us + 1; i < raw.size(); ++i) {
    if (raw[i] < '0' || raw[i] > '9')
      return parts;
    value = value * 10 + (raw[i] - '0');
    if (value > 1000000)
      return parts;
  }
  parts.plain = raw.substr(0, us);
  parts.number = value;
  return parts;
}

inline bool NameEquals(const std::string &a, const std::string &b) {
  if (NameIEquals(a, b))
    return true;
  const NameParts pa = SplitNameNumber(a);
  const NameParts pb = SplitNameNumber(b);
  return pa.number == pb.number && NameIEquals(pa.plain, pb.plain);
}

inline bool NameSuffixEquals(const std::string &name, const std::string &segment) {
  if (NameIEquals(name, segment))
    return true;
  const NameParts n = SplitNameNumber(name);
  const NameParts s = SplitNameNumber(segment);
  if (n.number != s.number || s.plain.size() > n.plain.size())
    return false;
  return NameIEquals(n.plain.substr(n.plain.size() - s.plain.size()), s.plain);
}

struct SearchPath {
  std::vector<std::string> outers;
  std::string inner;
  bool bSubobject = false;
};

inline SearchPath SplitSearchPath(const std::string &query) {
  SearchPath path;
  size_t begin = 0;
  while (true) {
    const size_t delim = query.find_first_of(".:", begin);
    if (delim == std::string::npos)
      break;
    if (delim == begin) {
      path.bSubobject = true;
    } else {
      path.outers.push_back(query.substr(begin, delim - begin));
    }
    if (query[delim] == ':')
      path.bSubobject = true;
    begin = delim + 1;
  }
  path.inner = query.substr(begin);
  return path;
}

struct QueryForm {
  std::string path;
  std::string expectedClassName;
};

inline std::vector<QueryForm> SplitQueryForms(const std::string &unwrapped) {
  std::vector<QueryForm> forms;
  forms.push_back({unwrapped, std::string()});
  const size_t space = unwrapped.find(' ');
  if (space != std::string::npos && space + 1 < unwrapped.size())
    forms.push_back({unwrapped.substr(space + 1), unwrapped.substr(0, space)});
  return forms;
}

inline bool MatchOuterChain(const EngineState &state, UObject *outer,
                            const SearchPath &path) {
  if (path.outers.empty())
    return true;
  for (size_t i = path.outers.size(); i > 1; --i) {
    if (!outer)
      return false;
    if (!NameEquals(GetObjectNameStr(state, outer), path.outers[i - 1]))
      return false;
    outer = GetObjectOuter(state, outer);
  }
  return outer && NameSuffixEquals(GetObjectNameStr(state, outer), path.outers[0]);
}

std::string BuildOuterColonFunc(const EngineState &state, UFunction *fn) {
  if (!fn)
    return std::string();
  const char *funcName = FastGetObjectName(state, (UObject *)fn);
  if (!funcName || !funcName[0]) {
    std::string fallback = GetObjectNameStr(state, (UObject *)fn);
    if (fallback.empty()) return std::string();
    return fallback;
  }
  UObject *outer = (state.UObject_OuterOff >= 0)
                       ? *(UObject **)((const uint8_t *)fn + state.UObject_OuterOff)
                       : nullptr;
  if (!outer)
    return std::string(funcName);
  const char *outerName = FastGetObjectName(state, outer);
  if (!outerName || !outerName[0])
    return std::string(funcName);

  std::string key;
  key.reserve(strlen(outerName) + 1 + strlen(funcName));
  key.append(outerName);
  key.push_back(':');
  key.append(funcName);
  return key;
}

void BuildFunctionCacheForObject(const EngineState &state, UObject *obj,
                                 std::unordered_map<std::string, UFunction *> &byFullKey,
                                 std::unordered_map<std::string, UFunction *> &byNameOnly,
                                 std::unordered_set<UFunction *> &liveFns) {
  if (!obj)
    return;
  if (state.UObject_ClassOff >= 0) {
    UClass *cls = *(UClass **)((const uint8_t *)obj + state.UObject_ClassOff);
    if (!cls || cls != (UClass *)state.UFunctionClass)
      return;
  }

  UFunction *fn = (UFunction *)obj;
  const char *funcName = FastGetObjectName(state, obj);
  if (funcName && funcName[0]) {
    byNameOnly.emplace(funcName, fn);
  }

  UObject *outer = (state.UObject_OuterOff >= 0)
                       ? *(UObject **)((const uint8_t *)obj + state.UObject_OuterOff)
                       : nullptr;
  const char *outerName = outer ? FastGetObjectName(state, outer) : nullptr;
  if (outerName && outerName[0] && funcName && funcName[0]) {
    std::string colonKey = std::string(outerName) + ":" + funcName;
    std::string dotKey = std::string(outerName) + "." + funcName;
    byFullKey.emplace(std::move(colonKey), fn);
    byFullKey.emplace(std::move(dotKey), fn);
  } else if (funcName && funcName[0]) {
    byFullKey.emplace(funcName, fn);
  }

  liveFns.insert(fn);
}
}

std::string StripTypeTokenWrapper(const std::string &query) {
  const size_t open = query.find('\'');
  if (open == std::string::npos)
    return query;
  const size_t close = query.rfind('\'');
  if (close <= open)
    return query;
  return query.substr(open + 1, close - open - 1);
}

namespace {

bool CandidateMatchesQuery(const EngineState &state, UObject *cand,
                           const SearchPath &path, UClass *requiredClass,
                           bool exactClass, UObject *withinOuter,
                           const std::string &expectedClassName) {
  if (!cand)
    return false;

  UClass *cls = GetObjectClass(state, cand);
  if (requiredClass) {
    if (!cls)
      return false;
    if (cls != requiredClass &&
        (exactClass ||
         !IsStructA(state, reinterpret_cast<UStruct *>(cls),
                    reinterpret_cast<UStruct *>(requiredClass))))
      return false;
  }

  if (!expectedClassName.empty()) {
    if (!cls || !NameEquals(GetObjectNameStr(state, reinterpret_cast<UObject *>(cls)),
                            expectedClassName))
      return false;
  }

  if (withinOuter)
    return GetObjectOuter(state, cand) == withinOuter;

  return MatchOuterChain(state, GetObjectOuter(state, cand), path);
}

}

static bool FunctionLookupMatches(const EngineState &state, UFunction *candidate,
                                  const std::string &lookup) {
  if (!candidate)
    return false;

  const std::string unwrapped = StripTypeTokenWrapper(lookup);
  for (const QueryForm &form : SplitQueryForms(unwrapped)) {
    const SearchPath path = SplitSearchPath(form.path);
    if (path.inner.empty())
      continue;
    if (!NameEquals(GetObjectNameStr(state, (UObject *)candidate), path.inner))
      continue;
    if (CandidateMatchesQuery(state, (UObject *)candidate, path, nullptr, false,
                              nullptr, form.expectedClassName))
      return true;
  }
  return false;
}

UObject *ResolveObjectPath(EngineState &state, const std::string &query,
                           UClass *requiredClass, bool exactClass,
                           UObject *withinOuter) {
  if (query.empty())
    return nullptr;

  const std::string unwrapped = StripTypeTokenWrapper(query);

  std::string attemptedLeaf;
  int32_t attemptedIndex = -2;
  size_t attemptedCandidates = 0;

  for (const QueryForm &form : SplitQueryForms(unwrapped)) {
    const SearchPath path = SplitSearchPath(form.path);
    if (path.inner.empty())
      continue;

    if (state.bIndexBuilt) {

      int32_t leafIndex = ResolveFNameIndex(state, path.inner);
      if (leafIndex < 0) {
        std::string why;
        leafIndex = FindOrAddFNameIndex(state, path.inner, &why);
        if (leafIndex < 0 && !why.empty()) {
          static std::mutex reportedMutex;
          static std::unordered_set<std::string> reported;
          std::lock_guard<std::mutex> lock(reportedMutex);
          if (reported.size() < 16 && reported.insert(path.inner).second) {
            Log("[Discovery] Name '%s' cannot be created through the engine's writer: %s",
                path.inner.c_str(), why.c_str());
          }
        }
      }

      attemptedLeaf = path.inner;
      attemptedIndex = leafIndex;
      const std::vector<UObject *> candidates = FindObjectsByNameFast(state, path.inner);
      attemptedCandidates = candidates.size();
      for (UObject *cand : candidates) {
        if (CandidateMatchesQuery(state, cand, path, requiredClass, exactClass,
                                  withinOuter, form.expectedClassName))
          return cand;
      }
      continue;
    }

    int32_t count = GetObjectCount(state);
    SafeMem_SetFastMode(1);
    UObject *resolved = nullptr;
    for (int32_t i = 0; i < count && !resolved; ++i) {
      UObject *obj = GetObjectByIndex(state, i);
      if (!obj)
        continue;
      if (NameEquals(GetObjectNameStr(state, obj), path.inner) &&
          CandidateMatchesQuery(state, obj, path, requiredClass, exactClass,
                                withinOuter, form.expectedClassName))
        resolved = obj;
    }
    SafeMem_SetFastMode(0);
    if (resolved)
      return resolved;
  }

  if (state.bIndexBuilt && !attemptedLeaf.empty()) {
    static std::mutex reportedMutex;
    static std::unordered_set<std::string> reported;
    std::lock_guard<std::mutex> lock(reportedMutex);
    if (reported.size() < 32 && reported.insert(query).second) {
      Log("[Discovery] Object path '%s' resolves nothing: leaf '%s' index=%d, %zu object(s) carry that name",
          query.c_str(), attemptedLeaf.c_str(), attemptedIndex, attemptedCandidates);
    }
  }

  return nullptr;
}

UObject *FindObjectByPath(EngineState &state, const std::string &query) {
  return ResolveObjectPath(state, query);
}

void RebuildFunctionCache(EngineState &state) {
  if (!state.GUObjectArray || !state.bIndexBuilt)
    return;
  if (!state.UFunctionClass)
    return;

  std::lock_guard<std::mutex> lock(*state.ReflectionCacheMutex);

  state.FunctionByFullKey.clear();
  state.FunctionByNameOnly.clear();
  state.LiveUFunctions.clear();

  auto it = state.ClassToInstances.find((UClass *)state.UFunctionClass);
  if (it != state.ClassToInstances.end() && !it->second.empty()) {
    const auto &funcList = it->second;
    state.FunctionByFullKey.reserve(funcList.size() * 2);
    state.FunctionByNameOnly.reserve(funcList.size());
    state.LiveUFunctions.reserve(funcList.size());

    SafeMem_SetFastMode(1);
    for (UObject *obj : funcList) {
      if (!obj) continue;
      UFunction *fn = (UFunction *)obj;
      const char *funcName = FastGetObjectName(state, obj);
      if (!funcName || !funcName[0]) continue;

      state.FunctionByNameOnly.emplace(funcName, fn);

      UObject *outer = (state.UObject_OuterOff >= 0)
                           ? *(UObject **)((const uint8_t *)obj + state.UObject_OuterOff)
                           : nullptr;
      const char *outerName = outer ? FastGetObjectName(state, outer) : nullptr;
      if (outerName && outerName[0]) {
        std::string colonKey = std::string(outerName) + ":" + funcName;
        std::string dotKey = std::string(outerName) + "." + funcName;
        state.FunctionByFullKey.emplace(std::move(colonKey), fn);
        state.FunctionByFullKey.emplace(std::move(dotKey), fn);
      } else {
        state.FunctionByFullKey.emplace(funcName, fn);
      }

      state.LiveUFunctions.insert(fn);
    }
    SafeMem_SetFastMode(0);
  } else {
    int32_t count = GetObjectCount(state);
    state.FunctionByFullKey.reserve(count / 4);
    state.FunctionByNameOnly.reserve(count / 8);
    state.LiveUFunctions.reserve(count / 4);

    SafeMem_SetFastMode(1);
    for (int32_t i = 0; i < count; ++i) {
      UObject *obj = GetObjectByIndex(state, i);
      BuildFunctionCacheForObject(state, obj, state.FunctionByFullKey,
                                   state.FunctionByNameOnly,
                                   state.LiveUFunctions);
    }
    SafeMem_SetFastMode(0);
  }

  state.FunctionCacheLastCount = GetObjectCount(state);
}

int32_t DeltaUpdateFunctionCache(EngineState &state) {
  if (!state.GUObjectArray || !state.bIndexBuilt || !state.UFunctionClass)
    return 0;

  std::lock_guard<std::mutex> lock(*state.ReflectionCacheMutex);

  int32_t currentCount = GetObjectCount(state);
  int32_t lastCount = state.FunctionCacheLastCount;
  int32_t added = 0;

  SafeMem_SetFastMode(1);

  if (currentCount > lastCount) {
    for (int32_t i = lastCount; i < currentCount; ++i) {
      UObject *obj = GetObjectByIndex(state, i);
      if (!obj)
        continue;
      UClass *cls = GetObjectClass(state, obj);
      if (cls && cls == (UClass *)state.UFunctionClass) {
        BuildFunctionCacheForObject(state, obj, state.FunctionByFullKey,
                                     state.FunctionByNameOnly,
                                     state.LiveUFunctions);
        ++added;
      }
    }
  }

  if (currentCount < lastCount) {
    if (state.LiveUFunctions.empty()) {
      SafeMem_SetFastMode(0);
      state.FunctionCacheLastCount = currentCount;
      return added;
    }

    std::unordered_set<UObject *> stillLive;
    stillLive.reserve(state.LiveUFunctions.size() * 2);
    for (int32_t i = 0; i < currentCount; ++i) {
      UObject *obj = GetObjectByIndex(state, i);
      if (obj)
        stillLive.insert(obj);
    }

    std::vector<UFunction *> dead;
    dead.reserve(64);
    for (UFunction *fn : state.LiveUFunctions) {
      if (stillLive.find((UObject *)fn) == stillLive.end())
        dead.push_back(fn);
    }
    for (UFunction *fn : dead) {
      std::string nameOnly = GetObjectNameStr(state, (UObject *)fn);
      if (!nameOnly.empty()) {
        auto it = state.FunctionByNameOnly.find(nameOnly);
        if (it != state.FunctionByNameOnly.end() && it->second == fn)
          state.FunctionByNameOnly.erase(it);
      }

      for (auto it = state.FunctionByFullKey.begin();
           it != state.FunctionByFullKey.end();) {
        if (it->second == fn)
          it = state.FunctionByFullKey.erase(it);
        else
          ++it;
      }
      state.LiveUFunctions.erase(fn);
    }
  }

  SafeMem_SetFastMode(0);
  state.FunctionCacheLastCount = currentCount;
  return added;
}

UFunction *FindFunctionCached(EngineState &state, const std::string &lookup) {
  if (lookup.empty())
    return nullptr;

  {
    std::lock_guard<std::mutex> lock(*state.ReflectionCacheMutex);
    auto itFull = state.FunctionByFullKey.find(lookup);
    if (itFull != state.FunctionByFullKey.end() && itFull->second != nullptr)
      return itFull->second;
    auto itBare = state.FunctionByNameOnly.find(lookup);
    if (itBare != state.FunctionByNameOnly.end() && itBare->second != nullptr)
      return itBare->second;
  }

  size_t lastSep = lookup.find_last_of(":.");
  std::string bareName = (lastSep != std::string::npos && lastSep + 1 < lookup.size())
                             ? lookup.substr(lastSep + 1)
                             : lookup;

  if (lastSep != std::string::npos) {
    UFunction *candidate = nullptr;
    {
      std::lock_guard<std::mutex> lock(*state.ReflectionCacheMutex);
      auto itBare = state.FunctionByNameOnly.find(bareName);
      if (itBare != state.FunctionByNameOnly.end())
        candidate = itBare->second;
    }
    if (candidate && FunctionLookupMatches(state, candidate, lookup)) {
      std::lock_guard<std::mutex> lock(*state.ReflectionCacheMutex);
      state.FunctionByFullKey[lookup] = candidate;
      return candidate;
    }
  }

  if (!state.GUObjectArray || !state.UFunctionClass) {
    return nullptr;
  }

  UFunction *resolved = nullptr;

  if (state.bIndexBuilt) {
    std::vector<UObject *> fastMatches = FindObjectsByNameFast(state, bareName);
    for (UObject *fast : fastMatches) {
      if (fast && GetObjectClass(state, fast) == state.UFunctionClass &&
          FunctionLookupMatches(state, (UFunction *)fast, lookup)) {
        resolved = (UFunction *)fast;
        break;
      }
    }
  }

  if (!resolved) {
    std::lock_guard<std::mutex> lock(*state.ReflectionCacheMutex);
    for (UFunction *liveFn : state.LiveUFunctions) {
      if (liveFn && FunctionLookupMatches(state, liveFn, lookup)) {
        resolved = liveFn;
        break;
      }
    }
  }

  if (!resolved && !state.bIndexBuilt) {
    int32_t count = GetObjectCount(state);
    SafeMem_SetFastMode(1);
    for (int32_t i = 0; i < count; ++i) {
      UObject *obj = GetObjectByIndex(state, i);
      if (!obj)
        continue;

      UClass *cls = GetObjectClass(state, obj);
      if (cls && cls == (UClass *)state.UFunctionClass &&
          FunctionLookupMatches(state, (UFunction *)obj, lookup)) {
        resolved = (UFunction *)obj;
        break;
      }
    }
    SafeMem_SetFastMode(0);
  }

  if (resolved) {
    std::lock_guard<std::mutex> lock(*state.ReflectionCacheMutex);
    state.LiveUFunctions.insert(resolved);
    std::string funcName = GetObjectNameStr(state, (UObject *)resolved);
    if (!funcName.empty())
      state.FunctionByNameOnly[funcName] = resolved;
    std::string key = BuildOuterColonFunc(state, resolved);
    if (!key.empty())
      state.FunctionByFullKey[key] = resolved;
    state.FunctionByFullKey[lookup] = resolved;
  }

  return resolved;
}

UClass *FindClassCached(EngineState &state, const std::string &className) {
  if (className.empty())
    return nullptr;

  {
    std::lock_guard<std::mutex> lock(*state.ReflectionCacheMutex);
    auto it = state.ClassByName.find(className);
    if (it != state.ClassByName.end())
      return it->second;
  }

  if (!state.GUObjectArray || !state.UClassClass) {
    std::lock_guard<std::mutex> lock(*state.ReflectionCacheMutex);
    state.ClassByName[className] = nullptr;
    return nullptr;
  }

  UClass *resolved = nullptr;

  if (state.bIndexBuilt) {
    std::vector<UObject *> fastMatches = FindObjectsByNameFast(state, className);
    for (UObject *fast : fastMatches) {
      if (fast && IsAnyClass(state, fast)) {
        resolved = (UClass *)fast;
        break;
      }
    }

    if (!resolved && className.size() > 1 && (className[0] == 'U' || className[0] == 'A')) {
      std::string stripped = className.substr(1);
      fastMatches = FindObjectsByNameFast(state, stripped);
      for (UObject *fast : fastMatches) {
        if (fast && IsAnyClass(state, fast)) {
          resolved = (UClass *)fast;
          break;
        }
      }
    }
    if (!resolved && (className.size() < 2 || className.substr(className.size() - 2) != "_C")) {
      std::string withC = className + "_C";
      fastMatches = FindObjectsByNameFast(state, withC);
      for (UObject *fast : fastMatches) {
        if (fast && IsAnyClass(state, fast)) {
          resolved = (UClass *)fast;
          break;
        }
      }
    }

    if (!resolved && className.size() > 2 && className.substr(className.size() - 2) == "_C") {
      std::string noC = className.substr(0, className.size() - 2);
      fastMatches = FindObjectsByNameFast(state, noC);
      for (UObject *fast : fastMatches) {
        if (fast && IsAnyClass(state, fast)) {
          resolved = (UClass *)fast;
          break;
        }
      }
    }

    if (!resolved && className.size() > 3 && (_strnicmp(className.c_str(), "BP_", 3) == 0)) {
      std::string noBP = className.substr(3);
      fastMatches = FindObjectsByNameFast(state, noBP);
      for (UObject *fast : fastMatches) {
        if (fast && IsAnyClass(state, fast)) {
          resolved = (UClass *)fast;
          break;
        }
      }
      if (!resolved && noBP.size() > 2 && noBP.substr(noBP.size() - 2) == "_C") {
        std::string noBPC = noBP.substr(0, noBP.size() - 2);
        fastMatches = FindObjectsByNameFast(state, noBPC);
        for (UObject *fast : fastMatches) {
          if (fast && IsAnyClass(state, fast)) {
            resolved = (UClass *)fast;
            break;
          }
        }
      }
    }

    if (!resolved) {
      std::string lowerTarget = className;
      std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(), ::tolower);
      if (lowerTarget.size() > 3 && lowerTarget.rfind("bp_", 0) == 0) lowerTarget = lowerTarget.substr(3);
      if (lowerTarget.size() > 2 && lowerTarget.substr(lowerTarget.size() - 2) == "_c") lowerTarget = lowerTarget.substr(0, lowerTarget.size() - 2);
      if (lowerTarget.size() >= 3) {
        std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);
        for (const auto& pair : state.NameToObjects) {
          int32_t nameIdx = pair.first;
          if (nameIdx >= 0 && static_cast<size_t>(nameIdx) < state.FNameCacheArray.size()) {
            const char* namePtr = state.FNameCacheArray[nameIdx];
            if (namePtr) {
              std::string candidate = namePtr;
              std::transform(candidate.begin(), candidate.end(), candidate.begin(), ::tolower);
              if (candidate.find(lowerTarget) != std::string::npos) {
                for (UObject* obj : pair.second) {
                  if (obj && IsAnyClass(state, obj)) {
                    resolved = (UClass*)obj;
                    break;
                  }
                }
                if (resolved) break;
              }
            }
          }
        }
      }
    }
  }

  if (!resolved && !state.bIndexBuilt) {
    int32_t count = GetObjectCount(state);
    SafeMem_SetFastMode(1);
    for (int32_t i = 0; i < count; ++i) {
      UObject *obj = GetObjectByIndex(state, i);
      if (!obj || !IsAnyClass(state, obj))
        continue;
      std::string objName = GetObjectNameStr(state, obj);
      if (_stricmp(objName.c_str(), className.c_str()) == 0) {
        resolved = (UClass *)obj;
        break;
      }
    }
    SafeMem_SetFastMode(0);
  }

  {
    std::lock_guard<std::mutex> lock(*state.ReflectionCacheMutex);
    state.ClassByName[className] = resolved;
  }
  return resolved;
}

void InvalidateObjectFromCache(EngineState &state, UObject *obj) {
  if (!obj)
    return;
  std::lock_guard<std::mutex> lock(*state.ReflectionCacheMutex);

  if (state.CachedPlayerControllerClass == obj)
    state.CachedPlayerControllerClass = nullptr;

  for (auto it = state.ClassByName.begin(); it != state.ClassByName.end();) {
    if (it->second == (UClass *)obj)
      it = state.ClassByName.erase(it);
    else
      ++it;
  }

  UFunction *asFn = (UFunction *)obj;
  auto itFn = state.LiveUFunctions.find(asFn);
  if (itFn != state.LiveUFunctions.end()) {
    state.LiveUFunctions.erase(itFn);
    for (auto it = state.FunctionByFullKey.begin();
         it != state.FunctionByFullKey.end();) {
      if (it->second == asFn)
        it = state.FunctionByFullKey.erase(it);
      else
        ++it;
    }
    for (auto it = state.FunctionByNameOnly.begin();
         it != state.FunctionByNameOnly.end();) {
      if (it->second == asFn)
        it = state.FunctionByNameOnly.erase(it);
      else
        ++it;
    }
  }
}

}
