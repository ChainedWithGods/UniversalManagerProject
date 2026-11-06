#include "LogSession.h"

#include <windows.h>
#include <d3d12.h>
#include <dxgi.h>
#include <intrin.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

static const char kSupportedEngineRange[] = "Unreal Engine 4.23 - 5.8";

namespace {

constexpr int kRuleWidth = 80;
constexpr int kTagColumn = 12;
constexpr int kThreadColumn = 6;
constexpr int kRotationKeep = 5;

constexpr int kLineWidth = 4096;

constexpr int kClockColumn = 14;

constexpr int kMessageColumn = kClockColumn + kTagColumn + 4;

struct SectionDef {
  const char *id;
  const char *title;
};

const SectionDef kSections[] = {
    {"config", "CONFIGURATION"},
    {"runtime", "RUNTIME LIFECYCLE"},
    {"discovery", "ENGINE DISCOVERY"},
    {"crypto", "CRYPTOGRAPHY & AES"},
    {"console", "CONSOLE ENABLER"},
    {"overlay", "OVERLAY & PRESENTATION"},
    {"lua", "LUA MOD RUNTIME"},
    {"cppmods", "C++ MOD RUNTIME"},
    {"level_transition", "LEVEL TRANSITION"},
};

struct TagDef {
  const char *tag;
  const char *section;
};

const TagDef kTagSections[] = {
    {"Avalonium", "runtime"},
    {"Runtime", "runtime"},
    {"Init", "runtime"},
    {"Config", "config"},
    {"Discovery", "discovery"},
    {"Intelligence", "discovery"},
    {"EngineDetection", "discovery"},
    {"OffsetDiscovery", "discovery"},
    {"WorldDiscovery", "discovery"},
    {"Input", "discovery"},
    {"AESKeyFinder", "crypto"},
    {"Crypto", "crypto"},
    {"PakVerifier", "crypto"},
    {"ConsoleEnabler", "console"},
    {"Console", "console"},
    {"Dumper", "console"},
    {"TravelGuard", "level_transition"},
    {"Lua", "lua"},
    {"LuaHook", "lua"},
    {"UniversalHUD", "lua"},
    {"UniversalOverlay", "lua"},
    {"Blueprint", "lua"},
    {"BPML", "lua"},
    {"BPModLoader", "lua"},
    {"LuaTypeGen", "lua"},
    {"ProcessEvent", "lua"},
    {"CppMod", "cppmods"},
    {"Overlay", "overlay"},
    {"SystemMetrics", "overlay"},
    {"d3d12hook", "overlay"},
    {"d3d11hook", "overlay"},
    {"ImGui", "overlay"},
    {"hooks", "overlay"},
};

struct SectionState {
  std::string id;
  std::string title;
  long long firstMs = 0;
  long long lastMs = 0;
  uint32_t entries = 0;
  uint32_t warnings = 0;
  uint32_t errors = 0;
  bool bannerWritten = false;
  bool closed = false;
};

struct BufferedLogEntry {
  std::string tag;
  std::string text;
  LogLevel level = LogLevel::Info;
  long long nowMs = 0;
};

std::string g_ExclusiveSectionId;
std::string g_ActiveSectionId;
std::vector<BufferedLogEntry> g_BufferedEntries;

std::mutex g_Mutex;
std::FILE *g_File = nullptr;
std::string g_Path;
bool g_Started = false;
bool g_HeaderWritten = false;
bool g_FooterWritten = false;
bool g_Verbose = false;
LogLevel g_MinLevel = LogLevel::Info;
const std::chrono::steady_clock::time_point g_SessionStart =
    std::chrono::steady_clock::now();
std::vector<SectionState> g_Sections;

thread_local std::string t_CurrentSection = "runtime";
std::vector<std::pair<std::string, std::string>> g_Facts;
std::unordered_map<std::string, std::string> g_FactIndex;
std::unordered_map<DWORD, std::string> g_ThreadNames;
std::string g_Outcome;
std::string g_OutcomeDetail;
uint64_t g_Sequence = 0;
uint32_t g_Counts[6] = {0, 0, 0, 0, 0, 0};

std::string ToLowerAscii(const std::string &s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

void TrimTrailing(std::string &s) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    s.pop_back();
}

void TrimLeading(const std::string &s, size_t &i) {
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
    ++i;
}

std::string PadRight(const std::string &s, size_t width) {
  if (s.size() >= width)
    return s;
  return s + std::string(width - s.size(), ' ');
}

std::string Repeat(const char *unit, int count) {
  std::string out;
  out.reserve(static_cast<size_t>(count) * std::strlen(unit));
  for (int i = 0; i < count; ++i)
    out += unit;
  return out;
}

bool Contains(const std::string &hay, const char *needle) {
  return hay.find(needle) != std::string::npos;
}

std::string FormatClock(long long ms) {
  char buf[48] = {};

  const long long clamped = (ms < 0) ? 0 : (ms > 99999999LL ? 99999999LL : ms);
  snprintf(buf, sizeof(buf), "[+%08lldms]", clamped);
  return buf;
}

std::string FormatDuration(long long ms) {
  char buf[48] = {};
  snprintf(buf, sizeof(buf), "%.3f s", static_cast<double>(ms) / 1000.0);
  return buf;
}

std::string FormatLocalTimestamp() {
  SYSTEMTIME st = {};
  GetLocalTime(&st);
  char buf[64] = {};
  snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u.%03u", st.wYear,
           st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
           st.wMilliseconds);
  return buf;
}

