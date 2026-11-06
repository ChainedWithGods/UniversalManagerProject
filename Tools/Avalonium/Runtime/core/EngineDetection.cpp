#include "EngineDetection.h"
#include "AllocatorSlotScan.h"
#include "GNativesResolver.h"
#include "RuntimeConfig.h"
#include "Logger.h"
#include "PatternScanner.h"
#include "ObjectArray.h"
#include "NameResolution.h"
#include "ObjectUtils.h"
#include "../utils/SafeMemory.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstring>
#include <emmintrin.h>
#include <execution>
#include <intrin.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <psapi.h>
#include <sstream>
#include <atomic>
#include <future>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace EngineDiscovery {

bool DetectVersion(EngineState &state) {
  auto tryDetectFromExecutableVersion = [&state]() -> bool {

    HMODULE hVersion = LoadLibraryW(L"version.dll");
    if (!hVersion) {
      return false;
    }

    auto pGetFileVersionInfoSizeW = (DWORD(WINAPI *)(
        LPCWSTR, LPDWORD))GetProcAddress(hVersion, "GetFileVersionInfoSizeW");
    auto pGetFileVersionInfoW =
        (BOOL(WINAPI *)(LPCWSTR, DWORD, DWORD, LPVOID))GetProcAddress(
            hVersion, "GetFileVersionInfoW");
    auto pVerQueryValueW =
        (BOOL(WINAPI *)(LPCVOID, LPCWSTR, LPVOID *, PUINT))GetProcAddress(
            hVersion, "VerQueryValueW");

    if (pGetFileVersionInfoSizeW && pGetFileVersionInfoW && pVerQueryValueW) {
      wchar_t exePath[MAX_PATH] = {};
      GetModuleFileNameW(NULL, exePath, MAX_PATH);
      DWORD dummy = 0;
      const DWORD infoSize = pGetFileVersionInfoSizeW(exePath, &dummy);
      Log("[Discovery] GetFileVersionInfoSizeW returned %d", infoSize);
      if (infoSize > 0) {
        std::vector<uint8_t> verData(infoSize);
        if (pGetFileVersionInfoW(exePath, dummy, infoSize, verData.data())) {

          struct LANGANDCODEPAGE {
            WORD wLanguage;
            WORD wCodePage;
          } *lpTranslate = nullptr;
          UINT cbTranslate = 0;

          if (pVerQueryValueW(verData.data(), L"\\VarFileInfo\\Translation",
                              (LPVOID *)&lpTranslate, &cbTranslate) &&
              lpTranslate) {
            size_t numTranslations =
                cbTranslate / sizeof(struct LANGANDCODEPAGE);
            Log("[Discovery] Found %zu translation(s) in VersionInfo",
                numTranslations);
            for (size_t i = 0; i < numTranslations; i++) {
              const wchar_t *keys[] = {L"ProductVersion", L"FileVersion",
                                       L"ProductName", L"FileDescription",
                                       L"InternalName"};
              for (const wchar_t *key : keys) {
                wchar_t subBlock[256];
                swprintf(subBlock, 256, L"\\StringFileInfo\\%04x%04x\\%s",
                         lpTranslate[i].wLanguage, lpTranslate[i].wCodePage,
                         key);

                wchar_t *prodVerStr = nullptr;
                UINT prodVerLen = 0;
                if (pVerQueryValueW(verData.data(), subBlock,
                                    (LPVOID *)&prodVerStr, &prodVerLen) &&
                    prodVerStr) {
                  std::wstring pvW = prodVerStr;
                  std::string pv(pvW.begin(),
                                 pvW.end());

                  Log("[Discovery] VersionInfo string: '%s'", pv.c_str());
                  size_t uePos = pv.find("++UE");
                  if (uePos != std::string::npos ||
                      pv.find("Release-") != std::string::npos) {
                    size_t relPos = pv.find("Release-");
                    if (relPos != std::string::npos) {
                      int major = 0, minor = 0;
                      if (sscanf_s(pv.c_str() + relPos + 8, "%d.%d", &major,
                                   &minor) == 2) {
                        if ((major == 4 || major == 5) && minor >= 0 &&
                            minor < 50) {
                          state.UEMajor = major;
                          state.UEMinor = minor;
                          Log("[Discovery] Detected UE%d.%d (from PE string: "
                              "%s)",
                              major, minor, pv.c_str());
                          FreeLibrary(hVersion);
                          return true;
                        } else {
                          Log("[Discovery] Parsed version out of bounds: %d.%d",
                              major, minor);
                        }
                      } else {
                        Log("[Discovery] sscanf_s failed to parse version "
                            "from: '%s'",
                            pv.c_str() + relPos + 8);
                      }
                    }
                  }
                }
              }
            }
          } else {
            Log("[Discovery] \\VarFileInfo\\Translation not found in "
                "VersionInfo");
          }

          VS_FIXEDFILEINFO *pFileInfo = nullptr;
          UINT len = 0;
          if (pVerQueryValueW(verData.data(), L"\\", (LPVOID *)&pFileInfo,
                              &len) &&
              pFileInfo) {
            const int major = HIWORD(pFileInfo->dwProductVersionMS);
            const int minor = LOWORD(pFileInfo->dwProductVersionMS);
            Log("[Discovery] VS_FIXEDFILEINFO dwProductVersionMS: %d.%d", major,
                minor);
            if ((major == 4 || major == 5) && minor >= 0 && minor < 50) {
              state.UEMajor = major;
              state.UEMinor = minor;
              Log("[Discovery] Detected UE%d.%d (from PE ProductVersion)",
                  major, minor);
              FreeLibrary(hVersion);
              return true;
            }
          } else {
            Log("[Discovery] Failed to read VS_FIXEDFILEINFO");
          }
        } else {
          Log("[Discovery] GetFileVersionInfoW failed");
        }
      }
    }

    FreeLibrary(hVersion);
    return false;
  };

  auto tryDetectFromReleaseString =
      [&state](const PatternScan::ModuleInfo &mod) -> bool {
    if (mod.size < 100) {
      return false;
    }

    uintptr_t found = 0;

    found = PatternScan::FindPatternInModule(
        mod.handle, "2D 52 65 6C 65 61 73 65 2D 35 2E");
    if (found) {
      state.UEMajor = 5;
      char minorBuf[8] = {};
      if (SafeMem_Read((void *)(found + 11), minorBuf, 4)) {
        state.UEMinor = atoi(minorBuf);
        if (state.UEMinor >= 0 && state.UEMinor < 20) {
          Log("[Discovery] Detected UE5.%d (from branch string)",
              state.UEMinor);
          return true;
        }
      }
    }

    found = PatternScan::FindPatternInModule(
        mod.handle,
        "2D 00 52 00 65 00 6C 00 65 00 61 00 73 00 65 00 2D 00 35 00 2E 00");
    if (found) {
      state.UEMajor = 5;
      char minorBuf[8] = {};
      if (SafeMem_Read((void *)(found + 22), minorBuf, 1) &&
          SafeMem_Read((void *)(found + 24), minorBuf + 1, 1)) {
        state.UEMinor = atoi(minorBuf);
        if (state.UEMinor >= 0 && state.UEMinor < 20) {
          Log("[Discovery] Detected UE5.%d (UTF-16 branch string)",
              state.UEMinor);
          return true;
        }
      }
    }

    found = PatternScan::FindPatternInModule(
        mod.handle, "2D 52 65 6C 65 61 73 65 2D 34 2E");
    if (found) {
      state.UEMajor = 4;
      char minorBuf[8] = {};
      if (SafeMem_Read((void *)(found + 11), minorBuf, 4)) {
        state.UEMinor = atoi(minorBuf);
        if (state.UEMinor > 0 && state.UEMinor < 50) {
          Log("[Discovery] Detected UE4.%d (from branch string)",
              state.UEMinor);
          return true;
        }
      }
    }

    found = PatternScan::FindPatternInModule(
      mod.handle,
      "2D 00 52 00 65 00 6C 00 65 00 61 00 73 00 65 00 2D 00 34 00 2E 00");
  if (found) {
    state.UEMajor = 4;
    char minorBuf[8] = {};
    if (SafeMem_Read((void *)(found + 22), minorBuf, 1) &&
        SafeMem_Read((void *)(found + 24), minorBuf + 1, 1)) {
      state.UEMinor = atoi(minorBuf);
      if (state.UEMinor > 0 && state.UEMinor < 50) {
        Log("[Discovery] Detected UE4.%d (UTF-16 branch string)",
            state.UEMinor);
        return true;
      }
    }
  }

  found = PatternScan::FindPatternInModule(
      mod.handle,
      "2B 2B 55 45 35 2B 52 65 6C 65 61 73 65 2D 35 2E");
  if (found) {
    state.UEMajor = 5;
    char minorBuf[8] = {};
    if (SafeMem_Read((void *)(found + 16), minorBuf, 4)) {
      state.UEMinor = atoi(minorBuf);
      if (state.UEMinor >= 0 && state.UEMinor < 20) {
        Log("[Discovery] Detected UE5.%d (++UE5+Release- marker)",
            state.UEMinor);
        return true;
      }
    }
  }

  found = PatternScan::FindPatternInModule(
      mod.handle, "2B 55 45 35 2B 52 65 6C 65 61 73 65 2D 35 2E");
  if (found) {
    state.UEMajor = 5;
    char minorBuf[8] = {};
    if (SafeMem_Read((void *)(found + 15), minorBuf, 4)) {
      state.UEMinor = atoi(minorBuf);
      if (state.UEMinor >= 0 && state.UEMinor < 20) {
        Log("[Discovery] Detected UE5.%d (+UE5+Release- marker)",
            state.UEMinor);
        return true;
      }
    }
  }

  found = PatternScan::FindPatternInModule(
      mod.handle,
      "2B 2B 55 45 34 2B 52 65 6C 65 61 73 65 2D 34 2E");
  if (found) {
    state.UEMajor = 4;
    char minorBuf[8] = {};
    if (SafeMem_Read((void *)(found + 16), minorBuf, 4)) {
      state.UEMinor = atoi(minorBuf);
      if (state.UEMinor > 0 && state.UEMinor < 50) {
        Log("[Discovery] Detected UE4.%d (++UE4+Release- marker)",
            state.UEMinor);
        return true;
      }
    }
  }

  found = PatternScan::FindPatternInModule(
      mod.handle, "2B 55 45 34 2B 52 65 6C 65 61 73 65 2D 34 2E");
  if (found) {
    state.UEMajor = 4;
    char minorBuf[8] = {};
    if (SafeMem_Read((void *)(found + 15), minorBuf, 4)) {
      state.UEMinor = atoi(minorBuf);
      if (state.UEMinor > 0 && state.UEMinor < 50) {
        Log("[Discovery] Detected UE4.%d (+UE4+Release- marker)",
            state.UEMinor);
        return true;
      }
    }
  }

  found = PatternScan::FindPatternInModule(
      mod.handle,
      "2B 00 55 00 45 00 34 00 2B 00 52 00 65 00 6C 00 65 00 61 00 "
      "73 00 65 00 2D 00 34 00 2E 00");
  if (found) {
    state.UEMajor = 4;
    char minorBuf[8] = {};
    if (SafeMem_Read((void *)(found + 30), minorBuf, 1) &&
        SafeMem_Read((void *)(found + 32), minorBuf + 1, 1)) {
      state.UEMinor = atoi(minorBuf);
      if (state.UEMinor > 0 && state.UEMinor < 50) {
        Log("[Discovery] Detected UE4.%d (UTF-16 +UE4+Release- marker)",
            state.UEMinor);
        return true;
      }
    }
  }

  return false;
};

  auto tryDetectGenericFallback =
      [&state](const PatternScan::ModuleInfo &mod) -> bool {
    if (mod.size < 100) {
      return false;
    }

    uintptr_t found =
        PatternScan::FindPatternInModule(mod.handle, "55 00 45 00 35 00");
    if (found) {
      state.UEMajor = 5;
      state.UEMinor = 0;
      Log("[Discovery] Detected UE5 (UTF-16 generic fallback)");
      return true;
    }

    found = PatternScan::FindPatternInModule(mod.handle, "55 45 35 2B");
    if (found) {
      state.UEMajor = 5;
      state.UEMinor = 0;
      Log("[Discovery] Detected UE5 (ASCII generic fallback)");
      return true;
    }

    found = PatternScan::FindPatternInModule(mod.handle, "55 00 45 00 34 00");
    if (found) {
      state.UEMajor = 4;
      state.UEMinor = 27;
      Log("[Discovery] Detected UE4 (UTF-16 generic fallback)");
      return true;
    }

    return false;
  };

  if (tryDetectFromExecutableVersion()) {
    return true;
  }

  auto modules = PatternScan::GetLoadedModules();
  const auto mainMod = PatternScan::GetMainModule();

  if (mainMod.handle && tryDetectFromReleaseString(mainMod)) {
    return true;
  }

  for (const auto &mod : modules) {
    if (mainMod.handle && mod.handle == mainMod.handle) {
      continue;
    }

    if (tryDetectFromReleaseString(mod)) {
      return true;
    }
  }

  if (mainMod.handle && tryDetectGenericFallback(mainMod)) {
    return true;
  }

  for (const auto &mod : modules) {
    if (mainMod.handle && mod.handle == mainMod.handle) {
      continue;
    }

    if (tryDetectGenericFallback(mod)) {
      return true;
    }
  }

  Log("[Discovery] Failed to detect UE version natively, defaulting to UE5.0 "
      "heuristically.");
  state.UEMajor = 5;
  state.UEMinor = 0;
  return true;
}

