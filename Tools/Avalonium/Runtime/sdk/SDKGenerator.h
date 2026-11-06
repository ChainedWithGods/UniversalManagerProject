#pragma once
#include <string>
#include <vector>
#include "../core/Types.h"

namespace SDKGenerator {

    bool GenerateSDK(const EngineState& state, const std::string& outputDir);

    bool GenerateReflectionKit(const EngineState& state, const std::string& outputDir);

    bool GenerateReflectionDiff(const EngineState& state, const std::string& outputDir,
                                const std::string& previousSnapshotPath, const std::string& currentSnapshotPath);

    void SdkLog(const char* fmt, ...);

}
