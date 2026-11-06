#pragma once
#include <string>
#include "../core/Types.h"

namespace SDKGenerator {

    bool IsCorePredefinedClass(const std::string& className);

    std::string GetCorePredefinedCanonicalName(const std::string& className);

    std::string GetPredefinedClassDeclaration(const EngineState& state, const std::string& className,
                                             int32_t baseSize, int32_t classSize);

    std::string GetPredefinedClassImplementation(const EngineState& state, const std::string& className);

}
