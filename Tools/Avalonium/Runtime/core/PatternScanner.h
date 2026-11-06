#pragma once

#include <string>
#include <vector>
#include <windows.h>

namespace PatternScan {

uintptr_t FindPattern(uintptr_t start, size_t size, const char *pattern);

uintptr_t FindPatternInModule(HMODULE module, const char *pattern);

uintptr_t FindPatternInExecutableSections(HMODULE module, const char *pattern);

void *FindExport(HMODULE module, const char *symbolName);

uintptr_t FindStringRef(HMODULE module, const char *str);
uintptr_t FindWStringRef(HMODULE module, const wchar_t *str);

uintptr_t ResolveRelative(uintptr_t instructionAddr, int32_t offset,
                          int instructionSize);

struct ModuleInfo {
  HMODULE handle;
  uintptr_t base;
  size_t size;
  std::string name;
};

std::vector<ModuleInfo> GetLoadedModules();
ModuleInfo GetModuleByName(const char *name);
ModuleInfo GetMainModule();

unsigned int AcquireWorkerBudget(unsigned int requested);
void ReleaseWorkerBudget(unsigned int granted);

unsigned int RecommendedWorkerCount(unsigned int maxWorkers = 0);

std::string ExtractGameName(const std::string &moduleName);

}
