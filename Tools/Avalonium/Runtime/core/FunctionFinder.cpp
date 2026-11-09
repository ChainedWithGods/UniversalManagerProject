#include "FunctionFinder.h"
#include "Logger.h"
#include "PatternScanner.h"
#include "ObjectArray.h"
#include "NameResolution.h"
#include "ObjectUtils.h"
#include "OffsetDiscovery.h"
#include "BlueprintDiscovery.h"
#include "ObjectIndex.h"
#include "../utils/SafeMemory.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <psapi.h>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace EngineDiscovery {

bool FindGetFullName(EngineState &state) {

  return false;
}

static bool IsAddressInModule(const PatternScan::ModuleInfo &mod,
                              uintptr_t addr) {
  return addr >= mod.base && addr < mod.base + mod.size;
}

static const PatternScan::ModuleInfo *
FindOwningModuleForAddress(const std::vector<PatternScan::ModuleInfo> &modules,
                          uintptr_t address) {
  if (!address)
    return nullptr;

  for (const auto &mod : modules) {
    if (IsAddressInModule(mod, address))
      return &mod;
  }
  return nullptr;
}

struct SectionRange { uintptr_t start, end; };

bool FindGEngine(EngineState &state) {
  if (state.GEngine) {
    std::string existingName = GetObjectNameStr(state, state.GEngine);
    if (!existingName.empty() && existingName.find("Default__") == std::string::npos) {
      return true;
    }
    state.GEngine = nullptr;
  }

  if (!state.bIndexBuilt || state.ClassToInstances.empty()) {
    return false;
  }

  UClass *gameEngineClass = nullptr;
  UClass *engineClass = nullptr;
  const int32_t gameEngineIdx = ResolveFNameIndex(state, "GameEngine");
  const int32_t engineIdx = ResolveFNameIndex(state, "Engine");

  for (const auto &pair : state.ClassToInstances) {
    if (!pair.first) continue;
    FName fn = {};
    if (SafeMem_Read((uint8_t *)pair.first + state.UObject_NameOff, &fn, sizeof(fn))) {
      if (gameEngineIdx >= 0 && fn.ComparisonIndex == static_cast<uint32_t>(gameEngineIdx)) {
        gameEngineClass = pair.first;
        if (engineClass || engineIdx < 0) break;
      } else if (engineIdx >= 0 && fn.ComparisonIndex == static_cast<uint32_t>(engineIdx)) {
        engineClass = pair.first;
        if (gameEngineClass || gameEngineIdx < 0) break;
      }
    } else {
      std::string name = GetObjectNameStr(state, pair.first);
      if (name == "GameEngine") {
        gameEngineClass = pair.first;
        if (engineClass) break;
      } else if (name == "Engine") {
        engineClass = pair.first;
        if (gameEngineClass) break;
      }
    }
  }

  UClass *searchClasses[] = {gameEngineClass, engineClass};
  for (UClass *targetClass : searchClasses) {
    if (!targetClass) continue;

    {
      std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);
      const auto it = state.ClassToInstances.find(targetClass);
      if (it != state.ClassToInstances.end()) {
        for (UObject *obj : it->second) {
          if (!obj) continue;
          std::string name = GetObjectNameStr(state, obj);
          if (!name.empty() && name.find("Default__") == std::string::npos) {
            state.GEngine = obj;
            Log("[Discovery] Found GEngine (instance of %s): %p",
                targetClass == gameEngineClass ? "GameEngine" : "Engine", state.GEngine);
            return true;
          }
        }
      }
    }

    std::vector<UObject *> derivedInstances = FindObjectsByClassFast(state, targetClass);
    for (UObject *obj : derivedInstances) {
      if (!obj) continue;
      std::string name = GetObjectNameStr(state, obj);
      if (!name.empty() && name.find("Default__") == std::string::npos) {
        state.GEngine = obj;
        UClass *actualCls = GetObjectClass(state, obj);
        Log("[Discovery] Found GEngine (instance of %s): %p",
            actualCls ? GetObjectNameStr(state, (UObject *)actualCls).c_str() : "GameEngine",
            state.GEngine);
        return true;
      }
    }
  }

  Log("[Discovery] GEngine live instance not found");
  return false;
}

namespace {

struct ExecutableRange { uintptr_t begin = 0, end = 0; };

struct StaticConstructCandidate {
  uintptr_t target = 0;
  uintptr_t callSite = 0;
  uintptr_t nativeEntry = 0;
  int32_t nativeFieldOffset = -1;
  int parameterMemberReads = 0;
  const char *evidence = "unknown";
  std::string callerName;
};

struct StaticConstructSearchStats {
  size_t signatureMatches = 0;
  size_t rfFlagMatches = 0;
  size_t newObjectStrings = 0;
  size_t newObjectXrefs = 0;
  size_t newObjectCandidates = 0;
  size_t anchors = 0;
  size_t nativeEntries = 0;
  size_t nativeFunctions = 0;
  int32_t nativeFieldOffset = -1;
  size_t directCalls = 0;
  size_t stackParameterCalls = 0;
  size_t structuralMatches = 0;

  size_t exportCandidates = 0;

  size_t callerFreqCandidates = 0;
  size_t callerFreqCallSites = 0;

