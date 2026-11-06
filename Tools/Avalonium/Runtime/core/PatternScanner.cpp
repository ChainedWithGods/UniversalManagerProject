#include "PatternScanner.h"
#include "../utils/SafeMemory.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <psapi.h>
#include <thread>
#include <vector>

namespace PatternScan {

static std::atomic<unsigned int> g_WorkersInUse{0};
static std::atomic<unsigned int> g_ActiveScans{0};

static unsigned int HardwareWorkers() {
  static const unsigned int cached = []() {
    unsigned int n = std::thread::hardware_concurrency();
    if (n == 0) n = 4;
    return n;
  }();
  return cached;
}

unsigned int AcquireWorkerBudget(unsigned int requested) {
  const unsigned int total = HardwareWorkers();
  if (requested == 0 || requested > total)
    requested = total;

  const unsigned int scans =
      g_ActiveScans.fetch_add(1, std::memory_order_relaxed) + 1;
  unsigned int share = total / (scans + 1);
  if (share < 1)
    share = 1;
  if (share > requested)
    share = requested;
  g_WorkersInUse.fetch_add(share, std::memory_order_relaxed);
  return share;
}

void ReleaseWorkerBudget(unsigned int granted) {
  if (granted > 0)
    g_WorkersInUse.fetch_sub(granted, std::memory_order_relaxed);
  g_ActiveScans.fetch_sub(1, std::memory_order_relaxed);
}

unsigned int RecommendedWorkerCount(unsigned int maxWorkers) {
  unsigned int n = HardwareWorkers();
  if (maxWorkers > 0 && n > maxWorkers)
    n = maxWorkers;
  return n == 0 ? 1 : n;
}

static bool MatchPattern(const uint8_t *data, const uint8_t *pattern,
                         const uint8_t *mask, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (mask[i] == 'x' && data[i] != pattern[i])
      return false;
  }
  return true;
}

static const uint8_t *FindU16(const uint8_t *data, size_t len, uint16_t value) {
  if (len < 2)
    return NULL;
  const uint8_t low = static_cast<uint8_t>(value & 0xFF);
  const uint8_t high = static_cast<uint8_t>(value >> 8);
  size_t offset = 0;
  while (offset + 1 < len) {
    const uint8_t *found = static_cast<const uint8_t *>(
        memchr(data + offset, low, len - offset - 1));
    if (!found)
      return NULL;
    if (found[1] == high)
      return found;
    offset = static_cast<size_t>(found - data) + 1;
  }
  return NULL;
}

static void ParsePattern(const char *patternStr, std::vector<uint8_t> &pattern,
                         std::vector<uint8_t> &mask) {
  pattern.clear();
  mask.clear();
  const char *p = patternStr;
  while (*p) {
    while (*p == ' ')
      p++;
    if (!*p)
      break;
    if (*p == '?') {
      pattern.push_back(0);
      mask.push_back('?');
      p++;
      if (*p == '?')
        p++;
    } else {
      char byte[3] = {p[0], p[1], 0};
      pattern.push_back((uint8_t)strtoul(byte, nullptr, 16));
      mask.push_back('x');
      p += 2;
    }
  }
}

