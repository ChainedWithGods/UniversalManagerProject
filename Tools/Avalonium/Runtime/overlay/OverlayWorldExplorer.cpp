#include "OverlayInternal.h"

void* GetLiveWorldPointer()
{

    if (g_State.GEngine && IsReadableWorldExplorerObject(g_State.GEngine))
    {
        void* engine = g_State.GEngine;
        void* engineClass = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(engine));
        if (engineClass)
        {
            int32_t vpOff = ConsoleEnabler::FindPropertyOffset(g_State, static_cast<UStruct*>(engineClass), "GameViewport");
            if (vpOff >= 0)
            {
                void* viewport = SafeMem_ReadPtr(static_cast<uint8_t*>(engine) + vpOff);
                if (viewport && IsReadableWorldExplorerObject(viewport))
                {
                    void* vpClass = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(viewport));
                    if (vpClass)
                    {
                        int32_t wOff = ConsoleEnabler::FindPropertyOffset(g_State, static_cast<UStruct*>(vpClass), "World");
                        if (wOff >= 0)
                        {
                            void* w = SafeMem_ReadPtr(static_cast<uint8_t*>(viewport) + wOff);
                            if (w && IsReadableWorldExplorerObject(w))
                            {
                                g_State.GWorld = static_cast<UObject*>(w);
                                return w;
                            }
                        }
                    }
                }
            }
        }
    }

    if (g_State.UWorldClass)
    {
        int count = EngineDiscovery::GetObjectCount(g_State);
        for (int i = count - 1; i >= 0; --i)
        {
            UObject* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
            if (!obj || !IsReadableWorldExplorerObject(obj)) continue;
            void* cls = EngineDiscovery::GetObjectClass(g_State, obj);
            if (cls == g_State.UWorldClass)
            {
                static const int worldOffsets[] = { 0x30, 0x38, 0x40, 0x1B0, 0x1A8, 0x120 };
                for (int wOff : worldOffsets)
                {
                    void* lvl = SafeMem_ReadPtr(static_cast<uint8_t*>(static_cast<void*>(obj)) + wOff);
                    if (lvl && IsReadableWorldExplorerObject(lvl))
                    {
                        g_State.GWorld = obj;
                        g_State.UWorld_PersistentLevelOff = wOff;
                        return obj;
                    }
                }
            }
        }
    }

    if (g_State.GWorldPtr)
    {
        void* currentWorld = nullptr;
        if (SafeMem_Read(g_State.GWorldPtr, &currentWorld, sizeof(currentWorld)) && currentWorld && IsReadableWorldExplorerObject(currentWorld))
        {
            g_State.GWorld = static_cast<UObject*>(currentWorld);
            return currentWorld;
        }
    }

    if (g_State.GWorld && IsReadableWorldExplorerObject(g_State.GWorld))
        return g_State.GWorld;

    return nullptr;
}

void* GetLivePersistentLevelPointer()
{
    void* world = GetLiveWorldPointer();
    if (!world)
        return nullptr;

    if (g_State.UWorld_PersistentLevelOff >= 0)
    {
        void* lvl = SafeMem_ReadPtr(static_cast<uint8_t*>(world) + g_State.UWorld_PersistentLevelOff);
        if (lvl && IsReadableWorldExplorerObject(lvl))
            return lvl;
    }

    static const int worldOffsets[] = { 0x30, 0x38, 0x40, 0x1B0, 0x1A8, 0x120 };
    for (int wOff : worldOffsets)
    {
        void* cand = SafeMem_ReadPtr(static_cast<uint8_t*>(world) + wOff);
        if (cand && IsReadableWorldExplorerObject(cand))
        {
            void* candCls = SafeMem_ReadPtr(static_cast<uint8_t*>(cand) + g_State.UObject_ClassOff);
            if (candCls)
            {
                std::string cName = ResolveCachedClassName(candCls);
                if (cName == "Level" || cName.find("Level") != std::string::npos)
                {
                    g_State.UWorld_PersistentLevelOff = wOff;
                    return cand;
                }
            }
        }
    }
    return nullptr;
}

int GetDiscoveryIndexedObjectCount()
{
    if (!g_State.bIndexBuilt || !g_State.IndexMutex)
        return 0;

    std::lock_guard<std::recursive_mutex> lock(*g_State.IndexMutex);
    return static_cast<int>(g_State.SortedNames.size());
}

int GetDiscoveryIndexedActorCount()
{
    if (!g_State.bIndexBuilt || !g_State.AActorClass || !g_State.IndexMutex)
        return 0;

    int count = 0;
    std::lock_guard<std::recursive_mutex> lock(*g_State.IndexMutex);
    for (const auto& pair : g_State.ClassToInstances)
    {
        if (pair.first && IsActorClass(pair.first))
        {
            count += static_cast<int>(pair.second.size());
        }
    }

    return count;
}