std::string FormatHostOs() {

  struct HostVersionInfo {
    ULONG dwOSVersionInfoSize;
    ULONG dwMajorVersion;
    ULONG dwMinorVersion;
    ULONG dwBuildNumber;
    ULONG dwPlatformId;
    WCHAR szCSDVersion[128];
  };
  using RtlGetVersionFn = LONG(WINAPI *)(HostVersionInfo *);

  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (!ntdll)
    return "Windows";
  auto fn = reinterpret_cast<RtlGetVersionFn>(
      reinterpret_cast<void *>(GetProcAddress(ntdll, "RtlGetVersion")));
  if (!fn)
    return "Windows";

  HostVersionInfo info = {};
  info.dwOSVersionInfoSize = sizeof(info);
  if (fn(&info) != 0)
    return "Windows";

  std::string osName = "Windows";
  if (info.dwMajorVersion == 10 && info.dwMinorVersion == 0) {
    osName = (info.dwBuildNumber >= 22000) ? "Windows 11" : "Windows 10";
  } else if (info.dwMajorVersion > 10) {
    osName = "Windows 11";
  } else if (info.dwMajorVersion == 6) {
    if (info.dwMinorVersion == 3)
      osName = "Windows 8.1";
    else if (info.dwMinorVersion == 2)
      osName = "Windows 8";
    else if (info.dwMinorVersion == 1)
      osName = "Windows 7";
    else if (info.dwMinorVersion == 0)
      osName = "Windows Vista";
  }

  char displayVer[32] = {};
  HMODULE advapi = LoadLibraryA("advapi32.dll");
  if (advapi) {
    using RegOpenKeyExAFn = LSTATUS(WINAPI *)(HKEY, LPCSTR, DWORD, REGSAM, PHKEY);
    using RegQueryValueExAFn = LSTATUS(WINAPI *)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
    using RegCloseKeyFn = LSTATUS(WINAPI *)(HKEY);

    auto pRegOpen = reinterpret_cast<RegOpenKeyExAFn>(GetProcAddress(advapi, "RegOpenKeyExA"));
    auto pRegQuery = reinterpret_cast<RegQueryValueExAFn>(GetProcAddress(advapi, "RegQueryValueExA"));
    auto pRegClose = reinterpret_cast<RegCloseKeyFn>(GetProcAddress(advapi, "RegCloseKey"));

    if (pRegOpen && pRegQuery && pRegClose) {
      HKEY hKey = nullptr;
      if (pRegOpen(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                   0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD dispSize = sizeof(displayVer);
        if (pRegQuery(hKey, "DisplayVersion", nullptr, nullptr,
                      reinterpret_cast<LPBYTE>(displayVer), &dispSize) != ERROR_SUCCESS) {
          dispSize = sizeof(displayVer);
          pRegQuery(hKey, "ReleaseId", nullptr, nullptr,
                    reinterpret_cast<LPBYTE>(displayVer), &dispSize);
        }
        pRegClose(hKey);
      }
    }
    FreeLibrary(advapi);
  }

  char buf[96] = {};
  if (displayVer[0] != '\0') {
    snprintf(buf, sizeof(buf), "%s %s build %lu", osName.c_str(), displayVer,
             info.dwBuildNumber);
  } else {
    snprintf(buf, sizeof(buf), "%s build %lu", osName.c_str(), info.dwBuildNumber);
  }
  return buf;
}

std::string FormatCpuBrand() {
  int cpuInfo[4] = {0};
  __cpuid(cpuInfo, static_cast<int>(0x80000000));
  const unsigned int maxExtId = static_cast<unsigned int>(cpuInfo[0]);

  char brand[64] = {};
  if (maxExtId >= 0x80000004) {
    __cpuid(reinterpret_cast<int *>(brand), static_cast<int>(0x80000002));
    __cpuid(reinterpret_cast<int *>(brand + 16), static_cast<int>(0x80000003));
    __cpuid(reinterpret_cast<int *>(brand + 32), static_cast<int>(0x80000004));
  }

  std::string s(brand);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
    s.erase(s.begin());
  }
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
    s.pop_back();
  }

  SYSTEM_INFO sysInfo = {};
  GetSystemInfo(&sysInfo);
  const std::string suffix = " (" + std::to_string(sysInfo.dwNumberOfProcessors) + " logical CPUs)";
  if (!s.empty()) {
    return s + suffix;
  }
  return std::to_string(sysInfo.dwNumberOfProcessors) + " logical CPUs";
}

std::string FormatCpuFeatures() {
  int cpuInfo[4] = {0};
  __cpuid(cpuInfo, 0);
  const int maxId = cpuInfo[0];
  if (maxId < 1)
    return "";

  __cpuid(cpuInfo, 1);
  const int ecx1 = cpuInfo[2];
  const int edx1 = cpuInfo[3];

  int ebx7 = 0;
  if (maxId >= 7) {
    __cpuidex(cpuInfo, 7, 0);
    ebx7 = cpuInfo[1];
  }

  std::vector<const char *> features;
  if (edx1 & (1 << 26)) features.push_back("SSE2");
  if (ecx1 & (1 << 19)) features.push_back("SSE4.1");
  if (ecx1 & (1 << 20)) features.push_back("SSE4.2");
  if (ecx1 & (1 << 25)) features.push_back("AES-NI");
  if (ecx1 & (1 << 12)) features.push_back("FMA3");
  if (ecx1 & (1 << 28)) features.push_back("AVX");
  if (ebx7 & (1 << 5))  features.push_back("AVX2");
  if (ebx7 & (1 << 16)) features.push_back("AVX-512F");
  if (ebx7 & (1 << 3))  features.push_back("BMI1");
  if (ebx7 & (1 << 8))  features.push_back("BMI2");
  if (ebx7 & (1 << 29)) features.push_back("SHA");

  std::string result;
  for (size_t i = 0; i < features.size(); ++i) {
    if (i > 0) result += ", ";
    result += features[i];
  }
  return result;
}

