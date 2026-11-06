#pragma once

#include "OverlayInternal.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

enum class LivePropType {
    Unknown,
    Bool,
    Byte,
    Int8,
    Int16,
    Int32,
    Int64,
    UInt16,
    UInt32,
    UInt64,
    Float,
    Double,
    Str,
    Name,
    Text,
    Object,
    Class,
    Struct,
    Enum,
    Array,
    Map,
    Set
};

struct LivePropertyInfo {
    std::string Name;
    std::string TypeName;
    std::string OwnerClassName;
    int32_t Offset = 0;
    int32_t ElementSize = 0;
    LivePropType Type = LivePropType::Unknown;
    void* FieldPointer = nullptr;
    void* InnerProp = nullptr;
    void* StructOrEnum = nullptr;

    uint8_t FieldMask = 1;
    uint8_t ByteMask = 1;
    uint8_t ByteOffset = 0;
    bool bIsBitfield = false;

    std::vector<LivePropertyInfo> InnerProperties;
    bool IsExpanded = false;
};

struct LiveFunctionParam {
    std::string Name;
    std::string TypeName;
    int32_t Offset = 0;
    int32_t ElementSize = 0;
    uint64_t PropertyFlags = 0;
    LivePropType Type = LivePropType::Unknown;
    bool bIsOutParam = false;
    bool bIsReturnParam = false;
    char InputBuffer[256] = {};
    bool BoolValue = false;
    int64_t IntValue = 0;
    double FloatValue = 0.0;
};

struct LiveFunctionInfo {
    void* FunctionPtr = nullptr;
    std::string Name;
    std::string FullName;
    uint32_t FunctionFlags = 0;
    int32_t ParmsSize = 0;
    int32_t ReturnValueOffset = -1;
    std::vector<LiveFunctionParam> Params;
    std::string LastResult;
    bool ExecutionSuccess = false;
    bool HasExecuted = false;
    float ExecutionTimeMs = 0.0f;
};

struct LiveWatchItem {
    void* ObjectPtr = nullptr;
    std::string ObjectName;
    std::string ObjectPath;
    std::string ClassName;
    std::string PropertyName;
    std::string TypeName;
    int32_t Offset = 0;
    LivePropType Type = LivePropType::Unknown;
    std::string CurrentValueStr;
    std::string LastValueStr;
    char EditBuffer[256] = {};
    bool IsEditing = false;
    bool Enabled = true;
    bool LogToFile = false;
    bool ShowHistory = false;
    bool bIsBitfield = false;
    uint8_t FieldMask = 1;
    uint8_t ByteOffset = 0;
    bool IsFunctionWatch = false;
    uint32_t InvocationCount = 0;
    std::vector<std::string> History;
};

struct LiveActorComponentEntry {
    void* ComponentPtr = nullptr;
    std::string Name;
    std::string ClassName;
    std::string FullName;
    bool IsRootComponent = false;
};

struct LiveEditorState {
    std::vector<LivePropertyInfo> Properties;
    std::vector<LiveFunctionInfo> Functions;
    std::vector<LiveWatchItem> Watches;
    std::vector<LiveActorComponentEntry> Components;

    void* CurrentObject = nullptr;
    void* CurrentClass = nullptr;

    char PropertyFilter[128] = {};
    char FunctionFilter[128] = {};
    bool ShowInheritedProperties = true;
    bool ShowInheritedFunctions = true;
    bool AutoRefreshLiveValues = true;
    bool ShowUnreflectedGaps = true;
    float RefreshIntervalSec = 0.1f;
    float TimeSinceLastRefresh = 0.0f;

    int32_t EditingPropertyOffset = -1;
    std::string EditingPropertyName;
    char TempEditBuffer[512] = {};

    std::vector<void*> History;
    int HistoryIndex = -1;

    std::string ActionStatus;
    float ActionStatusTimer = 0.0f;

    bool EnableLimit = false;
    int CustomLimitValue = 50;
};

extern LiveEditorState gLiveEditor;

namespace LiveEditor {

void Initialize();
void OnSelectObject(void* object);
void NavigateHistory(int direction);
void AddObjectToHistory(void* object);

LivePropType ClassifyPropertyType(const std::string& typeName);
ImVec4 GetPropertyTypeColor(LivePropType type);

std::vector<LivePropertyInfo> EnumerateClassProperties(void* uclass);
std::vector<LivePropertyInfo> EnumerateStructProperties(void* ustruct);
std::vector<LiveFunctionInfo> EnumerateClassFunctions(void* uclass);
std::vector<std::pair<std::string, int64_t>> GetEnumNamesAndValues(void* uenum);

void RefreshProperties(void* object);
void RefreshFunctions(void* object);
void RefreshComponents(void* object);

std::string ReadLivePropertyValueStr(void* object, const LivePropertyInfo& prop, int32_t parentOffset = 0);
bool WriteLivePropertyValue(void* object, const LivePropertyInfo& prop, const std::string& newValueStr, int32_t parentOffset = 0);
bool ExecuteLiveFunction(void* object, LiveFunctionInfo& func);

void AddPropertyToWatches(void* object, const LivePropertyInfo& prop, int32_t parentOffset = 0);
void AddFunctionToWatches(void* object, const LiveFunctionInfo& func);
void RemoveWatch(size_t index);
void ClearAllWatches();
void UpdateWatches();
void SaveWatchesToDisk();
void LoadWatchesFromDisk();

void DrawLiveEditorTab();
void DrawLivePropertiesTab();
void DrawLiveFunctionCallerTab();
void DrawLiveWatchesTab();
void DrawLiveComponentsTab();
void DrawNavigationBreadcrumbs();

}
