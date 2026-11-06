#include "UEInternalAPI.h"
#include "../core/EngineDiscovery.h"
#include "../core/FunctionFinder.h"
#include "../core/ObjectIndex.h"
#include "../core/RuntimeConfig.h"
#include <cstring>
#include <vector>
#include <string>
#include <mutex>
#include "../utils/SafeMemory.h"

EngineState g_State;
static std::mutex g_Mutex;
static char g_StringBuf[MAX_STRING_RESULT] = {};
static char g_StatusBuf[2048] = {};
static char g_VersionBuf[64] = {};
static char g_ChainBuf[4096] = {};
static char g_SharedMemName[256] = {};
extern HMODULE g_Module;

UE_API bool Initialize() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    if (g_State.bInitialized) return true;

    const RuntimeFeatureConfig& runtimeConfig = RuntimeConfig::Load(g_Module);
    bool skipConsoleManager = !runtimeConfig.EnableUnrealConsole;

    return EngineDiscovery::Initialize(g_State, skipConsoleManager);
}

UE_API void Shutdown() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_State = EngineState();
}

UE_API const char* GetLastErrorMsg() {
    return g_State.LastError;
}

UE_API const char* GetStatus() {

    snprintf(g_StatusBuf, sizeof(g_StatusBuf),
        "Game: %s\n"
        "Initialized: %s\n"
        "Version: UE%d.%d\n"
        "GUObjectArray: %p\n"
        "GNames: %p\n"
        "GWorld: %p\n"
        "GMalloc: %p\n"
        "GNatives: %p\n"
        "GUObjectHashTables: %p\n"
        "GEngine: %p\n"
        "ProcessEvent: VTable[%d]\n"
        "Class: %p\n"
        "Object: %p\n"
        "Actor: %p\n"
        "Pawn: %p\n"
        "Enum: %p\n"
        "Function: %p\n"
        "Package: %p\n"
        "Struct: %p\n"
        "World: %p\n"
        "Objects: %d\n"
        "Globals: %s\n"
        "Functions: %s\n"
        "CoreClasses: %s\n"
        "Offsets: %s\n"
        "SuperOff: 0x%X\n"
        "ChildrenOff: 0x%X\n"
        "UFieldNextOff: 0x%X\n"
        "ChildPropsOff: 0x%X\n"
        "FFieldNameOff: 0x%X\n"
        "FFieldNextOff: 0x%X\n",
        g_State.GameName[0] ? g_State.GameName : "Unknown",
        g_State.bInitialized ? "YES" : "NO",
        g_State.UEMajor, g_State.UEMinor,
        g_State.GUObjectArray, g_State.GNames, g_State.GWorld,
        g_State.GMalloc, g_State.GNatives, g_State.GUObjectHashTables,
        g_State.GEngine,
        g_State.ProcessEvent_VTableIdx,
        g_State.UClassClass ? (void*)g_State.UClassClass : (void*)g_State.UObjectClass,
        g_State.UObjectClass,
        g_State.AActorClass,
        g_State.APawnClass,
        g_State.UEnumClass,
        g_State.UFunctionClass,
        g_State.UPackageClass,
        g_State.UStructClass,
        g_State.UWorldClass,
        EngineDiscovery::GetObjectCount(g_State),
        g_State.bGlobalsFound ? "YES" : "NO",
        g_State.bFunctionsFound ? "YES" : "NO",
        g_State.bCoreClassesFound ? "YES" : "NO",
        g_State.bOffsetsDiscovered ? "YES" : "NO",
        g_State.UStruct_SuperOff,
        g_State.UStruct_ChildrenOff,
        g_State.UField_NextOff,
        g_State.UStruct_ChildPropsOff,
        g_State.FField_NameOff,
        g_State.FField_NextOff);
    return g_StatusBuf;
}

UE_API const char* GetDetectedVersion() {
    snprintf(g_VersionBuf, sizeof(g_VersionBuf), "UE%d.%d", g_State.UEMajor, g_State.UEMinor);
    return g_VersionBuf;
}

