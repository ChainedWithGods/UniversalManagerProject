#include "DataTableLayout.h"

#include "EngineDiscovery.h"
#include "Logger.h"
#include "ObjectUtils.h"
#include "../utils/SafeMemory.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace EngineDiscovery {

namespace {

constexpr int32_t kMaxRows = 50000;
constexpr int32_t kScanStart = 0x08;
constexpr int32_t kScanEnd = 0x200;
constexpr int32_t kStrideMin = 16;
constexpr int32_t kStrideMax = 64;
constexpr int32_t kDeclaredStride = 24;
constexpr int32_t kMaxRowStructSize = 0x40000;
constexpr size_t kMaxCachedTables = 64;
constexpr uint64_t kFailureRetryMs = 30000;

struct RowMapHeader
{
    uint8_t* Data;
    int32_t ArrayNum;
    int32_t ArrayMax;
};

bool ReadHeader(const uint8_t* base, int32_t offset, RowMapHeader& header)
{
    return SafeMem_Read(base + offset, &header, sizeof(header)) != 0;
}

int32_t RowStructSize(const EngineState& state, const void* rowStruct)
{
    if (!rowStruct) return 0;
    const int32_t sizeOffset = UStructPropertiesSizeOffset(state);
    if (sizeOffset < 0) return 0;
    int32_t size = 0;
    if (!SafeMem_Read(static_cast<const uint8_t*>(rowStruct) + sizeOffset, &size,
                      sizeof(size)))
        return 0;
    if (size <= 0 || size > kMaxRowStructSize) return 0;
    return size;
}

bool IsPlausibleRowName(const EngineState& state, const FName& name)
{

    if (name.ComparisonIndex == 0) return false;
    const std::string text = FNameToStr(state, name);
    return !text.empty() && text[0] != '<';
}

bool ElementValidates(const EngineState& state, const uint8_t* element, int32_t rowSize)
{
    if (!SafeMem_IsReadable(element, sizeof(FName) + sizeof(void*))) return false;
    FName name{};
    if (!SafeMem_Read(element, &name, sizeof(name))) return false;
    if (!IsPlausibleRowName(state, name)) return false;
    void* row = nullptr;
    if (!SafeMem_Read(element + sizeof(FName), &row, sizeof(row))) return false;
    if (!row) return false;
    const size_t need = rowSize > 0 ? static_cast<size_t>(rowSize) : sizeof(uint32_t);
    return SafeMem_IsReadable(row, need) != 0;
}

int32_t MeasureStride(const EngineState& state, const uint8_t* data, int32_t count,
                      int32_t rowSize)
{
    auto allValidate = [&](int32_t stride) {
        for (int32_t i = 0; i < count; ++i) {
            if (!ElementValidates(state, data + static_cast<size_t>(i) * stride, rowSize))
                return false;
        }
        return true;
    };

    if (allValidate(kDeclaredStride)) return kDeclaredStride;
    for (int32_t stride = kStrideMin; stride <= kStrideMax; stride += 8) {
        if (stride == kDeclaredStride) continue;
        if (allValidate(stride)) return stride;
    }
    return 0;
}

bool LooksLikeUStruct(const EngineState& state, void* candidate)
{
    if (!candidate || !SafeMem_IsReadable(candidate, 0x30)) return false;
    UClass* candidateClass = GetObjectClass(state, candidate);
    if (!candidateClass || !state.UStructClass) return false;
    return IsStructA(state, reinterpret_cast<UStruct*>(candidateClass),
                     reinterpret_cast<UStruct*>(state.UStructClass));
}

UStruct* ReadRowStructBefore(const EngineState& state, const uint8_t* base, int32_t mapOffset)
{
    if (mapOffset < static_cast<int32_t>(sizeof(void*))) return nullptr;
    void* candidate = nullptr;
    if (!SafeMem_Read(base + mapOffset - sizeof(void*), &candidate, sizeof(candidate)))
        return nullptr;
    return LooksLikeUStruct(state, candidate) ? static_cast<UStruct*>(candidate) : nullptr;
}

struct Candidate
{
    int32_t MapOffset = -1;
    int32_t Stride = 0;
    UStruct* RowStruct = nullptr;
    uint8_t* Data = nullptr;
    int32_t ArrayNum = 0;
    bool Validated = false;
};

bool TryResolveAt(const EngineState& state, const uint8_t* base, int32_t mapOffset,
                  Candidate& out)
{
    RowMapHeader header{};
    if (!ReadHeader(base, mapOffset, header)) return false;
    if (header.ArrayNum < 0 || header.ArrayNum > kMaxRows) return false;
    if (header.ArrayMax < header.ArrayNum || header.ArrayMax > (1 << 20)) return false;

    UStruct* rowStruct = ReadRowStructBefore(state, base, mapOffset);
    const int32_t rowSize = RowStructSize(state, rowStruct);

    if (header.ArrayNum == 0) {

        if (!header.Data && !rowStruct) return false;
        out = {mapOffset, 0, rowStruct, header.Data, 0, false};
        return true;
    }

    if (!header.Data || !SafeMem_IsReadable(header.Data, sizeof(void*) * 2)) return false;
    const int32_t stride = MeasureStride(state, header.Data, header.ArrayNum, rowSize);
    if (stride <= 0) return false;

    out = {mapOffset, stride, rowStruct, header.Data, header.ArrayNum, true};
    return true;
}

struct CacheEntry
{
    Candidate Layout;
    bool Failed = false;
    uint64_t FailedAt = 0;
};

std::mutex g_CacheMutex;
std::unordered_map<const void*, CacheEntry> g_Cache;

bool CachedLayoutStillValid(const EngineState& state, const uint8_t* base,
                            const Candidate& layout)
{
    RowMapHeader header{};
    if (!ReadHeader(base, layout.MapOffset, header)) return false;
    if (header.ArrayNum != layout.ArrayNum || header.Data != layout.Data) return false;
    if (header.ArrayNum == 0) return true;
    const int32_t rowSize = RowStructSize(state, layout.RowStruct);
    const uint8_t* first = header.Data;
    const uint8_t* last =
        header.Data + static_cast<size_t>(header.ArrayNum - 1) * layout.Stride;
    return ElementValidates(state, first, rowSize) && ElementValidates(state, last, rowSize);
}

void StoreEntry(const void* table, CacheEntry entry)
{
    if (g_Cache.size() >= kMaxCachedTables) g_Cache.clear();
    g_Cache[table] = entry;
}

}

