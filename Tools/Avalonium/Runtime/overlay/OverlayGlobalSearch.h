#pragma once

#include "OverlayInternal.h"
#include "OverlayLiveEditor.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

enum class GlobalSearchScope {
    PresentLevel = 0,
    AllActors = 1,
    AllObjects = 2,
    AllObjectsAndCDOs = 3,
    SelectedHierarchy = 4
};

enum class GlobalSearchResultKind {
    Property = 0,
    Function = 1,
    Object = 2
};

enum class GlobalSearchTypeCategory {
    All = 0,
    Numbers = 1,
    Booleans = 2,
    Objects = 3,
    Strings = 4,
    Structs = 5,
    Functions = 6
};

struct GlobalSearchResult {
    GlobalSearchResultKind Kind = GlobalSearchResultKind::Property;
    void* ObjectPtr = nullptr;
    void* ClassPtr = nullptr;
    std::string ObjectName;
    std::string ClassName;
    std::string FullPath;

    LivePropertyInfo PropInfo;
    std::string LiveValueStr;

    LiveFunctionInfo FuncInfo;

    std::string MatchLabel;
    int32_t Offset = 0;
};

struct GlobalSearchState {
    char Query[128] = {};
    char LastExecutedQuery[128] = {};
    GlobalSearchScope Scope = GlobalSearchScope::PresentLevel;

    bool MatchProperties = true;
    bool MatchFunctions = true;
    bool MatchObjects = true;
    bool MatchValues = false;
    bool UseRegex = false;
    bool SearchByAddress = false;
    bool IncludeClassHierarchy = false;

    GlobalSearchTypeCategory TypeCategory = GlobalSearchTypeCategory::All;

    std::vector<GlobalSearchResult> Results;
    bool EnableLimit = false;
    int CustomLimitValue = 500;
    int TotalMatchesFound = 0;
    bool ResultsTruncated = false;

    float LastSearchTimeMs = 0.0f;
    bool IsSearching = false;

    int32_t EditingRowIndex = -1;
    char TempEditBuffer[256] = {};

    std::string StatusMessage = "Enter a keyword and click Search.";
};

extern GlobalSearchState gGlobalSearch;

namespace GlobalSearch {

void Initialize();
void ExecuteSearch();
void ClearResults();
void RefreshLiveValues();

void DrawGlobalSearchTab();

}