UE_API void* GetGUObjectArray() { return g_State.GUObjectArray; }
UE_API void* GetFNamePool()     { return g_State.GNames; }
UE_API void* GetGWorld()        { return g_State.GWorld; }
UE_API void* GetGMalloc()       { return g_State.GMalloc; }
UE_API void* GetGNatives()      { return g_State.GNatives; }
UE_API void* GetGUObjectHashTables() { return g_State.GUObjectHashTables; }

UE_API int GetObjectCount() {
    return EngineDiscovery::GetObjectCount(g_State);
}

struct EnumContext {
    void** outArray;
    int maxCount;
    int count;
    const EngineState* state;
    const char* filterClassName;
    UClass* filterClass;
};

static bool ReadObjectFlags(const EngineState &state, UObject *obj, uint32_t *outFlags) {
    if (!obj || !outFlags || state.UObject_FlagsOff < 0) return false;
    return SafeMem_Read(reinterpret_cast<uint8_t *>(obj) + state.UObject_FlagsOff,
                        outFlags, sizeof(uint32_t));
}

static bool IsObjectTemplate(const EngineState &state, UObject *obj) {
    uint32_t flags = 0;
    if (!ReadObjectFlags(state, obj, &flags)) return false;
    return (flags & kRfObjectTemplateMask) != 0;
}

UE_API int GetAllUClasses(void** outClasses, int maxCount) {
    if (!g_State.bInitialized || !outClasses || maxCount <= 0) return 0;

    EnumContext ctx = {};
    ctx.outArray = outClasses;
    ctx.maxCount = maxCount;
    ctx.count = 0;
    ctx.state = &g_State;

    EngineDiscovery::ForEachObject(g_State, [](UObject* obj, int32_t idx, void* ud) -> bool {
        auto* ctx = (EnumContext*)ud;
        if (ctx->count >= ctx->maxCount) return false;

        UClass* cls = EngineDiscovery::GetObjectClass(*ctx->state, obj);
            if (!cls) return true;

            if (IsObjectTemplate(*ctx->state, obj)) return true;

            if (ctx->state->UClassClass) {
                if (cls == ctx->state->UClassClass) {
                    ctx->outArray[ctx->count++] = obj;
                }
            } else {

                UClass* meta = EngineDiscovery::GetObjectClass(*ctx->state, (UObject*)cls);
                if (meta == cls) {
                    ctx->outArray[ctx->count++] = obj;
                }
            }
        return true;
    }, &ctx);

    return ctx.count;
}

UE_API int GetAllActors(void** outActors, int maxCount) {
    if (!g_State.bInitialized || !outActors || maxCount <= 0) return 0;
    if (!g_State.AActorClass) return 0;

    if (g_State.bIndexBuilt && g_State.IndexMutex) {
        int count = 0;
        std::lock_guard<std::recursive_mutex> lock(*g_State.IndexMutex);
        for (const auto& pair : g_State.ClassToInstances) {
            if (!pair.first) continue;
            if (pair.first == g_State.AActorClass || EngineDiscovery::IsStructA(g_State, reinterpret_cast<UStruct*>(pair.first), reinterpret_cast<UStruct*>(g_State.AActorClass))) {
                for (UObject* obj : pair.second) {
                    if (!obj) continue;
                    outActors[count++] = obj;
                    if (count >= maxCount) return count;
                }
            }
        }
        return count;
    }

    EnumContext ctx = {};
    ctx.outArray = outActors;
    ctx.maxCount = maxCount;
    ctx.count = 0;
    ctx.state = &g_State;

    EngineDiscovery::ForEachObject(g_State, [](UObject* obj, int32_t idx, void* ud) -> bool {
        auto* ctx = (EnumContext*)ud;
        if (ctx->count >= ctx->maxCount) return false;

        UClass* cls = EngineDiscovery::GetObjectClass(*ctx->state, obj);
        if (!cls) return true;

        if (cls == ctx->state->AActorClass || EngineDiscovery::IsStructA(*ctx->state, (UStruct*)cls, (UStruct*)ctx->state->AActorClass)) {
            ctx->outArray[ctx->count++] = obj;
        }
        return true;
    }, &ctx);

    return ctx.count;
}

