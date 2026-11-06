#include "SDKGenerator_Cycles.h"
#include <sstream>
#include <functional>

namespace SDKGenerator {

    std::set<std::string> DetectCyclicPackages(const std::map<std::string, std::set<std::string>>& packageDependencies) {
        std::set<std::string> cyclicPackages;
        std::map<std::string, int> visitState;
        std::vector<std::string> path;

        std::function<void(const std::string&)> dfs = [&](const std::string& node) {
            visitState[node] = 1;
            path.push_back(node);

            auto it = packageDependencies.find(node);
            if (it != packageDependencies.end()) {
                for (const auto& neighbor : it->second) {
                    if (visitState[neighbor] == 1) {

                        bool inCycle = false;
                        for (const auto& p : path) {
                            if (p == neighbor) inCycle = true;
                            if (inCycle) cyclicPackages.insert(p);
                        }
                    } else if (visitState[neighbor] == 0) {
                        dfs(neighbor);
                    }
                }
            }

            path.pop_back();
            visitState[node] = 2;
        };

        for (const auto& kv : packageDependencies) {
            if (visitState[kv.first] == 0) {
                dfs(kv.first);
            }
        }

        return cyclicPackages;
    }

    std::string GenerateForwardDeclarations(const std::set<std::string>& referencedClasses,
                                            const std::set<std::string>& referencedEnums) {
        std::ostringstream out;

        if (!referencedEnums.empty()) {
            out << "// Forward declarations for Enums\n";
            for (const auto& enm : referencedEnums) {
                if (enm.empty()) continue;
                out << "enum class " << enm << " : uint8_t;\n";
            }
            out << "\n";
        }

        if (!referencedClasses.empty()) {
            out << "// Forward declarations for Classes\n";
            for (const auto& cls : referencedClasses) {
                if (cls.empty()) continue;
                out << "class " << cls << ";\n";
            }
            out << "\n";
        }

        return out.str();
    }

}
