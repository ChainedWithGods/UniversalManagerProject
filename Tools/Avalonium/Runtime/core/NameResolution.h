#pragma once
#include "Types.h"
#include <string>

namespace EngineDiscovery {

void BuildFNameCache(EngineState &state);

void RefreshFNamePoolDelta(EngineState &state);

std::string DecodeFNameEntryString(const EngineState &state, uint8_t *data, size_t offset, uint16_t meta);

std::string FNameToStr(const EngineState &state, const FName &name);

std::string GetObjectNameStr(const EngineState &state, UObject *obj);

std::string BuildPathName(const EngineState &state, UObject *obj);

std::string BuildFullName(const EngineState &state, UObject *obj);

bool DiscoverFNameLayout(EngineState &state);

}