UE_API int GetWorldActors(void** outActors, int maxCount) {
    if (!g_State.bInitialized || !outActors || maxCount <= 0) return 0;
    return EngineDiscovery::GetWorldActors(g_State, outActors, maxCount);
}

UE_API int GetAllEnums(void** outEnums, int maxCount) {
    if (!g_State.bInitialized || !outEnums || maxCount <= 0) return 0;

    EnumContext ctx = {};
    ctx.outArray = outEnums;
    ctx.maxCount = maxCount;
    ctx.count = 0;
    ctx.state = &g_State;

    EngineDiscovery::ForEachObject(g_State, [](UObject* obj, int32_t idx, void* ud) -> bool {
        auto* ctx = (EnumContext*)ud;
        if (ctx->count >= ctx->maxCount) return false;

        UClass* cls = EngineDiscovery::GetObjectClass(*ctx->state, obj);
            if (!cls) return true;

            if (ctx->state->UEnumClass && cls == ctx->state->UEnumClass) {
                ctx->outArray[ctx->count++] = obj;
            } else {
                std::string clsName = EngineDiscovery::GetObjectNameStr(*ctx->state, (UObject*)cls);
                if (clsName == "Enum" || clsName == "UserDefinedEnum") {
                    ctx->outArray[ctx->count++] = obj;
                }
            }
        return true;
    }, &ctx);

    return ctx.count;
}

UE_API int GetAllFunctions(void** outFunctions, int maxCount) {
    if (!g_State.bInitialized || !outFunctions || maxCount <= 0) return 0;

    EnumContext ctx = {};
    ctx.outArray = outFunctions;
    ctx.maxCount = maxCount;
    ctx.count = 0;
    ctx.state = &g_State;

    EngineDiscovery::ForEachObject(g_State, [](UObject* obj, int32_t idx, void* ud) -> bool {
        auto* ctx = (EnumContext*)ud;
        if (ctx->count >= ctx->maxCount) return false;

        UClass* cls = EngineDiscovery::GetObjectClass(*ctx->state, obj);
            if (!cls) return true;

            if (ctx->state->UFunctionClass && cls == ctx->state->UFunctionClass) {
                ctx->outArray[ctx->count++] = obj;
            } else {
                std::string clsName = EngineDiscovery::GetObjectNameStr(*ctx->state, (UObject*)cls);
                if (clsName == "Function") {
                    ctx->outArray[ctx->count++] = obj;
                }
            }
        return true;
    }, &ctx);

    return ctx.count;
}

UE_API const char* GetObjectName(void* obj) {
    if (!obj || !g_State.bInitialized) return "";
    std::string name = EngineDiscovery::GetObjectNameStr(g_State, (UObject*)obj);
    strncpy(g_StringBuf, name.c_str(), sizeof(g_StringBuf) - 1);
    return g_StringBuf;
}

UE_API const char* GetObjectFullName(void* obj) {
    if (!obj || !g_State.bInitialized) return "";
    std::string name = EngineDiscovery::BuildFullName(g_State, (UObject*)obj);
    strncpy(g_StringBuf, name.c_str(), sizeof(g_StringBuf) - 1);
    return g_StringBuf;
}

UE_API const char* GetUClassName(void* uclass) {
    return GetObjectName(uclass);
}

UE_API int GetObjectNameSafe(void* obj, char* outBuf, int bufSize) {
    if (!obj || !outBuf || bufSize <= 0 || !g_State.bInitialized) return 0;
    std::string name = EngineDiscovery::GetObjectNameStr(g_State, (UObject*)obj);
    int len = (int)name.length();
    int copyLen = (len < bufSize - 1) ? len : bufSize - 1;
    memcpy(outBuf, name.c_str(), copyLen);
    outBuf[copyLen] = 0;
    return copyLen;
}