  size_t rfPopulationHits = 0;
  size_t rfPopulationCandidates = 0;
};

struct NativeFunctionEntry {
  uintptr_t address = 0;
  UFunction *function = nullptr;
};

struct NativeFunctionCatalog {
  int32_t fieldOffset = -1;
  size_t functionCount = 0;
  size_t nativeFunctionCount = 0;
  size_t executableEntryCount = 0;
  std::vector<NativeFunctionEntry> entries;
};

static std::vector<ExecutableRange>
GetExecutableRanges(const PatternScan::ModuleInfo &module) {
  std::vector<ExecutableRange> ranges;
  const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(module.base);
  if (!module.base || !module.size || !IsReadable(dos, sizeof(*dos)) ||
      dos->e_magic != IMAGE_DOS_SIGNATURE)
    return ranges;
  const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(
      module.base + static_cast<DWORD>(dos->e_lfanew));
  if (!IsReadable(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE)
    return ranges;

  const uintptr_t moduleEnd = module.base + module.size;
  const auto *section = IMAGE_FIRST_SECTION(nt);
  for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
    if (!(section->Characteristics & IMAGE_SCN_MEM_EXECUTE) ||
        !section->Misc.VirtualSize)
      continue;
    const uintptr_t begin = module.base + section->VirtualAddress;
    const uintptr_t end = (std::min)(
        moduleEnd, begin + static_cast<uintptr_t>(section->Misc.VirtualSize));
    if (begin < end)
      ranges.push_back({begin, end});
  }
  return ranges;
}

static std::vector<ExecutableRange> GetAllExecutableRanges() {
  std::vector<ExecutableRange> allRanges;
  const auto mainMod = PatternScan::GetMainModule();
  const auto modules = PatternScan::GetLoadedModules();
  std::vector<PatternScan::ModuleInfo> prioritized;
  std::vector<PatternScan::ModuleInfo> others;

  if (mainMod.base && mainMod.size) {
    prioritized.push_back(mainMod);
  }
  for (const auto &mod : modules) {
    if (mod.base == mainMod.base)
      continue;
    std::string name = mod.name;
    for (char &c : name)
      c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    if (name.find("coreuobject") != std::string::npos ||
        name.find("engine") != std::string::npos ||
        name.find("unreal") != std::string::npos) {
      prioritized.push_back(mod);
    } else {
      others.push_back(mod);
    }
  }

  for (const auto &mod : prioritized) {
    const auto ranges = GetExecutableRanges(mod);
    allRanges.insert(allRanges.end(), ranges.begin(), ranges.end());
  }
  for (const auto &mod : others) {
    const auto ranges = GetExecutableRanges(mod);
    allRanges.insert(allRanges.end(), ranges.begin(), ranges.end());
  }
  return allRanges;
}

static bool IsExecutableAddress(const std::vector<ExecutableRange> &ranges,
                                uintptr_t address) {
  for (const auto &range : ranges) {
    if (address >= range.begin && address < range.end)
      return true;
  }
  return false;
}

static uintptr_t FollowJumpThunk(uintptr_t entry,
                                 const std::vector<ExecutableRange> &ranges) {
  uint8_t code[5] = {};
  if (!SafeMem_Read(reinterpret_cast<void *>(entry), code, sizeof(code)) ||
      code[0] != 0xE9)
    return entry;
  int32_t displacement = 0;
  memcpy(&displacement, code + 1, sizeof(displacement));
  const uintptr_t destination = entry + 5 + displacement;
  return IsExecutableAddress(ranges, destination) ? destination : entry;
}

static std::vector<UFunction *> SnapshotLiveUFunctions(const EngineState &state) {
  std::vector<UFunction *> functions;
  if (!state.ReflectionCacheMutex)
    return functions;
  std::lock_guard<std::mutex> lock(*state.ReflectionCacheMutex);
  functions.reserve(state.LiveUFunctions.size());
  for (UFunction *function : state.LiveUFunctions) {
    if (function)
      functions.push_back(function);
  }
  return functions;
}

static NativeFunctionCatalog BuildNativeFunctionCatalog(
    EngineState &state, const std::vector<ExecutableRange> &ranges) {
  NativeFunctionCatalog catalog;
  const auto functions = SnapshotLiveUFunctions(state);
  catalog.functionCount = functions.size();
  if (functions.empty())
    return catalog;

  constexpr int32_t kFirstField = 0x40;
  constexpr int32_t kLastField = 0x200;
  constexpr size_t kProbeSize = kLastField + sizeof(uintptr_t);

  constexpr size_t kFieldCount =
      (kLastField - kFirstField) / sizeof(uintptr_t) + 1;
  std::array<size_t, kFieldCount> executableCounts{};

  constexpr size_t kMaxSamples = 1000;
  std::vector<std::array<uint8_t, kProbeSize>> samples;
  samples.reserve((std::min)(functions.size(), kMaxSamples));
  std::vector<UFunction *> sampledFunctions;
  sampledFunctions.reserve((std::min)(functions.size(), kMaxSamples));
  for (UFunction *function : functions) {
    if (samples.size() >= kMaxSamples)
      break;
    std::array<uint8_t, kProbeSize> bytes{};
    if (!SafeMem_Read(function, bytes.data(), bytes.size()))
      continue;

    bool isNative = true;
    if (state.UFunction_FlagsOff >= 0 &&
        state.UFunction_FlagsOff + static_cast<int32_t>(sizeof(uint32_t)) <=
            static_cast<int32_t>(bytes.size())) {
      uint32_t flags = 0;
      memcpy(&flags, bytes.data() + state.UFunction_FlagsOff, sizeof(flags));
      isNative = (flags & kFuncNative) != 0;
    }
    if (!isNative)
      continue;

    ++catalog.nativeFunctionCount;
    for (int32_t offset = kFirstField; offset <= kLastField;
         offset += sizeof(uintptr_t)) {
      uintptr_t value = 0;
      memcpy(&value, bytes.data() + offset, sizeof(value));
      if (IsExecutableAddress(ranges, value)) {
        ++executableCounts[(offset - kFirstField) / sizeof(uintptr_t)];
      }
    }
    sampledFunctions.push_back(function);
    samples.push_back(bytes);
  }

  if (catalog.nativeFunctionCount == 0)
    return catalog;

  size_t bestCount = 0;
  size_t runnerUpCount = 0;
  int32_t bestOffset = -1;
  for (size_t i = 0; i < executableCounts.size(); ++i) {
    const size_t count = executableCounts[i];
    if (count > bestCount) {
      runnerUpCount = bestCount;
      bestCount = count;
      bestOffset = kFirstField + static_cast<int32_t>(i * sizeof(uintptr_t));
    } else if (count > runnerUpCount) {
      runnerUpCount = count;
    }
  }

  const size_t minimumEvidence = (std::min)(size_t{8}, catalog.nativeFunctionCount);
  if (bestCount < minimumEvidence ||
      (runnerUpCount != 0 && bestCount * 4 < runnerUpCount * 5)) {
    return catalog;
  }

  catalog.fieldOffset = bestOffset;
  state.UFunction_FuncOff = bestOffset;
  for (size_t i = 0; i < samples.size(); ++i) {
    uintptr_t entry = 0;
    memcpy(&entry, samples[i].data() + bestOffset, sizeof(entry));
    if (!IsExecutableAddress(ranges, entry))
      continue;
    catalog.entries.push_back({entry, sampledFunctions[i]});
    const uintptr_t destination = FollowJumpThunk(entry, ranges);
    if (destination != entry)
      catalog.entries.push_back({destination, sampledFunctions[i]});
  }

  static const char *kKnownConstructorNames[] = {
      "ConstructObjectFromClass",
      "CreateWidget",
      "Create",
      "SpawnObject",
      "DuplicateObject",
      "CreateDefaultSubobject",
      "InitializeInputComponent",
      "AddNewComponentByClass",
      "AddComponentByClass",
      "CreatePlayer",
      "SpawnActorDeferred",
      "SpawnActor",
      "ConstructObject",
      "NewObject",
  };
  for (const char *fnName : kKnownConstructorNames) {
    auto it = state.FunctionByNameOnly.find(fnName);
    if (it != state.FunctionByNameOnly.end() && it->second) {
      uintptr_t entry = 0;
      if (SafeMem_Read(reinterpret_cast<const uint8_t *>(it->second) + bestOffset,
                       &entry, sizeof(entry)) &&
          IsExecutableAddress(ranges, entry)) {
        catalog.entries.push_back({entry, it->second});
        const uintptr_t destination = FollowJumpThunk(entry, ranges);
        if (destination != entry)
          catalog.entries.push_back({destination, it->second});
      }
    }
  }

  for (UFunction *fn : functions) {
    if (!fn) continue;
    std::string fnName = GetObjectNameStr(state, fn);
    if (fnName.find("Construct") != std::string::npos ||
        fnName.find("CreateWidget") != std::string::npos ||
        fnName.find("SpawnObject") != std::string::npos ||
        fnName.find("DuplicateObject") != std::string::npos) {
      uintptr_t entry = 0;
      if (SafeMem_Read(reinterpret_cast<const uint8_t *>(fn) + bestOffset,
                       &entry, sizeof(entry)) &&
          IsExecutableAddress(ranges, entry)) {
        catalog.entries.push_back({entry, fn});
        const uintptr_t destination = FollowJumpThunk(entry, ranges);
        if (destination != entry)
          catalog.entries.push_back({destination, fn});
      }
    }
  }

  std::sort(catalog.entries.begin(), catalog.entries.end(),
            [](const NativeFunctionEntry &a, const NativeFunctionEntry &b) {
              return a.address != b.address ? a.address < b.address
                                             : a.function < b.function;
            });
  catalog.entries.erase(
      std::unique(catalog.entries.begin(), catalog.entries.end(),
                  [](const NativeFunctionEntry &a,
                     const NativeFunctionEntry &b) {
                    return a.address == b.address && a.function == b.function;
                  }),
      catalog.entries.end());
  catalog.executableEntryCount = catalog.entries.size();
  return catalog;
}

static bool DecodeModRM(const uint8_t *bytes, size_t available, size_t at,
                        uint8_t rex, uint8_t &mod, uint8_t &reg,
                        uint8_t &rm, int32_t &displacement,
                        size_t &instructionSize) {
  if (at >= available)
    return false;
  const uint8_t modrm = bytes[at];
  mod = modrm >> 6;
  reg = ((rex & 0x04) ? 8 : 0) + ((modrm >> 3) & 7);
  rm = ((rex & 0x01) ? 8 : 0) + (modrm & 7);
  displacement = 0;
  instructionSize = 1;
  if (mod == 3)
    return true;
  const bool hasSib = (modrm & 7) == 4;
  uint8_t rawBase = modrm & 7;
  if (hasSib) {
    if (at + instructionSize >= available)
      return false;
    rawBase = bytes[at + instructionSize++] & 7;
    rm = ((rex & 0x01) ? 8 : 0) + rawBase;
  }
  size_t displacementSize = 0;
  if (mod == 1)
    displacementSize = 1;
  else if (mod == 2 || (mod == 0 && rawBase == 5))
    displacementSize = 4;
  if (at + instructionSize + displacementSize > available)
    return false;
  if (displacementSize == 1)
    displacement = static_cast<int8_t>(bytes[at + instructionSize]);
  else if (displacementSize == 4)
    memcpy(&displacement, bytes + at + instructionSize, sizeof(displacement));
  instructionSize += displacementSize;
  return true;
}

static int ScoreStaticConstructParameterReads(uintptr_t target) {
  constexpr size_t kProbeSize = 0x800;
  uint8_t code[kProbeSize] = {};
  if (!SafeMem_Read(reinterpret_cast<void *>(target), code, sizeof(code)))
    return 0;

  uint16_t parameterRegisters = static_cast<uint16_t>(1u << 1);
  std::set<int32_t> memberOffsets;
  bool hasReturn = false;
  for (size_t i = 0; i + 3 < sizeof(code); ++i) {
    if (code[i] == 0xC3 || code[i] == 0xC2) {
      hasReturn = true;
      break;
    }
    uint8_t rex = 0;
    if ((code[i] & 0xF0) == 0x40) {
      rex = code[i++];
      if (i >= sizeof(code))
        break;
    }
    const uint8_t opcode = code[i];
    if (opcode != 0x8B && opcode != 0x89 && opcode != 0x8D && opcode != 0x0F)
      continue;

    size_t opcodeSize = 1;
    if (opcode == 0x0F) {
      if (i + 1 >= sizeof(code))
        continue;
      const uint8_t op2 = code[i + 1];
      if (op2 == 0xB6 || op2 == 0xB7 || op2 == 0xBE || op2 == 0xBF) {
        opcodeSize = 2;
      } else {
        continue;
      }
    }

    uint8_t mod = 0, reg = 0, rm = 0;
    int32_t displacement = 0;
    size_t modrmSize = 0;
    if (!DecodeModRM(code, sizeof(code), i + opcodeSize, rex, mod, reg, rm,
                     displacement, modrmSize))
      continue;
    if (mod == 3) {
      if ((opcode == 0x8B || opcode == 0x89) && (parameterRegisters & (1u << rm)))
        parameterRegisters |= static_cast<uint16_t>(1u << reg);
      else if ((opcode == 0x8B || opcode == 0x89) && (parameterRegisters & (1u << reg)))
        parameterRegisters |= static_cast<uint16_t>(1u << rm);
      continue;
    }
    if ((parameterRegisters & (1u << rm)) && displacement >= 0 &&
        displacement <= 0xB0) {
      memberOffsets.insert(displacement);
      if (opcode == 0x8D)
        parameterRegisters |= static_cast<uint16_t>(1u << reg);
    }
  }
  return hasReturn ? static_cast<int>(memberOffsets.size()) : 0;
}

struct StaticConstructBody {
  size_t bodyBytes = 0;
  bool maskInBody = false;
};

static StaticConstructBody ProbeStaticConstructBody(uintptr_t target) {
  StaticConstructBody body;
  constexpr size_t kProbeSize = 0x800;
  uint8_t code[kProbeSize] = {};
  if (!SafeMem_Read(reinterpret_cast<void *>(target), code, sizeof(code)))
    return body;
  body.bodyBytes = kProbeSize;
  for (size_t i = 0; i + 3 < sizeof(code); ++i) {
    if (code[i] == 0xC3 || code[i] == 0xC2) {
      body.bodyBytes = i;
      break;
    }
  }
  static constexpr uint8_t kRFMask[] = {0x80, 0x00, 0x00, 0x10};
  for (size_t i = 0; i + sizeof(kRFMask) <= body.bodyBytes; ++i) {
    if (memcmp(code + i, kRFMask, sizeof(kRFMask)) == 0) {
      body.maskInBody = true;
      break;
    }
  }
  return body;
}

static size_t CountDirectCallers(const std::vector<ExecutableRange> &ranges,
                                 uintptr_t target) {
  constexpr size_t kChunkSize = 256 * 1024;
  std::vector<uint8_t> code(kChunkSize);
  for (const auto &range : ranges) {
    for (uintptr_t cursor = range.begin; cursor < range.end;) {
      const size_t bytesToRead = static_cast<size_t>(
          (std::min)(range.end - cursor, static_cast<uintptr_t>(kChunkSize)));
      if (bytesToRead < 5)
        break;
      if (SafeMem_ReadSpanExact(reinterpret_cast<const void *>(cursor), code.data(),
                                bytesToRead) == bytesToRead) {
        for (size_t i = 0; i + 5 <= bytesToRead; ++i) {
          if (code[i] != 0xE8 && code[i] != 0xE9)
            continue;
          int32_t displacement = 0;
          memcpy(&displacement, code.data() + i + 1, sizeof(displacement));
          if (cursor + i + 5 + displacement == target)
            return 1;
        }
      }
      cursor += bytesToRead;
    }
  }
  return 0;
}

static std::string MakeBytePattern(const wchar_t *text) {
  std::ostringstream pattern;

  const size_t bytes = wcslen(text) * sizeof(wchar_t);
  const auto *raw = reinterpret_cast<const uint8_t *>(text);
  for (size_t i = 0; i < bytes; ++i) {
    if (i) pattern << ' ';
    pattern << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned int>(raw[i]);
  }
  return pattern.str();
}

static std::vector<uintptr_t> FindAllModulePatternMatches(
    const PatternScan::ModuleInfo &module, const char *pattern) {
  std::vector<uintptr_t> matches;
  const uintptr_t end = module.base + module.size;
  for (uintptr_t cursor = module.base; cursor < end;) {
    const uintptr_t found = PatternScan::FindPattern(cursor, end - cursor, pattern);
    if (!found || found >= end)
      break;
    matches.push_back(found);
    cursor = found + 1;
  }
  return matches;
}

static std::vector<uintptr_t> FindRipLeaXrefs(
    const std::vector<ExecutableRange> &ranges, uintptr_t referencedAddress) {
  std::vector<uintptr_t> xrefs;
  constexpr size_t kChunkSize = 256 * 1024;
  std::vector<uint8_t> code(kChunkSize);
  for (const auto &range : ranges) {
    for (uintptr_t cursor = range.begin; cursor < range.end;) {
      const size_t bytesToRead = static_cast<size_t>(
          (std::min)(range.end - cursor, static_cast<uintptr_t>(kChunkSize)));
      if (bytesToRead < 7)
        break;
      if (SafeMem_ReadSpanExact(reinterpret_cast<const void *>(cursor), code.data(),
                                bytesToRead) == bytesToRead) {
        for (size_t i = 0; i + 7 <= bytesToRead; ++i) {
          const uint8_t rex = code[i];
          if ((rex & 0xF0) != 0x40 || code[i + 1] != 0x8D)
            continue;
          const uint8_t modrm = code[i + 2];

          if ((modrm >> 6) != 0 || (modrm & 7) != 5)
            continue;
          int32_t displacement = 0;
          memcpy(&displacement, code.data() + i + 3, sizeof(displacement));
          if (cursor + i + 7 + displacement == referencedAddress)
            xrefs.push_back(cursor + i);
        }
      }
      cursor += bytesToRead;
    }
  }
  std::sort(xrefs.begin(), xrefs.end());
  xrefs.erase(std::unique(xrefs.begin(), xrefs.end()), xrefs.end());
  return xrefs;
}

static std::unordered_set<uintptr_t>
FindRipLeaTargetsAmong(const std::vector<ExecutableRange> &ranges,
                       const std::unordered_set<uintptr_t> &candidates) {
  std::unordered_set<uintptr_t> referenced;
  if (candidates.empty())
    return referenced;
  constexpr size_t kChunkSize = 256 * 1024;
  std::vector<uint8_t> code(kChunkSize);
  for (const auto &range : ranges) {
    for (uintptr_t cursor = range.begin; cursor < range.end;) {
      const size_t bytesToRead = static_cast<size_t>(
          (std::min)(range.end - cursor, static_cast<uintptr_t>(kChunkSize)));
      if (bytesToRead < 7)
        break;
      if (SafeMem_ReadSpanExact(reinterpret_cast<const void *>(cursor), code.data(),
                                bytesToRead) == bytesToRead) {
        for (size_t i = 0; i + 7 <= bytesToRead; ++i) {
          const uint8_t rex = code[i];
          if ((rex & 0xF0) != 0x40 || code[i + 1] != 0x8D)
            continue;
          const uint8_t modrm = code[i + 2];

          if ((modrm >> 6) != 0 || (modrm & 7) != 5)
            continue;
          int32_t displacement = 0;
          memcpy(&displacement, code.data() + i + 3, sizeof(displacement));
          const uintptr_t dest = cursor + i + 7 + displacement;
          if (candidates.count(dest))
            referenced.insert(dest);
        }
      }
      cursor += bytesToRead;
    }
  }
  return referenced;
}

static void FindStaticConstructNewObjectStringCandidates(
    const PatternScan::ModuleInfo &mainMod,
    const std::vector<ExecutableRange> &ranges,
    std::vector<StaticConstructCandidate> &candidates,
    StaticConstructSearchStats &stats) {
  static constexpr wchar_t kNewObjectMessage[] =
      L"NewObject with empty name can't be used to create default";
  const std::string pattern = MakeBytePattern(kNewObjectMessage);
  const auto strings = FindAllModulePatternMatches(mainMod, pattern.c_str());
  stats.newObjectStrings = strings.size();

  struct TargetHit {
    size_t hits = 0;
    uintptr_t callSite = 0;
    uintptr_t xref = 0;
  };
  std::unordered_map<uintptr_t, TargetHit> hitsByTarget;

  for (const uintptr_t stringAddress : strings) {
    for (const uintptr_t xref : FindRipLeaXrefs(ranges, stringAddress)) {
      ++stats.newObjectXrefs;
      constexpr uintptr_t kBefore = 0x200;
      constexpr size_t kWindow = 0xA00;
      const uintptr_t start = xref > kBefore ? xref - kBefore : xref;
      uint8_t code[kWindow] = {};
      if (!SafeMem_Read(reinterpret_cast<void *>(start), code, sizeof(code)))
        continue;
      for (size_t i = 0; i + 5 <= sizeof(code); ++i) {
        if (code[i] != 0xE8)
          continue;
        int32_t displacement = 0;
        memcpy(&displacement, code + i + 1, sizeof(displacement));
        const uintptr_t callSite = start + i;
        const uintptr_t target = callSite + 5 + displacement;
        if (!IsExecutableAddress(ranges, target))
          continue;
        auto &hit = hitsByTarget[target];
        if (hit.hits == 0) {
          hit.callSite = callSite;
          hit.xref = xref;
        }
        ++hit.hits;
      }
    }
  }

  std::vector<std::pair<uintptr_t, TargetHit>> ranked(hitsByTarget.begin(),
                                                      hitsByTarget.end());
  std::sort(ranked.begin(), ranked.end(),
            [](const std::pair<uintptr_t, TargetHit> &a,
               const std::pair<uintptr_t, TargetHit> &b) {
              return a.second.hits != b.second.hits
                         ? a.second.hits > b.second.hits
                         : a.first < b.first;
            });

  for (const auto &rankedTarget : ranked) {
    const uintptr_t target = rankedTarget.first;
    const TargetHit &hit = rankedTarget.second;
    if (!ProbeStaticConstructBody(target).maskInBody)
      continue;
    const int memberReads = ScoreStaticConstructParameterReads(target);
    ++stats.rfFlagMatches;
    ++stats.newObjectCandidates;
    candidates.push_back({target, hit.callSite, hit.xref, -1, memberReads,
                          "newobject-string"});
    return;
  }
}

static void FindStaticConstructByCallerFrequency(
    EngineState &state, const std::vector<ExecutableRange> &ranges,
    std::vector<StaticConstructCandidate> &candidates,
    StaticConstructSearchStats &stats) {
  const NativeFunctionCatalog catalog = BuildNativeFunctionCatalog(state, ranges);
  stats.nativeFunctions = catalog.nativeFunctionCount;
  stats.nativeEntries = catalog.executableEntryCount;
  stats.nativeFieldOffset = catalog.fieldOffset;
  if (catalog.entries.empty())
    return;

  struct FreqEntry {
    size_t callCount = 0;
    int bestMemberReads = 0;
    uintptr_t callSite = 0;
    uintptr_t nativeEntry = 0;
    int32_t nativeFieldOffset = -1;
  };
  std::unordered_map<uintptr_t, FreqEntry> freq;

  constexpr size_t kMaxProbeEntries = 1000;
  constexpr size_t kProbeCodeSize = 0x800;

  size_t probed = 0;
  for (const auto &entry : catalog.entries) {
    if (probed >= kMaxProbeEntries)
      break;
    ++probed;

    uint8_t code[kProbeCodeSize] = {};
    if (!SafeMem_Read(reinterpret_cast<void *>(entry.address), code, sizeof(code)))
      continue;

    for (size_t off = 0; off + 5 <= sizeof(code); ++off) {
      if (code[off] != 0xE8)
        continue;
      int32_t disp = 0;
      memcpy(&disp, code + off + 1, sizeof(disp));
      const uintptr_t callSite = entry.address + off;
      const uintptr_t target = callSite + 5 + disp;
      if (!IsExecutableAddress(ranges, target))
        continue;
      ++stats.callerFreqCallSites;
      auto &fe = freq[target];
      ++fe.callCount;
      if (fe.callSite == 0) {
        fe.callSite = callSite;
        fe.nativeEntry = entry.address;
        fe.nativeFieldOffset = catalog.fieldOffset;
      }
      if (fe.bestMemberReads == 0)
        fe.bestMemberReads = ScoreStaticConstructParameterReads(target);
    }
  }

  struct Scored {
    uintptr_t target;
    FreqEntry fe;
    int totalScore;
  };
  std::vector<Scored> verified;
  for (auto &[target, fe] : freq) {
    if (fe.bestMemberReads < 3)
      continue;
    if (!ProbeStaticConstructBody(target).maskInBody)
      continue;
    ++stats.rfFlagMatches;
    ++stats.callerFreqCandidates;
    int totalScore = 100 + fe.bestMemberReads * 10 + static_cast<int>(fe.callCount);
    verified.push_back({target, fe, totalScore});
  }
  if (verified.empty())
    return;

  std::sort(verified.begin(), verified.end(),
            [](const Scored &a, const Scored &b) {
              return a.totalScore != b.totalScore ? a.totalScore > b.totalScore
                                                  : a.fe.callCount > b.fe.callCount;
            });

  const auto &best = verified[0];
  candidates.push_back({best.target, best.fe.callSite, best.fe.nativeEntry,
                        best.fe.nativeFieldOffset, best.fe.bestMemberReads,
                        "dynamic-reflection"});
}

static void FindStaticConstructExportCandidate(
    const std::vector<ExecutableRange> &ranges,
    std::vector<StaticConstructCandidate> &candidates,
    StaticConstructSearchStats &stats) {
  static const char *kExportNames[] = {
      "?StaticConstructObject_Internal@@YAPEAVUObject@@AEBUFStaticConstructObjectParameters@@@Z",
      "?StaticConstructObject_Internal@@YAPEAVUObject@@AEAUFStaticConstructObjectParameters@@@Z",
      "?StaticConstructObject_Internal@@YAPEAVUObject@@PEBVUClass@@PEAV1@VFName@@W4EObjectFlags@@W4EInternalObjectFlags@@PEAV1@_NPEAUFObjectInstancingGraph@@4PEAVUPackage@@@Z",
      "StaticConstructObject_Internal",
  };
  const auto modules = PatternScan::GetLoadedModules();
  for (const auto &mod : modules) {
    for (const char *exportName : kExportNames) {
      void *exported = PatternScan::FindExport(mod.handle, exportName);
      if (!exported)
        continue;
      const uintptr_t target = reinterpret_cast<uintptr_t>(exported);
      if (!IsExecutableAddress(ranges, target))
        continue;
      ++stats.exportCandidates;
      const int memberReads = ScoreStaticConstructParameterReads(target);
      candidates.push_back({target, 0, 0, -1, memberReads, "export"});
      return;
    }
  }
}

static void FindStaticConstructByRFPopulationScan(
    const std::vector<ExecutableRange> &ranges,
    std::vector<StaticConstructCandidate> &candidates,
    StaticConstructSearchStats &stats) {
  constexpr uint8_t kRFMask[] = {0x80, 0x00, 0x00, 0x10};
  constexpr size_t kChunk = 256 * 1024;
  constexpr size_t kLookBack = 0x1000;

  std::unordered_set<uintptr_t> functionStarts;

  auto scanChunk = [&](const ExecutableRange &range, uintptr_t cursor,
                       size_t toRead, std::vector<uint8_t> &codeBuffer,
                       std::vector<uint8_t> &backBuffer,
                       std::vector<uintptr_t> &starts) -> int {
    if (toRead < sizeof(kRFMask) ||
        SafeMem_ReadSpanExact(reinterpret_cast<const void *>(cursor),
                              codeBuffer.data(), toRead) != toRead)
      return 0;

    int hits = 0;
    const uint8_t *data = codeBuffer.data();
    const uint8_t *end = data + toRead;
    const uint8_t *scan = data;
    while (scan + 4 <= end) {
      const uint8_t *found = static_cast<const uint8_t *>(
          memchr(scan, 0x80, end - scan - 3));
      if (!found)
        break;

      uint32_t val = 0;
      memcpy(&val, found, 4);
      if (val == 0x10000080) {
        ++hits;
        const uintptr_t hitAddr = cursor + (found - data);
        uintptr_t fnStart = hitAddr;

        const uintptr_t backLimit =
            hitAddr > kLookBack ? hitAddr - kLookBack : range.begin;
        const size_t backSize = static_cast<size_t>(hitAddr - backLimit);
        if (backSize > 0 &&
            SafeMem_Read(reinterpret_cast<void *>(backLimit),
                         backBuffer.data(), backSize)) {

          for (size_t p = backSize; p > 0; --p) {
            const uint8_t prevByte = backBuffer[p - 1];
            if (prevByte == 0xCC || prevByte == 0x90) {
              fnStart = backLimit + p;
              break;
            }
          }
        }
        starts.push_back(fnStart);
      }
      scan = found + 1;
    }
    return hits;
  };

  for (const auto &range : ranges) {
    const size_t rangeBytes =
        range.end > range.begin ? static_cast<size_t>(range.end - range.begin) : 0;
    if (rangeBytes == 0)
      continue;
    const size_t chunkCount = (rangeBytes + kChunk - 1) / kChunk;
    functionStarts.clear();

    if (rangeBytes < 8 * 1024 * 1024 || chunkCount <= 1) {
      std::vector<uint8_t> codeBuffer(kChunk);
      std::vector<uint8_t> backBuffer(kLookBack);
      std::vector<uintptr_t> starts;
      for (size_t c = 0; c < chunkCount; ++c) {
        const uintptr_t cursor = range.begin + c * kChunk;
        const size_t toRead = static_cast<size_t>(
            (std::min)(range.end - cursor, static_cast<uintptr_t>(kChunk)));
        stats.rfPopulationHits += scanChunk(range, cursor, toRead, codeBuffer,
                                            backBuffer, starts);
      }
      functionStarts.insert(starts.begin(), starts.end());
    } else {
      unsigned hw = std::thread::hardware_concurrency();
      if (hw == 0) hw = 1;
      const unsigned workerCount = (std::min)(
          (std::min)(static_cast<unsigned>(chunkCount), hw), 8u);
      std::vector<std::vector<uintptr_t>> starts(workerCount);
      std::vector<std::vector<uint8_t>> codeBuffers(
          workerCount, std::vector<uint8_t>(kChunk));
      std::vector<std::vector<uint8_t>> backBuffers(
          workerCount, std::vector<uint8_t>(kLookBack));
      std::vector<int> hitCounts(workerCount, 0);
      std::atomic<size_t> nextChunk{0};
      std::vector<std::thread> workers;
      workers.reserve(workerCount);
      for (unsigned w = 0; w < workerCount; ++w) {
        workers.emplace_back([&, w]() {
          for (;;) {
            const size_t c = nextChunk.fetch_add(1, std::memory_order_relaxed);
            if (c >= chunkCount)
              break;
            const uintptr_t cursor = range.begin + c * kChunk;
            const size_t toRead = static_cast<size_t>(
                (std::min)(range.end - cursor, static_cast<uintptr_t>(kChunk)));
            hitCounts[w] += scanChunk(range, cursor, toRead, codeBuffers[w],
                                      backBuffers[w], starts[w]);
          }
        });
      }
      for (auto &worker : workers)
        worker.join();
      for (unsigned w = 0; w < workerCount; ++w) {
        stats.rfPopulationHits += hitCounts[w];
        functionStarts.insert(starts[w].begin(), starts[w].end());
      }
    }

    if (!functionStarts.empty()) {
      struct RFScored {
        uintptr_t start;
        int score;
        int memberReads;
      };
      std::vector<RFScored> scored;
      std::vector<uintptr_t> starts(functionStarts.begin(), functionStarts.end());
      if (starts.size() < 64) {
        for (const uintptr_t fnStart : starts) {
          if (!ProbeStaticConstructBody(fnStart).maskInBody)
            continue;
          const int memberReads = ScoreStaticConstructParameterReads(fnStart);
          if (memberReads < 3)
            continue;
          scored.push_back({fnStart, memberReads * 10 + 30, memberReads});
        }
      } else {
        unsigned hw = std::thread::hardware_concurrency();
        if (hw == 0) hw = 1;
        const unsigned workerCount = (std::min)(hw, 8u);
        std::vector<std::vector<RFScored>> local(workerCount);
        std::atomic<size_t> nextStart{0};
        std::vector<std::thread> workers;
        workers.reserve(workerCount);
        for (unsigned w = 0; w < workerCount; ++w) {
          workers.emplace_back([&, w]() {
            for (;;) {
              const size_t i = nextStart.fetch_add(1, std::memory_order_relaxed);
              if (i >= starts.size())
                break;
              const uintptr_t fnStart = starts[i];
              if (!ProbeStaticConstructBody(fnStart).maskInBody)
                continue;
              const int memberReads = ScoreStaticConstructParameterReads(fnStart);
              if (memberReads < 3)
                continue;
              local[w].push_back({fnStart, memberReads * 10 + 30, memberReads});
            }
          });
        }
        for (auto &worker : workers)
          worker.join();
        for (auto &v : local)
          scored.insert(scored.end(), v.begin(), v.end());
      }
      if (!scored.empty()) {
        std::sort(scored.begin(), scored.end(),
                  [](const RFScored &a, const RFScored &b) {
                    return a.score > b.score;
                  });
        if (scored[0].score >= 50 &&
            (scored.size() == 1 || scored[0].score >= scored[1].score + 10) &&
            CountDirectCallers(ranges, scored[0].start) != 0) {
          ++stats.rfPopulationCandidates;
          ++stats.rfFlagMatches;
          candidates.push_back({scored[0].start, 0, 0, -1,
                                scored[0].memberReads, "rf-population-scan"});
          return;
        }
      }
      functionStarts.clear();
    }
  }
}

static std::vector<StaticConstructCandidate>
FindVerifiedStaticConstructCandidates(EngineState &state,
                                      const PatternScan::ModuleInfo &mainMod,
                                      StaticConstructSearchStats &stats) {
  std::vector<StaticConstructCandidate> candidates;

  const auto ranges = GetAllExecutableRanges();
  if (ranges.empty())
    return candidates;

  FindStaticConstructByCallerFrequency(state, ranges, candidates, stats);
  if (candidates.size() == 1) {
    return candidates;
  }

  if (candidates.empty()) {
    FindStaticConstructNewObjectStringCandidates(mainMod, ranges, candidates, stats);
    if (candidates.size() == 1) {
      return candidates;
    }
  }

  if (candidates.empty()) {
    FindStaticConstructExportCandidate(ranges, candidates, stats);
    if (candidates.size() == 1) {
      return candidates;
    }
  }

  if (candidates.empty()) {
    FindStaticConstructByRFPopulationScan(ranges, candidates, stats);
  }

  std::sort(candidates.begin(), candidates.end(),
            [](const StaticConstructCandidate &a,
               const StaticConstructCandidate &b) { return a.target < b.target; });
  candidates.erase(std::unique(candidates.begin(), candidates.end(),
      [](const StaticConstructCandidate &a, const StaticConstructCandidate &b) {
        return a.target == b.target;
      }), candidates.end());

  return candidates;
}

}