uintptr_t FindPattern(uintptr_t start, size_t size, const char *patternStr) {
  std::vector<uint8_t> pattern, mask;
  ParsePattern(patternStr, pattern, mask);
  if (pattern.empty())
    return 0;

  uintptr_t current = start;
  uintptr_t end = start + size;

  while (current < end) {
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery((void *)current, &mbi, sizeof(mbi)) == 0)
      break;

    uintptr_t regionBase = (uintptr_t)mbi.BaseAddress;
    uintptr_t regionEnd = regionBase + mbi.RegionSize;

    bool isReadable =
        (mbi.State == MEM_COMMIT) && !(mbi.Protect & PAGE_GUARD) &&
        (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ |
                        PAGE_EXECUTE_READWRITE));

    if (isReadable) {

      uintptr_t scanStart = (current > regionBase) ? current : regionBase;
      uintptr_t scanEnd = (regionEnd < end) ? regionEnd : end;

      if (scanEnd >= scanStart + pattern.size()) {
        const uint8_t *data = (const uint8_t *)scanStart;
        size_t scanSize = scanEnd - scanStart;
        size_t maxScan = scanSize - pattern.size();

        int discPos = -1;
        for (size_t k = 0; k + 1 < pattern.size(); k++) {
          if (mask[k] == 'x' && mask[k + 1] == 'x') {
            discPos = (int)k;
            break;
          }
        }

        if (discPos >= 0) {
          uint16_t discriminator;
          memcpy(&discriminator, pattern.data() + discPos, 2);
          size_t i = 0;
          while (i <= maxScan) {
            const uint8_t *found =
                FindU16(data + i + discPos, maxScan - i + 2, discriminator);
            if (!found)
              break;
            const size_t matchIdx = (size_t)(found - data) - (size_t)discPos;
            if (MatchPattern(data + matchIdx, pattern.data(), mask.data(),
                             pattern.size())) {
              return scanStart + matchIdx;
            }
            i = matchIdx + 1;
          }
        } else {
          int firstFixed = -1;
          for (size_t k = 0; k < pattern.size(); k++) {
            if (mask[k] == 'x') {
              firstFixed = (int)k;
              break;
            }
          }
          if (firstFixed != -1) {
            uint8_t targetByte = pattern[firstFixed];
            size_t i = 0;
            while (i <= maxScan) {
              const uint8_t *found = (const uint8_t *)memchr(
                  data + i + firstFixed, targetByte, maxScan - i + 1);
              if (!found)
                break;
              size_t matchIdx = (found - data) - firstFixed;
              if (MatchPattern(data + matchIdx, pattern.data(), mask.data(),
                               pattern.size())) {
                return scanStart + matchIdx;
              }
              i = matchIdx + 1;
            }
          } else {
            for (size_t i = 0; i <= maxScan; i++) {
              if (MatchPattern(data + i, pattern.data(), mask.data(),
                               pattern.size())) {
                return scanStart + i;
              }
            }
          }
        }
      }
    }

    current = regionEnd;
  }

  return 0;
}

uintptr_t FindPatternInModule(HMODULE module, const char *patternStr) {
  if (!module || !patternStr)
    return 0;
  MODULEINFO mi = {};
  if (!GetModuleInformation(GetCurrentProcess(), module, &mi, sizeof(mi)))
    return 0;
  return FindPattern((uintptr_t)mi.lpBaseOfDll, mi.SizeOfImage, patternStr);
}

uintptr_t FindPatternInExecutableSections(HMODULE module,
                                           const char *patternStr) {
  if (!module || !patternStr)
    return 0;

  MODULEINFO mi = {};
  if (!GetModuleInformation(GetCurrentProcess(), module, &mi, sizeof(mi)))
    return 0;

  const uintptr_t base = reinterpret_cast<uintptr_t>(mi.lpBaseOfDll);
  const uintptr_t moduleEnd = base + mi.SizeOfImage;
  const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    return 0;

  const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(
      base + static_cast<DWORD>(dos->e_lfanew));
  if (nt->Signature != IMAGE_NT_SIGNATURE)
    return 0;

  struct ExecChunk {
    uintptr_t start;
    size_t size;
  };
  std::vector<ExecChunk> allChunks;

  const unsigned int numCores =
      AcquireWorkerBudget(RecommendedWorkerCount(16));

  const IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(nt);
  for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
    if (!(section->Characteristics & IMAGE_SCN_MEM_EXECUTE) ||
        !section->Misc.VirtualSize)
      continue;

    const uintptr_t sectionBase = base + section->VirtualAddress;
    if (sectionBase >= moduleEnd)
      continue;
    const size_t sectionSize = static_cast<size_t>(
        (std::min)(moduleEnd - sectionBase,
                   static_cast<uintptr_t>(section->Misc.VirtualSize)));

    if (sectionSize > 1024 * 1024) {
      size_t chunkLen = (sectionSize + numCores - 1) / numCores;
      for (size_t c = 0; c < numCores; ++c) {
        uintptr_t cStart = sectionBase + c * chunkLen;
        if (cStart >= sectionBase + sectionSize) break;
        size_t cSize = std::min(chunkLen + 256, sectionSize - (c * chunkLen));
        allChunks.push_back({cStart, cSize});
      }
    } else {
      allChunks.push_back({sectionBase, sectionSize});
    }
  }

  if (allChunks.empty()) {
    ReleaseWorkerBudget(numCores);
    return 0;
  }
  if (allChunks.size() == 1) {
    const uintptr_t only =
        FindPattern(allChunks[0].start, allChunks[0].size, patternStr);
    ReleaseWorkerBudget(numCores);
    return only;
  }

  std::atomic<uintptr_t> foundMatch{0};
  std::vector<std::thread> workers;
  workers.reserve(allChunks.size());

  for (size_t idx = 0; idx < allChunks.size(); ++idx) {
    workers.emplace_back([&allChunks, patternStr, &foundMatch, idx]() {
      FastMemScope fastMem;
      if (foundMatch.load(std::memory_order_relaxed) != 0) return;
      uintptr_t match = FindPattern(allChunks[idx].start, allChunks[idx].size, patternStr);
      if (match != 0) {
        uintptr_t expected = 0;
        foundMatch.compare_exchange_strong(expected, match, std::memory_order_relaxed);
      }
    });
  }

  for (auto &t : workers) {
    if (t.joinable()) t.join();
  }
  ReleaseWorkerBudget(numCores);

  return foundMatch.load(std::memory_order_relaxed);
}

