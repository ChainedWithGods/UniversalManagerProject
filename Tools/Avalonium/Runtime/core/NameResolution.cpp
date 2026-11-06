#include "NameResolution.h"
#include "ObjectArray.h"
#include "ObjectUtils.h"
#include "Logger.h"
#include "../utils/SafeMemory.h"
#include <windows.h>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <algorithm>

namespace EngineDiscovery {

static int ScoreFUObjectItemLayout(const EngineState &state, void *chunk,
                                   int itemSize, int objOff,
                                   int sampleCount = 16) {
  if (!chunk || itemSize < 0x10 || objOff < 0 || objOff + 8 > itemSize)
    return -1;

  int score = 0;
  for (int i = 0; i < sampleCount; ++i) {
    void *obj = nullptr;
    if (!SafeMem_Read((uint8_t *)chunk + i * itemSize + objOff, &obj, 8) ||
        !obj) {
      continue;
    }

    const uintptr_t objAddr = reinterpret_cast<uintptr_t>(obj);
    if (objAddr < 0x10000 || objAddr >= 0x7FFFFFFFFFFFULL) {
      continue;
    }
    if (!IsReadable(obj, 0x28)) {
      continue;
    }

    score += 2;

    int32_t internalIndex = -1;
    if (SafeMem_Read((uint8_t *)obj + state.UObject_IndexOff, &internalIndex,
                     sizeof(internalIndex))) {
      if (internalIndex == i) {
        score += 6;
      } else if (internalIndex >= 0 && internalIndex < 8000000) {
        score += 1;
      }
    }

    void *cls = SafeMem_ReadPtr((uint8_t *)obj + state.UObject_ClassOff);
    if (!cls)
      continue;
    const uintptr_t clsAddr = reinterpret_cast<uintptr_t>(cls);
    if (clsAddr < 0x10000 || clsAddr >= 0x7FFFFFFFFFFFULL ||
        !IsReadable(cls, 0x20)) {
      continue;
    }

    score += 2;

    void *metacls = SafeMem_ReadPtr((uint8_t *)cls + state.UObject_ClassOff);
    if (!metacls)
      continue;
    const uintptr_t metaAddr = reinterpret_cast<uintptr_t>(metacls);
    if (metaAddr < 0x10000 || metaAddr >= 0x7FFFFFFFFFFFULL ||
        !IsReadable(metacls, 0x20)) {
      continue;
    }

    void *metameta =
        SafeMem_ReadPtr((uint8_t *)metacls + state.UObject_ClassOff);
    if (metacls == cls || metacls == metameta) {
      score += 3;
    }
  }

  return score;
}

static std::string WideToUtf8(const wchar_t *wide, int length) {
  if (!wide || length <= 0)
    return "";

  int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide, length, nullptr, 0,
                                    nullptr, nullptr);
  if (utf8Len <= 0)
    return "";

  std::string result(static_cast<size_t>(utf8Len), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide, length, result.data(), utf8Len, nullptr,
                      nullptr);
  return result;
}

std::string DecodeFNameEntryString(const EngineState &state,
                                          uint8_t *data, size_t offset,
                                          uint16_t meta) {
  const bool isWide = (meta & 1) != 0;
  uint16_t size = (meta >> state.FName_LenShift) & state.FName_LenMask;
  if (size == 0 || size > 1024)
    return "";

  int strStart = state.FName_HeaderOff + 2;

  if (isWide) {
    std::vector<wchar_t> wideBuf(static_cast<size_t>(size) + 1, 0);
    const size_t byteCount = static_cast<size_t>(size) * sizeof(wchar_t);
    if (!SafeMem_Read(data + offset + strStart, wideBuf.data(), byteCount))
      return "";
    return WideToUtf8(wideBuf.data(), size);
  }

  std::vector<char> ansiBuf(static_cast<size_t>(size) + 1, 0);
  if (!SafeMem_Read(data + offset + strStart, ansiBuf.data(), size))
    return "";

  std::string result(ansiBuf.data(), size);
  for (char &c : result) {
    if (c >= 0 && c < 32) {
      if (c == '\r' || c == '\n' || c == '\t')
        c = ' ';
      else
        return "";
    }
  }
  return result;
}