struct WritableSection {
  uintptr_t start;
  uintptr_t end;
};

static inline uint32_t RexPrefixPositions(const uint8_t *p) {
  const __m128i block = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
  const __m128i rex48 = _mm_cmpeq_epi8(block, _mm_set1_epi8(0x48));
  const __m128i rex4C = _mm_cmpeq_epi8(block, _mm_set1_epi8(0x4C));
  return static_cast<uint32_t>(_mm_movemask_epi8(_mm_or_si128(rex48, rex4C)));
}

static void ScanExecutableSectionSafe(uintptr_t start, uintptr_t end,
                                      const WritableSection *sections,
                                      int sectionCount,
                                      std::vector<uintptr_t> &out) {
  __try {
    uintptr_t scan = start;

    while (scan + 7 + 16 < end) {
      const uint32_t hits =
          RexPrefixPositions(reinterpret_cast<const uint8_t *>(scan));
      if (hits == 0) {
        scan += 16;
        continue;
      }
      unsigned long bit = 0;
      _BitScanForward(&bit, hits);
      scan += static_cast<uintptr_t>(bit);

      if (scan + 7 >= end)
        break;

      uint8_t b0 = *reinterpret_cast<const uint8_t *>(scan);
      if (b0 != 0x48 && b0 != 0x4C) {
        ++scan;
        continue;
      }
      uint8_t b1 = *reinterpret_cast<const uint8_t *>(scan + 1);
      if (b1 != 0x8B && b1 != 0x8D) {
        ++scan;
        continue;
      }
      uint8_t b2 = *reinterpret_cast<const uint8_t *>(scan + 2);
      if ((b2 & 0x07) != 0x05) {
        ++scan;
        continue;
      }

      int32_t rel32 = *reinterpret_cast<const int32_t *>(scan + 3);
      uintptr_t target = scan + 7 + rel32;
      if (target) {
        for (int k = 0; k < sectionCount; ++k) {
          if (target >= sections[k].start && target < sections[k].end) {
            out.push_back(target);
            break;
          }
        }
      }
      ++scan;
    }
    for (; scan + 7 < end; ++scan) {
      uint8_t b0 = *reinterpret_cast<const uint8_t *>(scan);
      if (b0 != 0x48 && b0 != 0x4C)
        continue;
      uint8_t b1 = *reinterpret_cast<const uint8_t *>(scan + 1);
      if (b1 != 0x8B && b1 != 0x8D)
        continue;
      uint8_t b2 = *reinterpret_cast<const uint8_t *>(scan + 2);
      if ((b2 & 0x07) != 0x05)
        continue;

      int32_t rel32 = *reinterpret_cast<const int32_t *>(scan + 3);
      uintptr_t target = scan + 7 + rel32;
      if (!target)
        continue;

      for (int k = 0; k < sectionCount; ++k) {
        if (target >= sections[k].start && target < sections[k].end) {
          out.push_back(target);
          break;
        }
      }
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {

  }
}

static void ReadKnownModuleData(const void *address, void *out, size_t size) {

  memcpy(out, address, size);
}

static bool HasPlausibleFUObjectArrayHeader(const void *candidate,
                                            bool candidateIsModuleData) {
  if (!candidate)
    return false;

  uint8_t header[0x60] = {};
  if (candidateIsModuleData) {
    ReadKnownModuleData(candidate, header, sizeof(header));
  } else {
    if (!IsMapped(candidate, sizeof(header)))
      return false;
    if (!SafeMem_Read(candidate, header, sizeof(header)))
      return false;
  }

  for (int ptrOff = 0; ptrOff <= 0x30; ptrOff += 8) {
    void *objects = nullptr;
    memcpy(&objects, header + ptrOff, sizeof(objects));
    const uintptr_t objectsAddr = reinterpret_cast<uintptr_t>(objects);
    if (objectsAddr < 0x10000 || objectsAddr >= 0x7FF000000000ULL)
      continue;

    bool chunkGeometryFound = false;
    const int countStart = ptrOff + 4;
    const int countEnd = (std::min)(0x50, ptrOff + 40);
    for (int maxOff = countStart; maxOff <= countEnd && !chunkGeometryFound;
         maxOff += 4) {
      if (maxOff == ptrOff || maxOff == ptrOff + 4)
        continue;
      const int32_t maxElements = *reinterpret_cast<const int32_t *>(header + maxOff);
      if (maxElements < 65536 || maxElements > 30000000 || (maxElements % 16) != 0)
        continue;

      for (int numOff = countStart; numOff <= countEnd; numOff += 4) {
        if (numOff == ptrOff || numOff == ptrOff + 4 || numOff == maxOff)
          continue;
        const int32_t numElements = *reinterpret_cast<const int32_t *>(header + numOff);
        if (numElements < 500 || numElements > maxElements)
          continue;

        static const int32_t kChunkSizes[] = {16384, 32768, 65536, 131072};
        for (const int32_t elementsPerChunk : kChunkSizes) {
          if (chunkGeometryFound)
            break;
          if ((maxElements % elementsPerChunk) != 0)
            continue;
          const int32_t maxChunks = maxElements / elementsPerChunk;
          if (maxChunks < 1 || maxChunks > 4000)
            continue;

          for (int maxChunksOff = countStart; maxChunksOff <= countEnd && !chunkGeometryFound;
               maxChunksOff += 4) {
            if (maxChunksOff == ptrOff || maxChunksOff == ptrOff + 4 ||
                maxChunksOff == maxOff || maxChunksOff == numOff)
              continue;
            if (*reinterpret_cast<const int32_t *>(header + maxChunksOff) != maxChunks)
              continue;

          for (int numChunksOff = countStart; numChunksOff <= countEnd && !chunkGeometryFound;
               numChunksOff += 4) {
            if (numChunksOff == ptrOff || numChunksOff == ptrOff + 4 ||
                numChunksOff == maxOff || numChunksOff == numOff ||
                numChunksOff == maxChunksOff)
              continue;
            const int32_t numChunks = *reinterpret_cast<const int32_t *>(header + numChunksOff);
            if (numChunks < 1 || numChunks > maxChunks)
              continue;

            const int32_t expectedChunks1 = (numElements + elementsPerChunk - 1) / elementsPerChunk;
            const int32_t expectedChunks2 = (numElements / elementsPerChunk) + 1;
            if (numChunks != expectedChunks1 && numChunks != expectedChunks2)
              continue;

            chunkGeometryFound = true;
          }
          }
        }
      }
    }

    if (!chunkGeometryFound)
      continue;

    void *firstChunk = SafeMem_ReadPtr(objects);
    if (!firstChunk)
      continue;
    const uintptr_t chunkAddr = reinterpret_cast<uintptr_t>(firstChunk);
    if (chunkAddr < 0x10000 || chunkAddr >= 0x7FF000000000ULL)
      continue;

    if (SafeMem_IsReadable(firstChunk, 0x20))
      return true;
  }

  return false;
}

namespace {

std::mutex g_StaticPointerBagMutex;
std::condition_variable g_StaticPointerBagCv;
std::vector<uintptr_t> g_StaticPointerBag;
bool g_StaticPointerBagReady = false;

void ResetStaticPointerBag() {
  std::lock_guard<std::mutex> lock(g_StaticPointerBagMutex);
  g_StaticPointerBag.clear();
  g_StaticPointerBagReady = false;
}

void PublishStaticPointerBag(const std::vector<uintptr_t> &bag) {
  {
    std::lock_guard<std::mutex> lock(g_StaticPointerBagMutex);
    g_StaticPointerBag = bag;
    g_StaticPointerBagReady = true;
  }
  g_StaticPointerBagCv.notify_all();
}

}

static void CanonicalizeObjectArrayBase(EngineState &state) {
  if (!state.GUObjectArray || state.TUObjectArray_ObjectsOff <= 0)
    return;

  void *alias = state.GUObjectArray;
  void *canonical = reinterpret_cast<uint8_t *>(alias) +
                    state.TUObjectArray_ObjectsOff;
  if (canonical == alias)
    return;

  state.GUObjectArray = canonical;
  if (!DiscoverTUObjectArrayLayout(state)) {
    state.GUObjectArray = alias;
    Log("[Discovery] GUObjectArray canonicalization failed; keeping alias %p",
        alias);
    return;
  }

  Log("[Discovery] GUObjectArray canonicalized %p -> %p (ObjOff=%d)",
      alias, state.GUObjectArray, state.TUObjectArray_ObjectsOff);
}

bool FindGUObjectArray(EngineState &state) {
  auto modules = PatternScan::GetLoadedModules();

  for (auto &mod : modules) {

    void *exported = PatternScan::FindExport(
        mod.handle, "?GUObjectArray@@3VFUObjectArray@@A");
    if (exported) {
      state.GUObjectArray = exported;
      Log("[Discovery] Found GUObjectArray via export: %p", exported);
      return true;
    }
  }

  struct PatternInfo {
    const char *pattern;
    int dispPos;
    int postAdjust;
  };

  PatternInfo patterns[] = {

      {"03 ?? ?? ?? ?? ?? FF C8 3B D0 0F 8D", 2, 0},

      {"8B 51 04 85 D2 74 5A 48 63 C1 48 8B 0D ?? ?? ?? ?? 48 85 C9 74 47 4C "
       "8B C7 4C 8B CB",
       29, -0x10},

      {"48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? E8", 3, -0x10},

      {"48 8B 05 ?? ?? ?? ?? 48 85 C0", 3, 0},

      {"48 89 1D ?? ?? ?? ?? 48 8B 5C 24", 3, 0},

      {"48 89 05 ?? ?? ?? ?? 48 83 C4", 3, 0},

      {"48 8D 0D ?? ?? ?? ?? E8", 3, -0x10},

      {"48 8B 1D ?? ?? ?? ?? 48 85 DB", 3, 0},

      {"48 3B ?? ?? ?? ?? ?? 0F 84", 3, 0},

      {"48 85 ?? ?? ?? ?? ?? 74", 3, 0},

      {"48 8B 05 ?? ?? ?? ?? 48 8B 0C C8 48 8D 04 D1", 3, 0},

      {"48 8B 05 ?? ?? ?? ?? 48 8B 0C C8 48 8B 04 D1", 3, 0},

      {"48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8D 43 13", 3, -0x10},

      {"48 8B 0D ?? ?? ?? ?? 48 81 C1 ?? ?? 00 00 48 8B 01", 3, 0},
  };

  const bool skipPatternScan = RuntimeConfig::Get().SkipPatternScan;
  if (!skipPatternScan) {

  constexpr DWORD PATTERN_SCAN_TIMEOUT_MS = 5000;
  const DWORD patternScanStart = GetTickCount64();
  bool patternScanTimedOut = false;

  for (auto &mod : modules) {
    if (mod.size < 100)
      continue;

    for (const auto &pi : patterns) {

      if (GetTickCount64() - patternScanStart > PATTERN_SCAN_TIMEOUT_MS) {
        patternScanTimedOut = true;
        break;
      }

      uintptr_t currentMatch = mod.base;
      int evalCount = 0;
      while (true) {

        uintptr_t found = PatternScan::FindPattern(
            currentMatch, mod.size - (currentMatch - mod.base), pi.pattern);
        if (!found)
          break;

        int32_t rel = 0;
        if (SafeMem_Read((void *)(found + pi.dispPos), &rel, 4)) {

          uintptr_t target = found + pi.dispPos + 4 + rel + pi.postAdjust;

          if (target > mod.base && target < mod.base + mod.size) {

            state.GUObjectArray = (void *)target;

            if (DiscoverTUObjectArrayLayout(state)) {
              Log("[Discovery] Found GUObjectArray via pattern: %p",
                  state.GUObjectArray);
              return true;
            }

            void *ptrVal = SafeMem_ReadPtr((void *)target);
            if (ptrVal && reinterpret_cast<uintptr_t>(ptrVal) > 0x10000 &&
                reinterpret_cast<uintptr_t>(ptrVal) < 0x7FFFFFFFFFFF) {
              state.GUObjectArray = ptrVal;
              if (DiscoverTUObjectArrayLayout(state)) {
                Log("[Discovery] Found GUObjectArray pointer via pattern: %p "
                    "-> %p",
                    (void *)target, ptrVal);
                return true;
              }
            }
          }
        }

        currentMatch = found + 1;
        evalCount++;
        if (evalCount > 100)
          break;

      }
    }

    if (patternScanTimedOut)
      break;
  }

  if (patternScanTimedOut) {
      Log("[Discovery] Default GUObjectArray scan exceeded %lu seconds, switching to obfuscation Logic",
          (unsigned long)(PATTERN_SCAN_TIMEOUT_MS / 1000));
  }

  } else {
    Log("[Discovery] SkipPatternScan enabled; skipping default AOB scan");
  }

  if (!skipPatternScan) {
    Log("[Discovery] Default GUObjectArray scan failed. Don't worry, let Avalonium cock!");
  } else {
    Log("[Discovery] Obfuscation scan active (SkipPatternScan=true)");
  }
  const auto mainMod = PatternScan::GetMainModule();
  if (mainMod.base) {
    uintptr_t base = mainMod.base;
    auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
      auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(
          base + static_cast<DWORD>(dos->e_lfanew));
      if (nt->Signature == IMAGE_NT_SIGNATURE) {
        std::vector<WritableSection> codeSections;
        std::vector<WritableSection> dataSections;

        const IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
          if (!section->Misc.VirtualSize || !section->VirtualAddress)
            continue;
          uintptr_t secBase = base + section->VirtualAddress;
          uintptr_t secEnd = secBase + section->Misc.VirtualSize;

          if (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            codeSections.push_back({secBase, secEnd});
          } else if (section->Characteristics & IMAGE_SCN_MEM_WRITE) {
            dataSections.push_back({secBase, secEnd});
          }
        }

        const unsigned int numCores =
            PatternScan::AcquireWorkerBudget(PatternScan::RecommendedWorkerCount(16));
        struct CodeChunkJob {
          uintptr_t start;
          uintptr_t end;
        };
        std::vector<CodeChunkJob> codeJobs;
        for (const auto &codeSec : codeSections) {
          if (codeSec.start >= codeSec.end) continue;
          size_t secLen = codeSec.end - codeSec.start;
          size_t chunkLen = (secLen + numCores - 1) / numCores;
          for (size_t c = 0; c < numCores; ++c) {
            uintptr_t cStart = codeSec.start + c * chunkLen;
            if (cStart >= codeSec.end) break;
            uintptr_t cEnd = std::min(codeSec.end, cStart + chunkLen + 15);
            codeJobs.push_back({cStart, cEnd});
          }
        }

        std::vector<std::vector<uintptr_t>> threadResults(codeJobs.size());
        std::vector<std::thread> scanThreads;
        scanThreads.reserve(codeJobs.size());

        for (size_t j = 0; j < codeJobs.size(); ++j) {
          scanThreads.emplace_back([&, j]() {
            FastMemScope fastMem;
            ScanExecutableSectionSafe(
                codeJobs[j].start, codeJobs[j].end, dataSections.data(),
                static_cast<int>(dataSections.size()), threadResults[j]);
          });
        }

        for (auto &t : scanThreads) {
          if (t.joinable()) t.join();
        }
        PatternScan::ReleaseWorkerBudget(numCores);

        size_t totalCount = 0;
        for (const auto &res : threadResults) {
          totalCount += res.size();
        }

        std::vector<uintptr_t> bag;
        bag.reserve(totalCount);
        for (auto &res : threadResults) {
          bag.insert(bag.end(), res.begin(), res.end());
        }

        const size_t rawCandidateCount = bag.size();
        PublishStaticPointerBag(bag);
        std::sort(std::execution::par, bag.begin(), bag.end());
        bag.erase(std::unique(bag.begin(), bag.end()), bag.end());

        Log("[Discovery] Obfuscation scan found %zu candidate static pointers "
            "(%zu unique targets), screening headers...",
            rawCandidateCount, bag.size());

        std::vector<void *> objectArrayCandidates;
        std::mutex candMutex;
        std::vector<std::thread> filterThreads;
        const size_t chunkSize = (bag.size() + numCores - 1) / numCores;

        for (unsigned int tc = 0; tc < numCores; ++tc) {
          filterThreads.emplace_back([&, tc]() {
            const size_t startIdx = tc * chunkSize;
            const size_t endIdx = std::min(bag.size(), startIdx + chunkSize);
            std::vector<void *> localCands;
            for (size_t i = startIdx; i < endIdx; ++i) {
              void *inlineCandidate = reinterpret_cast<void *>(bag[i]);
              if (HasPlausibleFUObjectArrayHeader(inlineCandidate, true)) {
                localCands.push_back(inlineCandidate);
              }
            }
            if (!localCands.empty()) {
              std::lock_guard<std::mutex> lock(candMutex);
              objectArrayCandidates.insert(objectArrayCandidates.end(), localCands.begin(), localCands.end());
            }
          });
        }
        for (auto &t : filterThreads) {
          if (t.joinable()) t.join();
        }

        std::sort(objectArrayCandidates.begin(), objectArrayCandidates.end());
        objectArrayCandidates.erase(
            std::unique(objectArrayCandidates.begin(),
                        objectArrayCandidates.end()),
            objectArrayCandidates.end());

        Log("[Discovery] Obfuscation direct-header filter retained %zu "
            "candidate%s; running full validation...",
            objectArrayCandidates.size(),
            objectArrayCandidates.size() == 1 ? "" : "s");

        for (void *candidate : objectArrayCandidates) {
          state.GUObjectArray = candidate;
          if (DiscoverTUObjectArrayLayout(state)) {
            CanonicalizeObjectArrayBase(state);
            Log("[Discovery] Found GUObjectArray via obfuscation scan: %p",
                state.GUObjectArray);
            return true;
          }
        }

        objectArrayCandidates.clear();
        std::vector<std::thread> indirectFilterThreads;
        for (unsigned int tc = 0; tc < numCores; ++tc) {
          indirectFilterThreads.emplace_back([&, tc]() {
            const size_t startIdx = tc * chunkSize;
            const size_t endIdx = std::min(bag.size(), startIdx + chunkSize);
            std::vector<void *> localCands;
            for (size_t i = startIdx; i < endIdx; ++i) {
              void *ptrVal = nullptr;
              ReadKnownModuleData(reinterpret_cast<void *>(bag[i]), &ptrVal, sizeof(ptrVal));
              if (!ptrVal) continue;
              const uintptr_t ptrValAddr = reinterpret_cast<uintptr_t>(ptrVal);
              if (ptrValAddr >= 0x10000 && ptrValAddr < 0x7FFFFFFFFFFFULL &&
                  HasPlausibleFUObjectArrayHeader(ptrVal, false)) {
                localCands.push_back(ptrVal);
              }
            }
            if (!localCands.empty()) {
              std::lock_guard<std::mutex> lock(candMutex);
              objectArrayCandidates.insert(objectArrayCandidates.end(), localCands.begin(), localCands.end());
            }
          });
        }
        for (auto &t : indirectFilterThreads) {
          if (t.joinable()) t.join();
        }

        std::sort(objectArrayCandidates.begin(), objectArrayCandidates.end());
        objectArrayCandidates.erase(
            std::unique(objectArrayCandidates.begin(),
                        objectArrayCandidates.end()),
            objectArrayCandidates.end());

        Log("[Discovery] Obfuscation indirect-header filter retained %zu "
            "candidate%s; "
            "running full validation...",
            objectArrayCandidates.size(),
            objectArrayCandidates.size() == 1 ? "" : "s");

        for (void *candidate : objectArrayCandidates) {
          state.GUObjectArray = candidate;
          if (DiscoverTUObjectArrayLayout(state)) {
            CanonicalizeObjectArrayBase(state);
            Log("[Discovery] Found GUObjectArray via obfuscation scan: %p",
                state.GUObjectArray);
            return true;
          }
        }
      }
    }
  }

  state.GUObjectArray = nullptr;
  Log("[Discovery] Failed to find GUObjectArray");
  return false;
}