void *FindExport(HMODULE module, const char *symbolName) {
  if (!module)
    return nullptr;
  return (void *)GetProcAddress(module, symbolName);
}

uintptr_t FindStringRef(HMODULE module, const char *str) {
  if (!module)
    return 0;
  MODULEINFO mi = {};
  if (!GetModuleInformation(GetCurrentProcess(), module, &mi, sizeof(mi)))
    return 0;

  uintptr_t base = (uintptr_t)mi.lpBaseOfDll;
  size_t moduleSize = mi.SizeOfImage;
  size_t strLen = strlen(str);

  std::vector<uint8_t> strPattern(str, str + strLen);
  std::vector<uint8_t> strMask(strLen, 'x');

  uintptr_t strAddr = 0;

  uintptr_t current = base;
  uintptr_t end = base + moduleSize;
  while (current < end) {
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery((void *)current, &mbi, sizeof(mbi)) == 0)
      break;
    uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;

    if (mbi.State == MEM_COMMIT && !(mbi.Protect & PAGE_GUARD) &&
        (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ |
                        PAGE_EXECUTE_READWRITE))) {
      uintptr_t scanStart = (current > (uintptr_t)mbi.BaseAddress)
                                ? current
                                : (uintptr_t)mbi.BaseAddress;
      uintptr_t scanEnd = (regionEnd < end) ? regionEnd : end;
      if (scanEnd >= scanStart + strLen) {
        const uint8_t *data = (const uint8_t *)scanStart;
        size_t maxScan = (scanEnd - scanStart) - strLen;

        uint8_t targetByte = strPattern[0];
        size_t i = 0;
        while (i <= maxScan) {
          const uint8_t *found = (const uint8_t *)memchr(
              data + i, targetByte, maxScan - i + 1);
          if (!found)
            break;

          size_t matchIdx = found - data;
          if (MatchPattern(data + matchIdx, strPattern.data(), strMask.data(),
                           strLen)) {
            strAddr = scanStart + matchIdx;
            break;
          }
          i = matchIdx + 1;
        }
        if (strAddr)
          break;
      }
    }
    current = regionEnd;
  }
  if (!strAddr)
    return 0;

  current = base;
  while (current < end) {
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery((void *)current, &mbi, sizeof(mbi)) == 0)
      break;
    uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;

    if (mbi.State == MEM_COMMIT && !(mbi.Protect & PAGE_GUARD) &&
        (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ |
                        PAGE_EXECUTE_READWRITE))) {
      uintptr_t scanStart = (current > (uintptr_t)mbi.BaseAddress)
                                ? current
                                : (uintptr_t)mbi.BaseAddress;
      uintptr_t scanEnd = (regionEnd < end) ? regionEnd : end;
      if (scanEnd >= scanStart + 7) {
        const uint8_t *data = (const uint8_t *)scanStart;
        size_t maxScan = (scanEnd - scanStart) - 7;

        size_t i = 0;
        while (i <= maxScan) {
          const uint8_t *found = (const uint8_t *)memchr(
              data + i, 0x48, maxScan - i + 1);
          if (!found)
            break;

          size_t matchIdx = found - data;
          if (data[matchIdx + 1] == 0x8D) {
            int32_t rel = *(int32_t *)(data + matchIdx + 3);
            uintptr_t target = scanStart + matchIdx + 7 + rel;
            if (target == strAddr) {
              return scanStart + matchIdx;
            }
          }
          i = matchIdx + 1;
        }
      }
    }
    current = regionEnd;
  }
  return 0;
}

