#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AVALONIUM_MOD_API_VERSION 11u
#define AVALONIUM_MOD_API_MIN_VERSION 1u

#define AVALONIUM_MOD_ABI_VERSION 1u

#if defined(_WIN32)
#  if defined(AVALONIUM_EXPORTS)
#    define AVALONIUM_MOD_API_ENTRY extern "C" __declspec(dllexport)
#  else
#    define AVALONIUM_MOD_API_ENTRY extern "C" __declspec(dllimport)
#  endif
#else
#  define AVALONIUM_MOD_API_ENTRY extern "C"
#endif

typedef enum AvaloniumValueKind
{
    AVALONIUM_VALUE_NONE = 0,
    AVALONIUM_VALUE_BOOL = 1,
    AVALONIUM_VALUE_INT8 = 2,
    AVALONIUM_VALUE_INT16 = 3,
    AVALONIUM_VALUE_INT32 = 4,
    AVALONIUM_VALUE_INT64 = 5,
    AVALONIUM_VALUE_UINT8 = 6,
    AVALONIUM_VALUE_UINT16 = 7,
    AVALONIUM_VALUE_UINT32 = 8,
    AVALONIUM_VALUE_UINT64 = 9,
    AVALONIUM_VALUE_FLOAT = 10,
    AVALONIUM_VALUE_DOUBLE = 11,
    AVALONIUM_VALUE_STRING = 12,
    AVALONIUM_VALUE_NAME = 13,
    AVALONIUM_VALUE_OBJECT = 14,
    AVALONIUM_VALUE_STRUCT = 15
} AvaloniumValueKind;

typedef enum AvaloniumLogLevel
{
    AVALONIUM_LOG_DEBUG = 0,
    AVALONIUM_LOG_INFO = 1,
    AVALONIUM_LOG_WARNING = 2,
    AVALONIUM_LOG_ERROR = 3
} AvaloniumLogLevel;

typedef struct AvaloniumValue
{
    int32_t kind;
    int32_t size;
    int64_t int_value;
    double float_value;
    const void* pointer;
    const char* string;
    uint32_t name_number;
    uint32_t reserved;
} AvaloniumValue;

typedef struct AvaloniumPropertyInfo
{
    char name[128];
    char type_name[64];
    int32_t offset;
    int32_t element_size;
    int32_t array_dim;
    int32_t reserved;
    uint64_t flags;
} AvaloniumPropertyInfo;

typedef struct AvaloniumFunctionInfo
{
    char name[128];
    void* function;
    uint32_t function_flags;
    int32_t frame_size;
} AvaloniumFunctionInfo;

typedef void (*AvaloniumHookCallback)(void* user_data, void* object, void* function, void* frame, void* result);

typedef void (*AvaloniumKeyCallback)(void* user_data);

typedef void (*AvaloniumTaskCallback)(void* user_data);

typedef void (*AvaloniumWidgetCallback)(void* user_data);

typedef void (*AvaloniumImGuiCallback)(void* user_data);

typedef void* (*AvaloniumImGuiAlloc)(size_t size, void* user_data);
typedef void (*AvaloniumImGuiFree)(void* pointer, void* user_data);

