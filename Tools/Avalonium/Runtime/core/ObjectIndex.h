#pragma once
#include "EngineDiscovery.h"
#include <cstdint>
#include <string>
#include <vector>

namespace EngineDiscovery {

void BuildObjectIndex(EngineState &state);

UObject *FindObjectByNameFast(EngineState &state, const std::string &name);

std::vector<UObject *> FindObjectsByNameFast(EngineState &state,
                                             const std::string &name);

std::vector<UObject *> FindObjectsByClassFast(EngineState &state,
                                              UClass *cls);

std::vector<UObject *> FindLiveObjectsByClass(EngineState &state, UClass *cls);

void AdvanceLiveLookupGeneration(EngineState &state);

std::vector<UObject *> FindObjectsByPrefix(EngineState &state,
                                           const std::string &prefix);

UObject *FindObjectByPath(EngineState &state, const std::string &query);

std::string StripTypeTokenWrapper(const std::string &query);

UObject *ResolveObjectPath(EngineState &state, const std::string &query,
                           UClass *requiredClass = nullptr,
                           bool exactClass = false,
                           UObject *withinOuter = nullptr);

std::string GetCachedFName(EngineState &state, int32_t nameIdx);

int32_t ResolveFNameIndex(EngineState &state, const std::string &name);

void RebuildFunctionCache(EngineState &state);

int32_t DeltaUpdateFunctionCache(EngineState &state);

UFunction *FindFunctionCached(EngineState &state, const std::string &lookup);

UClass *FindClassCached(EngineState &state, const std::string &className);

void InvalidateObjectFromCache(EngineState &state, UObject *obj);

}