bool IsValidFNamePool(EngineState &state, void *candidate) {
  if (!candidate || !IsReadable(candidate, 8))
    return false;

  int possibleOffsets[] = {0x10, 0x00, 0x08, 0x20, 0x40};
  for (int off : possibleOffsets) {
    void *testBlockAddr = (uint8_t *)candidate + off;
    if (!IsReadable(testBlockAddr, 8))
      continue;

    void *ptr1 = nullptr;
    if (!SafeMem_Read(testBlockAddr, &ptr1, sizeof(ptr1)) || !ptr1)
      continue;

    void *possibleBlock0s[2] = {ptr1, nullptr};
    if (IsReadable(ptr1, 8)) {
      void *ptr2 = nullptr;
      if (SafeMem_Read(ptr1, &ptr2, 8) && ptr2 && (uintptr_t)ptr2 > 0x10000 &&
          (uintptr_t)ptr2 < 0x00007FFFFFFFFFFF) {
        possibleBlock0s[1] = ptr2;
      }
    }

    for (void *testBlock : possibleBlock0s) {
      if (!testBlock)
        continue;

      char buf[64] = {0};
      if (!SafeMem_Read(testBlock, buf, 64))
        continue;

      bool foundNone = false;

      for (int k : {0, 2, 4, 6}) {
        uint16_t header = *(uint16_t *)(buf + k);
        if ((header & 1) != 0)
          continue;

        for (int shift : {1, 6}) {
          int mask = (shift == 1) ? 0x7FF : 0x3FF;
          int len = (header >> shift) & mask;

          if (len == 4 && memcmp(buf + k + 2, "None", 4) == 0) {
            state.FName_ChunksStart = off;
            state.FName_HeaderOff = k;
            state.FName_Stride = (k <= 2) ? 2 : 4;
            state.FName_LenShift = shift;
            state.FName_LenMask = mask;

            if (testBlock == possibleBlock0s[1]) {
              state.FName_IsIndirect = true;
            } else {
              state.FName_IsIndirect = false;
            }

            Log("[Discovery] Validated FNamePool candidate %p via 'None' at "
                "hdrOff=%d (chunksStart=0x%X, format=%c, block0=%p, "
                "indirect=%d)",
                candidate, k, off, shift == 1 ? 'B' : 'A', testBlock,
                state.FName_IsIndirect);
            return true;
          }
        }
      }
    }
  }
  return false;
}

