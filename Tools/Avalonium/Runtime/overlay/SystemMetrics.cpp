#include "SystemMetrics.h"

#include "../core/Logger.h"

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <mutex>
#include <vector>

namespace SystemMetrics {
namespace {

constexpr ULONGLONG kPollIntervalMs = 250;
constexpr uint32_t kMaxAdapters = 16;
constexpr int32_t kQueryTypeAdapterPerfData = 62;
constexpr int32_t kQueryTypeAdapterPerfDataCaps = 63;

struct Luid {
    uint32_t LowPart;
    int32_t HighPart;
};

struct AdapterInfo {
    uint32_t hAdapter;
    Luid AdapterLuid;
    uint32_t NumOfSources;
    int32_t bPrecisePresentRegionsPreferred;
};

struct EnumAdapters2 {
    uint32_t NumAdapters;
    AdapterInfo *pAdapters;
};

struct OpenAdapterFromLuid {
    Luid AdapterLuid;
    uint32_t hAdapter;
};

struct QueryAdapterInfo {
    uint32_t hAdapter;
    int32_t Type;
    void *pPrivateDriverData;
    uint32_t PrivateDriverDataSize;
};

struct CloseAdapter {
    uint32_t hAdapter;
};

struct AdapterPerfData {
    uint32_t PhysicalAdapterIndex;
    uint64_t MemoryFrequency;
    uint64_t MaxMemoryFrequency;
    uint64_t MaxMemoryFrequencyOC;
    uint64_t MemoryBandwidth;
    uint64_t PCIEBandwidth;
    uint32_t FanRPM;
    uint32_t Power;
    uint32_t Temperature;
    uint8_t PowerStateOverride;
};

struct AdapterPerfDataCaps {
    uint32_t PhysicalAdapterIndex;
    uint64_t MaxMemoryBandwidth;
    uint64_t MaxPCIEBandwidth;
    uint32_t MaxFanRPM;
    uint32_t TemperatureMax;
    uint32_t TemperatureWarning;
};

using EnumAdapters2Fn = LONG(WINAPI *)(EnumAdapters2 *);
using OpenAdapterFromLuidFn = LONG(WINAPI *)(OpenAdapterFromLuid *);
using QueryAdapterInfoFn = LONG(WINAPI *)(QueryAdapterInfo *);
using CloseAdapterFn = LONG(WINAPI *)(CloseAdapter *);

struct D3dkmtApi {
    EnumAdapters2Fn EnumAdapters2 = nullptr;
    OpenAdapterFromLuidFn OpenAdapterFromLuid = nullptr;
    QueryAdapterInfoFn QueryAdapterInfo = nullptr;
    CloseAdapterFn CloseAdapter = nullptr;
    bool available = false;
};

struct AdapterProbe {
    bool opened = false;
    bool perfOk = false;
    bool capsOk = false;
    AdapterPerfData perf = {};
    AdapterPerfDataCaps caps = {};
};

struct State {
    std::mutex mutex;
    bool initialized = false;
    D3dkmtApi d3dkmt;
    Luid renderAdapter = {};
    bool haveRenderAdapter = false;
    bool temperatureSupported = false;
    uint32_t temperatureMax = 0;
    bool pdhReady = false;
    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER gpuCounter = nullptr;
    std::vector<uint8_t> counterBuffer;
    FILETIME prevIdle = {};
    FILETIME prevKernel = {};
    FILETIME prevUser = {};
    bool cpuPrimed = false;
    ULONGLONG lastPollMs = 0;
    Snapshot snapshot;
};

State g_State;

D3dkmtApi LoadD3dkmt() {
    D3dkmtApi api;
    HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
    if (!gdi) gdi = LoadLibraryW(L"gdi32.dll");
    if (!gdi) return api;
    api.EnumAdapters2 = reinterpret_cast<EnumAdapters2Fn>(GetProcAddress(gdi, "D3DKMTEnumAdapters2"));
    api.OpenAdapterFromLuid = reinterpret_cast<OpenAdapterFromLuidFn>(GetProcAddress(gdi, "D3DKMTOpenAdapterFromLuid"));
    api.QueryAdapterInfo = reinterpret_cast<QueryAdapterInfoFn>(GetProcAddress(gdi, "D3DKMTQueryAdapterInfo"));
    api.CloseAdapter = reinterpret_cast<CloseAdapterFn>(GetProcAddress(gdi, "D3DKMTCloseAdapter"));
    api.available = api.EnumAdapters2 && api.OpenAdapterFromLuid && api.QueryAdapterInfo && api.CloseAdapter;
    return api;
}

AdapterProbe ProbeAdapter(D3dkmtApi &api, const Luid &luid) {
    AdapterProbe probe;
    OpenAdapterFromLuid open = {};
    open.AdapterLuid = luid;
    if (api.OpenAdapterFromLuid(&open) != 0) return probe;
    probe.opened = true;

    QueryAdapterInfo query = {};
    query.hAdapter = open.hAdapter;
    query.Type = kQueryTypeAdapterPerfData;
    query.pPrivateDriverData = &probe.perf;
    query.PrivateDriverDataSize = sizeof(probe.perf);
    probe.perfOk = api.QueryAdapterInfo(&query) == 0;

    query.Type = kQueryTypeAdapterPerfDataCaps;
    query.pPrivateDriverData = &probe.caps;
    query.PrivateDriverDataSize = sizeof(probe.caps);
    probe.capsOk = api.QueryAdapterInfo(&query) == 0;

    CloseAdapter close = {};
    close.hAdapter = open.hAdapter;
    api.CloseAdapter(&close);
    return probe;
}

void DiscoverAdapterLocked() {
    g_State.d3dkmt = LoadD3dkmt();
    if (!g_State.d3dkmt.available) return;

    AdapterInfo adapters[kMaxAdapters] = {};
    EnumAdapters2 enumerate = {};
    enumerate.NumAdapters = kMaxAdapters;
    enumerate.pAdapters = adapters;
    if (g_State.d3dkmt.EnumAdapters2(&enumerate) != 0) return;

    const uint32_t count = (std::min)(enumerate.NumAdapters, kMaxAdapters);
    bool found = false;
    uint64_t bestBandwidth = 0;
    bool bestTemperature = false;
    for (uint32_t i = 0; i < count; ++i) {
        const AdapterProbe probe = ProbeAdapter(g_State.d3dkmt, adapters[i].AdapterLuid);
        if (!probe.opened) continue;
        const uint64_t bandwidth = probe.capsOk ? probe.caps.MaxMemoryBandwidth : 0;
        const bool temperature = probe.capsOk && probe.perfOk && probe.caps.TemperatureMax > 0;
        const bool better = !found || bandwidth > bestBandwidth ||
                            (bandwidth == bestBandwidth && temperature && !bestTemperature);
        if (!better) continue;
        found = true;
        bestBandwidth = bandwidth;
        bestTemperature = temperature;
        g_State.renderAdapter = adapters[i].AdapterLuid;
        g_State.temperatureSupported = temperature;
        g_State.temperatureMax = probe.capsOk ? probe.caps.TemperatureMax : 0;
    }
    g_State.haveRenderAdapter = found;
}

bool OpenGpuUsageLocked() {
    if (PdhOpenQueryW(nullptr, 0, &g_State.query) != ERROR_SUCCESS) return false;
    if (PdhAddEnglishCounterW(g_State.query, L"\\GPU Engine(*)\\Utilization Percentage", 0,
                              &g_State.gpuCounter) != ERROR_SUCCESS) {
        PdhCloseQuery(g_State.query);
        g_State.query = nullptr;
        g_State.gpuCounter = nullptr;
        return false;
    }
    PdhCollectQueryData(g_State.query);
    return true;
}

uint64_t FileTimeValue(const FILETIME &time) {
    ULARGE_INTEGER value = {};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return value.QuadPart;
}

void UpdateCpuLocked() {
    FILETIME idle = {};
    FILETIME kernel = {};
    FILETIME user = {};
    if (!GetSystemTimes(&idle, &kernel, &user)) {
        g_State.snapshot.CpuUsagePercent = -1.0f;
        return;
    }
    if (!g_State.cpuPrimed) {
        g_State.prevIdle = idle;
        g_State.prevKernel = kernel;
        g_State.prevUser = user;
        g_State.cpuPrimed = true;
        g_State.snapshot.CpuUsagePercent = -1.0f;
        return;
    }

    const uint64_t idleDelta = FileTimeValue(idle) - FileTimeValue(g_State.prevIdle);
    const uint64_t kernelDelta = FileTimeValue(kernel) - FileTimeValue(g_State.prevKernel);
    const uint64_t userDelta = FileTimeValue(user) - FileTimeValue(g_State.prevUser);
    g_State.prevIdle = idle;
    g_State.prevKernel = kernel;
    g_State.prevUser = user;

    const uint64_t total = kernelDelta + userDelta;
    if (total == 0) {
        g_State.snapshot.CpuUsagePercent = -1.0f;
        return;
    }
    const double busy = 1.0 - static_cast<double>(idleDelta) / static_cast<double>(total);
    g_State.snapshot.CpuUsagePercent = static_cast<float>(std::clamp(busy * 100.0, 0.0, 100.0));
}

void UpdateGpuUsageLocked() {
    if (!g_State.pdhReady) {
        g_State.snapshot.GpuUsagePercent = -1.0f;
        return;
    }
    if (PdhCollectQueryData(g_State.query) != ERROR_SUCCESS) {
        g_State.snapshot.GpuUsagePercent = -1.0f;
        return;
    }

    DWORD size = 0;
    DWORD count = 0;
    PDH_STATUS status = PdhGetFormattedCounterArrayW(g_State.gpuCounter, PDH_FMT_DOUBLE, &size, &count, nullptr);
    if (status != PDH_MORE_DATA && status != ERROR_SUCCESS) {
        g_State.snapshot.GpuUsagePercent = -1.0f;
        return;
    }
    if (size == 0 || count == 0) {
        g_State.snapshot.GpuUsagePercent = 0.0f;
        return;
    }

    g_State.counterBuffer.resize(size);
    auto *items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(g_State.counterBuffer.data());
    status = PdhGetFormattedCounterArrayW(g_State.gpuCounter, PDH_FMT_DOUBLE, &size, &count, items);
    if (status != ERROR_SUCCESS) {
        g_State.snapshot.GpuUsagePercent = -1.0f;
        return;
    }

    wchar_t luidTag[48] = {};
    if (g_State.haveRenderAdapter) {
        std::swprintf(luidTag, 48, L"luid_0x%08X_0x%08X",
                      static_cast<unsigned>(g_State.renderAdapter.HighPart),
                      static_cast<unsigned>(g_State.renderAdapter.LowPart));
    }

    double sum = 0.0;
    bool matched3D = false;
    for (DWORD i = 0; i < count; ++i) {
        const PDH_FMT_COUNTERVALUE_ITEM_W &item = items[i];
        if (!item.szName || item.FmtValue.CStatus != 0) continue;
        if (!std::wcsstr(item.szName, L"engtype_3D")) continue;
        if (luidTag[0] && !std::wcsstr(item.szName, luidTag)) continue;
        sum += item.FmtValue.doubleValue;
        matched3D = true;
    }
    if (!matched3D && luidTag[0]) {

        for (DWORD i = 0; i < count; ++i) {
            const PDH_FMT_COUNTERVALUE_ITEM_W &item = items[i];
            if (!item.szName || item.FmtValue.CStatus != 0) continue;
            if (!std::wcsstr(item.szName, luidTag)) continue;
            sum += item.FmtValue.doubleValue;
        }
    }

    g_State.snapshot.GpuUsagePercent = static_cast<float>(std::clamp(sum, 0.0, 100.0));
}

void UpdateGpuTemperatureLocked() {
    if (!g_State.temperatureSupported || !g_State.haveRenderAdapter) {
        g_State.snapshot.GpuTemperatureC = -1.0f;
        return;
    }
    const AdapterProbe probe = ProbeAdapter(g_State.d3dkmt, g_State.renderAdapter);
    if (!probe.opened || !probe.perfOk || probe.perf.Temperature == 0) {
        g_State.snapshot.GpuTemperatureC = -1.0f;
        return;
    }
    g_State.snapshot.GpuTemperatureC = static_cast<float>(probe.perf.Temperature) / 10.0f;
}

void InitLocked() {
    if (g_State.initialized) return;
    g_State.initialized = true;

    FILETIME idle = {};
    FILETIME kernel = {};
    FILETIME user = {};
    if (GetSystemTimes(&idle, &kernel, &user)) {
        g_State.prevIdle = idle;
        g_State.prevKernel = kernel;
        g_State.prevUser = user;
        g_State.cpuPrimed = true;
    }

    DiscoverAdapterLocked();
    g_State.pdhReady = OpenGpuUsageLocked();

    char line[320] = {};
    if (g_State.temperatureSupported) {
        std::snprintf(line, sizeof(line),
                      "[SystemMetrics] CPU usage %s; GPU usage %s; GPU temperature ready "
                      "(adapter %08X:%08X, max %u C)",
                      g_State.cpuPrimed ? "ready" : "unavailable",
                      g_State.pdhReady ? "ready" : "unavailable",
                      static_cast<unsigned>(g_State.renderAdapter.HighPart),
                      static_cast<unsigned>(g_State.renderAdapter.LowPart),
                      g_State.temperatureMax);
    } else {
        std::snprintf(line, sizeof(line),
                      "[SystemMetrics] CPU usage %s; GPU usage %s; GPU temperature unavailable on this adapter",
                      g_State.cpuPrimed ? "ready" : "unavailable",
                      g_State.pdhReady ? "ready" : "unavailable");
    }
    EngineDiscovery::WriteLogLine(line);
}

}

void Initialize() {
    std::lock_guard<std::mutex> lock(g_State.mutex);
    InitLocked();
}

Snapshot Poll() {
    std::lock_guard<std::mutex> lock(g_State.mutex);
    if (!g_State.initialized) InitLocked();

    const ULONGLONG now = GetTickCount64();
    if (g_State.lastPollMs != 0 && now - g_State.lastPollMs < kPollIntervalMs) {
        return g_State.snapshot;
    }
    g_State.lastPollMs = now;

    UpdateCpuLocked();
    UpdateGpuUsageLocked();
    UpdateGpuTemperatureLocked();
    return g_State.snapshot;
}

}
