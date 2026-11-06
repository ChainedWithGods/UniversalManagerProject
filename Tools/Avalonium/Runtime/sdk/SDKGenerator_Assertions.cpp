#include "SDKGenerator_Assertions.h"
#include "SDKGenerator_LayoutRules.h"
#include <iomanip>

namespace SDKGenerator {

    std::string GenerateStructAssertions(const StructAssertionInfo& info) {
        if (info.CppName.empty()) return "";

        const std::string& name = info.CppName;
        std::ostringstream out;

        out << "// Layout assertions for: " << name << "\n";
        out << "#define AVALONIUM_ASSERTS_" << name;

        if (info.Size <= 0) {
            out << "\n";
            return out.str();
        }

        out << " \\\n";

        out << "    static_assert(sizeof(" << name << ") == 0x" << std::hex << info.Size
            << ", \"expected sizeof(" << name << ") == 0x" << std::hex << info.Size << "\"); \\\n";

        for (const auto& member : info.Members) {
            if (member.IsBitfield || member.MemberName.empty()) continue;

            out << "    static_assert(offsetof(" << name << ", " << member.MemberName << ") == 0x"
                << std::hex << member.Offset << ", \"expected " << name << "::" << member.MemberName
                << " at 0x" << std::hex << member.Offset << "\"); \\\n";
        }
        out << "\n";
        return out.str();
    }

}
