#include "CppModWidgets.h"

#include "CppModReflection.h"

#include "../core/BlueprintDiscovery.h"
#include "../core/EngineDiscovery.h"
#include "../core/ObjectArray.h"
#include "../utils/SafeMemory.h"

#include <cstdint>
#include <cstring>
#include <unordered_set>
#include <vector>

namespace Avalonium::CppMods::Widgets
{
    namespace
    {

        constexpr int32_t kWeakPtrSize = 8;
        constexpr int32_t kArrayHeaderSize = 0x10;
        constexpr int32_t kSerialOffsetInItem = 0x10;

        constexpr uint32_t kFuncNative = 0x00000400;

        constexpr uint32_t kFuncDelegate = 0x00100000;

        int32_t EngineFNameSize(const EngineState& state)
        {
            if (state.FName_Size == 4 || state.FName_Size == 8 || state.FName_Size == 12)
                return state.FName_Size;
            return 8;
        }

        int32_t DelegatePrefixSize(const EngineState& state)
        {
            return kWeakPtrSize + EngineFNameSize(state);
        }

        int32_t ReadObjectSerial(const EngineState& state, int32_t internalIndex)
        {
            if (!state.GUObjectArray || internalIndex < 0) return 0;

            void* objectsPtr = SafeMem_ReadPtr(
                reinterpret_cast<uint8_t*>(state.GUObjectArray) + state.TUObjectArray_ObjectsOff);
            if (!objectsPtr) return 0;

            const int32_t elementsPerChunk = EngineDiscovery::GetElementsPerChunk(state);
            if (elementsPerChunk <= 0) return 0;

            void* chunk = SafeMem_ReadPtr(
                reinterpret_cast<uint8_t*>(objectsPtr) +
                (internalIndex / elementsPerChunk) * static_cast<int32_t>(sizeof(void*)));
            if (!chunk) return 0;

            int32_t serial = 0;
            if (!SafeMem_Read(reinterpret_cast<uint8_t*>(chunk) +
                                  (internalIndex % elementsPerChunk) * state.UObjectArray_ItemSize +
                                  kSerialOffsetInItem,
                              &serial, sizeof(serial)))
            {
                return 0;
            }
            return serial;
        }

        int32_t ReadOrClaimObjectSerial(const EngineState& state, int32_t internalIndex)
        {
            if (!state.GUObjectArray || internalIndex < 0) return 0;

            void* objectsPtr = SafeMem_ReadPtr(
                reinterpret_cast<uint8_t*>(state.GUObjectArray) + state.TUObjectArray_ObjectsOff);
            if (!objectsPtr) return 0;

            const int32_t elementsPerChunk = EngineDiscovery::GetElementsPerChunk(state);
            if (elementsPerChunk <= 0) return 0;

            void* chunk = SafeMem_ReadPtr(
                reinterpret_cast<uint8_t*>(objectsPtr) +
                (internalIndex / elementsPerChunk) * static_cast<int32_t>(sizeof(void*)));
            if (!chunk) return 0;

            uint8_t* serialAddress =
                reinterpret_cast<uint8_t*>(chunk) +
                (internalIndex % elementsPerChunk) * state.UObjectArray_ItemSize +
                kSerialOffsetInItem;

            int32_t serial = 0;
            if (!SafeMem_Read(serialAddress, &serial, sizeof(serial))) return 0;
            if (serial != 0) return serial;

            serial = 0x40000000 + (internalIndex & 0x3FFFFFFF);
            if (!SafeMem_Write(serialAddress, &serial, sizeof(serial))) return 0;
            return serial;
        }

        bool BuildDispatchEntry(EngineState& state, int32_t objectIndex, int32_t serial,
                                uint32_t nameIndex, std::vector<uint8_t>& entry)
        {
            const int32_t fnameSize = EngineFNameSize(state);
            entry.assign(static_cast<size_t>(kWeakPtrSize + fnameSize), 0);
            std::memcpy(entry.data(), &objectIndex, sizeof(objectIndex));
            std::memcpy(entry.data() + 4, &serial, sizeof(serial));
            return EngineDiscovery::WriteFNameBytes(state, nameIndex, 0,
                                                    entry.data() + kWeakPtrSize, fnameSize);
        }

