#include "ObjectArray.h"
#include "Logger.h"
#include "../utils/SafeMemory.h"

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

static bool HasSequentialInternalIndices(const EngineState &state, void *chunk,
                                         int itemSize, int objOff,
                                         int sampleCount = 16) {
  int matches = 0;
  int mismatches = 0;

  for (int i = 0; i < sampleCount; ++i) {
    void *obj = nullptr;
    if (!SafeMem_Read((uint8_t *)chunk + i * itemSize + objOff, &obj,
                      sizeof(obj)) ||
        !obj) {
      continue;
    }

    const uintptr_t objAddr = reinterpret_cast<uintptr_t>(obj);
    if (objAddr < 0x10000 || objAddr >= 0x7FFFFFFFFFFFULL ||
        !IsReadable(obj, state.UObject_IndexOff + sizeof(int32_t))) {
      continue;
    }

    int32_t internalIndex = -1;
    if (!SafeMem_Read((uint8_t *)obj + state.UObject_IndexOff,
                      &internalIndex, sizeof(internalIndex))) {
      continue;
    }

    if (internalIndex == i) {
      ++matches;
    } else {
      ++mismatches;
    }
  }

  return matches >= 5 && mismatches == 0;
}

static constexpr int32_t kSourceNumElementsPerChunk = 64 * 1024;
static constexpr int32_t kMaxChunksScanned = 512;

static int32_t ChunkSizeFromInternalIndex(const EngineState &state, void *chunk,
                                         int32_t chunkIdx, int32_t itemSize,
                                         int32_t objOff, int32_t numElements) {
  for (int32_t slot = 0; slot < 64; ++slot) {
    UObject *obj = nullptr;
    if (!SafeMem_Read((uint8_t *)chunk + slot * itemSize + objOff, &obj,
                      sizeof(obj)) ||
        !obj)
      continue;

    const uintptr_t objAddr = reinterpret_cast<uintptr_t>(obj);
    if (objAddr < 0x10000 || objAddr >= 0x7FFFFFFFFFFFULL)
      continue;

    int32_t internalIndex = -1;
    if (!SafeMem_Read((uint8_t *)obj + state.UObject_IndexOff, &internalIndex,
                      sizeof(internalIndex)))
      continue;

    const int32_t span = internalIndex - slot;
    if (span <= 0 || (span % chunkIdx) != 0)
      continue;

    const int32_t chunkSize = span / chunkIdx;
    if (chunkSize <= 0 || chunkIdx * chunkSize + slot >= numElements)
      continue;

    int32_t matches = 0, mismatches = 0;
    for (int32_t k = 1; k <= 16; ++k) {
      UObject *next = nullptr;
      if (!SafeMem_Read((uint8_t *)chunk + (slot + k) * itemSize + objOff,
                        &next, sizeof(next)) ||
          !next)
        continue;
      int32_t nextIndex = -1;
      if (!SafeMem_Read((uint8_t *)next + state.UObject_IndexOff, &nextIndex,
                        sizeof(nextIndex)))
        continue;
      if (nextIndex == chunkIdx * chunkSize + slot + k)
        ++matches;
      else
        ++mismatches;
    }
    if (matches >= 4 && mismatches == 0)
      return chunkSize;
  }
  return 0;
}

static int32_t CountChunksReadable(void *objectsPtr, int32_t cap) {
  int32_t chunks = 0;
  while (chunks < cap) {
    if (!SafeMem_ReadPtr((uint8_t *)objectsPtr + chunks * sizeof(void *)))
      break;
    ++chunks;
  }
  return chunks;
}

