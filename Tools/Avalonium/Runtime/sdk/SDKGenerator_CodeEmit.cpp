#include "SDKGenerator_Impl.h"
#include <windows.h>
namespace SDKGenerator {

    void CollectStructRefsFromType(const std::string& cppType, std::set<std::string>& out) {
        const std::string target = "struct ";
        size_t pos = 0;
        while ((pos = cppType.find(target, pos)) != std::string::npos) {

            if (pos > 0 && (isalnum(static_cast<unsigned char>(cppType[pos - 1])) || cppType[pos - 1] == '_')) {
                pos += target.size();
                continue;
            }
            size_t start = pos + target.size();
            size_t nameEnd = start;
            while (nameEnd < cppType.size() &&
                   (isalnum(static_cast<unsigned char>(cppType[nameEnd])) || cppType[nameEnd] == '_')) {
                nameEnd++;
            }
            if (nameEnd > start) {
                out.insert(cppType.substr(start, nameEnd - start));
            }
            pos = nameEnd;
        }
    }

    std::vector<int> SortTypeDefOrder(const std::vector<PendingTypeDef>& defs) {
        const int n = static_cast<int>(defs.size());
        if (n <= 1) {
            if (n == 1) return {0};
            return {};
        }
        std::unordered_map<std::string, int> indexOf;
        indexOf.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) indexOf.emplace(defs[i].cppName, i);

        std::vector<std::vector<int>> dependents(static_cast<size_t>(n));
        std::vector<int> indegree(static_cast<size_t>(n), 0);
        for (int i = 0; i < n; ++i) {
            for (const std::string& dep : defs[static_cast<size_t>(i)].deps) {
                auto it = indexOf.find(dep);
                if (it == indexOf.end() || it->second == i) continue;
                dependents[static_cast<size_t>(it->second)].push_back(i);
                indegree[static_cast<size_t>(i)]++;
            }
        }

        std::vector<int> ready;
        for (int i = 0; i < n; ++i) if (indegree[static_cast<size_t>(i)] == 0) ready.push_back(i);

        std::vector<int> out;
        out.reserve(static_cast<size_t>(n));
        std::vector<char> emitted(static_cast<size_t>(n), 0);
        while (!ready.empty()) {
            auto it = std::min_element(ready.begin(), ready.end());
            const int cur = *it;
            ready.erase(it);
            if (emitted[static_cast<size_t>(cur)]) continue;
            emitted[static_cast<size_t>(cur)] = 1;
            out.push_back(cur);
            for (int next : dependents[static_cast<size_t>(cur)]) {
                if (--indegree[static_cast<size_t>(next)] == 0) ready.push_back(next);
            }
        }
        for (int i = 0; i < n; ++i) if (!emitted[static_cast<size_t>(i)]) out.push_back(i);
        return out;
    }

    std::vector<PendingTypeDef> SortTypeDefs(const std::vector<PendingTypeDef>& defs) {
        std::vector<int> order = SortTypeDefOrder(defs);
        std::vector<PendingTypeDef> out;
        out.reserve(order.size());
        for (int idx : order) out.push_back(defs[idx]);
        return out;
    }

}
