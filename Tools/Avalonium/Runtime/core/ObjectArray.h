#pragma once
#include "Types.h"
#include <vector>
#include <string>

namespace EngineDiscovery {

bool DiscoverTUObjectArrayLayout(EngineState &state);

int32_t GetElementsPerChunk(const EngineState &state);

int32_t GetObjectCount(const EngineState &state);

UObject *GetObjectByIndex(const EngineState &state, int32_t index);

typedef bool (*ObjectIteratorCallback)(UObject *object, int32_t index, void *userdata);

int32_t ForEachObject(const EngineState &state, ObjectIteratorCallback callback, void *userdata);

}