bool FindStaticConstructObject(EngineState &state) {
  if (state.StaticConstructObject) {
    const uintptr_t target = reinterpret_cast<uintptr_t>(state.StaticConstructObject);
    if (ProbeStaticConstructBody(target).maskInBody) {
      const int memberReads = ScoreStaticConstructParameterReads(target);
      state.bStaticConstructTakesParams = (memberReads >= 3) || (state.UEMajor >= 5) || (state.UEMajor == 4 && state.UEMinor >= 26);
      Log("[Discovery] StaticConstructObject_Internal verified from cache: %p (RF flags; parameter-members=%d).",
          reinterpret_cast<void *>(target), memberReads);
      return true;
    }
    Log("[Discovery] StaticConstructObject_Internal cache candidate %p failed semantic check; falling back to full scan.",
        reinterpret_cast<void *>(state.StaticConstructObject));
    state.StaticConstructObject = nullptr;
  }
  if (!state.bIndexBuilt || !state.UFunctionClass) {
    Log("[Discovery] StaticConstructObject verifier deferred: reflection index is not ready.");
    return false;
  }
  StaticConstructSearchStats stats;
  const auto candidates = FindVerifiedStaticConstructCandidates(
      state, PatternScan::GetMainModule(), stats);
  if (IsVerboseLoggingEnabled()) {
    Log("[Discovery] StaticConstructObject verifier: signature-matches=%zu rf-flag-matches=%zu newobject-strings=%zu newobject-xrefs=%zu newobject-candidates=%zu anchors=%zu native-functions=%zu native-entries=%zu native-field=0x%X direct-calls=%zu stack-parameter-calls=%zu structural-matches=%zu exports=%zu freq-calls=%zu freq-candidates=%zu rf-pop-hits=%zu rf-pop-candidates=%zu.",
        stats.signatureMatches, stats.rfFlagMatches, stats.newObjectStrings,
        stats.newObjectXrefs, stats.newObjectCandidates, stats.anchors,
        stats.nativeFunctions, stats.nativeEntries, stats.nativeFieldOffset,
        stats.directCalls, stats.stackParameterCalls, stats.structuralMatches,
        stats.exportCandidates, stats.callerFreqCallSites, stats.callerFreqCandidates,
        stats.rfPopulationHits, stats.rfPopulationCandidates);
  }
  if (candidates.size() != 1) {
    Log("[Discovery] StaticConstructObject_Internal NOT confirmed: verifier found %zu candidate(s); refusing to select one.",
        candidates.size());
    for (const auto &candidate : candidates) {
      Log("[Discovery]   rejected candidate=%p call=%p members=%d evidence=%s%s%s",
          reinterpret_cast<void *>(candidate.target),
          reinterpret_cast<void *>(candidate.callSite),
          candidate.parameterMemberReads, candidate.evidence,
          candidate.callerName.empty() ? "" : " caller=",
          candidate.callerName.empty() ? "" : candidate.callerName.c_str());
    }
    return false;
  }
  const auto &candidate = candidates.front();
  state.StaticConstructObject =
      reinterpret_cast<StaticConstructObject_Internal_t>(candidate.target);
  state.bStaticConstructTakesParams = (candidate.parameterMemberReads >= 3) || (state.UEMajor >= 5) || (state.UEMajor == 4 && state.UEMinor >= 26);
  if (stats.nativeFieldOffset >= 0) {
    Log("[Discovery] StaticConstructObject_Internal verified: %p (pattern=%s; RF flags; parameter-members=%d; UFunction::Func=0x%X, native-bindings=%zu).",
        reinterpret_cast<void *>(state.StaticConstructObject), candidate.evidence,
        candidate.parameterMemberReads, stats.nativeFieldOffset,
        stats.nativeEntries);
  } else {
    Log("[Discovery] StaticConstructObject_Internal verified: %p (pattern=%s; RF flags; parameter-members=%d).",
        reinterpret_cast<void *>(state.StaticConstructObject), candidate.evidence,
        candidate.parameterMemberReads);
  }
  if (!candidate.callerName.empty()) {
    Log("[Discovery] StaticConstructObject_Internal caller: %s (native=%p; UFunction::Func=0x%X).",
        candidate.callerName.c_str(), reinterpret_cast<void *>(candidate.nativeEntry),
        candidate.nativeFieldOffset);
  } else if (IsVerboseLoggingEnabled()) {
    Log("[Discovery] StaticConstructObject verifier: verified call at %p has no reflected native owner.",
        reinterpret_cast<void *>(candidate.callSite));
  }
  return true;
}

namespace {

struct ConsoleManagerHit {
  void *manager = nullptr;

  uintptr_t location = 0;
  int mapOff = -1;
  int entries = 0;
  int valid = 0;
  int sampled = 0;
  int stride = 0;
};

constexpr int kConsoleMapStrides[] = {0x18, 0x20, 0x28, 0x30};
constexpr int kConsoleMapWindow = 16;
constexpr int kConsoleMapWindows = 4;

constexpr int kMinConsoleObjects = 8;
constexpr size_t kManagerHeaderBytes = 0xB0;

bool IsInsideLoadedModule(uintptr_t address,
                          const std::vector<PatternScan::ModuleInfo> &modules) {
  for (const auto &mod : modules) {
    if (mod.base && address >= mod.base && address < mod.base + mod.size)
      return true;
  }
  return false;
}

bool IsLiveConsoleObjectName(const uint8_t *key) {
  void *text = nullptr;
  int32_t length = 0;
  int32_t capacity = 0;
  memcpy(&text, key, sizeof(text));
  memcpy(&length, key + 8, sizeof(length));
  memcpy(&capacity, key + 12, sizeof(capacity));
  if (!text || length <= 0 || length > capacity)
    return false;
  const uintptr_t textAddr = (uintptr_t)text;
  if (textAddr < 0x10000 || textAddr >= 0x7FFFFFFFFFFFULL)
    return false;
  return IsMapped(text, static_cast<size_t>(length) * sizeof(wchar_t));
}

bool IsConsoleObjectValue(const uint8_t *field,
                          const std::vector<PatternScan::ModuleInfo> &modules) {
  void *object = nullptr;
  memcpy(&object, field, sizeof(object));
  const uintptr_t objectAddr = (uintptr_t)object;
  if (objectAddr < 0x10000 || objectAddr >= 0x7FFFFFFFFFFFULL)
    return false;
  if (!IsMapped(object, sizeof(void *)))
    return false;
  const uintptr_t vtable = (uintptr_t)SafeMem_ReadPtr(object);
  return vtable != 0 && IsInsideLoadedModule(vtable, modules);
}

int ScoreConsoleObjectElements(
    const uint8_t *dataPtr, int32_t elements, int stride,
    const std::vector<PatternScan::ModuleInfo> &modules, int &sampledOut) {
  int valid = 0;
  int sampled = 0;
  for (int window = 0; window < kConsoleMapWindows; ++window) {
    int32_t first =
        static_cast<int32_t>(static_cast<int64_t>(elements) * window /
                             kConsoleMapWindows);
    if (first >= elements)
      break;
    int32_t count = kConsoleMapWindow;
    if (first + count > elements)
      count = elements - first;
    if (count <= 0)
      continue;

    uint8_t buffer[kConsoleMapWindow * kConsoleMapStrides[3]];
    const size_t bytes = static_cast<size_t>(count) * stride;
    const uint8_t *base = dataPtr + static_cast<size_t>(first) * stride;
    if (bytes > sizeof(buffer))
      continue;
    if (!IsMapped(base, bytes) || !SafeMem_Read(base, buffer, bytes))
      continue;

    for (int32_t i = 0; i < count; ++i) {
      const uint8_t *element = buffer + static_cast<size_t>(i) * stride;
      ++sampled;
      if (IsLiveConsoleObjectName(element) &&
          IsConsoleObjectValue(element + 0x10, modules))
        ++valid;
    }
  }
  sampledOut = sampled;
  return valid;
}

bool DescribeConsoleManager(
    void *candidatePtr, const std::vector<PatternScan::ModuleInfo> &modules,
    ConsoleManagerHit &hit) {
  const uintptr_t candidate = (uintptr_t)candidatePtr;
  if (!IsMapped(candidatePtr, kManagerHeaderBytes))
    return false;

  uint8_t header[kManagerHeaderBytes];
  if (!SafeMem_Read(candidatePtr, header, sizeof(header)))
    return false;

  void *vtable = nullptr;
  memcpy(&vtable, header, sizeof(vtable));
  if (!vtable || !IsInsideLoadedModule((uintptr_t)vtable, modules))
    return false;

  bool found = false;
  for (int off = 0x8; off <= 0xA0; off += 8) {
    void *dataPtr = nullptr;
    int32_t elements = 0;
    int32_t capacity = 0;
    memcpy(&dataPtr, header + off, sizeof(dataPtr));
    memcpy(&elements, header + off + 8, sizeof(elements));
    memcpy(&capacity, header + off + 12, sizeof(capacity));

    const uintptr_t dataAddr = (uintptr_t)dataPtr;
    if (!dataPtr || dataAddr < 0x10000 || dataAddr >= 0x7FFFFFFFFFFFULL)
      continue;
    if (elements <= 0 || capacity < elements || capacity > (1 << 24))
      continue;

    for (int stride : kConsoleMapStrides) {
      int sampled = 0;
      const int valid = ScoreConsoleObjectElements(
          static_cast<const uint8_t *>(dataPtr), elements, stride, modules,
          sampled);

      if (valid < kMinConsoleObjects || valid * 2 < sampled)
        continue;
      if (found && valid <= hit.valid)
        continue;
      hit.manager = candidatePtr;
      hit.mapOff = off;
      hit.entries = elements;
      hit.valid = valid;
      hit.sampled = sampled;
      hit.stride = stride;
      found = true;
    }
  }
  return found;
}

struct AddressRange {
  uintptr_t start;
  uintptr_t end;
};

void SweepWritableRangeForConsoleManager(
    uintptr_t start, uintptr_t end,
    const std::vector<PatternScan::ModuleInfo> &modules, std::mutex &hitsMutex,
    std::atomic<bool> &stopRequested, std::vector<ConsoleManagerHit> &hits) {
  std::vector<ConsoleManagerHit> local;
  constexpr uintptr_t kPieceBytes = 64 * 1024;
  for (uintptr_t piece = start; piece < end;) {
    if (stopRequested.load(std::memory_order_relaxed))
      break;
    const uintptr_t pieceEnd = (std::min)(end, piece + kPieceBytes);
    if (IsMapped(reinterpret_cast<void *>(piece), pieceEnd - piece)) {
      for (uintptr_t addr = piece; addr + sizeof(void *) <= pieceEnd; addr += sizeof(void *)) {
        if ((addr & 0xFFF) == 0 &&
            stopRequested.load(std::memory_order_relaxed))
          break;
        void *candidatePtr = nullptr;
        memcpy(&candidatePtr, reinterpret_cast<const void *>(addr),
               sizeof(candidatePtr));
        const uintptr_t candidate = (uintptr_t)candidatePtr;
        if (candidate < 0x10000 || candidate >= 0x7FFFFFFFFFFFULL)
          continue;
        ConsoleManagerHit hit;
        if (DescribeConsoleManager(candidatePtr, modules, hit)) {
          hit.location = addr;

          if (hit.valid == hit.sampled && hit.sampled >= 32 &&
              hit.entries >= 256)
            stopRequested.store(true, std::memory_order_relaxed);
          local.push_back(hit);
        }
      }
    }
    piece = pieceEnd;
  }
  if (!local.empty()) {
    std::lock_guard<std::mutex> guard(hitsMutex);
    hits.insert(hits.end(), local.begin(), local.end());
  }
}

}

bool FindConsoleManagerHeuristic(EngineState &state) {
  const PatternScan::ModuleInfo mainMod = PatternScan::GetMainModule();
  if (!mainMod.base || !mainMod.size)
    return false;

  const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(mainMod.base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    return false;
  const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(
      mainMod.base + static_cast<DWORD>(dos->e_lfanew));
  if (nt->Signature != IMAGE_NT_SIGNATURE)
    return false;

  std::vector<AddressRange> pieces;
  const IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(nt);
  for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
    if (!section->Misc.VirtualSize || !section->VirtualAddress)
      continue;
    if (!(section->Characteristics & IMAGE_SCN_MEM_WRITE) ||
        (section->Characteristics & IMAGE_SCN_MEM_EXECUTE))
      continue;
    uintptr_t sectionStart = mainMod.base + section->VirtualAddress;
    uintptr_t sectionEnd = sectionStart + section->Misc.VirtualSize;
    sectionStart = (sectionStart + sizeof(void *) - 1) & ~(uintptr_t)(sizeof(void *) - 1);
    sectionEnd &= ~(uintptr_t)(sizeof(void *) - 1);
    constexpr uintptr_t kPieceBytes = 64 * 1024;
    for (uintptr_t piece = sectionStart; piece < sectionEnd; piece += kPieceBytes)
      pieces.push_back({piece, (std::min)(sectionEnd, piece + kPieceBytes)});
  }
  if (pieces.empty())
    return false;

  const auto modules = PatternScan::GetLoadedModules();
  std::vector<ConsoleManagerHit> hits;
  std::mutex hitsMutex;
  std::atomic<bool> stopRequested{false};

  const unsigned workerCount = (std::min)(
      (std::max)(1u, std::thread::hardware_concurrency()),
      static_cast<unsigned>(pieces.size()));
  std::atomic<size_t> nextPiece{0};
  std::vector<std::thread> workers;
  workers.reserve(workerCount);
  for (unsigned w = 0; w < workerCount; ++w) {
    workers.emplace_back([&]() {
      FastMemScope fastWorker;
      for (;;) {
        const size_t index = nextPiece.fetch_add(1, std::memory_order_relaxed);
        if (index >= pieces.size())
          break;
        SweepWritableRangeForConsoleManager(pieces[index].start,
                                            pieces[index].end, modules,
                                            hitsMutex, stopRequested, hits);
      }
    });
  }
  for (auto &worker : workers) {
    if (worker.joinable())
      worker.join();
  }

  if (hits.empty())
    return false;

  if (stopRequested.load(std::memory_order_relaxed))
    Log("[Discovery] IConsoleManager sweep stopped at a conclusive hit");

  const ConsoleManagerHit *best = &hits.front();
  for (const auto &hit : hits) {
    if (hit.valid > best->valid ||
        (hit.valid == best->valid &&
         (hit.location < best->location ||
          (hit.location == best->location &&
           reinterpret_cast<uintptr_t>(hit.manager) <
               reinterpret_cast<uintptr_t>(best->manager)))))
      best = &hit;
  }

  state.IConsoleManager = best->manager;
  state.IConsoleManagerLocation = reinterpret_cast<void *>(best->location);
  state.IConsoleManager_TMapOff = best->mapOff;
  const int percent =
      (best->sampled > 0) ? (best->valid * 100 / best->sampled) : 0;
  Log("[Discovery] Found IConsoleManager via BSS scan: %p "
      "(global %p, TMap off=0x%X, commands=%d, verified=%d/%d (%d%%), stride=0x%X)",
      best->manager, reinterpret_cast<void *>(best->location), best->mapOff,
      best->entries, best->valid, best->sampled, percent, best->stride);
  return true;
}

