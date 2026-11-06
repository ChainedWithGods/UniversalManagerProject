#pragma once

#include "../core/Types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Avalonium::CppMods::Widgets
{

    struct DelegateInfo
    {
        void* Property = nullptr;
        int32_t Offset = -1;
        int32_t ElementSize = 0;
        int32_t Stride = 0;
        bool Multicast = false;
        std::string Name;
        std::string TypeName;
    };

    struct ScriptDelegateValue
    {
        std::vector<uint8_t> Bytes;
        int32_t ObjectIndex = -1;
        int32_t Serial = 0;
        uint32_t FunctionNameIndex = 0;

        bool Bound() const
        {
            return ObjectIndex >= 0 && Serial != 0 && FunctionNameIndex != 0;
        }
    };

    bool FindDelegate(EngineState& state, void* object, const char* eventName,
                      DelegateInfo& out, std::string* why);

    bool PickDispatchFunction(EngineState& state, void* object,
                              const std::vector<uint32_t>& usedNameIndices,
                              void*& function, uint32_t& nameIndex, std::string* why);

    bool AttachMulticast(EngineState& state, void* object, const DelegateInfo& delegate,
                         uint32_t dispatchNameIndex, std::string* why);

    bool DetachMulticast(EngineState& state, void* object, const DelegateInfo& delegate,
                         uint32_t dispatchNameIndex);

    bool AttachUnicast(EngineState& state, void* object, const DelegateInfo& delegate,
                       uint32_t dispatchNameIndex, ScriptDelegateValue& original,
                       std::string* why);

    bool DetachUnicast(EngineState& state, void* object, const DelegateInfo& delegate,
                       uint32_t dispatchNameIndex, const ScriptDelegateValue& original);

    uint32_t GetFunctionNameIndex(EngineState& state, void* function);

    bool GetObjectIdentity(EngineState& state, void* object, int32_t& objectIndex,
                           int32_t& serial);

    bool IsSameObject(EngineState& state, void* object, int32_t objectIndex, int32_t serial);

    void* ResolveWeakObject(EngineState& state, int32_t objectIndex, int32_t serial);

    void* ResolveDelegateFunction(EngineState& state, void* object,
                                  uint32_t functionNameIndex);
}
