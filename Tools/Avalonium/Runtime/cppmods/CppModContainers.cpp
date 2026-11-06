#include "CppModContainers.h"

#include "CppModReflection.h"
#include "CppModValue.h"

#include "../core/BlueprintDiscovery.h"
#include "../core/EngineDiscovery.h"
#include "../core/ObjectUtils.h"
#include "../utils/SafeMemory.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace Avalonium::CppMods::Containers
{
    namespace
    {

        constexpr int32_t kArrayHeaderSize = 0x10;
        constexpr int32_t kMaxElements = 1 << 24;
        constexpr size_t kMaxMoveBytes = 64u * 1024u * 1024u;

        struct ArrayView
        {
            uint8_t* Header = nullptr;
            uint8_t* Data = nullptr;
            int32_t Num = 0;
            int32_t Max = 0;
            int32_t ElementSize = 0;
            PropertyRef Container;
            PropertyRef Element;
        };

        PropertyRef MakeElementRef(EngineState& state, void* field)
        {
            PropertyRef element;
            element.Property = field;
            element.TypeName = EngineDiscovery::GetFFieldTypeName(state, field);
            element.Offset = 0;

            int32_t size = 0;
            if (state.FProperty_ElementSizeOff >= 0 &&
                SafeMem_Read(reinterpret_cast<uint8_t*>(field) + state.FProperty_ElementSizeOff,
                             &size, sizeof(size)) &&
                size > 0)
            {
                element.ElementSize = size;
            }
            else
            {
                element.ElementSize = 8;
            }

            uint64_t flags = 0;
            if (state.FProperty_PropertyFlagsOff >= 0)
            {
                SafeMem_Read(reinterpret_cast<uint8_t*>(field) +
                                 state.FProperty_PropertyFlagsOff,
                             &flags, sizeof(flags));
            }
            element.Flags = flags;

            if (element.IsBool() && state.FBoolProperty_BaseOff >= 0)
            {
                SafeMem_Read(reinterpret_cast<uint8_t*>(field) + state.FBoolProperty_BaseOff + 1,
                             &element.BoolByteOffset, 1);
                SafeMem_Read(reinterpret_cast<uint8_t*>(field) + state.FBoolProperty_BaseOff + 2,
                             &element.BoolByteMask, 1);
                SafeMem_Read(reinterpret_cast<uint8_t*>(field) + state.FBoolProperty_BaseOff + 3,
                             &element.BoolFieldMask, 1);
                if (element.BoolByteMask == 0) element.BoolByteMask = 1;
                if (element.BoolFieldMask == 0)
                    element.BoolFieldMask = element.BoolByteMask;
            }
            return element;
        }

        bool OpenArray(EngineState& state, const PropertyRef& property, uint8_t* header,
                       ArrayView& out, std::string* why)
        {
            if (property.Offset < 0 || !property.Property || !header)
            {
                if (why) *why = "the array property has no usable offset";
                return false;
            }

            void* innerField = nullptr;
            if (state.FArrayProperty_InnerOff < 0 ||
                !SafeMem_Read(reinterpret_cast<uint8_t*>(property.Property) +
                                  state.FArrayProperty_InnerOff,
                              &innerField, sizeof(innerField)) ||
                !innerField)
            {
                if (why) *why = "the array has no element property";
                return false;
            }

            out.Header = header;
            out.Container = property;
            out.Element = MakeElementRef(state, innerField);
            out.ElementSize = out.Element.ElementSize > 0 ? out.Element.ElementSize : 8;
            if (out.ElementSize > 0x10000)
            {
                if (why) *why = "the array element size is implausible";
                return false;
            }

            if (!SafeMem_Read(out.Header, &out.Data, sizeof(out.Data)) ||
                !SafeMem_Read(out.Header + 8, &out.Num, sizeof(out.Num)) ||
                !SafeMem_Read(out.Header + 12, &out.Max, sizeof(out.Max)))
            {
                if (why) *why = "the array header is not readable";
                return false;
            }
            if (out.Num < 0 || out.Max < out.Num || out.Num > kMaxElements)
            {
                if (why) *why = "the array header is implausible";
                return false;
            }
            if (out.Num > 0 && (!out.Data ||
                                !SafeMem_IsReadable(out.Data,
                                                    static_cast<size_t>(out.Num) *
                                                        static_cast<size_t>(out.ElementSize))))
            {
                if (why) *why = "the array elements are not readable";
                return false;
            }
            return true;
        }

        bool ResolveArray(EngineState& state, UObject* object, const char* propertyName,
                          ArrayView& out, std::string* why)
        {
            if (!object || !propertyName || !*propertyName)
            {
                if (why) *why = "the object or property name is empty";
                return false;
            }

            UClass* objectClass = EngineDiscovery::GetObjectClass(state, object);
            if (!objectClass)
            {
                if (why) *why = "the object has no class";
                return false;
            }

            PropertyRef property;
            if (!FindProperty(state, objectClass, propertyName, property, why)) return false;
            if (property.TypeName.find("ArrayProperty") == std::string::npos)
            {
                if (why)
                    *why = "'" + std::string(propertyName) + "' is a " + property.TypeName +
                           ", not an ArrayProperty";
                return false;
            }

            return OpenArray(state, property,
                             reinterpret_cast<uint8_t*>(object) + property.Offset, out, why);
        }

        void* AllocateEngineBuffer(EngineState& state, size_t bytes)
        {
            void* mallocInstance = state.GMallocLocation
                                       ? SafeMem_ReadPtr(state.GMallocLocation)
                                       : state.GMalloc;
            if (!mallocInstance) mallocInstance = state.GMalloc;
            if (!mallocInstance || state.GMallocMallocSlot < 0 || bytes == 0) return nullptr;
            return SafeMem_CallMallocVftable(mallocInstance, bytes, 0, state.GMallocMallocSlot);
        }

        constexpr int32_t kInlineWordBytes = 4 * static_cast<int32_t>(sizeof(uint32_t));
        constexpr int32_t kBitArrayInstanceSize =
            kInlineWordBytes + static_cast<int32_t>(sizeof(void*));
        constexpr int32_t kBitArrayHeaderSize =
            kBitArrayInstanceSize + 2 * static_cast<int32_t>(sizeof(int32_t));
        constexpr int32_t kNumFreeIndicesOffset =
            kArrayHeaderSize + kBitArrayHeaderSize + static_cast<int32_t>(sizeof(int32_t));

        int32_t AlignUp(int32_t value, int32_t alignment)
        {
            if (alignment <= 1) return value;
            return ((value + alignment - 1) / alignment) * alignment;
        }

        struct StructBuilder
        {
            int32_t End = 0;
            int32_t Alignment = 0;

            int32_t Add(int32_t size, int32_t alignment)
            {
                if (alignment < 1) alignment = 1;
                const int32_t offset = AlignUp(End, alignment);
                End = offset + size;
                if (alignment > Alignment) Alignment = alignment;
                return offset;
            }
            int32_t Size() const { return AlignUp(End, Alignment); }
        };

        int32_t SparseSlotStride(int32_t elementSize, int32_t elementAlignment)
        {
            constexpr int32_t kFreeListLinkSize = 2 * static_cast<int32_t>(sizeof(int32_t));
            constexpr int32_t kFreeListLinkAlign = static_cast<int32_t>(sizeof(int32_t));
            const int32_t alignment =
                elementAlignment > kFreeListLinkAlign ? elementAlignment : kFreeListLinkAlign;
            const int32_t size = elementSize > kFreeListLinkSize ? elementSize : kFreeListLinkSize;
            return AlignUp(size, alignment);
        }

        struct SparseSlots
        {
            const uint32_t* Words = nullptr;
            int32_t NumBits = 0;
            bool Valid = false;
            int32_t Allocated = 0;

            bool IsAllocated(int32_t index) const
            {
                if (!Valid) return true;
                if (index < 0 || index >= NumBits) return false;
                return ((Words[index >> 5] >> (index & 31)) & 1u) != 0;
            }
        };

        SparseSlots ReadSparseSlots(EngineState& state, const uint8_t* sparseArray,
                                    int32_t slotCount)
        {
            (void)state;
            SparseSlots result;
            result.NumBits = slotCount > 0 ? slotCount : 0;
            result.Allocated = result.NumBits;
            if (!sparseArray || slotCount <= 0) return result;

            const uint8_t* bitmap = sparseArray + kArrayHeaderSize;
            const void* secondary = nullptr;
            int32_t bitCount = 0;
            if (!SafeMem_Read(bitmap + kInlineWordBytes, &secondary, sizeof(secondary)) ||
                !SafeMem_Read(bitmap + kBitArrayInstanceSize, &bitCount, sizeof(bitCount)) ||
                bitCount != slotCount)
            {
                return result;
            }

            int32_t freeSlots = 0;
            if (!SafeMem_Read(sparseArray + kNumFreeIndicesOffset, &freeSlots, sizeof(freeSlots)) ||
                freeSlots < 0 || freeSlots > slotCount)
            {
                return result;
            }

            const size_t wordBytes =
                static_cast<size_t>((slotCount + 31) / 32) * sizeof(uint32_t);
            const bool inlineHoldsAll = wordBytes <= static_cast<size_t>(kInlineWordBytes);
            const uint32_t* inlineWords =
                inlineHoldsAll ? reinterpret_cast<const uint32_t*>(bitmap) : nullptr;

            const uint32_t* candidates[2] = {inlineWords, nullptr};
            int32_t candidateCount = inlineWords ? 1 : 0;
            if (secondary && SafeMem_IsReadable(secondary, wordBytes))
            {
                candidates[0] = static_cast<const uint32_t*>(secondary);
                candidates[1] = inlineWords;
                candidateCount = inlineWords ? 2 : 1;
            }

            for (int32_t candidate = 0; candidate < candidateCount; ++candidate)
            {
                const uint32_t* words = candidates[candidate];
                if (!words) continue;
                if (candidate > 0 && words == candidates[candidate - 1]) continue;

                int32_t used = 0;
                for (int32_t i = 0; i < bitCount; ++i)
                {
                    if (((words[i >> 5] >> (i & 31)) & 1u) != 0) ++used;
                }
                if (used != slotCount - freeSlots) continue;

                result.Words = words;
                result.Valid = true;
                result.Allocated = used;
                return result;
            }
            return result;
        }

        struct MapView
        {
            uint8_t* Data = nullptr;
            int32_t Slots = 0;
            int32_t Count = 0;
            int32_t ValueOffset = 0;
            int32_t Stride = 0;
            PropertyRef Container;
            PropertyRef Key;
            PropertyRef Value;
            SparseSlots Allocated;
        };

        struct SetView
        {
            uint8_t* Data = nullptr;
            int32_t Slots = 0;
            int32_t Count = 0;
            int32_t Stride = 0;
            PropertyRef Container;
            PropertyRef Element;
            SparseSlots Allocated;
        };

        bool OpenMap(EngineState& state, const PropertyRef& property, uint8_t* header,
                     MapView& out, std::string* why)
        {
            if (!property.Property || !header)
            {
                if (why) *why = "the map property has no usable address";
                return false;
            }

            void* keyField = nullptr;
            void* valueField = nullptr;
            if (state.FMapProperty_KeyPropOff < 0 || state.FMapProperty_ValuePropOff < 0 ||
                !SafeMem_Read(reinterpret_cast<uint8_t*>(property.Property) +
                                  state.FMapProperty_KeyPropOff,
                              &keyField, sizeof(keyField)) ||
                !SafeMem_Read(reinterpret_cast<uint8_t*>(property.Property) +
                                  state.FMapProperty_ValuePropOff,
                              &valueField, sizeof(valueField)) ||
                !keyField || !valueField)
            {
                if (why) *why = "the map has no key or value property";
                return false;
            }

            out.Container = property;
            out.Key = MakeElementRef(state, keyField);
            out.Value = MakeElementRef(state, valueField);
            const int32_t keySize = out.Key.ElementSize > 0 ? out.Key.ElementSize : 8;
            const int32_t valueSize = out.Value.ElementSize > 0 ? out.Value.ElementSize : 8;

            StructBuilder pair;
            pair.Add(keySize, EngineDiscovery::GetPropertyAlignment(state, keyField, keySize));
            out.ValueOffset =
                pair.Add(valueSize, EngineDiscovery::GetPropertyAlignment(state, valueField,
                                                                          valueSize));
            StructBuilder slot;
            slot.Add(pair.Size(), pair.Alignment);
            slot.Add(static_cast<int32_t>(sizeof(int32_t)),
                     static_cast<int32_t>(sizeof(int32_t)));
            slot.Add(static_cast<int32_t>(sizeof(int32_t)),
                     static_cast<int32_t>(sizeof(int32_t)));
            out.Stride = SparseSlotStride(slot.Size(), slot.Alignment);
            if (out.Stride <= 0 || out.Stride > 0x100000)
            {
                if (why) *why = "the map slot layout is implausible";
                return false;
            }

            if (!SafeMem_Read(header, &out.Data, sizeof(out.Data)) ||
                !SafeMem_Read(header + 8, &out.Slots, sizeof(out.Slots)))
            {
                if (why) *why = "the map header is not readable";
                return false;
            }
            if (out.Slots < 0 || out.Slots > kMaxElements)
            {
                if (why) *why = "the map slot count is implausible";
                return false;
            }

            out.Allocated = ReadSparseSlots(state, header, out.Slots);
            out.Count = out.Allocated.Valid ? out.Allocated.Allocated : out.Slots;
            return true;
        }

        bool ResolveMap(EngineState& state, UObject* object, const char* propertyName,
                        MapView& out, std::string* why)
        {
            UClass* objectClass = object ? EngineDiscovery::GetObjectClass(state, object) : nullptr;
            PropertyRef property;
            if (!objectClass || !FindProperty(state, objectClass, propertyName, property, why))
                return false;
            if (property.TypeName.find("MapProperty") == std::string::npos)
            {
                if (why)
                    *why = "'" + std::string(propertyName) + "' is a " + property.TypeName +
                           ", not a MapProperty";
                return false;
            }
            if (property.Offset < 0) return false;
            return OpenMap(state, property, reinterpret_cast<uint8_t*>(object) + property.Offset,
                           out, why);
        }

        bool OpenSet(EngineState& state, const PropertyRef& property, uint8_t* header,
                     SetView& out, std::string* why)
        {
            if (!property.Property || !header)
            {
                if (why) *why = "the set property has no usable address";
                return false;
            }

            void* elementField = nullptr;
            if (state.FSetProperty_ElementPropOff < 0 ||
                !SafeMem_Read(reinterpret_cast<uint8_t*>(property.Property) +
                                  state.FSetProperty_ElementPropOff,
                              &elementField, sizeof(elementField)) ||
                !elementField)
            {
                if (why) *why = "the set has no element property";
                return false;
            }

            out.Container = property;
            out.Element = MakeElementRef(state, elementField);
            const int32_t elementSize =
                out.Element.ElementSize > 0 ? out.Element.ElementSize : 8;

            StructBuilder slot;
            slot.Add(elementSize,
                     EngineDiscovery::GetPropertyAlignment(state, elementField, elementSize));
            slot.Add(static_cast<int32_t>(sizeof(int32_t)),
                     static_cast<int32_t>(sizeof(int32_t)));
            slot.Add(static_cast<int32_t>(sizeof(int32_t)),
                     static_cast<int32_t>(sizeof(int32_t)));
            out.Stride = SparseSlotStride(slot.Size(), slot.Alignment);
            if (out.Stride <= 0 || out.Stride > 0x100000)
            {
                if (why) *why = "the set slot layout is implausible";
                return false;
            }

            if (!SafeMem_Read(header, &out.Data, sizeof(out.Data)) ||
                !SafeMem_Read(header + 8, &out.Slots, sizeof(out.Slots)))
            {
                if (why) *why = "the set header is not readable";
                return false;
            }
            if (out.Slots < 0 || out.Slots > kMaxElements)
            {
                if (why) *why = "the set slot count is implausible";
                return false;
            }

            out.Allocated = ReadSparseSlots(state, header, out.Slots);
            out.Count = out.Allocated.Valid ? out.Allocated.Allocated : out.Slots;
            return true;
        }

        bool ResolveSet(EngineState& state, UObject* object, const char* propertyName,
                        SetView& out, std::string* why)
        {
            UClass* objectClass = object ? EngineDiscovery::GetObjectClass(state, object) : nullptr;
            PropertyRef property;
            if (!objectClass || !FindProperty(state, objectClass, propertyName, property, why))
                return false;
            if (property.TypeName.find("SetProperty") == std::string::npos)
            {
                if (why)
                    *why = "'" + std::string(propertyName) + "' is a " + property.TypeName +
                           ", not a SetProperty";
                return false;
            }
            if (property.Offset < 0) return false;
            return OpenSet(state, property, reinterpret_cast<uint8_t*>(object) + property.Offset,
                           out, why);
        }

        bool FindSlot(const SparseSlots& slots, int32_t index, int32_t slotCount,
                      int32_t& slotOut)
        {
            if (index < 0) return false;
            int32_t seen = 0;
            for (int32_t slot = 0; slot < slotCount; ++slot)
            {
                if (!slots.IsAllocated(slot)) continue;
                if (seen == index)
                {
                    slotOut = slot;
                    return true;
                }
                ++seen;
            }
            return false;
        }

        constexpr uint8_t kExLocalVariable = 0x00;
        constexpr uint8_t kExInstanceVariable = 0x01;
        constexpr uint8_t kExEndFunctionParms = 0x16;
        constexpr int32_t kFrameBufferBytes = 0x100;
        constexpr size_t kPropertyCloneBytes = 0x100;
        constexpr uint32_t kFuncNative = 0x00000400;

        struct EngineStringHeader
        {
            const void* Data;
            int32_t Num;
            int32_t Max;
        };
        static_assert(sizeof(EngineStringHeader) == 0x10,
                      "FString header must stay 16 bytes");

        struct ThunkArgument
        {
            PropertyRef Property;
            const AvaloniumValue* Value = nullptr;
            int32_t Offset = 0;
            std::vector<uint8_t> Storage;
            std::vector<uint8_t> Clone;
        };

        bool IsStringProperty(const PropertyRef& property)
        {
            return property.TypeName.find("StrProperty") != std::string::npos;
        }

        bool FillThunkArgument(EngineState& state, ThunkArgument& argument, uint8_t* locals,
                               std::string* why)
        {
            const PropertyRef& property = argument.Property;
            const AvaloniumValue& value = *argument.Value;
            uint8_t* destination = locals + argument.Offset;

            if (IsStringProperty(property))
            {
                if (value.kind == AVALONIUM_VALUE_STRUCT && value.pointer &&
                    value.size == property.ElementSize)
                {
                    if (!SafeMem_Write(destination, value.pointer,
                                       static_cast<size_t>(value.size)))
                    {
                        if (why) *why = "the engine string copy is not readable";
                        return false;
                    }
                    return true;
                }
                if (value.kind != AVALONIUM_VALUE_STRING &&
                    value.kind != AVALONIUM_VALUE_NAME)
                {
                    if (why) *why = "a string argument needs text or an engine string copy";
                    return false;
                }

                const std::string text = value.string ? value.string : "";
                EngineStringHeader header{};
                const bool wide = property.TypeName.find("Utf8StrProperty") == std::string::npos &&
                                  property.TypeName.find("AnsiStrProperty") == std::string::npos;
                if (wide)
                {
                    const int length =
                        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
                    if (length <= 0)
                    {
                        if (why) *why = "the string argument is not valid UTF-8";
                        return false;
                    }
                    argument.Storage.resize(static_cast<size_t>(length) * sizeof(wchar_t));
                    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1,
                                        reinterpret_cast<wchar_t*>(argument.Storage.data()),
                                        length);
                    header.Data = argument.Storage.data();
                    header.Num = length;
                    header.Max = length;
                }
                else if (property.TypeName.find("Utf8StrProperty") != std::string::npos)
                {
                    const size_t bytes = text.size() + 1;
                    argument.Storage.assign(text.begin(), text.end());
                    argument.Storage.push_back(0);
                    header.Data = argument.Storage.data();
                    header.Num = static_cast<int32_t>(bytes);
                    header.Max = static_cast<int32_t>(bytes);
                }
                else
                {
                    const int wlength =
                        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
                    if (wlength <= 0)
                    {
                        if (why) *why = "the string argument is not valid UTF-8";
                        return false;
                    }
                    std::vector<wchar_t> wideText(static_cast<size_t>(wlength), 0);
                    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wideText.data(),
                                        wlength);
                    const int length = WideCharToMultiByte(CP_ACP, 0, wideText.data(), -1,
                                                           nullptr, 0, nullptr, nullptr);
                    if (length <= 0)
                    {
                        if (why) *why = "the string argument is not valid ANSI text";
                        return false;
                    }
                    argument.Storage.resize(static_cast<size_t>(length));
                    WideCharToMultiByte(CP_ACP, 0, wideText.data(), -1,
                                        reinterpret_cast<char*>(argument.Storage.data()),
                                        length, nullptr, nullptr);
                    header.Data = argument.Storage.data();
                    header.Num = length;
                    header.Max = length;
                }

                if (!SafeMem_Write(destination, &header, sizeof(header)))
                {
                    if (why) *why = "the string argument could not be placed in the frame";
                    return false;
                }
                return true;
            }

            return WriteValueAt(state, property, destination, value);
        }

        bool ClonePropertyForOffset(EngineState& state, void* property, int32_t offset,
                                    std::vector<uint8_t>& clone, std::string* why)
        {
            if (!property || state.FProperty_OffsetOff < 0 ||
                state.FProperty_OffsetOff + static_cast<int32_t>(sizeof(int32_t)) >
                    static_cast<int32_t>(kPropertyCloneBytes))
            {
                if (why) *why = "the property layout is not resolved";
                return false;
            }
            clone.assign(kPropertyCloneBytes, 0);
            if (SafeMem_ReadSpan(property, clone.data(), clone.size()) != clone.size())
            {
                if (why) *why = "the property could not be copied";
                return false;
            }
            std::memcpy(clone.data() + state.FProperty_OffsetOff, &offset,
                        sizeof(offset));
            return true;
        }

        void AppendScriptPointer(std::vector<uint8_t>& code, const void* pointer)
        {
            const size_t at = code.size();
            code.resize(at + sizeof(pointer));
            std::memcpy(code.data() + at, &pointer, sizeof(pointer));
        }

        struct LibraryFunction
        {
            UFunction* Function = nullptr;
            void* DefaultObject = nullptr;
        };

        bool ResolveLibraryFunction(EngineState& state, const char* className,
                                    const char* functionName, LibraryFunction& out,
                                    std::string* why)
        {
            UClass* libraryClass = FindClass(state, className);
            if (!libraryClass)
            {
                if (why) *why = std::string("the engine class '") + className + "' was not found";
                return false;
            }
            UObject* defaultObject = GetClassDefaultObject(state, libraryClass);
            if (!defaultObject)
            {
                if (why) *why = std::string("'") + className + "' has no default object";
                return false;
            }
            UFunction* function = FindFunction(state, libraryClass, functionName);
            if (!function)
            {
                if (why)
                    *why = std::string("'") + functionName + "' is not a function of '" +
                           className + "'";
                return false;
            }
            if ((GetFunctionFlags(state, function) & kFuncNative) == 0 ||
                state.UFunction_FuncOff < 0)
            {
                if (why)
                    *why = std::string("'") + functionName + "' is not a native function";
                return false;
            }
            void* exec = nullptr;
            if (!SafeMem_Read(reinterpret_cast<uint8_t*>(function) + state.UFunction_FuncOff,
                              &exec, sizeof(exec)) ||
                !exec)
            {
                if (why)
                    *why = std::string("'") + functionName + "' has no native entry";
                return false;
            }
            out.Function = function;
            out.DefaultObject = defaultObject;
            return true;
        }

        bool InvokeThunkRaw(void* exec, void* context, void* frame, void* result)
        {
            __try
            {
                typedef void(__fastcall* ThunkFn)(void*, void*, void*);
                reinterpret_cast<ThunkFn>(exec)(context, frame, result);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        bool CallContainerThunk(EngineState& state, const char* className,
                                const char* functionName, void* context,
                                const PropertyRef& container, ThunkArgument* arguments,
                                int32_t argumentCount, bool* resultBool,
                                std::string* why)
        {
            LibraryFunction library;
            if (!ResolveLibraryFunction(state, className, functionName, library, why))
                return false;
            if (!state.GNatives)
            {
                if (why) *why = "the VM dispatch table is not resolved";
                return false;
            }

            int32_t localsSize = 8;
            for (int32_t i = 0; i < argumentCount; ++i)
            {
                ThunkArgument& argument = arguments[i];
                const int32_t size = argument.Property.ElementSize > 0
                                         ? argument.Property.ElementSize
                                         : 8;
                const int32_t end = argument.Offset + size;
                if (end > localsSize) localsSize = end;
            }
            std::vector<uint8_t> locals(static_cast<size_t>(localsSize), 0);

            for (int32_t i = 0; i < argumentCount; ++i)
            {
                if (!FillThunkArgument(state, arguments[i], locals.data(), why))
                    return false;
                if (!ClonePropertyForOffset(state, arguments[i].Property.Property,
                                            arguments[i].Offset, arguments[i].Clone, why))
                {
                    return false;
                }
            }

            std::vector<uint8_t> code;
            code.push_back(kExInstanceVariable);
            AppendScriptPointer(code, container.Property);
            for (int32_t i = 0; i < argumentCount; ++i)
            {
                code.push_back(kExLocalVariable);
                AppendScriptPointer(code, arguments[i].Clone.data());
            }
            code.push_back(kExEndFunctionParms);

            std::vector<uint8_t> frame(static_cast<size_t>(kFrameBufferBytes), 0);
            auto store = [&frame](int32_t offset, const void* value) {
                std::memcpy(frame.data() + offset, &value, sizeof(value));
            };
            store(FFrame_NodeOff, library.Function);
            store(FFrame_ObjectOff, context);
            store(FFrame_CodeOff, code.data());
            store(FFrame_LocalsOff, locals.data());

            void* exec = nullptr;
            if (!SafeMem_Read(reinterpret_cast<uint8_t*>(library.Function) +
                                  state.UFunction_FuncOff,
                              &exec, sizeof(exec)) ||
                !exec)
            {
                if (why) *why = "the engine function entry is not readable";
                return false;
            }

            if (!InvokeThunkRaw(exec, context, frame.data(), resultBool))
            {
                if (why) *why = "the engine container entry faulted";
                return false;
            }
            return true;
        }

        bool ValuesEqual(EngineState& state, const PropertyRef& property,
                         const AvaloniumValue& a, const AvaloniumValue& b)
        {
            const bool aNumeric = a.kind == AVALONIUM_VALUE_BOOL ||
                                  (a.kind >= AVALONIUM_VALUE_INT8 &&
                                   a.kind <= AVALONIUM_VALUE_UINT64) ||
                                  a.kind == AVALONIUM_VALUE_FLOAT ||
                                  a.kind == AVALONIUM_VALUE_DOUBLE;
            const bool bNumeric = b.kind == AVALONIUM_VALUE_BOOL ||
                                  (b.kind >= AVALONIUM_VALUE_INT8 &&
                                   b.kind <= AVALONIUM_VALUE_UINT64) ||
                                  b.kind == AVALONIUM_VALUE_FLOAT ||
                                  b.kind == AVALONIUM_VALUE_DOUBLE;
            if (aNumeric && bNumeric)
            {
                if (a.kind == AVALONIUM_VALUE_FLOAT || a.kind == AVALONIUM_VALUE_DOUBLE ||
                    b.kind == AVALONIUM_VALUE_FLOAT || b.kind == AVALONIUM_VALUE_DOUBLE)
                {
                    return a.float_value == b.float_value;
                }
                return a.int_value == b.int_value;
            }

            if (a.kind == AVALONIUM_VALUE_OBJECT || b.kind == AVALONIUM_VALUE_OBJECT)
                return a.kind == b.kind && a.pointer == b.pointer;

            if ((a.kind == AVALONIUM_VALUE_STRING || a.kind == AVALONIUM_VALUE_NAME) &&
                (b.kind == AVALONIUM_VALUE_STRING || b.kind == AVALONIUM_VALUE_NAME))
            {
                const char* left = a.string ? a.string : "";
                const char* right = b.string ? b.string : "";
                if (_stricmp(left, right) != 0) return false;
                if (a.kind == AVALONIUM_VALUE_NAME && b.kind == AVALONIUM_VALUE_NAME)
                    return a.name_number == b.name_number;
                return true;
            }

            if (a.kind == AVALONIUM_VALUE_STRUCT && b.kind == AVALONIUM_VALUE_STRUCT)
            {
                if (a.size != b.size || a.size <= 0 || !a.pointer || !b.pointer)
                    return false;
                std::vector<uint8_t> left(static_cast<size_t>(a.size));
                std::vector<uint8_t> right(static_cast<size_t>(b.size));
                if (SafeMem_ReadSpan(a.pointer, left.data(), left.size()) != left.size() ||
                    SafeMem_ReadSpan(b.pointer, right.data(), right.size()) != right.size())
                {
                    return false;
                }
                (void)property;
                return std::memcmp(left.data(), right.data(), left.size()) == 0;
            }

            return false;
        }

        bool ReadMapEntry(EngineState& state, MapView& view, int32_t index, int32_t& slotOut,
                          AvaloniumValue* keyOut, AvaloniumValue* valueOut)
        {
            if (index < 0 || index >= view.Count) return false;
            if (!FindSlot(view.Allocated, index, view.Slots, slotOut) || !view.Data)
                return false;
            uint8_t* pair = view.Data + static_cast<size_t>(slotOut) *
                                           static_cast<size_t>(view.Stride);
            if (!SafeMem_IsReadable(pair, static_cast<size_t>(view.Stride))) return false;
            if (keyOut && !ReadValueAt(state, view.Key, pair, *keyOut)) return false;
            if (valueOut &&
                !ReadValueAt(state, view.Value, pair + view.ValueOffset, *valueOut))
            {
                return false;
            }
            return true;
        }

    }

    int32_t ArrayLength(EngineState& state, UObject* object, const char* propertyName)
    {
        ArrayView view;
        if (!ResolveArray(state, object, propertyName, view, nullptr)) return -1;
        return view.Num;
    }

    int32_t ArrayElementSize(EngineState& state, UObject* object, const char* propertyName)
    {
        ArrayView view;
        if (!ResolveArray(state, object, propertyName, view, nullptr)) return 0;
        return view.ElementSize;
    }

    bool ArrayGet(EngineState& state, UObject* object, const char* propertyName, int32_t index,
                  AvaloniumValue& out, std::string* why)
    {
        SetNoneValue(out);

        ArrayView view;
        if (!ResolveArray(state, object, propertyName, view, why)) return false;
        if (index < 0 || index >= view.Num)
        {
            if (why) *why = "the array index is out of range";
            return false;
        }

        uint8_t* element = view.Data + static_cast<size_t>(index) *
                                          static_cast<size_t>(view.ElementSize);
        if (!ReadValueAt(state, view.Element, element, out))
        {
            if (why) *why = "the element type has no reader";
            return false;
        }
        StabilizeStructValue(out);
        return true;
    }

    bool ArraySet(EngineState& state, UObject* object, const char* propertyName, int32_t index,
                  const AvaloniumValue& value, std::string* why)
    {
        ArrayView view;
        if (!ResolveArray(state, object, propertyName, view, why)) return false;
        if (index < 0 || index >= view.Num)
        {
            if (why) *why = "the array index is out of range";
            return false;
        }

        uint8_t* element = view.Data + static_cast<size_t>(index) *
                                          static_cast<size_t>(view.ElementSize);
        if (!WriteValueAt(state, view.Element, element, value))
        {
            if (why) *why = "the element type rejected the value";
            return false;
        }
        return true;
    }

    bool ArrayAdd(EngineState& state, UObject* object, const char* propertyName,
                  const AvaloniumValue& value, std::string* why)
    {
        ArrayView view;
        if (!ResolveArray(state, object, propertyName, view, why)) return false;

        if (view.Num < view.Max && view.Data)
        {
            uint8_t* element = view.Data + static_cast<size_t>(view.Num) *
                                              static_cast<size_t>(view.ElementSize);
            if (!WriteValueAt(state, view.Element, element, value))
            {
                if (why) *why = "the element type rejected the value";
                return false;
            }
            const int32_t next = view.Num + 1;
            if (!SafeMem_Write(view.Header + 8, &next, sizeof(next)))
            {
                if (why) *why = "the array count could not be written";
                return false;
            }
            return true;
        }

        int32_t newMax = view.Max > 0 ? view.Max * 2 : 4;
        if (newMax <= view.Num) newMax = view.Num + 1;
        if (newMax > kMaxElements)
        {
            if (why) *why = "the array would exceed the supported element count";
            return false;
        }

        const size_t bytes = static_cast<size_t>(newMax) *
                             static_cast<size_t>(view.ElementSize);
        uint8_t* buffer = static_cast<uint8_t*>(AllocateEngineBuffer(state, bytes));
        if (!buffer)
        {
            if (why) *why = "the engine allocator is not resolved";
            return false;
        }
        std::memset(buffer, 0, bytes);

        if (view.Num > 0)
        {
            const size_t existing = static_cast<size_t>(view.Num) *
                                    static_cast<size_t>(view.ElementSize);
            if (SafeMem_ReadSpan(view.Data, buffer, existing) != existing)
            {
                if (why) *why = "the existing elements could not be copied";
                return false;
            }
        }

        if (!WriteValueAt(state, view.Element,
                          buffer + static_cast<size_t>(view.Num) *
                                       static_cast<size_t>(view.ElementSize),
                          value))
        {
            if (why) *why = "the element type rejected the value";
            return false;
        }

        const int32_t next = view.Num + 1;
        if (!SafeMem_Write(view.Header, &buffer, sizeof(buffer)) ||
            !SafeMem_Write(view.Header + 8, &next, sizeof(next)) ||
            !SafeMem_Write(view.Header + 12, &newMax, sizeof(newMax)))
        {
            if (why) *why = "the grown array could not be published";
            return false;
        }
        return true;
    }

    bool ArrayRemove(EngineState& state, UObject* object, const char* propertyName, int32_t index,
                     std::string* why)
    {
        ArrayView view;
        if (!ResolveArray(state, object, propertyName, view, why)) return false;
        if (index < 0 || index >= view.Num)
        {
            if (why) *why = "the array index is out of range";
            return false;
        }

        const size_t tail = static_cast<size_t>(view.Num - index - 1) *
                            static_cast<size_t>(view.ElementSize);
        if (tail > 0)
        {
            if (tail > kMaxMoveBytes)
            {
                if (why) *why = "the array is too large to shift";
                return false;
            }
            std::vector<uint8_t> moved(tail);
            uint8_t* source = view.Data + static_cast<size_t>(index + 1) *
                                           static_cast<size_t>(view.ElementSize);
            if (SafeMem_ReadSpan(source, moved.data(), tail) != tail ||
                !SafeMem_Write(view.Data + static_cast<size_t>(index) *
                                               static_cast<size_t>(view.ElementSize),
                               moved.data(), tail))
            {
                if (why) *why = "the array tail could not be moved";
                return false;
            }
        }

        const int32_t next = view.Num - 1;
        if (!SafeMem_Write(view.Header + 8, &next, sizeof(next)))
        {
            if (why) *why = "the array count could not be written";
            return false;
        }
        return true;
    }

    int32_t MapLength(EngineState& state, UObject* object, const char* propertyName)
    {
        MapView view;
        if (!ResolveMap(state, object, propertyName, view, nullptr)) return -1;
        return view.Count;
    }

    bool MapGetAt(EngineState& state, UObject* object, const char* propertyName, int32_t index,
                  AvaloniumValue& outKey, AvaloniumValue& outValue, std::string* why)
    {
        SetNoneValue(outKey);
        SetNoneValue(outValue);

        MapView view;
        if (!ResolveMap(state, object, propertyName, view, why)) return false;
        if (index < 0 || index >= view.Count)
        {
            if (why) *why = "the map entry index is out of range";
            return false;
        }

        int32_t slot = -1;
        if (!FindSlot(view.Allocated, index, view.Slots, slot) || !view.Data)
        {
            if (why) *why = "the map entry slot could not be resolved";
            return false;
        }

        uint8_t* pair = view.Data + static_cast<size_t>(slot) * static_cast<size_t>(view.Stride);
        if (!SafeMem_IsReadable(pair, static_cast<size_t>(view.Stride)))
        {
            if (why) *why = "the map entry is not readable";
            return false;
        }

        if (!ReadValueAt(state, view.Key, pair, outKey) ||
            !ReadValueAt(state, view.Value, pair + view.ValueOffset, outValue))
        {
            if (why) *why = "the map key or value type has no reader";
            return false;
        }
        return true;
    }

    bool MapSetValueAt(EngineState& state, UObject* object, const char* propertyName,
                       int32_t index, const AvaloniumValue& value, std::string* why)
    {
        MapView view;
        if (!ResolveMap(state, object, propertyName, view, why)) return false;
        if (index < 0 || index >= view.Count)
        {
            if (why) *why = "the map entry index is out of range";
            return false;
        }

        int32_t slot = -1;
        if (!FindSlot(view.Allocated, index, view.Slots, slot) || !view.Data)
        {
            if (why) *why = "the map entry slot could not be resolved";
            return false;
        }

        uint8_t* pair = view.Data + static_cast<size_t>(slot) * static_cast<size_t>(view.Stride);
        if (!SafeMem_IsReadable(pair, static_cast<size_t>(view.Stride)))
        {
            if (why) *why = "the map entry is not readable";
            return false;
        }
        if (!WriteValueAt(state, view.Value, pair + view.ValueOffset, value))
        {
            if (why) *why = "the map value type rejected the value";
            return false;
        }
        return true;
    }

    int32_t SetLength(EngineState& state, UObject* object, const char* propertyName)
    {
        SetView view;
        if (!ResolveSet(state, object, propertyName, view, nullptr)) return -1;
        return view.Count;
    }

    bool SetGetAt(EngineState& state, UObject* object, const char* propertyName, int32_t index,
                  AvaloniumValue& out, std::string* why)
    {
        SetNoneValue(out);

        SetView view;
        if (!ResolveSet(state, object, propertyName, view, why)) return false;
        if (index < 0 || index >= view.Count)
        {
            if (why) *why = "the set element index is out of range";
            return false;
        }

        int32_t slot = -1;
        if (!FindSlot(view.Allocated, index, view.Slots, slot) || !view.Data)
        {
            if (why) *why = "the set element slot could not be resolved";
            return false;
        }

        uint8_t* element =
            view.Data + static_cast<size_t>(slot) * static_cast<size_t>(view.Stride);
        if (!SafeMem_IsReadable(element, static_cast<size_t>(view.Stride)))
        {
            if (why) *why = "the set element is not readable";
            return false;
        }
        if (!ReadValueAt(state, view.Element, element, out))
        {
            if (why) *why = "the set element type has no reader";
            return false;
        }
        return true;
    }

    namespace
    {
        struct PathTarget
        {
            uint8_t* Address = nullptr;
            PropertyRef Property;
        };

        bool ReadPathName(const std::string& path, size_t& pos, std::string& name)
        {
            const size_t start = pos;
            while (pos < path.size() && path[pos] != '.' && path[pos] != '[' &&
                   path[pos] != '{')
            {
                ++pos;
            }
            if (pos == start) return false;
            name = path.substr(start, pos - start);
            return true;
        }

        bool IsPlainObjectReference(const PropertyRef& property)
        {
            const std::string& type = property.TypeName;
            if (type.find("SoftObjectProperty") != std::string::npos ||
                type.find("WeakObjectProperty") != std::string::npos ||
                type.find("LazyObjectProperty") != std::string::npos)
            {
                return false;
            }
            return type.find("ObjectProperty") != std::string::npos ||
                   type.find("ClassProperty") != std::string::npos ||
                   type.find("InterfaceProperty") != std::string::npos;
        }

        bool ParsePathKey(EngineState& state, const PropertyRef& keyProperty,
                          const std::string& text, AvaloniumValue& out, std::string& storage,
                          std::string* why)
        {
            SetNoneValue(out);
            const std::string& type = keyProperty.TypeName;

            if (type.find("StructProperty") != std::string::npos)
            {
                if (why)
                    *why = "struct map keys cannot be written in a property path";
                return false;
            }
            if (type.find("StrProperty") != std::string::npos)
            {
                storage = text;
                out.kind = AVALONIUM_VALUE_STRING;
                out.string = storage.c_str();
                return true;
            }
            if (type.find("NameProperty") != std::string::npos)
            {
                storage = text;
                uint32_t number = 0;
                const size_t suffix = storage.rfind('_');
                if (suffix != std::string::npos && suffix + 1 < storage.size())
                {
                    bool digits = true;
                    for (size_t i = suffix + 1; i < storage.size(); ++i)
                    {
                        if (storage[i] < '0' || storage[i] > '9') { digits = false; break; }
                    }
                    if (digits)
                    {
                        number = static_cast<uint32_t>(std::strtoul(storage.c_str() + suffix + 1,
                                                                   nullptr, 10));
                        storage.resize(suffix);
                    }
                }
                out.kind = AVALONIUM_VALUE_NAME;
                out.string = storage.c_str();
                out.name_number = number;
                return true;
            }
            if (type.find("BoolProperty") != std::string::npos)
            {
                out.kind = AVALONIUM_VALUE_BOOL;
                out.int_value =
                    (_stricmp(text.c_str(), "true") == 0 || text == "1") ? 1 : 0;
                return true;
            }
            if (type.find("FloatProperty") != std::string::npos ||
                type.find("DoubleProperty") != std::string::npos)
            {
                char* end = nullptr;
                const double parsed = std::strtod(text.c_str(), &end);
                if (end == text.c_str() || *end != '\0')
                {
                    if (why) *why = "'" + text + "' is not a number";
                    return false;
                }
                out.kind = AVALONIUM_VALUE_DOUBLE;
                out.float_value = parsed;
                return true;
            }
            if (IsPlainObjectReference(keyProperty))
            {
                void* object = FindObject(state, text);
                if (!object) object = EngineDiscovery::FindObjectByNameFast(state, text);
                if (!object)
                {
                    if (why) *why = "the object '" + text + "' was not found";
                    return false;
                }
                out.kind = AVALONIUM_VALUE_OBJECT;
                out.pointer = object;
                return true;
            }

            const bool hex = text.size() > 2 && text[0] == '0' &&
                             (text[1] == 'x' || text[1] == 'X');
            char* end = nullptr;
            const long long parsed = std::strtoll(text.c_str(), &end, hex ? 16 : 10);
            if (end == text.c_str() || *end != '\0')
            {
                if (why) *why = "'" + text + "' is not a numeric key";
                return false;
            }
            out.kind = AVALONIUM_VALUE_INT64;
            out.int_value = parsed;
            return true;
        }

        bool ResolvePath(EngineState& state, UObject* root, const std::string& path,
                         PathTarget& out, std::string* why)
        {
            out = {};
            if (!root || path.empty())
            {
                if (why) *why = "the object or property path is empty";
                return false;
            }

            UClass* rootClass = EngineDiscovery::GetObjectClass(state, root);
            if (!rootClass)
            {
                if (why) *why = "the object has no class";
                return false;
            }

            size_t pos = 0;
            std::string name;
            if (!ReadPathName(path, pos, name))
            {
                if (why) *why = "the property path starts with a selector";
                return false;
            }

            PropertyRef current;
            if (!FindProperty(state, rootClass, name, current, why)) return false;
            if (current.Offset < 0)
            {
                if (why) *why = "the property has no usable offset";
                return false;
            }
            uint8_t* address = reinterpret_cast<uint8_t*>(root) + current.Offset;

            while (pos < path.size())
            {
                const char marker = path[pos];
                if (marker == '.')
                {
                    ++pos;
                    std::string fieldName;
                    if (!ReadPathName(path, pos, fieldName))
                    {
                        if (why) *why = "the property path ends after '.'";
                        return false;
                    }

                    if (current.TypeName.find("StructProperty") != std::string::npos)
                    {
                        void* structure = nullptr;
                        if (state.FStructProperty_StructOff < 0 ||
                            !SafeMem_Read(reinterpret_cast<uint8_t*>(current.Property) +
                                              state.FStructProperty_StructOff,
                                          &structure, sizeof(structure)) ||
                            !structure)
                        {
                            if (why) *why = "the struct type is not available";
                            return false;
                        }
                        PropertyRef field;
                        if (!FindProperty(state, reinterpret_cast<UClass*>(structure),
                                          fieldName, field, why))
                        {
                            return false;
                        }
                        if (field.Offset < 0)
                        {
                            if (why) *why = "the struct field has no usable offset";
                            return false;
                        }
                        address += field.Offset;
                        current = field;
                    }
                    else if (IsPlainObjectReference(current))
                    {
                        void* referenced = nullptr;
                        if (!SafeMem_Read(address, &referenced, sizeof(referenced)) ||
                            !referenced)
                        {
                            if (why) *why = "the object reference is null";
                            return false;
                        }
                        UClass* referencedClass = EngineDiscovery::GetObjectClass(
                            state, static_cast<UObject*>(referenced));
                        if (!referencedClass)
                        {
                            if (why) *why = "the referenced object has no class";
                            return false;
                        }
                        PropertyRef field;
                        if (!FindProperty(state, referencedClass, fieldName, field, why))
                            return false;
                        if (field.Offset < 0)
                        {
                            if (why) *why = "the object field has no usable offset";
                            return false;
                        }
                        address = reinterpret_cast<uint8_t*>(referenced) + field.Offset;
                        current = field;
                    }
                    else
                    {
                        if (why)
                            *why = "'.' needs a struct or object property before '" +
                                   fieldName + "'";
                        return false;
                    }
                }
                else if (marker == '[')
                {
                    ++pos;
                    const size_t close = path.find(']', pos);
                    if (close == std::string::npos)
                    {
                        if (why) *why = "the array index is not terminated";
                        return false;
                    }
                    const std::string text = path.substr(pos, close - pos);
                    pos = close + 1;

                    if (current.TypeName.find("ArrayProperty") == std::string::npos)
                    {
                        if (why) *why = "'[...]' needs an array property";
                        return false;
                    }
                    ArrayView view;
                    if (!OpenArray(state, current, address, view, why)) return false;

                    char* end = nullptr;
                    const long long index = std::strtoll(text.c_str(), &end, 10);
                    if (end == text.c_str() || *end != '\0' || index < 0 ||
                        index >= view.Num)
                    {
                        if (why) *why = "the array index is out of range";
                        return false;
                    }
                    address = view.Data + static_cast<size_t>(index) *
                                               static_cast<size_t>(view.ElementSize);
                    current = view.Element;
                }
                else if (marker == '{')
                {
                    ++pos;
                    const size_t close = path.find('}', pos);
                    if (close == std::string::npos)
                    {
                        if (why) *why = "the map or set selector is not terminated";
                        return false;
                    }
                    const std::string text = path.substr(pos, close - pos);
                    pos = close + 1;

                    if (current.TypeName.find("MapProperty") != std::string::npos)
                    {
                        MapView view;
                        if (!OpenMap(state, current, address, view, why)) return false;

                        std::string storage;
                        AvaloniumValue key;
                        if (!ParsePathKey(state, view.Key, text, key, storage, why))
                            return false;

                        int32_t slot = -1;
                        for (int32_t i = 0; i < view.Count && slot < 0; ++i)
                        {
                            int32_t entrySlot = -1;
                            AvaloniumValue entryKey;
                            if (!ReadMapEntry(state, view, i, entrySlot, &entryKey, nullptr))
                                continue;
                            if (ValuesEqual(state, view.Key, entryKey, key)) slot = entrySlot;
                        }
                        if (slot < 0)
                        {
                            if (why) *why = "the map has no entry for that key";
                            return false;
                        }
                        address = view.Data + static_cast<size_t>(slot) *
                                                   static_cast<size_t>(view.Stride) +
                                  view.ValueOffset;
                        current = view.Value;
                    }
                    else if (current.TypeName.find("SetProperty") != std::string::npos)
                    {
                        SetView view;
                        if (!OpenSet(state, current, address, view, why)) return false;

                        char* end = nullptr;
                        const long long index = std::strtoll(text.c_str(), &end, 10);
                        if (end == text.c_str() || *end != '\0' || index < 0 ||
                            index >= view.Count)
                        {
                            if (why) *why = "the set element index is out of range";
                            return false;
                        }
                        int32_t slot = -1;
                        if (!FindSlot(view.Allocated, static_cast<int32_t>(index), view.Slots,
                                      slot) ||
                            !view.Data)
                        {
                            if (why) *why = "the set element slot could not be resolved";
                            return false;
                        }
                        address = view.Data + static_cast<size_t>(slot) *
                                                   static_cast<size_t>(view.Stride);
                        current = view.Element;
                    }
                    else
                    {
                        if (why) *why = "'{...}' needs a map or set property";
                        return false;
                    }
                }
                else
                {
                    if (why) *why = "the property path has an unexpected character";
                    return false;
                }
            }

            out.Address = address;
            out.Property = current;
            return true;
        }
    }

    bool MapAdd(EngineState& state, UObject* object, const char* propertyName,
                const AvaloniumValue& key, const AvaloniumValue& value, std::string* why)
    {
        MapView view;
        if (!ResolveMap(state, object, propertyName, view, why)) return false;

        ThunkArgument arguments[2];
        arguments[0].Property = view.Key;
        arguments[0].Value = &key;
        arguments[0].Offset = 0;
        arguments[1].Property = view.Value;
        arguments[1].Value = &value;
        arguments[1].Offset = view.ValueOffset;

        if (!CallContainerThunk(state, "BlueprintMapLibrary", "Map_Add", object,
                                view.Container, arguments, 2, nullptr, why))
        {
            return false;
        }

        std::string checkWhy;
        if (!MapContains(state, object, propertyName, key, &checkWhy))
        {
            if (why)
            {
                *why = checkWhy.empty() ? "the engine did not add the map entry"
                                        : checkWhy;
            }
            return false;
        }
        return true;
    }

    bool MapRemove(EngineState& state, UObject* object, const char* propertyName,
                   const AvaloniumValue& key, std::string* why)
    {
        MapView view;
        if (!ResolveMap(state, object, propertyName, view, why)) return false;

        ThunkArgument argument;
        argument.Property = view.Key;
        argument.Value = &key;
        argument.Offset = 0;

        bool removed = false;
        if (!CallContainerThunk(state, "BlueprintMapLibrary", "Map_Remove", object,
                                view.Container, &argument, 1, &removed, why))
        {
            return false;
        }
        if (!removed && why) *why = "the map has no entry for that key";
        return removed;
    }

    bool MapClear(EngineState& state, UObject* object, const char* propertyName,
                  std::string* why)
    {
        MapView view;
        if (!ResolveMap(state, object, propertyName, view, why)) return false;
        if (!CallContainerThunk(state, "BlueprintMapLibrary", "Map_Clear", object,
                                view.Container, nullptr, 0, nullptr, why))
        {
            return false;
        }

        MapView check;
        if (!ResolveMap(state, object, propertyName, check, why)) return false;
        if (check.Count != 0)
        {
            if (why) *why = "the engine did not clear the map";
            return false;
        }
        return true;
    }

    bool MapContains(EngineState& state, UObject* object, const char* propertyName,
                     const AvaloniumValue& key, std::string* why)
    {
        MapView view;
        if (!ResolveMap(state, object, propertyName, view, why)) return false;

        ThunkArgument argument;
        argument.Property = view.Key;
        argument.Value = &key;
        argument.Offset = 0;

        bool contains = false;
        if (!CallContainerThunk(state, "BlueprintMapLibrary", "Map_Contains", object,
                                view.Container, &argument, 1, &contains, why))
        {
            return false;
        }
        return contains;
    }

    bool MapFind(EngineState& state, UObject* object, const char* propertyName,
                 const AvaloniumValue& key, AvaloniumValue& outValue, std::string* why)
    {
        SetNoneValue(outValue);
        MapView view;
        if (!ResolveMap(state, object, propertyName, view, why)) return false;

        for (int32_t i = 0; i < view.Count; ++i)
        {
            int32_t slot = -1;
            AvaloniumValue entryKey;
            if (!ReadMapEntry(state, view, i, slot, &entryKey, nullptr)) continue;
            if (!ValuesEqual(state, view.Key, entryKey, key)) continue;

            if (!ReadValueAt(state, view.Value,
                             view.Data + static_cast<size_t>(slot) *
                                             static_cast<size_t>(view.Stride) +
                                 view.ValueOffset,
                             outValue))
            {
                if (why) *why = "the map value type has no reader";
                return false;
            }
            return true;
        }

        if (why) *why = "the map has no entry for that key";
        return false;
    }

    bool SetAdd(EngineState& state, UObject* object, const char* propertyName,
                const AvaloniumValue& value, std::string* why)
    {
        SetView view;
        if (!ResolveSet(state, object, propertyName, view, why)) return false;

        ThunkArgument argument;
        argument.Property = view.Element;
        argument.Value = &value;
        argument.Offset = 0;

        if (!CallContainerThunk(state, "BlueprintSetLibrary", "Set_Add", object,
                                view.Container, &argument, 1, nullptr, why))
        {
            return false;
        }

        std::string checkWhy;
        if (!SetContains(state, object, propertyName, value, &checkWhy))
        {
            if (why)
            {
                *why = checkWhy.empty() ? "the engine did not add the set element"
                                        : checkWhy;
            }
            return false;
        }
        return true;
    }

    bool SetRemove(EngineState& state, UObject* object, const char* propertyName,
                   const AvaloniumValue& value, std::string* why)
    {
        SetView view;
        if (!ResolveSet(state, object, propertyName, view, why)) return false;

        ThunkArgument argument;
        argument.Property = view.Element;
        argument.Value = &value;
        argument.Offset = 0;

        bool removed = false;
        if (!CallContainerThunk(state, "BlueprintSetLibrary", "Set_Remove", object,
                                view.Container, &argument, 1, &removed, why))
        {
            return false;
        }
        if (!removed && why) *why = "the set has no such element";
        return removed;
    }

    bool SetClear(EngineState& state, UObject* object, const char* propertyName,
                  std::string* why)
    {
        SetView view;
        if (!ResolveSet(state, object, propertyName, view, why)) return false;
        if (!CallContainerThunk(state, "BlueprintSetLibrary", "Set_Clear", object,
                                view.Container, nullptr, 0, nullptr, why))
        {
            return false;
        }

        SetView check;
        if (!ResolveSet(state, object, propertyName, check, why)) return false;
        if (check.Count != 0)
        {
            if (why) *why = "the engine did not clear the set";
            return false;
        }
        return true;
    }

    bool SetContains(EngineState& state, UObject* object, const char* propertyName,
                     const AvaloniumValue& value, std::string* why)
    {
        SetView view;
        if (!ResolveSet(state, object, propertyName, view, why)) return false;

        ThunkArgument argument;
        argument.Property = view.Element;
        argument.Value = &value;
        argument.Offset = 0;

        bool contains = false;
        if (!CallContainerThunk(state, "BlueprintSetLibrary", "Set_Contains", object,
                                view.Container, &argument, 1, &contains, why))
        {
            return false;
        }
        return contains;
    }

    bool ReadPath(EngineState& state, UObject* object, const char* path,
                  AvaloniumValue& out, std::string* why)
    {
        SetNoneValue(out);
        if (!path || !*path)
        {
            if (why) *why = "the property path is empty";
            return false;
        }

        PathTarget target;
        if (!ResolvePath(state, object, path, target, why)) return false;
        if (!ReadValueAt(state, target.Property, target.Address, out))
        {
            if (why) *why = "the property type has no reader";
            return false;
        }
        return true;
    }

    bool WritePath(EngineState& state, UObject* object, const char* path,
                   const AvaloniumValue& value, std::string* why)
    {
        if (!path || !*path)
        {
            if (why) *why = "the property path is empty";
            return false;
        }

        PathTarget target;
        if (!ResolvePath(state, object, path, target, why)) return false;
        if (!WriteValueAt(state, target.Property, target.Address, value))
        {
            if (why) *why = "the property type rejected the value";
            return false;
        }
        return true;
    }
}
