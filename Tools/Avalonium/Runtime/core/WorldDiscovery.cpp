#include "WorldDiscovery.h"
#include "NameResolution.h"
#include "ObjectArray.h"
#include "ObjectUtils.h"
#include "FunctionFinder.h"
#include "Logger.h"
#include "../utils/SafeMemory.h"
#include "../console/ConsoleInternal.h"
#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace EngineDiscovery {

namespace {

bool LooksLikeWorld(const EngineState &state, const void *candidate) {
  if (!candidate || !SafeMem_IsReadable(const_cast<void *>(candidate), 0x28))
    return false;

  UObject *object = reinterpret_cast<UObject *>(const_cast<void *>(candidate));
  UClass *cls = GetObjectClass(state, object);
  if (!cls)
    return false;

  if (state.UWorldClass)
    return IsStructA(state, reinterpret_cast<UStruct *>(cls),
                     reinterpret_cast<UStruct *>(state.UWorldClass));

  for (int32_t depth = 0; depth < 32 && cls; ++depth) {
    const std::string name = GetObjectNameStr(state, reinterpret_cast<UObject *>(cls));
    if (_stricmp(name.c_str(), "World") == 0)
      return true;
    cls = GetSuperStruct(state, reinterpret_cast<UStruct *>(cls));
  }
  return false;
}

}

UObject *GetLiveWorld(EngineState &state) {
  void *slot = nullptr;
  if (state.GWorldPtr) {
    SafeMem_Read(state.GWorldPtr, &slot, sizeof(void *));
  }

  if (LooksLikeWorld(state, slot))
    return static_cast<UObject *>(slot);
  if (LooksLikeWorld(state, state.GWorld))
    return state.GWorld;

  static bool s_LoggedUnverified = false;
  if (!s_LoggedUnverified) {
    s_LoggedUnverified = true;
    Log("[Discovery] No world passed validation (slot=%p instance=%p); answering with the "
        "recovered instance", slot, state.GWorld);
  }
  return state.GWorld ? state.GWorld : static_cast<UObject *>(slot);
}