static void RecordChunkGeometry(EngineState &state, int32_t numElements) {
  if (numElements <= 0 || !state.GUObjectArray)
    return;

  void *objectsPtr = SafeMem_ReadPtr((uint8_t *)state.GUObjectArray +
                                    state.TUObjectArray_ObjectsOff);
  if (!objectsPtr)
    return;

  const int32_t itemSize =
      state.UObjectArray_ItemSize > 0 ? state.UObjectArray_ItemSize : 24;
  const int32_t objOff =
      state.FUObjectItem_ObjectOff >= 0 ? state.FUObjectItem_ObjectOff : 0;

  int32_t measured = 0;
  int32_t statedBy = 0;
  for (int32_t chunkIdx = 1; chunkIdx <= 3; ++chunkIdx) {
    void *chunk =
        SafeMem_ReadPtr((uint8_t *)objectsPtr + chunkIdx * sizeof(void *));
    if (!chunk)
      break;

    const int32_t chunkSize = ChunkSizeFromInternalIndex(
        state, chunk, chunkIdx, itemSize, objOff, numElements);
    if (chunkSize <= 0)
      continue;
    ++statedBy;
    if (measured == 0)
      measured = chunkSize;
    else if (measured != chunkSize)
      measured = 0;
  }
  if (measured == 0)
    statedBy = 0;

  state.UObjectArray_NumElementsPerChunk = measured;
  state.UObjectArray_NumChunks = CountChunksReadable(objectsPtr, kMaxChunksScanned);

  const int32_t perChunk =
      measured > 0 ? measured : kSourceNumElementsPerChunk;
  const int32_t needed = (numElements + perChunk - 1) / perChunk;

  if (measured > 0) {
    Log("[Discovery] TUObjectArray geometry: NumElementsPerChunk=%d measured from the "
        "InternalIndex the objects carry (%d chunk(s) stated it; chunks readable=%d, "
        "Num=%d needs %d)",
        perChunk, statedBy, state.UObjectArray_NumChunks, numElements, needed);
  } else {
    Log("[Discovery] TUObjectArray geometry: NumElementsPerChunk=%d from "
        "Public/UObject/UObjectArray.h; this build's own chunks stated none "
        "(chunks readable=%d, Num=%d needs %d)",
        perChunk, state.UObjectArray_NumChunks, numElements, needed);
  }

  if (state.UObjectArray_NumChunks < needed) {
    Log("[Discovery] TUObjectArray geometry: %d chunk(s) readable but %d needed -- "
        "element(s) past chunk %d cannot be reached by a walk on this grid",
        state.UObjectArray_NumChunks, needed, state.UObjectArray_NumChunks - 1);
  }
}

int32_t GetElementsPerChunk(const EngineState &state) {
  return state.UObjectArray_NumElementsPerChunk > 0
             ? state.UObjectArray_NumElementsPerChunk
             : kSourceNumElementsPerChunk;
}

