#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace IPCQuery {

struct MemRegion {
    uintptr_t base = 0;
    size_t size = 0;
    uint32_t protect = 0;
    uint32_t state = 0;
    uint32_t type = 0;
    std::string module;
};

std::vector<MemRegion> EnumRegions(bool includeImages, bool includeMapped,
                                   bool includePrivate, bool readableOnly);

struct ModuleEntry {
    std::string name;
    uintptr_t base = 0;
    size_t size = 0;
    std::string path;
    bool isMain = false;
};
std::vector<ModuleEntry> EnumModules();

std::string HandleQuery(const std::string& args);

std::string SearchPattern(uintptr_t base, size_t size, const std::string& pattern,
                          const std::string& scope, int limit);

std::string SearchString(uintptr_t base, size_t size, const std::string& needle,
                         const std::string& scope, int limit, bool utf16);

std::string SearchPointers(uintptr_t base, size_t size, uintptr_t target,
                           const std::string& scope, int limit);

std::string ClassInfo(const std::string& target);

std::string VTable(uintptr_t ptr, int limit);

std::string Modules();

std::string Stats();

}