bool FindConsoleManager(EngineState &state) {
  if (state.IConsoleManager)
    return true;

  FastMemScope fastMem;
  if (FindConsoleManagerHeuristic(state)) {
    return true;
  }

  WriteLogLine("[Discovery] Failed to find IConsoleManager.");
  return false;
}

bool FindEngineFunctions(EngineState &state, bool skipConsoleManager) {

  if (!state.GetFullName) {
    FindGetFullName(state);
  }

  if (!skipConsoleManager) {
    if (!state.IConsoleManager) {
      FindConsoleManager(state);
    }
  } else {

    state.IConsoleManager = nullptr;
    state.IConsoleManagerLocation = nullptr;
    Log("[Discovery] Skipping IConsoleManager discovery (command dump and Lua "
        "are disabled)");
  }
  state.bFunctionsFound = (state.FNameToString != nullptr);
  return state.bFunctionsFound;
}

bool FindCoreClasses(EngineState &state) {
  SafeMem_SetFastMode(1);

  int32_t count = GetObjectCount(state);
  if (count < 10) {
    SafeMem_SetFastMode(0);
    return false;
  }

  static const char* const kTargetNames[] = {
    "Class", "Object", "Actor", "Pawn", "Enum",
    "Function", "Package", "Struct", "ScriptStruct", "UClass", "World"
  };
  constexpr int kNumTargets = 11;
  int32_t resolvedIndices[kNumTargets];
  for (int i = 0; i < kNumTargets; i++) resolvedIndices[i] = -1;

  const bool caseSensitiveNames = state.bUseCasePreservingName;
  auto nameMatches = [caseSensitiveNames](const char *poolText, const char *wanted) {
    return caseSensitiveNames ? strcmp(poolText, wanted) == 0
                              : _stricmp(poolText, wanted) == 0;
  };

  {
    const size_t sz = state.FNameCacheArray.size();
    int remaining = kNumTargets;

    const size_t fastLimit = std::min(sz, static_cast<size_t>(16384));
    for (size_t i = 0; i < fastLimit && remaining > 0; i++) {
      const char *p = state.FNameCacheArray[i];
      if (!p) continue;
      for (int k = 0; k < kNumTargets; k++) {
        if (resolvedIndices[k] < 0 && nameMatches(p, kTargetNames[k])) {
          resolvedIndices[k] = static_cast<int32_t>(i);
          remaining--;
          break;
        }
      }
    }

    for (size_t i = fastLimit; i < sz && remaining > 0; i++) {
      const char *p = state.FNameCacheArray[i];
      if (!p) continue;
      for (int k = 0; k < kNumTargets; k++) {
        if (resolvedIndices[k] < 0 && nameMatches(p, kTargetNames[k])) {
          resolvedIndices[k] = static_cast<int32_t>(i);
          remaining--;
          break;
        }
      }
    }
  }

  UObject *resolvedObjs[kNumTargets] = {};
  {
    int remainingObjs = 0;
    for (int k = 0; k < kNumTargets; k++) {
      if (resolvedIndices[k] >= 0) remainingObjs++;
    }

    void *objectsPtr = SafeMem_ReadPtr((uint8_t *)state.GUObjectArray + state.TUObjectArray_ObjectsOff);
    if (objectsPtr && state.UObject_NameOff >= 0) {
      int32_t itemSize = state.UObjectArray_ItemSize > 0 ? state.UObjectArray_ItemSize : 24;
      int32_t objOff = state.FUObjectItem_ObjectOff >= 0 ? state.FUObjectItem_ObjectOff : 0;
      int32_t scanLimit = std::min(count, 8192);

      void *chunk0 = SafeMem_ReadPtr(objectsPtr);
      if (chunk0) {
        uint8_t *chunkBytes = (uint8_t *)chunk0;
        for (int i = 0; i < scanLimit && remainingObjs > 0; i++) {
          UObject *obj = *(UObject **)(chunkBytes + i * itemSize + objOff);
          if (!obj || (uintptr_t)obj < 0x10000 || (uintptr_t)obj >= 0x800000000000ULL) continue;

          int32_t nameIdx = ((const FName *)((const uint8_t *)obj + state.UObject_NameOff))->ComparisonIndex;
          for (int k = 0; k < kNumTargets; k++) {
            if (resolvedIndices[k] >= 0 && resolvedIndices[k] == nameIdx && !resolvedObjs[k]) {
              resolvedObjs[k] = obj;
              remainingObjs--;
            }
          }
        }
      }
    }
  }

  int32_t idx_Class = resolvedIndices[0];
  int32_t idx_UClass = resolvedIndices[9];

  auto findByIndex = [&](int32_t slot) -> UObject * {
    if (slot < 0 || slot >= kNumTargets) return nullptr;
    return resolvedObjs[slot];
  };

  UClass *classMetaClass =
      state.UClassClass;

  if (!classMetaClass) {
    UObject *classObj = findByIndex(0);
    if (!classObj) classObj = findByIndex(9);

    if (classObj) {
      UClass *cls = GetObjectClass(state, classObj);
      if (cls == classObj) {
        classMetaClass = (UClass *)classObj;
        state.UClassClass = classMetaClass;
        Log("[Discovery] Found 'Class' metaclass: %p", classObj);
      }
    }
  }

  if (!classMetaClass) {
    Log("[Discovery] Could not find 'Class' metaclass");
    return false;
  }

  struct ClassSearch {
    int slot;
    const char *name;
    UClass **target;
  };
  ClassSearch searches[] = {
      {1,  "Object",      &state.UObjectClass},
      {2,  "Actor",       &state.AActorClass},
      {3,  "Pawn",        &state.APawnClass},
      {4,  "Enum",        &state.UEnumClass},
      {5,  "Function",    &state.UFunctionClass},
      {6,  "Package",     &state.UPackageClass},
      {7,  "Struct",      &state.UStructClass},
      {8,  "ScriptStruct",&state.UStructClass},
      {10, "World",       &state.UWorldClass},
  };

  int found = 1;
  for (auto &search : searches) {
    if (*search.target != nullptr)
      continue;

    UObject *obj = findByIndex(search.slot);
    if (!obj)
      continue;

    UClass *cls = GetObjectClass(state, obj);
    if (cls == classMetaClass) {
      *search.target = (UClass *)obj;
      Log("[Discovery] Found core class '%s': %p", search.name, obj);
      found++;
    }
  }

  state.bCoreClassesFound = (found >= 3);
  SafeMem_SetFastMode(0);
  Log("[Discovery] Resolved %d core classes", found);
  return state.bCoreClassesFound;
}

static bool HasPatternInRange(const uint8_t *start, size_t size,
                              const uint8_t *pattern, const int8_t *mask,
                              size_t patLen) {
  if (size < patLen)
    return false;
  for (size_t i = 0; i <= size - patLen; i++) {
    bool found = true;
    for (size_t j = 0; j < patLen; j++) {
      if (mask[j] >= 0 && start[i + j] != pattern[j]) {
        found = false;
        break;
      }
    }
    if (found)
      return true;
  }
  return false;
}

struct InheritanceKey {
  UStruct *Type;
  UStruct *Want;

  int SuperOff;
  bool operator==(const InheritanceKey &other) const {
    return Type == other.Type && Want == other.Want && SuperOff == other.SuperOff;
  }
};

struct InheritanceKeyHash {
  size_t operator()(const InheritanceKey &k) const {
    return std::hash<void*>()(k.Type) ^ (std::hash<void*>()(k.Want) << 1) ^
           (std::hash<int>()(k.SuperOff) << 7);
  }
};

static std::unordered_map<InheritanceKey, bool, InheritanceKeyHash> s_InheritanceCache;
static std::mutex s_InheritanceMutex;

void ClearInheritanceCache() {
  std::lock_guard<std::mutex> lock(s_InheritanceMutex);
  s_InheritanceCache.clear();
}

bool IsStructA(const EngineState &state, UStruct *type, UStruct *want) {
  if (!type || !want)
    return false;
  if (type == want)
    return true;

  const int superOff = state.UStruct_SuperOff >= 0 ? state.UStruct_SuperOff : 0x40;

  InheritanceKey key = {type, want, superOff};
  {
    std::lock_guard<std::mutex> lock(s_InheritanceMutex);
    auto it = s_InheritanceCache.find(key);
    if (it != s_InheritanceCache.end()) {
      return it->second;
    }
  }

  UStruct *curr = type;
  bool result = false;

  for (int depth = 0; depth < 32 && curr; depth++) {
    if (curr == want) {
      result = true;
      break;
    }
    const uintptr_t currAddr = reinterpret_cast<uintptr_t>(curr);
    if (currAddr < 0x10000 || currAddr >= 0x7FFFFFFFFFFFULL) {
      break;
    }
    UStruct *next = nullptr;
    if (!SafeMem_Read((uint8_t *)curr + superOff, &next, sizeof(next)))
      break;
    curr = next;
  }

  {
    std::lock_guard<std::mutex> lock(s_InheritanceMutex);
    s_InheritanceCache[key] = result;
  }
  return result;
}

static void *ReadClassVTableSlot(UObject *instance, int slotIdx) {
  if (!instance || slotIdx < 0)
    return nullptr;
  void **vft = nullptr;
  if (!SafeMem_Read(instance, &vft, sizeof(vft)) || !vft)
    return nullptr;
  if ((uintptr_t)vft < 0x10000)
    return nullptr;
  void *fn = nullptr;
  if (!SafeMem_Read(&vft[slotIdx], &fn, sizeof(fn)))
    return nullptr;
  return fn;
}

bool ResolveProcessEventImplementations(EngineState &state, int slotIdx,
                                        void **uobjectImpl, void **aactorImpl,
                                        bool *distinct) {
  if (uobjectImpl) *uobjectImpl = nullptr;
  if (aactorImpl) *aactorImpl = nullptr;
  if (distinct) *distinct = false;
  if (slotIdx < 0 || !state.UObjectClass || !state.AActorClass)
    return false;

  UObject *uobjectDefault = FindClassDefaultObject(state, state.UObjectClass);
  UObject *aactorDefault = FindClassDefaultObject(state, state.AActorClass);
  if (!uobjectDefault || !aactorDefault)
    return false;

  void *uobjectFn = ReadClassVTableSlot(uobjectDefault, slotIdx);
  void *aactorFn = ReadClassVTableSlot(aactorDefault, slotIdx);
  if (!uobjectFn || !aactorFn)
    return false;

  if (uobjectImpl) *uobjectImpl = uobjectFn;
  if (aactorImpl) *aactorImpl = aactorFn;
  if (distinct) *distinct = (uobjectFn != aactorFn);
  return true;
}

int FindDirectBranchTo(void *from, void *to, int window) {
  if (!from || !to || from == to)
    return -1;

  uint8_t code[0x800];
  if (window <= 5 || window > (int)sizeof(code))
    window = (int)sizeof(code);
  if (!SafeMem_Read(from, code, (size_t)window))
    return -1;

  const uintptr_t base = reinterpret_cast<uintptr_t>(from);
  const uintptr_t want = reinterpret_cast<uintptr_t>(to);
  for (int i = 0; i + 5 <= window; ++i) {
    if (code[i] != 0xE8 && code[i] != 0xE9)
      continue;
    int32_t rel = 0;
    memcpy(&rel, code + i + 1, sizeof(rel));
    if (base + static_cast<uintptr_t>(i) + 5 + static_cast<intptr_t>(rel) == want)
      return i;
  }
  return -1;
}

int FindFuncNativeFlagsOffset(void *fn, int window) {
  if (!fn)
    return -1;

  uint8_t code[0x800];
  if (window <= 0 || window > (int)sizeof(code))
    window = (int)sizeof(code);
  if (!SafeMem_Read(fn, code, (size_t)window))
    return -1;

  constexpr uint32_t kNativeByte = (kFuncNative >> 8) & 0xFF;

  auto decodeMemoryOperand = [&](size_t modrmIndex, int32_t &displacement,
                                 size_t &immediateIndex) -> bool {
    if (modrmIndex + 1 > static_cast<size_t>(window))
      return false;
    const uint8_t modrm = code[modrmIndex];
    const uint8_t mod = static_cast<uint8_t>(modrm >> 6);
    const uint8_t rm = static_cast<uint8_t>(modrm & 0x07);
    if (mod == 3)
      return false;
    size_t cursor = modrmIndex + 1;
    displacement = 0;
    if (rm == 4) {
      if (cursor >= static_cast<size_t>(window))
        return false;
      const uint8_t sib = code[cursor++];
      if (mod == 0 && (sib & 0x07) == 5) {
        if (cursor + 4 > static_cast<size_t>(window))
          return false;
        memcpy(&displacement, code + cursor, 4);
        cursor += 4;
      }
    } else if (mod == 0 && rm == 5) {
      if (cursor + 4 > static_cast<size_t>(window))
        return false;
      memcpy(&displacement, code + cursor, 4);
      cursor += 4;
    }
    if (mod == 1) {
      if (cursor + 1 > static_cast<size_t>(window))
        return false;
      displacement = static_cast<int8_t>(code[cursor]);
      cursor += 1;
    } else if (mod == 2) {
      if (cursor + 4 > static_cast<size_t>(window))
        return false;
      memcpy(&displacement, code + cursor, 4);
      cursor += 4;
    }
    immediateIndex = cursor;
    return true;
  };

  const auto inFlagsRange = [](int32_t value) {
    return value >= 0x40 && value < 0x400;
  };

  auto findRegisterLoad = [&](size_t testIndex, int testedReg) -> int {
    const size_t limit = testIndex >= 24 ? testIndex - 24 : 0;
    for (size_t k = testIndex; k-- > limit;) {
      const uint8_t opcode = code[k];
      if (opcode != 0x8B && opcode != 0x8D)
        continue;
      if (k + 2 > static_cast<size_t>(window))
        continue;
      const uint8_t rex = (k > 0 && (code[k - 1] & 0xF0) == 0x40) ? code[k - 1] : 0;
      const uint8_t modrm = code[k + 1];
      const int sourceReg =
          ((modrm >> 3) & 0x07) | ((rex & 0x04) ? 8 : 0);
      if (sourceReg != testedReg)
        continue;
      int32_t displacement = 0;
      size_t immediateIndex = 0;
      if (!decodeMemoryOperand(k + 1, displacement, immediateIndex))
        continue;
      if (inFlagsRange(displacement))
        return displacement;
    }
    return -1;
  };

  for (int i = 0; i + 2 <= window; ++i) {
    if (code[i] == 0xF7) {
      const uint8_t modrm = code[i + 1];
      if ((modrm & 0x38) != 0)
        continue;
      if ((modrm >> 6) == 3) {
        uint32_t imm = 0;
        memcpy(&imm, code + i + 2, sizeof(imm));
        if (imm != kFuncNative)
          continue;
        const uint8_t rex = (i > 0 && (code[i - 1] & 0xF0) == 0x40) ? code[i - 1] : 0;
        const int testedReg = (modrm & 0x07) | ((rex & 0x01) ? 8 : 0);
        const int displacement = findRegisterLoad(static_cast<size_t>(i), testedReg);
        if (displacement >= 0)
          return displacement;
        continue;
      }
      int32_t displacement = 0;
      size_t immediateIndex = 0;
      if (!decodeMemoryOperand(static_cast<size_t>(i) + 1, displacement,
                               immediateIndex))
        continue;
      if (immediateIndex + 4 > static_cast<size_t>(window))
        continue;
      uint32_t imm = 0;
      memcpy(&imm, code + immediateIndex, sizeof(imm));
      if (imm != kFuncNative || !inFlagsRange(displacement))
        continue;
      return displacement;
    }
    if (code[i] == 0xF6) {
      const uint8_t modrm = code[i + 1];
      if ((modrm & 0x38) != 0)
        continue;
      int32_t displacement = 0;
      size_t immediateIndex = 0;
      if (!decodeMemoryOperand(static_cast<size_t>(i) + 1, displacement,
                               immediateIndex))
        continue;
      if (immediateIndex >= static_cast<size_t>(window))
        continue;
      if (code[immediateIndex] != kNativeByte)
        continue;
      const int32_t flagsOffset = displacement - 1;
      if (!inFlagsRange(flagsOffset))
        continue;
      return flagsOffset;
    }
  }
  return -1;
}

