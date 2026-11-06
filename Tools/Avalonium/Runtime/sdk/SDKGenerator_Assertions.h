#pragma once
#include <string>
#include <vector>
#include <sstream>
#include "SDKGenerator_Impl.h"

namespace SDKGenerator {

    struct StructAssertionInfo {
        std::string CppName;
        int32_t Size = 0;

        int32_t Alignment = 0;

        int32_t LayoutShift = 0;
        int32_t LiveBaseSize = 0;
        int32_t CppBaseSize = 0;

        bool IsClass = false;
        struct MemberAssertion {
            std::string MemberName;
            int32_t Offset = 0;
            bool IsBitfield = false;
        };
        std::vector<MemberAssertion> Members;
    };

    std::string GenerateStructAssertions(const StructAssertionInfo& info);

}