bool DiscoverWorldLayout(EngineState &state) {
  if (state.GWorldPtr) {
    void *currentWorld = nullptr;
    if (SafeMem_Read(state.GWorldPtr, &currentWorld, sizeof(void *)) && currentWorld && SafeMem_IsReadable(currentWorld, 0x28)) {
      state.GWorld = currentWorld;
    }
  }

  uint8_t worldBuf[0x200];
  bool needScan = !state.GWorld || !SafeMem_Read(state.GWorld, worldBuf, sizeof(worldBuf));

  if (!needScan) {

    void *worldCls = *(void **)(worldBuf + state.UObject_ClassOff);
    if (!worldCls || (state.UWorldClass && worldCls != state.UWorldClass)) {
      std::string worldClassName = GetObjectNameStr(state, (UObject *)worldCls);
      if (worldClassName != "World") needScan = true;
    }
  }

  if (needScan) {
    bool foundFallback = false;

    if (state.UWorldClass && !state.ClassToInstances.empty()) {
      auto it = state.ClassToInstances.find(state.UWorldClass);
      if (it != state.ClassToInstances.end() && !it->second.empty()) {
        for (auto rit = it->second.rbegin(); rit != it->second.rend(); ++rit) {
          UObject *obj = *rit;
          uint32_t flags = 0;
          if (SafeMem_Read((uint8_t *)obj + state.UObject_FlagsOff, &flags, sizeof(flags)) && !(flags & 0x00000010)) {
            state.GWorld = obj;
            foundFallback = true;
            Log("[Discovery] Recovered UWorld instance from index at: %p", state.GWorld);
            break;
          }
        }
      }
    }

    if (!foundFallback) {
      Log("[Discovery] Launching fast object scan for active UWorld instance...");
      int32_t count = GetObjectCount(state);
      void *knownWorldClass = state.UWorldClass;

      for (int i = count - 1; i >= 0; i--) {
        UObject *obj = GetObjectByIndex(state, i);
        if (!obj || !IsReadable(obj, 0x28)) continue;

        uint32_t flags = 0;
        if (!SafeMem_Read((uint8_t *)obj + state.UObject_FlagsOff, &flags, sizeof(flags))) continue;
        if (flags & 0x00000010) continue;

        void *cls = SafeMem_ReadPtr((uint8_t *)obj + state.UObject_ClassOff);
        if (!cls) continue;

        bool isWorld = false;
        if (knownWorldClass) {
          isWorld = (cls == knownWorldClass);
        } else {
          std::string clsName = GetObjectNameStr(state, (UObject *)cls);
          isWorld = (clsName == "World");
          if (isWorld) {
            state.UWorldClass = (UClass *)cls;
            knownWorldClass = cls;
          }
        }

        if (isWorld) {
          state.GWorld = obj;
          Log("[Discovery] Recovered UWorld instance natively at: %p", state.GWorld);
          foundFallback = true;
          break;
        }
      }
    }

    if (!foundFallback || !state.GWorld || !SafeMem_Read(state.GWorld, worldBuf, sizeof(worldBuf))) {
      Log("[Discovery] Fallback failed. Could not find any active UWorld instance.");
      return false;
    }
  }

  for (int off = 0x28; off <= 0x1F0; off += 8) {
    void *candidate = *(void **)(worldBuf + off);
    if (!candidate)
      continue;
    if ((uintptr_t)candidate < 0x10000 ||
        (uintptr_t)candidate >= 0x7FFFFFFFFFFFULL)
      continue;

    void *candCls =
        SafeMem_ReadPtr((uint8_t *)candidate + state.UObject_ClassOff);
    if (!candCls)
      continue;

    std::string clsName = GetObjectNameStr(state, (UObject *)candCls);
    if (clsName != "Level")
      continue;

    state.UWorld_PersistentLevelOff = off;
    Log("[Discovery] Found UWorld::PersistentLevel at offset 0x%X -> %p", off,
        candidate);

    uint8_t levelBuf[0x300];
    if (!SafeMem_Read(candidate, levelBuf, sizeof(levelBuf)))
      break;

    int bestActOff = -1;
    int bestScore = 0;

    for (int actOff = 0x28; actOff <= 0x2F0; actOff += 8) {
      void *dataPtr = *(void **)(levelBuf + actOff);
      if (!dataPtr)
        continue;
      if ((uintptr_t)dataPtr < 0x10000 ||
          (uintptr_t)dataPtr >= 0x7FFFFFFFFFFFULL)
        continue;

      int32_t arrCount = *(int32_t *)(levelBuf + actOff + 8);
      int32_t arrMax = *(int32_t *)(levelBuf + actOff + 12);
      if (arrCount < 1 || arrCount > 200000 || arrMax < arrCount)
        continue;

      const int checkCount = std::min(arrCount, 10);
      uint8_t entryBuf[80];
      if (!SafeMem_Read(dataPtr, entryBuf, checkCount * 8))
        continue;

      int actorChainScore = 0;
      int vtableScore = 0;

      for (int e = 0; e < checkCount; e++) {
        void *entry = *(void **)(entryBuf + e * 8);
        if (!entry)
          continue;
        if ((uintptr_t)entry < 0x10000 || (uintptr_t)entry >= 0x7FFFFFFFFFFFULL)
          continue;

        void *vtbl = SafeMem_ReadPtr(entry);
        if (!vtbl || (uintptr_t)vtbl < 0x10000 ||
            (uintptr_t)vtbl >= 0x7FFFFFFFFFFFULL)
          continue;

        void *entryCls =
            SafeMem_ReadPtr((uint8_t *)entry + state.UObject_ClassOff);
        if (!entryCls || (uintptr_t)entryCls < 0x10000 ||
            (uintptr_t)entryCls >= 0x7FFFFFFFFFFFULL)
          continue;

        vtableScore++;

        if (state.AActorClass) {
          UStruct *cur = (UStruct *)entryCls;
          for (int depth = 0; depth < 20 && cur; depth++) {
            if (cur == (UStruct *)state.AActorClass) {
              actorChainScore++;
              break;
            }
            cur = GetSuperStruct(state, cur);
          }
        }
      }

      int score = actorChainScore * 3 + vtableScore;
      bool isPerfectMatch = (checkCount > 0 && actorChainScore == checkCount);
      bool valid =
          state.AActorClass ? (actorChainScore >= 1) : (vtableScore >= 3);

      if (valid && score > bestScore) {
        bestScore = score;
        bestActOff = actOff;
        if (isPerfectMatch) {
          Log("[Discovery] ULevel::Actors candidate: off=0x%X count=%d (100%% perfect match)",
              actOff, arrCount);
        } else {
          Log("[Discovery] ULevel::Actors candidate: off=0x%X count=%d chain=%d "
              "vtable=%d score=%d",
              actOff, arrCount, actorChainScore, vtableScore, score);
        }
      }
    }

    if (bestActOff >= 0) {
      int32_t finalCount = *(int32_t *)(levelBuf + bestActOff + 8);
      state.ULevel_ActorsOff = bestActOff;
      if (bestScore > 0 && bestScore >= (std::min(finalCount, 10) * 4)) {
        Log("[Discovery] Found ULevel::Actors at offset 0x%X (count=%d) -> 100%% confidence",
            bestActOff, finalCount);
      } else {
        Log("[Discovery] Found ULevel::Actors at offset 0x%X (count=%d, "
            "validated score=%d)",
            bestActOff, finalCount, bestScore);
      }
      return true;
    }

    Log("[Discovery] Found PersistentLevel but couldn't find Actors TArray");
    break;
  }

  return false;
}