std::string FormatRamInfo() {
  MEMORYSTATUSEX mem = {};
  mem.dwLength = sizeof(mem);
  if (!GlobalMemoryStatusEx(&mem)) {
    return "";
  }
  const double totalGb = static_cast<double>(mem.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
  const double availGb = static_cast<double>(mem.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
  char buf[96] = {};
  snprintf(buf, sizeof(buf), "%.1f GB total (%.1f GB available, %lu%% in use)",
           totalGb, availGb, mem.dwMemoryLoad);
  return buf;
}

std::string FormatProcessMem() {
  PROCESS_MEMORY_COUNTERS_EX pmc = {};
  pmc.cb = sizeof(pmc);
  if (GetProcessMemoryInfo(
          GetCurrentProcess(),
          reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&pmc), sizeof(pmc))) {
    const double wsMb =
        static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    const double peakWsMb =
        static_cast<double>(pmc.PeakWorkingSetSize) / (1024.0 * 1024.0);
    const double commitMb =
        static_cast<double>(pmc.PrivateUsage) / (1024.0 * 1024.0);
    char buf[128] = {};
    snprintf(buf, sizeof(buf),
             "%.1f MB working set (peak %.1f MB, commit %.1f MB)", wsMb,
             peakWsMb, commitMb);
    return buf;
  }
  return "";
}

std::string FormatGpuInfo() {
  IDXGIFactory1 *factory = nullptr;
  HRESULT hr = E_FAIL;

  HMODULE dxgiMod = GetModuleHandleA("dxgi.dll");
  if (!dxgiMod) {
    dxgiMod = LoadLibraryA("dxgi.dll");
  }

  if (dxgiMod) {
    using CreateDXGIFactory1Fn = HRESULT(WINAPI *)(REFIID, void **);
    auto pCreateFactory1 = reinterpret_cast<CreateDXGIFactory1Fn>(
        GetProcAddress(dxgiMod, "CreateDXGIFactory1"));
    if (pCreateFactory1) {
      hr = pCreateFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory));
    }
  }

  if (SUCCEEDED(hr) && factory) {
    std::string bestName;
    SIZE_T bestVram = 0;
    SIZE_T bestShared = 0;

    IDXGIAdapter1 *adapter = nullptr;
    for (UINT i = 0; SUCCEEDED(factory->EnumAdapters1(i, &adapter)); ++i) {
      DXGI_ADAPTER_DESC1 desc = {};
      if (SUCCEEDED(adapter->GetDesc1(&desc))) {
        if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
          if (desc.DedicatedVideoMemory > bestVram || bestName.empty()) {
            bestVram = desc.DedicatedVideoMemory;
            bestShared = desc.SharedSystemMemory;
            char nameBuf[256] = {};
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
            bestName = nameBuf;
          }
        }
      }
      adapter->Release();
    }
    factory->Release();

    if (!bestName.empty()) {
      const double vramGb = static_cast<double>(bestVram) / (1024.0 * 1024.0 * 1024.0);
      const double sharedGb = static_cast<double>(bestShared) / (1024.0 * 1024.0 * 1024.0);
      char buf[320] = {};
      if (vramGb >= 0.1 && sharedGb >= 0.1) {
        snprintf(buf, sizeof(buf), "%s (%.1f GB Dedicated VRAM, %.1f GB Shared)",
                 bestName.c_str(), vramGb, sharedGb);
      } else if (vramGb >= 0.1) {
        snprintf(buf, sizeof(buf), "%s (%.1f GB Dedicated VRAM)", bestName.c_str(), vramGb);
      } else {
        snprintf(buf, sizeof(buf), "%s", bestName.c_str());
      }
      return buf;
    }
  }

  DISPLAY_DEVICEA dd = {};
  dd.cb = sizeof(dd);
  for (DWORD i = 0; EnumDisplayDevicesA(nullptr, i, &dd, 0); ++i) {
    if ((dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) && dd.DeviceString[0] != '\0') {
      std::string regPath = dd.DeviceKey;
      const std::string prefix = "\\Registry\\Machine\\";
      if (regPath.rfind(prefix, 0) == 0) {
        regPath = regPath.substr(prefix.size());
      }
      uint64_t vramBytes = 0;
      HKEY hKey = nullptr;
      if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(vramBytes);
        if (RegQueryValueExA(hKey, "HardwareInformation.qwMemorySize", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(&vramBytes), &size) != ERROR_SUCCESS || vramBytes == 0) {
          DWORD dwordVal = 0;
          size = sizeof(dwordVal);
          if (RegQueryValueExA(hKey, "HardwareInformation.MemorySize", nullptr, nullptr,
                               reinterpret_cast<LPBYTE>(&dwordVal), &size) == ERROR_SUCCESS) {
            vramBytes = dwordVal;
          }
        }
        RegCloseKey(hKey);
      }
      if (vramBytes > 0) {
        const double vramGb = static_cast<double>(vramBytes) / (1024.0 * 1024.0 * 1024.0);
        char buf[256] = {};
        snprintf(buf, sizeof(buf), "%s (%.1f GB VRAM)", dd.DeviceString, vramGb);
        return buf;
      }
      return dd.DeviceString;
    }
  }
  return "";
}

std::string FormatDisplayInfo() {
  DEVMODEA dm = {};
  dm.dmSize = sizeof(dm);
  if (EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &dm)) {
    char buf[64] = {};
    if (dm.dmDisplayFrequency > 1) {
      snprintf(buf, sizeof(buf), "%lu x %lu @ %lu Hz (%lu-bit)",
               dm.dmPelsWidth, dm.dmPelsHeight, dm.dmDisplayFrequency, dm.dmBitsPerPel);
    } else {
      snprintf(buf, sizeof(buf), "%lu x %lu (%lu-bit)", dm.dmPelsWidth, dm.dmPelsHeight, dm.dmBitsPerPel);
    }
    return buf;
  }
  return "";
}

std::string FormatGameDrive() {
  char exePath[MAX_PATH] = {};
  if (!GetModuleFileNameA(nullptr, exePath, MAX_PATH))
    return "";

  char root[4] = {exePath[0], ':', '\\', '\0'};
  ULARGE_INTEGER freeBytesCaller = {}, totalBytes = {}, totalFree = {};
  char fsName[32] = {};
  GetVolumeInformationA(root, nullptr, 0, nullptr, nullptr, nullptr, fsName,
                         sizeof(fsName));
  if (GetDiskFreeSpaceExA(root, &freeBytesCaller, &totalBytes, &totalFree)) {
    const double freeGb =
        static_cast<double>(totalFree.QuadPart) / (1024.0 * 1024.0 * 1024.0);
    char buf[64] = {};
    snprintf(buf, sizeof(buf), "%s (%s, %.1f GB free)", root,
             fsName[0] ? fsName : "Disk", freeGb);
    return buf;
  }
  return "";
}

std::string FormatTimerResolution() {
  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (ntdll) {
    using NtQueryTimerResFn = LONG(NTAPI *)(PULONG, PULONG, PULONG);
    auto pQuery = reinterpret_cast<NtQueryTimerResFn>(
        reinterpret_cast<void *>(GetProcAddress(ntdll, "NtQueryTimerResolution")));
    if (pQuery) {
      ULONG minRes = 0, maxRes = 0, curRes = 0;
      if (pQuery(&minRes, &maxRes, &curRes) == 0) {
        const double curMs = static_cast<double>(curRes) / 10000.0;
        char buf[64] = {};
        snprintf(buf, sizeof(buf), "%.3f ms (%s)", curMs,
                 (curRes <= 10000) ? "High Resolution" : "Standard");
        return buf;
      }
    }
  }
  return "";
}

std::string FormatBinaryInfo() {
  HMODULE hExe = GetModuleHandleA(nullptr);
  if (!hExe)
    return "";

  auto dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(hExe);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    return "";

  auto nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(
      reinterpret_cast<const BYTE *>(hExe) + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE)
    return "";

  const double imageMb =
      static_cast<double>(nt->OptionalHeader.SizeOfImage) / (1024.0 * 1024.0);
  const WORD chars = nt->OptionalHeader.DllCharacteristics;

  std::string sec;
  if (chars & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE)
    sec += "ASLR";
  if (chars & IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA)
    sec += "+HighEntropy";
  if (chars & IMAGE_DLLCHARACTERISTICS_NX_COMPAT) {
    if (!sec.empty())
      sec += ", ";
    sec += "DEP";
  }
  if (chars & IMAGE_DLLCHARACTERISTICS_GUARD_CF) {
    if (!sec.empty())
      sec += ", ";
    sec += "CFG";
  }

  char buf[128] = {};
  snprintf(buf, sizeof(buf), "PE32+ (x64), size %.1f MB%s%s", imageMb,
           sec.empty() ? "" : ", ", sec.c_str());
  return buf;
}

