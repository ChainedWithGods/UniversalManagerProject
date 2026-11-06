#include "SDKGenerator_LayoutRules.h"

#include <iomanip>
#include <sstream>

namespace SDKGenerator {

    const char* BitfieldUnderlyingType(uint8_t fieldSizeBytes) {
        switch (fieldSizeBytes) {
        case 1: return "uint8_t";
        case 2: return "uint16_t";
        case 4: return "uint32_t";
        case 8: return "uint64_t";
        default: return "uint8_t";
        }
    }

    uint32_t EnumUnderlyingBytes(int64_t minValue, int64_t maxValue) {
        if (minValue >= 0) {
            if (maxValue > static_cast<int64_t>(0xFFFFFFFFULL)) return 8;
            if (maxValue > 0xFFFF) return 4;
            if (maxValue > 0xFF) return 2;
            return 1;
        }
        if (minValue < INT32_MIN || maxValue > INT32_MAX) return 8;
        if (minValue < -32768 || maxValue > 32767) return 4;
        if (minValue < -128 || maxValue > 127) return 2;
        return 1;
    }

    const char* EnumUnderlyingTypeName(int64_t minValue, int64_t maxValue) {
        static constexpr const char* SignedNames[4] = {"int8_t", "int16_t", "int32_t", "int64_t"};
        static constexpr const char* UnsignedNames[4] = {"uint8_t", "uint16_t", "uint32_t", "uint64_t"};

        const uint32_t bytes = EnumUnderlyingBytes(minValue, maxValue);
        const int index = (bytes == 1) ? 0 : (bytes == 2) ? 1 : (bytes == 4) ? 2 : 3;
        return (minValue >= 0) ? UnsignedNames[index] : SignedNames[index];
    }

    const char* EnumUnderlyingTypeNameForBytes(uint32_t bytes, bool isSigned) {
        static constexpr const char* SignedNames[4] = {"int8_t", "int16_t", "int32_t", "int64_t"};
        static constexpr const char* UnsignedNames[4] = {"uint8_t", "uint16_t", "uint32_t", "uint64_t"};
        const int index = (bytes <= 1) ? 0 : (bytes == 2) ? 1 : (bytes == 4) ? 2 : 3;
        return isSigned ? SignedNames[index] : UnsignedNames[index];
    }

    EnumValueRange ComputeEnumValueRange(const std::vector<std::pair<std::string, int64_t>>& values,
                                         const std::string& enumName) {        EnumValueRange range;
        const std::string sentinelName = enumName.empty() ? std::string() : enumName + "_MAX";
        bool first = true;

        for (const auto& value : values) {
            if (!sentinelName.empty() && value.first == sentinelName) {
                range.HasSentinel = true;
                range.SentinelValue = value.second;
                continue;
            }
            if (first) {
                range.Min = range.Max = value.second;
                first = false;
                continue;
            }
            if (value.second < range.Min) range.Min = value.second;
            if (value.second > range.Max) range.Max = value.second;
        }
        return range;
    }

    bool ValueFitsUnderlyingType(int64_t value, uint32_t bytes, bool isSigned) {
        if (bytes == 0 || bytes > 8) return false;
        if (bytes == 8) return true;

        const int bits = static_cast<int>(bytes) * 8;
        if (isSigned) {
            const int64_t low = -(int64_t(1) << (bits - 1));
            const int64_t high = (int64_t(1) << (bits - 1)) - 1;
            return value >= low && value <= high;
        }
        return value >= 0 && value < (int64_t(1) << bits);
    }

    uint8_t BitfieldBitIndex(uint8_t byteOffset, uint8_t byteMask) {
        uint8_t bitInByte = 0;
        uint8_t mask = byteMask;
        while ((mask & 1) == 0 && bitInByte < 8) {
            mask >>= 1;
            bitInByte++;
        }
        if (bitInByte >= 8) bitInByte = 0;
        return static_cast<uint8_t>(byteOffset * 8 + bitInByte);
    }

    std::string FixedSizeEnumWrapper(const std::string& enumCppName,
                                     uint32_t enumUnderlyingBytes,
                                     uint32_t propertySizeBytes) {
        if (enumCppName.empty()) return "";
        if (propertySizeBytes == 0 || propertySizeBytes == enumUnderlyingBytes) return "";

        if (enumUnderlyingBytes <= propertySizeBytes) return "";

        const char* alias = nullptr;
        switch (propertySizeBytes) {
        case 1: alias = "T1ByteEnum"; break;
        case 2: alias = "T2ByteEnum"; break;
        case 4: alias = "T4ByteEnum"; break;
        case 8: alias = "T8ByteEnum"; break;
        default: return "";
        }

        return std::string(alias) + "<" + enumCppName + ">";
    }

