#pragma once
#include <string>
#include <vector>
#include <set>
#include <map>

namespace SDKGenerator {

    std::set<std::string> DetectCyclicPackages(const std::map<std::string, std::set<std::string>>& packageDependencies);

    std::string GenerateForwardDeclarations(const std::set<std::string>& referencedClasses,
                                            const std::set<std::string>& referencedEnums);

}