std::string FormatBaseAddress() {
  HMODULE hExe = GetModuleHandleA(nullptr);
  if (!hExe)
    return "";

  auto dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(hExe);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    return "";

  auto nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(
      reinterpret_cast<const BYTE *>(hExe) + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE)
    return "";

  const void *entryPoint = reinterpret_cast<const void *>(
      reinterpret_cast<const BYTE *>(hExe) +
      nt->OptionalHeader.AddressOfEntryPoint);

  char buf[128] = {};
  snprintf(buf, sizeof(buf), "%p (entry point %p)", hExe, entryPoint);
  return buf;
}

std::string FormatProcessElevatedSuffix() {
  HANDLE hToken = nullptr;
  if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
    TOKEN_ELEVATION elev = {};
    DWORD retLen = 0;
    if (GetTokenInformation(hToken, TokenElevation, &elev, sizeof(elev), &retLen)) {
      CloseHandle(hToken);
      return elev.TokenIsElevated ? ", Elevated" : ", Standard";
    }
    CloseHandle(hToken);
  }
  return "";
}

std::string FormatCpuTopology() {
  DWORD length = 0;
  GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
  if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && length > 0) {
    std::vector<BYTE> buffer(length);
    auto info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(buffer.data());
    if (GetLogicalProcessorInformationEx(RelationProcessorCore, info, &length)) {
      int physicalCores = 0;
      int logicalThreads = 0;
      BYTE *ptr = buffer.data();
      while (ptr < buffer.data() + length) {
        auto item = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(ptr);
        if (item->Relationship == RelationProcessorCore) {
          physicalCores++;
          for (WORD g = 0; g < item->Processor.GroupCount; ++g) {
            KAFFINITY mask = item->Processor.GroupMask[g].Mask;
            while (mask) {
              logicalThreads += static_cast<int>(mask & 1);
              mask >>= 1;
            }
          }
        }
        ptr += item->Size;
      }
      if (physicalCores > 0) {
        char buf[96] = {};
        snprintf(buf, sizeof(buf), "%d physical cores, %d logical threads",
                 physicalCores, logicalThreads);
        return buf;
      }
    }
  }
  SYSTEM_INFO sysInfo = {};
  GetSystemInfo(&sysInfo);
  return std::to_string(sysInfo.dwNumberOfProcessors) + " logical threads";
}

std::string FormatCpuCache() {
  DWORD length = 0;
  GetLogicalProcessorInformationEx(RelationCache, nullptr, &length);
  if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && length > 0) {
    std::vector<BYTE> buffer(length);
    if (GetLogicalProcessorInformationEx(
            RelationCache,
            reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(buffer.data()),
            &length)) {
      SIZE_T l1d = 0, l1i = 0, l2 = 0, l3 = 0;
      BYTE *ptr = buffer.data();
      while (ptr < buffer.data() + length) {
        auto item = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(ptr);
        if (item->Relationship == RelationCache) {
          const auto &c = item->Cache;
          if (c.Level == 1) {
            if (c.Type == CacheData)
              l1d += c.CacheSize;
            else if (c.Type == CacheInstruction)
              l1i += c.CacheSize;
            else
              l1d += c.CacheSize;
          } else if (c.Level == 2) {
            l2 += c.CacheSize;
          } else if (c.Level == 3) {
            l3 += c.CacheSize;
          }
        }
        ptr += item->Size;
      }
      char buf[128] = {};
      const double l1Kb = static_cast<double>(l1d + l1i) / 1024.0;
      const double l2Mb = static_cast<double>(l2) / (1024.0 * 1024.0);
      const double l3Mb = static_cast<double>(l3) / (1024.0 * 1024.0);
      snprintf(buf, sizeof(buf), "L1: %.0f KB, L2: %.1f MB, L3: %.1f MB",
               l1Kb, l2Mb, l3Mb);
      return buf;
    }
  }
  return "";
}

std::string FormatHypervisorInfo() {
  int cpuInfo[4] = {0};
  __cpuid(cpuInfo, 1);
  const bool hypervisorPresent = (cpuInfo[2] & (1 << 31)) != 0;
  if (!hypervisorPresent) {
    return "Bare Metal (Native hardware, no hypervisor)";
  }

  __cpuid(cpuInfo, static_cast<int>(0x40000000));
  char sig[13] = {};
  memcpy(sig, &cpuInfo[1], 4);
  memcpy(sig + 4, &cpuInfo[2], 4);
  memcpy(sig + 8, &cpuInfo[3], 4);

  std::string name(sig);
  if (name == "Microsoft Hv") {
    return "Hyper-V / VBS active (Microsoft Hv)";
  } else if (name == "VMwareVMware") {
    return "VMware Virtual Machine";
  } else if (name == "KVMKVMKVM") {
    return "Linux KVM Virtual Machine";
  }
  return "Virtual Machine (" + name + ")";
}

