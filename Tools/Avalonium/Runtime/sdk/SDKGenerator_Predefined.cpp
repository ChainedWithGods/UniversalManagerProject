#include "SDKGenerator_Predefined.h"
#include "SDKGenerator.h"
#include "../core/ObjectUtils.h"
#include "../utils/SafeMemory.h"
#include <cctype>
#include <sstream>

namespace SDKGenerator {
namespace {

    void EmitPad(std::ostringstream& out, int32_t& cursor, int32_t target)
    {
        if (target <= cursor) return;
        out << "    uint8_t Pad_0x" << std::hex << cursor << "[0x" << (target - cursor)
            << "];\n" << std::dec;
        cursor = target;
    }

    void EmitField(std::ostringstream& out, int32_t& cursor, int32_t offset, int32_t width,
                   const char* declaration)
    {
        if (offset < 0 || width <= 0 || offset < cursor) return;
        EmitPad(out, cursor, offset);
        out << "    " << declaration << "\n";
        cursor += width;
    }

    void FinishClass(std::ostringstream& out, int32_t& cursor, int32_t classSize)
    {
        if (classSize < cursor) classSize = cursor;
        EmitPad(out, cursor, classSize);
    }

    int32_t BaseCursor(int32_t baseSize, int32_t fallback)
    {
        if (baseSize > 0) return baseSize;
        return fallback > 0 ? fallback : 0;
    }

    bool NarrowMinAlignment(const EngineState& state)
    {
        return state.UEMajor > 5 || (state.UEMajor == 5 && state.UEMinor >= 6);
    }

    int32_t DiscoverClassCastFlagsOffset(const EngineState& state)
    {
        UClass* fieldClass =
            state.UStructClass
                ? reinterpret_cast<UClass*>(EngineDiscovery::GetSuperStruct(
                      state, reinterpret_cast<UStruct*>(state.UStructClass)))
                : nullptr;

        struct Probe
        {
            UClass* Class;
            uint64_t Flags;
        };
        const Probe probes[] = {
            { fieldClass, 0x1 },
            { state.UEnumClass, 0x5 },
            { state.UStructClass, 0x9 },
            { state.UClassClass, 0x29 },
            { state.UFunctionClass, 0x80009 },
        };

        for (int32_t offset = 0x60; offset <= 0x300; offset += 8)
        {
            bool matched = true;
            for (const Probe& probe : probes)
            {
                uint64_t value = 0;
                if (!probe.Class ||
                    !SafeMem_Read(reinterpret_cast<const uint8_t*>(probe.Class) + offset,
                                  &value, sizeof(value)) ||
                    value != probe.Flags)
                {
                    matched = false;
                    break;
                }
            }
            if (matched) return offset;
        }
        return -1;
    }
}

    std::string GetCorePredefinedCanonicalName(const std::string& className) {
        if (className.empty()) return "";
        std::string lower;
        lower.reserve(className.size());
        for (char c : className) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (lower.size() > 1 && lower[0] == 'u') lower = lower.substr(1);

        while (!lower.empty() && lower.back() == '_') lower.pop_back();

        if (lower == "object") return "Object";
        if (lower == "field") return "Field";
        if (lower == "struct") return "Struct";
        if (lower == "class") return "Class";
        if (lower == "function") return "Function";

        return "";
    }

    bool IsCorePredefinedClass(const std::string& className) {
        return !GetCorePredefinedCanonicalName(className).empty();
    }