bool TryGetPersistentLevelActorCountQuick(int* outCount)
{
    if (outCount)
        *outCount = 0;

    if (!g_State.bInitialized)
        return false;

    if (g_State.UWorld_PersistentLevelOff < 0) {
        UClass* worldClass = g_State.UWorldClass ? g_State.UWorldClass : ConsoleEnabler::FindClassByNameExact(g_State, "World");
        if (worldClass) {
            g_State.UWorld_PersistentLevelOff = ConsoleEnabler::FindPropertyOffset(g_State, (UStruct*)worldClass, "PersistentLevel");
        }
    }
    if (g_State.ULevel_ActorsOff < 0) {
        UClass* levelClass = ConsoleEnabler::FindClassByNameExact(g_State, "Level");
        if (levelClass) {
            g_State.ULevel_ActorsOff = ConsoleEnabler::FindPropertyOffset(g_State, (UStruct*)levelClass, "Actors");
        }
    }

    void* level = GetLivePersistentLevelPointer();
    if (!level || !IsReadableWorldExplorerObject(level))
        return false;

    uint8_t tarrayBuf[16] = {};
    if (g_State.ULevel_ActorsOff >= 0 && SafeMem_Read(static_cast<uint8_t*>(level) + g_State.ULevel_ActorsOff, tarrayBuf, sizeof(tarrayBuf)))
    {
        void* dataPtr = *(void**)tarrayBuf;
        int32_t arrCount = *(int32_t*)(tarrayBuf + 8);
        int32_t arrMax = *(int32_t*)(tarrayBuf + 12);

        if (arrCount >= 0 && arrCount <= 500000 && arrMax >= arrCount)
        {
            if (arrCount == 0 || (dataPtr && (uintptr_t)dataPtr >= 0x10000 && (uintptr_t)dataPtr < 0x7FFFFFFFFFFFULL))
            {
                if (outCount)
                    *outCount = arrCount;
                return true;
            }
        }
    }

    static const int levelOffsets[] = { 0x98, 0xA0, 0x70, 0xA8, 0xB0, 0x88, 0xC0 };
    for (int candOff : levelOffsets)
    {
        if (SafeMem_Read(static_cast<uint8_t*>(level) + candOff, tarrayBuf, sizeof(tarrayBuf)))
        {
            void* dataPtr = *(void**)tarrayBuf;
            int32_t arrCount = *(int32_t*)(tarrayBuf + 8);
            int32_t arrMax = *(int32_t*)(tarrayBuf + 12);

            if (arrCount > 0 && arrCount <= 500000 && arrMax >= arrCount && dataPtr && (uintptr_t)dataPtr >= 0x10000 && (uintptr_t)dataPtr < 0x7FFFFFFFFFFFULL)
            {
                g_State.ULevel_ActorsOff = candOff;
                if (outCount)
                    *outCount = arrCount;
                return true;
            }
        }
    }

    return false;
}

void InvalidateWorldExplorerCeilingCache()
{
    gWorldExplorer.CachedScopeCeilingsValid = false;
}

void RebuildWorldExplorerCeilingCache()
{
    if (gWorldExplorer.CachedScopeCeilingsValid)
        return;

    for (int index = 0; index < 3; ++index)
    {
        gWorldExplorer.CachedScopeCeilings[index] = 0;
        gWorldExplorer.CachedScopeCeilingsExact[index] = false;
    }

    int persistentLevelActorCount = 0;
    if (TryGetPersistentLevelActorCountQuick(&persistentLevelActorCount) && persistentLevelActorCount > 0)
    {
        gWorldExplorer.CachedScopeCeilings[WorldExplorerSource_PersistentLevel] = persistentLevelActorCount;
        gWorldExplorer.CachedScopeCeilingsExact[WorldExplorerSource_PersistentLevel] = true;
    }
    else
    {
        int indexedActors = GetDiscoveryIndexedActorCount();
        gWorldExplorer.CachedScopeCeilings[WorldExplorerSource_PersistentLevel] = indexedActors > 0 ? indexedActors : 2048;
        gWorldExplorer.CachedScopeCeilingsExact[WorldExplorerSource_PersistentLevel] = (persistentLevelActorCount > 0);
    }

    if (g_State.bIndexBuilt && g_State.AActorClass && g_State.IndexMutex)
    {
        gWorldExplorer.CachedScopeCeilings[WorldExplorerSource_AllActors] = GetDiscoveryIndexedActorCount();
        gWorldExplorer.CachedScopeCeilingsExact[WorldExplorerSource_AllActors] = true;
    }

    if (g_State.bIndexBuilt && g_State.IndexMutex)
    {
        gWorldExplorer.CachedScopeCeilings[WorldExplorerSource_AllObjects] = GetDiscoveryIndexedObjectCount();
        gWorldExplorer.CachedScopeCeilingsExact[WorldExplorerSource_AllObjects] = true;
    }
    else if (g_State.bInitialized)
    {
        gWorldExplorer.CachedScopeCeilings[WorldExplorerSource_AllObjects] = EngineDiscovery::GetObjectCount(g_State);
        gWorldExplorer.CachedScopeCeilingsExact[WorldExplorerSource_AllObjects] = true;
    }

    gWorldExplorer.CachedScopeCeilingsValid = true;
}