std::string FormatDirectXCapabilities() {

  IDXGIFactory1 *factory = nullptr;
  IDXGIAdapter1 *hardwareAdapter = nullptr;
  HMODULE dxgiMod = GetModuleHandleA("dxgi.dll");
  if (!dxgiMod)
    dxgiMod = LoadLibraryA("dxgi.dll");
  if (dxgiMod) {
    using CreateDXGIFactory1Fn = HRESULT(WINAPI *)(REFIID, void **);
    auto pCreateFactory1 = reinterpret_cast<CreateDXGIFactory1Fn>(
        GetProcAddress(dxgiMod, "CreateDXGIFactory1"));
    if (pCreateFactory1 && SUCCEEDED(pCreateFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory)))) {
      IDXGIAdapter1 *cur = nullptr;
      for (UINT i = 0; SUCCEEDED(factory->EnumAdapters1(i, &cur)); ++i) {
        DXGI_ADAPTER_DESC1 desc = {};
        if (SUCCEEDED(cur->GetDesc1(&desc))) {
          if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            hardwareAdapter = cur;
            break;
          }
        }
        cur->Release();
      }
    }
  }

  HMODULE d3d12 = GetModuleHandleA("d3d12.dll");
  if (!d3d12)
    d3d12 = LoadLibraryA("d3d12.dll");

  if (d3d12) {
    using PFN_D3D12_CREATE_DEVICE =
        HRESULT(WINAPI *)(IUnknown *, D3D_FEATURE_LEVEL, REFIID, void **);
    auto pCreate12 = reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(
        GetProcAddress(d3d12, "D3D12CreateDevice"));
    if (pCreate12) {
      const D3D_FEATURE_LEVEL levels12[] = {
          static_cast<D3D_FEATURE_LEVEL>(0xc200),
          D3D_FEATURE_LEVEL_12_1,
          D3D_FEATURE_LEVEL_12_0,
          D3D_FEATURE_LEVEL_11_1,
          D3D_FEATURE_LEVEL_11_0};
      for (auto lvl : levels12) {
        HRESULT hr = pCreate12(hardwareAdapter, lvl, __uuidof(ID3D12Device), nullptr);
        if (!SUCCEEDED(hr) && hardwareAdapter) {
          hr = pCreate12(nullptr, lvl, __uuidof(ID3D12Device), nullptr);
        }
        if (SUCCEEDED(hr)) {
          if (hardwareAdapter) hardwareAdapter->Release();
          if (factory) factory->Release();
          if (lvl == static_cast<D3D_FEATURE_LEVEL>(0xc200))
            return "Direct3D 12.2 (Feature Level 12_2 Ultimate)";
          if (lvl == D3D_FEATURE_LEVEL_12_1)
            return "Direct3D 12.1 (Feature Level 12_1)";
          if (lvl == D3D_FEATURE_LEVEL_12_0)
            return "Direct3D 12.0 (Feature Level 12_0)";
          if (lvl == D3D_FEATURE_LEVEL_11_1)
            return "Direct3D 11.1 (Feature Level 11_1)";
          if (lvl == D3D_FEATURE_LEVEL_11_0)
            return "Direct3D 11.0 (Feature Level 11_0)";
        }
      }
    }
  }

  HMODULE d3d11 = GetModuleHandleA("d3d11.dll");
  if (!d3d11)
    d3d11 = LoadLibraryA("d3d11.dll");

  if (d3d11) {
    using PFN_D3D11_CREATE_DEVICE = HRESULT(WINAPI *)(
        IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT,
        const D3D_FEATURE_LEVEL *, UINT, UINT,
        void **, D3D_FEATURE_LEVEL *, void **);
    auto pCreate11 = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(
        GetProcAddress(d3d11, "D3D11CreateDevice"));
    if (pCreate11) {
      const D3D_FEATURE_LEVEL levels11[] = {
          D3D_FEATURE_LEVEL_11_1,
          D3D_FEATURE_LEVEL_11_0,
          D3D_FEATURE_LEVEL_10_1,
          D3D_FEATURE_LEVEL_10_0,
          D3D_FEATURE_LEVEL_9_3};
      D3D_FEATURE_LEVEL detectedLevel = D3D_FEATURE_LEVEL_9_1;
      const D3D_DRIVER_TYPE driverType = hardwareAdapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
      if (SUCCEEDED(pCreate11(hardwareAdapter, driverType, nullptr, 0,
                              levels11, 5, 7, nullptr, &detectedLevel, nullptr))) {
        if (hardwareAdapter) hardwareAdapter->Release();
        if (factory) factory->Release();
        if (detectedLevel == D3D_FEATURE_LEVEL_11_1)
          return "Direct3D 11.1 (Feature Level 11_1)";
        if (detectedLevel == D3D_FEATURE_LEVEL_11_0)
          return "Direct3D 11.0 (Feature Level 11_0)";
        if (detectedLevel == D3D_FEATURE_LEVEL_10_1)
          return "Direct3D 10.1 (Feature Level 10_1)";
        if (detectedLevel == D3D_FEATURE_LEVEL_10_0)
          return "Direct3D 10.0 (Feature Level 10_0)";
        if (detectedLevel == D3D_FEATURE_LEVEL_9_3)
          return "Direct3D 9.3 (Feature Level 9_3)";
      }
    }
  }

  if (hardwareAdapter) hardwareAdapter->Release();
  if (factory) factory->Release();
  return "Direct3D 11";
}

std::string FormatProcessStats() {
  DWORD handleCount = 0;
  GetProcessHandleCount(GetCurrentProcess(), &handleCount);

  DWORD threadCount = 0;
  HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (hSnap != INVALID_HANDLE_VALUE) {
    THREADENTRY32 te = {};
    te.dwSize = sizeof(te);
    const DWORD pid = GetCurrentProcessId();
    if (Thread32First(hSnap, &te)) {
      do {
        if (te.th32OwnerProcessID == pid) {
          threadCount++;
        }
      } while (Thread32Next(hSnap, &te));
    }
    CloseHandle(hSnap);
  }

  char buf[96] = {};
  if (threadCount > 0 && handleCount > 0) {
    snprintf(buf, sizeof(buf), "%lu threads, %lu open handles", threadCount, handleCount);
  } else if (handleCount > 0) {
    snprintf(buf, sizeof(buf), "%lu open handles", handleCount);
  } else {
    return "";
  }
  return buf;
}

std::string FormatVirtualMemoryInfo() {
  SYSTEM_INFO sysInfo = {};
  GetSystemInfo(&sysInfo);
  const SIZE_T largePageMin = GetLargePageMinimum();

  const double userSpaceTb =
      static_cast<double>(reinterpret_cast<uintptr_t>(sysInfo.lpMaximumApplicationAddress)) /
      (1024.0 * 1024.0 * 1024.0 * 1024.0);

  char buf[128] = {};
  if (largePageMin > 0) {
    snprintf(buf, sizeof(buf), "%.1f TB address space (64 KB alloc, %zu MB large pages)",
             userSpaceTb, largePageMin / (1024 * 1024));
  } else {
    snprintf(buf, sizeof(buf), "%.1f TB address space (64 KB alloc, 4 KB pages)",
             userSpaceTb);
  }
  return buf;
}

std::string FormatEnvironmentInfo() {
  if (IsDebuggerPresent()) {
    return "Debugger Attached (Active Debugging)";
  }
  return "Native Execution (No Debugger Attached)";
}

std::string FormatProcessName() {
  char path[MAX_PATH] = {};
  if (!GetModuleFileNameA(nullptr, path, MAX_PATH))
    return "(unknown)";
  std::string full(path);
  const size_t pos = full.find_last_of("\\/");
  return (pos == std::string::npos) ? full : full.substr(pos + 1);
}

char *ModuleAnchor() {
  static char anchor = 0;
  return &anchor;
}

HMODULE OwnModule() {
  HMODULE mod = nullptr;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                     reinterpret_cast<LPCSTR>(ModuleAnchor()), &mod);
  return mod;
}

std::string FormatOwnModule() {
  HMODULE mod = OwnModule();
  if (!mod)
    return "(unknown)";
  FreeLibrary(mod);

  char path[MAX_PATH] = {};
  if (!GetModuleFileNameA(mod, path, MAX_PATH))
    return "(unknown)";
  std::string full(path);
  const size_t pos = full.find_last_of("\\/");
  return (pos == std::string::npos) ? full : full.substr(pos + 1);
}

std::string FormatOwnModuleBase() {
  HMODULE mod = OwnModule();
  if (!mod)
    return "(unknown)";
  FreeLibrary(mod);
  char buf[32] = {};
  snprintf(buf, sizeof(buf), "%p", reinterpret_cast<void *>(mod));
  return buf;
}

