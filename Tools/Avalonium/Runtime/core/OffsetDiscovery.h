#pragma once
#include "EngineDiscovery.h"

namespace EngineDiscovery {

bool DiscoverUObjectFlagsOffset(EngineState &state);

bool DiscoverUObjectLayout(EngineState &state);

bool DiscoverUStructLayout(EngineState &state);

bool DiscoverOffsets(EngineState &state);

}