static void CountImplementationCarriers(EngineState &state, ProcessEventSlotLaw *law) {
  if (!law)
    return;
  const int total = GetObjectCount(state);
  constexpr int kSamples = 200;
  const int64_t stride = (total > kSamples) ? (total / kSamples) : 1;

  for (int64_t i = 0; i < total; i += stride) {
    UObject *obj = GetObjectByIndex(state, (int32_t)i);
    if (!obj)
      continue;
    void **vft = nullptr;
    if (!SafeMem_Read(obj, &vft, sizeof(vft)) || !vft)
      continue;
    if (reinterpret_cast<uintptr_t>(vft) < 0x10000)
      continue;
    void *fn = nullptr;
    if (!SafeMem_Read(&vft[law->slot], &fn, sizeof(fn)) || !fn)
      continue;

    UClass *cls = GetObjectClass(state, obj);
    const bool isActor = cls && state.AActorClass &&
                         IsStructA(state, (UStruct *)cls, (UStruct *)state.AActorClass);
    if (isActor) {
      ++law->actorReads;
      if (fn == law->aactorImpl)
        ++law->actorAgree;
    } else {
      ++law->objectReads;
      if (fn == law->uobjectImpl)
        ++law->objectAgree;
    }
  }
}

bool FindProcessEventSlotByEngineLaw(EngineState &state, int slotLo, int slotHi,
                                     ProcessEventSlotLaw *out) {
  if (out)
    *out = ProcessEventSlotLaw{};
  if (!out || !state.UObjectClass || !state.AActorClass)
    return false;
  if (slotLo < 0)
    slotLo = 0;
  if (slotHi < slotLo || slotHi > 0x400)
    return false;

  UObject *uobjectDefault = FindClassDefaultObject(state, state.UObjectClass);
  UObject *aactorDefault = FindClassDefaultObject(state, state.AActorClass);
  if (!uobjectDefault || !aactorDefault)
    return false;
  void **uobjectVft = nullptr;
  void **aactorVft = nullptr;
  if (!SafeMem_Read(uobjectDefault, &uobjectVft, sizeof(uobjectVft)) || !uobjectVft)
    return false;
  if (!SafeMem_Read(aactorDefault, &aactorVft, sizeof(aactorVft)) || !aactorVft)
    return false;

  ProcessEventSlotLaw best;
  int bestAgreement = -1;
  int lawSlots = 0;

  for (int slot = slotLo; slot <= slotHi; ++slot) {
    void *uobjectImpl = nullptr;
    void *aactorImpl = nullptr;
    if (!SafeMem_Read(&uobjectVft[slot], &uobjectImpl, sizeof(uobjectImpl)) || !uobjectImpl)
      continue;
    if (!SafeMem_Read(&aactorVft[slot], &aactorImpl, sizeof(aactorImpl)) || !aactorImpl)
      continue;
    if (uobjectImpl == aactorImpl)
      continue;

    const int branchOffset = FindDirectBranchTo(aactorImpl, uobjectImpl);
    if (branchOffset < 0)
      continue;

    const int uobjectFlags = FindFuncNativeFlagsOffset(uobjectImpl);
    if (uobjectFlags < 0)
      continue;
    if (FindFuncNativeFlagsOffset(aactorImpl) != uobjectFlags)
      continue;

    ++lawSlots;
    ProcessEventSlotLaw candidate;
    candidate.slot = slot;
    candidate.uobjectImpl = uobjectImpl;
    candidate.aactorImpl = aactorImpl;
    candidate.overrideBranchOffset = branchOffset;
    candidate.flagsOffset = uobjectFlags;
    CountImplementationCarriers(state, &candidate);

    const int agreement = candidate.objectAgree + candidate.actorAgree;
    if (agreement > bestAgreement) {
      bestAgreement = agreement;
      best = candidate;
    }
  }

  if (best.slot < 0) {
    Log("[Discovery] ProcessEvent: no slot in [%d, %d] carries the two implementations "
        "Object.h declares and Actor.h overrides (the override branching into the "
        "implementation, both testing FUNC_Native at the offset Script.h declares)",
        slotLo, slotHi);
    return false;
  }

  if (lawSlots > 1) {
    Log("[Discovery] ProcessEvent: %d slots in [%d, %d] satisfy the engine's laws; "
        "taking the one the object array carries most (slot %d)",
        lawSlots, slotLo, slotHi, best.slot);
  }

  *out = best;
  return true;
}

static int ScoreProcessEventFlagPatterns(const uint8_t *start, size_t size,
                                         int funcFlagsOff) {
  if (!start || size < 16 || funcFlagsOff < 0)
    return 0;

  int flagsOffLo = funcFlagsOff;
  int flagsOffHi = funcFlagsOff + 1;
  int flagsOffMid = funcFlagsOff + 2;
  int flagsOffTop = funcFlagsOff + 3;

  uint8_t nativeDwordPat[] = {0xF7,
                              0x00,
                              (uint8_t)(flagsOffLo & 0xFF),
                              (uint8_t)((flagsOffLo >> 8) & 0xFF),
                              (uint8_t)((flagsOffLo >> 16) & 0xFF),
                              (uint8_t)((flagsOffLo >> 24) & 0xFF),
                              0x00,
                              0x04,
                              0x00,
                              0x00};
  int8_t nativeDwordMask[] = {0, -1, 0, 0, 0, 0, 0, 0, 0, 0};

  uint8_t netDwordPat[] = {0xF7,
                           0x00,
                           (uint8_t)(flagsOffLo & 0xFF),
                           (uint8_t)((flagsOffLo >> 8) & 0xFF),
                           (uint8_t)((flagsOffLo >> 16) & 0xFF),
                           (uint8_t)((flagsOffLo >> 24) & 0xFF),
                           0x40,
                           0x00,
                           0x00,
                           0x00};
  int8_t netDwordMask[] = {0, -1, 0, 0, 0, 0, 0, 0, 0, 0};

  uint8_t eventDwordPat[] = {0xF7,
                             0x00,
                             (uint8_t)(flagsOffLo & 0xFF),
                             (uint8_t)((flagsOffLo >> 8) & 0xFF),
                             (uint8_t)((flagsOffLo >> 16) & 0xFF),
                             (uint8_t)((flagsOffLo >> 24) & 0xFF),
                             0x00,
                             0x08,
                             0x00,
                             0x00};
  int8_t eventDwordMask[] = {0, -1, 0, 0, 0, 0, 0, 0, 0, 0};

  uint8_t nativeBytePat[] = {0xF6,
                             0x00,
                             (uint8_t)(flagsOffHi & 0xFF),
                             (uint8_t)((flagsOffHi >> 8) & 0xFF),
                             (uint8_t)((flagsOffHi >> 16) & 0xFF),
                             (uint8_t)((flagsOffHi >> 24) & 0xFF),
                             0x04};
  int8_t nativeByteMask[] = {0, -1, 0, 0, 0, 0, 0};

  uint8_t netBytePat[] = {0xF6,
                          0x00,
                          (uint8_t)(flagsOffLo & 0xFF),
                          (uint8_t)((flagsOffLo >> 8) & 0xFF),
                          (uint8_t)((flagsOffLo >> 16) & 0xFF),
                          (uint8_t)((flagsOffLo >> 24) & 0xFF),
                          0x40};
  int8_t netByteMask[] = {0, -1, 0, 0, 0, 0, 0};

  uint8_t eventBytePat[] = {0xF6,
                            0x00,
                            (uint8_t)(flagsOffHi & 0xFF),
                            (uint8_t)((flagsOffHi >> 8) & 0xFF),
                            (uint8_t)((flagsOffHi >> 16) & 0xFF),
                            (uint8_t)((flagsOffHi >> 24) & 0xFF),
                            0x08};
  int8_t eventByteMask[] = {0, -1, 0, 0, 0, 0, 0};

  uint8_t uberGraphBytePat[] = {0xF6,
                                0x00,
                                (uint8_t)(flagsOffHi & 0xFF),
                                (uint8_t)((flagsOffHi >> 8) & 0xFF),
                                (uint8_t)((flagsOffHi >> 16) & 0xFF),
                                (uint8_t)((flagsOffHi >> 24) & 0xFF),
                                0x80};
  int8_t uberGraphByteMask[] = {0, -1, 0, 0, 0, 0, 0};

  uint8_t hasOutParmsBytePat[] = {0xF6,
                                  0x00,
                                  (uint8_t)(flagsOffMid & 0xFF),
                                  (uint8_t)((flagsOffMid >> 8) & 0xFF),
                                  (uint8_t)((flagsOffMid >> 16) & 0xFF),
                                  (uint8_t)((flagsOffMid >> 24) & 0xFF),
                                  0x40};
  int8_t hasOutParmsByteMask[] = {0, -1, 0, 0, 0, 0, 0};

  uint8_t blueprintCallableBytePat[] = {0xF6,
                                        0x00,
                                        (uint8_t)(flagsOffTop & 0xFF),
                                        (uint8_t)((flagsOffTop >> 8) & 0xFF),
                                        (uint8_t)((flagsOffTop >> 16) & 0xFF),
                                        (uint8_t)((flagsOffTop >> 24) & 0xFF),
                                        0x04};
  int8_t blueprintCallableByteMask[] = {0, -1, 0, 0, 0, 0, 0};

  uint8_t blueprintEventBytePat[] = {0xF6,
                                     0x00,
                                     (uint8_t)(flagsOffTop & 0xFF),
                                     (uint8_t)((flagsOffTop >> 8) & 0xFF),
                                     (uint8_t)((flagsOffTop >> 16) & 0xFF),
                                     (uint8_t)((flagsOffTop >> 24) & 0xFF),
                                     0x08};
  int8_t blueprintEventByteMask[] = {0, -1, 0, 0, 0, 0, 0};

  uint8_t btNativePat[] = {0x0F,
                           0xBA,
                           0x00,
                           (uint8_t)(flagsOffLo & 0xFF),
                           (uint8_t)((flagsOffLo >> 8) & 0xFF),
                           (uint8_t)((flagsOffLo >> 16) & 0xFF),
                           (uint8_t)((flagsOffLo >> 24) & 0xFF),
                           0x0A};
  int8_t btNativeMask[] = {0, 0, -1, 0, 0, 0, 0, 0};

  int score = 0;
  if (HasPatternInRange(start, size, nativeDwordPat, nativeDwordMask,
                        sizeof(nativeDwordPat)))
    score += 15;
  if (HasPatternInRange(start, size, netDwordPat, netDwordMask,
                        sizeof(netDwordPat)))
    score += 8;
  if (HasPatternInRange(start, size, eventDwordPat, eventDwordMask,
                        sizeof(eventDwordPat)))
    score += 8;
  if (HasPatternInRange(start, size, nativeBytePat, nativeByteMask,
                        sizeof(nativeBytePat)))
    score += 15;
  if (HasPatternInRange(start, size, netBytePat, netByteMask,
                        sizeof(netBytePat)))
    score += 8;
  if (HasPatternInRange(start, size, eventBytePat, eventByteMask,
                        sizeof(eventBytePat)))
    score += 8;
  if (HasPatternInRange(start, size, uberGraphBytePat, uberGraphByteMask,
                        sizeof(uberGraphBytePat)))
    score += 8;
  if (HasPatternInRange(start, size, hasOutParmsBytePat, hasOutParmsByteMask,
                        sizeof(hasOutParmsBytePat)))
    score += 8;
  if (HasPatternInRange(start, size, blueprintCallableBytePat,
                        blueprintCallableByteMask,
                        sizeof(blueprintCallableBytePat)))
    score += 6;
  if (HasPatternInRange(start, size, blueprintEventBytePat,
                        blueprintEventByteMask, sizeof(blueprintEventBytePat)))
    score += 6;
  if (HasPatternInRange(start, size, btNativePat, btNativeMask,
                        sizeof(btNativePat)))
    score += 15;
  return score;
}

static int CountFlagOffsetReferences(const uint8_t *start, size_t size,
                                     int funcFlagsOff) {
  if (!start || size < 8 || funcFlagsOff < 0)
    return 0;

  int refCount = 0;
  for (size_t i = 0; i + 6 < size; i++) {
    uint8_t op = start[i];
    if (op != 0xF6 && op != 0xF7 && op != 0x80 && op != 0x81 && op != 0x83 &&
        op != 0x8A && op != 0x8B && op != 0x0F)
      continue;

    if (op == 0x0F && i + 7 < size) {
      uint8_t op2 = start[i + 1];
      if (op2 == 0xBA || op2 == 0xA3) {
        uint8_t modrm = start[i + 2];
        uint8_t mod = modrm >> 6;
        if (mod == 0x2) {
          uint32_t disp = start[i + 3] | (start[i + 4] << 8) |
                          (start[i + 5] << 16) | (start[i + 6] << 24);
          if (disp >= (uint32_t)funcFlagsOff &&
              disp <= (uint32_t)(funcFlagsOff + 3)) {
            refCount++;
            i += 7;
            continue;
          }
        }
      }
    }

    uint8_t modrm = start[i + 1];
    uint8_t mod = modrm >> 6;
    if (mod != 0x2)
      continue;

    uint32_t disp = start[i + 2] | (start[i + 3] << 8) | (start[i + 4] << 16) |
                    (start[i + 5] << 24);

    if (disp >= (uint32_t)funcFlagsOff &&
        disp <= (uint32_t)(funcFlagsOff + 3)) {
      refCount++;
      i += (op == 0xF7 || op == 0x81) ? 9 : 6;
    }
  }

  return refCount;
}

static bool TryReadRepresentativeVTable(const EngineState &state,
                                        const std::vector<ExecutableRange> &ranges,
                                        void **vtEntries, int maxEntries,
                                        UObject **outObject = nullptr) {
  if (!vtEntries || maxEntries <= 0)
    return false;

  const size_t vtBytes = sizeof(void *) * (size_t)maxEntries;

  auto validateVft = [&](void *obj) -> bool {
    if (!obj || !IsReadable(obj, sizeof(void *)))
      return false;
    void **vft = nullptr;
    if (!SafeMem_Read(obj, &vft, sizeof(vft)) || !vft)
      return false;
    if (!IsReadable(vft, vtBytes) || !SafeMem_Read(vft, vtEntries, vtBytes))
      return false;

    int validEntries = 0;
    for (int j = 0; j < std::min(maxEntries, 16); j++) {
      uintptr_t fa = reinterpret_cast<uintptr_t>(vtEntries[j]);
      if (IsExecutableAddress(ranges, fa))
        validEntries++;
    }
    if (validEntries >= 4) {
      if (outObject)
        *outObject = reinterpret_cast<UObject *>(obj);
      return true;
    }
    return false;
  };

  if (state.GWorld && validateVft(state.GWorld))
    return true;
  if (state.GEngine && validateVft(state.GEngine))
    return true;

  int32_t count = GetObjectCount(state);
  int32_t limit = std::min(count, 2048);
  for (int i = 0; i < limit; i++) {
    UObject *obj = GetObjectByIndex(state, i);
    if (!obj)
      continue;
    UClass *cls = GetObjectClass(state, obj);
    if (!cls)
      continue;
    if (cls == reinterpret_cast<UClass *>(state.UClassClass) ||
        cls == reinterpret_cast<UClass *>(state.UFunctionClass) ||
        cls == reinterpret_cast<UClass *>(state.UStructClass) ||
        cls == reinterpret_cast<UClass *>(state.UPackageClass) ||
        cls == reinterpret_cast<UClass *>(state.UEnumClass))
      continue;

    if (validateVft(obj))
      return true;
  }

  UObject *fallbackObjs[] = {
      reinterpret_cast<UObject *>(state.AActorClass),
      reinterpret_cast<UObject *>(state.APawnClass),
      reinterpret_cast<UObject *>(state.UObjectClass),
  };
  for (UObject *obj : fallbackObjs) {
    if (validateVft(obj))
      return true;
  }

  return false;
}