bool HasExactWorldExplorerCeiling()
{
    RebuildWorldExplorerCeilingCache();
    return gWorldExplorer.CachedScopeCeilingsExact[static_cast<int>(gWorldExplorer.SourceMode)];
}

void ClearWorldExplorerResults()
{
    gWorldExplorer.Entries.clear();
    gWorldExplorer.VisibleIndices.clear();
    InvalidateInspectorProperties();
    gWorldExplorer.SelectedObject = nullptr;
    gWorldExplorer.SelectedIndex = -1;
    gWorldExplorer.LastActorCount = 0;
    gWorldExplorer.LastMatchCount = 0;
    gWorldExplorer.ResultsTruncated = false;
    gWorldExplorer.VisibleEntriesDirty = false;
    gWorldExplorer.Inspector = WorldInspectorSnapshot{};
}

void QueueWorldExplorerRefresh(const char* statusText)
{
    gWorldExplorer.RefreshPending = true;
    InvalidateWorldExplorerCeilingCache();
    ClearWorldExplorerResults();
    if (statusText && statusText[0] != '\0')
        gWorldExplorer.IndexStatus = statusText;
}

int GetWorldExplorerMeasuredCeiling()
{
    RebuildWorldExplorerCeilingCache();
    return std::max(gWorldExplorer.CachedScopeCeilings[static_cast<int>(gWorldExplorer.SourceMode)], 0);
}

int GetWorldExplorerResultCeiling()
{
    return std::max(GetWorldExplorerMeasuredCeiling(), kMinWorldExplorerResults);
}

void ApplyWorldExplorerMaxResults(int requestedValue)
{
    const int measuredCeiling = GetWorldExplorerMeasuredCeiling();
    const int userCeiling = HasExactWorldExplorerCeiling()
        ? std::max(measuredCeiling, 1)
        : std::max(std::max(gWorldExplorer.MaxResults, requestedValue), kMinWorldExplorerResults);

    gWorldExplorer.MaxResults = std::clamp(requestedValue, 1, userCeiling);
    InvalidateWorldExplorerVisibleCache();
}

std::string BuildWorldExplorerResultsSummary(int actorCount)
{
    if (gWorldExplorer.LastMatchCount <= 0)
        return FormatCountValue(actorCount) + " shown";

    if (gWorldExplorer.ResultsTruncated)
        return "Showing " + FormatCountValue(actorCount) + " of " + FormatCountValue(gWorldExplorer.LastMatchCount) + " matches";

    return FormatCountValue(gWorldExplorer.LastMatchCount) + " matches";
}

bool IsReadableWorldExplorerObject(void* object)
{
    if (!object)
        return false;

    uint8_t probe[0x30] = {};
    return SafeMem_Read(object, probe, sizeof(probe));
}

std::string ResolveObjectName(void* object)
{
    if (!object)
        return "<none>";
    if (!IsReadableWorldExplorerObject(object))
        return "<stale object>";

    char buffer[256] = {};
    if (GetObjectNameSafe(object, buffer, static_cast<int>(sizeof(buffer))) > 0)
        return SanitizeUiText(buffer, "<unnamed>");

    return "<unnamed>";
}

std::string ResolveObjectFullName(void* object)
{
    if (!object)
        return "<none>";
    if (!IsReadableWorldExplorerObject(object))
        return "<stale object>";

    char buffer[1024] = {};
    if (GetObjectFullNameSafe(object, buffer, static_cast<int>(sizeof(buffer))) > 0)
        return SanitizeUiText(buffer, "<unresolved>");

    return "<unresolved>";
}

std::string ResolveInheritanceChain(void* uclass)
{
    if (!uclass)
        return "<none>";

    const char* chain = GetInheritanceChain(uclass);
    if (chain && chain[0] != '\0')
        return SanitizeUiText(chain, "<unresolved>");

    return "<unresolved>";
}

std::string ResolveCachedClassName(void* objectClass)
{
    if (!objectClass)
        return "<unknown>";

    const uintptr_t key = reinterpret_cast<uintptr_t>(objectClass);
    auto it = gWorldExplorer.ClassNameCache.find(key);
    if (it != gWorldExplorer.ClassNameCache.end())
        return it->second;

    std::string resolved = ResolveObjectName(objectClass);
    gWorldExplorer.ClassNameCache.emplace(key, resolved);
    return resolved;
}

const char* GetWorldExplorerSourceLabel()
{
    switch (gWorldExplorer.SourceMode)
    {
    case WorldExplorerSource_PersistentLevel: return "Persistent Level";
    case WorldExplorerSource_AllActors:      return "All Actors";
    case WorldExplorerSource_AllObjects:     return "All Objects";
    default:                                 return "All Actors";
    }
}

const char* GetWorldExplorerSourceSubtitle()
{
    switch (gWorldExplorer.SourceMode)
    {
    case WorldExplorerSource_PersistentLevel: return "Actors from the current persistent level snapshot";
    case WorldExplorerSource_AllActors:      return "Actor objects discovered across the live GUObjectArray";
    case WorldExplorerSource_AllObjects:     return "All live objects discovered across the GUObjectArray";
    default:                                 return "Actor objects discovered across the live GUObjectArray";
    }
}

