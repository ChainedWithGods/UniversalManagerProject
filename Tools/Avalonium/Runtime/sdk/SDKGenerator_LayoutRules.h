#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace SDKGenerator {

    const char* BitfieldUnderlyingType(uint8_t fieldSizeBytes);

    uint8_t BitfieldBitIndex(uint8_t byteOffset, uint8_t byteMask);

    uint32_t EnumUnderlyingBytes(int64_t minValue, int64_t maxValue);

    const char* EnumUnderlyingTypeName(int64_t minValue, int64_t maxValue);

    const char* EnumUnderlyingTypeNameForBytes(uint32_t bytes, bool isSigned);

    struct EnumValueRange {
        int64_t Min = 0;
        int64_t Max = 0;
        int64_t SentinelValue = 0;
        bool HasSentinel = false;
    };

    EnumValueRange ComputeEnumValueRange(const std::vector<std::pair<std::string, int64_t>>& values,
                                         const std::string& enumName);

    bool ValueFitsUnderlyingType(int64_t value, uint32_t bytes, bool isSigned);

    std::string FixedSizeEnumWrapper(const std::string& enumCppName,
                                     uint32_t enumUnderlyingBytes,
                                     uint32_t propertySizeBytes);

    std::string GetFixedSizeEnumTemplateContent();

    struct ArrayDimRead {
        int32_t Value = 1;

        bool Repaired = false;
    };

    ArrayDimRead SanitizeArrayDim(int32_t asInt32, uint8_t asByte);

    bool IsPlausibleStructAlignment(int32_t value);

    int32_t AlignUp(int32_t value, int32_t alignment);

    std::string BytePaddingLine(int32_t offset, int32_t padSize, const char* reason);

    std::string BitPaddingLine(int32_t offset, uint8_t fieldSizeBytes, uint8_t prevBitEndBit,
                               int32_t padBits, const char* reason);

    std::string BitfieldMemberLine(const std::string& memberName, uint8_t bitCount,
                                   int32_t offset, uint8_t bitIndex, uint8_t fieldSizeBytes,
                                   uint64_t flags, const std::string& flagsText);

    std::string MemberComment(int32_t offset, int32_t size, uint64_t flags,
                              const std::string& flagsText, const std::string& signatureHash);

}
