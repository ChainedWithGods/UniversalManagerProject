#pragma once
#include "EngineDiscovery.h"

namespace EngineDiscovery {

bool DetectVersion(EngineState &state);

bool FindGUObjectArray(EngineState &state);

bool FindGNames(EngineState &state);

bool FindGWorld(EngineState &state);

bool FindGMalloc(EngineState &state);

bool IsValidMallocInstance(void *instance);

bool GetAllocatorDescriptiveName(void *instance, char (&outName)[64],
                                 int &outSlot);

void *ProbeAllocatorMalloc(void *instance, int32_t slot, size_t bytes);

bool FindGNatives(EngineState &state);

bool FindGUObjectHashTables(EngineState &state);

bool IsValidFNamePool(EngineState &state, void *candidate);

bool FindGlobals(EngineState &state);

}
