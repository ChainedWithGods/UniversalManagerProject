#include "GNativesResolver.h"

#include "Logger.h"
#include "PatternScanner.h"
#include "../utils/SafeMemory.h"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace EngineDiscovery {
namespace {

struct AddressRange {
  uintptr_t Begin;
  uintptr_t End;
};

struct DataRegion {
  uintptr_t Base;
  size_t Size;
};

bool IsExecutableProtect(DWORD protect) {
  if (protect & PAGE_GUARD) return false;
  const DWORD base = protect & 0xFF;
  return base == PAGE_EXECUTE || base == PAGE_EXECUTE_READ ||
         base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
}

bool IsReadableDataProtect(DWORD protect) {
  if (protect & PAGE_GUARD) return false;
  const DWORD base = protect & 0xFF;
  return base == PAGE_READONLY || base == PAGE_READWRITE ||
         base == PAGE_WRITECOPY || base == PAGE_EXECUTE_READ ||
         base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
}

bool IsWritableProtect(DWORD protect) {
  const DWORD base = protect & 0xFF;
  return base == PAGE_READWRITE || base == PAGE_WRITECOPY ||
         base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
}

void CollectExecutableRanges(const std::vector<PatternScan::ModuleInfo> &modules,
                             std::vector<AddressRange> &ranges) {
  for (const PatternScan::ModuleInfo &mod : modules) {
    if (!mod.base || mod.size == 0) continue;
    const uintptr_t end = mod.base + mod.size;
    for (uintptr_t current = mod.base; current < end;) {
      MEMORY_BASIC_INFORMATION info = {};
      if (VirtualQuery(reinterpret_cast<LPCVOID>(current), &info, sizeof(info)) == 0)
        break;
      const uintptr_t regionBase = reinterpret_cast<uintptr_t>(info.BaseAddress);
      const uintptr_t regionEnd = regionBase + info.RegionSize;
      if (regionEnd <= current) break;
      if (info.State == MEM_COMMIT && IsExecutableProtect(info.Protect)) {
        const uintptr_t begin = (std::max)(current, regionBase);
        const uintptr_t stop = (std::min)(regionEnd, end);
        if (stop > begin) ranges.push_back({begin, stop});
      }
      current = regionEnd;
    }
  }

  std::sort(ranges.begin(), ranges.end(),
            [](const AddressRange &lhs, const AddressRange &rhs) {
              return lhs.Begin < rhs.Begin;
            });
  std::vector<AddressRange> merged;
  merged.reserve(ranges.size());
  for (const AddressRange &range : ranges) {
    if (!merged.empty() && range.Begin <= merged.back().End) {
      merged.back().End = (std::max)(merged.back().End, range.End);
    } else {
      merged.push_back(range);
    }
  }
  ranges.swap(merged);
}

bool IsExecutablePointer(uintptr_t value,
                         const std::vector<AddressRange> &ranges) {
  if (value < 0x10000) return false;
  const auto upper = std::upper_bound(
      ranges.begin(), ranges.end(), value,
      [](uintptr_t candidate, const AddressRange &range) {
        return candidate < range.Begin;
      });
  if (upper == ranges.begin()) return false;
  const AddressRange &range = *(upper - 1);
  return value >= range.Begin && value < range.End;
}

std::vector<size_t> NativeTableEntryCounts(const EngineState &state) {
  if (state.UEMajor == 5 && state.UEMinor >= 1) return {0xFF};
  if (state.UEMajor == 4 || (state.UEMajor == 5 && state.UEMinor == 0))
    return {0x100};
  return {0xFF, 0x100};
}

bool LooksLikeNativeTable(const uintptr_t *entries, size_t count,
                          const std::vector<AddressRange> &execRanges,
                          uint32_t &longestRun, uint32_t &uniqueCount) {
  if (!entries || count == 0 || count > 256) return false;

  uintptr_t seen[256] = {};
  size_t seenCount = 0;
  uint32_t run = 1;
  longestRun = 1;

  for (size_t i = 0; i < count; ++i) {
    const uintptr_t value = entries[i];
    if (!IsExecutablePointer(value, execRanges)) return false;

    if (i && value == entries[i - 1]) {
      ++run;
      if (run > longestRun) longestRun = run;
    } else {
      run = 1;
    }

    bool alreadySeen = false;
    for (size_t j = 0; j < seenCount; ++j) {
      if (seen[j] == value) {
        alreadySeen = true;
        break;
      }
    }
    if (!alreadySeen && seenCount < 256) seen[seenCount++] = value;
  }

  uniqueCount = static_cast<uint32_t>(seenCount);
  return uniqueCount >= 16 && longestRun >= 8;
}

bool ReadNativeTable(const void *address, size_t count,
                     std::vector<uintptr_t> &table) {
  if (!address || count == 0) return false;
  table.resize(count);
  const size_t bytes = count * sizeof(uintptr_t);
  return SafeMem_ReadSpanExact(address, table.data(), bytes) == bytes;
}

void CollectDataRegions(const PatternScan::ModuleInfo &mod, bool writablePass,
                        std::vector<DataRegion> &regions) {
  if (!mod.base || mod.size == 0) return;
  const uintptr_t end = mod.base + mod.size;
  for (uintptr_t current = mod.base; current < end;) {
    MEMORY_BASIC_INFORMATION info = {};
    if (VirtualQuery(reinterpret_cast<LPCVOID>(current), &info, sizeof(info)) == 0)
      break;
    const uintptr_t regionBase = reinterpret_cast<uintptr_t>(info.BaseAddress);
    const uintptr_t regionEnd = regionBase + info.RegionSize;
    if (regionEnd <= current) break;

    if (info.State == MEM_COMMIT && IsReadableDataProtect(info.Protect) &&
        !IsExecutableProtect(info.Protect) &&
        (IsWritableProtect(info.Protect) == writablePass)) {
      const uintptr_t begin = (std::max)(current, regionBase);
      const uintptr_t stop = (std::min)(regionEnd, end);
      if (stop > begin) regions.push_back({begin, static_cast<size_t>(stop - begin)});
    }
    current = regionEnd;
  }
}

bool ScanModuleForNativeTable(
    const PatternScan::ModuleInfo &mod,
    const std::vector<size_t> &entryCounts,
    const std::vector<AddressRange> &execRanges, uintptr_t &found,
    size_t &foundEntries, uint32_t &foundRun, uint32_t &foundUnique,
    uint64_t &scannedBytes) {
  std::vector<DataRegion> regions;
  std::vector<uintptr_t> words;

  for (int pass = 0; pass < 2; ++pass) {
    regions.clear();
    CollectDataRegions(mod, pass == 0, regions);

    for (const DataRegion &region : regions) {
      words.resize((region.Size + sizeof(uintptr_t) - 1) / sizeof(uintptr_t));
      if (SafeMem_ReadSpanExact(reinterpret_cast<const void *>(region.Base),
                                words.data(), region.Size) != region.Size)
        continue;

      scannedBytes += region.Size;
      const uintptr_t *entries = words.data();
      const size_t wordCount = region.Size / sizeof(uintptr_t);

      for (const size_t count : entryCounts) {
        if (wordCount < count) continue;
        for (size_t i = 0; i + count <= wordCount; ++i) {
          if (!IsExecutablePointer(entries[i], execRanges)) continue;

          uint32_t run = 0;
          uint32_t unique = 0;
          if (!LooksLikeNativeTable(entries + i, count, execRanges, run,
                                    unique))
            continue;

          found = region.Base + i * sizeof(uintptr_t);
          foundEntries = count;
          foundRun = run;
          foundUnique = unique;
          return true;
        }
      }
    }
  }
  return false;
}

std::vector<PatternScan::ModuleInfo> OrderModules(
    const std::vector<PatternScan::ModuleInfo> &modules,
    const PatternScan::ModuleInfo &mainModule) {
  std::vector<PatternScan::ModuleInfo> ordered;
  ordered.reserve(modules.size());

  auto nameHasCoreUObject = [](const PatternScan::ModuleInfo &mod) {
    std::string name = mod.name;
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return name.find("coreuobject") != std::string::npos;
  };

  for (const auto &mod : modules) {
    if (nameHasCoreUObject(mod)) ordered.push_back(mod);
  }
  if (mainModule.handle) {
    const bool present = std::any_of(
        ordered.begin(), ordered.end(),
        [&](const PatternScan::ModuleInfo &mod) { return mod.base == mainModule.base; });
    if (!present) ordered.push_back(mainModule);
  }
  for (const auto &mod : modules) {
    const bool present = std::any_of(
        ordered.begin(), ordered.end(),
        [&](const PatternScan::ModuleInfo &candidate) {
          return candidate.base == mod.base;
        });
    if (!present) ordered.push_back(mod);
  }
  return ordered;
}

bool TryExports(const std::vector<PatternScan::ModuleInfo> &modules,
                const std::vector<size_t> &entryCounts,
                const std::vector<AddressRange> &execRanges,
                uintptr_t &found, size_t &foundEntries, uint32_t &foundRun,
                uint32_t &foundUnique) {
  static const char *exportNames[] = {
      "?GNatives@@3PAP6AXPEAVUObject@@AEAUFFrame@@QEAX@ZA",
      "GNatives",
      "?GNatives@@3P6AXPEAVUObject@@AEAUFFrame@@QEAX@ZA"};

  std::vector<uintptr_t> table;
  for (const auto &mod : modules) {
    if (!mod.handle) continue;
    for (const char *name : exportNames) {
      FARPROC proc = GetProcAddress(mod.handle, name);
      if (!proc) continue;
      for (const size_t count : entryCounts) {
        if (!ReadNativeTable(reinterpret_cast<const void *>(proc), count, table))
          continue;
        uint32_t run = 0;
        uint32_t unique = 0;
        if (!LooksLikeNativeTable(table.data(), count, execRanges, run, unique))
          continue;
        found = reinterpret_cast<uintptr_t>(proc);
        foundEntries = count;
        foundRun = run;
        foundUnique = unique;
        return true;
      }
    }
  }
  return false;
}

}