void BuildFNameCache(EngineState &state) {
  if (!state.GNames)
    return;

  auto startTime = std::chrono::high_resolution_clock::now();
  int chunksStart = state.FName_ChunksStart;

  if (chunksStart < 0) {
    chunksStart = 0x10;
    state.FName_ChunksStart = 0x10;
  }

  const int maxChunks = 1024;
  int highestChunk = 0;
  SafeMem_SetFastMode(1);
  void *blockArrayPtr = nullptr;
  if (state.FName_IsIndirect) {
    SafeMem_Read((uint8_t *)state.GNames + chunksStart, &blockArrayPtr, 8);
  }
  for (int c = 0; c < maxChunks; c++) {
    void *block = nullptr;
    if (state.FName_IsIndirect) {
      if (blockArrayPtr)
        SafeMem_Read((uint8_t *)blockArrayPtr + c * 8, &block, 8);
    } else {
      SafeMem_Read((uint8_t *)state.GNames + chunksStart + c * 8, &block, 8);
    }
    if (block) highestChunk = c;
  }
  SafeMem_SetFastMode(0);

  const unsigned int totalChunks = static_cast<unsigned int>(highestChunk + 1);
  unsigned int hw = std::thread::hardware_concurrency();
  if (hw == 0) hw = 4;
  unsigned int numThreads = (std::min)(totalChunks, hw);
  if (numThreads < 1) numThreads = 1;
  if (numThreads > 8) numThreads = 8;

  const size_t maxSlots = static_cast<size_t>(highestChunk + 1) << state.FName_BlockOffsetBits;
  state.FNameCacheArray.assign(maxSlots, nullptr);

  std::vector<uint32_t> scannedSlots(highestChunk >= 0 ? static_cast<size_t>(highestChunk) + 1 : 0, 0);

  const size_t arenaSizePerThread = 3 * 1024 * 1024;
  std::vector<std::vector<char>> threadArenas(numThreads);
  for (auto &ar : threadArenas) { ar.resize(arenaSizePerThread); }

  std::atomic<int32_t> nameCount{0};
  std::vector<std::thread> threads;
  threads.reserve(numThreads);

  for (unsigned int t = 0; t < numThreads; t++) {
    threads.emplace_back([&state, &threadArenas, &scannedSlots, chunksStart, blockArrayPtr, highestChunk, numThreads, t, maxSlots, &nameCount]() {
      SafeMem_SetFastMode(1);
      auto &arena = threadArenas[t];
      char *arenaBase = arena.data();
      size_t arenaUsed = 0;
      const size_t arenaCap = arena.size();
      int32_t localCount = 0;

      const size_t maxWalk = (1ULL << state.FName_BlockOffsetBits) * state.FName_Stride;
      std::vector<uint8_t> chunkBuffer(maxWalk > 0 ? maxWalk : 131072);

      for (int chunkIdx = (int)t; chunkIdx <= highestChunk; chunkIdx += (int)numThreads) {
        void *block = nullptr;
        if (state.FName_IsIndirect) {
          if (!blockArrayPtr || !SafeMem_Read((uint8_t *)blockArrayPtr + chunkIdx * 8, &block, 8)) break;
        } else {
          if (!SafeMem_Read((uint8_t *)state.GNames + chunksStart + chunkIdx * 8, &block, 8)) break;
        }
        if (!block) continue;

        if (maxWalk == 0 ||
            SafeMem_ReadSpanExact(block, chunkBuffer.data(), maxWalk) != maxWalk)
          continue;

        uint8_t *data = chunkBuffer.data();
        size_t offset = 0;
        uint32_t currentSlot = 0;
        const int strStart = state.FName_HeaderOff + 2;

        while (offset + strStart < maxWalk) {
          uint16_t meta = *(uint16_t *)(data + offset + state.FName_HeaderOff);
          uint16_t size = (meta >> state.FName_LenShift) & state.FName_LenMask;
          if (size == 0 || size > 1024) break;

          const bool isWide = (meta & 1) != 0;
          const size_t dataBytes = static_cast<size_t>(size) * (isWide ? sizeof(wchar_t) : sizeof(char));
          size_t entrySize = (state.FName_HeaderOff + 2 + dataBytes + state.FName_Stride - 1) & ~(size_t)(state.FName_Stride - 1);
          uint32_t slots = (uint32_t)(entrySize / state.FName_Stride);

          if (offset + strStart + dataBytes > maxWalk) break;

          int32_t nameIdx = (chunkIdx << state.FName_BlockOffsetBits) | currentSlot;

          if (isWide) {
            std::string utf8 = WideToUtf8(reinterpret_cast<const wchar_t *>(data + offset + strStart), size);
            if (!utf8.empty() && utf8[0] != '<') {
              size_t req = utf8.size() + 1;
              if (arenaUsed + req <= arenaCap) {
                char *dst = arenaBase + arenaUsed;
                memcpy(dst, utf8.c_str(), utf8.size());
                dst[utf8.size()] = '\0';
                if ((size_t)nameIdx < maxSlots) {
                  state.FNameCacheArray[nameIdx] = dst;
                  localCount++;
                }
                arenaUsed += req;
              }
            }
          } else {
            const char *ansiPtr = reinterpret_cast<const char *>(data + offset + strStart);
            if (ansiPtr[0] != '<') {
              size_t req = static_cast<size_t>(size) + 1;
              if (arenaUsed + req <= arenaCap) {
                char *dst = arenaBase + arenaUsed;
                memcpy(dst, ansiPtr, size);
                dst[size] = '\0';

                for (size_t i = 0; i < size; i++) {
                  if (dst[i] == '\r' || dst[i] == '\n' || dst[i] == '\t') dst[i] = ' ';
                }
                if ((size_t)nameIdx < maxSlots) {
                  state.FNameCacheArray[nameIdx] = dst;
                  localCount++;
                }
                arenaUsed += req;
              }
            }
          }

          offset += entrySize;
          currentSlot += slots;
        }

        scannedSlots[chunkIdx] = currentSlot;
      }
      nameCount += localCount;
      SafeMem_SetFastMode(0);
    });
  }

  for (auto &th : threads) {
    if (th.joinable()) th.join();
  }

  std::lock_guard<std::recursive_mutex> indexLock(*state.IndexMutex);

  state.FNameThreadArenas = std::move(threadArenas);
  state.StringToFNameIndex.clear();

  state.NameTextToIndex.clear();
  state.FNameExtraNames.clear();
  state.NameTextToIndex.reserve(static_cast<size_t>(nameCount.load()) * 3 / 2);
  for (size_t slot = 0; slot < state.FNameCacheArray.size(); ++slot) {
    const char *text = state.FNameCacheArray[slot];
    if (!text || !text[0]) continue;
    state.NameTextToIndex.try_emplace(std::string_view(text), static_cast<int32_t>(slot));
  }

  state.FNamePoolHighestChunk = highestChunk;
  state.FNamePoolNextSlot =
      (highestChunk >= 0 && static_cast<size_t>(highestChunk) < scannedSlots.size())
          ? static_cast<int32_t>(scannedSlots[highestChunk])
          : 0;

  auto endTime = std::chrono::high_resolution_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
  Log("[Discovery] Built FName cache: %d names in %lldms (text index %zu entries)",
      nameCount.load(), ms, state.NameTextToIndex.size());
}