bool IsActorClass(void* objectClass)
{
    if (!objectClass || !g_State.AActorClass)
        return false;

    if (objectClass == g_State.AActorClass)
        return true;

    static std::unordered_map<void*, bool> s_ActorClassCache;
    auto it = s_ActorClassCache.find(objectClass);
    if (it != s_ActorClassCache.end())
        return it->second;

    bool isActor = EngineDiscovery::IsStructA(g_State, static_cast<UStruct*>(objectClass), reinterpret_cast<UStruct*>(g_State.AActorClass));
    s_ActorClassCache[objectClass] = isActor;
    return isActor;
}

void PrimeWorldExplorerEntry(WorldExplorerEntry& entry)
{
    entry.Name = SanitizeUiText(entry.Name, "<unnamed>");
    entry.ClassName = SanitizeUiText(entry.ClassName, "<unknown>");
    entry.SearchText = ToLowerCopy(entry.Name);
    entry.SearchText.push_back('\n');
    entry.SearchText += ToLowerCopy(entry.ClassName);
}

bool EntryMatchesScope(const WorldExplorerEntry& entry, WorldExplorerSourceMode sourceMode)
{
    switch (sourceMode)
    {
    case WorldExplorerSource_PersistentLevel:
        return (entry.Flags & kWorldExplorerFlag_InPersistentLevel) != 0;
    case WorldExplorerSource_AllActors:
        return (entry.Flags & kWorldExplorerFlag_IsActor) != 0;
    case WorldExplorerSource_AllObjects:
    default:
        return true;
    }
}

void SortWorldExplorerEntries(std::vector<WorldExplorerEntry>& entries)
{
    std::sort(entries.begin(), entries.end(), [](const WorldExplorerEntry& lhs, const WorldExplorerEntry& rhs)
    {
        if (lhs.Name != rhs.Name)
            return lhs.Name < rhs.Name;
        if (lhs.ClassName != rhs.ClassName)
            return lhs.ClassName < rhs.ClassName;
        return reinterpret_cast<uintptr_t>(lhs.Object) < reinterpret_cast<uintptr_t>(rhs.Object);
    });
}

void InvalidateWorldExplorerVisibleCache()
{
    gWorldExplorer.VisibleEntriesDirty = true;
}

WorldExplorerEntry BuildWorldExplorerEntry(void* object, void* objectClass, uint32_t flags)
{
    WorldExplorerEntry entry;
    entry.Object = object;
    entry.Class = objectClass ? objectClass : EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(object));
    entry.Name = ResolveObjectName(object);
    entry.ClassName = ResolveCachedClassName(entry.Class);
    entry.Flags = flags;
    PrimeWorldExplorerEntry(entry);
    return entry;
}

bool EntryMatchesFilter(const WorldExplorerEntry& entry, const char* filter)
{
    if (!filter || filter[0] == '\0')
        return true;

    return entry.SearchText.find(ToLowerCopy(filter)) != std::string::npos;
}

void RestoreWorldExplorerSelection(const void* previousSelection)
{
    gWorldExplorer.SelectedObject = nullptr;
    gWorldExplorer.SelectedIndex = -1;

    if (previousSelection)
    {
        for (int index = 0; index < static_cast<int>(gWorldExplorer.Entries.size()); ++index)
        {
            if (gWorldExplorer.Entries[static_cast<size_t>(index)].Object == previousSelection)
            {
                gWorldExplorer.SelectedObject = gWorldExplorer.Entries[static_cast<size_t>(index)].Object;
                gWorldExplorer.SelectedIndex = index;
                return;
            }
        }
    }

    if (!gWorldExplorer.Entries.empty())
    {
        gWorldExplorer.SelectedObject = gWorldExplorer.Entries.front().Object;
        gWorldExplorer.SelectedIndex = 0;
    }
}

void RebuildWorldExplorerVisibleIndices()
{
    if (!gWorldExplorer.VisibleEntriesDirty)
        return;

    gWorldExplorer.VisibleEntriesDirty = false;
    gWorldExplorer.CachedFilter = gWorldExplorer.ActorFilter;
    gWorldExplorer.VisibleIndices.clear();
    gWorldExplorer.ResultsTruncated = false;
    gWorldExplorer.LastMatchCount = 0;

    const std::string loweredFilter = ToLowerCopy(gWorldExplorer.CachedFilter);
    const int maxResults = std::max(gWorldExplorer.MaxResults, 1);
    bool selectedIsVisible = false;

    for (int index = 0; index < static_cast<int>(gWorldExplorer.Entries.size()); ++index)
    {
        const WorldExplorerEntry& entry = gWorldExplorer.Entries[static_cast<size_t>(index)];
        if (!loweredFilter.empty() && entry.SearchText.find(loweredFilter) == std::string::npos)
            continue;

        ++gWorldExplorer.LastMatchCount;
        if (static_cast<int>(gWorldExplorer.VisibleIndices.size()) < maxResults)
        {
            gWorldExplorer.VisibleIndices.push_back(index);
            if (gWorldExplorer.SelectedObject == entry.Object)
                selectedIsVisible = true;
        }
        else
        {
            gWorldExplorer.ResultsTruncated = true;
        }
    }

    gWorldExplorer.LastActorCount = static_cast<int>(gWorldExplorer.VisibleIndices.size());

    if (!selectedIsVisible)
    {
        if (!gWorldExplorer.VisibleIndices.empty())
        {
            gWorldExplorer.SelectedIndex = gWorldExplorer.VisibleIndices.front();
            gWorldExplorer.SelectedObject = gWorldExplorer.Entries[static_cast<size_t>(gWorldExplorer.SelectedIndex)].Object;
        }
        else
        {
            gWorldExplorer.SelectedIndex = -1;
            gWorldExplorer.SelectedObject = nullptr;
        }
    }
}

