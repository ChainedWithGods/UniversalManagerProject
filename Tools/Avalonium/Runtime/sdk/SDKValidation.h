#pragma once
#include "../core/Types.h"
#include "../core/Logger.h"
#include <string>

namespace SDKValidation {

    void StartSDKValidationThread(const EngineState& state, const std::string& sdkPath);

    void StartSDKGenerationThread(const std::string& sdkPath,
                                  const std::string& gameName,
                                  const std::string& libclangPath);
}
