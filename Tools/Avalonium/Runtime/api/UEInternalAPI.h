#pragma once

#include "../core/Types.h"

#ifdef AVALONIUM_EXPORTS
#define UE_API extern "C" __declspec(dllexport)
#else
#define UE_API extern "C" __declspec(dllimport)
#endif

UE_API bool Initialize();
UE_API void Shutdown();
UE_API const char* GetLastErrorMsg();
UE_API const char* GetStatus();
UE_API const char* GetDetectedVersion();

UE_API void* GetGUObjectArray();
UE_API void* GetFNamePool();
UE_API void* GetGWorld();
UE_API void* GetGMalloc();
UE_API void* GetGNatives();
UE_API void* GetGUObjectHashTables();

UE_API void SetLogPath(const char* path);

UE_API int GetAllUClasses(void** outClasses, int maxCount);

UE_API int GetAllActors(void** outActors, int maxCount);

UE_API int GetWorldActors(void** outActors, int maxCount);

UE_API int GetAllEnums(void** outEnums, int maxCount);

UE_API int GetAllFunctions(void** outFunctions, int maxCount);

UE_API int GetObjectCount();

UE_API const char* GetObjectName(void* obj);
UE_API const char* GetObjectFullName(void* obj);
UE_API const char* GetUClassName(void* uclass);

UE_API int GetObjectNameSafe(void* obj, char* outBuf, int bufSize);
UE_API int GetObjectFullNameSafe(void* obj, char* outBuf, int bufSize);

UE_API bool InheritsFrom(void* uclass, const char* baseClassName);
UE_API void* GetSuperClass(void* uclass);

UE_API const char* GetInheritanceChain(void* uclass);

UE_API int GetClassProperties(void* uclass, PropertyInfoEx* outProps, int maxCount);

UE_API const char* DumpAllClassesSharedMem(int* outTotalSize);

UE_API bool GenerateUSMapFile(const char* outputPath);

UE_API bool GenerateSDKFile(const char* outputPath);

UE_API bool GenerateReflectionKit(const char* outputPath);

UE_API bool GenerateReflectionDiffFile(const char* outputPath, const char* previousSnapshotPath, const char* currentSnapshotPath);