bool FindProcessEvent(EngineState &state) {
  if (state.ProcessEvent_VTableIdx >= 0 && state.UFunction_FlagsOff >= 0) {
    Log("[Discovery] ProcessEvent restored from cache: FunctionFlags=0x%X, VTable[%d]",
        state.UFunction_FlagsOff, state.ProcessEvent_VTableIdx);
    return true;
  }

  if (state.UStruct_ChildrenOff < 0 || !state.AActorClass ||
      !state.UFunctionClass) {
    Log("[Discovery] Cannot find ProcessEvent: missing Children, Actor class, "
        "or Function class");
    return false;
  }

  constexpr int kMaxFunctions = 24;
  void *functions[kMaxFunctions] = {};
  int funcCount = 0;
  int childSampleCount = 0;
  int objectArraySampleCount = 0;
  int objectArrayFunctionsSeen = 0;
  struct ChildScanSummary {
    const char *label;
    int walked;
    int added;
  };
  ChildScanSummary childScans[5] = {};
  int childScanCount = 0;
  std::unordered_set<void *> seenFunctions;

  auto tryAddFunctionSample = [&](void *candidate, bool fromObjectArray, bool skipClassCheck = false) {
    if (!candidate || funcCount >= kMaxFunctions)
      return false;
    if (!seenFunctions.insert(candidate).second)
      return false;

    if (!skipClassCheck) {
      UClass *candidateCls = GetObjectClass(state, (UObject *)candidate);
      bool isFunction =
          candidateCls && IsStructA(state, (UStruct *)candidateCls,
                                    (UStruct *)state.UFunctionClass);
      if (!isFunction)
        return false;
    }

    uint32_t objectFlags = 0;
    if (state.UObject_FlagsOff >= 0) {
      SafeMem_Read((uint8_t *)candidate + state.UObject_FlagsOff, &objectFlags,
                   sizeof(objectFlags));
      if (objectFlags & 0x00000010)
        return false;
    }

    std::string name = GetObjectNameStr(state, (UObject *)candidate);
    if (name.empty() || name == "None" || name[0] == '<')
      return false;

    functions[funcCount++] = candidate;
    if (fromObjectArray)
      objectArraySampleCount++;
    else
      childSampleCount++;
    return true;
  };

  auto addFunctionsFromClass = [&](UClass *cls, const char *classLabel) {
    if (!cls || funcCount >= kMaxFunctions)
      return;

    void *child = GetChildrenHead(state, (UStruct *)cls);
    int walked = 0;
    int added = 0;
    while (child && walked++ < 4096 && funcCount < kMaxFunctions) {
      void *next = GetFieldNext(state, child);
      if (tryAddFunctionSample(child, false)) {
        added++;
      }
      child = next;
    }

    if (childScanCount < (int)(sizeof(childScans) / sizeof(childScans[0]))) {
      childScans[childScanCount++] = {classLabel, walked, added};
    }
  };

  auto addFunctionsFromObjectArray = [&]() {
    if (!state.GUObjectArray) return;
    void *objectsPtr = SafeMem_ReadPtr((uint8_t *)state.GUObjectArray + state.TUObjectArray_ObjectsOff);
    if (!objectsPtr) return;

    int32_t totalNum = GetObjectCount(state);
    int32_t itemSize = state.UObjectArray_ItemSize > 0 ? state.UObjectArray_ItemSize : 24;
    int32_t objOff = state.FUObjectItem_ObjectOff >= 0 ? state.FUObjectItem_ObjectOff : 0;
    int32_t elementsPerChunk = GetElementsPerChunk(state);
    int32_t numChunks = (totalNum + elementsPerChunk - 1) / elementsPerChunk;

    for (int32_t c = 0; c < numChunks && funcCount < kMaxFunctions; c++) {
      void *chunk = SafeMem_ReadPtr((uint8_t *)objectsPtr + c * sizeof(void *));
      if (!chunk) continue;

      int32_t chunkStartIdx = c * elementsPerChunk;
      int32_t countInChunk = std::min(elementsPerChunk, totalNum - chunkStartIdx);
      uint8_t *chunkBytes = (uint8_t *)chunk;

      for (int32_t i = 0; i < countInChunk && funcCount < kMaxFunctions; i++) {
        uint8_t *itemPtr = chunkBytes + i * itemSize;
        UObject *obj = *(UObject **)(itemPtr + objOff);
        if (!obj || (uintptr_t)obj < 0x10000 || (uintptr_t)obj >= 0x800000000000ULL) continue;

        UClass *cls = GetObjectClass(state, obj);
        if (!cls || cls != (UClass*)state.UFunctionClass) continue;

        objectArrayFunctionsSeen++;
        tryAddFunctionSample(obj, true, true);
      }
    }
  };

  addFunctionsFromClass(state.AActorClass, "Actor");
  addFunctionsFromClass(state.UObjectClass, "Object");
  addFunctionsFromClass(state.APawnClass, "Pawn");
  addFunctionsFromClass(state.UStructClass, "Struct");
  addFunctionsFromClass(state.UFunctionClass, "Function");

  if (funcCount < 6) {
    addFunctionsFromObjectArray();
  }

  auto logFunctionSampling = [&]() {
    if (objectArraySampleCount == 0) {
      if (childSampleCount >= kMaxFunctions) {
        Log("[Discovery] UFunction sampling: total=%d/%d (children=%d, "
            "objectArray=0 - sample budget filled, array scan not needed)",
            funcCount, kMaxFunctions, childSampleCount);
      } else {
        Log("[Discovery] UFunction sampling: total=%d (children=%d, "
            "objectArray=0 - children satisfied requirement, array scan not needed)",
            funcCount, childSampleCount);
      }
    } else {
      Log("[Discovery] UFunction sampling: total=%d (children=%d, "
          "objectArray=%d)",
          funcCount, childSampleCount, objectArraySampleCount);
    }
  };

  if (funcCount < 3) {
    Log("[Discovery] UFunction sampling failed: total=%d (children=%d, "
        "objectArray=%d)",
        funcCount, childSampleCount, objectArraySampleCount);
    for (int i = 0; i < childScanCount; i++) {
      Log("[Discovery]   child-scan %-8s walked=%d added=%d",
          childScans[i].label, childScans[i].walked, childScans[i].added);
    }
    if (objectArrayFunctionsSeen > 0 || objectArraySampleCount > 0) {
      Log("[Discovery]   object-scan seen=%d added=%d",
          objectArrayFunctionsSeen, objectArraySampleCount);
    }
    Log("[Discovery] Too few real UFunctions found (%d), can't discover "
        "FunctionFlags",
        funcCount);
    return false;
  }

  const auto ranges = GetAllExecutableRanges();
  constexpr int kVTableEntries = 200;
  void *vtEntries[kVTableEntries] = {};
  UObject *vtableSourceObj = nullptr;
  if (!TryReadRepresentativeVTable(state, ranges, vtEntries, kVTableEntries,
                                   &vtableSourceObj)) {
    Log("[Discovery] Failed to locate a representative UObject VTable");
    return false;
  }

  std::string vtableSourceName =
      vtableSourceObj ? GetObjectNameStr(state, vtableSourceObj) : "";

  std::vector<std::string> probeLogs;
  probeLogs.reserve(40);
  std::vector<std::string> fallbackSlotLogs;
  fallbackSlotLogs.reserve(20);

  int bestFlagsOnlyOff = -1;
  int bestFlagsOnlyScore = -1;
  int bestEvidenceOff = -1;
  int bestEvidenceScore = -1;
  bool fallbackSlotRejected = false;
  int bestEvidenceVtIdx = -1;
  int bestEvidencePatternScore = 0;
  int bestEvidenceRefCount = 0;

  constexpr int kProcessEventSlotLo = 0x30;
  constexpr int kProcessEventSlotHi = 0x60;

  ProcessEventSlotLaw law;
  bool bLawFound = FindProcessEventSlotByEngineLaw(state, kProcessEventSlotLo,
                                                  kProcessEventSlotHi, &law);
  if (bLawFound) {
    Log("[Discovery] ProcessEvent slot %d: Object.h's implementation %p and Actor.h's "
        "override %p - the override branches into it at +0x%X (Actor.cpp's "
        "Super::ProcessEvent) and both test FUNC_Native (0x400, Script.h) on the "
        "Function argument's flags at +0x%X",
        law.slot, law.uobjectImpl, law.aactorImpl, law.overrideBranchOffset,
        law.flagsOffset);

    bestEvidenceOff = law.flagsOffset;
    bestEvidenceVtIdx = law.slot;
    bestEvidenceScore = 15;
    bestEvidencePatternScore = 15;
    bestEvidenceRefCount = 3;
    state.ProcessEvent_UObjectImpl = law.uobjectImpl;
    state.ProcessEvent_AActorImpl = law.aactorImpl;
  }

  struct VTableCode {
    int index;
    uint8_t bytes[0x1000];
  };
  std::vector<VTableCode> vtableCode;
  if (!bLawFound) {
    vtableCode.reserve(kVTableEntries);
    for (int i = 0; i < kVTableEntries; i++) {
      if (i < 40)
        continue;

      void *funcAddr = vtEntries[i];
      const uintptr_t funcAddrValue = reinterpret_cast<uintptr_t>(funcAddr);
      if (!funcAddr || !IsExecutableAddress(ranges, funcAddrValue))
        continue;

      VTableCode sample = {};
      sample.index = i;
      if (SafeMem_Read(funcAddr, sample.bytes, sizeof(sample.bytes)))
        vtableCode.push_back(sample);
    }
  }

  constexpr uint32_t kUnusedFunctionFlagBits = 0x00000010u | 0x00000020u;
  constexpr uint32_t kFuncAccessSpecifiers = 0x00020000u | 0x00040000u | 0x00080000u;
  constexpr int kEnumViolationPenalty = 12;

  if (!bLawFound) {
    for (int off = 0x70; off <= 0x140; off += 4) {
      int validCount = 0;
      int zeroCount = 0;
      int enumViolations = 0;
      uint32_t orAll = 0;
      uint32_t andAll = 0xFFFFFFFF;
      bool readFailed = false;
      std::unordered_set<uint32_t> distinctValues;

      for (int i = 0; i < funcCount; i++) {
        uint32_t val = 0;
        if (!SafeMem_Read((uint8_t *)functions[i] + off, &val, sizeof(val))) {
          readFailed = true;
          break;
        }

        if (val == 0) {
          zeroCount++;
        } else {
          validCount++;
          orAll |= val;
          andAll &= val;
          distinctValues.insert(val);
          if (val & kUnusedFunctionFlagBits)
            ++enumViolations;
          const uint32_t accessSpecifiers = val & kFuncAccessSpecifiers;
          if (accessSpecifiers && (accessSpecifiers & (accessSpecifiers - 1)) != 0)
            ++enumViolations;
        }
      }

      int totalOk = validCount + zeroCount;
      int flagScore = -1;
      if (!readFailed && totalOk == funcCount &&
          validCount >= std::max(2, funcCount / 3)) {
        flagScore = validCount * 3;
        if (zeroCount <= funcCount / 3)
          flagScore += 4;
        if ((int)distinctValues.size() >= 3)
          flagScore += 6;
        if (andAll & 0x20000)
          flagScore += 10;
        if (orAll & 0x400)
          flagScore += 10;
        if (orAll & 0x20000)
          flagScore += 6;
        if (orAll & 0x04000000)
          flagScore += 6;
        if (orAll & 0x800)
          flagScore += 5;
        if (orAll & 0x08000000)
          flagScore += 5;
        if (orAll & 0x4000)
          flagScore += 4;
        if (orAll & 0x80000)
          flagScore += 3;
        if (orAll & 0x40000)
          flagScore += 3;
        if (orAll & 0x1)
          flagScore += 3;
        if (orAll & 0x2000)
          flagScore += 3;
        if (orAll & 0x200)
          flagScore += 3;
        flagScore -= kEnumViolationPenalty * enumViolations;
      }

      int bestVtIdxForOff = -1;
      int bestPatternScoreForOff = 0;
      int bestRefCountForOff = 0;

      if (flagScore >= 0) {
        for (const auto &sample : vtableCode) {
          if (sample.index < 40)
            continue;

          int patternScore = ScoreProcessEventFlagPatterns(
              sample.bytes, sizeof(sample.bytes), off);
          int refCount = CountFlagOffsetReferences(
              sample.bytes, sizeof(sample.bytes), off);

          int rangeBonus = (sample.index >= 0x40 && sample.index <= 0x55) ? 5 : 0;
          int evidenceScore = patternScore * 10 + refCount * 4 + rangeBonus;
          int currentBestEvidenceScore =
              bestPatternScoreForOff * 10 + bestRefCountForOff * 4 +
              ((bestVtIdxForOff >= 0x40 && bestVtIdxForOff <= 0x55) ? 5 : 0);
          if (evidenceScore > currentBestEvidenceScore) {
            bestPatternScoreForOff = patternScore;
            bestRefCountForOff = refCount;
            bestVtIdxForOff = sample.index;
          }
        }
      }

      char probeBuf[256];
      snprintf(probeBuf, sizeof(probeBuf),
               "[Discovery]   probe off=0x%X: valid=%d zero=%d score=%d "
               "bestVT=%d pattern=%d refs=%d OR=0x%X AND=0x%X",
               off, validCount, zeroCount, flagScore, bestVtIdxForOff,
               bestPatternScoreForOff, bestRefCountForOff, orAll, andAll);
      probeLogs.emplace_back(probeBuf);

      if (flagScore > bestFlagsOnlyScore) {
        bestFlagsOnlyScore = flagScore;
        bestFlagsOnlyOff = off;
      }

      bool hasEvidence = (bestPatternScoreForOff > 0 || bestRefCountForOff >= 2);
      int combinedEvidenceScore =
          flagScore + bestPatternScoreForOff * 12 + bestRefCountForOff * 3;
      if (hasEvidence && combinedEvidenceScore > bestEvidenceScore) {
        bestEvidenceScore = combinedEvidenceScore;
        bestEvidenceOff = off;
        bestEvidenceVtIdx = bestVtIdxForOff;
        bestEvidencePatternScore = bestPatternScoreForOff;
        bestEvidenceRefCount = bestRefCountForOff;
      }
    }
  }

  struct ProcessEventVote {
    int objectReads = 0;
    int actorReads = 0;
    int objectMatch = 0;
    int actorMatch = 0;
    int unexplained = 0;
    uintptr_t firstUnexplained = 0;
    uintptr_t uobjectImpl = 0;
    uintptr_t aactorImpl = 0;
    bool derived = false;

    uintptr_t objectMajority = 0;
    int objectMajorityCount = 0;
    uintptr_t actorMajority = 0;
    int actorMajorityCount = 0;
  };

  auto MeasureProcessEventVote = [&](int slotIdx) -> ProcessEventVote {
    ProcessEventVote vote;
    if (slotIdx < 0) return vote;

    void *uobjectImpl = nullptr;
    void *aactorImpl = nullptr;
    bool distinct = false;
    if (ResolveProcessEventImplementations(state, slotIdx, &uobjectImpl, &aactorImpl,
                                           &distinct)) {
      vote.uobjectImpl = (uintptr_t)uobjectImpl;
      vote.aactorImpl = (uintptr_t)aactorImpl;
      vote.derived = distinct;
    }

    std::map<uintptr_t, int> objectFreq;
    std::map<uintptr_t, int> actorFreq;
    const int total = GetObjectCount(state);
    const int kSamples = 250;
    const int64_t stride = (total > kSamples) ? (total / kSamples) : 1;
    for (int64_t i = 0; i < total; i += stride) {
      UObject *obj = GetObjectByIndex(state, (int32_t)i);
      if (!obj) continue;
      void **vft = nullptr;
      if (!SafeMem_Read(obj, &vft, sizeof(vft)) || !vft) continue;
      if (((uintptr_t)vft) < 0x10000) continue;
      void *fn = nullptr;
      if (!SafeMem_Read(&vft[slotIdx], &fn, sizeof(fn)) || !fn) continue;

      UClass *cls = GetObjectClass(state, obj);
      const bool isActor =
          cls && state.AActorClass &&
          IsStructA(state, (UStruct *)cls, (UStruct *)state.AActorClass);
      const uintptr_t address = (uintptr_t)fn;
      if (isActor)
        ++vote.actorReads;
      else
        ++vote.objectReads;

      if (vote.derived) {
        const uintptr_t predicted = isActor ? vote.aactorImpl : vote.uobjectImpl;
        if (address == predicted) {
          if (isActor)
            ++vote.actorMatch;
          else
            ++vote.objectMatch;
        } else {
          ++vote.unexplained;
          if (!vote.firstUnexplained) vote.firstUnexplained = address;
        }
        continue;
      }

      if (isActor)
        ++actorFreq[address];
      else
        ++objectFreq[address];
    }
    for (const auto &kv : objectFreq) {
      if (kv.second > vote.objectMajorityCount) {
        vote.objectMajorityCount = kv.second;
        vote.objectMajority = kv.first;
      }
    }
    for (const auto &kv : actorFreq) {
      if (kv.second > vote.actorMajorityCount) {
        vote.actorMajorityCount = kv.second;
        vote.actorMajority = kv.first;
      }
    }
    return vote;
  };

  constexpr int kMinPopulationPercent = 70;

  auto VoteAccepts = [](const ProcessEventVote &vote) -> bool {
    const int classified = vote.objectReads + vote.actorReads;
    if (classified < 30)
      return true;

    if (vote.derived) {
      if (vote.objectReads > 0 &&
          vote.objectMatch * 100 < kMinPopulationPercent * vote.objectReads)
        return false;
      if (vote.actorReads > 0 &&
          vote.actorMatch * 100 < kMinPopulationPercent * vote.actorReads)
        return false;
      return true;
    }

    if (vote.objectReads > 0 &&
        vote.objectMajorityCount * 100 < kMinPopulationPercent * vote.objectReads)
      return false;
    if (vote.actorReads > 0 &&
        vote.actorMajorityCount * 100 < kMinPopulationPercent * vote.actorReads)
      return false;

    if (vote.actorReads >= 10 && vote.objectReads >= 10 &&
        vote.actorMajority == vote.objectMajority)
      return false;
    return true;
  };

  auto LogDerivedPair = [](int slot, const ProcessEventVote &vote) {
    if (!vote.derived) return;
    Log("[Discovery] ProcessEvent VTable[%d]: UObject's implementation %p (read from "
        "the UObject class default) and %p (Actor.h's override, read from the AActor "
        "class default)",
        slot, (void *)vote.uobjectImpl, (void *)vote.aactorImpl);
  };

  auto PercentTenths = [](int part, int whole) -> int {
    return whole > 0 ? (part * 1000) / whole : 0;
  };

  auto DescribeVote = [&PercentTenths](const ProcessEventVote &vote) -> std::string {
    char buf[640];
    if (vote.derived) {
      const int classified = vote.objectReads + vote.actorReads;
      const int matched = vote.objectMatch + vote.actorMatch;
      const int totalPct = PercentTenths(matched, classified);
      const int objectPct = PercentTenths(vote.objectMatch, vote.objectReads);
      const int actorPct = PercentTenths(vote.actorMatch, vote.actorReads);
      snprintf(buf, sizeof(buf),
               "%d/%d objects (%d.%d%%) carry the implementation their class chain "
               "predicts - non-AActor %d/%d (%d.%d%%) on %p (UObject's "
               "implementation), AActor %d/%d (%d.%d%%) on %p (AActor::ProcessEvent, "
               "the override Actor.h declares)%s",
               matched, classified, totalPct / 10, totalPct % 10, vote.objectMatch,
               vote.objectReads, objectPct / 10, objectPct % 10,
               (void *)vote.uobjectImpl, vote.actorMatch, vote.actorReads,
               actorPct / 10, actorPct % 10, (void *)vote.aactorImpl,
               vote.unexplained ? " - the remaining objects carry neither" : "");
      return std::string(buf);
    }
    const int objectPct = PercentTenths(vote.objectMajorityCount, vote.objectReads);
    if (vote.actorReads > 0) {
      const int actorPct = PercentTenths(vote.actorMajorityCount, vote.actorReads);
      snprintf(buf, sizeof(buf),
               "%d.%d%% of %d non-AActor objects agree on %p, %d.%d%% of %d AActor "
               "objects agree on %p (the class defaults could not be read, so these "
               "are the sample's own majorities)",
               objectPct / 10, objectPct % 10, vote.objectReads,
               (void *)vote.objectMajority, actorPct / 10, actorPct % 10,
               vote.actorReads, (void *)vote.actorMajority);
    } else {
      snprintf(buf, sizeof(buf), "%d.%d%% of %d objects agree on %p", objectPct / 10,
               objectPct % 10, vote.objectReads, (void *)vote.objectMajority);
    }
    return std::string(buf);
  };

  if (bestEvidenceOff >= 0) {
    state.UFunction_FlagsOff = bestEvidenceOff;
    state.ProcessEvent_VTableIdx = bestEvidenceVtIdx;
    logFunctionSampling();
    Log("[Discovery] ProcessEvent resolved: FunctionFlags=0x%X, VTable[%d], "
        "pattern=%d, refs=%d",
        bestEvidenceOff, bestEvidenceVtIdx, bestEvidencePatternScore,
        bestEvidenceRefCount);

    const ProcessEventVote vote = MeasureProcessEventVote(bestEvidenceVtIdx);
    LogDerivedPair(bestEvidenceVtIdx, vote);
    if (!VoteAccepts(vote)) {
      Log("[Discovery] ProcessEvent rejected: VTable[%d] cross-object consensus "
          "only %s - class-specific virtual, rescanning fallback slots",
          bestEvidenceVtIdx, DescribeVote(vote).c_str());
      state.ProcessEvent_VTableIdx = -1;
    } else {
      Log("[Discovery] ProcessEvent VTable[%d] cross-object consensus: %s",
          bestEvidenceVtIdx, DescribeVote(vote).c_str());
      return true;
    }
  }

  if (bestFlagsOnlyOff < 0 || bestFlagsOnlyScore < 0) {
    logFunctionSampling();
    Log("[Discovery] VTable source object: %p (%s)", vtableSourceObj,
        vtableSourceName.empty() ? "<unnamed>" : vtableSourceName.c_str());
    for (const auto &line : probeLogs)
      Log("%s", line.c_str());
    Log("[Discovery] Could not find UFunction::FunctionFlags");
    return false;
  }

  state.UFunction_FlagsOff = bestFlagsOnlyOff;

  int commonIndices[] = {0x4D, 0x44, 0x43, 0x45, 0x42, 0x4E, 0x4F, 0x41, 0x40,
                         0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x50,
                         0xBB, 0xBC, 0xBA};
  int bestFallbackIdx = -1;
  int bestFallbackPatternScore = 0;
  int bestFallbackRefCount = 0;

  for (int idx : commonIndices) {
    if (idx < 0 || idx >= kVTableEntries)
      continue;

    const VTableCode *sample = nullptr;
    for (const auto &cachedCode : vtableCode) {
      if (cachedCode.index == idx) {
        sample = &cachedCode;
        break;
      }
    }
    if (!sample)
      continue;

    int patternScore = ScoreProcessEventFlagPatterns(
        sample->bytes, sizeof(sample->bytes), bestFlagsOnlyOff);
    int refCount = CountFlagOffsetReferences(
        sample->bytes, sizeof(sample->bytes), bestFlagsOnlyOff);

    char fallbackBuf[160];
    snprintf(fallbackBuf, sizeof(fallbackBuf),
             "[Discovery]   fallback VTable[%d]: pattern=%d refs=%d", idx,
             patternScore, refCount);
    fallbackSlotLogs.emplace_back(fallbackBuf);

    int evidenceScore = patternScore * 10 + refCount * 4;
    int bestFallbackEvidenceScore =
        bestFallbackPatternScore * 10 + bestFallbackRefCount * 4;
    if (evidenceScore > bestFallbackEvidenceScore) {
      bestFallbackIdx = idx;
      bestFallbackPatternScore = patternScore;
      bestFallbackRefCount = refCount;
    }
  }

  if (bestFallbackIdx >= 0 &&
      (bestFallbackPatternScore > 0 || bestFallbackRefCount >= 1)) {
    state.ProcessEvent_VTableIdx = bestFallbackIdx;
    logFunctionSampling();
    Log("[Discovery] ProcessEvent resolved: FunctionFlags=0x%X, VTable[%d] "
        "(fallback, pattern=%d, refs=%d)",
        bestFlagsOnlyOff, bestFallbackIdx, bestFallbackPatternScore,
        bestFallbackRefCount);

    const ProcessEventVote vote = MeasureProcessEventVote(bestFallbackIdx);
    LogDerivedPair(bestFallbackIdx, vote);
    if (!VoteAccepts(vote)) {
      Log("[Discovery] ProcessEvent fallback rejected: VTable[%d] consensus "
          "only %s - continuing to default slot",
          bestFallbackIdx, DescribeVote(vote).c_str());
      state.ProcessEvent_VTableIdx = -1;
      fallbackSlotRejected = true;
    } else {
      Log("[Discovery] ProcessEvent VTable[%d] cross-object consensus "
          "(fallback): %s",
          bestFallbackIdx, DescribeVote(vote).c_str());
      return true;
    }
  }

  int defaultIdx = (state.UEMajor >= 5) ? ((state.UEMinor >= 4) ? 0x4D : 0x44) : 0x43;

  state.ProcessEvent_VTableIdx = (!fallbackSlotRejected && bestFallbackIdx >= 0) ? bestFallbackIdx : defaultIdx;
  logFunctionSampling();
  Log("[Discovery] VTable source object: %p (%s)", vtableSourceObj,
      vtableSourceName.empty() ? "<unnamed>" : vtableSourceName.c_str());
  Log("[Discovery] ProcessEvent resolved: FunctionFlags=0x%X, VTable[%d] (default for UE%d.%d)",
      bestFlagsOnlyOff, state.ProcessEvent_VTableIdx, state.UEMajor, state.UEMinor);

  {
    const ProcessEventVote vote =
        MeasureProcessEventVote(state.ProcessEvent_VTableIdx);
    LogDerivedPair(state.ProcessEvent_VTableIdx, vote);
    if (!VoteAccepts(vote)) {
      Log("[Discovery] ProcessEvent rejected: VTable[%d] consensus only %s; "
          "no slot reached cross-object consensus - hooks stay disabled for safety",
          state.ProcessEvent_VTableIdx, DescribeVote(vote).c_str());
      state.ProcessEvent_VTableIdx = -1;
      return false;
    }
    Log("[Discovery] ProcessEvent VTable[%d] cross-object consensus (default): %s",
        state.ProcessEvent_VTableIdx, DescribeVote(vote).c_str());
  }
  return true;
}