bool FindGNames(EngineState &state) {
  auto modules = PatternScan::GetLoadedModules();

  for (auto &mod : modules) {
    void *exported = PatternScan::FindExport(
        mod.handle, "?GFNameTableForDebuggerVisualizers_MT@@"
                    "3PEAVFChunkedFixedUObjectArray@@EA");
    if (exported) {
      state.GNames = *(void **)exported;
      Log("[Discovery] Found GNames via debug export: %p", state.GNames);
      return true;
    }
  }

  auto mainMod = PatternScan::GetMainModule();

  const char *patterns[] = {
      "48 8D 05 ?? ?? ?? ?? EB ?? 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? C6 05",
      "48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 4C 8B C0 C6 05",
      "48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? C6 05 ?? ?? ?? ?? 01 48",
      "48 8D 05 ?? ?? ?? ?? EB ?? 48 8D 0D ?? ?? ?? ?? E8",
      "4C 8D 05 ?? ?? ?? ?? EB 16 48 8D 0D ?? ?? ?? ?? E8",
  };

  for (const char *pat : patterns) {
      uintptr_t found = PatternScan::FindPatternInExecutableSections(mainMod.handle, pat);
      if (found) {
        for (int off = 0; off < 24; off++) {
          if (found + off + 7 > mainMod.base + mainMod.size)
            continue;
          uint8_t b1 = *(uint8_t *)(found + off);
          uint8_t b2 = *(uint8_t *)(found + off + 1);
          if ((b1 == 0x48 || b1 == 0x4C) && b2 == 0x8D) {
            int32_t rel = *(int32_t *)(found + off + 3);
            uintptr_t target = found + off + 7 + rel;

            if (target > mainMod.base && target < mainMod.base + mainMod.size) {
              if (IsValidFNamePool(state, (void *)target)) {
                state.GNames = (void *)target;

                void *firstPointer = nullptr;
                if (SafeMem_Read((uint8_t *)state.GNames +
                                     state.FName_ChunksStart,
                                 &firstPointer, 8) &&
                    firstPointer) {
                  void *derefPointer = nullptr;
                  if (SafeMem_Read(firstPointer, &derefPointer, 8) &&
                      derefPointer && (uintptr_t)derefPointer > 0x10000 &&
                      (uintptr_t)derefPointer < 0x00007FFFFFFFFFFF) {
                    void *secondPointer = nullptr;
                    SafeMem_Read((uint8_t *)firstPointer + 8, &secondPointer,
                                 8);
                    if (!secondPointer ||
                        ((uintptr_t)secondPointer > 0x10000 &&
                         (uintptr_t)secondPointer < 0x00007FFFFFFFFFFF)) {
                      state.FName_IsIndirect = true;
                    }
                  }
                }

                Log("[Discovery] Found valid GNames/FNamePool via pattern: %p",
                    state.GNames);
                return true;
              }
            }
          }
        }
      }
  }

  Log("[Discovery] Pattern scan failed, starting BSS scan for FNamePool...");
  SafeMem_SetFastMode(1);
  constexpr DWORD FNAME_BSS_TIMEOUT_MS = 5000;
  const DWORD fnBssStart = GetTickCount64();
  IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)mainMod.base;
  IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(mainMod.base + dos->e_lfanew);
  IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);

  for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
    if (GetTickCount64() - fnBssStart > FNAME_BSS_TIMEOUT_MS)
      break;
    bool isData = (sec->Characteristics & IMAGE_SCN_MEM_WRITE) != 0;
    if (!isData)
      continue;

    uintptr_t secStart = mainMod.base + sec->VirtualAddress;
    uintptr_t secSize = sec->Misc.VirtualSize;

    for (uintptr_t addr = secStart; addr < secStart + secSize - 16; addr += 16) {
      if (GetTickCount64() - fnBssStart > FNAME_BSS_TIMEOUT_MS)
        break;
      void *blockPtrA = *(void **)addr;
      void *blockPtrB = *(void **)(addr + 8);
      for (int slot = 0; slot < 2; slot++) {
        void *blockPtr = slot == 0 ? blockPtrA : blockPtrB;
        if (!blockPtr)
          continue;
        uintptr_t blockAddr = (uintptr_t)blockPtr;
        if (blockAddr < 0x10000 || blockAddr > 0x00007FFFFFFFFFFF)
          continue;

        if (!IsMapped(blockPtr, 64))
          continue;

        char buf[64] = {0};
        if (!SafeMem_Read(blockPtr, buf, 64))
          continue;

        bool foundNone = false;
        char *buffersToTest[2] = {buf, nullptr};
        char buf2[64] = {0};
        void *innerPtr = nullptr;
        if (SafeMem_Read(blockPtr, &innerPtr, 8) && innerPtr &&
            (uintptr_t)innerPtr > 0x10000 &&
            (uintptr_t)innerPtr < 0x00007FFFFFFFFFFF &&
            IsMapped(innerPtr, 64)) {
          if (SafeMem_Read(innerPtr, buf2, 64))
            buffersToTest[1] = buf2;
        }
        for (char *testBuf : buffersToTest) {
          if (!testBuf)
            continue;
          for (int k : {0, 2, 4, 6}) {
            uint16_t header = *(uint16_t *)(testBuf + k);
            if ((header & 1) != 0)
              continue;
            for (int shift : {1, 6}) {
              int mask = (shift == 1) ? 0x7FF : 0x3FF;
              int len = (header >> shift) & mask;
              if (len == 4 && memcmp(testBuf + k + 2, "None", 4) == 0) {
                foundNone = true;
                break;
              }
            }
            if (foundNone)
              break;
          }
          if (foundNone)
            break;
        }
        if (foundNone) {
          int possibleOffsets[] = {0x10, 0x00, 0x08, 0x20, 0x40};
          for (int off : possibleOffsets) {
            void *candidate = (void *)(addr + slot * 8 - off);
            if (IsValidFNamePool(state, candidate)) {
              state.GNames = candidate;
              SafeMem_SetFastMode(0);
              Log("[Discovery] Found valid GNames/FNamePool via BSS scan: %p",
                  state.GNames);
              return true;
            }
          }
        }
      }
    }
  }
  SafeMem_SetFastMode(0);
  return false;
}

