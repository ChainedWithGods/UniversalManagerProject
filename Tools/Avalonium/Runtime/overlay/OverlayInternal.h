#pragma once

#include "../../Helpers/Universal-Dear-ImGui-Hook/stdafx.h"
#include "../api/UEInternalAPI.h"
#include "../core/EngineDiscovery.h"
#include "../core/FunctionFinder.h"
#include "../core/ObjectIndex.h"
#include "../console/ConsoleInternal.h"
#include "../utils/SafeMemory.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern EngineState g_State;

extern ImFont* gUiFont;
extern ImFont* gUiFontBold;
extern bool gFontsConfigured;
extern ImGuiContext* gFontContext;

constexpr uint32_t kWorldExplorerFlag_IsActor = 1u << 0;
constexpr uint32_t kWorldExplorerFlag_InPersistentLevel = 1u << 1;

struct WorldExplorerEntry {
    void* Object = nullptr;
    void* Class = nullptr;
    std::string Name;
    std::string ClassName;
    std::string SearchText;
    uint32_t Flags = 0;
};

enum WorldExplorerSourceMode {
    WorldExplorerSource_PersistentLevel = 0,
    WorldExplorerSource_AllActors = 1,
    WorldExplorerSource_AllObjects = 2,
};

enum RuntimeMainView {
    RuntimeMainView_Globals = 0,
    RuntimeMainView_WorldExplorer = 1,
    RuntimeMainView_LiveEditor = 2,
    RuntimeMainView_GlobalSearch = 3,
};

struct WorldInspectorSnapshot {
    void* SelectedClass = nullptr;
    void* SelectedOuter = nullptr;
    std::string Name = "<none>";
    std::string Pointer = "<not found>";
    std::string FullName = "<unresolved>";
    std::string ClassName = "<unknown>";
    std::string ClassPointer = "<not found>";
    std::string OuterName = "<none>";
    std::string OuterPointer = "<not found>";
    std::string Inheritance = "<unresolved>";
};

struct WorldExplorerState {
    std::vector<WorldExplorerEntry> Entries;
    std::vector<int> VisibleIndices;
    std::vector<PropertyInfoEx> Properties;
    bool PropertiesLoaded = false;
    bool PropertiesLoadPending = false;
    std::unordered_map<uintptr_t, std::string> ClassNameCache;

    WorldInspectorSnapshot Inspector;

    char ActorFilter[128] = {};
    char PropertyFilter[128] = {};
    void* SelectedObject = nullptr;
    int SelectedIndex = -1;
    int MaxResults = 1024;
    int LastActorCount = 0;
    int LastMatchCount = 0;
    float LastRefreshMs = 0.0f;
    bool AutoRefresh = false;
    bool HasLoadedOnce = false;
    bool RefreshPending = true;
    bool ResultsTruncated = false;
    bool VisibleEntriesDirty = true;
    bool CachedScopeCeilingsValid = false;
    int CachedScopeCeilings[3] = {};
    bool CachedScopeCeilingsExact[3] = {};

    WorldExplorerSourceMode SourceMode = WorldExplorerSource_AllActors;

    void* LastObservedWorld = nullptr;
    void* LastObservedLevel = nullptr;
    std::string CachedFilter;
    std::string IndexStatus = "Explorer idle. Click Refresh to load data.";
};

extern WorldExplorerState gWorldExplorer;
extern RuntimeMainView gActiveMainView;
constexpr int kMinWorldExplorerResults = 128;
constexpr int kMaxInspectorProperties = 1024;

void ConfigureUiFontsForCurrentContext();
void EnsureUiFonts();

std::string FormatPointer(void* value);
std::string FormatOffset(int32_t value);
std::string FormatCountValue(int value);
std::string SanitizeUiText(const std::string& value, const char* fallback = "<unnamed>");
bool ContainsCaseInsensitive(const std::string& haystack, const char* needle);
std::string ToLowerCopy(const std::string& value);

std::string GetDetectedVersionLabel();
std::string GetRendererBackendLabel();
std::string GetProcessEventLabel();
void* GetLiveWorldPointer();
void* GetLivePersistentLevelPointer();

bool BeginSectionPanel(const char* id, const ImVec2& size = ImVec2(0.0f, 0.0f));
void EndSectionPanel();
void DrawPanelHeading(const char* title, const char* subtitle = nullptr);
void DrawMetricBlock(const char* label, const std::string& value);
void DrawInspectorInfoRow(const char* label, const std::string& value);
void DrawMutedText(const char* text);

int GetDiscoveryIndexedObjectCount();
int GetDiscoveryIndexedActorCount();
bool TryGetPersistentLevelActorCountQuick(int* outCount);
void InvalidateWorldExplorerCeilingCache();
void RebuildWorldExplorerCeilingCache();
bool HasExactWorldExplorerCeiling();
void ClearWorldExplorerResults();
void QueueWorldExplorerRefresh(const char* statusText);
int GetWorldExplorerMeasuredCeiling();
int GetWorldExplorerResultCeiling();
void ApplyWorldExplorerMaxResults(int requestedValue);
std::string BuildWorldExplorerResultsSummary(int actorCount);
bool IsReadableWorldExplorerObject(void* object);
std::string ResolveObjectName(void* object);
std::string ResolveObjectFullName(void* object);
std::string ResolveInheritanceChain(void* uclass);
std::string ResolveCachedClassName(void* objectClass);
const char* GetWorldExplorerSourceLabel();
const char* GetWorldExplorerSourceSubtitle();
bool IsActorClass(void* objectClass);
void PrimeWorldExplorerEntry(WorldExplorerEntry& entry);
bool EntryMatchesScope(const WorldExplorerEntry& entry, WorldExplorerSourceMode sourceMode);
void SortWorldExplorerEntries(std::vector<WorldExplorerEntry>& entries);
void InvalidateWorldExplorerVisibleCache();
WorldExplorerEntry BuildWorldExplorerEntry(void* object, void* objectClass, uint32_t flags);
bool EntryMatchesFilter(const WorldExplorerEntry& entry, const char* filter);
void RebuildWorldExplorerVisibleIndices();
void RefreshWorldExplorer();
void MaybeAutoRefreshWorldExplorer();
void RestoreWorldExplorerSelection(const void* previousSelection);

void DrawWorldExplorerSummarySection(int actorCount);
void DrawWorldExplorerControls(int actorCount);
void DrawWorldExplorerListPanel();
void DrawWorldExplorerInspectorTab();

void RefreshInspectorSnapshot();
void InvalidateInspectorProperties();
void RefreshInspectorProperties();
void MaybeAutoLoadInspectorProperties();
void DrawInspectorPropertiesTable();
void DrawInspectorOverviewTab(const WorldInspectorSnapshot& inspector);
void DrawWorldInspectorPanel();

bool DrawMainViewTabButton(const char* label, RuntimeMainView view, const ImVec2& size = ImVec2(0.0f, 0.0f));
void DrawCompactTopBar();
void DrawGlobalsTab();

namespace GlobalSearch {
void Initialize();
void DrawGlobalSearchTab();
}