static void *GetActiveWorld(const EngineState &state) {

  if (state.GEngine && SafeMem_IsReadable(state.GEngine, 32)) {
    UObject* engine = state.GEngine;
    UClass* engineClass = EngineDiscovery::GetObjectClass(state, engine);
    if (engineClass) {
      int32_t vpOff = ConsoleEnabler::FindPropertyOffset(state, (UStruct*)engineClass, "GameViewport");
      if (vpOff >= 0) {
        UObject* viewport = (UObject*)SafeMem_ReadPtr((uint8_t*)engine + vpOff);
        if (viewport && SafeMem_IsReadable(viewport, 32)) {
          UClass* vpClass = EngineDiscovery::GetObjectClass(state, viewport);
          if (vpClass) {
            int32_t wOff = ConsoleEnabler::FindPropertyOffset(state, (UStruct*)vpClass, "World");
            if (wOff >= 0) {
              void* w = SafeMem_ReadPtr((uint8_t*)viewport + wOff);
              if (w && SafeMem_IsReadable(w, 0x28)) {
                const_cast<EngineState&>(state).GWorld = (UObject*)w;
                return w;
              }
            }
          }
        }
      }
    }
  }

  if (state.UWorldClass) {
    int count = GetObjectCount(state);
    for (int i = count - 1; i >= 0; --i) {
      UObject* obj = GetObjectByIndex(state, i);
      if (!obj || !SafeMem_IsReadable(obj, 0x28)) continue;
      void* cls = EngineDiscovery::GetObjectClass(state, obj);
      if (cls == state.UWorldClass) {
        static const int worldOffsets[] = { 0x30, 0x38, 0x40, 0x1B0, 0x1A8, 0x120 };
        for (int wOff : worldOffsets) {
          void* lvl = SafeMem_ReadPtr((uint8_t*)obj + wOff);
          if (lvl && SafeMem_IsReadable(lvl, 0x28)) {
            const_cast<EngineState&>(state).GWorld = obj;
            const_cast<EngineState&>(state).UWorld_PersistentLevelOff = wOff;
            return obj;
          }
        }
      }
    }
  }

  if (state.GWorldPtr) {
    void *currentWorld = nullptr;
    if (SafeMem_Read(state.GWorldPtr, &currentWorld, sizeof(void *)) && currentWorld && SafeMem_IsReadable(currentWorld, 0x28)) {
      const_cast<EngineState&>(state).GWorld = (UObject*)currentWorld;
      return currentWorld;
    }
  }

  if (state.GWorld && SafeMem_IsReadable(state.GWorld, 0x28)) {
    return state.GWorld;
  }

  return nullptr;
}