void RefreshWorldExplorer()
{
    const auto refreshStart = std::chrono::steady_clock::now();
    gWorldExplorer.HasLoadedOnce = true;
    gWorldExplorer.LastObservedWorld = GetLiveWorldPointer();
    gWorldExplorer.LastObservedLevel = GetLivePersistentLevelPointer();

    const void* previousSelection = gWorldExplorer.SelectedObject;
    gWorldExplorer.Entries.clear();
    gWorldExplorer.VisibleIndices.clear();
    InvalidateInspectorProperties();
    gWorldExplorer.SelectedObject = nullptr;
    gWorldExplorer.SelectedIndex = -1;
    gWorldExplorer.LastActorCount = 0;
    gWorldExplorer.LastMatchCount = 0;
    gWorldExplorer.ResultsTruncated = false;
    InvalidateWorldExplorerVisibleCache();

    if (!g_State.bInitialized)
    {
        InvalidateInspectorProperties();
        gWorldExplorer.RefreshPending = true;
        gWorldExplorer.IndexStatus = "Discovery is still initializing. Refresh again once the engine scan is ready.";
        return;
    }

    InvalidateWorldExplorerCeilingCache();

    const bool wasShowingAll = (gWorldExplorer.MaxResults >= gWorldExplorer.CachedScopeCeilings[static_cast<int>(gWorldExplorer.SourceMode)] || gWorldExplorer.MaxResults <= 0);

    const int currentObjCount = EngineDiscovery::GetObjectCount(g_State);
    if (!g_State.bIndexBuilt || currentObjCount > static_cast<int>(g_State.SortedNames.size()) + 200)
    {
        EngineDiscovery::BuildObjectIndex(g_State);
    }

    gWorldExplorer.RefreshPending = false;
    gWorldExplorer.IndexStatus = "Live runtime snapshot loaded.";

    const bool hasFilter = gWorldExplorer.ActorFilter[0] != '\0';
    const int datasetCeiling = std::max(std::max(GetDiscoveryIndexedObjectCount(), EngineDiscovery::GetObjectCount(g_State)), kMinWorldExplorerResults);
    const int maxResults = (wasShowingAll || gWorldExplorer.MaxResults <= 0) ? datasetCeiling : gWorldExplorer.MaxResults;

    SafeMem_SetFastMode(1);

    if (gWorldExplorer.SourceMode == WorldExplorerSource_PersistentLevel)
    {
        const int dynamicCapacity = std::max(datasetCeiling, 131072);
        std::vector<void*> actors(static_cast<size_t>(dynamicCapacity));
        const int actorCount = EngineDiscovery::GetWorldActors(g_State, actors.data(), static_cast<int>(actors.size()));

        for (int actorIndex = 0; actorIndex < actorCount; ++actorIndex)
        {
            void* actor = actors[static_cast<size_t>(actorIndex)];
            if (!actor)
                continue;

            WorldExplorerEntry entry = BuildWorldExplorerEntry(actor, nullptr, kWorldExplorerFlag_IsActor | kWorldExplorerFlag_InPersistentLevel);
            if (!EntryMatchesFilter(entry, gWorldExplorer.ActorFilter))
                continue;

            if (static_cast<int>(gWorldExplorer.Entries.size()) < maxResults)
                gWorldExplorer.Entries.push_back(std::move(entry));
            else
            {
                gWorldExplorer.ResultsTruncated = true;
                break;
            }
        }
    }
    else if (gWorldExplorer.SourceMode == WorldExplorerSource_AllActors)
    {
        if (g_State.bIndexBuilt && g_State.IndexMutex)
        {
            std::lock_guard<std::recursive_mutex> lock(*g_State.IndexMutex);
            for (const auto& pair : g_State.ClassToInstances)
            {
                if (!pair.first) continue;
                if (IsActorClass(pair.first))
                {
                    for (UObject* obj : pair.second)
                    {
                        if (!obj) continue;
                        WorldExplorerEntry entry = BuildWorldExplorerEntry(obj, pair.first, kWorldExplorerFlag_IsActor);
                        if (hasFilter && !EntryMatchesFilter(entry, gWorldExplorer.ActorFilter))
                            continue;

                        if (static_cast<int>(gWorldExplorer.Entries.size()) < maxResults)
                            gWorldExplorer.Entries.push_back(std::move(entry));
                        else
                        {
                            gWorldExplorer.ResultsTruncated = true;
                            break;
                        }
                    }
                    if (static_cast<int>(gWorldExplorer.Entries.size()) >= maxResults)
                        break;
                }
            }
        }
        else
        {
            const int scanLimit = std::clamp(std::max(maxResults, 4096), kMinWorldExplorerResults, datasetCeiling);

            std::vector<void*> actors(static_cast<size_t>(scanLimit));
            const int actorCount = GetAllActors(actors.data(), static_cast<int>(actors.size()));

            const int visibleCount = std::min(actorCount, maxResults);
            gWorldExplorer.Entries.reserve(static_cast<size_t>(visibleCount));
            for (int actorIndex = 0; actorIndex < visibleCount; ++actorIndex)
            {
                void* actor = actors[static_cast<size_t>(actorIndex)];
                if (!actor)
                    continue;
                WorldExplorerEntry entry = BuildWorldExplorerEntry(actor, nullptr, kWorldExplorerFlag_IsActor);
                if (hasFilter && !EntryMatchesFilter(entry, gWorldExplorer.ActorFilter))
                    continue;
                gWorldExplorer.Entries.push_back(std::move(entry));
            }

            if (actorCount > visibleCount || actorCount >= scanLimit)
                gWorldExplorer.ResultsTruncated = true;
        }
    }
    else
    {
        if (g_State.bIndexBuilt && g_State.IndexMutex)
        {
            std::lock_guard<std::recursive_mutex> lock(*g_State.IndexMutex);
            for (const auto& pair : g_State.ClassToInstances)
            {
                if (!pair.first) continue;
                const bool isActor = IsActorClass(pair.first);
                const uint32_t flags = isActor ? kWorldExplorerFlag_IsActor : 0u;

                for (UObject* obj : pair.second)
                {
                    if (!obj) continue;
                    WorldExplorerEntry entry = BuildWorldExplorerEntry(obj, pair.first, flags);
                    if (hasFilter && !EntryMatchesFilter(entry, gWorldExplorer.ActorFilter))
                        continue;

                    if (static_cast<int>(gWorldExplorer.Entries.size()) < maxResults)
                        gWorldExplorer.Entries.push_back(std::move(entry));
                    else
                    {
                        gWorldExplorer.ResultsTruncated = true;
                        break;
                    }
                }
                if (static_cast<int>(gWorldExplorer.Entries.size()) >= maxResults)
                    break;
            }
        }
        else
        {
            const int objectCount = EngineDiscovery::GetObjectCount(g_State);
            for (int objectIndex = 0; objectIndex < objectCount; ++objectIndex)
            {
                UObject* object = EngineDiscovery::GetObjectByIndex(g_State, objectIndex);
                if (!IsReadableWorldExplorerObject(object))
                    continue;

                void* objectClass = EngineDiscovery::GetObjectClass(g_State, object);
                const bool isActor = IsActorClass(objectClass);
                const uint32_t flags = isActor ? kWorldExplorerFlag_IsActor : 0u;

                if (!hasFilter)
                {
                    if (static_cast<int>(gWorldExplorer.Entries.size()) >= maxResults)
                    {
                        gWorldExplorer.ResultsTruncated = true;
                        break;
                    }

                    gWorldExplorer.Entries.push_back(BuildWorldExplorerEntry(object, objectClass, flags));
                    continue;
                }

                WorldExplorerEntry entry = BuildWorldExplorerEntry(object, objectClass, flags);
                if (!EntryMatchesFilter(entry, gWorldExplorer.ActorFilter))
                    continue;

                if (static_cast<int>(gWorldExplorer.Entries.size()) < maxResults)
                    gWorldExplorer.Entries.push_back(std::move(entry));
                else
                {
                    gWorldExplorer.ResultsTruncated = true;
                    break;
                }
            }
        }
    }

    SafeMem_SetFastMode(0);
    SortWorldExplorerEntries(gWorldExplorer.Entries);
    RestoreWorldExplorerSelection(previousSelection);

    const int totalHarvested = static_cast<int>(gWorldExplorer.Entries.size());
    gWorldExplorer.CachedScopeCeilings[static_cast<int>(gWorldExplorer.SourceMode)] = totalHarvested;
    gWorldExplorer.CachedScopeCeilingsExact[static_cast<int>(gWorldExplorer.SourceMode)] = true;
    gWorldExplorer.CachedScopeCeilingsValid = true;

    if (wasShowingAll)
    {
        gWorldExplorer.MaxResults = std::max(totalHarvested, 1);
    }
    else
    {
        gWorldExplorer.MaxResults = std::clamp(gWorldExplorer.MaxResults, 1, std::max(totalHarvested, 1));
    }

    RebuildWorldExplorerVisibleIndices();
    RefreshInspectorSnapshot();
    InvalidateInspectorProperties();

    const auto refreshEnd = std::chrono::steady_clock::now();
    gWorldExplorer.LastRefreshMs = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(refreshEnd - refreshStart).count());
}