bool FindGWorld(EngineState &state) {

  auto modules = PatternScan::GetLoadedModules();

  for (auto &mod : modules) {
    void *exported =
        PatternScan::FindExport(mod.handle, "?GWorld@@3PEAVUWorld@@EA");
    if (exported) {
      state.GWorldPtr = (void **)exported;
      state.GWorld = *(void **)exported;
      Log("[Discovery] Found GWorld via export: ptr=%p, current=%p", exported,
          state.GWorld);
      return true;
    }
  }

  auto mainMod = PatternScan::GetMainModule();
  const char
      *patterns[] =
          {
              "48 8B 1D ?? ?? ?? ?? 48 85 DB 74 ?? 41 B0 01",

              "48 8B 05 ?? ?? ?? ?? 48 8B 88 ?? ?? ?? ?? 48 85 C9 74 ?? 48 8B "
              "49",
              "48 8B 0D ?? ?? ?? ?? 48 85 C9 74 ?? E8",

              "48 8B 15 ?? ?? ?? ?? 48 85 D2 0F 84",
          };

  for (auto &mod : modules) {
    for (const char *pat : patterns) {
      uintptr_t found = PatternScan::FindPatternInExecutableSections(mod.handle, pat);
      if (found) {
        int32_t rel = *(int32_t *)(found + 3);
        uintptr_t target = found + 7 + rel;
        if (target > mod.base && target < mod.base + mod.size) {
          void *worldPtr = *(void **)target;
          if (worldPtr && IsReadable(worldPtr, 8)) {
            state.GWorldPtr = (void **)target;
            state.GWorld = worldPtr;
            Log("[Discovery] Found GWorld via pattern: ptr=%p, current=%p",
                (void *)target, state.GWorld);
            return true;
          }
        }
      }
    }
  }

  Log("[Discovery] Trying GEngine->WorldContexts approach...");

  void *gEnginePtr = nullptr;

  for (auto &mod : modules) {

    const char *engineExports[] = {
        "?GEngine@@3PEAVUEngine@@EA",
        "GEngine"};

    for (const char *exportName : engineExports) {
      void *exported = PatternScan::FindExport(mod.handle, exportName);
      if (exported && IsReadable(exported, 8)) {
        gEnginePtr = *(void **)exported;
        if (gEnginePtr && IsReadable(gEnginePtr, 0x100)) {
          Log("[Discovery] Found GEngine via export '%s': %p", exportName,
              gEnginePtr);
          break;
        }
      }
    }
    if (gEnginePtr)
      break;
  }

  if (!gEnginePtr) {
    const char
        *
            enginePatterns
                [] =
                    {
                        "48 8B 0D ?? ?? ?? ?? 48 85 C9 0F 84 ?? ?? ?? ?? 48 8B "
                        "89 ?? ?? ?? ??",
                        "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B 80 ?? ?? ?? "
                        "??",
                    };

    for (auto &mod : modules) {
      for (const char *pat : enginePatterns) {
        uintptr_t found = PatternScan::FindPatternInExecutableSections(mod.handle, pat);
        if (found) {
          int32_t rel = *(int32_t *)(found + 3);
          uintptr_t target = found + 7 + rel;
          if (target > mod.base && target < mod.base + mod.size) {
            void *enginePtr = *(void **)target;
            if (enginePtr && IsReadable(enginePtr, 0x100)) {
              gEnginePtr = enginePtr;
              Log("[Discovery] Found GEngine via pattern: %p", gEnginePtr);
              break;
            }
          }
        }
      }
      if (gEnginePtr)
        break;
    }
  }

  if (gEnginePtr) {

    uint8_t engineBuf[0x300];
    if (SafeMem_Read(gEnginePtr, engineBuf, sizeof(engineBuf))) {

      for (int32_t off = 0x80; off < 0x280; off += 8) {
        void *arrayData = *(void **)(engineBuf + off);
        int32_t arrayNum = *(int32_t *)(engineBuf + off + 8);
        int32_t arrayMax = *(int32_t *)(engineBuf + off + 12);

        if (arrayNum > 0 && arrayNum <= 16 && arrayMax >= arrayNum &&
            arrayData && IsReadable(arrayData, arrayNum * 8)) {

          uint8_t contextBuf[0x200];
          if (SafeMem_Read(arrayData, contextBuf, sizeof(contextBuf))) {

            for (int32_t worldOff = 0x00; worldOff < 0x100; worldOff += 8) {
              void *potentialWorld = *(void **)(contextBuf + worldOff);

              if (!potentialWorld || !IsReadable(potentialWorld, 0x100))
                continue;

              UClass *cls = GetObjectClass(state, (UObject *)potentialWorld);
              if (!cls)
                continue;

              std::string className = GetObjectNameStr(state, (UObject *)cls);
              if (className == "World") {
                state.GWorld = potentialWorld;
                state.GWorldPtr =
                    nullptr;
                Log("[Discovery] Found UWorld via GEngine->WorldContexts: %p "
                    "(offset in GEngine: 0x%X, offset in context: 0x%X)",
                    potentialWorld, off, worldOff);
                return true;
              }
            }
          }
        }
      }
    }
  }

  Log("[Discovery] Trying GWorld memory scan fallback...");

  UClass *worldClass = nullptr;
  int32_t count = GetObjectCount(state);

  for (int i = 0; i < count && i < 5000; i++) {
    UObject *obj = GetObjectByIndex(state, i);
    if (!obj)
      continue;

    std::string name = GetObjectNameStr(state, obj);
    if (name == "World") {
      worldClass = (UClass *)obj;
      Log("[Discovery] Found World class at index %d", i);
      break;
    }
  }

  if (!worldClass) {
    Log("[Discovery] GWorld memory scan: World class not found");
    return false;
  }

  for (int i = 0; i < count; i++) {
    UObject *obj = GetObjectByIndex(state, i);
    if (!obj)
      continue;

    uint32_t flags = *(uint32_t *)((uint8_t *)obj + state.UObject_FlagsOff);
    if (flags & 0x00000010)
      continue;

    UClass *cls = GetObjectClass(state, obj);
    if (cls != worldClass)
      continue;

    uintptr_t worldAddr = (uintptr_t)obj;
    Log("[Discovery] Found UWorld instance at %p, scanning for static "
        "pointers...",
        (void *)worldAddr);

    std::vector<void *> candidates;

    uintptr_t current = mainMod.base;
    uintptr_t end = mainMod.base + mainMod.size;

    while (current < end && candidates.size() < 10) {
      MEMORY_BASIC_INFORMATION mbi = {};
      if (VirtualQuery((LPCVOID)current, &mbi, sizeof(mbi)) == 0)
        break;

      uintptr_t regionBase = (uintptr_t)mbi.BaseAddress;
      uintptr_t regionEnd = regionBase + mbi.RegionSize;

      bool isWritableData = (mbi.State == MEM_COMMIT) &&
                            !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
                            (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY));

      if (isWritableData) {
        uintptr_t scanStart = (current > regionBase) ? current : regionBase;
        uintptr_t scanEnd = (regionEnd < end) ? regionEnd : end;

        for (uintptr_t addr = scanStart; addr < scanEnd - 8; addr += 8) {
          if (!IsReadable((void *)addr, 8))
            continue;

          uintptr_t val = *(uintptr_t *)addr;
          if (val == worldAddr) {
            candidates.push_back((void *)addr);
            if (candidates.size() >= 10)
              break;
          }
        }
      }

      current = regionEnd;
    }

    if (candidates.empty())
      continue;

    void *result = nullptr;

    if (candidates.size() == 1) {
      result = candidates[0];
    } else if (candidates.size() >= 2) {

      uintptr_t first = *(uintptr_t *)candidates[0];
      Sleep(50);
      uintptr_t firstNow = *(uintptr_t *)candidates[0];

      if (first != firstNow) {
        result = candidates[0];
        Log("[Discovery] Selected changing pointer (likely GWorld)");
      } else {

        for (void *candidate : candidates) {
          if (*(void **)candidate == obj) {
            result = candidate;
            break;
          }
        }
        if (!result)
          result = candidates[0];
      }
    }

    if (result) {
      state.GWorldPtr = (void **)result;
      state.GWorld = *(void **)result;
      Log("[Discovery] Found GWorld via memory scan: ptr=%p, current=%p",
          result, state.GWorld);
      return true;
    }
  }

  Log("[Discovery] Failed to find GWorld");
  return false;
}

struct RipTargetStats {
  int refCount = 0;
};