namespace {

constexpr uint32_t kBytecodeNativeFlag = 0x00000400u;

constexpr uint8_t kExprReturn = 0x04;
constexpr uint8_t kExprNothing = 0x0B;

constexpr uint8_t kFrameNodeOff = 0x10;
constexpr uint8_t kFrameObjectOff = 0x18;
constexpr uint8_t kFrameCodeOff = 0x20;
constexpr uint8_t kFrameLocalsOff = 0x28;

constexpr int32_t kFuncFieldWindowFirst = 0x40;
constexpr int32_t kFuncFieldWindowLast = 0x200;

constexpr size_t kProcessInternalProbeSize = 0x400;
constexpr size_t kInterpreterProbeSize = 0x600;

struct CallTargetSummary {
  uint32_t count = 0;
  uintptr_t firstCallSite = 0;
};

static void TallyDirectTransfers(
    uintptr_t function, const uint8_t *code, size_t size,
    const std::vector<ExecutableRange> &ranges,
    std::unordered_map<uintptr_t, CallTargetSummary> &out) {
  for (size_t i = 0; i + 5 <= size; ++i) {
    uintptr_t target = 0;
    if (code[i] == 0xE8 || code[i] == 0xE9) {
      int32_t displacement = 0;
      memcpy(&displacement, code + i + 1, sizeof(displacement));
      target = function + i + 5 + displacement;
    } else if (code[i] == 0xEB) {
      target = function + i + 2 + static_cast<int8_t>(code[i + 1]);
    } else {
      continue;
    }
    if (!IsExecutableAddress(ranges, target))
      continue;
    CallTargetSummary &summary = out[target];
    if (summary.count == 0)
      summary.firstCallSite = function + i;
    ++summary.count;
  }
}

struct InterpreterEvidence {
  uintptr_t target = 0;
  uintptr_t callSite = 0;
  uint32_t callCount = 0;
  uint32_t repeatedCalleeCount = 0;
  int frameFieldReads = 0;
  bool sawFrameCode = false;
  bool sawReturnSentinel = false;
  bool sawNothingSentinel = false;
  bool hasAddressReference = false;
  bool referencesGnatives = false;
  int score = 0;
};

static bool IsByteCompareAgainst(const uint8_t *code, size_t size,
                                 size_t opcodeIndex, uint8_t expected) {
  if (opcodeIndex + 2 > size)
    return false;
  const uint8_t modrm = code[opcodeIndex + 1];
  if ((modrm & 0x38) != 0x38)
    return false;
  const uint8_t mod = static_cast<uint8_t>(modrm >> 6);
  const uint8_t rm = static_cast<uint8_t>(modrm & 0x07);
  size_t immediate = opcodeIndex + 2;
  if (mod != 3) {
    if (rm == 4) {
      if (immediate >= size)
        return false;
      const uint8_t sib = code[immediate++];
      if (mod == 0 && (sib & 0x07) == 5)
        immediate += 4;
    } else if (mod == 0 && rm == 5) {
      immediate += 4;
    }
    if (mod == 1)
      immediate += 1;
    else if (mod == 2)
      immediate += 4;
  }
  if (immediate >= size)
    return false;
  return code[immediate] == expected;
}

static void TallyRipLeaTargets(uintptr_t function, const uint8_t *code,
                               size_t size,
                               std::unordered_set<uintptr_t> &targets) {
  for (size_t i = 0; i + 7 <= size; ++i) {
    if ((code[i] & 0xF0) != 0x40 || code[i + 1] != 0x8D)
      continue;
    const uint8_t modrm = code[i + 2];
    if ((modrm >> 6) != 0 || (modrm & 7) != 5)
      continue;
    int32_t displacement = 0;
    memcpy(&displacement, code + i + 3, sizeof(displacement));
    targets.insert(function + i + 7 + displacement);
  }
}

static void TallyCallArgumentLeas(
    uintptr_t function, const uint8_t *code, size_t size,
    const std::vector<ExecutableRange> &ranges,
    std::unordered_set<uintptr_t> &targets) {
  for (size_t i = 0; i + 7 <= size; ++i) {
    if ((code[i] & 0xF0) != 0x40 || code[i + 1] != 0x8D)
      continue;
    const uint8_t modrm = code[i + 2];
    if ((modrm >> 6) != 0 || (modrm & 7) != 5)
      continue;
    int32_t displacement = 0;
    memcpy(&displacement, code + i + 3, sizeof(displacement));
    const uintptr_t target = function + i + 7 + displacement;
    if (!IsExecutableAddress(ranges, target))
      continue;
    bool feedsCall = false;
    const size_t limit = (std::min)(size, i + 7 + 0x20);
    for (size_t k = i + 7; k + 5 <= limit; ++k) {
      if (code[k] == 0xE8) {
        feedsCall = true;
        break;
      }
    }
    if (feedsCall)
      targets.insert(target);
  }
}

static InterpreterEvidence ScoreInterpreterBody(
    uintptr_t target, uintptr_t callSite, uint32_t callCount,
    const std::vector<ExecutableRange> &ranges, uintptr_t gnatives) {
  InterpreterEvidence evidence;
  evidence.target = target;
  evidence.callSite = callSite;
  evidence.callCount = callCount;

  std::vector<uint8_t> code(kInterpreterProbeSize, 0);
  if (!SafeMem_Read(reinterpret_cast<void *>(target), code.data(), code.size()))
    return evidence;

  std::unordered_map<uintptr_t, CallTargetSummary> callees;
  TallyDirectTransfers(target, code.data(), code.size(), ranges, callees);
  for (const auto &pair : callees) {
    if (pair.second.count > evidence.repeatedCalleeCount)
      evidence.repeatedCalleeCount = pair.second.count;
  }

  uint8_t frameMembers[8] = {};
  for (size_t i = 0; i + 4 <= code.size(); ++i) {
    if (code[i] < 0x48 || code[i] > 0x4F)
      continue;
    if (code[i + 1] != 0x8B && code[i + 1] != 0x8D)
      continue;
    const uint8_t modrm = code[i + 2];
    if ((modrm & 0xC0) != 0x40)
      continue;
    const uint8_t base = modrm & 0x07;
    if (base == 0x04)
      continue;
    switch (code[i + 3]) {
    case kFrameNodeOff: frameMembers[base] |= 0x1; break;
    case kFrameObjectOff: frameMembers[base] |= 0x2; break;
    case kFrameCodeOff:
      frameMembers[base] |= 0x4;
      evidence.sawFrameCode = true;
      break;
    case kFrameLocalsOff: frameMembers[base] |= 0x8; break;
    default: break;
    }
  }
  for (const uint8_t members : frameMembers) {
    int seen = 0;
    for (int bit = 0; bit < 4; ++bit) {
      if (members & (1 << bit))
        ++seen;
    }
    evidence.frameFieldReads = (std::max)(evidence.frameFieldReads, seen);
  }

  for (size_t i = 0; i + 3 <= code.size(); ++i) {
    if (code[i] != 0x80)
      continue;
    if (IsByteCompareAgainst(code.data(), code.size(), i, kExprReturn))
      evidence.sawReturnSentinel = true;
    else if (IsByteCompareAgainst(code.data(), code.size(), i, kExprNothing))
      evidence.sawNothingSentinel = true;
  }

  if (gnatives) {
    std::unordered_set<uintptr_t> leaTargets;
    TallyRipLeaTargets(target, code.data(), code.size(), leaTargets);
    evidence.referencesGnatives = leaTargets.count(gnatives) != 0;
  }

  evidence.score =
      (evidence.sawReturnSentinel ? 200 : 0) +
      (evidence.sawNothingSentinel ? 200 : 0) +
      (evidence.referencesGnatives ? 150 : 0) +
      (evidence.sawFrameCode ? 100 : 0) + evidence.frameFieldReads * 30 +
      static_cast<int>((std::min)(evidence.repeatedCalleeCount, 4u)) * 25;
  return evidence;
}

static void LogInterpreterCandidate(const InterpreterEvidence &candidate,
                                    uintptr_t function) {
  Log("[Discovery]   ProcessLocalScriptFunction candidate=%p "
      "(from ProcessInternal+0x%llX, transfers=%u, repeated-callee=%u, "
      "frame-members=%d, frame-code=%d, EX_Return=%d, EX_Nothing=%d, "
      "gnatives=%d, score=%d)",
      reinterpret_cast<void *>(candidate.target),
      static_cast<unsigned long long>(candidate.callSite - function),
      candidate.callCount, candidate.repeatedCalleeCount,
      candidate.frameFieldReads, candidate.sawFrameCode ? 1 : 0,
      candidate.sawReturnSentinel ? 1 : 0, candidate.sawNothingSentinel ? 1 : 0,
      candidate.referencesGnatives ? 1 : 0, candidate.score);
}

struct BytecodeFuncBinding {
  int32_t fieldOffset = -1;
  uintptr_t address = 0;
  size_t agreeingFunctions = 0;
  size_t runnerUpAgreement = 0;
  size_t sampledFunctions = 0;
};

static bool FindBytecodeFuncBinding(
    EngineState &state, const std::vector<ExecutableRange> &ranges,
    BytecodeFuncBinding &binding) {
  const auto functions = SnapshotLiveUFunctions(state);
  if (functions.empty() || state.UFunction_FlagsOff < 0)
    return false;

  const size_t window =
      static_cast<size_t>(kFuncFieldWindowLast) + sizeof(uintptr_t);
  std::vector<uint8_t> bytes(window, 0);
  std::unordered_map<int32_t, std::unordered_map<uintptr_t, size_t>> votes;

  constexpr size_t kMaxSamples = 2000;
  for (UFunction *function : functions) {
    if (binding.sampledFunctions >= kMaxSamples)
      break;
    if (!function)
      continue;
    if (!SafeMem_Read(function, bytes.data(), bytes.size()))
      continue;

    uint32_t flags = 0;
    memcpy(&flags, bytes.data() + state.UFunction_FlagsOff, sizeof(flags));

    if ((flags & kBytecodeNativeFlag) != 0)
      continue;
    ++binding.sampledFunctions;

    for (int32_t offset = kFuncFieldWindowFirst; offset <= kFuncFieldWindowLast;
         offset += static_cast<int32_t>(sizeof(uintptr_t))) {
      uintptr_t value = 0;
      memcpy(&value, bytes.data() + offset, sizeof(value));
      if (!IsExecutableAddress(ranges, value))
        continue;
      ++votes[offset][FollowJumpThunk(value, ranges)];
    }
  }

  if (binding.sampledFunctions == 0)
    return false;

  for (const auto &field : votes) {
    for (const auto &value : field.second) {
      if (value.second > binding.agreeingFunctions) {
        binding.runnerUpAgreement = binding.agreeingFunctions;
        binding.agreeingFunctions = value.second;
        binding.fieldOffset = field.first;
        binding.address = value.first;
      } else if (value.second > binding.runnerUpAgreement) {
        binding.runnerUpAgreement = value.second;
      }
    }
  }

  const size_t minimumEvidence =
      (std::min)(size_t{8}, binding.sampledFunctions);
  if (!binding.address || binding.agreeingFunctions < minimumEvidence ||
      binding.agreeingFunctions <= binding.runnerUpAgreement)
    return false;
  return true;
}

static bool ResolveInterpreterViaLocalFunctionHandlers(
    EngineState &state, const std::vector<ExecutableRange> &ranges,
    InterpreterEvidence &winner) {
  constexpr uint32_t kExecLocalVirtualFunction = 0x45;
  constexpr uint32_t kExecLocalFinalFunction = 0x46;
  if (!state.GNatives) {
    Log("[Discovery] ProcessLocalScriptFunction fallback skipped: GNatives is "
        "not resolved.");
    return false;
  }
  const uintptr_t gnatives = reinterpret_cast<uintptr_t>(state.GNatives);
  const uintptr_t processInternal =
      reinterpret_cast<uintptr_t>(state.ProcessInternal);

  constexpr size_t kHandlerProbe = 0x300;
  constexpr size_t kLocalFunctionProbe = 0x400;
  std::vector<uint8_t> body(kLocalFunctionProbe, 0);
  std::unordered_set<uintptr_t> candidates;
  std::unordered_set<uintptr_t> scannedHandlers;

  for (uint32_t token : {kExecLocalVirtualFunction, kExecLocalFinalFunction}) {
    uintptr_t handler = 0;
    const uintptr_t entry =
        gnatives + static_cast<uintptr_t>(token) * sizeof(void *);
    if (!SafeMem_Read(reinterpret_cast<const void *>(entry), &handler,
                      sizeof(handler)) ||
        !IsExecutableAddress(ranges, handler) ||
        !scannedHandlers.insert(handler).second)
      continue;

    handler = FollowJumpThunk(handler, ranges);
    if (!IsExecutableAddress(ranges, handler))
      continue;

    if (!SafeMem_Read(reinterpret_cast<void *>(handler), body.data(),
                      kHandlerProbe))
      continue;
    TallyCallArgumentLeas(handler, body.data(), kHandlerProbe, ranges,
                          candidates);

    std::unordered_map<uintptr_t, CallTargetSummary> callees;
    TallyDirectTransfers(handler, body.data(), kHandlerProbe, ranges, callees);
    for (const auto &pair : callees) {
      const uintptr_t callee = FollowJumpThunk(pair.first, ranges);
      if (callee == handler || !IsExecutableAddress(ranges, callee))
        continue;
      if (!SafeMem_Read(reinterpret_cast<void *>(callee), body.data(),
                        kLocalFunctionProbe))
        continue;
      TallyCallArgumentLeas(callee, body.data(), kLocalFunctionProbe, ranges,
                            candidates);
    }
  }

  std::vector<InterpreterEvidence> scored;
  for (uintptr_t candidate : candidates) {
    if (!IsExecutableAddress(ranges, candidate) || candidate == processInternal)
      continue;
    InterpreterEvidence evidence =
        ScoreInterpreterBody(candidate, 0, 0, ranges, gnatives);

    if (!evidence.sawReturnSentinel || !evidence.sawNothingSentinel ||
        !evidence.sawFrameCode || evidence.frameFieldReads < 2)
      continue;
    if (!evidence.referencesGnatives && evidence.repeatedCalleeCount < 2)
      continue;
    scored.push_back(evidence);
  }
  std::sort(scored.begin(), scored.end(),
            [](const InterpreterEvidence &a, const InterpreterEvidence &b) {
              return a.score > b.score;
            });

  if (scored.empty()) {
    Log("[Discovery] ProcessLocalScriptFunction fallback found no interpreter "
        "body among %zu address-referenced candidate(s) from GNatives[0x45/0x46].",
        candidates.size());
    return false;
  }
  if (scored.size() > 1 && scored[0].score == scored[1].score) {
    Log("[Discovery] ProcessLocalScriptFunction fallback ambiguous: %zu bodies "
        "carry identical interpreter traits; refusing to guess.",
        scored.size());
    for (size_t i = 0; i < scored.size() && i < 6; ++i)
      Log("[Discovery]   fallback candidate=%p gnatives=%d repeated-callee=%u "
          "frame-members=%d EX_Return=%d EX_Nothing=%d score=%d",
          reinterpret_cast<void *>(scored[i].target),
          scored[i].referencesGnatives ? 1 : 0, scored[i].repeatedCalleeCount,
          scored[i].frameFieldReads, scored[i].sawReturnSentinel ? 1 : 0,
          scored[i].sawNothingSentinel ? 1 : 0, scored[i].score);
    return false;
  }
  winner = scored.front();
  winner.hasAddressReference = true;
  return true;
}

}