uintptr_t FindWStringRef(HMODULE module, const wchar_t *str) {

  (void)module;
  (void)str;
  return 0;
}

uintptr_t ResolveRelative(uintptr_t addr, int32_t offset, int instSize) {
  return addr + instSize + offset;
}

std::vector<ModuleInfo> GetLoadedModules() {
  std::vector<ModuleInfo> result;
  HMODULE modules[1024];
  DWORD needed;
  if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules),
                          &needed))
    return result;

  char winDir[MAX_PATH] = {};
  GetWindowsDirectoryA(winDir, MAX_PATH);
  std::string winDirStr = winDir;
  std::transform(winDirStr.begin(), winDirStr.end(), winDirStr.begin(),
                 ::tolower);

  int count = needed / sizeof(HMODULE);
  for (int i = 0; i < count; i++) {
    char name[MAX_PATH] = {};
    if (!GetModuleFileNameA(modules[i], name, MAX_PATH))
      continue;

    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   ::tolower);

    if (lowerName.find(winDirStr) == 0)
      continue;

    ModuleInfo mi;
    mi.handle = modules[i];
    mi.name = name;

    MODULEINFO info = {};
    if (GetModuleInformation(GetCurrentProcess(), modules[i], &info,
                             sizeof(info))) {
      mi.base = (uintptr_t)info.lpBaseOfDll;
      mi.size = info.SizeOfImage;
    }

    result.push_back(mi);
  }
  return result;
}

ModuleInfo GetModuleByName(const char *name) {
  auto mods = GetLoadedModules();
  std::string target = name;
  std::transform(target.begin(), target.end(), target.begin(), ::tolower);
  for (auto &m : mods) {
    std::string modName = m.name;
    std::transform(modName.begin(), modName.end(), modName.begin(), ::tolower);
    if (modName.find(target) != std::string::npos)
      return m;
  }
  return {};
}

ModuleInfo GetMainModule() {
  ModuleInfo mi;
  mi.handle = GetModuleHandleA(nullptr);
  MODULEINFO info = {};
  if (GetModuleInformation(GetCurrentProcess(), mi.handle, &info,
                           sizeof(info))) {
    mi.base = (uintptr_t)info.lpBaseOfDll;
    mi.size = info.SizeOfImage;
  }
  char name[MAX_PATH] = {};
  GetModuleFileNameA(mi.handle, name, MAX_PATH);
  mi.name = name;
  return mi;
}

std::string ExtractGameName(const std::string &moduleName) {
  size_t lastSlash = moduleName.find_last_of("\\\\/");
  std::string filename = (lastSlash != std::string::npos)
                             ? moduleName.substr(lastSlash + 1)
                             : moduleName;

  size_t dotPos = filename.find(".exe");
  if (dotPos != std::string::npos) {
    filename = filename.substr(0, dotPos);
  }

  size_t dashPos = filename.find("-Win64-Shipping");
  if (dashPos != std::string::npos) {
    return filename.substr(0, dashPos);
  }

  dashPos = filename.find("-Win64");
  if (dashPos != std::string::npos) {
    return filename.substr(0, dashPos);
  }

  dashPos = filename.find("-Shipping");
  if (dashPos != std::string::npos) {
    return filename.substr(0, dashPos);
  }

  return filename;
}

}