void RefreshFNamePoolDelta(EngineState &state) {
  if (!state.GNames || state.FNamePoolHighestChunk < 0)
    return;

  std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);

  const int32_t slotsPerChunk = 1 << state.FName_BlockOffsetBits;
  const size_t maxWalk =
      static_cast<size_t>(slotsPerChunk) * static_cast<size_t>(state.FName_Stride);
  if (slotsPerChunk <= 0 || maxWalk == 0)
    return;

  auto readBlock = [&state](int32_t index) -> void * {
    void *block = nullptr;
    if (state.FName_IsIndirect) {
      void *blockArray = nullptr;
      if (!SafeMem_Read(reinterpret_cast<uint8_t *>(state.GNames) + state.FName_ChunksStart,
                        &blockArray, sizeof(blockArray)) ||
          !blockArray)
        return nullptr;
      SafeMem_Read(reinterpret_cast<uint8_t *>(blockArray) +
                       static_cast<size_t>(index) * sizeof(void *),
                   &block, sizeof(block));
    } else {
      SafeMem_Read(reinterpret_cast<uint8_t *>(state.GNames) + state.FName_ChunksStart +
                       static_cast<size_t>(index) * sizeof(void *),
                   &block, sizeof(block));
    }
    return block;
  };

  int32_t chunk = state.FNamePoolHighestChunk;
  int32_t startSlot = state.FNamePoolNextSlot;
  if (startSlot >= slotsPerChunk) {

    ++chunk;
    startSlot = 0;
  }

  if (startSlot < slotsPerChunk) {
    void *cursorBlock = readBlock(chunk);
    if (!cursorBlock) return;
    uint16_t meta = 0;
    const size_t headerAt = static_cast<size_t>(startSlot) *
                                static_cast<size_t>(state.FName_Stride) +
                            static_cast<size_t>(state.FName_HeaderOff);
    const bool readable =
        SafeMem_Read(reinterpret_cast<uint8_t *>(cursorBlock) + headerAt, &meta, sizeof(meta)) != 0;
    const bool empty =
        readable && ((meta >> state.FName_LenShift) & state.FName_LenMask) == 0;
    if (empty && !readBlock(chunk + 1))
      return;
  }

  std::vector<uint8_t> buffer(maxWalk);

  for (int probe = 0; probe < 4; ++probe) {
    void *block = readBlock(chunk);
    if (!block)
      return;

    if (SafeMem_ReadSpanExact(block, buffer.data(), maxWalk) != maxWalk)
      return;

    const uint8_t *data = buffer.data();
    const int strStart = state.FName_HeaderOff + 2;
    size_t offset = 0;
    int32_t slot = 0;
    bool endOfNames = false;

    while (offset + static_cast<size_t>(strStart) < maxWalk) {
      uint16_t meta = 0;
      memcpy(&meta, data + offset + state.FName_HeaderOff, sizeof(meta));
      const uint16_t size = static_cast<uint16_t>((meta >> state.FName_LenShift) &
                                                  state.FName_LenMask);
      if (size == 0 || size > 1024) {
        endOfNames = true;
        break;
      }

      const bool isWide = (meta & 1) != 0;
      const size_t dataBytes =
          static_cast<size_t>(size) * (isWide ? sizeof(wchar_t) : sizeof(char));
      const size_t entrySize =
          (state.FName_HeaderOff + 2 + dataBytes + state.FName_Stride - 1) &
          ~(static_cast<size_t>(state.FName_Stride) - 1);
      if (entrySize == 0) {
        endOfNames = true;
        break;
      }
      const int32_t slots = static_cast<int32_t>(entrySize / static_cast<size_t>(state.FName_Stride));

      if (slot >= startSlot) {
        std::string text =
            isWide ? WideToUtf8(reinterpret_cast<const wchar_t *>(data + offset + strStart), size)
                   : std::string(reinterpret_cast<const char *>(data + offset + strStart), size);

        if (text.find('\0') != std::string::npos) {
          endOfNames = true;
          break;
        }
        for (char &c : text) {
          if (c == '\r' || c == '\n' || c == '\t') c = ' ';
        }

        if (!text.empty() && text[0] != '<' &&
            state.NameTextToIndex.find(std::string_view(text)) == state.NameTextToIndex.end()) {
          const int32_t nameIdx = (chunk << state.FName_BlockOffsetBits) | slot;
          state.FNameExtraNames.push_back(std::move(text));
          const std::string &stored = state.FNameExtraNames.back();
          state.NameTextToIndex.emplace(std::string_view(stored), nameIdx);
          state.FNameCache[nameIdx] = stored;
          state.StringToFNameIndex[stored] = nameIdx;
        }
      }

      offset += entrySize;
      slot += slots;
    }

    state.FNamePoolHighestChunk = chunk;
    state.FNamePoolNextSlot = endOfNames ? slot : slotsPerChunk;

    if (!readBlock(chunk + 1))
      return;
    ++chunk;
    startSlot = 0;
  }
}