bool ResolveDataTableRowMap(EngineState& state, void* dataTable, int32_t rowStructOffsetHint,
                            DataTableRowMap& out)
{
    out = {};
    if (!dataTable || !SafeMem_IsReadable(dataTable, sizeof(void*))) return false;
    const uint8_t* base = static_cast<const uint8_t*>(dataTable);
    const uint64_t now = GetTickCount64();
    const int32_t hint = rowStructOffsetHint > 0 ? rowStructOffsetHint : -1;

    std::lock_guard<std::mutex> lock(g_CacheMutex);

    auto existing = g_Cache.find(dataTable);
    if (existing != g_Cache.end()) {
        CacheEntry& entry = existing->second;
        if (!entry.Failed && CachedLayoutStillValid(state, base, entry.Layout)) {
            const Candidate& layout = entry.Layout;
            out.Data = layout.Data;
            out.ArrayNum = layout.ArrayNum;
            out.MapOffset = layout.MapOffset;
            out.Stride = layout.Stride;
            out.RowStruct = layout.RowStruct;
            out.Validated = layout.Validated;
            return true;
        }
        if (entry.Failed && now - entry.FailedAt < kFailureRetryMs) return false;
    }

    Candidate hit;
    bool resolved = false;
    if (hint > 0) {
        resolved = TryResolveAt(state, base, hint + static_cast<int32_t>(sizeof(void*)), hit);
    }
    if (!resolved) {
        for (int32_t offset = kScanStart; offset <= kScanEnd; offset += 8) {
            if (hint > 0 && offset == hint + static_cast<int32_t>(sizeof(void*))) continue;
            if (TryResolveAt(state, base, offset, hit)) {
                resolved = true;
                break;
            }
        }
    }

    const std::string tableName = GetObjectNameStr(state, static_cast<UObject*>(dataTable));

    if (resolved) {
        const bool fromHint = hint > 0 && hit.MapOffset == hint + static_cast<int32_t>(sizeof(void*));
        Log("[DataTable] %s: row map at +0x%X (%s), stride %d, %d rows", tableName.c_str(),
            hit.MapOffset, fromHint ? "reflected RowStruct" : "validated scan", hit.Stride,
            hit.ArrayNum);
        if (hit.Validated && hit.Stride != kDeclaredStride) {
            Log("[DataTable] %s: stride %d won validation over the declared %d",
                tableName.c_str(), hit.Stride, kDeclaredStride);
        }

        CacheEntry entry;
        entry.Layout = hit;
        StoreEntry(dataTable, entry);

        out.Data = hit.Data;
        out.ArrayNum = hit.ArrayNum;
        out.MapOffset = hit.MapOffset;
        out.Stride = hit.Stride;
        out.RowStruct = hit.RowStruct;
        out.Validated = hit.Validated;
        return true;
    }

    const bool alreadyLogged = existing != g_Cache.end() && existing->second.Failed;
    if (!alreadyLogged) {
        Log("[DataTable] %s: row map did not resolve; no offset or stride validated",
            tableName.c_str());
    }
    CacheEntry failed;
    failed.Failed = true;
    failed.FailedAt = now;
    StoreEntry(dataTable, failed);
    return false;
}

UStruct* GetDataTableRowStruct(EngineState& state, void* dataTable, int32_t rowStructOffsetHint)
{
    if (!dataTable) return nullptr;
    DataTableRowMap layout{};
    if (ResolveDataTableRowMap(state, dataTable, rowStructOffsetHint, layout) && layout.RowStruct)
        return layout.RowStruct;
    if (rowStructOffsetHint > 0) {
        void* direct = nullptr;
        if (SafeMem_Read(static_cast<const uint8_t*>(dataTable) + rowStructOffsetHint, &direct,
                         sizeof(direct)) &&
            LooksLikeUStruct(state, direct))
            return static_cast<UStruct*>(direct);
    }
    return nullptr;
}

int32_t ResolveRowMapStride(EngineState& state, const void* mapData, int32_t arrayNum,
                            void* rowStruct)
{
    if (!mapData || arrayNum <= 0 || arrayNum > kMaxRows) return 0;
    return MeasureStride(state, static_cast<const uint8_t*>(mapData), arrayNum,
                         RowStructSize(state, rowStruct));
}

}
