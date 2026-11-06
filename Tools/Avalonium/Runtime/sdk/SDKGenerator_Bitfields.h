#pragma once
#include <string>
#include <vector>
#include <sstream>
#include "../core/Types.h"
#include "SDKGenerator_Impl.h"

namespace SDKGenerator {

    void ExtractBoolPropertyBitfields(const EngineState& state, void* propField, SDKProperty& prop);

    void SortPropertiesForLayout(std::vector<SDKProperty>& props);

    void WriteMembersAccurate(std::ostringstream& out, const std::vector<SDKProperty>& props,
                             int superSize, int structSize, const std::string& scopeName = "",
                             std::vector<int32_t>* emittedOffsets = nullptr);

}
