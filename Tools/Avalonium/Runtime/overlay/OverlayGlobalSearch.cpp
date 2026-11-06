#include "OverlayGlobalSearch.h"
#include "OverlayLiveEditor.h"
#include "OverlayInternal.h"
#include "../core/EngineDiscovery.h"
#include "../core/FunctionFinder.h"
#include "../core/ObjectIndex.h"
#include "../core/WorldDiscovery.h"
#include "../utils/SafeMemory.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

GlobalSearchState gGlobalSearch;

namespace {

static std::unordered_map<void*, std::vector<LivePropertyInfo>> s_ClassPropertiesCache;
static std::unordered_map<void*, std::vector<LiveFunctionInfo>> s_ClassFunctionsCache;

std::vector<LivePropertyInfo>& GetCachedClassProperties(void* uclass)
{
    auto it = s_ClassPropertiesCache.find(uclass);
    if (it != s_ClassPropertiesCache.end())
        return it->second;

    auto props = LiveEditor::EnumerateClassProperties(uclass);
    auto inserted = s_ClassPropertiesCache.emplace(uclass, std::move(props));
    return inserted.first->second;
}

std::vector<LiveFunctionInfo>& GetCachedClassFunctions(void* uclass)
{
    auto it = s_ClassFunctionsCache.find(uclass);
    if (it != s_ClassFunctionsCache.end())
        return it->second;

    auto funcs = LiveEditor::EnumerateClassFunctions(uclass);
    auto inserted = s_ClassFunctionsCache.emplace(uclass, std::move(funcs));
    return inserted.first->second;
}

bool MatchesTypeCategory(LivePropType type, GlobalSearchTypeCategory category)
{
    if (category == GlobalSearchTypeCategory::All) return true;

    switch (category)
    {
    case GlobalSearchTypeCategory::Numbers:
        return type == LivePropType::Int8 || type == LivePropType::Int16 || type == LivePropType::Int32 ||
               type == LivePropType::Int64 || type == LivePropType::UInt16 || type == LivePropType::UInt32 ||
               type == LivePropType::UInt64 || type == LivePropType::Byte || type == LivePropType::Float ||
               type == LivePropType::Double;

    case GlobalSearchTypeCategory::Booleans:
        return type == LivePropType::Bool;

    case GlobalSearchTypeCategory::Objects:
        return type == LivePropType::Object || type == LivePropType::Class;

    case GlobalSearchTypeCategory::Strings:
        return type == LivePropType::Str || type == LivePropType::Name || type == LivePropType::Text;

    case GlobalSearchTypeCategory::Structs:
        return type == LivePropType::Struct || type == LivePropType::Array || type == LivePropType::Map ||
               type == LivePropType::Set || type == LivePropType::Enum;

    default:
        return true;
    }
}

std::vector<void*> CollectScopeCandidates(GlobalSearchScope scope)
{
    std::vector<void*> candidates;
    if (!g_State.bInitialized) return candidates;

    switch (scope)
    {
    case GlobalSearchScope::SelectedHierarchy:
    {
        void* target = gLiveEditor.CurrentObject ? gLiveEditor.CurrentObject : gWorldExplorer.SelectedObject;
        if (target && IsReadableWorldExplorerObject(target))
        {
            candidates.push_back(target);

            for (const auto& comp : gLiveEditor.Components)
            {
                if (comp.ComponentPtr && IsReadableWorldExplorerObject(comp.ComponentPtr))
                    candidates.push_back(comp.ComponentPtr);
            }

            void* outer = EngineDiscovery::GetObjectOuter(g_State, static_cast<UObject*>(target));
            if (outer && IsReadableWorldExplorerObject(outer))
                candidates.push_back(outer);
        }
        break;
    }

    case GlobalSearchScope::PresentLevel:
    {

        if (!gWorldExplorer.Entries.empty())
        {
            for (const auto& entry : gWorldExplorer.Entries)
            {
                if ((entry.Flags & kWorldExplorerFlag_InPersistentLevel) && entry.Object && IsReadableWorldExplorerObject(entry.Object))
                {
                    candidates.push_back(entry.Object);
                }
            }
        }

        if (candidates.empty())
        {

            void* actorBuf[2048] = {};
            int32_t count = EngineDiscovery::GetWorldActors(g_State, actorBuf, 2048);
            for (int32_t idx = 0; idx < count; ++idx)
            {
                void* act = actorBuf[idx];
                if (act && IsReadableWorldExplorerObject(act))
                    candidates.push_back(act);
            }
        }
        break;
    }

    case GlobalSearchScope::AllActors:
    {
        if (!gWorldExplorer.Entries.empty())
        {
            for (const auto& entry : gWorldExplorer.Entries)
            {
                if ((entry.Flags & kWorldExplorerFlag_IsActor) && entry.Object && IsReadableWorldExplorerObject(entry.Object))
                {
                    candidates.push_back(entry.Object);
                }
            }
        }

        if (candidates.empty() && g_State.AActorClass && !g_State.ClassToInstances.empty())
        {
            auto it = g_State.ClassToInstances.find(g_State.AActorClass);
            if (it != g_State.ClassToInstances.end())
            {
                for (UObject* obj : it->second)
                {
                    if (obj && IsReadableWorldExplorerObject(obj))
                        candidates.push_back(obj);
                }
            }
        }
        break;
    }

    case GlobalSearchScope::AllObjects:
    {

        int count = EngineDiscovery::GetObjectCount(g_State);
        candidates.reserve(static_cast<size_t>(std::min(count, 100000)));

        for (int i = 0; i < count; ++i)
        {
            UObject* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
            if (!obj || !IsReadableWorldExplorerObject(obj)) continue;

            uint32_t flags = 0;
            if (g_State.UObject_FlagsOff >= 0)
            {
                SafeMem_Read(reinterpret_cast<uint8_t*>(obj) + g_State.UObject_FlagsOff, &flags, 4);
                if (flags & 0x00000010) continue;
            }

            candidates.push_back(obj);
        }
        break;
    }

    case GlobalSearchScope::AllObjectsAndCDOs:
    {
        int count = EngineDiscovery::GetObjectCount(g_State);
        candidates.reserve(static_cast<size_t>(std::min(count, 100000)));

        for (int i = 0; i < count; ++i)
        {
            UObject* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
            if (obj && IsReadableWorldExplorerObject(obj))
                candidates.push_back(obj);
        }
        break;
    }
    }

    return candidates;
}

}