bool FindProcessInternal(EngineState &state) {
  if (state.ProcessInternal)
    return true;
  if (!state.bIndexBuilt) {
    Log("[Discovery] ProcessInternal deferred: the reflection index is not "
        "ready.");
    return false;
  }
  if (state.UFunction_FlagsOff < 0) {
    Log("[Discovery] ProcessInternal unresolved: UFunction::FunctionFlags has "
        "no offset yet.");
    return false;
  }

  const auto ranges = GetAllExecutableRanges();
  if (ranges.empty())
    return false;

  BytecodeFuncBinding binding;
  if (!FindBytecodeFuncBinding(state, ranges, binding)) {
    Log("[Discovery] ProcessInternal unresolved: none of the %zu bytecode "
        "UFunctions shared one code pointer (UFunction::Bind binds Func to "
        "ProcessInternal for them).",
        binding.sampledFunctions);
    return false;
  }

  if (state.UFunction_FuncOff != binding.fieldOffset) {
    if (state.UFunction_FuncOff >= 0) {
      Log("[Discovery] UFunction::Func: bytecode consensus says 0x%X while "
          "native bindings said 0x%X; keeping the bytecode consensus.",
          binding.fieldOffset, state.UFunction_FuncOff);
    }
    state.UFunction_FuncOff = binding.fieldOffset;
  }

  state.ProcessInternal = reinterpret_cast<void *>(binding.address);
  Log("[Discovery] ProcessInternal resolved: %p (UFunction::Bind binds Func to "
      "ProcessInternal: %zu of %zu bytecode UFunctions agree, runner-up %zu, "
      "UFunction::Func=0x%X).",
      reinterpret_cast<void *>(binding.address), binding.agreeingFunctions,
      binding.sampledFunctions, binding.runnerUpAgreement,
      binding.fieldOffset);
  return true;
}

bool FindProcessLocalScriptFunction(EngineState &state) {
  if (state.ProcessLocalScriptFunction)
    return true;
  if (!state.ProcessInternal) {
    if (!FindProcessInternal(state)) {
      Log("[Discovery] ProcessLocalScriptFunction deferred: ProcessInternal is "
          "not resolved.");
      return false;
    }
  }

  const auto ranges = GetAllExecutableRanges();
  if (ranges.empty())
    return false;

  const uintptr_t function =
      reinterpret_cast<uintptr_t>(state.ProcessInternal);
  std::vector<uint8_t> code(kProcessInternalProbeSize, 0);
  if (!SafeMem_Read(reinterpret_cast<void *>(function), code.data(),
                    code.size())) {
    Log("[Discovery] ProcessLocalScriptFunction unresolved: ProcessInternal "
        "body at %p is not readable.",
        reinterpret_cast<void *>(function));
    return false;
  }

  const uintptr_t gnatives = reinterpret_cast<uintptr_t>(state.GNatives);

  {
    const InterpreterEvidence self =
        ScoreInterpreterBody(function, 0, 0, ranges, gnatives);
    if (self.sawReturnSentinel && self.sawNothingSentinel) {
      Log("[Discovery] ProcessLocalScriptFunction is inlined into "
          "ProcessInternal (EX_Return/EX_Nothing tests found in its body %p); "
          "resolving through GNatives[0x45/0x46] -> execLocal*Function -> "
          "ProcessLocalFunction -> ProcessScriptFunction.",
          reinterpret_cast<void *>(function));
      InterpreterEvidence fallback;
      if (ResolveInterpreterViaLocalFunctionHandlers(state, ranges, fallback)) {
        state.ProcessLocalScriptFunction =
            reinterpret_cast<void *>(fallback.target);
        Log("[Discovery] ProcessLocalScriptFunction resolved: %p (address "
            "passed to ProcessScriptFunction by ProcessLocalFunction; frame "
            "members read=%d, EX_Return=%d, EX_Nothing=%d, gnatives=%d).",
            reinterpret_cast<void *>(fallback.target), fallback.frameFieldReads,
            fallback.sawReturnSentinel ? 1 : 0,
            fallback.sawNothingSentinel ? 1 : 0,
            fallback.referencesGnatives ? 1 : 0);
        return true;
      }
      Log("[Discovery] ProcessLocalScriptFunction unresolved: the interpreter "
          "is inlined into ProcessInternal and the GNatives handler edge did "
          "not produce a verified interpreter body.");
      return false;
    }
  }

  std::unordered_map<uintptr_t, CallTargetSummary> callees;
  TallyDirectTransfers(function, code.data(), code.size(), ranges, callees);

  std::vector<InterpreterEvidence> scored;
  for (const auto &pair : callees) {

    const uintptr_t target = FollowJumpThunk(pair.first, ranges);
    if (target == function)
      continue;

    if (target > function && target < function + kProcessInternalProbeSize)
      continue;
    scored.push_back(ScoreInterpreterBody(target, pair.second.firstCallSite,
                                          pair.second.count, ranges, gnatives));
  }
  std::sort(scored.begin(), scored.end(),
            [](const InterpreterEvidence &a, const InterpreterEvidence &b) {
              return a.score > b.score;
            });

  if (IsVerboseLoggingEnabled()) {
    for (const auto &candidate : scored)
      LogInterpreterCandidate(candidate, function);
  }

  const bool shapeConfirmed = !scored.empty() &&
                              scored.front().frameFieldReads >= 2 &&
                              scored.front().sawFrameCode &&
                              scored.front().sawReturnSentinel &&
                              scored.front().sawNothingSentinel &&
                              scored.front().repeatedCalleeCount >= 2;
  size_t winnerIndex = 0;

  std::unordered_set<uintptr_t> addressReferenced;
  const auto HasAddressReference = [&addressReferenced](uintptr_t target) {
    return addressReferenced.count(target) != 0;
  };

  if (shapeConfirmed && scored.size() > 1 &&
      scored.front().score == scored[1].score) {
    size_t tied = 1;
    while (tied < scored.size() && scored[tied].score == scored.front().score)
      ++tied;

    std::unordered_set<uintptr_t> candidates;
    for (size_t i = 0; i < tied; ++i)
      candidates.insert(scored[i].target);

    const auto modules = PatternScan::GetLoadedModules();
    const PatternScan::ModuleInfo *owner =
        FindOwningModuleForAddress(modules, function);
    if (owner) {
      const auto ownerRanges = GetExecutableRanges(*owner);

      std::vector<ExecutableRange> localRanges;
      constexpr uintptr_t kLocalWindow = 512 * 1024;
      for (const auto &r : ownerRanges) {
        if (function >= r.begin && function < r.end) {
          uintptr_t wStart = (function > r.begin + kLocalWindow) ? (function - kLocalWindow) : r.begin;
          uintptr_t wEnd = (std::min)(r.end, function + kLocalWindow);
          localRanges.push_back({wStart, wEnd});
        }
      }
      if (!localRanges.empty()) {
        addressReferenced = FindRipLeaTargetsAmong(localRanges, candidates);
      }
      if (addressReferenced.empty()) {
        addressReferenced = FindRipLeaTargetsAmong(ownerRanges, candidates);
      }
    }

    size_t referenced = 0;
    for (size_t i = 0; i < tied; ++i) {
      if (HasAddressReference(scored[i].target)) {
        ++referenced;
        winnerIndex = i;
      }
    }

    if (referenced != 1) {
      Log("[Discovery] ProcessLocalScriptFunction ambiguous: %zu bodies carry "
          "identical interpreted-bytecode evidence, %zu of them are the target "
          "of an address reference; refusing to guess.",
          tied, referenced);
      return false;
    }
    Log("[Discovery] ProcessLocalScriptFunction tie broken by the source-line-8 "
        "address reference: %p of %zu otherwise identical bodies.",
        reinterpret_cast<void *>(scored[winnerIndex].target), tied);
  }
  if (!shapeConfirmed) {
    Log("[Discovery] ProcessLocalScriptFunction NOT confirmed: %zu direct "
        "transfer(s) out of ProcessInternal %p were checked and none carried "
        "the interpreted-bytecode shape.",
        scored.size(), reinterpret_cast<void *>(function));
    for (size_t i = 0; i < scored.size() && i < 8; ++i)
      LogInterpreterCandidate(scored[i], function);
    return false;
  }

  InterpreterEvidence &winner = scored[winnerIndex];
  winner.hasAddressReference = HasAddressReference(winner.target);

  state.ProcessLocalScriptFunction = reinterpret_cast<void *>(winner.target);
  Log("[Discovery] ProcessLocalScriptFunction resolved: %p (called from "
      "ProcessInternal+0x%llX; FFrame::Step retargets=%u, frame members read=%d, "
      "EX_Return=%d, EX_Nothing=%d, address reference=%d).",
      reinterpret_cast<void *>(winner.target),
      static_cast<unsigned long long>(winner.callSite - function),
      winner.repeatedCalleeCount, winner.frameFieldReads,
      winner.sawReturnSentinel ? 1 : 0, winner.sawNothingSentinel ? 1 : 0,
      winner.hasAddressReference ? 1 : 0);
  return true;
}

void *FindFunctionByName(const EngineState &state, UStruct *cls,
                         const char *funcName) {
  if (!cls || !funcName || state.UStruct_ChildrenOff < 0)
    return nullptr;

  void *child = GetChildrenHead(state, cls);
  int count = 0;

  while (child && count < 500) {

    std::string name = GetObjectNameStr(state, (UObject *)child);
    if (_stricmp(name.c_str(), funcName) == 0)
      return child;
    child = GetFieldNext(state, child);
    count++;
  }

  UStruct *super = GetSuperStruct(state, cls);
  if (super) {
    return FindFunctionByName(state, super, funcName);
  }

  return nullptr;
}

static void LogProcessEventDeduplicated(void *object, void *ufunction, void *peFunc, void *params) {
  static std::unordered_set<uintptr_t> s_SeenCalls;
  static std::mutex s_SeenCallsMutex;

  uintptr_t callKey = (uintptr_t)ufunction;
  bool isFirstTime = false;
  {
    std::lock_guard<std::mutex> lock(s_SeenCallsMutex);
    if (s_SeenCalls.size() > 50000) {
      s_SeenCalls.clear();
    }
    isFirstTime = s_SeenCalls.insert(callKey).second;
  }

  if (isFirstTime) {
    Log("[ProcessEvent] CallProcessEvent: obj=%p, func=%p, PE=%p, params=%p", object,
        ufunction, peFunc, params);
  }
}

namespace {

thread_local int s_LoaderDispatchedCallDepth = 0;

struct ProcessEventFault {
  bool captured = false;
  uint32_t code = 0;
  uint64_t faultAddress = 0;
  uint64_t rip = 0;
  uint64_t rsp = 0;
  uint64_t rbp = 0;
};

bool CaptureProcessEventFault(EXCEPTION_POINTERS *ep, ProcessEventFault *out) {
  if (ep && ep->ExceptionRecord && out) {
    out->captured = true;
    out->code = ep->ExceptionRecord->ExceptionCode;
    out->faultAddress = reinterpret_cast<uint64_t>(ep->ExceptionRecord->ExceptionAddress);
    if (ep->ContextRecord) {
      out->rip = ep->ContextRecord->Rip;
      out->rsp = ep->ContextRecord->Rsp;
      out->rbp = ep->ContextRecord->Rbp;
    }
  }
  return false;
}

}

bool IsLoaderDispatchedCall() { return s_LoaderDispatchedCallDepth > 0; }

static void FillObjectFullName(const EngineState &state, void *object, char *out, int outSize) {
  out[0] = '\0';

  std::string name = BuildFullName(state, (UObject *)object);
  if (name.empty()) {
    name = GetObjectNameStr(state, (UObject *)object);
  }
  if (!name.empty()) {
    strncpy_s(out, static_cast<size_t>(outSize), name.c_str(), _TRUNCATE);
  }
}

static void DescribeProcessEventTarget(const EngineState &state, void *object, void *ufunction,
                                       char *out, size_t outSize) {
  char functionName[320] = {};
  char objectName[320] = {};

  __try {
    FillObjectFullName(state, ufunction, functionName, sizeof(functionName));
    FillObjectFullName(state, object, objectName, sizeof(objectName));
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    if (functionName[0] == '\0') {
      strncpy_s(functionName, sizeof(functionName), "(unreadable - engine structures already invalid)", _TRUNCATE);
    }
    if (objectName[0] == '\0') {
      strncpy_s(objectName, sizeof(objectName), "(unreadable)", _TRUNCATE);
    }
  }

  snprintf(out, outSize, "calling %s on %s (obj=%p, func=%p)",
           functionName[0] != '\0' ? functionName : "(unnamed UFunction)",
           objectName[0] != '\0' ? objectName : "(unknown object)", object, ufunction);
}

static void ReportSwallowedProcessEventFault(const EngineState &state, void *object,
                                             void *ufunction,
                                             const ProcessEventFault &fault) {
  if (!fault.captured) {

    return;
  }

  char summary[512] = {};
  DescribeProcessEventTarget(state, object, ufunction, summary, sizeof(summary));

  Log("[ProcessEvent] RECOVERED FAULT 0x%X at %p", fault.code,
      reinterpret_cast<void *>(static_cast<uintptr_t>(fault.faultAddress)));
  Log("[ProcessEvent]   %s", summary);
}

bool CallProcessEvent(const EngineState &state, void *object, void *ufunction,
                      void *params) {
  if (!object || !ufunction || state.ProcessEvent_VTableIdx < 0)
    return false;

  void **vft = nullptr;
  if (!SafeMem_Read(object, &vft, sizeof(void *)) || !vft)
    return false;

  void *peFunc = nullptr;
  if (!SafeMem_Read(&vft[state.ProcessEvent_VTableIdx], &peFunc,
                    sizeof(void *)) ||
      !peFunc)
    return false;

  LogProcessEventDeduplicated(object, ufunction, peFunc, params);

  typedef void (*ProcessEventFn)(void *, void *, void *);
  ProcessEventFn pe = (ProcessEventFn)peFunc;

  ProcessEventFault fault;

  ++s_LoaderDispatchedCallDepth;

  __try {
    pe(object, ufunction, params);
    --s_LoaderDispatchedCallDepth;
    return true;
  } __except (CaptureProcessEventFault(GetExceptionInformation(), &fault),
              EXCEPTION_EXECUTE_HANDLER) {
    --s_LoaderDispatchedCallDepth;
    Log("[ProcessEvent] CallProcessEvent CRASHED (exception 0x%X)", GetExceptionCode());
    ReportSwallowedProcessEventFault(state, object, ufunction, fault);
    return false;
  }
}

}