void MaybeAutoRefreshWorldExplorer()
{
    if (!gWorldExplorer.HasLoadedOnce || !gWorldExplorer.AutoRefresh)
        return;

    void* currentWorld = GetLiveWorldPointer();
    void* currentLevel = GetLivePersistentLevelPointer();
    if (currentWorld != gWorldExplorer.LastObservedWorld || currentLevel != gWorldExplorer.LastObservedLevel)
    {
        QueueWorldExplorerRefresh("World changed. Refresh is deferred to avoid freezing large games; click Refresh when ready.");
        gWorldExplorer.LastObservedWorld = currentWorld;
        gWorldExplorer.LastObservedLevel = currentLevel;
    }
}

void DrawWorldExplorerSummarySection(int actorCount)
{
    const std::string resultsText = BuildWorldExplorerResultsSummary(actorCount);
    const std::string selectedName = gWorldExplorer.SelectedObject ? gWorldExplorer.Inspector.Name : std::string("<none>");

    ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
    if (ImGui::CollapsingHeader("Runtime summary"))
    {
        if (ImGui::BeginTable("WorldExplorerMetrics", 5, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            DrawMetricBlock("Live World", FormatPointer(GetLiveWorldPointer()));

            ImGui::TableSetColumnIndex(1);
            DrawMetricBlock("Persistent Level", FormatPointer(GetLivePersistentLevelPointer()));

            ImGui::TableSetColumnIndex(2);
            DrawMetricBlock("Scope", GetWorldExplorerSourceLabel());

            ImGui::TableSetColumnIndex(3);
            DrawMetricBlock("Results", resultsText);

            ImGui::TableSetColumnIndex(4);
            DrawMetricBlock("Last Refresh", std::to_string(static_cast<int>(gWorldExplorer.LastRefreshMs)) + " ms");

            ImGui::EndTable();
        }

        DrawMutedText((std::string("Selection: ") + selectedName).c_str());
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
    }
}

void DrawWorldExplorerControls(int actorCount)
{
    const char* sourceLabels[] = { "Persistent Level", "All Actors", "All Objects" };
    int sourceMode = static_cast<int>(gWorldExplorer.SourceMode);
    bool selectionMayHaveChanged = false;

    ImGui::SetNextItemWidth(320.0f);
    if (ImGui::InputTextWithHint("##WorldActorFilter", "Find actor or class", gWorldExplorer.ActorFilter, IM_ARRAYSIZE(gWorldExplorer.ActorFilter)))
    {
        InvalidateWorldExplorerVisibleCache();
        selectionMayHaveChanged = true;
    }

    const int dynamicCeiling = GetWorldExplorerMeasuredCeiling();
    const bool hasExactCeiling = HasExactWorldExplorerCeiling();
    const bool showingAllAvailable = hasExactCeiling && gWorldExplorer.MaxResults >= std::max(dynamicCeiling, 1);

    ImGui::SameLine();
    const char* refreshLabel = !gWorldExplorer.HasLoadedOnce
        ? "Refresh list"
        : (gWorldExplorer.RefreshPending ? "Apply refresh" : "Refresh list");
    if (ImGui::Button(refreshLabel))
        RefreshWorldExplorer();

    DrawMutedText(!gWorldExplorer.HasLoadedOnce
        ? "Click Refresh list to load actors and objects."
        : (gWorldExplorer.RefreshPending
            ? "Pending changes stay queued until you press Refresh list."
            : "Browsing stays light; the Inspector now auto-loads reflected properties for the current selection."));

    ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
    if (ImGui::CollapsingHeader("Advanced"))
    {
        if (ImGui::Checkbox("Auto queue refresh on world change", &gWorldExplorer.AutoRefresh) && gWorldExplorer.AutoRefresh)
            gWorldExplorer.IndexStatus = "World changes queue a refresh instead of blocking the current frame.";

        const std::string maxPreview = hasExactCeiling
            ? (showingAllAvailable
                ? "All available (" + FormatCountValue(dynamicCeiling) + ")"
                : FormatCountValue(gWorldExplorer.MaxResults) + " shown")
            : "Refresh to measure";
        int customMaxResults = gWorldExplorer.MaxResults;

        if (ImGui::BeginTable("ExplorerOptionSelectors", 3, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("ScopeColumn", ImGuiTableColumnFlags_WidthStretch, 1.15f);
            ImGui::TableSetupColumn("MaxPresetColumn", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("MaxCustomColumn", ImGuiTableColumnFlags_WidthStretch, 0.8f);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            DrawMutedText("Scope");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##WorldExplorerScope", sourceLabels[sourceMode]))
            {
                for (int index = 0; index < IM_ARRAYSIZE(sourceLabels); ++index)
                {
                    ImGui::PushID(index);

                    const bool isSelected = (sourceMode == index);
                    if (ImGui::Selectable(sourceLabels[index], isSelected))
                    {
                        gWorldExplorer.SourceMode = static_cast<WorldExplorerSourceMode>(index);
                        QueueWorldExplorerRefresh("Scope changed. Click Refresh list to scan the new live view.");
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }

            ImGui::TableSetColumnIndex(1);
            DrawMutedText("Max results");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##WorldExplorerMaxPreset", maxPreview.c_str()))
            {
                std::vector<int> maxPresets;
                const int fallbackCeiling = std::max(EngineDiscovery::GetObjectCount(g_State), 131072);
                const int presetCeiling = hasExactCeiling ? dynamicCeiling : std::max(gWorldExplorer.MaxResults, fallbackCeiling);
                for (int preset = kMinWorldExplorerResults; preset < presetCeiling && preset > 0; preset *= 2)
                {
                    maxPresets.push_back(preset);
                    if (preset > presetCeiling / 2)
                        break;
                }
                if (maxPresets.empty() || maxPresets.back() != presetCeiling)
                    maxPresets.push_back(presetCeiling);
                if (hasExactCeiling && maxPresets.back() != dynamicCeiling)
                    maxPresets.push_back(dynamicCeiling);

                for (int preset : maxPresets)
                {
                    ImGui::PushID(preset);
                    const bool isAllAvailablePreset = hasExactCeiling && preset == dynamicCeiling;
                    const bool isSelected = (gWorldExplorer.MaxResults == preset) || (isAllAvailablePreset && showingAllAvailable);
                    const std::string presetLabel = isAllAvailablePreset
                        ? "All available (" + FormatCountValue(dynamicCeiling) + ")"
                        : FormatCountValue(preset);
                    if (ImGui::Selectable(presetLabel.c_str(), isSelected))
                    {
                        ApplyWorldExplorerMaxResults(preset);
                        selectionMayHaveChanged = true;
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }

            ImGui::TableSetColumnIndex(2);
            DrawMutedText("Custom cap");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##WorldExplorerCustomMax", &customMaxResults, 128, 512))
            {
                ApplyWorldExplorerMaxResults(customMaxResults);
                selectionMayHaveChanged = true;
            }

            ImGui::EndTable();
        }

        DrawMutedText(hasExactCeiling
            ? (std::string("Current scope can show up to ") + FormatCountValue(dynamicCeiling) + " entries.").c_str()
            : "Exact totals are measured on refresh so the UI stays responsive.");

        DrawMutedText(gWorldExplorer.IndexStatus.c_str());
        if (gWorldExplorer.ResultsTruncated)
            DrawMutedText("Showing the first matches only. Raise Max or tighten the filter to go deeper.");
        DrawWorldExplorerSummarySection(actorCount);
    }

    if (selectionMayHaveChanged)
    {
        RebuildWorldExplorerVisibleIndices();
        RefreshInspectorSnapshot();
        InvalidateInspectorProperties();
    }

    ImGui::Dummy(ImVec2(0.0f, 12.0f));
}

void DrawWorldExplorerListPanel()
{
    RebuildWorldExplorerVisibleIndices();

    if (BeginSectionPanel("WorldExplorerListPanel"))
    {
        DrawPanelHeading("World Explorer", GetWorldExplorerSourceSubtitle());

        if (gWorldExplorer.VisibleIndices.empty())
        {
            if (!g_State.bInitialized)
                DrawMutedText("Discovery is still initializing.");
            else if (gWorldExplorer.ActorFilter[0] != '\0')
                DrawMutedText("No matches in the current in-memory snapshot.");
            else
                DrawMutedText("No entries were returned for the selected scope.");
        }
        else
        {
            const ImGuiTableFlags tableFlags =
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_BordersInnerH |
                ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_SizingStretchProp |
                ImGuiTableFlags_PadOuterX;

            if (ImGui::BeginTable("WorldActorsTable", 3, tableFlags, ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 56.0f);
                ImGui::TableSetupColumn("Actor", ImGuiTableColumnFlags_WidthStretch, 1.5f);
                ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthStretch, 1.1f);
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(gWorldExplorer.VisibleIndices.size()));
                while (clipper.Step())
                {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                    {
                        const int actualIndex = gWorldExplorer.VisibleIndices[static_cast<size_t>(row)];
                        const WorldExplorerEntry& entry = gWorldExplorer.Entries[static_cast<size_t>(actualIndex)];
                        const bool isSelected = (gWorldExplorer.SelectedObject == entry.Object);

                        ImGui::TableNextRow();
                        ImGui::PushID(actualIndex);

                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%d", actualIndex);

                        ImGui::TableSetColumnIndex(1);
                        std::string rowLabel = entry.Name + "##act_row_" + std::to_string(actualIndex);
                        if (ImGui::Selectable(rowLabel.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
                        {
                            gWorldExplorer.SelectedObject = entry.Object;
                            gWorldExplorer.SelectedIndex = actualIndex;
                            RefreshInspectorSnapshot();
                            InvalidateInspectorProperties();
                        }

                        ImGui::TableSetColumnIndex(2);
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.79f, 0.92f, 1.00f));
                        ImGui::TextUnformatted(entry.ClassName.c_str());
                        ImGui::PopStyleColor();
                        ImGui::PopID();
                    }
                }

                ImGui::EndTable();
            }
        }
    }
    EndSectionPanel();
}
