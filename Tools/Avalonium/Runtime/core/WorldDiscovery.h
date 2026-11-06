#pragma once
#include "Types.h"

namespace EngineDiscovery {

bool DiscoverWorldLayout(EngineState &state);

UObject *GetLiveWorld(EngineState &state);

UObject *GetPersistentLevel(const EngineState &state);

int32_t GetWorldActors(const EngineState &state, void **outActors, int32_t maxCount);

}