std::string SectionTitleForId(const std::string &id) {
  for (const SectionDef &def : kSections) {
    if (id == def.id)
      return def.title;
  }
  return "RUNTIME LIFECYCLE";
}

const char *SectionForTag(const std::string &tag) {
  for (const TagDef &def : kTagSections) {
    if (tag == def.tag)
      return def.section;
  }
  return nullptr;
}

bool ExtractLeadingTag(const std::string &in, std::string &tag,
                       std::string &rest) {
  size_t i = 0;
  std::string last;
  bool any = false;

  while (i < in.size() && in[i] == '[') {
    const size_t close = in.find(']', i);
    if (close == std::string::npos || close == i + 1)
      break;
    const std::string candidate = in.substr(i + 1, close - i - 1);
    bool valid = true;
    for (char c : candidate) {
      if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-' &&
          c != '.') {
        valid = false;
        break;
      }
    }
    if (!valid)
      break;
    last = candidate;
    any = true;
    i = close + 1;
  }

  if (!any)
    return false;
  tag = last;
  TrimLeading(in, i);
  rest = in.substr(i);
  return true;
}

const char *kWarningPhrases[] = {
    "failed",     "failure",   "could not",  "couldn't", "unable to",
    "timed out",  "not found", "unsupported", "not resolved",
    "not available", "out of memory",
};

const char *kErrorPhrases[] = {
    "fatal",     "access violation", "unhandled exception",
    "stack overflow",
};

const char *kBenignPhrases[] = {
    "0 failed",     "0 failure",  "0 error",     "0 errors",   "no error",   "no errors",
    "0 warnings",   "trying",     "retrying",    "fallback",   "falls back",
    "heuristic",    "skipped by", "left as it was", "not resolved on this build",
    "could not be resolved on this build", "0 not found",
};

}

const char *LogLevelTag(LogLevel level) {
  switch (level) {
  case LogLevel::Trace:
    return "TRC";
  case LogLevel::Debug:
    return "DBG";
  case LogLevel::Info:
    return "INF";
  case LogLevel::Warning:
    return "WRN";
  case LogLevel::Error:
    return "ERR";
  case LogLevel::Fatal:
    return "FTL";
  }
  return "???";
}

namespace LogSession {

namespace {

SectionState *FindSection(const std::string &id) {
  for (SectionState &s : g_Sections) {
    if (s.id == id)
      return &s;
  }
  return nullptr;
}

SectionState &OpenSection(const std::string &id, long long nowMs) {
  std::string targetId = id;
  bool valid = false;
  for (const SectionDef &def : kSections) {
    if (targetId == def.id) {
      valid = true;
      break;
    }
  }
  if (!valid) {
    targetId = "runtime";
  }

  if (SectionState *existing = FindSection(targetId))
    return *existing;

  SectionState s;
  s.id = targetId;
  s.title = SectionTitleForId(targetId);
  s.firstMs = nowMs;
  s.lastMs = nowMs;
  g_Sections.push_back(s);
  return g_Sections.back();
}

void EmitRawLocked(const std::string &line) {
  if (g_File) {
    fputs(line.c_str(), g_File);
    fputc('\n', g_File);
    fflush(g_File);
  }

  OutputDebugStringA((line + "\n").c_str());
}

void EmitBannerLocked(const std::string &text) { EmitRawLocked(text); }

void PrintFieldLocked(const std::string &key, const std::string &value) {
  if (value.empty())
    return;
  EmitRawLocked("  " + PadRight(key, 14) + " : " + value);
}

void PrintDetailLocked(const std::string &value) {
  if (value.empty())
    return;
  EmitRawLocked("  " + PadRight("", 14) + "   " + value);
}

bool RotateLocked() {
  if (g_Path.empty())
    return false;

  WIN32_FILE_ATTRIBUTE_DATA data = {};
  if (!GetFileAttributesExA(g_Path.c_str(), GetFileExInfoStandard, &data))
    return false;
  const uint64_t size = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) |
                        data.nFileSizeLow;
  if (size == 0)
    return false;

