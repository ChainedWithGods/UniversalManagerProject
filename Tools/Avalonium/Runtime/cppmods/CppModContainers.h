#pragma once

#include "../core/Types.h"
#include "sdk/Avalonium/ModApi.h"

#include <string>

namespace Avalonium::CppMods::Containers
{
    int32_t ArrayLength(EngineState& state, UObject* object, const char* propertyName);
    int32_t ArrayElementSize(EngineState& state, UObject* object, const char* propertyName);

    bool ArrayGet(EngineState& state, UObject* object, const char* propertyName, int32_t index,
                  AvaloniumValue& out, std::string* why = nullptr);
    bool ArraySet(EngineState& state, UObject* object, const char* propertyName, int32_t index,
                  const AvaloniumValue& value, std::string* why = nullptr);
    bool ArrayAdd(EngineState& state, UObject* object, const char* propertyName,
                  const AvaloniumValue& value, std::string* why = nullptr);
    bool ArrayRemove(EngineState& state, UObject* object, const char* propertyName, int32_t index,
                     std::string* why = nullptr);

    int32_t MapLength(EngineState& state, UObject* object, const char* propertyName);
    bool MapGetAt(EngineState& state, UObject* object, const char* propertyName, int32_t index,
                  AvaloniumValue& outKey, AvaloniumValue& outValue, std::string* why = nullptr);
    bool MapSetValueAt(EngineState& state, UObject* object, const char* propertyName,
                        int32_t index, const AvaloniumValue& value, std::string* why = nullptr);
    int32_t SetLength(EngineState& state, UObject* object, const char* propertyName);
    bool SetGetAt(EngineState& state, UObject* object, const char* propertyName, int32_t index,
                  AvaloniumValue& out, std::string* why = nullptr);

    bool MapAdd(EngineState& state, UObject* object, const char* propertyName,
                const AvaloniumValue& key, const AvaloniumValue& value,
                std::string* why = nullptr);
    bool MapRemove(EngineState& state, UObject* object, const char* propertyName,
                   const AvaloniumValue& key, std::string* why = nullptr);
    bool MapClear(EngineState& state, UObject* object, const char* propertyName,
                  std::string* why = nullptr);
    bool MapContains(EngineState& state, UObject* object, const char* propertyName,
                     const AvaloniumValue& key, std::string* why = nullptr);
    bool MapFind(EngineState& state, UObject* object, const char* propertyName,
                 const AvaloniumValue& key, AvaloniumValue& outValue,
                 std::string* why = nullptr);

    bool SetAdd(EngineState& state, UObject* object, const char* propertyName,
                const AvaloniumValue& value, std::string* why = nullptr);
    bool SetRemove(EngineState& state, UObject* object, const char* propertyName,
                   const AvaloniumValue& value, std::string* why = nullptr);
    bool SetClear(EngineState& state, UObject* object, const char* propertyName,
                  std::string* why = nullptr);
    bool SetContains(EngineState& state, UObject* object, const char* propertyName,
                     const AvaloniumValue& value, std::string* why = nullptr);

    bool ReadPath(EngineState& state, UObject* object, const char* path,
                  AvaloniumValue& out, std::string* why = nullptr);
    bool WritePath(EngineState& state, UObject* object, const char* path,
                   const AvaloniumValue& value, std::string* why = nullptr);
}