std::string FNameToStr(const EngineState &state, const FName &name) {
  if (name.ComparisonIndex == 0)
    return "None";

  std::string baseName;

  if (name.ComparisonIndex >= 0 && (size_t)name.ComparisonIndex < state.FNameCacheArray.size()) {
    const char *ptr = state.FNameCacheArray[name.ComparisonIndex];
    if (ptr) baseName.assign(ptr);
  }

  if (baseName.empty()) {

    if (!state.GNames)
      return "<unknown>";

  int blockMask = (1 << state.FName_BlockOffsetBits) - 1;
  uint32_t chunkIdx = (name.ComparisonIndex >> state.FName_BlockOffsetBits);
  uint32_t strideOff = name.ComparisonIndex & blockMask;

  void *block = nullptr;
  if (state.FName_IsIndirect) {
    void *blockArrayPtr = nullptr;
    if (SafeMem_Read((uint8_t *)state.GNames + state.FName_ChunksStart,
                     &blockArrayPtr, 8) &&
        blockArrayPtr) {
      SafeMem_Read((uint8_t *)blockArrayPtr + chunkIdx * 8, &block, 8);
    }
  } else {
    void *blockAddr =
        (uint8_t *)state.GNames + state.FName_ChunksStart + chunkIdx * 8;
    SafeMem_Read(blockAddr, &block, 8);
  }

  if (!block)
    return "<unknown>";

  uint8_t *data = (uint8_t *)block;
  size_t offset = 0;
  uint32_t currentSlot = 0;
  const size_t maxWalk =
      (1ULL << state.FName_BlockOffsetBits) * state.FName_Stride;

  while (currentSlot <= strideOff && offset < maxWalk) {
    uint16_t meta = 0;
    if (!SafeMem_Read(data + offset + state.FName_HeaderOff, &meta, 2))
      break;

    uint16_t size = (meta >> state.FName_LenShift) & state.FName_LenMask;
    if (size == 0 || size > 1024)
      break;

    const bool isWide = (meta & 1) != 0;
    const size_t dataBytes =
        static_cast<size_t>(size) * (isWide ? sizeof(wchar_t) : sizeof(char));
    size_t entrySize =
        (state.FName_HeaderOff + 2 + dataBytes + state.FName_Stride - 1) &
        ~(size_t)(state.FName_Stride - 1);
    uint32_t slots = (uint32_t)(entrySize / state.FName_Stride);

    if (strideOff >= currentSlot && strideOff < currentSlot + slots) {
      std::string result = DecodeFNameEntryString(state, data, offset, meta);
      if (result.empty())
        return "<bad_entry>";

      std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);
      const_cast<EngineState &>(state).FNameCache[name.ComparisonIndex] =
          result;
      baseName = result;
      break;
    }

    offset += entrySize;
    currentSlot += slots;
  }

  if (baseName.empty())
    return "<unresolved>";

  }

  if (name.Number > 0 && !state.bUseOutlineNumber && !baseName.empty() &&
      baseName[0] != '<') {
    baseName += '_';
    baseName += std::to_string(name.Number - 1);
  }
  return baseName;
}