bool ResolveGNatives(EngineState &state) {
  const uint64_t started = GetTickCount64();
  const auto modules = PatternScan::GetLoadedModules();
  if (modules.empty()) {
    Log("[Discovery] GNatives unresolved: no loaded modules to inspect");
    return false;
  }

  const PatternScan::ModuleInfo mainModule = PatternScan::GetMainModule();
  const auto ordered = OrderModules(modules, mainModule);
  const std::vector<size_t> entryCounts = NativeTableEntryCounts(state);

  std::vector<AddressRange> execRanges;
  CollectExecutableRanges(modules, execRanges);

  uintptr_t found = 0;
  size_t foundEntries = 0;
  uint32_t foundRun = 0;
  uint32_t foundUnique = 0;

  if (TryExports(ordered, entryCounts, execRanges, found, foundEntries,
                 foundRun, foundUnique)) {
    state.GNatives = reinterpret_cast<void *>(found);
    Log("[Discovery] Found GNatives via export: %p (%llu entries, %u unique, "
        "run=%u)",
        state.GNatives, static_cast<unsigned long long>(foundEntries),
        foundUnique, foundRun);
    return true;
  }

  uint64_t scannedBytes = 0;
  for (const auto &mod : ordered) {
    if (ScanModuleForNativeTable(mod, entryCounts, execRanges, found,
                                 foundEntries, foundRun, foundUnique,
                                 scannedBytes)) {
      state.GNatives = reinterpret_cast<void *>(found);
      Log("[Discovery] Found GNatives via native-function table: %p "
          "(%llu entries, %u unique, run=%u, %.1f MB scanned in %llu ms)",
          state.GNatives, static_cast<unsigned long long>(foundEntries),
          foundUnique, foundRun,
          static_cast<double>(scannedBytes) / (1024.0 * 1024.0),
          static_cast<unsigned long long>(GetTickCount64() - started));
      return true;
    }
  }

  Log("[Discovery] GNatives unresolved: no executable-function table of %s "
      "entries found in loaded module data (%.1f MB scanned in %llu ms)",
      entryCounts.size() == 1 ? (entryCounts[0] == 0xFF ? "0xFF" : "0x100")
                              : "0x100/0xFF",
      static_cast<double>(scannedBytes) / (1024.0 * 1024.0),
      static_cast<unsigned long long>(GetTickCount64() - started));
  return false;
}

}