    std::string GetFixedSizeEnumTemplateContent() {
        return R"(

template<typename EnumType, typename UnderlyingType>
struct TFixedSizeEnum
{
    using EnumUnderlyingType = UnderlyingType;

    UnderlyingType Value;

    constexpr TFixedSizeEnum() : Value(static_cast<UnderlyingType>(0)) {}
    constexpr TFixedSizeEnum(EnumType InValue) : Value(static_cast<UnderlyingType>(InValue)) {}

    constexpr TFixedSizeEnum& operator=(EnumType InValue) { Value = static_cast<UnderlyingType>(InValue); return *this; }

    constexpr operator EnumType() const { return static_cast<EnumType>(Value); }

    constexpr EnumType GetValue() const { return static_cast<EnumType>(Value); }

    constexpr bool operator==(EnumType Other) const { return Value == static_cast<UnderlyingType>(Other); }
    constexpr bool operator!=(EnumType Other) const { return Value != static_cast<UnderlyingType>(Other); }
};

template<typename EnumType> using T1ByteSignedEnum = TFixedSizeEnum<EnumType, int8_t>;
template<typename EnumType> using T2ByteSignedEnum = TFixedSizeEnum<EnumType, int16_t>;
template<typename EnumType> using T4ByteSignedEnum = TFixedSizeEnum<EnumType, int32_t>;
template<typename EnumType> using T8ByteSignedEnum = TFixedSizeEnum<EnumType, int64_t>;

template<typename EnumType> using T1ByteEnum = TFixedSizeEnum<EnumType, uint8_t>;
template<typename EnumType> using T2ByteEnum = TFixedSizeEnum<EnumType, uint16_t>;
template<typename EnumType> using T4ByteEnum = TFixedSizeEnum<EnumType, uint32_t>;
template<typename EnumType> using T8ByteEnum = TFixedSizeEnum<EnumType, uint64_t>;
)";
    }

    ArrayDimRead SanitizeArrayDim(int32_t asInt32, uint8_t asByte) {
        ArrayDimRead read;

        const bool int32Plausible = (asInt32 >= 1 && asInt32 <= 0x1000);
        if (int32Plausible) {
            read.Value = asInt32;
            return read;
        }

        const bool bytePlausible = (asByte >= 1);
        read.Value = bytePlausible ? asByte : 1;
        read.Repaired = true;
        return read;
    }

    bool IsPlausibleStructAlignment(int32_t value) {
        return value == 1 || value == 2 || value == 4 || value == 8 || value == 16 || value == 32;
    }

    int32_t AlignUp(int32_t value, int32_t alignment) {
        if (alignment <= 1 || value <= 0) return value;
        const int32_t remainder = value % alignment;
        return remainder == 0 ? value : value + (alignment - remainder);
    }

    std::string BytePaddingLine(int32_t offset, int32_t padSize, const char* reason) {
        std::ostringstream out;
        out << "    uint8_t Pad_" << std::hex << offset << "[0x" << padSize << "];"
            << " // 0x" << std::hex << offset << "(0x" << padSize << ")(" << reason << ")\n";
        return out.str();
    }

    std::string BitPaddingLine(int32_t offset, uint8_t fieldSizeBytes, uint8_t prevBitEndBit,
                               int32_t padBits, const char* reason) {
        std::ostringstream out;
        out << "    " << BitfieldUnderlyingType(fieldSizeBytes) << " BitPad_"
            << std::hex << offset << "_" << static_cast<int>(prevBitEndBit) << " : " << std::dec << padBits << ";"
            << " // 0x" << std::hex << offset << "(0x" << std::setw(4) << std::setfill('0')
            << static_cast<int>(fieldSizeBytes) << std::setfill(' ') << ")"
            << "(" << reason << ")\n";
        return out.str();
    }

    std::string BitfieldMemberLine(const std::string& memberName, uint8_t bitCount,
                                   int32_t offset, uint8_t bitIndex, uint8_t fieldSizeBytes,
                                   uint64_t flags, const std::string& flagsText) {
        std::ostringstream out;
        out << "    " << BitfieldUnderlyingType(fieldSizeBytes) << " " << memberName << " : " << std::dec << static_cast<int>(bitCount) << ";"
            << " // 0x" << std::hex << offset
            << "(0x" << std::setw(4) << std::setfill('0') << static_cast<int>(fieldSizeBytes) << std::setfill(' ') << ")"
            << "(BitIndex: 0x" << static_cast<int>(bitIndex)
            << ", PropSize: 0x" << std::setw(4) << std::setfill('0') << static_cast<int>(fieldSizeBytes)
            << std::setfill(' ') << " flags=0x" << flags;
        if (!flagsText.empty()) out << " (" << flagsText << ")";
        out << ")\n";
        return out.str();
    }

    std::string MemberComment(int32_t offset, int32_t size, uint64_t flags,
                              const std::string& flagsText, const std::string& signatureHash) {
        std::ostringstream out;
        out << " // 0x" << std::hex << offset << "(0x" << size << ")"
            << " flags=0x" << flags;
        if (!flagsText.empty()) out << " (" << flagsText << ")";
        if (!signatureHash.empty()) out << " hash=" << signatureHash;
        out << "\n";
        return out.str();
    }

}