bool DiscoverTUObjectArrayLayout(EngineState &state) {
  if (!state.GUObjectArray)
    return false;

  uint8_t buf[0x60];
  if (!SafeMem_Read(state.GUObjectArray, buf, sizeof(buf))) {
    static int readFailCount = 0;
    if (readFailCount < 3)
      Log("[Discovery] GUObjectArray address %p is not readable",
          state.GUObjectArray);
    readFailCount++;
    return false;
  }

  struct Candidate {
    int ptrOff, numOff, objOff, itemSize;
    int32_t numElements;
  };
  Candidate best = {-1, -1, -1, 0x18, 0};

  for (int ptrOff = 0; ptrOff <= 0x30; ptrOff += 8) {
    void *objectsPtr = *(void **)(buf + ptrOff);

    if (!objectsPtr || reinterpret_cast<uintptr_t>(objectsPtr) < 0x10000)
      continue;

    for (int maxOff = 0; maxOff <= 0x48; maxOff += 4) {
      if (maxOff == ptrOff || maxOff == ptrOff + 4)
        continue;

      int32_t maxElements = *(int32_t *)(buf + maxOff);
      if (maxElements <= 65536 || maxElements > 30000000)
        continue;
      if ((maxElements % 16) != 0)
        continue;

      for (int numOff = 0; numOff <= 0x48; numOff += 4) {
        if (numOff == ptrOff || numOff == ptrOff + 4 || numOff == maxOff)
          continue;

          int32_t numElements = *(int32_t *)(buf + numOff);
          if (numElements <= 2048)
            continue;
          if (numElements > maxElements)
            continue;

        for (int maxChunksOff = 0; maxChunksOff <= 0x48; maxChunksOff += 4) {
          if (maxChunksOff == ptrOff || maxChunksOff == ptrOff + 4 ||
              maxChunksOff == maxOff || maxChunksOff == numOff)
            continue;

          int32_t maxChunks = *(int32_t *)(buf + maxChunksOff);
          if (maxChunks < 6 || maxChunks > 4000)
            continue;

          for (int numChunksOff = 0; numChunksOff <= 0x48; numChunksOff += 4) {
            if (numChunksOff == ptrOff || numChunksOff == ptrOff + 4 ||
                numChunksOff == maxOff || numChunksOff == numOff ||
                numChunksOff == maxChunksOff)
              continue;

            int32_t numChunks = *(int32_t *)(buf + numChunksOff);
            if (numChunks < 1 || numChunks > 150)
              continue;
            if (numChunks > maxChunks)
              continue;

            int32_t elementsPerChunk = maxElements / maxChunks;
            if ((elementsPerChunk % 16) != 0)
              continue;
            if (elementsPerChunk < 32768 || elementsPerChunk > 524288)
              continue;

            int32_t expectedChunks = (numElements / elementsPerChunk) + 1;
            if (expectedChunks != numChunks)
              continue;

            void *firstChunk = SafeMem_ReadPtr(objectsPtr);
            if (!firstChunk ||
                reinterpret_cast<uintptr_t>(firstChunk) < 0x10000)
              continue;

            if (reinterpret_cast<uintptr_t>(firstChunk) >= 0x7FF000000000ULL)
              continue;

            uint8_t itemBuf[64];
            if (!SafeMem_Read(firstChunk, itemBuf, sizeof(itemBuf)))
              continue;

            int uobjectOffset = -1;
            int detectedItemSize = 0x18;
            for (int off = 0; off <= 0x20; off += 4) {
              void *ptr = *(void **)(&itemBuf[off]);
              uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);

              if (addr < 0x10000 || addr >= 0x7FF000000000ULL)
                continue;

              void *uclass = SafeMem_ReadPtr((uint8_t *)ptr + 0x10);
              if (!uclass)
                continue;
              uintptr_t uclassAddr = reinterpret_cast<uintptr_t>(uclass);
              if (uclassAddr < 0x10000 || uclassAddr >= 0x7FF000000000ULL)
                continue;

              void *metacls = SafeMem_ReadPtr((uint8_t *)uclass + 0x10);
              if (!metacls)
                continue;
              uintptr_t metaclsAddr = reinterpret_cast<uintptr_t>(metacls);
              if (metaclsAddr < 0x10000 || metaclsAddr >= 0x7FF000000000ULL)
                continue;
              void *metameta = SafeMem_ReadPtr((uint8_t *)metacls + 0x10);
              if (metacls != uclass && metacls != metameta)
                continue;

              uobjectOffset = off;

              detectedItemSize = (off >= 0x10) ? 0x20 : 0x18;
              break;
            }

            if (uobjectOffset < 0)
              continue;

            int candidateSizes[] = {0x18, 0x20, 0x28};
            int bestItemScore = -1;
            for (int candidateSize : candidateSizes) {
              if (candidateSize < uobjectOffset + 8)
                continue;

              const int score = ScoreFUObjectItemLayout(
                  state, firstChunk, candidateSize, uobjectOffset);
              if (score > bestItemScore ||
                  (score == bestItemScore && candidateSize < detectedItemSize)) {
                bestItemScore = score;
                detectedItemSize = candidateSize;
              }
            }

            if (!HasSequentialInternalIndices(state, firstChunk,
                                              detectedItemSize,
                                              uobjectOffset)) {
              continue;
            }

            Log("[Discovery] TUObjectArray Candidate: PtrOff=%d NumOff=%d "
                "Num=%d MaxElems=%d Chunks=%d/%d UObjOff=%d ItemSize=%d",
                ptrOff, numOff, numElements, maxElements, numChunks, maxChunks,
                uobjectOffset, detectedItemSize);

            if (numElements > best.numElements) {
              best.ptrOff = ptrOff;
              best.numOff = numOff;
              best.objOff = uobjectOffset;
              best.itemSize = detectedItemSize;
              best.numElements = numElements;
            }
          }
        }
      }
    }
  }

  if (best.ptrOff >= 0) {
    state.TUObjectArray_ObjectsOff = best.ptrOff;
    state.TUObjectArray_NumElementsOff = best.numOff;
    state.FUObjectItem_ObjectOff = best.objOff;
    state.FUObjectItem_Size = best.itemSize;
    state.UObjectArray_ItemSize = best.itemSize;

    static bool bLoggedSuccess = false;
    if (!bLoggedSuccess) {
      Log("[Discovery] TUObjectArray Discovered: ObjOff=%d NumOff=%d Num=%d "
          "UObjectOff=%d ItemSize=0x%X",
          best.ptrOff, best.numOff, best.numElements, best.objOff,
          best.itemSize);
      bLoggedSuccess = true;
      RecordChunkGeometry(state, best.numElements);
    }
    return true;
  }

  {
    static bool loggedFallback = false;
    if (!loggedFallback) {
      Log("[Discovery] Complex array heuristic failed, trying naive pointer "
          "matching");
      loggedFallback = true;
    }
  }
  for (int ptrOff = 0; ptrOff <= 0x30; ptrOff += 8) {
    void *objectsPtr = *(void **)(buf + ptrOff);
    if (!objectsPtr || reinterpret_cast<uintptr_t>(objectsPtr) < 0x10000)
      continue;

    if (reinterpret_cast<uintptr_t>(objectsPtr) >= 0x7FFFFFFFFFFFULL)
      continue;

      void *firstChunk = SafeMem_ReadPtr(objectsPtr);
      if (!firstChunk || (uintptr_t)firstChunk < 0x10000)
        continue;
      if (reinterpret_cast<uintptr_t>(firstChunk) >= 0x7FFFFFFFFFFFULL)
        continue;

      struct BestNaive {
        int objOff = -1;
        int itemSize = -1;
        int score = -1;
        int validCount = 0;
      } bestLayout;

      for (int candidateObjOff = 0; candidateObjOff <= 24;
           candidateObjOff += 4) {

        void *obj =
            SafeMem_ReadPtr((uint8_t *)firstChunk + candidateObjOff);
        if (!obj)
          continue;
        uintptr_t objAddr = reinterpret_cast<uintptr_t>(obj);
        if (objAddr < 0x10000 || objAddr >= 0x7FFFFFFFFFFFULL)
          continue;

        void *cls = SafeMem_ReadPtr((uint8_t *)obj + 0x10);
        if (!cls)
          continue;
        uintptr_t clsAddr = reinterpret_cast<uintptr_t>(cls);
        if (clsAddr < 0x10000 || clsAddr >= 0x7FFFFFFFFFFFULL)
          continue;

        void *metacls = SafeMem_ReadPtr((uint8_t *)cls + 0x10);
        if (!metacls)
          continue;
        uintptr_t metaclsAddr = reinterpret_cast<uintptr_t>(metacls);
        if (metaclsAddr < 0x10000 || metaclsAddr >= 0x7FFFFFFFFFFFULL)
          continue;
        void *metameta = SafeMem_ReadPtr((uint8_t *)metacls + 0x10);
        bool validChain = (metacls == cls) || (metacls == metameta);
        if (!validChain)
          continue;

        void *commonMetaCls = metacls;

        const int minItemSize = ((candidateObjOff + 8) + 3) & ~3;
        for (int candidateItemSize = minItemSize; candidateItemSize <= 64;
             candidateItemSize += 4) {

          int validCount = 1;
          for (int k = 1; k < 10; k++) {
            void *objK =
                SafeMem_ReadPtr((uint8_t *)firstChunk +
                                k * candidateItemSize + candidateObjOff);
            if (!objK)
              continue;
            uintptr_t objKAddr = reinterpret_cast<uintptr_t>(objK);
            if (objKAddr < 0x10000 || objKAddr >= 0x7FFFFFFFFFFFULL)
              continue;
            void *clsK = SafeMem_ReadPtr((uint8_t *)objK + 0x10);
            if (!clsK)
              continue;
            uintptr_t clsKAddr = reinterpret_cast<uintptr_t>(clsK);
            if (clsKAddr < 0x10000 || clsKAddr >= 0x7FFFFFFFFFFFULL)
              continue;
            void *metaK = SafeMem_ReadPtr((uint8_t *)clsK + 0x10);
            if (metaK == commonMetaCls || clsK == commonMetaCls)
              validCount++;
          }

          if (validCount < 5)
            continue;

          int layoutScore =
              ScoreFUObjectItemLayout(state, firstChunk, candidateItemSize,
                                     candidateObjOff, 10);
          if (layoutScore > bestLayout.score ||
              (layoutScore == bestLayout.score &&
               candidateItemSize < bestLayout.itemSize)) {
            bestLayout.objOff = candidateObjOff;
            bestLayout.itemSize = candidateItemSize;
            bestLayout.score = layoutScore;
            bestLayout.validCount = validCount;
          }
        }
      }

      if (bestLayout.objOff < 0)
        continue;

      if (!HasSequentialInternalIndices(state, firstChunk,
                                        bestLayout.itemSize,
                                        bestLayout.objOff)) {
        continue;
      }

      {
        int32_t bestNum = 0;
        int bestNumOff = -1;

        for (int numOff = 0x48; numOff >= 0x08; numOff -= 4) {
          if (numOff == ptrOff || numOff == ptrOff + 4)
            continue;
          if (numOff - 4 == ptrOff || numOff - 4 == ptrOff + 4)
            continue;

          int32_t numElements = *(int32_t *)(buf + numOff);
          int32_t maxElements = *(int32_t *)(buf + numOff - 4);

          if (numElements > 1000 && numElements <= 30000000 &&
              maxElements >= numElements && maxElements <= 30000000 &&
              maxElements > 1000) {
            bestNum = numElements;
            bestNumOff = numOff;
            break;
          }
        }

        if (bestNumOff < 0) {
          for (int numOff = 0x04; numOff <= 0x48; numOff += 4) {
            if (numOff == ptrOff || numOff == ptrOff + 4)
              continue;
            int32_t candidate = *(int32_t *)(buf + numOff);
            if (candidate > 1000 && candidate <= 30000000) {
              if (candidate > bestNum) {
                bestNum = candidate;
                bestNumOff = numOff;
              }
            }
          }
        }

        if (bestNumOff < 0)
          continue;

        state.TUObjectArray_ObjectsOff = ptrOff;
        state.TUObjectArray_NumElementsOff = bestNumOff;
        state.FUObjectItem_ObjectOff = bestLayout.objOff;        state.FUObjectItem_Size = bestLayout.itemSize;
        state.UObjectArray_ItemSize = bestLayout.itemSize;

        static bool bLoggedSuccess = false;
        if (!bLoggedSuccess) {
          Log("[Discovery] TUObjectArray Discovered: ObjOff=%d NumOff=%d Num=%d "
              "UObjectOff=%d ItemSize=0x%X",
              ptrOff, bestNumOff, bestNum, bestLayout.objOff,
              bestLayout.itemSize);
          bLoggedSuccess = true;
          RecordChunkGeometry(state, bestNum);
        }
        return true;
    }
  }

  return false;
}