namespace GlobalSearch {

void Initialize()
{
    gGlobalSearch.Results.clear();
    gGlobalSearch.TotalMatchesFound = 0;
    gGlobalSearch.ResultsTruncated = false;
    gGlobalSearch.IsSearching = false;
    gGlobalSearch.EditingRowIndex = -1;
    gGlobalSearch.Scope = GlobalSearchScope::PresentLevel;
    gGlobalSearch.MatchProperties = true;
    gGlobalSearch.MatchFunctions = true;
    gGlobalSearch.MatchObjects = true;
    gGlobalSearch.MatchValues = false;
    gGlobalSearch.TypeCategory = GlobalSearchTypeCategory::All;
    gGlobalSearch.StatusMessage = "Enter a keyword and click Search.";
}

void ClearResults()
{
    gGlobalSearch.Results.clear();
    gGlobalSearch.TotalMatchesFound = 0;
    gGlobalSearch.ResultsTruncated = false;
    gGlobalSearch.EditingRowIndex = -1;
    gGlobalSearch.StatusMessage = "Results cleared.";
}

void RefreshLiveValues()
{

}

void ExecuteSearch()
{
    std::string query = gGlobalSearch.Query;
    while (!query.empty() && (query.front() == ' ' || query.front() == '\t')) query.erase(query.begin());
    while (!query.empty() && (query.back() == ' ' || query.back() == '\t')) query.pop_back();

    if (query.empty())
    {
        ClearResults();
        gGlobalSearch.StatusMessage = "Please enter a search query.";
        return;
    }

    snprintf(gGlobalSearch.LastExecutedQuery, sizeof(gGlobalSearch.LastExecutedQuery), "%s", query.c_str());
    const std::string loweredQuery = ToLowerCopy(query);

    auto startTime = std::chrono::high_resolution_clock::now();
    gGlobalSearch.IsSearching = true;
    gGlobalSearch.Results.clear();
    gGlobalSearch.TotalMatchesFound = 0;
    gGlobalSearch.ResultsTruncated = false;
    gGlobalSearch.EditingRowIndex = -1;

    SafeMem_SetFastMode(1);

    std::vector<void*> candidates = CollectScopeCandidates(gGlobalSearch.Scope);

    std::unordered_map<void*, std::vector<void*>> classToInstances;
    for (void* obj : candidates)
    {
        void* cls = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj));
        if (cls)
        {
            classToInstances[cls].push_back(obj);
        }
    }

    const bool bLimit = gGlobalSearch.EnableLimit && (gGlobalSearch.CustomLimitValue > 0);
    const int maxResults = bLimit ? gGlobalSearch.CustomLimitValue : 1000000;

    std::vector<std::string> searchTerms;
    {
        std::istringstream iss(loweredQuery);
        std::string token;
        while (iss >> token)
        {
            searchTerms.push_back(token);
        }
        if (searchTerms.empty())
            searchTerms.push_back(loweredQuery);
    }

    std::regex queryRegex;
    bool regexValid = false;
    if (gGlobalSearch.UseRegex)
    {
        try {
            queryRegex = std::regex(query, std::regex::icase);
            regexValid = true;
        } catch (...) {
            regexValid = false;
        }
    }

    uintptr_t searchAddress = 0;
    if (gGlobalSearch.SearchByAddress)
    {
        try {
            std::string hexStr = query;
            if (hexStr.rfind("0x", 0) == 0 || hexStr.rfind("0X", 0) == 0) hexStr = hexStr.substr(2);
            searchAddress = std::stoull(hexStr, nullptr, 16);
        } catch (...) {
            searchAddress = 0;
        }
    }

    auto MatchesAnyTerm = [&](const std::string& text) -> bool {
        if (regexValid)
        {
            try { return std::regex_search(text, queryRegex); } catch (...) { return false; }
        }
        for (const auto& term : searchTerms)
        {
            if (text.find(term) == std::string::npos)
                return false;
        }
        return true;
    };

    if (gGlobalSearch.MatchProperties && gGlobalSearch.TypeCategory != GlobalSearchTypeCategory::Functions)
    {
        for (const auto& pair : classToInstances)
        {
            if (bLimit && static_cast<int>(gGlobalSearch.Results.size()) >= maxResults)
            {
                gGlobalSearch.ResultsTruncated = true;
                break;
            }

            void* cls = pair.first;
            const auto& instances = pair.second;
            const auto& properties = GetCachedClassProperties(cls);
            std::string cachedClassName;

            for (const auto& prop : properties)
            {
                if (bLimit && static_cast<int>(gGlobalSearch.Results.size()) >= maxResults)
                {
                    gGlobalSearch.ResultsTruncated = true;
                    break;
                }

                if (!MatchesTypeCategory(prop.Type, gGlobalSearch.TypeCategory))
                    continue;

                const std::string loweredPropName = ToLowerCopy(prop.Name);
                bool nameMatches = MatchesAnyTerm(loweredPropName);

                if (nameMatches)
                {
                    if (cachedClassName.empty()) cachedClassName = ResolveCachedClassName(cls);

                    for (void* obj : instances)
                    {
                        ++gGlobalSearch.TotalMatchesFound;
                        if (static_cast<int>(gGlobalSearch.Results.size()) < maxResults)
                        {
                            GlobalSearchResult res;
                            res.Kind = GlobalSearchResultKind::Property;
                            res.ObjectPtr = obj;
                            res.ClassPtr = cls;
                            res.ObjectName = ResolveObjectName(obj);
                            res.ClassName = cachedClassName;
                            res.FullPath = ResolveObjectFullName(obj);
                            res.PropInfo = prop;
                            res.LiveValueStr = "";
                            res.Offset = prop.Offset;
                            res.MatchLabel = prop.Name;
                            gGlobalSearch.Results.push_back(res);
                        }
                        else
                        {
                            gGlobalSearch.ResultsTruncated = true;
                            if (bLimit) break;
                        }
                    }
                }
                else if (gGlobalSearch.MatchValues || searchAddress != 0)
                {
                    if (cachedClassName.empty()) cachedClassName = ResolveCachedClassName(cls);

                    for (void* obj : instances)
                    {
                        bool addressMatches = false;
                        if (searchAddress != 0 && (prop.Type == LivePropType::Object || prop.Type == LivePropType::Class) && prop.Offset >= 0)
                        {
                            void* ptr = nullptr;
                            SafeMem_Read(reinterpret_cast<uint8_t*>(obj) + prop.Offset, &ptr, sizeof(ptr));
                            if (reinterpret_cast<uintptr_t>(ptr) == searchAddress)
                                addressMatches = true;
                        }

                        bool valueMatches = false;
                        std::string liveVal;
                        if (gGlobalSearch.MatchValues && !addressMatches)
                        {
                            liveVal = LiveEditor::ReadLivePropertyValueStr(obj, prop);
                            std::string loweredVal = ToLowerCopy(liveVal);
                            valueMatches = MatchesAnyTerm(loweredVal);
                        }

                        if (addressMatches || valueMatches)
                        {
                            ++gGlobalSearch.TotalMatchesFound;
                            if (static_cast<int>(gGlobalSearch.Results.size()) < maxResults)
                            {
                                GlobalSearchResult res;
                                res.Kind = GlobalSearchResultKind::Property;
                                res.ObjectPtr = obj;
                                res.ClassPtr = cls;
                                res.ObjectName = ResolveObjectName(obj);
                                res.ClassName = cachedClassName;
                                res.FullPath = ResolveObjectFullName(obj);
                                res.PropInfo = prop;
                                res.LiveValueStr = liveVal.empty() ? LiveEditor::ReadLivePropertyValueStr(obj, prop) : liveVal;
                                res.Offset = prop.Offset;
                                res.MatchLabel = prop.Name;
                                gGlobalSearch.Results.push_back(res);
                            }
                            else
                            {
                                gGlobalSearch.ResultsTruncated = true;
                                if (bLimit) break;
                            }
                        }
                    }
                }
            }
        }
    }

    if (gGlobalSearch.MatchFunctions && (gGlobalSearch.TypeCategory == GlobalSearchTypeCategory::All || gGlobalSearch.TypeCategory == GlobalSearchTypeCategory::Functions))
    {
        for (const auto& pair : classToInstances)
        {
            if (bLimit && static_cast<int>(gGlobalSearch.Results.size()) >= maxResults)
            {
                gGlobalSearch.ResultsTruncated = true;
                break;
            }

            void* cls = pair.first;
            const auto& instances = pair.second;
            const auto& functions = GetCachedClassFunctions(cls);
            std::string cachedClassName;

            for (const auto& func : functions)
            {
                if (bLimit && static_cast<int>(gGlobalSearch.Results.size()) >= maxResults)
                {
                    gGlobalSearch.ResultsTruncated = true;
                    break;
                }

                const std::string loweredFuncName = ToLowerCopy(func.Name);
                if (MatchesAnyTerm(loweredFuncName))
                {
                    if (cachedClassName.empty()) cachedClassName = ResolveCachedClassName(cls);

                    for (void* obj : instances)
                    {
                        ++gGlobalSearch.TotalMatchesFound;
                        if (static_cast<int>(gGlobalSearch.Results.size()) < maxResults)
                        {
                            GlobalSearchResult res;
                            res.Kind = GlobalSearchResultKind::Function;
                            res.ObjectPtr = obj;
                            res.ClassPtr = cls;
                            res.ObjectName = ResolveObjectName(obj);
                            res.ClassName = cachedClassName;
                            res.FullPath = ResolveObjectFullName(obj);
                            res.FuncInfo = func;
                            res.Offset = 0;
                            res.MatchLabel = func.Name;
                            res.LiveValueStr = "UFunction (Callable)";
                            gGlobalSearch.Results.push_back(res);
                        }
                        else
                        {
                            gGlobalSearch.ResultsTruncated = true;
                            if (bLimit) break;
                        }
                    }
                }
            }
        }
    }

    if (gGlobalSearch.MatchObjects && (gGlobalSearch.TypeCategory == GlobalSearchTypeCategory::All || gGlobalSearch.TypeCategory == GlobalSearchTypeCategory::Objects))
    {
        for (const auto& pair : classToInstances)
        {
            if (bLimit && static_cast<int>(gGlobalSearch.Results.size()) >= maxResults)
            {
                gGlobalSearch.ResultsTruncated = true;
                break;
            }

            void* cls = pair.first;
            const auto& instances = pair.second;
            std::string clsName = cls ? ResolveCachedClassName(cls) : "";
            std::string loweredCls = ToLowerCopy(clsName);

            bool clsMatches = MatchesAnyTerm(loweredCls);

            if (!clsMatches && gGlobalSearch.IncludeClassHierarchy && cls)
            {
                for (void* curSuper = EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(cls)); curSuper != nullptr; curSuper = EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(curSuper)))
                {
                    std::string superName = ResolveCachedClassName(curSuper);
                    if (MatchesAnyTerm(ToLowerCopy(superName)))
                    {
                        clsMatches = true;
                        break;
                    }
                }
            }

            for (void* obj : instances)
            {
                if (bLimit && static_cast<int>(gGlobalSearch.Results.size()) >= maxResults)
                {
                    gGlobalSearch.ResultsTruncated = true;
                    break;
                }

                bool objMatches = clsMatches;
                std::string objName;

                if (!objMatches)
                {
                    objName = ResolveObjectName(obj);
                    std::string loweredObj = ToLowerCopy(objName);
                    objMatches = MatchesAnyTerm(loweredObj);
                }
                else
                {
                    objName = ResolveObjectName(obj);
                }

                if (!objMatches && searchAddress != 0)
                {
                    uintptr_t objAddr = reinterpret_cast<uintptr_t>(obj);
                    if (objAddr == searchAddress || (searchAddress >= objAddr && searchAddress < objAddr + 0x1000))
                        objMatches = true;
                }

                if (objMatches)
                {
                    ++gGlobalSearch.TotalMatchesFound;
                    if (static_cast<int>(gGlobalSearch.Results.size()) < maxResults)
                    {
                        GlobalSearchResult res;
                        res.Kind = GlobalSearchResultKind::Object;
                        res.ObjectPtr = obj;
                        res.ClassPtr = cls;
                        res.ObjectName = objName;
                        res.ClassName = clsName;
                        res.FullPath = ResolveObjectFullName(obj);
                        res.Offset = 0;
                        res.MatchLabel = objName;
                        res.LiveValueStr = FormatPointer(obj);
                        gGlobalSearch.Results.push_back(res);
                    }
                    else
                    {
                        gGlobalSearch.ResultsTruncated = true;
                        if (bLimit) break;
                    }
                }
            }
        }
    }

    SafeMem_SetFastMode(0);

    auto endTime = std::chrono::high_resolution_clock::now();
    gGlobalSearch.LastSearchTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    gGlobalSearch.IsSearching = false;

    char statusBuf[256] = {};
    if (gGlobalSearch.ResultsTruncated)
    {
        snprintf(statusBuf, sizeof(statusBuf), "Found %d+ matches (showing first %d) across %d candidate objects in %.1f ms",
                 gGlobalSearch.TotalMatchesFound, static_cast<int>(gGlobalSearch.Results.size()), static_cast<int>(candidates.size()), gGlobalSearch.LastSearchTimeMs);
    }
    else
    {
        snprintf(statusBuf, sizeof(statusBuf), "Found %d matches across %d candidate objects in %.1f ms",
                 gGlobalSearch.TotalMatchesFound, static_cast<int>(candidates.size()), gGlobalSearch.LastSearchTimeMs);
    }
    gGlobalSearch.StatusMessage = statusBuf;
}