  const std::string oldest =
      g_Path + "." + std::to_string(kRotationKeep) + ".log";
  DeleteFileA(oldest.c_str());
  for (int i = kRotationKeep - 1; i >= 1; --i) {
    const std::string from = g_Path + "." + std::to_string(i) + ".log";
    const std::string to = g_Path + "." + std::to_string(i + 1) + ".log";
    if (GetFileAttributesA(from.c_str()) != INVALID_FILE_ATTRIBUTES)
      MoveFileA(from.c_str(), to.c_str());
  }
  return MoveFileA(g_Path.c_str(), (g_Path + ".1.log").c_str()) != 0;
}

void WriteHeaderLocked() {
  if (g_HeaderWritten)
    return;
  g_HeaderWritten = true;

  EmitBannerLocked("╭── ⋅ ── ✩ ── ⋅ ── ✩ ── ⋅ ──╮");
  EmitBannerLocked("  𝑨𝒗𝒂𝒍𝒐𝒏𝒊𝒖𝒎 𝑰𝒔 𝑳𝒐𝒂𝒅𝒊𝒏𝒈. . .");
  EmitBannerLocked("╰── ⋅ ── ✩ ── ⋅ ── ✩ ── ⋅ ──╯");

  PrintFieldLocked("Log file", g_Path);
  PrintFieldLocked("Session start", FormatLocalTimestamp());
  PrintFieldLocked("Host", FormatHostOs() + " (" +
                                std::to_string(sizeof(void *) * 8) + "-bit)");
  PrintFieldLocked("Hypervisor", FormatHypervisorInfo());
  PrintFieldLocked("CPU", FormatCpuBrand());
  PrintFieldLocked("CPU topology", FormatCpuTopology());
  PrintFieldLocked("CPU cache", FormatCpuCache());
  PrintFieldLocked("CPU features", FormatCpuFeatures());
  PrintFieldLocked("GPU", FormatGpuInfo());
  PrintFieldLocked("DirectX", FormatDirectXCapabilities());
  PrintFieldLocked("RAM", FormatRamInfo());
  PrintFieldLocked("Virtual mem", FormatVirtualMemoryInfo());
  PrintFieldLocked("Process mem", FormatProcessMem());
  PrintFieldLocked("Display", FormatDisplayInfo());
  PrintFieldLocked("Game drive", FormatGameDrive());
  PrintFieldLocked("Timer res", FormatTimerResolution());
  PrintFieldLocked("Process", FormatProcessName() + " (PID " +
                                  std::to_string(GetCurrentProcessId()) +
                                  FormatProcessElevatedSuffix() + ")");
  PrintFieldLocked("Process stats", FormatProcessStats());
  PrintFieldLocked("Binary info", FormatBinaryInfo());
  PrintFieldLocked("Base address", FormatBaseAddress());
  PrintFieldLocked("Environment", FormatEnvironmentInfo());
  PrintFieldLocked("Module", FormatOwnModule() + " @ " + FormatOwnModuleBase());
  PrintFieldLocked("Runtime", std::string("Universal build, ") +
                                  kSupportedEngineRange);

  char levels[96] = {};
  snprintf(levels, sizeof(levels), "Avalonium Engine %s%s",
           LogLevelTag(g_MinLevel), g_Verbose ? ", verbose" : "");
  PrintFieldLocked("Verbosity", levels);

}

void WriteSectionBannerLocked(SectionState &s, size_t index) {
  if (s.bannerWritten)
    return;
  s.bannerWritten = true;
  char prefix[64] = {};
  snprintf(prefix, sizeof(prefix), "--[ %zu ]  %s ", index + 1, s.title.c_str());
  const size_t used = std::strlen(prefix);
  const size_t pad = used < static_cast<size_t>(kRuleWidth)
                         ? static_cast<size_t>(kRuleWidth) - used
                         : 1;
  EmitBannerLocked("");
  EmitBannerLocked(prefix + Repeat("-", static_cast<int>(pad)));
}

void WriteSectionEndBannerLocked(SectionState &s) {
  if (s.closed)
    return;
  s.closed = true;
  char prefix[64] = {};
  snprintf(prefix, sizeof(prefix), "--End of %s ", s.title.c_str());
  const size_t used = std::strlen(prefix);
  const size_t pad = used < static_cast<size_t>(kRuleWidth)
                         ? static_cast<size_t>(kRuleWidth) - used
                         : 1;
  EmitBannerLocked(prefix + Repeat("-", static_cast<int>(pad)));
}

void BeginLocked() {
  if (g_Started)
    return;
  g_Started = true;

  if (g_ThreadNames.find(1) == g_ThreadNames.end()) {

    g_ThreadNames[1] = "main";
  }

  if (g_Path.empty())
    return;

  RotateLocked();

  g_File = fopen(g_Path.c_str(), "a+");
  if (!g_File)
    return;
  fseek(g_File, 0, SEEK_END);

  WriteHeaderLocked();
}

void EmitAllLocked(const char *tag, const std::string &text, LogLevel level,
                   long long nowMs) {
  if (text.empty())
    return;

  std::vector<std::string> lines;
  {
    std::string current;
    for (char c : text) {
      if (c == '\n') {
        TrimTrailing(current);
        lines.push_back(current);
        current.clear();
      } else if (c != '\r') {
        current.push_back(c);
      }
    }
    TrimTrailing(current);
    lines.push_back(current);
  }

  for (std::string &line : lines) {
    if (line.empty())
      continue;

    std::string effectiveTag;
    std::string message = line;
    if (!ExtractLeadingTag(line, effectiveTag, message)) {
      if (tag)
        effectiveTag = tag;
    }
    if (message.empty())
      continue;

    std::string sectionId = t_CurrentSection;
    const char *mapped =
        effectiveTag.empty() ? nullptr : SectionForTag(effectiveTag);
    if (mapped)
      sectionId = mapped;

    if (effectiveTag.empty() && g_ActiveSectionId == "lua") {
      sectionId = "lua";
    }

    if (!g_ExclusiveSectionId.empty()) {
      if (t_CurrentSection == g_ExclusiveSectionId) {
        sectionId = g_ExclusiveSectionId;
      } else if (sectionId != g_ExclusiveSectionId) {
        g_BufferedEntries.push_back({effectiveTag, message, level, nowMs});
        continue;
      }
    }

    if (level < g_MinLevel)
      continue;
    if (!g_Verbose && level <= LogLevel::Debug)
      continue;

    SectionState &section = OpenSection(sectionId, nowMs);

    if (section.closed && (sectionId == "lua" || sectionId == "level_transition")) {
      section.closed = false;
      section.bannerWritten = false;
    }

    g_ActiveSectionId = sectionId;

    const bool sectionIsNew = !section.bannerWritten;
    section.entries++;
    section.lastMs = nowMs;
    if (level >= LogLevel::Warning && level < LogLevel::Error)
      section.warnings++;
    if (level >= LogLevel::Error)
      section.errors++;

    g_Counts[static_cast<int>(level)]++;
    ++g_Sequence;

    if (sectionIsNew) {
      size_t sectionIndex = 0;
      for (size_t i = 0; i < g_Sections.size(); ++i) {
        if (g_Sections[i].id == section.id) {
          sectionIndex = i;
          break;
        }
      }
      WriteSectionBannerLocked(section, sectionIndex);
    }

    std::string prefix = FormatClock(nowMs);
    prefix += " [";
    prefix += effectiveTag;
    prefix += "] ";

    EmitRawLocked(prefix + message);
  }
}

long long NowMsLocked() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - g_SessionStart)
      .count();
}

}

void SetThreadName(const char *name) {
  if (!name || !*name)
    return;
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_ThreadNames[GetCurrentThreadId()] = name;
}

bool SetSection(const char *id) {
  if (!id || !*id)
    return false;
  bool valid = false;
  for (const SectionDef &def : kSections) {
    if (std::strcmp(id, def.id) == 0) {
      valid = true;
      break;
    }
  }
  if (!valid)
    return false;
  if (t_CurrentSection == id)
    return false;
  t_CurrentSection = id;
  return true;
}

const char *CurrentSectionId() { return t_CurrentSection.c_str(); }

void BeginExclusiveSection(const char *id) {
  if (!id || !*id)
    return;
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (!g_Started)
    BeginLocked();

  if (id && std::strcmp(id, "level_transition") == 0) {
    SectionState *luaSec = FindSection("lua");
    if (luaSec && luaSec->bannerWritten && !luaSec->closed) {
      WriteSectionEndBannerLocked(*luaSec);
    }
  }

  g_ExclusiveSectionId = id;
  g_ActiveSectionId = id;
  t_CurrentSection = id;
  SectionState &section = OpenSection(id, NowMsLocked());

  if (section.closed) {
    section.closed = false;
    section.bannerWritten = false;
  }

  if (!section.bannerWritten) {
    size_t sectionIndex = 0;
    for (size_t i = 0; i < g_Sections.size(); ++i) {
      if (g_Sections[i].id == section.id) {
        sectionIndex = i;
        break;
      }
    }
    WriteSectionBannerLocked(section, sectionIndex);
  }
}