std::string GetObjectNameStr(const EngineState &state, UObject *obj) {
  if (!obj ||
      !IsReadable((uint8_t *)obj + state.UObject_NameOff, sizeof(FName)))
    return "";
  FName name = {};
  if (!SafeMem_Read((uint8_t *)obj + state.UObject_NameOff, &name,
                    sizeof(name)))
    return "";
  return FNameToStr(state, name);
}

std::string BuildPathName(const EngineState &state, UObject *obj) {
  if (!obj)
    return "";

  std::string result = GetObjectNameStr(state, obj);
  UObject *outer = GetObjectOuter(state, obj);
  int depth = 0;
  while (outer && depth++ < 50) {
    char separator = '.';
    if (state.UPackageClass &&
        GetObjectClass(state, outer) != state.UPackageClass) {
      UObject *outerOfOuter = GetObjectOuter(state, outer);
      if (outerOfOuter && GetObjectClass(state, outerOfOuter) == state.UPackageClass)
        separator = ':';
    }
    result = GetObjectNameStr(state, outer) + separator + result;
    outer = GetObjectOuter(state, outer);
  }
  return result;
}

std::string BuildFullName(const EngineState &state, UObject *obj) {
  if (!obj)
    return "";

  const std::string path = BuildPathName(state, obj);
  UClass *cls = GetObjectClass(state, obj);
  if (!cls)
    return path;
  return GetObjectNameStr(state, cls) + " " + path;
}