    std::string GetPredefinedClassDeclaration(const EngineState& state, const std::string& className,
                                             int32_t baseSize, int32_t classSize) {
        std::string canonical = GetCorePredefinedCanonicalName(className);
        std::ostringstream out;

        if (canonical == "Object") {
            out << "// Class CoreUObject.Object\n\n#pragma pack(push, 1)\nclass UObject\n{\npublic:\n";
            int32_t cursor = 0;
            EmitField(out, cursor, state.UObject_VTableOff, 8, "void* VTable;");
            EmitField(out, cursor, state.UObject_FlagsOff, 4, "int32_t ObjectFlags;");
            EmitField(out, cursor, state.UObject_IndexOff, 4, "int32_t InternalIndex;");
            EmitField(out, cursor, state.UObject_ClassOff, 8, "class UClass* Class;");
            EmitField(out, cursor, state.UObject_NameOff, 8, "struct FName Name;");
            EmitField(out, cursor, state.UObject_OuterOff, 8, "class UObject* Outer;");
            FinishClass(out, cursor, classSize);
            out << R"(
public:
    static class UClass* StaticClass()
    {
        static SDKHelpers::StaticClassInfo Info{};
        return SDKHelpers::GetStaticClass("Object", Info);
    }

    inline void ProcessEvent(class UFunction* Function, void* Parms)
    {
        SDKHelpers::ProcessEvent(this, Function, Parms);
    }

    inline std::string GetName() const
    {
        return SDKHelpers::GetObjectName(this);
    }

    inline std::string GetFullName() const
    {
        return SDKHelpers::GetObjectFullName(this);
    }

    bool IsA(class UClass* Clss) const;

    template<typename TargetClass>
    inline bool IsA() const
    {
        return IsA(TargetClass::StaticClass());
    }

    void ExecuteUbergraph(int32_t EntryPoint);
};
#pragma pack(pop)
)";
            return out.str();
        }

        if (canonical == "Field") {
            out << "// Class CoreUObject.Field\n\n#pragma pack(push, 1)\nclass UField : public UObject\n{\npublic:\n";
            int32_t cursor = BaseCursor(baseSize, state.UObject_OuterOff + 8);
            EmitField(out, cursor, state.UField_NextOff, 8, "class UField* Next;");
            FinishClass(out, cursor, classSize);
            out << R"(
public:
    static class UClass* StaticClass()
    {
        static SDKHelpers::StaticClassInfo Info{};
        return SDKHelpers::GetStaticClass("Field", Info);
    }
};
#pragma pack(pop)
)";
            return out.str();
        }

        if (canonical == "Struct") {
            out << "// Class CoreUObject.Struct\n\n#pragma pack(push, 1)\nclass UStruct : public UField\n{\npublic:\n";
            int32_t cursor = BaseCursor(baseSize, state.UField_NextOff + 8);
            EmitField(out, cursor, state.UStruct_SuperOff, 8, "class UStruct* SuperStruct;");
            EmitField(out, cursor, state.UStruct_ChildrenOff, 8, "class UField* Children;");
            if (state.UStruct_ChildPropsOff >= 0)
                EmitField(out, cursor, state.UStruct_ChildPropsOff, 8, "void* ChildProperties;");

            const int32_t propertiesSizeOff = EngineDiscovery::UStructPropertiesSizeOffset(state);
            EmitField(out, cursor, propertiesSizeOff, 4, "int32_t PropertiesSize;");
            if (state.UStruct_MinAlignmentOff >= 0)
            {
                if (NarrowMinAlignment(state))
                    EmitField(out, cursor, state.UStruct_MinAlignmentOff, 2, "int16_t MinAlignment;");
                else
                    EmitField(out, cursor, state.UStruct_MinAlignmentOff, 4, "int32_t MinAlignment;");
            }

            FinishClass(out, cursor, classSize);
            out << R"(
public:
    static class UClass* StaticClass()
    {
        static SDKHelpers::StaticClassInfo Info{};
        return SDKHelpers::GetStaticClass("Struct", Info);
    }

    inline bool IsChildOf(const class UStruct* Base) const
    {
        return SDKHelpers::IsChildOf(this, Base);
    }
};
#pragma pack(pop)
)";
            return out.str();
        }

        if (canonical == "Class") {
            out << "// Class CoreUObject.Class\n\n#pragma pack(push, 1)\nclass UClass : public UStruct\n{\npublic:\n";
            int32_t cursor = BaseCursor(baseSize, state.UStruct_MinAlignmentOff + 4);
            const int32_t classCastFlagsOff = DiscoverClassCastFlagsOffset(state);
            if (classCastFlagsOff < 0)
            {
                SdkLog("[SDK] Layout: UClass::ClassCastFlags was not found; the field is omitted");
            }
            EmitField(out, cursor, classCastFlagsOff, 8, "uint64_t ClassCastFlags;");
            EmitField(out, cursor, state.UClass_ClassDefaultObjectOff, 8,
                      "class UObject* ClassDefaultObject;");
            FinishClass(out, cursor, classSize);
            out << R"(
public:
    static class UClass* StaticClass()
    {
        static SDKHelpers::StaticClassInfo Info{};
        return SDKHelpers::GetStaticClass("Class", Info);
    }

    inline class UFunction* GetFunction(const std::string& ClassName, const std::string& FuncName) const
    {
        return SDKHelpers::GetFunction(ClassName.c_str(), FuncName.c_str());
    }
};
#pragma pack(pop)
)";
            return out.str();
        }

        if (canonical == "Function") {
            out << "// Class CoreUObject.Function\n\n#pragma pack(push, 1)\nclass UFunction : public UStruct\n{\npublic:\n";
            int32_t cursor = BaseCursor(baseSize, state.UStruct_MinAlignmentOff + 4);
            EmitField(out, cursor, state.UFunction_FlagsOff, 4, "uint32_t FunctionFlags;");
            EmitField(out, cursor, state.UFunction_NumParmsOff, 1, "uint8_t NumParms;");
            EmitField(out, cursor, state.UFunction_ParmsSizeOff, 2, "uint16_t ParmsSize;");
            EmitField(out, cursor, state.UFunction_ReturnValueOffsetOff, 2, "uint16_t ReturnValueOffset;");
            EmitField(out, cursor, state.UFunction_RPCIdOff, 2, "uint16_t RPCId;");
            EmitField(out, cursor, state.UFunction_RPCResponseIdOff, 2, "uint16_t RPCResponseId;");
            EmitField(out, cursor, state.UFunction_FuncOff, 8, "void* ExecFunction;");
            FinishClass(out, cursor, classSize);
            out << R"(
public:
    static class UClass* StaticClass()
    {
        static SDKHelpers::StaticClassInfo Info{};
        return SDKHelpers::GetStaticClass("Function", Info);
    }
};
#pragma pack(pop)
)";
            return out.str();
        }

        return "";
    }

    std::string GetPredefinedClassImplementation(const EngineState& state, const std::string& className) {
        std::string canonical = GetCorePredefinedCanonicalName(className);
        std::ostringstream out;

        if (canonical == "Object") {
            out << R"(bool UObject::IsA(class UClass* Clss) const
{
    return SDKHelpers::IsA(this, Clss);
}

void UObject::ExecuteUbergraph(int32_t EntryPoint)
{
    static SDKHelpers::ReloadableFuncInfo FuncInfo{};
    class UFunction* Func = SDKHelpers::GetCachedFunction(FuncInfo);
    if (Func == nullptr)
        Func = SDKHelpers::GetFunction("Object", "ExecuteUbergraph");
    SDKHelpers::SetCachedFunction(Func, FuncInfo);

    struct Object_ExecuteUbergraph_Params
    {
    public:
        int32_t EntryPoint;
    } Parms{};

    Parms.EntryPoint = EntryPoint;
    ProcessEvent(Func, &Parms);
}
)";
            return out.str();
        }

        return "";
    }

}
