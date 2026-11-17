#pragma once

#include "../core/Types.h"

namespace Avalonium::CppMods
{

    bool IsModAsset(EngineState& state, UObject* object);

    void PumpAssetLoads();
}