typedef struct AvaloniumModApi
{
    uint32_t struct_size;
    uint32_t api_version;

    int32_t (*get_engine_major)(void);
    int32_t (*get_engine_minor)(void);
    const char* (*get_game_name)(void);
    const char* (*get_runtime_version)(void);
    const char* (*get_mod_directory)(void);

    void (*log_message)(int32_t level, const char* message);

    bool (*is_in_game_thread)(void);
    bool (*run_on_game_thread)(AvaloniumTaskCallback callback, void* user_data);
    double (*get_delta_seconds)(void);
    uint64_t (*get_frame_count)(void);

    void* (*find_object)(const char* path);
    void* (*find_class)(const char* name);
    void* (*find_first_object_of_class)(void* uclass);
    void* (*get_class_default_object)(void* uclass);
    int32_t (*find_objects_of_class)(void* uclass, void** out_objects, int32_t max_objects);
    int32_t (*get_object_count)(void);
    void* (*get_object_by_index)(int32_t index);
    bool (*is_valid_object)(void* object);
    const char* (*get_object_name)(void* object);
    const char* (*get_object_full_name)(void* object);
    const char* (*get_object_path_name)(void* object);
    void* (*get_object_class)(void* object);
    void* (*get_object_outer)(void* object);
    void* (*get_super_class)(void* uclass);
    bool (*is_a)(void* object, const char* class_name);

    void* (*find_function)(void* uclass, const char* function_name);
    int32_t (*get_class_properties)(void* uclass, AvaloniumPropertyInfo* out_properties, int32_t max_properties);
    int32_t (*get_class_functions)(void* uclass, AvaloniumFunctionInfo* out_functions, int32_t max_functions);
    uint32_t (*get_function_flags)(void* function);
    int32_t (*get_function_frame_size)(void* function);

    bool (*get_property)(void* object, const char* property_name, AvaloniumValue* out_value);
    bool (*set_property)(void* object, const char* property_name, const AvaloniumValue* value);
    bool (*get_function_argument)(void* function, void* frame, const char* parameter_name, AvaloniumValue* out_value);
    bool (*set_function_argument)(void* function, void* frame, const char* parameter_name, const AvaloniumValue* value);
    bool (*get_hook_result)(void* function, void* result, AvaloniumValue* out_value);

    bool (*call_function)(void* object, const char* function_name, const AvaloniumValue* args, int32_t arg_count, AvaloniumValue* out_result);
    bool (*process_event)(void* object, void* function, void* params);

    uint32_t (*register_hook)(const char* class_name, const char* function_name, AvaloniumHookCallback pre_callback, AvaloniumHookCallback post_callback, void* user_data);
    bool (*unregister_hook)(uint32_t hook_id);

    uint32_t (*register_key_bind)(const char* key_name, AvaloniumKeyCallback callback, void* user_data);
    bool (*unregister_key_bind)(uint32_t key_id);

    void* (*construct_object)(void* uclass, void* outer);

    void* (*get_world)(void);

    uint32_t (*bind_widget_click)(void* widget, AvaloniumWidgetCallback callback, void* user_data);
    bool (*unbind_widget_click)(uint32_t binding_id);

    uint32_t (*bind_widget_event)(void* widget, const char* event_name,
                                  AvaloniumWidgetCallback callback, void* user_data);

    int32_t (*array_length)(void* object, const char* property_name);
    int32_t (*array_element_size)(void* object, const char* property_name);
    bool (*array_get)(void* object, const char* property_name, int32_t index,
                      AvaloniumValue* out_value);
    bool (*array_set)(void* object, const char* property_name, int32_t index,
                      const AvaloniumValue* value);
    bool (*array_add)(void* object, const char* property_name, const AvaloniumValue* value);
    bool (*array_remove)(void* object, const char* property_name, int32_t index);

    int32_t (*map_length)(void* object, const char* property_name);
    bool (*map_get_at)(void* object, const char* property_name, int32_t index,
                       AvaloniumValue* out_key, AvaloniumValue* out_value);
    bool (*map_set_value_at)(void* object, const char* property_name, int32_t index,
                             const AvaloniumValue* value);
    int32_t (*set_length)(void* object, const char* property_name);
    bool (*set_get_at)(void* object, const char* property_name, int32_t index,
                       AvaloniumValue* out_value);

    void* (*get_imgui_context)(void);
    void (*get_imgui_allocator_functions)(AvaloniumImGuiAlloc* out_alloc,
                                          AvaloniumImGuiFree* out_free,
                                          void** out_user_data);
    uint32_t (*register_imgui_render)(const char* name, AvaloniumImGuiCallback callback,
                                      void* user_data);
    bool (*unregister_imgui_render)(uint32_t id);

    bool (*set_imgui_input_active)(uint32_t id, bool active);

    uint32_t (*register_hook_function)(void* function, AvaloniumHookCallback pre_callback,
                                       AvaloniumHookCallback post_callback, void* user_data);

    bool (*map_add)(void* object, const char* property_name, const AvaloniumValue* key,
                    const AvaloniumValue* value);
    bool (*map_remove)(void* object, const char* property_name, const AvaloniumValue* key);
    bool (*map_clear)(void* object, const char* property_name);
    bool (*map_contains)(void* object, const char* property_name, const AvaloniumValue* key);
    bool (*map_find)(void* object, const char* property_name, const AvaloniumValue* key,
                     AvaloniumValue* out_value);

    bool (*set_add)(void* object, const char* property_name, const AvaloniumValue* value);
    bool (*set_remove)(void* object, const char* property_name, const AvaloniumValue* value);
    bool (*set_clear)(void* object, const char* property_name);
    bool (*set_contains)(void* object, const char* property_name, const AvaloniumValue* value);

    bool (*get_property_path)(void* object, const char* path, AvaloniumValue* out_value);
    bool (*set_property_path)(void* object, const char* path, const AvaloniumValue* value);

    void* (*get_module_handle)(const char* module_name);
    void* (*find_export)(void* module, const char* symbol_name);
    uint64_t (*find_pattern)(void* module, const char* pattern, bool executable_only);
    uint64_t (*resolve_relative)(uint64_t instruction, int32_t displacement_offset,
                                 int32_t instruction_length);
    void* (*get_vtable)(void* object);
    void* (*get_virtual_function)(void* object, int32_t slot);
} AvaloniumModApi;

AVALONIUM_MOD_API_ENTRY const AvaloniumModApi* AvaloniumGetModApi(uint32_t requested_version);

#ifdef __cplusplus
}
#endif