void EndSection(const char *id) {
  std::lock_guard<std::mutex> lock(g_Mutex);
  std::string targetId;
  if (id && *id) {
    targetId = id;
  } else if (!g_ExclusiveSectionId.empty()) {
    targetId = g_ExclusiveSectionId;
  } else {
    targetId = t_CurrentSection;
  }

  SectionState *sec = FindSection(targetId);
  if (sec) {
    WriteSectionEndBannerLocked(*sec);
  }

  if (g_ExclusiveSectionId == targetId) {
    g_ExclusiveSectionId.clear();
    std::vector<BufferedLogEntry> toFlush = std::move(g_BufferedEntries);
    g_BufferedEntries.clear();
    for (const auto &entry : toFlush) {
      EmitAllLocked(entry.tag.empty() ? nullptr : entry.tag.c_str(),
                    entry.text, entry.level, entry.nowMs);
    }
  }

  if (g_ActiveSectionId == targetId) {
    g_ActiveSectionId.clear();
  }

  if (t_CurrentSection == targetId) {
    t_CurrentSection = "runtime";
  }
}

void SetMinLevel(LogLevel level) {
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_MinLevel = level;
}

LogLevel MinLevel() {
  std::lock_guard<std::mutex> lock(g_Mutex);
  return g_MinLevel;
}

void SetVerbose(bool verbose) {
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_Verbose = verbose;
}

bool Verbose() {
  std::lock_guard<std::mutex> lock(g_Mutex);
  return g_Verbose;
}

void SetFact(const char *key, const std::string &value) {
  if (!key || !*key)
    return;
  std::lock_guard<std::mutex> lock(g_Mutex);
  auto it = g_FactIndex.find(key);
  if (it != g_FactIndex.end()) {
    for (auto &fact : g_Facts) {
      if (fact.first == key) {
        fact.second = value;
        break;
      }
    }
    it->second = value;
    return;
  }
  g_Facts.emplace_back(key, value);
  g_FactIndex.emplace(key, value);
}

void SetFact(const char *key, const char *value) {
  SetFact(key, std::string(value ? value : ""));
}

const char *GetFact(const char *key) {
  if (!key)
    return nullptr;
  std::lock_guard<std::mutex> lock(g_Mutex);
  auto it = g_FactIndex.find(key);
  return (it == g_FactIndex.end()) ? nullptr : it->second.c_str();
}

void SetResult(const char *outcome, const char *detail) {
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_Outcome = outcome ? outcome : "";
  g_OutcomeDetail = detail ? detail : "";
}

LogLevel ClassifyMessage(const std::string &text) {
  const std::string lower = ToLowerAscii(text);

  for (const char *phrase : kBenignPhrases) {
    if (Contains(lower, phrase))
      return LogLevel::Info;
  }
  for (const char *phrase : kErrorPhrases) {
    if (Contains(lower, phrase))
      return LogLevel::Error;
  }
  for (const char *phrase : kWarningPhrases) {
    if (Contains(lower, phrase))
      return LogLevel::Warning;
  }
  return LogLevel::Info;
}

void Write(const char *tag, const std::string &text, LogLevel level) {
  if (text.empty())
    return;

  std::lock_guard<std::mutex> lock(g_Mutex);

  if (!g_Started)
    BeginLocked();

  EmitAllLocked(tag, text, level, NowMsLocked());
}

void SetPath(const std::string &path) {
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (g_Started)
    return;
  g_Path = path;
}

void Begin() {
  std::lock_guard<std::mutex> lock(g_Mutex);
  BeginLocked();
}

void End(const char *reason) {
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (!g_Started || g_FooterWritten)
    return;

  if (!g_ExclusiveSectionId.empty()) {
    SectionState *sec = FindSection(g_ExclusiveSectionId);
    if (sec) {
      WriteSectionEndBannerLocked(*sec);
    }
    g_ExclusiveSectionId.clear();
    std::vector<BufferedLogEntry> toFlush = std::move(g_BufferedEntries);
    g_BufferedEntries.clear();
    for (const auto &entry : toFlush) {
      EmitAllLocked(entry.tag.empty() ? nullptr : entry.tag.c_str(),
                    entry.text, entry.level, entry.nowMs);
    }
  }

  SectionState *luaSec = FindSection("lua");
  if (luaSec && luaSec->bannerWritten && !luaSec->closed) {
    WriteSectionEndBannerLocked(*luaSec);
  }

  g_ActiveSectionId.clear();
  g_FooterWritten = true;

  const long long totalMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - g_SessionStart)
          .count();

  char line[256] = {};
  EmitBannerLocked(Repeat("←→", kRuleWidth / 2));
  EmitBannerLocked(" SESSION SUMMARY");
  EmitBannerLocked(Repeat("←→", kRuleWidth / 2));

  std::string outcome = g_Outcome.empty() ? "UNREPORTED" : g_Outcome;
  PrintFieldLocked("Outcome", outcome);
  PrintDetailLocked(g_OutcomeDetail);
  PrintFieldLocked("Duration", FormatDuration(totalMs));
  PrintFieldLocked("Entries", std::to_string(g_Sequence));

  snprintf(line, sizeof(line),
           "%u at info, %u warning%s, %u error%s (trace/debug %s)",
           g_Counts[2], g_Counts[3], g_Counts[3] == 1 ? "" : "s", g_Counts[4],
           g_Counts[4] == 1 ? "" : "s",
           (g_Counts[0] + g_Counts[1]) == 0 ? "hidden" : "shown");
  PrintFieldLocked("By level", line);

  if (reason && *reason)
    PrintFieldLocked("Closed by", reason);

  if (!g_Facts.empty()) {
    EmitBannerLocked("");
    EmitBannerLocked("  DETECTED AT RUNTIME");
    for (const auto &fact : g_Facts)
      PrintFieldLocked(fact.first, fact.second);
  }

  if (!g_Sections.empty()) {
    EmitBannerLocked("");
    snprintf(line, sizeof(line), "  %-3s %-30s %8s %12s %5s %5s", "#",
             "SECTION", "ENTRIES", "SPAN", "WARN", "ERR");
    EmitBannerLocked(line);
    EmitBannerLocked("  " + Repeat("-", kRuleWidth - 2));

    uint32_t entries = 0;
    uint32_t warnings = 0;
    uint32_t errors = 0;
    for (size_t i = 0; i < g_Sections.size(); ++i) {
      const SectionState &s = g_Sections[i];
      entries += s.entries;
      warnings += s.warnings;
      errors += s.errors;
      snprintf(line, sizeof(line), "  %-3zu %-30s %8u %12s %5u %5u", i + 1,
               s.title.c_str(), s.entries,
               FormatDuration(s.lastMs - s.firstMs).c_str(), s.warnings,
               s.errors);
      EmitBannerLocked(line);
    }

    EmitBannerLocked("  " + Repeat("-", kRuleWidth - 2));
    snprintf(line, sizeof(line), "  %-3s %-30s %8u %12s %5u %5u", "",
             "TOTAL", entries, FormatDuration(totalMs).c_str(), warnings,
             errors);
    EmitBannerLocked(line);
  }

  EmitBannerLocked(Repeat("←→", kRuleWidth / 2));

  if (g_File) {
    fclose(g_File);
    g_File = nullptr;
  }
}

}
