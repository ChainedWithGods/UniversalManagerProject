#pragma once

#include "Types.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "PatternScanner.h"
#include "Logger.h"
#include "ObjectArray.h"
#include "NameResolution.h"
#include "ObjectUtils.h"
#include "WorldDiscovery.h"
#include "EngineDetection.h"
#include "OffsetDiscovery.h"
#include "FunctionFinder.h"
#include "ObjectIndex.h"

namespace EngineDiscovery {

std::string GetMainWindowTitle();
bool FindGameWindow(EngineState& state);

bool Initialize(EngineState &state, bool skipConsoleManager = false);

bool DetectVersion(EngineState &state);

bool FindGlobals(EngineState &state);
bool FindGUObjectArray(EngineState &state);
bool FindGNames(EngineState &state);
bool FindGWorld(EngineState &state);
bool FindGMalloc(EngineState &state);
bool FindGNatives(EngineState &state);
bool FindGUObjectHashTables(EngineState &state);

bool DiscoverOffsets(EngineState &state);
bool DiscoverUObjectLayout(EngineState &state);
bool DiscoverUStructLayout(EngineState &state);

bool FindEngineFunctions(EngineState &state);
bool FindGetFullName(EngineState &state);
bool FindStaticConstructObject(EngineState &state);
bool FindConsoleManager(EngineState &state);

bool FindCoreClasses(EngineState &state);

bool FindProcessEvent(EngineState &state);
bool FindProcessInternal(EngineState &state);
bool FindProcessLocalScriptFunction(EngineState &state);

void BuildObjectIndex(EngineState &state);
UObject *FindObjectByNameFast(EngineState &state, const std::string &name);
std::vector<UObject *> FindObjectsByNameFast(EngineState &state, const std::string &name);
std::vector<UObject *> FindObjectsByClassFast(EngineState &state, UClass *cls);
std::vector<UObject *> FindObjectsByPrefix(EngineState &state, const std::string &prefix);

std::string GetCachedFName(EngineState &state, int32_t nameIdx);
int32_t ResolveFNameIndex(EngineState &state, const std::string &name);

void *FindFunctionByName(const EngineState &state, UStruct *cls, const char *funcName);
bool CallProcessEvent(const EngineState &state, void *object, void *ufunction, void *params);

bool GenerateUSMap(const EngineState &state, const std::string &outputPath);

}
