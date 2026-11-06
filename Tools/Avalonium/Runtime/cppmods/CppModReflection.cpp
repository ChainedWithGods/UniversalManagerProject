#include "CppModReflection.h"

#include "../console/ConsoleInternal.h"
#include "../core/BlueprintDiscovery.h"
#include "../core/EngineDiscovery.h"
#include "../core/NameResolution.h"
#include "../core/ObjectIndex.h"
#include "../core/ObjectUtils.h"
#include "../lua/LuaUObject.h"
#include "../utils/SafeMemory.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Avalonium::CppMods
{
    namespace
    {
        bool SameText(const std::string& a, const std::string& b)
        {
            return _stricmp(a.c_str(), b.c_str()) == 0;
        }

        std::string Lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        std::string WithoutBlueprintSuffix(std::string name)
        {
            if (name.size() > 2 && name.compare(name.size() - 2, 2, "_C") == 0)
                name.resize(name.size() - 2);
            return name;
        }

        bool IsClassObject(const EngineState& state, UObject* object)
        {
            if (!object || !state.UClassClass) return false;
            if (EngineDiscovery::GetObjectClass(state, object) != state.UClassClass) return false;

            uint32_t flags = 0;
            if (state.UObject_FlagsOff >= 0 &&
                SafeMem_Read(reinterpret_cast<uint8_t*>(object) + state.UObject_FlagsOff,
                             &flags, sizeof(flags)))
            {
                if ((flags & kRfObjectTemplateMask) != 0) return false;
                return true;
            }

            const std::string name = EngineDiscovery::GetObjectNameStr(state, object);
            return name.rfind("Default__", 0) != 0;
        }

        PropertyRef PropertyFromFound(const LuaEngine::FoundPropertyEx& found)
        {
            PropertyRef property;
            property.Property = found.Prop;
            property.TypeName = found.TypeName;
            property.Offset = found.Offset;
            property.ElementSize = found.ElementSize;
            property.Flags = found.Flags;
            property.BoolByteOffset = found.ByteOffset;
            property.BoolByteMask = found.ByteMask;
            property.BoolFieldMask = found.FieldMask;
            return property;
        }

        bool FindFunctionMember(EngineState& state, void* function, const std::string& name,
                                PropertyRef& out)
        {
            if (!function || name.empty()) return false;

            std::vector<EngineDiscovery::ReflectedMember> members;
            if (!EngineDiscovery::GetReflectedMembers(state,
                                                      static_cast<UStruct*>(function), members))
                return false;

            for (const EngineDiscovery::ReflectedMember& member : members)
            {
                if (SameText(member.Name, name))
                {
                    out = MakePropertyRef(state, member);
                    return true;
                }
            }
            return false;
        }

        bool FindReturnMember(EngineState& state, void* function, PropertyRef& out)
        {
            if (!function) return false;

            std::vector<EngineDiscovery::ReflectedMember> members;
            if (!EngineDiscovery::GetReflectedMembers(state,
                                                      static_cast<UStruct*>(function), members))
                return false;

            const int32_t returnOffset = EngineDiscovery::GetFunctionReturnValueOffset(state, function);
            for (const EngineDiscovery::ReflectedMember& member : members)
            {
                if ((member.Flags & kCpfReturnParm) != 0 ||
                    SameText(member.Name, "ReturnValue") ||
                    (returnOffset >= 0 && member.Offset == returnOffset))
                {
                    out = MakePropertyRef(state, member);
                    return true;
                }
            }
            return false;
        }

        uint8_t* FrameLocals(void* frame)
        {
            if (!frame) return nullptr;
            void* locals = nullptr;

            if (!SafeMem_Read(reinterpret_cast<uint8_t*>(frame) + FFrame_LocalsOff,
                              &locals, sizeof(locals)))
                return nullptr;
            return static_cast<uint8_t*>(locals);
        }

        bool IsGameThreadCallable(EngineState& state, std::string* why)
        {
            if (!state.bInitialized)
            {
                if (why) *why = "the engine is not initialized";
                return false;
            }
            return true;
        }
    }

    UObject* FindObject(EngineState& state, const std::string& path)
    {
        if (path.empty()) return nullptr;

        UObject* object = EngineDiscovery::FindObjectByPath(state, path);
        if (object) return object;

        if (path.find('/') == std::string::npos && path.find('.') == std::string::npos)
        {
            object = EngineDiscovery::FindObjectByNameFast(state, path);
            if (object) return object;

            const std::vector<UObject*> matches =
                EngineDiscovery::FindObjectsByNameFast(state, path);
            if (!matches.empty()) return matches.front();
        }
        return nullptr;
    }

    UClass* FindClass(EngineState& state, const std::string& name)
    {
        if (name.empty() || !state.UClassClass) return nullptr;

        if (name.find('/') != std::string::npos || name.find('.') != std::string::npos)
        {
            UObject* object = EngineDiscovery::FindObjectByPath(state, name);
            if (object && IsClassObject(state, object)) return static_cast<UClass*>(object);
        }

        auto tryName = [&](const std::string& candidate) -> UClass* {
            if (candidate.empty()) return nullptr;
            const std::vector<UObject*> matches =
                EngineDiscovery::FindObjectsByNameFast(state, candidate);
            for (UObject* object : matches)
            {
                if (IsClassObject(state, object)) return static_cast<UClass*>(object);
            }
            return nullptr;
        };

        if (UClass* resolved = tryName(name)) return resolved;

        const std::string bare = WithoutBlueprintSuffix(name);
        if (bare != name)
        {
            if (UClass* resolved = tryName(bare)) return resolved;
        }
        if (UClass* resolved = tryName(bare + "_C")) return resolved;

        const std::vector<UObject*> classes =
            EngineDiscovery::FindObjectsByClassFast(state, state.UClassClass);
        for (UObject* object : classes)
        {
            if (!IsClassObject(state, object)) continue;
            const std::string candidate = EngineDiscovery::GetObjectNameStr(state, object);
            if (SameText(candidate, name) ||
                SameText(WithoutBlueprintSuffix(candidate), bare))
            {
                return static_cast<UClass*>(object);
            }
        }

        return nullptr;
    }

    UObject* FindFirstObjectOfClass(EngineState& state, UClass* objectClass)
    {
        if (!objectClass) return nullptr;

        UObject* object = ConsoleEnabler::FindFirstObjectOfClass(state, objectClass, false);
        if (object) return object;

        std::vector<UObject*> instances =
            EngineDiscovery::FindObjectsByClassFast(state, objectClass);
        for (UObject* candidate : instances)
        {
            if (!candidate) continue;
            const std::string name = EngineDiscovery::GetObjectNameStr(state, candidate);
            if (name.rfind("Default__", 0) == 0) continue;
            return candidate;
        }
        return nullptr;
    }

    UObject* GetClassDefaultObject(EngineState& state, UClass* objectClass)
    {
        if (!objectClass) return nullptr;
        return EngineDiscovery::FindClassDefaultObject(state, objectClass);
    }

    int32_t FindObjectsOfClass(EngineState& state, UClass* objectClass,
                               std::vector<UObject*>& out, int32_t maxObjects)
    {
        out.clear();
        if (!objectClass || maxObjects <= 0) return 0;

        std::vector<UObject*> instances =
            EngineDiscovery::FindObjectsByClassFast(state, objectClass);
        const int32_t count = static_cast<int32_t>(
            (std::min)(static_cast<size_t>(maxObjects), instances.size()));
        out.reserve(static_cast<size_t>(count));
        for (int32_t i = 0; i < count; ++i)
            out.push_back(instances[static_cast<size_t>(i)]);
        return count;
    }

    UFunction* FindFunction(EngineState& state, UClass* objectClass,
                            const std::string& functionName)
    {
        if (!objectClass || functionName.empty()) return nullptr;

        UFunction* function = static_cast<UFunction*>(
            LuaEngine::FindFunctionOnClassEx(objectClass, functionName));
        if (!function) return nullptr;

        UObject* outer = EngineDiscovery::GetObjectOuter(state,
                                                         static_cast<UObject*>(function));
        for (UStruct* current = static_cast<UStruct*>(objectClass); current;
             current = EngineDiscovery::GetSuperStruct(state, current))
        {
            if (static_cast<UObject*>(current) == outer) return function;
        }
        return nullptr;
    }

    bool FindProperty(EngineState& state, UClass* objectClass, const std::string& propertyName,
                      PropertyRef& out, std::string* why)
    {
        if (!objectClass)
        {
            if (why) *why = "the object has no class";
            return false;
        }
        if (propertyName.empty())
        {
            if (why) *why = "the property name is empty";
            return false;
        }

        const LuaEngine::FoundPropertyEx found =
            LuaEngine::FindPropertyOnClassEx(objectClass, propertyName);
        if (!found.Found || found.Offset < 0)
        {
            if (why) *why = "property '" + propertyName + "' was not found on the class chain";
            return false;
        }

        out = PropertyFromFound(found);
        out.Name = propertyName;
        return true;
    }

    bool ReadProperty(EngineState& state, UObject* object, const std::string& propertyName,
                      AvaloniumValue& out, std::string* why)
    {
        if (!object || !SafeMem_IsReadable(object, 0x20))
        {
            if (why) *why = "the object pointer is not readable";
            return false;
        }

        UClass* objectClass = EngineDiscovery::GetObjectClass(state, object);
        PropertyRef property;
        if (!FindProperty(state, objectClass, propertyName, property, why)) return false;

        if (!ReadValueAt(state, property,
                         reinterpret_cast<uint8_t*>(object) + property.Offset, out))
        {
            if (why) *why = "property '" + propertyName + "' has an unsupported value type '" +
                            property.TypeName + "'";
            return false;
        }
        return true;
    }

    bool WriteProperty(EngineState& state, UObject* object, const std::string& propertyName,
                       const AvaloniumValue& value, std::string* why)
    {
        if (!object || !SafeMem_IsReadable(object, 0x20))
        {
            if (why) *why = "the object pointer is not readable";
            return false;
        }

        UClass* objectClass = EngineDiscovery::GetObjectClass(state, object);
        PropertyRef property;
        if (!FindProperty(state, objectClass, propertyName, property, why)) return false;

        if (!WriteValueAt(state, property,
                          reinterpret_cast<uint8_t*>(object) + property.Offset, value))
        {
            if (why) *why = "property '" + propertyName + "' rejected a value of kind " +
                            std::to_string(value.kind) + " for type '" + property.TypeName + "'";
            return false;
        }
        return true;
    }

    bool ReadFunctionArgument(EngineState& state, void* function, void* frame,
                              const std::string& parameterName, AvaloniumValue& out,
                              std::string* why)
    {
        PropertyRef property;
        if (!FindFunctionMember(state, function, parameterName, property))
        {
            if (why) *why = "parameter '" + parameterName + "' was not found on the function";
            return false;
        }

        uint8_t* locals = FrameLocals(frame);
        if (!locals)
        {
            if (why) *why = "the frame carries no local storage";
            return false;
        }

        if (!ReadValueAt(state, property, locals + property.Offset, out))
        {
            if (why) *why = "parameter '" + parameterName + "' has an unsupported value type";
            return false;
        }
        return true;
    }

    bool WriteFunctionArgument(EngineState& state, void* function, void* frame,
                               const std::string& parameterName, const AvaloniumValue& value,
                               std::string* why)
    {
        PropertyRef property;
        if (!FindFunctionMember(state, function, parameterName, property))
        {
            if (why) *why = "parameter '" + parameterName + "' was not found on the function";
            return false;
        }

        uint8_t* locals = FrameLocals(frame);
        if (!locals)
        {
            if (why) *why = "the frame carries no local storage";
            return false;
        }

        if (!WriteValueAt(state, property, locals + property.Offset, value))
        {
            if (why) *why = "parameter '" + parameterName + "' rejected the value";
            return false;
        }
        return true;
    }

    bool ReadHookResult(EngineState& state, void* function, void* result, AvaloniumValue& out,
                        std::string* why)
    {
        if (!function || !result)
        {
            if (why) *why = "the hook has no function or result storage";
            return false;
        }

        PropertyRef property;
        if (!FindReturnMember(state, function, property))
        {
            if (why) *why = "the function returns nothing";
            SetNoneValue(out);
            return false;
        }

        if (!ReadValueAt(state, property, static_cast<const uint8_t*>(result), out))
        {
            if (why) *why = "the return value has an unsupported type";
            return false;
        }
        return true;
    }

    bool CallFunction(EngineState& state, UObject* object, const std::string& functionName,
                      const AvaloniumValue* args, int32_t argCount, AvaloniumValue& outResult,
                      std::string* why)
    {
        SetNoneValue(outResult);
        if (!IsGameThreadCallable(state, why)) return false;
        if (!object || !SafeMem_IsReadable(object, 0x20))
        {
            if (why) *why = "the object pointer is not readable";
            return false;
        }

        UClass* objectClass = EngineDiscovery::GetObjectClass(state, object);
        UFunction* function = FindFunction(state, objectClass, functionName);
        if (!function)
        {
            if (why) *why = "function '" + functionName + "' was not found on class '" +
                            (objectClass ? EngineDiscovery::GetObjectNameStr(state, objectClass)
                                         : std::string("<none>")) + "'";
            return false;
        }

        std::vector<EngineDiscovery::ReflectedMember> members;
        EngineDiscovery::GetReflectedMembers(state, static_cast<UStruct*>(function), members);

        int32_t frameSize = EngineDiscovery::GetFunctionFrameSize(state, function);
        if (frameSize <= 0) frameSize = static_cast<int32_t>(sizeof(uint64_t));
        if (frameSize > (1 << 20))
        {
            if (why) *why = "the function's reflected frame size is implausible";
            return false;
        }

        std::vector<uint64_t> block(
            static_cast<size_t>((frameSize + static_cast<int32_t>(sizeof(uint64_t)) - 1) /
                                sizeof(uint64_t)) + 1, 0);
        uint8_t* params = reinterpret_cast<uint8_t*>(block.data());

        std::vector<PropertyRef> inParams;
        inParams.reserve(members.size());
        PropertyRef returnProperty;
        bool hasReturn = false;
        const int32_t returnOffset =
            EngineDiscovery::GetFunctionReturnValueOffset(state, function);

        for (const EngineDiscovery::ReflectedMember& member : members)
        {
            const bool isParam = (member.Flags & kCpfParm) != 0;
            const bool isReturn = (member.Flags & kCpfReturnParm) != 0 ||
                                  member.Name == "ReturnValue" ||
                                  (returnOffset >= 0 && member.Offset == returnOffset);
            if (!isParam || isReturn) continue;
            inParams.push_back(MakePropertyRef(state, member));
        }

        const int32_t provided = args ? (std::max)(argCount, 0) : 0;
        const int32_t bound = (std::min)(provided, static_cast<int32_t>(inParams.size()));
        for (int32_t i = 0; i < bound; ++i)
        {
            if (args[i].kind == AVALONIUM_VALUE_NONE) continue;
            const PropertyRef& property = inParams[static_cast<size_t>(i)];
            if (!WriteValueAt(state, property, params + property.Offset, args[i]))
            {
                if (why) *why = "argument " + std::to_string(i) + " ('" + property.Name +
                                "', " + property.TypeName + ") rejected the value";
                return false;
            }
        }

        if (!EngineDiscovery::CallProcessEvent(state, object, function, params))
        {
            if (why) *why = "the engine refused the reflected call";
            return false;
        }

        hasReturn = FindReturnMember(state, function, returnProperty);
        if (hasReturn && returnProperty.Offset >= 0)
        {
            ReadValueAt(state, returnProperty, params + returnProperty.Offset, outResult);
            StabilizeStructValue(outResult);
        }

        return true;
    }

    bool ProcessEvent(EngineState& state, UObject* object, UFunction* function, void* params)
    {
        if (!IsGameThreadCallable(state, nullptr)) return false;
        if (!object || !function) return false;
        return EngineDiscovery::CallProcessEvent(state, object, function, params);
    }

    int32_t GetClassProperties(EngineState& state, UClass* objectClass,
                               AvaloniumPropertyInfo* outProperties, int32_t maxProperties)
    {
        if (!objectClass) return 0;

        std::vector<PropertyRef> collected;
        std::unordered_set<std::string> seen;

        for (UStruct* current = static_cast<UStruct*>(objectClass); current;
             current = EngineDiscovery::GetSuperStruct(state, current))
        {
            std::vector<EngineDiscovery::ReflectedMember> members;
            if (!EngineDiscovery::GetReflectedMembers(state, current, members)) continue;

            for (const EngineDiscovery::ReflectedMember& member : members)
            {
                if (member.Name.empty()) continue;
                const std::string key = Lower(member.Name);
                if (!seen.insert(key).second) continue;
                collected.push_back(MakePropertyRef(state, member));
            }
        }

        if (!outProperties || maxProperties <= 0)
            return static_cast<int32_t>(collected.size());

        const int32_t count =
            (std::min)(maxProperties, static_cast<int32_t>(collected.size()));
        for (int32_t i = 0; i < count; ++i)
        {
            const PropertyRef& property = collected[static_cast<size_t>(i)];
            AvaloniumPropertyInfo& info = outProperties[i];
            std::memset(&info, 0, sizeof(info));
            std::snprintf(info.name, sizeof(info.name), "%s", property.Name.c_str());
            std::snprintf(info.type_name, sizeof(info.type_name), "%s",
                          property.TypeName.c_str());
            info.offset = property.Offset;
            info.element_size = property.ElementSize;
            info.array_dim = property.ArrayDim;
            info.flags = property.Flags;
        }
        return count;
    }

    int32_t GetClassFunctions(EngineState& state, UClass* objectClass,
                              AvaloniumFunctionInfo* outFunctions, int32_t maxFunctions)
    {
        if (!objectClass || !state.UFunctionClass) return 0;

        struct CollectedFunction
        {
            std::string Name;
            void* Function = nullptr;
        };
        std::vector<CollectedFunction> collected;
        std::unordered_set<std::string> seen;

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

                const std::string name = EngineDiscovery::GetObjectNameStr(state, field);
                if (name.empty()) continue;

                const std::string key = Lower(name);
                if (!seen.insert(key).second) continue;

                collected.push_back({name, child});
            }
        }

        if (!outFunctions || maxFunctions <= 0)
            return static_cast<int32_t>(collected.size());

        const int32_t count =
            (std::min)(maxFunctions, static_cast<int32_t>(collected.size()));
        for (int32_t i = 0; i < count; ++i)
        {
            const CollectedFunction& function = collected[static_cast<size_t>(i)];
            AvaloniumFunctionInfo& info = outFunctions[i];
            std::memset(&info, 0, sizeof(info));
            std::snprintf(info.name, sizeof(info.name), "%s", function.Name.c_str());
            info.function = function.Function;
            info.function_flags = GetFunctionFlags(state, function.Function);
            info.frame_size = GetFunctionFrameSize(state, function.Function);
        }
        return count;
    }

    uint32_t GetFunctionFlags(EngineState& state, void* function)
    {
        if (!function || state.UFunction_FlagsOff < 0) return 0;
        uint32_t flags = 0;
        SafeMem_Read(reinterpret_cast<uint8_t*>(function) + state.UFunction_FlagsOff,
                     &flags, sizeof(flags));
        return flags;
    }

    int32_t GetFunctionFrameSize(EngineState& state, void* function)
    {
        if (!function) return 0;
        return EngineDiscovery::GetFunctionFrameSize(state, function);
    }
}