void DrawGlobalSearchTab()
{

    LiveEditor::DrawNavigationBreadcrumbs();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Universal Live Memory & Property Finder");
    ImGui::SameLine();
    DrawMutedText("| Instant live reflection & memory search across objects, properties, and functions");

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::SetNextItemWidth(380.0f);
    bool enterPressed = ImGui::InputTextWithHint("##GlobalSearchQuery", "Search keyword, regex, or 0xAddress...",
                                                 gGlobalSearch.Query, sizeof(gGlobalSearch.Query), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.8f, 0.9f));
    if (ImGui::Button("Search##GlobalSearchBtn") || enterPressed)
    {
        ExecuteSearch();
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();
    if (ImGui::Button("Clear##GlobalSearchClearBtn"))
    {
        gGlobalSearch.Query[0] = '\0';
        ClearResults();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Limit Results##GSearchLimitToggle", &gGlobalSearch.EnableLimit);

    if (gGlobalSearch.EnableLimit)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputInt("##GSearchCustomLimit", &gGlobalSearch.CustomLimitValue, 100, 500);
        if (gGlobalSearch.CustomLimitValue < 1) gGlobalSearch.CustomLimitValue = 1;
        ImGui::SameLine();
        if (ImGui::SmallButton("Apply##GSearchLimitApply"))
        {
            if (gGlobalSearch.Query[0] != '\0') ExecuteSearch();
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Scope:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Present Level Actors##GSearchScope", gGlobalSearch.Scope == GlobalSearchScope::PresentLevel))
    {
        gGlobalSearch.Scope = GlobalSearchScope::PresentLevel;
        if (gGlobalSearch.Query[0] != '\0') ExecuteSearch();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("All World Actors##GSearchScope", gGlobalSearch.Scope == GlobalSearchScope::AllActors))
    {
        gGlobalSearch.Scope = GlobalSearchScope::AllActors;
        if (gGlobalSearch.Query[0] != '\0') ExecuteSearch();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("All Live Objects##GSearchScope", gGlobalSearch.Scope == GlobalSearchScope::AllObjects))
    {
        gGlobalSearch.Scope = GlobalSearchScope::AllObjects;
        if (gGlobalSearch.Query[0] != '\0') ExecuteSearch();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("All Objects & CDOs##GSearchScope", gGlobalSearch.Scope == GlobalSearchScope::AllObjectsAndCDOs))
    {
        gGlobalSearch.Scope = GlobalSearchScope::AllObjectsAndCDOs;
        if (gGlobalSearch.Query[0] != '\0') ExecuteSearch();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Selected Hierarchy##GSearchScope", gGlobalSearch.Scope == GlobalSearchScope::SelectedHierarchy))
    {
        gGlobalSearch.Scope = GlobalSearchScope::SelectedHierarchy;
        if (gGlobalSearch.Query[0] != '\0') ExecuteSearch();
    }

    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Targets:");
    ImGui::SameLine();
    if (ImGui::Checkbox("Properties##GSearchTarget", &gGlobalSearch.MatchProperties))
    {
        if (gGlobalSearch.Query[0] != '\0') ExecuteSearch();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Functions##GSearchTarget", &gGlobalSearch.MatchFunctions))
    {
        if (gGlobalSearch.Query[0] != '\0') ExecuteSearch();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Objects / Actors##GSearchTarget", &gGlobalSearch.MatchObjects))
    {
        if (gGlobalSearch.Query[0] != '\0') ExecuteSearch();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Match Memory Values##GSearchTarget", &gGlobalSearch.MatchValues))
    {
        if (gGlobalSearch.Query[0] != '\0') ExecuteSearch();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Regex##GSearchRegex", &gGlobalSearch.UseRegex))
    {
        if (gGlobalSearch.Query[0] != '\0') ExecuteSearch();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Address Search##GSearchAddr", &gGlobalSearch.SearchByAddress))
    {
        if (gGlobalSearch.Query[0] != '\0') ExecuteSearch();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Inheritance##GSearchInherit", &gGlobalSearch.IncludeClassHierarchy))
    {
        if (gGlobalSearch.Query[0] != '\0') ExecuteSearch();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("| Category:");
    ImGui::SameLine();

    auto DrawCategoryPill = [](const char* name, GlobalSearchTypeCategory cat) {
        bool selected = (gGlobalSearch.TypeCategory == cat);
        if (selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.5f, 0.85f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.18f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.7f, 0.8f, 1.0f));
        }

        std::string pillId = std::string(name) + "##CategoryPillBtn";
        if (ImGui::SmallButton(pillId.c_str()))
        {
            gGlobalSearch.TypeCategory = cat;
            if (gGlobalSearch.Query[0] != '\0') ExecuteSearch();
        }

        ImGui::PopStyleColor(2);
        ImGui::SameLine();
    };

    DrawCategoryPill("All", GlobalSearchTypeCategory::All);
    DrawCategoryPill("Numbers", GlobalSearchTypeCategory::Numbers);
    DrawCategoryPill("Booleans", GlobalSearchTypeCategory::Booleans);
    DrawCategoryPill("Objects", GlobalSearchTypeCategory::Objects);
    DrawCategoryPill("Strings", GlobalSearchTypeCategory::Strings);
    DrawCategoryPill("Structs", GlobalSearchTypeCategory::Structs);
    DrawCategoryPill("Functions", GlobalSearchTypeCategory::Functions);

    ImGui::NewLine();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.85f, 0.5f, 1.0f));
    ImGui::TextUnformatted(gGlobalSearch.StatusMessage.c_str());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    if (gGlobalSearch.Results.empty())
    {
        DrawMutedText("No results to display. Type a keyword (e.g. Gun, Damage, Costume, Speed, Ammo) above and press Search or Enter.");
        return;
    }

    const ImGuiTableFlags tableFlags =
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInnerH |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("GlobalSearchResultsTable", 7, tableFlags, ImVec2(0.0f, 0.0f)))
    {
        ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 75.0f);
        ImGui::TableSetupColumn("Target / Field", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Live Value (Memory)", ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableSetupColumn("Object / Actor", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableHeadersRow();

        void* pendingJumpObject = nullptr;

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(gGlobalSearch.Results.size()));
        SafeMem_SetFastMode(1);
        while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            GlobalSearchResult& res = gGlobalSearch.Results[i];

            if (res.Kind == GlobalSearchResultKind::Property && res.ObjectPtr && res.PropInfo.Offset >= 0)
            {
                res.LiveValueStr = LiveEditor::ReadLivePropertyValueStr(res.ObjectPtr, res.PropInfo);
            }

            ImGui::TableNextRow();
            ImGui::PushID(i);

            ImGui::TableSetColumnIndex(0);
            if (res.Kind == GlobalSearchResultKind::Property)
                ImGui::TextDisabled("%s", FormatOffset(res.Offset).c_str());
            else
                ImGui::TextDisabled("-");

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(res.MatchLabel.c_str());
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Target: %s\nObject: %s\nClass: %s\nFull Path: %s\nOffset: 0x%X (%d bytes)",
                                  res.MatchLabel.c_str(), res.ObjectName.c_str(), res.ClassName.c_str(), res.FullPath.c_str(), res.Offset, res.Offset);
            }

            if (ImGui::BeginPopupContextItem("##SearchResultContextMenu"))
            {
                if (ImGui::MenuItem("Copy Target Name"))
                    ImGui::SetClipboardText(res.MatchLabel.c_str());
                if (ImGui::MenuItem("Copy Full Path"))
                    ImGui::SetClipboardText(res.FullPath.c_str());
                if (res.Kind == GlobalSearchResultKind::Property)
                {
                    if (ImGui::MenuItem("Copy Offset Hex"))
                        ImGui::SetClipboardText(FormatOffset(res.Offset).c_str());
                    if (ImGui::MenuItem("Copy Offset Decimal"))
                        ImGui::SetClipboardText(std::to_string(res.Offset).c_str());
                    if (ImGui::MenuItem("Copy Current Value"))
                        ImGui::SetClipboardText(res.LiveValueStr.c_str());
                    if (ImGui::MenuItem("Pin to Watch List"))
                        LiveEditor::AddPropertyToWatches(res.ObjectPtr, res.PropInfo);
                }
                if (ImGui::MenuItem("Copy Object Address"))
                    ImGui::SetClipboardText(FormatPointer(res.ObjectPtr).c_str());
                if (ImGui::MenuItem("Inspect in Live Editor"))
                {
                    pendingJumpObject = res.ObjectPtr;
                }
                ImGui::EndPopup();
            }

            ImGui::TableSetColumnIndex(2);
            if (res.Kind == GlobalSearchResultKind::Property)
            {
                ImVec4 typeColor = LiveEditor::GetPropertyTypeColor(res.PropInfo.Type);
                ImGui::PushStyleColor(ImGuiCol_Text, typeColor);
                ImGui::TextUnformatted(res.PropInfo.TypeName.c_str());
                ImGui::PopStyleColor();
            }
            else if (res.Kind == GlobalSearchResultKind::Function)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.4f, 0.9f, 1.0f));
                ImGui::TextUnformatted("UFunction");
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                ImGui::TextUnformatted("UObject");
                ImGui::PopStyleColor();
            }

            ImGui::TableSetColumnIndex(3);
            if (res.Kind == GlobalSearchResultKind::Property)
            {
                if (gGlobalSearch.EditingRowIndex == i)
                {

                    ImGui::SetNextItemWidth(160.0f);
                    bool enterSave = ImGui::InputText("##SearchEditVal", gGlobalSearch.TempEditBuffer, sizeof(gGlobalSearch.TempEditBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::SameLine();

                    if (ImGui::SmallButton("Set##SearchSetVal") || enterSave)
                    {
                        LiveEditor::WriteLivePropertyValue(res.ObjectPtr, res.PropInfo, gGlobalSearch.TempEditBuffer);
                        res.LiveValueStr = LiveEditor::ReadLivePropertyValueStr(res.ObjectPtr, res.PropInfo);
                        gGlobalSearch.EditingRowIndex = -1;
                    }

                    ImGui::SameLine();
                    if (ImGui::SmallButton("Cancel##SearchCancelVal"))
                    {
                        gGlobalSearch.EditingRowIndex = -1;
                    }
                }
                else if (res.PropInfo.Type == LivePropType::Bool)
                {

                    bool bVal = (res.LiveValueStr == "true");
                    if (ImGui::Checkbox("##SearchBoolToggle", &bVal))
                    {
                        LiveEditor::WriteLivePropertyValue(res.ObjectPtr, res.PropInfo, bVal ? "true" : "false");
                        res.LiveValueStr = bVal ? "true" : "false";
                    }
                    ImGui::SameLine();
                    if (bVal)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.5f, 1.0f));
                        ImGui::TextUnformatted("[ON] true");
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
                        ImGui::TextUnformatted("[OFF] false");
                    }
                    ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::TextUnformatted(res.LiveValueStr.c_str());
                }
            }
            else
            {
                ImGui::TextUnformatted(res.LiveValueStr.c_str());
            }

            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(res.ObjectName.c_str());

            ImGui::TableSetColumnIndex(5);
            ImGui::TextDisabled("%s", res.ClassName.c_str());

            ImGui::TableSetColumnIndex(6);

            if (ImGui::SmallButton("Jump ->##SearchResultJump"))
            {
                pendingJumpObject = res.ObjectPtr;
            }

            if (res.Kind == GlobalSearchResultKind::Property)
            {
                ImGui::SameLine();
                if (gGlobalSearch.EditingRowIndex == i)
                {

                }
                else
                {
                    if (ImGui::SmallButton("Edit##SearchResultEdit"))
                    {
                        gGlobalSearch.EditingRowIndex = i;
                        snprintf(gGlobalSearch.TempEditBuffer, sizeof(gGlobalSearch.TempEditBuffer), "%s", res.LiveValueStr.c_str());
                    }
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("+Watch##SearchResultWatch"))
                {
                    LiveEditor::AddPropertyToWatches(res.ObjectPtr, res.PropInfo);
                }
            }
            else if (res.Kind == GlobalSearchResultKind::Function)
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("+Watch##SearchResultFuncWatch"))
                {
                    LiveEditor::AddFunctionToWatches(res.ObjectPtr, res.FuncInfo);
                }
            }

            ImGui::PopID();
        }
        clipper.End();
        SafeMem_SetFastMode(0);

        ImGui::EndTable();

        if (pendingJumpObject && IsReadableWorldExplorerObject(pendingJumpObject))
        {
            gWorldExplorer.SelectedObject = pendingJumpObject;
            LiveEditor::OnSelectObject(pendingJumpObject);
            gActiveMainView = RuntimeMainView_LiveEditor;
        }
    }
}

}