UE_API int GetObjectFullNameSafe(void* obj, char* outBuf, int bufSize) {
    if (!obj || !outBuf || bufSize <= 0 || !g_State.bInitialized) return 0;
    std::string name = EngineDiscovery::BuildFullName(g_State, (UObject*)obj);
    int len = (int)name.length();
    int copyLen = (len < bufSize - 1) ? len : bufSize - 1;
    memcpy(outBuf, name.c_str(), copyLen);
    outBuf[copyLen] = 0;
    return copyLen;
}

UE_API bool InheritsFrom(void* uclass, const char* baseClassName) {
    if (!uclass || !baseClassName || !g_State.bInitialized) return false;
    if (g_State.UStruct_SuperOff < 0) return false;

    UStruct* current = (UStruct*)uclass;
    for (int depth = 0; depth < 50 && current; depth++) {
        std::string name = EngineDiscovery::GetObjectNameStr(g_State, (UObject*)current);
        if (name == baseClassName) return true;
        current = EngineDiscovery::GetSuperStruct(g_State, current);
    }
    return false;
}

UE_API void* GetSuperClass(void* uclass) {
    if (!uclass || !g_State.bInitialized) return nullptr;
    return EngineDiscovery::GetSuperStruct(g_State, (UStruct*)uclass);
}

UE_API const char* GetInheritanceChain(void* uclass) {
    if (!uclass || !g_State.bInitialized) { g_ChainBuf[0] = 0; return g_ChainBuf; }

    std::string chain;
    UStruct* current = (UStruct*)uclass;
    for (int depth = 0; depth < 50 && current; depth++) {
        std::string name = EngineDiscovery::GetObjectNameStr(g_State, (UObject*)current);
        if (!chain.empty()) chain += " -> ";
        chain += name;
        current = EngineDiscovery::GetSuperStruct(g_State, current);
    }

    strncpy(g_ChainBuf, chain.c_str(), sizeof(g_ChainBuf) - 1);
    return g_ChainBuf;
}

UE_API int GetClassProperties(void* uclass, PropertyInfoEx* outProps, int maxCount) {
    if (!uclass || !outProps || maxCount <= 0 || !g_State.bInitialized) return 0;

    int count = 0;
    std::string className = EngineDiscovery::GetObjectNameStr(g_State, (UObject*)uclass);

    if (g_State.UStruct_ChildPropsOff >= 0 && g_State.FField_NameOff >= 0) {
        void* currentClass = uclass;
        while (currentClass && count < maxCount) {
            std::string currentClassName = EngineDiscovery::GetObjectNameStr(g_State, (UObject*)currentClass);
            void* child = EngineDiscovery::GetChildPropsHead(g_State, (UStruct*)currentClass);

            while (child && count < maxCount) {
                PropertyInfoEx& prop = outProps[count];
            memset(&prop, 0, sizeof(prop));

            std::string name = EngineDiscovery::GetFFieldName(g_State, child);
            if (name.empty()) break;
            strncpy(prop.Name, name.c_str(), sizeof(prop.Name) - 1);

            std::string typeName = EngineDiscovery::GetFFieldTypeName(g_State, child);
            strncpy(prop.TypeName, typeName.c_str(), sizeof(prop.TypeName) - 1);

            prop.bIsArray = (typeName == "ArrayProperty");
            prop.bIsMap = (typeName == "MapProperty");
            prop.bIsSet = (typeName == "SetProperty");
            prop.bIsStruct = (typeName == "StructProperty");
            prop.bIsEnum = (typeName == "EnumProperty" || typeName == "ByteProperty");
            prop.bIsObject = (typeName == "ObjectProperty" || typeName == "ObjectPtrProperty");
            prop.bIsBool = (typeName == "BoolProperty");
            prop.bIsString = (typeName == "StrProperty" || typeName == "NameProperty" || typeName == "TextProperty");

            if (g_State.FProperty_OffsetOff >= 0) {
                SafeMem_Read((uint8_t*)child + g_State.FProperty_OffsetOff, &prop.Offset, 4);
            }

            strncpy(prop.ClassName, currentClassName.c_str(), sizeof(prop.ClassName) - 1);

            count++;
            child = EngineDiscovery::GetFFieldNext(g_State, child);
        }

        currentClass = EngineDiscovery::GetSuperStruct(g_State, (UStruct*)currentClass);
    }
    return count;
    }

    if (g_State.UStruct_ChildrenOff < 0 || g_State.UField_NextOff < 0) return 0;

    void* child = EngineDiscovery::GetChildrenHead(g_State, (UStruct*)uclass);

    while (child && count < maxCount) {
        PropertyInfoEx& prop = outProps[count];
        memset(&prop, 0, sizeof(prop));

        std::string name = EngineDiscovery::GetFieldName(g_State, child);
        if (name.empty()) break;
        strncpy(prop.Name, name.c_str(), sizeof(prop.Name) - 1);

        UClass* propClass = EngineDiscovery::GetObjectClass(g_State, (UObject*)child);
        if (propClass) {
            std::string typeName = EngineDiscovery::GetObjectNameStr(g_State, (UObject*)propClass);
            strncpy(prop.TypeName, typeName.c_str(), sizeof(prop.TypeName) - 1);
        }

        if (g_State.UProperty_OffsetOff >= 0) {
            SafeMem_Read((uint8_t*)child + g_State.UProperty_OffsetOff, &prop.Offset, 4);
        }

        strncpy(prop.ClassName, className.c_str(), sizeof(prop.ClassName) - 1);

        count++;
        child = EngineDiscovery::GetFieldNext(g_State, child);
    }

    return count;
}