static void CollectRipRelativeTargets(const PatternScan::ModuleInfo &module,
                                      std::unordered_map<uintptr_t, RipTargetStats> &stats) {
  const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(module.base);
  if (!module.base || !module.size || !SafeMem_IsReadable(dos, sizeof(*dos)) ||
      dos->e_magic != IMAGE_DOS_SIGNATURE)
    return;
  const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(
      module.base + static_cast<DWORD>(dos->e_lfanew));
  if (!SafeMem_IsReadable(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE)
    return;

  auto sectionList = IMAGE_FIRST_SECTION(nt);

  struct SectionRange { uintptr_t start; uintptr_t end; };
  std::vector<SectionRange> writableRanges;
  writableRanges.reserve(8);
  for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
    const auto &sec = sectionList[i];
    if ((sec.Characteristics & IMAGE_SCN_MEM_WRITE) && sec.Misc.VirtualSize > 0) {
      writableRanges.push_back({ module.base + sec.VirtualAddress,
                                 module.base + sec.VirtualAddress + sec.Misc.VirtualSize });
    }
  }
  if (writableRanges.empty())
    return;

  std::vector<SectionRange> execRanges;
  execRanges.reserve(8);
  for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
    const auto &sec = sectionList[i];
    if ((sec.Characteristics & IMAGE_SCN_MEM_EXECUTE) && sec.Misc.VirtualSize > 0) {
      execRanges.push_back({ module.base + sec.VirtualAddress,
                             module.base + sec.VirtualAddress + sec.Misc.VirtualSize });
    }
  }
  if (execRanges.empty())
    return;

  constexpr uintptr_t kSliceSize = 4 * 1024 * 1024;
  struct WorkUnit { uintptr_t start; uintptr_t end; };
  std::vector<WorkUnit> workUnits;
  workUnits.reserve(128);
  for (const auto &sec : execRanges) {
    for (uintptr_t cur = sec.start; cur < sec.end; cur += kSliceSize) {
      const uintptr_t sliceEnd = (std::min)(cur + kSliceSize, sec.end);
      workUnits.push_back({ cur, sliceEnd });
    }
  }

  uintptr_t minWritable = UINTPTR_MAX;
  uintptr_t maxWritable = 0;
  for (const auto &r : writableRanges) {
    if (r.start < minWritable) minWritable = r.start;
    if (r.end > maxWritable) maxWritable = r.end;
  }

  const unsigned int numThreads =
      PatternScan::AcquireWorkerBudget(PatternScan::RecommendedWorkerCount(16));
  std::vector<std::unordered_map<uintptr_t, RipTargetStats>> threadMaps(numThreads);
  for (auto &m : threadMaps) {
    m.max_load_factor(0.7f);
    m.reserve(1u << 15);
  }

  auto processRipCandidate =
      [&](const uint8_t *scan,
          std::unordered_map<uintptr_t, RipTargetStats> &localMap) {
        const int32_t disp = *reinterpret_cast<const int32_t *>(scan + 3);
        const uintptr_t target =
            reinterpret_cast<uintptr_t>(scan + 7) + static_cast<int64_t>(disp);

        if (target < minWritable || target >= maxWritable)
          return;

        bool inWritable = false;
        for (const auto &r : writableRanges) {
          if (target >= r.start && target < r.end) {
            inWritable = true;
            break;
          }
        }
        if (!inWritable)
          return;

        ++localMap[target].refCount;
      };

  auto scanSlice = [&](size_t sliceIdx, unsigned int threadId) {
    const auto &unit = workUnits[sliceIdx];
    auto &localMap = threadMaps[threadId];

    __try {
      const uint8_t *pStart = reinterpret_cast<const uint8_t *>(unit.start);
      const uint8_t *pEnd = reinterpret_cast<const uint8_t *>(unit.end);

      const uint8_t *scan = pStart;

      while (scan + 7 + 16 <= pEnd) {
        const uint32_t hits = RexPrefixPositions(scan);
        if (hits == 0) {
          scan += 16;
          continue;
        }
        unsigned long bit = 0;
        _BitScanForward(&bit, hits);
        scan += static_cast<size_t>(bit);

        if (scan + 7 > pEnd)
          break;
        if (*scan != 0x48 && *scan != 0x4C) {
          ++scan;
          continue;
        }
        if (*(scan + 1) != 0x8B || (*(scan + 2) & 0xC7) != 0x05) {
          ++scan;
          continue;
        }
        processRipCandidate(scan, localMap);
        ++scan;
      }
      for (; scan + 7 <= pEnd; ++scan) {
        const uint8_t rex = *scan;
        if (rex != 0x48 && rex != 0x4C)
          continue;
        const uint8_t op = *(scan + 1);
        if (op != 0x8B)
          continue;
        const uint8_t modrm = *(scan + 2);
        if ((modrm & 0xC7) != 0x05)
          continue;

        processRipCandidate(scan, localMap);
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
  };

  if (numThreads <= 1 || workUnits.size() <= 1) {
    for (size_t i = 0; i < workUnits.size(); ++i) {
      scanSlice(i, 0);
    }
  } else {
    std::atomic<size_t> nextUnit(0);
    std::vector<std::thread> workers;
    workers.reserve(numThreads);
    for (unsigned int t = 0; t < numThreads; ++t) {
      workers.emplace_back([&, t]() {
        while (true) {
          size_t idx = nextUnit.fetch_add(1, std::memory_order_relaxed);
          if (idx >= workUnits.size())
            break;
          scanSlice(idx, t);
        }
      });
    }
    for (auto &w : workers) {
      w.join();
    }
  }
  PatternScan::ReleaseWorkerBudget(numThreads);

  size_t totalTargets = 0;
  for (const auto &lm : threadMaps) totalTargets += lm.size();
  stats.reserve(totalTargets);
  for (const auto &lm : threadMaps) {
    for (const auto &[target, stat] : lm) {
      stats[target].refCount += stat.refCount;
    }
  }
}

static int ChooseMallocSlot(const uint16_t *slotCalls,
                            const uint16_t *alignmentCalls) {
  constexpr size_t kMaxSlot = 8;
  char votes[192] = {};
  size_t used = 0;
  for (size_t slot = 1; slot < kMaxSlot; ++slot) {
    if (slotCalls[slot] == 0 || used >= sizeof(votes))
      continue;
    used += static_cast<size_t>(snprintf(
        votes + used, sizeof(votes) - used, "%s%zu=%u/%u", used ? ", " : "",
        slot, alignmentCalls[slot], slotCalls[slot]));
  }
  for (size_t slot = 1; slot < kMaxSlot; ++slot) {
    if (alignmentCalls[slot] < 2)
      continue;
    if (IsVerboseLoggingEnabled()) {
      Log("[Discovery] GMalloc: Malloc slot %d chosen (%u call sites pass a "
          "constant alignment; slot alignment/calls: %s)",
          static_cast<int>(slot), alignmentCalls[slot], votes);
    }
    return static_cast<int>(slot);
  }
  Log("[Discovery] GMalloc: Malloc slot unresolved (no vtable slot has two "
      "constant-alignment call sites; slot alignment/calls: %s); engine "
      "allocation stays disabled",
      votes);
  return -1;
}

static int MeasureAllocatorMallocSlot(const PatternScan::ModuleInfo &module,
                                      uintptr_t target) {
  constexpr size_t kMaxSlot = 8;

  if (!module.base || !module.size)
    return -1;
  const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(module.base);
  if (!SafeMem_IsReadable(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE)
    return -1;
  const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(
      module.base + static_cast<DWORD>(dos->e_lfanew));
  if (!SafeMem_IsReadable(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE)
    return -1;

  uint16_t calls[kMaxSlot] = {};
  uint16_t aligned[kMaxSlot] = {};
  constexpr size_t kChunkSize = 256 * 1024;
  std::vector<uint8_t> code(kChunkSize);
  const uint64_t scanStart = GetTickCount64();
  uint64_t scannedBytes = 0;

  auto sectionList = IMAGE_FIRST_SECTION(nt);
  for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
    const auto &sec = sectionList[i];
    if (!(sec.Characteristics & IMAGE_SCN_MEM_EXECUTE) || sec.Misc.VirtualSize == 0)
      continue;
    const uintptr_t start = module.base + sec.VirtualAddress;
    const uintptr_t end = start + sec.Misc.VirtualSize;

    for (uintptr_t cursor = start; cursor + 7 <= end;) {
      const size_t toRead = static_cast<size_t>(
          (std::min)(end - cursor, static_cast<uintptr_t>(kChunkSize)));
      const size_t got = SafeMem_ReadSpan(reinterpret_cast<const void *>(cursor),
                                          code.data(), toRead);
      if (got < 7) {

        const uintptr_t nextPage = (cursor + 0x1000) & ~static_cast<uintptr_t>(0xFFF);
        cursor = nextPage > cursor ? nextPage : cursor + 0x1000;
        continue;
      }
      scannedBytes += got;

      for (size_t j = 0; j + 7 <= got; ++j) {
        const uint8_t rex = code[j];
        if (rex != 0x48 && rex != 0x4C)
          continue;
        if (code[j + 1] != 0x8B || (code[j + 2] & 0xC7) != 0x05)
          continue;
        int32_t disp = 0;
        memcpy(&disp, code.data() + j + 3, sizeof(disp));
        if (cursor + j + 7 + static_cast<int64_t>(disp) != target)
          continue;

        const int reg = ((code[j + 2] >> 3) & 0x07) | (rex == 0x4C ? 0x08 : 0x00);
        AllocatorCallSiteVote vote;
        if (AnalyzeAllocatorCallSite(cursor + j, end, reg, vote) &&
            vote.slot >= 0 && vote.slot < static_cast<int>(kMaxSlot)) {
          if (calls[vote.slot] < 0xFFFF)
            ++calls[vote.slot];
          if (vote.constantAlignment && aligned[vote.slot] < 0xFFFF)
            ++aligned[vote.slot];
        }
      }

      if (got < toRead) {
        const uintptr_t nextPage = (cursor + got + 0x1000) & ~static_cast<uintptr_t>(0xFFF);
        cursor = nextPage > cursor ? nextPage : cursor + got;
        continue;
      }
      if (toRead < kChunkSize)
        break;
      cursor += toRead - 7;
    }
  }

  if (IsVerboseLoggingEnabled()) {
    Log("[Discovery] GMalloc: slot scan covered %llu bytes in %llu ms",
        static_cast<unsigned long long>(scannedBytes),
        static_cast<unsigned long long>(GetTickCount64() - scanStart));
  }

  return ChooseMallocSlot(calls, aligned);
}

static bool FindAllocatorDescriptiveName(void *instance,
                                         char (&outName)[64], int &outSlot);

static bool IsValidMallocInstanceWithModules(
    void *instance, const std::vector<PatternScan::ModuleInfo> &modules) {
  if (!instance || !SafeMem_IsCanonical(instance))
    return false;

  const uintptr_t instAddr = reinterpret_cast<uintptr_t>(instance);

  for (const auto &mod : modules) {
    if (instAddr >= mod.base && instAddr < mod.base + mod.size)
      return false;
  }

  if (!SafeMem_IsMapped(instance, sizeof(void *)))
    return false;

  void *vtable = SafeMem_ReadPtr(instance);
  if (!vtable || !SafeMem_IsCanonical(vtable))
    return false;

  const uintptr_t vtAddr = reinterpret_cast<uintptr_t>(vtable);
  bool vtableInModule = false;
  for (const auto &mod : modules) {
    if (vtAddr >= mod.base && vtAddr < mod.base + mod.size) {
      vtableInModule = true;
      break;
    }
  }
  if (!vtableInModule)
    return false;

  for (int slot = 0; slot < 4; ++slot) {
    void *fn = SafeMem_ReadPtr(static_cast<void **>(vtable) + slot);
    if (!fn || !SafeMem_IsCanonical(fn))
      return false;
    const uintptr_t fnAddr = reinterpret_cast<uintptr_t>(fn);
    bool fnInModule = false;
    for (const auto &mod : modules) {
      if (fnAddr >= mod.base && fnAddr < mod.base + mod.size) {
        fnInModule = true;
        break;
      }
    }
    if (!fnInModule)
      return false;
  }

  return true;
}

bool IsValidMallocInstance(void *instance) {
  std::vector<PatternScan::ModuleInfo> fastModules = { PatternScan::GetMainModule() };
  if (!IsValidMallocInstanceWithModules(instance, fastModules)) {
    if (!IsValidMallocInstanceWithModules(instance,
                                          PatternScan::GetLoadedModules()))
      return false;
  }
  char name[64];
  int slot = -1;
  return FindAllocatorDescriptiveName(instance, name, slot);
}

bool GetAllocatorDescriptiveName(void *instance, char (&outName)[64],
                                 int &outSlot) {
  return FindAllocatorDescriptiveName(instance, outName, outSlot);
}

static bool IsKnownAllocatorName(const char *name) {
  static const char *kKnown[] = {
      "binned", "binned2", "binned3", "binned4",
      "binnedarena", "binnedgpu", "ansi", "tbb", "mimalloc", "stomp",
  };
  if (!name[0])
    return false;
  for (const char *known : kKnown) {
    size_t i = 0;
    for (; known[i] && name[i]; ++i) {
      char a = name[i], b = known[i];
      if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
      if (a != b)
        break;
    }
    if (known[i] == 0 && name[i] == 0)
      return true;
  }
  return false;
}

static bool FindAllocatorDescriptiveName(void *instance,
                                         char (&outName)[64], int &outSlot) {
  outName[0] = 0;
  outSlot = -1;
  if (!instance || !SafeMem_IsCanonical(instance))
    return false;
  void *vtable = SafeMem_ReadPtr(instance);
  if (!vtable)
    return false;
  for (int slot = 0; slot < 48; ++slot) {
    void *fn = SafeMem_ReadPtr(static_cast<void **>(vtable) + slot);
    if (!fn || !SafeMem_IsCanonical(fn))
      continue;
    uint8_t prologue[8] = {};
    if (!SafeMem_Read(fn, prologue, sizeof(prologue)))
      continue;
    if (prologue[0] != 0x48 || prologue[1] != 0x8D || prologue[2] != 0x05 ||
        prologue[7] != 0xC3)
      continue;
    int32_t disp = 0;
    memcpy(&disp, prologue + 3, sizeof(disp));
    const void *literal =
        static_cast<const uint8_t *>(fn) + 7 + static_cast<int64_t>(disp);
    wchar_t name[32] = {};
    if (!SafeMem_Read(literal, name, sizeof(name) - sizeof(wchar_t)))
      continue;
    name[31] = 0;
    if (name[0]) {
      WideCharToMultiByte(CP_UTF8, 0, name, -1, outName,
                          static_cast<int>(sizeof(outName)), nullptr, nullptr);
      if (IsKnownAllocatorName(outName)) {
        outSlot = slot;
        return true;
      }
    }
  }
  return false;
}

void *ProbeAllocatorMalloc(void *instance, int32_t slot, size_t bytes) {
  if (!instance || slot < 0 || bytes == 0)
    return nullptr;
  return SafeMem_CallMallocVftable(instance, bytes, 0, slot);
}

static void VerifyAllocatorMallocSlot(EngineState &state) {
  if (state.GMallocMallocSlot < 0 || !state.GMalloc)
    return;
  void *probe =
      ProbeAllocatorMalloc(state.GMalloc, state.GMallocMallocSlot, 64);
  if (!probe) {
    Log("[Discovery] GMalloc: Malloc slot %d self-test failed; engine "
        "allocation stays disabled",
        state.GMallocMallocSlot);
    state.GMallocMallocSlot = -1;
    return;
  }
  if (IsVerboseLoggingEnabled()) {
    Log("[Discovery] GMalloc: Malloc slot %d self-test allocated 64 bytes at "
        "%p (writable)",
        state.GMallocMallocSlot, probe);
  }
}

bool FindGMalloc(EngineState &state) {

  {
    const char *exportNames[] = {
        "?GMalloc@@3PEAVFMalloc@@EA",
        "?GMalloc@Private@UE@@3PEAVFMalloc@@EA",
    };
    const auto stage1Modules = PatternScan::GetLoadedModules();
    for (const auto &mod : stage1Modules) {
      for (const char *name : exportNames) {
        void *exported = PatternScan::FindExport(mod.handle, name);
        if (!exported)
          continue;
        void *value = nullptr;
        if (!SafeMem_Read(exported, &value, sizeof(value)))
          continue;
        if (IsValidMallocInstanceWithModules(value, stage1Modules)) {
          char allocName[64] = {};
          int nameSlot = -1;
          if (!FindAllocatorDescriptiveName(value, allocName, nameSlot))
            continue;
          state.GMallocLocation = exported;
          state.GMalloc = value;
          strncpy(state.GMallocName, allocName, sizeof(state.GMallocName) - 1);
          state.GMallocMallocSlot =
              MeasureAllocatorMallocSlot(mod, reinterpret_cast<uintptr_t>(exported));
          VerifyAllocatorMallocSlot(state);
          Log("[Discovery] GMalloc: found export %s in %s at %p (instance %p)",
              name, mod.name.c_str(), exported, value);
          Log("[Discovery] GMalloc: allocator self-identifies as \"%s\" (vtable slot %d)",
              allocName, nameSlot);
          return true;
        }
        if (SafeMem_IsCanonical(value)) {
          void *indirect = nullptr;
          if (SafeMem_Read(value, &indirect, sizeof(indirect)) &&
              IsValidMallocInstanceWithModules(indirect, stage1Modules)) {
            char allocName[64] = {};
            int nameSlot = -1;
            if (!FindAllocatorDescriptiveName(indirect, allocName, nameSlot))
              continue;
            state.GMallocLocation = value;
            state.GMalloc = indirect;
            strncpy(state.GMallocName, allocName, sizeof(state.GMallocName) - 1);
            state.GMallocMallocSlot =
                MeasureAllocatorMallocSlot(mod, reinterpret_cast<uintptr_t>(value));
            VerifyAllocatorMallocSlot(state);
            Log("[Discovery] GMalloc: found export %s in %s at %p "
                "(reference alias -> global %p, instance %p)",
                name, mod.name.c_str(), exported, value, indirect);
            Log("[Discovery] GMalloc: allocator self-identifies as \"%s\" (vtable slot %d)",
                allocName, nameSlot);
            return true;
          }
        }
      }
    }
  }

  {
    auto modules = PatternScan::GetLoadedModules();
    const HMODULE mainHandle = PatternScan::GetMainModule().handle;
    std::sort(modules.begin(), modules.end(), [mainHandle](const auto &a, const auto &b) {
      const bool aMain = a.handle == mainHandle;
      const bool bMain = b.handle == mainHandle;
      if (aMain != bMain)
        return aMain;
      return a.size > b.size;
    });

    constexpr size_t kMaxTrackedTargets = 4u * 1024u * 1024u;
    constexpr size_t kMaxCandidatesToValidate = 2000;
    const auto stage2Modules = PatternScan::GetLoadedModules();
    const auto mainModule = PatternScan::GetMainModule();

    auto tryResolveStats =
        [&](std::unordered_map<uintptr_t, RipTargetStats> &targetStats,
            const PatternScan::ModuleInfo &mod) -> bool {
      if (targetStats.empty())
        return false;

      std::vector<std::pair<uintptr_t, int>> ranked;
      ranked.reserve(targetStats.size());
      for (const auto &[target, stat] : targetStats) {
        ranked.emplace_back(target, stat.refCount);
      }
      if (ranked.size() > kMaxTrackedTargets)
        return false;

      const size_t limit = (std::min)(ranked.size(), kMaxCandidatesToValidate);
      const uint64_t rankStart = GetTickCount64();
      std::partial_sort(ranked.begin(), ranked.begin() + limit, ranked.end(),
                        [](const auto &a, const auto &b) {
                          return a.second > b.second;
                        });
      if (IsVerboseLoggingEnabled()) {
        Log("[Discovery] GMalloc: ranked %zu targets in %llu ms",
            ranked.size(),
            static_cast<unsigned long long>(GetTickCount64() - rankStart));
      }

      int gateRejectionsLogged = 0;
      for (size_t i = 0; i < limit; ++i) {
        const auto &[target, weight] = ranked[i];
        void *instance = nullptr;
        if (!SafeMem_Read(reinterpret_cast<void *>(target), &instance,
                          sizeof(instance)))
          continue;
        if (!IsValidMallocInstanceWithModules(instance, stage2Modules))
          continue;
        char allocName[64] = {};
        int nameSlot = -1;
        if (!FindAllocatorDescriptiveName(instance, allocName, nameSlot)) {
          if (gateRejectionsLogged++ < 3) {
            Log("[Discovery] GMalloc: candidate %p (weight %d) rejected: "
                "no allocator-name getter (slot0 %p)",
                instance, weight, SafeMem_ReadPtr(instance));
          }
          continue;
        }
        state.GMallocLocation = reinterpret_cast<void *>(target);
        state.GMalloc = instance;
        strncpy(state.GMallocName, allocName, sizeof(state.GMallocName) - 1);

        const uint64_t slotStart = GetTickCount64();
        state.GMallocMallocSlot = MeasureAllocatorMallocSlot(mod, target);
        if (IsVerboseLoggingEnabled()) {
          Log("[Discovery] GMalloc: slot vote %llu ms",
              static_cast<unsigned long long>(GetTickCount64() - slotStart));
        }
        VerifyAllocatorMallocSlot(state);

        Log("[Discovery] GMalloc: resolved global at %p (references %d in %s, instance %p)",
            reinterpret_cast<void *>(target), weight, mod.name.c_str(), instance);
        Log("[Discovery] GMalloc: allocator self-identifies as \"%s\" (vtable slot %d)",
            allocName, nameSlot);
        return true;
      }
      return false;
    };

    std::vector<uintptr_t> staticBag;
    if (RuntimeConfig::Get().SkipPatternScan) {
      std::unique_lock<std::mutex> lock(g_StaticPointerBagMutex);
      if (g_StaticPointerBagCv.wait_for(
              lock, std::chrono::milliseconds(1500),
              [] { return g_StaticPointerBagReady; })) {
        staticBag = g_StaticPointerBag;
      } else {
        Log("[Discovery] GMalloc: structural pointer bag not ready; scanning code");
      }
    }

    if (!staticBag.empty() && mainModule.handle) {
      std::unordered_map<uintptr_t, RipTargetStats> bagStats;
      bagStats.max_load_factor(0.7f);
      bagStats.reserve(1u << 17);
      const uint64_t bagStart = GetTickCount64();
      for (const uintptr_t target : staticBag)
        ++bagStats[target].refCount;
      Log("[Discovery] GMalloc: bag scan %llu ms (%zu references, %zu targets in %s)",
          static_cast<unsigned long long>(GetTickCount64() - bagStart),
          staticBag.size(), bagStats.size(), mainModule.name.c_str());
      if (tryResolveStats(bagStats, mainModule))
        return true;
      Log("[Discovery] GMalloc: structural bag produced no allocator; scanning code");
    }

    std::unordered_map<uintptr_t, RipTargetStats> stats;
    for (const auto &mod : modules) {
      if (mod.size < 0x1000)
        continue;
      stats.clear();
      const uint64_t collectStart = GetTickCount64();
      CollectRipRelativeTargets(mod, stats);
      const uint64_t collectMs = GetTickCount64() - collectStart;
      if (stats.empty())
        continue;

      if (IsVerboseLoggingEnabled()) {
        Log("[Discovery] GMalloc: reference scan %llu ms (%zu targets in %s)",
            static_cast<unsigned long long>(collectMs), stats.size(),
            mod.name.c_str());
      }

      if (tryResolveStats(stats, mod))
        return true;
    }
  }

  Log("[Discovery] GMalloc not found");
  return false;
}

bool FindGNatives(EngineState &state) {
  return ResolveGNatives(state);
}

namespace {

struct HashTablesCandidate {
  uintptr_t address = 0;
  int32_t counts[4] = {0, 0, 0, 0};
  size_t stride = 0;
  int32_t score = -1;
  int32_t keysValid = 0;
  int32_t keysSampled = 0;
  bool csOk = false;
  size_t lockSize = 0x28;
  int32_t tier = 0;
};

bool IsCanonicalHashPointer(uintptr_t value) {
  return value >= 0x10000 && value < 0x7FFFFFFFFFFFULL;
}

enum class MapShape { Invalid, Empty, Populated };

MapShape InspectMapHeader(const uint8_t *map, int32_t &elementCount) {
  uintptr_t data = 0;
  uintptr_t secondaryBits = 0;
  uintptr_t hashBuckets = 0;
  int32_t arrayNum = 0, arrayMax = 0;
  int32_t numBits = 0, maxBits = 0;
  int32_t firstFreeIndex = 0, numFreeIndices = 0;
  int32_t hashSize = 0;
  memcpy(&data, map + 0x00, sizeof(data));
  memcpy(&arrayNum, map + 0x08, sizeof(arrayNum));
  memcpy(&arrayMax, map + 0x0C, sizeof(arrayMax));
  memcpy(&secondaryBits, map + 0x20, sizeof(secondaryBits));
  memcpy(&numBits, map + 0x28, sizeof(numBits));
  memcpy(&maxBits, map + 0x2C, sizeof(maxBits));
  memcpy(&firstFreeIndex, map + 0x30, sizeof(firstFreeIndex));
  memcpy(&numFreeIndices, map + 0x34, sizeof(numFreeIndices));
  memcpy(&hashBuckets, map + 0x40, sizeof(hashBuckets));
  memcpy(&hashSize, map + 0x48, sizeof(hashSize));

  if (!data && !arrayNum && !arrayMax && !numBits && !hashBuckets && !hashSize)
    return MapShape::Empty;

  if (arrayNum <= 0 || arrayNum > 20 * 1000 * 1000)
    return MapShape::Invalid;
  if (arrayMax < arrayNum || arrayMax > 40 * 1000 * 1000)
    return MapShape::Invalid;

  if (numBits != arrayNum)
    return MapShape::Invalid;
  if (maxBits < numBits || maxBits > 40 * 1000 * 1000)
    return MapShape::Invalid;
  if (firstFreeIndex < -1 || firstFreeIndex >= arrayMax)
    return MapShape::Invalid;
  if (numFreeIndices < 0 || numFreeIndices > arrayNum)
    return MapShape::Invalid;
  if (hashSize <= 0 || hashSize > 16 * 1000 * 1000 ||
      (hashSize & (hashSize - 1)) != 0)
    return MapShape::Invalid;

  if (!IsCanonicalHashPointer(data) || !IsReadable((void *)data, 8))
    return MapShape::Invalid;

  if (numBits > 128 &&
      (!IsCanonicalHashPointer(secondaryBits) ||
       !IsReadable((void *)secondaryBits, 4)))
    return MapShape::Invalid;

  if (hashSize > 1 &&
      (!IsCanonicalHashPointer(hashBuckets) ||
       !IsReadable((void *)hashBuckets, 4)))
    return MapShape::Invalid;

  elementCount = arrayNum;
  return MapShape::Populated;
}

bool IsMappedModulePointer(uintptr_t value,
                           const std::vector<PatternScan::ModuleInfo> &modules) {
  if (!IsCanonicalHashPointer(value))
    return false;
  for (const auto &mod : modules) {
    if (mod.base && value >= mod.base && value < mod.base + mod.size)
      return true;
  }
  return false;
}

bool LooksLikeUObject(uintptr_t object,
                      const std::vector<PatternScan::ModuleInfo> &modules) {
  if (!IsCanonicalHashPointer(object) || !IsReadable((void *)object, 0x28))
    return false;
  uintptr_t vtable = 0, classPtr = 0, classVtable = 0;
  if (SafeMem_ReadSpanExact((const void *)object, &vtable, sizeof(vtable)) !=
      sizeof(vtable))
    return false;
  if (SafeMem_ReadSpanExact((const void *)(object + 0x10), &classPtr,
                            sizeof(classPtr)) != sizeof(classPtr))
    return false;
  if (!IsMappedModulePointer(vtable, modules))
    return false;
  if (!IsCanonicalHashPointer(classPtr) ||
      !IsReadable((void *)classPtr, 0x28))
    return false;
  if (SafeMem_ReadSpanExact((const void *)classPtr, &classVtable,
                            sizeof(classVtable)) != sizeof(classVtable))
    return false;
  return IsMappedModulePointer(classVtable, modules);
}

bool SampleObjectOuterKeys(const uint8_t *map,
                           const std::vector<PatternScan::ModuleInfo> &modules,
                           int32_t &validOut, int32_t &sampledOut) {
  uintptr_t data = 0;
  uintptr_t secondaryBits = 0;
  int32_t arrayNum = 0, numBits = 0, maxBits = 0;
  memcpy(&data, map + 0x00, sizeof(data));
  memcpy(&arrayNum, map + 0x08, sizeof(arrayNum));
  memcpy(&secondaryBits, map + 0x20, sizeof(secondaryBits));
  memcpy(&numBits, map + 0x28, sizeof(numBits));
  memcpy(&maxBits, map + 0x2C, sizeof(maxBits));
  if (arrayNum <= 0 || numBits != arrayNum || maxBits < numBits)
    return false;

  const uint32_t *words = nullptr;
  uint32_t inlineWords[4] = {};
  if (numBits <= 128) {
    memcpy(inlineWords, map + 0x10, sizeof(inlineWords));
    words = inlineWords;
  } else {
    if (!IsCanonicalHashPointer(secondaryBits))
      return false;
    const size_t wordBytes =
        (static_cast<size_t>(numBits) + 31) / 32 * sizeof(uint32_t);
    if (!IsReadable((void *)secondaryBits, wordBytes))
      return false;
    words = reinterpret_cast<const uint32_t *>(secondaryBits);
  }

  static const size_t kElementStrides[] = {0x20, 0x28};
  constexpr int32_t kMaxSamples = 8;
  constexpr int32_t kMinSamples = 4;
  const int32_t cap = (std::min)(arrayNum, 1024);

  int32_t bestValid = 0, bestSampled = 0;
  for (size_t stride : kElementStrides) {
    int32_t valid = 0, sampled = 0;
    for (int32_t i = 0; i < cap && sampled < kMaxSamples; ++i) {
      if (((words[i >> 5] >> (i & 31)) & 1u) == 0)
        continue;
      uintptr_t key = 0;
      ++sampled;
      if (SafeMem_ReadSpanExact(
              (const void *)(data + static_cast<size_t>(i) * stride), &key,
              sizeof(key)) != sizeof(key))
        continue;
      if (LooksLikeUObject(key, modules))
        ++valid;
    }
    if (sampled >= kMinSamples && valid * 4 >= sampled * 3) {
      validOut = valid;
      sampledOut = sampled;
      return true;
    }
    if (sampled > bestSampled || (sampled == bestSampled && valid > bestValid)) {
      bestValid = valid;
      bestSampled = sampled;
    }
  }
  validOut = bestValid;
  sampledOut = bestSampled;
  return false;
}

bool LooksLikeCriticalSectionBase(const uint8_t *buffer, size_t size,
                                  size_t baseOffset) {
  if (baseOffset + 0x28 > size)
    return false;
  const uint8_t *base = buffer + baseOffset;
  uint64_t debugInfo = 0, owningThread = 0, lockSemaphore = 0;
  int32_t lockCount = 0, recursionCount = 0;
  memcpy(&debugInfo, base + 0x00, 8);
  memcpy(&lockCount, base + 0x08, 4);
  memcpy(&recursionCount, base + 0x0C, 4);
  memcpy(&owningThread, base + 0x10, 8);
  memcpy(&lockSemaphore, base + 0x18, 8);
  if (debugInfo != 0 && !IsCanonicalHashPointer((uintptr_t)debugInfo))
    return false;
  if (lockCount < -1)
    return false;
  if (recursionCount < 0 || recursionCount > 0xFFFF)
    return false;
  if (owningThread >= 0x7FFFFFFFFFFFULL ||
      lockSemaphore >= 0x7FFFFFFFFFFFULL)
    return false;
  return true;
}

enum class ChainResult { None, Relaxed, Strict };

ChainResult DescribeHashTablesChain(const uint8_t *buffer, size_t size,
                                    size_t map0Offset, size_t stride,
                                    HashTablesCandidate &candidate,
                                    size_t &baseOffset) {
  if (map0Offset < 0x28 || map0Offset + 3 * stride + 0x50 > size)
    return ChainResult::None;

  int32_t counts[4] = {};
  bool middleMapsOk = true;
  for (int i = 0; i < 4; ++i) {
    const MapShape shape =
        InspectMapHeader(buffer + map0Offset + i * stride, counts[i]);

    if (i == 0 || i == 2) {
      if (shape != MapShape::Populated)
        return ChainResult::None;
    } else {
      if (shape == MapShape::Invalid)
        return ChainResult::None;
      middleMapsOk = middleMapsOk && shape == MapShape::Populated;
    }
  }

  candidate.counts[0] = counts[0];
  candidate.counts[1] = counts[1];
  candidate.counts[2] = counts[2];
  candidate.counts[3] = counts[3];
  candidate.stride = stride;
  candidate.score = counts[0] + counts[1] + counts[2] + counts[3];

  size_t derivedLock = 0;
  for (size_t back = 0x08; back <= 0x80; back += 8) {
    if (map0Offset < back + 0x20)
      break;
    uint64_t spin = 0;
    memcpy(&spin, buffer + map0Offset - back, sizeof(spin));
    if (spin != 4000)
      continue;
    const size_t lockOffset = map0Offset - back - 0x20;
    if (!LooksLikeCriticalSectionBase(buffer, size, lockOffset))
      continue;
    derivedLock = back + 0x20;
    break;
  }
  candidate.csOk = derivedLock != 0;
  candidate.lockSize = candidate.csOk ? derivedLock : 0x28;
  baseOffset = map0Offset - candidate.lockSize;
  return middleMapsOk ? ChainResult::Strict : ChainResult::Relaxed;
}

}

bool FindGUObjectHashTables(EngineState &state) {
  FastMemScope fastMem;
  const auto modules = PatternScan::GetLoadedModules();
  constexpr size_t kMaxSectionBytes = 64 * 1024 * 1024;
  constexpr size_t kMaxContentChecks = 256;
  static const size_t kMapStrides[] = {0x50};
  struct SectionHit {
    HashTablesCandidate candidate;
    size_t map0Offset = 0;
  };

  HashTablesCandidate best;
  HashTablesCandidate bestRejected;
  size_t verifiedChains = 0;
  size_t rejectedChains = 0;

  for (const auto &mod : modules) {
    if (!mod.base || mod.size < 0x1000)
      continue;
    const auto *dos =
        reinterpret_cast<const IMAGE_DOS_HEADER *>(mod.base);
    if (!SafeMem_IsReadable(dos, sizeof(*dos)) ||
        dos->e_magic != IMAGE_DOS_SIGNATURE)
      continue;
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(
        mod.base + static_cast<DWORD>(dos->e_lfanew));
    if (!SafeMem_IsReadable(nt, sizeof(*nt)) ||
        nt->Signature != IMAGE_NT_SIGNATURE)
      continue;
    if (nt->FileHeader.NumberOfSections == 0 ||
        nt->FileHeader.NumberOfSections > 256)
      continue;
    const auto *sections = IMAGE_FIRST_SECTION(nt);
    if (!SafeMem_IsReadable(
            sections, sizeof(IMAGE_SECTION_HEADER) *
                          nt->FileHeader.NumberOfSections))
      continue;

    const uintptr_t moduleEnd = mod.base + mod.size;
    for (WORD s = 0; s < nt->FileHeader.NumberOfSections; ++s) {
      const IMAGE_SECTION_HEADER &section = sections[s];
      if (!section.Misc.VirtualSize || !section.VirtualAddress)
        continue;
      if (!(section.Characteristics & IMAGE_SCN_MEM_WRITE) ||
          (section.Characteristics & IMAGE_SCN_MEM_EXECUTE))
        continue;

      uintptr_t sectionBase = mod.base + section.VirtualAddress;
      if (sectionBase >= moduleEnd)
        continue;
      size_t sectionSize = (std::min)(
          static_cast<size_t>(section.Misc.VirtualSize),
          static_cast<size_t>(moduleEnd - sectionBase));
      sectionSize = (std::min)(sectionSize, kMaxSectionBytes);
      if (sectionSize < 0x200)
        continue;

      std::vector<uint8_t> buffer(sectionSize);
      if (SafeMem_ReadSpanExact(reinterpret_cast<const void *>(sectionBase),
                                buffer.data(), sectionSize) != sectionSize)
        continue;

      std::vector<SectionHit> hits;
      for (size_t map0 = 0x28; map0 + 0x50 <= sectionSize; map0 += 8) {

        int32_t headNum = 0, headMax = 0;
        memcpy(&headNum, buffer.data() + map0 + 0x08, sizeof(headNum));
        if (headNum <= 0 || headNum > 20 * 1000 * 1000)
          continue;
        memcpy(&headMax, buffer.data() + map0 + 0x0C, sizeof(headMax));
        if (headMax < headNum || headMax > 40 * 1000 * 1000)
          continue;

        for (size_t stride : kMapStrides) {
          HashTablesCandidate hit;
          size_t baseOffset = 0;
          const ChainResult result = DescribeHashTablesChain(
              buffer.data(), sectionSize, map0, stride, hit, baseOffset);
          if (result == ChainResult::None)
            continue;
          hit.tier = result == ChainResult::Strict ? 2 : 1;
          hit.address = sectionBase + baseOffset;
          hits.push_back({hit, map0});
        }
      }

      std::stable_sort(hits.begin(), hits.end(),
                       [](const SectionHit &a, const SectionHit &b) {
                         if (a.candidate.tier != b.candidate.tier)
                           return a.candidate.tier > b.candidate.tier;
                         return a.candidate.score > b.candidate.score;
                       });
      size_t checks = 0;
      for (const SectionHit &hit : hits) {
        if (checks >= kMaxContentChecks)
          break;
        ++checks;
        int32_t valid = 0, sampled = 0;
        const bool keysAreObjects = SampleObjectOuterKeys(
            buffer.data() + hit.map0Offset + 2 * hit.candidate.stride,
            modules, valid, sampled);
        HashTablesCandidate scored = hit.candidate;
        scored.keysValid = valid;
        scored.keysSampled = sampled;
        if (!keysAreObjects) {
          ++rejectedChains;
          if (scored.score > bestRejected.score)
            bestRejected = scored;
          continue;
        }
        ++verifiedChains;
        if (scored.tier > best.tier ||
            (scored.tier == best.tier && scored.score > best.score))
          best = scored;
        break;
      }
    }
  }

  if (!best.address) {
    if (bestRejected.address) {
      Log("[Discovery] GUObjectHashTables chain at %p rejected: "
          "maps=%d/%d/%d/%d, uobject-keys=%d/%d",
          reinterpret_cast<void *>(bestRejected.address),
          bestRejected.counts[0], bestRejected.counts[1],
          bestRejected.counts[2], bestRejected.counts[3],
          bestRejected.keysValid, bestRejected.keysSampled);
    }
    Log("[Discovery] GUObjectHashTables not found (chains=%zu, rejected=%zu "
        "in writable module data)",
        verifiedChains, rejectedChains);
    return false;
  }

  state.GUObjectHashTables = reinterpret_cast<void *>(best.address);
  Log("[Discovery] Found GUObjectHashTables by structure: %p "
      "(stride=0x%llX, %s, lock=0x%llX%s, maps=%d/%d/%d/%d, "
      "uobject-keys=%d/%d, score=%d)",
      state.GUObjectHashTables,
      static_cast<unsigned long long>(best.stride),
      best.tier == 2 ? "four-map chain" : "hash+outer chain",
      static_cast<unsigned long long>(best.lockSize),
      best.csOk ? " derived" : " assumed", best.counts[0],
      best.counts[1], best.counts[2], best.counts[3], best.keysValid,
      best.keysSampled, best.score);

  if (bestRejected.address && bestRejected.score > best.score) {
    Log("[Discovery] GUObjectHashTables larger chain rejected at %p "
        "(maps=%d/%d/%d/%d, uobject-keys=%d/%d)",
        reinterpret_cast<void *>(bestRejected.address),
        bestRejected.counts[0], bestRejected.counts[1],
        bestRejected.counts[2], bestRejected.counts[3],
        bestRejected.keysValid, bestRejected.keysSampled);
  }
  return true;
}

bool FindGlobals(EngineState &state) {
  ResetStaticPointerBag();
  std::thread t0([&state]() { FindGUObjectArray(state); });
  std::thread t1([&state]() { FindGNames(state); });
  std::thread t2([&state]() { FindGWorld(state); });
  std::thread t3([&state]() { FindGMalloc(state); });
  std::thread t4([&state]() { FindGNatives(state); });
  std::thread t5([&state]() { FindGUObjectHashTables(state); });

  t0.join();
  t1.join();
  t2.join();
  t3.join();
  t4.join();
  t5.join();

  state.bGlobalsFound = (state.GUObjectArray != nullptr || state.GNames != nullptr);
  return state.bGlobalsFound;
}

}