UObject *GetPersistentLevel(const EngineState &state) {
  void *currentWorld = GetActiveWorld(state);
  if (!currentWorld || state.UWorld_PersistentLevelOff < 0)
    return nullptr;
  return (UObject *)SafeMem_ReadPtr((uint8_t *)currentWorld +
                                    state.UWorld_PersistentLevelOff);
}

int32_t GetWorldActors(const EngineState &state, void **outActors,
                       int32_t maxCount) {
  if (!outActors || maxCount <= 0)
    return 0;

  FastMemScope fastMem;

  if (state.UWorld_PersistentLevelOff < 0) {
    UClass* worldClass = state.UWorldClass ? state.UWorldClass : ConsoleEnabler::FindClassByNameExact(state, "World");
    if (worldClass) {
      const_cast<EngineState&>(state).UWorld_PersistentLevelOff = ConsoleEnabler::FindPropertyOffset(state, (UStruct*)worldClass, "PersistentLevel");
    }
  }
  if (state.ULevel_ActorsOff < 0) {
    UClass* levelClass = ConsoleEnabler::FindClassByNameExact(state, "Level");
    if (levelClass) {
      const_cast<EngineState&>(state).ULevel_ActorsOff = ConsoleEnabler::FindPropertyOffset(state, (UStruct*)levelClass, "Actors");
    }
  }

  void *currentWorld = GetActiveWorld(state);
  if (!currentWorld)
    return 0;

  int32_t written = 0;
  std::unordered_set<void*> seenActors;
  seenActors.reserve(static_cast<size_t>(std::min(maxCount, 8192)));

  auto AddActor = [&](void* actor) {
    if (!actor) return;
    if ((uintptr_t)actor < 0x10000 || (uintptr_t)actor >= 0x7FFFFFFFFFFFULL) return;
    void* cls = SafeMem_ReadPtr((uint8_t*)actor + state.UObject_ClassOff);
    if (!cls || (uintptr_t)cls < 0x10000 || (uintptr_t)cls >= 0x7FFFFFFFFFFFULL) return;

    if (state.UObject_FlagsOff >= 0) {
      uint32_t flags = 0;
      SafeMem_Read((uint8_t*)actor + state.UObject_FlagsOff, &flags, sizeof(flags));
      if (flags & 0x00000200) return;
    }

    if (state.AActorClass && !EngineDiscovery::IsStructA(state, static_cast<UStruct*>(cls), reinterpret_cast<UStruct*>(state.AActorClass))) {
      return;
    }

    if (seenActors.insert(actor).second) {
      if (written < maxCount) {
        outActors[written++] = actor;
      }
    }
  };

  auto HarvestActorsFromLevel = [&](void* levelPtr) {
    if (!levelPtr || !SafeMem_IsReadable(levelPtr, 0x28)) return;
    int actOff = state.ULevel_ActorsOff;
    uint8_t tarrayBuf[16] = {};

    bool validTArray = false;
    if (actOff >= 0 && SafeMem_Read((uint8_t *)levelPtr + actOff, tarrayBuf, 16)) {
      void *data = *(void **)tarrayBuf;
      int32_t count = *(int32_t *)(tarrayBuf + 8);
      int32_t max = *(int32_t *)(tarrayBuf + 12);
      if (count >= 0 && count <= 500000 && max >= count && (count == 0 || (data && (uintptr_t)data >= 0x10000 && (uintptr_t)data < 0x7FFFFFFFFFFFULL))) {
        validTArray = true;
      }
    }

    if (!validTArray) {
      static const int levelOffsets[] = { 0x98, 0xA0, 0x70, 0xA8, 0xB0, 0x88, 0xC0 };
      for (int candOff : levelOffsets) {
        if (SafeMem_Read((uint8_t *)levelPtr + candOff, tarrayBuf, 16)) {
          void *data = *(void **)tarrayBuf;
          int32_t count = *(int32_t *)(tarrayBuf + 8);
          int32_t max = *(int32_t *)(tarrayBuf + 12);
          if (count > 0 && count <= 500000 && max >= count && data && (uintptr_t)data >= 0x10000 && (uintptr_t)data < 0x7FFFFFFFFFFFULL) {
            actOff = candOff;
            const_cast<EngineState&>(state).ULevel_ActorsOff = candOff;
            validTArray = true;
            break;
          }
        }
      }
    }

    if (!validTArray) return;

    void *data = *(void **)tarrayBuf;
    int32_t count = *(int32_t *)(tarrayBuf + 8);
    if (!data || count <= 0) return;

    int32_t readCount = std::min(count, maxCount * 2);
    constexpr int32_t chunkSize = 512;
    void* ptrBuf[chunkSize];

    for (int32_t i = 0; i < readCount && written < maxCount; i += chunkSize) {
      int32_t thisChunk = std::min(chunkSize, readCount - i);
      if (!SafeMem_Read((uint8_t *)data + i * sizeof(void*), ptrBuf, thisChunk * sizeof(void*)))
        break;

      for (int32_t j = 0; j < thisChunk && written < maxCount; j++) {
        AddActor(ptrBuf[j]);
      }
    }
  };

  void *persistentLevel = nullptr;
  if (state.UWorld_PersistentLevelOff >= 0) {
    persistentLevel = SafeMem_ReadPtr((uint8_t *)currentWorld + state.UWorld_PersistentLevelOff);
  }
  if (!persistentLevel || !SafeMem_IsReadable(persistentLevel, 0x28)) {
    static const int worldOffsets[] = { 0x30, 0x38, 0x40, 0x1B0, 0x1A8, 0x120 };
    for (int wOff : worldOffsets) {
      void* cand = SafeMem_ReadPtr((uint8_t *)currentWorld + wOff);
      if (cand && SafeMem_IsReadable(cand, 0x28)) {
        void* candCls = SafeMem_ReadPtr((uint8_t *)cand + state.UObject_ClassOff);
        if (candCls) {
          std::string cName = EngineDiscovery::GetObjectNameStr(state, (UObject*)candCls);
          if (cName == "Level" || cName.find("Level") != std::string::npos) {
            persistentLevel = cand;
            const_cast<EngineState&>(state).UWorld_PersistentLevelOff = wOff;
            break;
          }
        }
      }
    }
  }

  if (persistentLevel) {
    HarvestActorsFromLevel(persistentLevel);
  }

  UClass* worldClass = state.UWorldClass ? state.UWorldClass : ConsoleEnabler::FindClassByNameExact(state, "World");
  if (worldClass) {
    int32_t levelsOff = ConsoleEnabler::FindPropertyOffset(state, (UStruct*)worldClass, "Levels");
    if (levelsOff >= 0) {
      uint8_t levelsBuf[16] = {};
      if (SafeMem_Read((uint8_t *)currentWorld + levelsOff, levelsBuf, 16)) {
        void* levelsData = *(void**)levelsBuf;
        int32_t numLevels = *(int32_t*)(levelsBuf + 8);
        int32_t maxLevels = *(int32_t*)(levelsBuf + 12);
        if (levelsData && numLevels > 0 && numLevels <= 500 && maxLevels >= numLevels) {
          std::vector<void*> subLevels(static_cast<size_t>(numLevels));
          if (SafeMem_Read(levelsData, subLevels.data(), numLevels * sizeof(void*))) {
            for (void* subLvl : subLevels) {
              if (subLvl && subLvl != persistentLevel && written < maxCount) {
                HarvestActorsFromLevel(subLvl);
              }
            }
          }
        }
      }
    }
  }

  if (written == 0 && state.bIndexBuilt && state.IndexMutex && state.AActorClass) {
    std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);
    for (const auto& pair : state.ClassToInstances) {
      if (!pair.first) continue;

      if (pair.first == (void*)state.AActorClass || EngineDiscovery::IsStructA(state, static_cast<UStruct*>(pair.first), reinterpret_cast<UStruct*>(state.AActorClass))) {
        for (UObject* actorObj : pair.second) {
          if (!actorObj || written >= maxCount) break;
          AddActor(actorObj);
        }
      }
      if (written >= maxCount) break;
    }
  }

  return written;
}

}