int32_t GetObjectCount(const EngineState &state) {
  if (!state.GUObjectArray)
    return 0;
  int32_t count = 0;
  if (!SafeMem_Read((uint8_t *)state.GUObjectArray +
                        state.TUObjectArray_NumElementsOff,
                    &count, sizeof(count)))
    return 0;
  return count;
}

UObject *GetObjectByIndex(const EngineState &state, int32_t index) {
  if (!state.GUObjectArray)
    return nullptr;

  void *objectsPtr = SafeMem_ReadPtr((uint8_t *)state.GUObjectArray +
                                     state.TUObjectArray_ObjectsOff);
  if (!objectsPtr)
    return nullptr;

  const int32_t elementsPerChunk = GetElementsPerChunk(state);
  const int32_t chunkIdx = index / elementsPerChunk;
  const int32_t withinChunk = index % elementsPerChunk;

  void *chunk =
      SafeMem_ReadPtr((uint8_t *)objectsPtr + chunkIdx * sizeof(void *));
  if (!chunk)
    return nullptr;

  void *objectPtr = nullptr;
  if (!SafeMem_Read((uint8_t *)chunk +
                        withinChunk * state.UObjectArray_ItemSize +
                        state.FUObjectItem_ObjectOff,
                    &objectPtr, 8) ||
      !objectPtr)
    return nullptr;

  return (UObject *)objectPtr;
}

int32_t ForEachObject(const EngineState &state, ObjectIteratorCallback callback,
                      void *userdata) {
  int32_t count = GetObjectCount(state);
  int32_t processed = 0;

  for (int32_t i = 0; i < count; i++) {
    UObject *obj = GetObjectByIndex(state, i);
    if (!obj || !IsReadable(obj, 0x30))
      continue;

    if (!callback(obj, i, userdata))
      break;
    processed++;
  }
  return processed;
}

}