static int32_t CountClassProperties(const EngineState &state, UStruct *ustruct) {
    if (!ustruct) return 0;
    int32_t count = 0;

    if (state.UStruct_ChildPropsOff >= 0 && state.FField_NameOff >= 0) {

        for (UStruct *cur = ustruct; cur; cur = EngineDiscovery::GetSuperStruct(state, cur)) {
            for (void *child = EngineDiscovery::GetChildPropsHead(state, cur); child;
                 child = EngineDiscovery::GetFFieldNext(state, child)) {
                if (EngineDiscovery::GetFFieldName(state, child).empty()) break;
                ++count;
            }
        }
        return count;
    }

    if (state.UStruct_ChildrenOff < 0 || state.UField_NextOff < 0) return 0;
    for (void *child = EngineDiscovery::GetChildrenHead(state, ustruct); child;
         child = EngineDiscovery::GetFieldNext(state, child)) {
        if (EngineDiscovery::GetFieldName(state, child).empty()) break;
        ++count;
    }
    return count;
}

static int32_t CountClassFunctions(const EngineState &state, UStruct *ustruct) {
    if (!ustruct) return 0;
    if (state.UStruct_ChildrenOff < 0 || state.UField_NextOff < 0) return 0;

    int32_t count = 0;
    for (void *child = EngineDiscovery::GetChildrenHead(state, ustruct); child;
         child = EngineDiscovery::GetFieldNext(state, child)) {

        std::string cls = EngineDiscovery::GetFFieldTypeName(state, child);
        if (cls.empty()) {
            UClass *c = EngineDiscovery::GetObjectClass(state, (UObject *)child);
            if (c) cls = EngineDiscovery::GetObjectNameStr(state, (UObject *)c);
        }

        uint32_t flags = 0;
        if (state.UFunction_FlagsOff >= 0) {
            SafeMem_Read((uint8_t *)child + state.UFunction_FlagsOff, &flags, sizeof(uint32_t));
        }

        if (cls.find("Function") != std::string::npos || flags != 0) ++count;
    }
    return count;
}

static int32_t ReadStructPropertiesSize(const EngineState &state, UStruct *ustruct) {
    if (!ustruct || state.UStruct_PropertiesSizeOff < 0) return 0;
    int32_t size = 0;
    if (!SafeMem_Read((uint8_t *)ustruct + state.UStruct_PropertiesSizeOff, &size, sizeof(int32_t)))
        return 0;
    return size > 0 ? size : 0;
}