bool DiscoverFNameLayout(EngineState &state) {
  if (!state.GNames || state.UObject_NameOff < 0) {
    Log("[Discovery] Cannot detect FName layout: missing GNames or NameOff");
    return false;
  }

  void *objectsPtr = nullptr;
  SafeMem_Read((uint8_t *)state.GUObjectArray + state.TUObjectArray_ObjectsOff,
               &objectsPtr, 8);
  void *chunk0 = nullptr;
  if (objectsPtr && SafeMem_Read(objectsPtr, &chunk0, 8) && chunk0) {
    const int objOff =
        state.FUObjectItem_ObjectOff;
    int presetSize = state.FUObjectItem_Size;
    int presetScore = presetSize > 0
                          ? ScoreFUObjectItemLayout(state, chunk0, presetSize,
                                                    objOff)
                          : -1;

    int bestSize = presetSize > 0 ? presetSize : 24;
    int bestScore = presetScore;
    for (int testSize = 12; testSize <= 64; testSize += 4) {
      if (testSize == presetSize)
        continue;

      int testScore = ScoreFUObjectItemLayout(state, chunk0, testSize, objOff);
      if (testScore > bestScore ||
          (testScore == bestScore && testSize < bestSize)) {
        bestScore = testScore;
        bestSize = testSize;
      }
    }

    if (presetSize > 0 && bestSize != presetSize && bestScore <= presetScore + 4) {
      bestSize = presetSize;
      bestScore = presetScore;
    }

    state.UObjectArray_ItemSize = bestSize;
    state.FUObjectItem_Size = bestSize;

    if (presetSize > 0 && bestSize != presetSize) {
      Log("[Discovery] FUObjectItem size auto-corrected: %d -> %d bytes "
          "(score: %d vs %d, objOff=%d)",
          presetSize, bestSize, bestScore, presetScore, objOff);
    } else {
      if (bestScore == 208) {
        Log("[Discovery] FUObjectItem size confirmed: %d bytes (100%% perfect match, ItemObjOff=%d)",
            state.UObjectArray_ItemSize, state.FUObjectItem_ObjectOff);
      } else {
        Log("[Discovery] FUObjectItem size confirmed: %d bytes (ItemObjOff=%d, "
            "score=%d)",
            state.UObjectArray_ItemSize, state.FUObjectItem_ObjectOff,
            bestScore);
      }
    }
  }

  UObject *firstObj = GetObjectByIndex(state, 0);
  if (!firstObj)
    return false;

  uint8_t *nameAddr = (uint8_t *)firstObj + state.UObject_NameOff;
  if (!IsReadable(nameAddr, 16))
    return false;

  int32_t FNameFirstInt = *(int32_t *)(nameAddr + 0);
  int32_t FNameSecondInt = *(int32_t *)(nameAddr + 4);

  int32_t FNameSize = state.UObject_OuterOff - state.UObject_NameOff;
  if (FNameSize <= 0 || FNameSize > 24) {

    FNameSize = 8;
  }

  constexpr float MinPercentage = 0.03f;
  int32_t namesWithNumberOneToFour = 0;
  int32_t totalNames = 0;

  int32_t count = GetObjectCount(state);
  for (int i = 0; i < std::min(count, 500); i++) {
    UObject *obj = GetObjectByIndex(state, i);
    if (!obj)
      continue;

    uint8_t *objNameAddr = (uint8_t *)obj + state.UObject_NameOff;
    if (!IsReadable(objNameAddr, 8))
      continue;

    uint32_t number = *(uint32_t *)(objNameAddr + 4);
    if (number > 0 && number < 5) {
      namesWithNumberOneToFour++;
    }
    totalNames++;
  }

  float percentage =
      totalNames > 0 ? (float)namesWithNumberOneToFour / totalNames : 0;

  if (FNameSize == 8 && FNameFirstInt == FNameSecondInt) {

    state.bUseCasePreservingName = true;
    state.bUseOutlineNumber = true;
    state.FName_Size = 8;
    state.FName_NumberOff = -1;
    Log("[Discovery] FName layout: CasePreserving + OutlineNumber (size=%d)",
        FNameSize);
  } else if (FNameSize >= 12) {

    state.bUseCasePreservingName = true;
    state.bUseOutlineNumber = false;
    state.FName_Size = FNameSize;
    state.FName_NumberOff = (FNameFirstInt == FNameSecondInt) ? 8 : 4;
    Log("[Discovery] FName layout: CasePreserving (size=%d, NumberOff=%d)",
        FNameSize, state.FName_NumberOff);
  } else if (percentage < MinPercentage) {

    bool foundNumberedDuplicate = false;
    {
      std::unordered_map<uint32_t, uint32_t> seenNames;
      seenNames.reserve(4096);
      int32_t verifyCount = GetObjectCount(state);
      for (int i = 0; i < std::min(verifyCount, 3000) && !foundNumberedDuplicate; i++) {
        UObject *obj = GetObjectByIndex(state, i);
        if (!obj)
          continue;
        uint8_t *objNameAddr = (uint8_t *)obj + state.UObject_NameOff;
        if (!IsReadable(objNameAddr, 8))
          continue;
        uint32_t compIdx = *(uint32_t *)objNameAddr;
        uint32_t number = *(uint32_t *)(objNameAddr + 4);
        if (compIdx == 0)
          continue;
        auto it = seenNames.find(compIdx);
        if (it == seenNames.end()) {
          seenNames.emplace(compIdx, number);
        } else if (it->second != number) {
          foundNumberedDuplicate = true;
        }
      }
    }

    if (foundNumberedDuplicate) {

      state.bUseCasePreservingName = false;
      state.bUseOutlineNumber = false;
      state.FName_Size = 8;
      state.FName_NumberOff = 4;
      Log("[Discovery] FName layout: Default (size=8, NumberOff=4, verified "
          "via duplicate numbered object names)");
    } else {

      state.bUseCasePreservingName = false;
      state.FName_Size = FNameSize;
      state.FName_NumberOff = (FNameSize >= 8) ? 4 : -1;
      state.bUseOutlineNumber = (state.FName_NumberOff < 0);
      Log("[Discovery] FName layout: %s (size=%d, NumberOff=%d, from the "
          "%d bytes the object layout reserves for the name, percentage=%.3f)",
          state.bUseOutlineNumber ? "OutlineNumber" : "Default",
          state.FName_Size, state.FName_NumberOff, FNameSize, percentage);
    }
  } else {

    state.bUseCasePreservingName = false;
    state.bUseOutlineNumber = false;
    state.FName_Size = 8;
    state.FName_NumberOff = 4;
    Log("[Discovery] FName layout: Default (size=8, NumberOff=4, "
        "percentage=%.3f)",
        percentage);
  }

  state.bFNameLayoutDiscovered = true;
  return true;
}

}