        bool ReadDelegateFields(const EngineState& state, const uint8_t* address,
                                ScriptDelegateValue& out)
        {
            const int32_t prefix = DelegatePrefixSize(state);
            out.Bytes.assign(static_cast<size_t>(prefix), 0);
            if (SafeMem_ReadSpan(address, out.Bytes.data(), out.Bytes.size()) !=
                static_cast<size_t>(prefix))
            {
                return false;
            }

            std::memcpy(&out.ObjectIndex, out.Bytes.data(), sizeof(out.ObjectIndex));
            std::memcpy(&out.Serial, out.Bytes.data() + 4, sizeof(out.Serial));
            std::memcpy(&out.FunctionNameIndex, out.Bytes.data() + kWeakPtrSize,
                        sizeof(out.FunctionNameIndex));
            return true;
        }
    }

    uint32_t GetFunctionNameIndex(EngineState& state, void* function)
    {
        if (!function || state.UObject_NameOff < 0) return 0;

        uint32_t comparisonIndex = 0;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(function) + state.UObject_NameOff,
                          &comparisonIndex, sizeof(comparisonIndex)))
        {
            return 0;
        }
        return comparisonIndex;
    }

    void* ResolveWeakObject(EngineState& state, int32_t objectIndex, int32_t serial)
    {
        if (objectIndex < 0 || serial == 0) return nullptr;
        if (ReadObjectSerial(state, objectIndex) != serial) return nullptr;
        return EngineDiscovery::GetObjectByIndex(state, objectIndex);
    }

    bool GetObjectIdentity(EngineState& state, void* object, int32_t& objectIndex,
                           int32_t& serial)
    {
        objectIndex = -1;
        serial = 0;
        if (!object || state.UObject_IndexOff < 0) return false;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(object) + state.UObject_IndexOff,
                          &objectIndex, sizeof(objectIndex)) ||
            objectIndex < 0)
        {
            return false;
        }
        serial = ReadOrClaimObjectSerial(state, objectIndex);
        return serial != 0;
    }

    bool IsSameObject(EngineState& state, void* object, int32_t objectIndex, int32_t serial)
    {
        if (!object || objectIndex < 0) return false;
        int32_t currentIndex = -1;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(object) + state.UObject_IndexOff,
                          &currentIndex, sizeof(currentIndex)) ||
            currentIndex != objectIndex)
        {
            return false;
        }
        return ResolveWeakObject(state, objectIndex, serial) == object;
    }

    void* ResolveDelegateFunction(EngineState& state, void* object,
                                  uint32_t functionNameIndex)
    {
        if (!object || functionNameIndex == 0 || !state.UFunctionClass) return nullptr;
        const std::string name =
            EngineDiscovery::GetCachedFName(state, static_cast<int32_t>(functionNameIndex));
        if (name.empty()) return nullptr;
        UClass* objectClass = EngineDiscovery::GetObjectClass(state, static_cast<UObject*>(object));
        if (!objectClass) return nullptr;

        for (UStruct* current = static_cast<UStruct*>(objectClass); current;
             current = EngineDiscovery::GetSuperStruct(state, current))
        {
            for (void* child = EngineDiscovery::GetChildrenHead(state, current); child;
                 child = EngineDiscovery::GetFieldNext(state, child))
            {
                UClass* childClass = EngineDiscovery::GetObjectClass(
                    state, static_cast<UObject*>(child));
                if (!childClass ||
                    !EngineDiscovery::IsStructA(state, reinterpret_cast<UStruct*>(childClass),
                                                reinterpret_cast<UStruct*>(state.UFunctionClass)))
                {
                    continue;
                }
                const std::string childName =
                    EngineDiscovery::GetObjectNameStr(state, static_cast<UObject*>(child));
                if (_stricmp(childName.c_str(), name.c_str()) == 0) return child;
            }
        }
        return nullptr;
    }

    bool FindDelegate(EngineState& state, void* objectPointer, const char* eventName,
                      DelegateInfo& out, std::string* why)
    {
        auto* object = static_cast<UObject*>(objectPointer);
        out = DelegateInfo{};
        if (!object)
        {
            if (why) *why = "the object is null";
            return false;
        }
        if (!eventName || !*eventName)
        {
            if (why) *why = "the delegate name is empty";
            return false;
        }

        UClass* objectClass = EngineDiscovery::GetObjectClass(state, object);
        if (!objectClass)
        {
            if (why) *why = "the object has no class";
            return false;
        }

        PropertyRef property;
        if (!FindProperty(state, objectClass, eventName, property, why)) return false;
        if (property.TypeName.find("DelegateProperty") == std::string::npos)
        {
            if (why)
                *why = "'" + std::string(eventName) + "' has the property type '" +
                       property.TypeName + "', not a delegate";
            return false;
        }
        if (property.Offset < 0)
        {
            if (why) *why = "'" + std::string(eventName) + "' has no live offset";
            return false;
        }

        const bool multicast = property.TypeName.find("Multicast") != std::string::npos;
        if (multicast && property.TypeName.find("Sparse") != std::string::npos)
        {
            if (why)
                *why = "'" + std::string(eventName) +
                       "' is a sparse delegate, which stores its list outside the object";
            return false;
        }
        const int32_t prefix = DelegatePrefixSize(state);

        if (multicast)
        {
            if (property.ElementSize != kArrayHeaderSize)
            {
                if (why)
                    *why = "'" + std::string(eventName) + "' layout is unexpected (offset " +
                           std::to_string(property.Offset) + ", size " +
                           std::to_string(property.ElementSize) + ")";
                return false;
            }
        }
        else if (property.ElementSize < prefix)
        {
            if (why)
                *why = "'" + std::string(eventName) + "' layout is unexpected (offset " +
                       std::to_string(property.Offset) + ", size " +
                       std::to_string(property.ElementSize) + ")";
            return false;
        }

        out.Property = property.Property;
        out.Offset = property.Offset;
        out.ElementSize = property.ElementSize;
        out.Stride = prefix;
        out.Multicast = multicast;
        out.Name = eventName;
        out.TypeName = property.TypeName;
        return true;
    }

    bool PickDispatchFunction(EngineState& state, void* objectPointer,
                              const std::vector<uint32_t>& usedNameIndices,
                              void*& function, uint32_t& nameIndex, std::string* why)
    {
        function = nullptr;
        nameIndex = 0;

        auto* object = static_cast<UObject*>(objectPointer);
        UClass* objectClass = object ? EngineDiscovery::GetObjectClass(state, object) : nullptr;
        if (!objectClass)
        {
            if (why) *why = "the object has no class";
            return false;
        }
        if (!state.UFunctionClass)
        {
            if (why) *why = "the UFunction class is not resolved";
            return false;
        }

        std::unordered_set<uint32_t> used(usedNameIndices.begin(), usedNameIndices.end());
        std::unordered_set<uint32_t> seen;

        for (UStruct* current = static_cast<UStruct*>(objectClass); current;
             current = EngineDiscovery::GetSuperStruct(state, current))
        {
            for (void* child = EngineDiscovery::GetChildrenHead(state, current); child;
                 child = EngineDiscovery::GetFieldNext(state, child))
            {
                UObject* field = static_cast<UObject*>(child);
                UClass* fieldClass = EngineDiscovery::GetObjectClass(state, field);

                if (!fieldClass ||
                    !EngineDiscovery::IsStructA(state, reinterpret_cast<UStruct*>(fieldClass),
                                                reinterpret_cast<UStruct*>(state.UFunctionClass)))
                {
                    continue;
                }

                const uint32_t candidateName = GetFunctionNameIndex(state, child);
                if (candidateName == 0 || !seen.insert(candidateName).second) continue;

                const uint32_t flags = GetFunctionFlags(state, child);
                if ((flags & kFuncDelegate) == 0) continue;
                if ((flags & kFuncNative) != 0) continue;
                if (used.find(candidateName) != used.end()) continue;

                function = child;
                nameIndex = candidateName;
                return true;
            }
        }

        if (why)
        {
            *why = "the class has no free delegate signature function to dispatch through";
        }
        return false;
    }

    bool AttachMulticast(EngineState& state, void* objectPointer, const DelegateInfo& delegate,
                         uint32_t dispatchNameIndex, std::string* why)
    {
        auto* object = static_cast<UObject*>(objectPointer);
        if (!object || !delegate.Multicast || delegate.Offset < 0 || dispatchNameIndex == 0)
        {
            if (why) *why = "the multicast delegate is not resolved";
            return false;
        }

        int32_t internalIndex = -1;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(object) + state.UObject_IndexOff,
                          &internalIndex, sizeof(internalIndex)) ||
            internalIndex < 0)
        {
            if (why) *why = "the object has no object-array index";
            return false;
        }

        const int32_t serial = ReadOrClaimObjectSerial(state, internalIndex);
        if (serial == 0)
        {
            if (why) *why = "the object has no object-array serial number";
            return false;
        }

        std::vector<uint8_t> entry;
        if (!BuildDispatchEntry(state, internalIndex, serial, dispatchNameIndex, entry))
        {
            if (why) *why = "the dispatch name could not be written into the delegate";
            return false;
        }

        const int32_t stride = delegate.Stride;
        uint8_t* array = reinterpret_cast<uint8_t*>(object) + delegate.Offset;
        void* data = nullptr;
        int32_t num = 0;
        int32_t max = 0;
        if (!SafeMem_Read(array, &data, sizeof(data)) ||
            !SafeMem_Read(array + 8, &num, sizeof(num)) ||
            !SafeMem_Read(array + 12, &max, sizeof(max)))
        {
            if (why) *why = "the delegate array header is not readable";
            return false;
        }
        if (num < 0 || max < 0 || num > 4096 || max < num || (num > 0 && !data))
        {
            if (why) *why = "the delegate array header is implausible";
            return false;
        }

        for (int32_t i = 0; i < num && data; ++i)
        {
            uint8_t* source = reinterpret_cast<uint8_t*>(data) +
                              static_cast<size_t>(i) * static_cast<size_t>(stride);
            int32_t entryIndex = -1;
            int32_t entrySerial = 0;
            uint32_t entryName = 0;
            if (!SafeMem_Read(source, &entryIndex, sizeof(entryIndex)) ||
                !SafeMem_Read(source + 4, &entrySerial, sizeof(entrySerial)) ||
                !SafeMem_Read(source + kWeakPtrSize, &entryName, sizeof(entryName)))
            {
                break;
            }
            if (entryIndex == internalIndex && entrySerial == serial &&
                entryName == dispatchNameIndex)
            {
                return true;
            }
        }

        if (num < max && data)
        {
            if (!SafeMem_Write(reinterpret_cast<uint8_t*>(data) +
                                   static_cast<size_t>(num) * static_cast<size_t>(stride),
                               entry.data(), entry.size()))
            {
                if (why) *why = "the delegate entry could not be written";
                return false;
            }
            const int32_t next = num + 1;
            if (!SafeMem_Write(array + 8, &next, sizeof(next)))
            {
                if (why) *why = "the delegate count could not be written";
                return false;
            }
            return true;
        }

        void* mallocInstance = state.GMallocLocation
                                   ? SafeMem_ReadPtr(state.GMallocLocation)
                                   : state.GMalloc;
        if (!mallocInstance || state.GMallocMallocSlot < 0)
        {
            if (why) *why = "the engine allocator is not resolved";
            return false;
        }

        const size_t bytes = static_cast<size_t>(num + 1) * static_cast<size_t>(stride);
        void* buffer = SafeMem_CallMallocVftable(mallocInstance, bytes, 0,
                                                 state.GMallocMallocSlot);
        if (!buffer)
        {
            if (why) *why = "the delegate buffer could not be allocated";
            return false;
        }

        if (num > 0 && data)
        {
            const size_t existing = static_cast<size_t>(num) * static_cast<size_t>(stride);
            if (SafeMem_ReadSpan(data, buffer, existing) != existing)
            {
                if (why) *why = "the existing delegate entries could not be copied";
                return false;
            }
        }
        std::memcpy(reinterpret_cast<uint8_t*>(buffer) +
                        static_cast<size_t>(num) * static_cast<size_t>(stride),
                    entry.data(), entry.size());

        const int32_t next = num + 1;
        if (!SafeMem_Write(array, &buffer, sizeof(buffer)) ||
            !SafeMem_Write(array + 8, &next, sizeof(next)) ||
            !SafeMem_Write(array + 12, &next, sizeof(next)))
        {
            if (why) *why = "the grown delegate array could not be published";
            return false;
        }
        return true;
    }

    bool DetachMulticast(EngineState& state, void* objectPointer, const DelegateInfo& delegate,
                         uint32_t dispatchNameIndex)
    {
        auto* object = static_cast<UObject*>(objectPointer);
        if (!object || !delegate.Multicast || delegate.Offset < 0 || dispatchNameIndex == 0)
            return false;

        int32_t internalIndex = -1;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(object) + state.UObject_IndexOff,
                          &internalIndex, sizeof(internalIndex)) ||
            internalIndex < 0)
        {
            return false;
        }
        const int32_t serial = ReadObjectSerial(state, internalIndex);
        if (serial == 0) return false;

        const int32_t stride = delegate.Stride;
        uint8_t* array = reinterpret_cast<uint8_t*>(object) + delegate.Offset;
        void* data = nullptr;
        int32_t num = 0;
        if (!SafeMem_Read(array, &data, sizeof(data)) ||
            !SafeMem_Read(array + 8, &num, sizeof(num)) || num <= 0 || !data)
        {
            return false;
        }
        if (num > 4096) return false;

        std::vector<uint8_t> kept;
        kept.reserve(static_cast<size_t>(num) * static_cast<size_t>(stride));

        for (int32_t i = 0; i < num; ++i)
        {
            uint8_t* source = reinterpret_cast<uint8_t*>(data) +
                              static_cast<size_t>(i) * static_cast<size_t>(stride);
            int32_t entryIndex = -1;
            int32_t entrySerial = 0;
            uint32_t entryName = 0;
            if (!SafeMem_Read(source, &entryIndex, sizeof(entryIndex)) ||
                !SafeMem_Read(source + 4, &entrySerial, sizeof(entrySerial)) ||
                !SafeMem_Read(source + kWeakPtrSize, &entryName, sizeof(entryName)))
            {
                return false;
            }
            const bool ours = entryIndex == internalIndex && entrySerial == serial &&
                              entryName == dispatchNameIndex;
            if (ours) continue;
            kept.insert(kept.end(), source, source + stride);
        }

        const int32_t remaining = static_cast<int32_t>(kept.size() / static_cast<size_t>(stride));
        if (remaining == num) return true;
        if (remaining > 0 && !SafeMem_Write(data, kept.data(), kept.size()))
        {
            return false;
        }
        if (!SafeMem_Write(array + 8, &remaining, sizeof(remaining))) return false;
        return true;
    }

    bool AttachUnicast(EngineState& state, void* objectPointer, const DelegateInfo& delegate,
                       uint32_t dispatchNameIndex, ScriptDelegateValue& original,
                       std::string* why)
    {
        auto* object = static_cast<UObject*>(objectPointer);
        if (!object || delegate.Multicast || delegate.Offset < 0 || dispatchNameIndex == 0)
        {
            if (why) *why = "the single-cast delegate is not resolved";
            return false;
        }

        uint8_t* value = reinterpret_cast<uint8_t*>(object) + delegate.Offset;
        if (!ReadDelegateFields(state, value, original))
        {
            if (why) *why = "the delegate value is not readable";
            return false;
        }

        int32_t internalIndex = -1;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(object) + state.UObject_IndexOff,
                          &internalIndex, sizeof(internalIndex)) ||
            internalIndex < 0)
        {
            if (why) *why = "the object has no object-array index";
            return false;
        }

        const int32_t serial = ReadOrClaimObjectSerial(state, internalIndex);
        if (serial == 0)
        {
            if (why) *why = "the object has no object-array serial number";
            return false;
        }

        std::vector<uint8_t> entry;
        if (!BuildDispatchEntry(state, internalIndex, serial, dispatchNameIndex, entry))
        {
            if (why) *why = "the dispatch name could not be written into the delegate";
            return false;
        }
        if (!SafeMem_Write(value, entry.data(), entry.size()))
        {
            if (why) *why = "the delegate value could not be written";
            return false;
        }
        return true;
    }

    bool DetachUnicast(EngineState& state, void* objectPointer, const DelegateInfo& delegate,
                       uint32_t dispatchNameIndex, const ScriptDelegateValue& original)
    {
        auto* object = static_cast<UObject*>(objectPointer);
        if (!object || delegate.Multicast || delegate.Offset < 0 || dispatchNameIndex == 0)
            return false;

        int32_t internalIndex = -1;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(object) + state.UObject_IndexOff,
                          &internalIndex, sizeof(internalIndex)) ||
            internalIndex < 0)
        {
            return false;
        }
        const int32_t serial = ReadObjectSerial(state, internalIndex);
        if (serial == 0) return false;

        uint8_t* value = reinterpret_cast<uint8_t*>(object) + delegate.Offset;
        ScriptDelegateValue current;
        if (!ReadDelegateFields(state, value, current)) return false;

        if (current.ObjectIndex != internalIndex || current.Serial != serial ||
            current.FunctionNameIndex != dispatchNameIndex)
        {
            return false;
        }

        if (original.Bytes.empty()) return false;
        return SafeMem_Write(value, original.Bytes.data(), original.Bytes.size());
    }
}
