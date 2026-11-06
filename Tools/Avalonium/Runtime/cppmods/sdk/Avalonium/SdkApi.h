#pragma once

#include <stdint.h>
#include <stddef.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef AVALONIUM_SDK_API_VERSION
#define AVALONIUM_SDK_API_VERSION 1u
#endif

#if defined(_WIN32)
#  if defined(AVALONIUM_EXPORTS)
#    define AVALONIUM_SDK_API_ENTRY extern "C" __declspec(dllexport)
#  else
#    define AVALONIUM_SDK_API_ENTRY extern "C" __declspec(dllimport)
#  endif
#else
#  define AVALONIUM_SDK_API_ENTRY extern "C"
#endif

#ifndef AVALONIUM_SDK_API_TYPES_DEFINED
#define AVALONIUM_SDK_API_TYPES_DEFINED
typedef struct AvaloniumSdkApi
{
    uint32_t struct_size;
    uint32_t api_version;

    int32_t (*get_engine_major)(void);
    int32_t (*get_engine_minor)(void);

    int32_t (*get_object_count)(void);
    void* (*get_object_by_index)(int32_t index);
    int32_t (*get_object_index)(void* object);
    void* (*get_object_class)(void* object);
    void* (*get_object_outer)(void* object);
    void* (*get_super_struct)(void* object);
    const char* (*get_object_name)(void* object);
    const char* (*get_object_full_name)(void* object);
    const char* (*get_fname_string)(int32_t comparison_index, int32_t number);
    bool (*is_a)(void* object, const char* class_name);

    void* (*find_object)(const char* path);
    void* (*find_class)(const char* name);
    void* (*get_class_default_object)(void* uclass);
    void* (*find_function)(void* uclass, const char* function_name);

    bool (*process_event)(void* object, void* function, void* params);
} AvaloniumSdkApi;
#endif

AVALONIUM_SDK_API_ENTRY const AvaloniumSdkApi* AvaloniumGetSdkApi(uint32_t requested_version);

#ifdef __cplusplus
}
#endif