UE_API const char* DumpAllClassesSharedMem(int* outTotalSize) {
    if (!g_State.bInitialized) {
        if (outTotalSize) *outTotalSize = 0;
        return "";
    }

    std::vector<void*> classes(10000);
    int classCount = GetAllUClasses(classes.data(), (int)classes.size());

    int totalSize = sizeof(int32_t) + classCount * sizeof(ClassInfoEx);

    snprintf(g_SharedMemName, sizeof(g_SharedMemName), "AvaloniumDump_%lu", GetCurrentProcessId());

    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, totalSize, g_SharedMemName);
    if (!hMap) {
        if (outTotalSize) *outTotalSize = 0;
        return "";
    }

    void* mapped = MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, totalSize);
    if (!mapped) {
        CloseHandle(hMap);
        if (outTotalSize) *outTotalSize = 0;
        return "";
    }

    *(int32_t*)mapped = classCount;
    ClassInfoEx* infos = (ClassInfoEx*)((uint8_t*)mapped + sizeof(int32_t));

    for (int i = 0; i < classCount; i++) {
        ClassInfoEx& info = infos[i];
        memset(&info, 0, sizeof(info));

        info.Address = classes[i];

        std::string name = EngineDiscovery::GetObjectNameStr(g_State, (UObject*)classes[i]);
        strncpy(info.Name, name.c_str(), sizeof(info.Name) - 1);

        std::string fullName = EngineDiscovery::BuildFullName(g_State, (UObject*)classes[i]);
        strncpy(info.FullPath, fullName.c_str(), sizeof(info.FullPath) - 1);

        UStruct* super = EngineDiscovery::GetSuperStruct(g_State, (UStruct*)classes[i]);
        if (super) {
            std::string superName = EngineDiscovery::GetObjectNameStr(g_State, (UObject*)super);
            strncpy(info.SuperClassName, superName.c_str(), sizeof(info.SuperClassName) - 1);
        }

        UStruct* asStruct = (UStruct*)classes[i];
        info.PropertiesCount = CountClassProperties(g_State, asStruct);
        info.FunctionsCount = CountClassFunctions(g_State, asStruct);
        info.Size = ReadStructPropertiesSize(g_State, asStruct);
    }

    UnmapViewOfFile(mapped);

    if (outTotalSize) *outTotalSize = totalSize;
    return g_SharedMemName;
}

#include "../usmap/USMapGenerator.h"

UE_API bool GenerateUSMapFile(const char* outputPath) {
    if (!g_State.bInitialized) return false;
    if (!outputPath || outputPath[0] == '\0') return false;

    std::lock_guard<std::mutex> lock(g_Mutex);
    return EngineDiscovery::GenerateUSMap(g_State, outputPath);
}

#include "../sdk/SDKGenerator.h"

UE_API bool GenerateSDKFile(const char* outputPath) {
    if (!g_State.bInitialized) return false;
    if (!outputPath || outputPath[0] == '\0') return false;

    std::lock_guard<std::mutex> lock(g_Mutex);
    return SDKGenerator::GenerateSDK(g_State, outputPath);
}

UE_API bool GenerateReflectionKit(const char* outputPath) {
    if (!g_State.bInitialized) return false;
    if (!outputPath || outputPath[0] == '\0') return false;

    std::lock_guard<std::mutex> lock(g_Mutex);
    return SDKGenerator::GenerateReflectionKit(g_State, outputPath);
}

UE_API bool GenerateReflectionDiffFile(const char* outputPath, const char* previousSnapshotPath, const char* currentSnapshotPath) {
    if (!g_State.bInitialized) return false;
    if (!outputPath || outputPath[0] == '\0') return false;
    if (!previousSnapshotPath || previousSnapshotPath[0] == '\0') return false;
    if (!currentSnapshotPath || currentSnapshotPath[0] == '\0') return false;

    std::lock_guard<std::mutex> lock(g_Mutex);
    return SDKGenerator::GenerateReflectionDiff(g_State, outputPath, previousSnapshotPath, currentSnapshotPath);
}
