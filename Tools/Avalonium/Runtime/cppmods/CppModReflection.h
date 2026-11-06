#pragma once

#include "../core/Types.h"
#include "CppModValue.h"
#include "sdk/Avalonium/ModApi.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Avalonium::CppMods
{
    UObject* FindObject(EngineState& state, const std::string& path);
    UClass* FindClass(EngineState& state, const std::string& name);
    UObject* FindFirstObjectOfClass(EngineState& state, UClass* objectClass);
    UObject* GetClassDefaultObject(EngineState& state, UClass* objectClass);
    int32_t FindObjectsOfClass(EngineState& state, UClass* objectClass,
                               std::vector<UObject*>& out, int32_t maxObjects);

    UFunction* FindFunction(EngineState& state, UClass* objectClass,
                            const std::string& functionName);
    bool FindProperty(EngineState& state, UClass* objectClass, const std::string& propertyName,
                      PropertyRef& out, std::string* why = nullptr);

    bool ReadProperty(EngineState& state, UObject* object, const std::string& propertyName,
                      AvaloniumValue& out, std::string* why = nullptr);
    bool WriteProperty(EngineState& state, UObject* object, const std::string& propertyName,
                       const AvaloniumValue& value, std::string* why = nullptr);

    bool ReadFunctionArgument(EngineState& state, void* function, void* frame,
                              const std::string& parameterName, AvaloniumValue& out,
                              std::string* why = nullptr);
    bool WriteFunctionArgument(EngineState& state, void* function, void* frame,
                               const std::string& parameterName, const AvaloniumValue& value,
                               std::string* why = nullptr);
    bool ReadHookResult(EngineState& state, void* function, void* result, AvaloniumValue& out,
                        std::string* why = nullptr);

    bool CallFunction(EngineState& state, UObject* object, const std::string& functionName,
                      const AvaloniumValue* args, int32_t argCount, AvaloniumValue& outResult,
                      std::string* why = nullptr);

    bool ProcessEvent(EngineState& state, UObject* object, UFunction* function, void* params);

    int32_t GetClassProperties(EngineState& state, UClass* objectClass,
                               AvaloniumPropertyInfo* outProperties, int32_t maxProperties);
    int32_t GetClassFunctions(EngineState& state, UClass* objectClass,
                              AvaloniumFunctionInfo* outFunctions, int32_t maxFunctions);

    uint32_t GetFunctionFlags(EngineState& state, void* function);
    int32_t GetFunctionFrameSize(EngineState& state, void* function);
}
