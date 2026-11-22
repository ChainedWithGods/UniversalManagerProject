#pragma once

#include "../core/Types.h"

#include <cstddef>
#include <string>

namespace Avalonium::CppMods
{

    UObject* ImportImageFile(EngineState& state, const std::string& path);
    UObject* ImportImageMemory(EngineState& state, const void* bytes, size_t size);

    bool IsModAsset(EngineState& state, UObject* object);

    bool IsModPackagePath(EngineState& state, const char* assetPath);

    int32_t ModPackageCount(EngineState& state);

    void PumpAssetLoads();
}
