#include "SDKGenerator_Bitfields.h"
#include "SDKGenerator_LayoutRules.h"
#include <iomanip>
#include <algorithm>
#include <bit>

namespace SDKGenerator {

    void ExtractBoolPropertyBitfields(const EngineState& state, void* propField, SDKProperty& prop) {
        if (!propField) return;

        uint8_t fieldSize = 1;
        uint8_t byteOffset = 0;
        uint8_t byteMask = 0;
        uint8_t fieldMask = 0;

        if (state.bUseFProperty) {

            int32_t baseOff = state.FBoolProperty_BaseOff >= 0 ? state.FBoolProperty_BaseOff : 0x78;
            SafeMem_Read(reinterpret_cast<uint8_t*>(propField) + baseOff + 0, &fieldSize, 1);
            SafeMem_Read(reinterpret_cast<uint8_t*>(propField) + baseOff + 1, &byteOffset, 1);
            SafeMem_Read(reinterpret_cast<uint8_t*>(propField) + baseOff + 2, &byteMask, 1);
            SafeMem_Read(reinterpret_cast<uint8_t*>(propField) + baseOff + 3, &fieldMask, 1);
        } else {

            int32_t baseOff = state.FBoolProperty_BaseOff >= 0 ? state.FBoolProperty_BaseOff : 0x70;
            SafeMem_Read(reinterpret_cast<uint8_t*>(propField) + baseOff + 0, &fieldSize, 1);
            SafeMem_Read(reinterpret_cast<uint8_t*>(propField) + baseOff + 1, &byteOffset, 1);
            SafeMem_Read(reinterpret_cast<uint8_t*>(propField) + baseOff + 2, &byteMask, 1);
            SafeMem_Read(reinterpret_cast<uint8_t*>(propField) + baseOff + 3, &fieldMask, 1);
        }

        if (fieldSize != 1 && fieldSize != 2 && fieldSize != 4 && fieldSize != 8) fieldSize = 1;
        prop.FieldSize = fieldSize;

        if (byteMask == 0xFF || fieldMask == 0xFF || byteMask == 0) {
            prop.IsBitfield = false;
            prop.BitIndex = 0xFF;
            prop.BitCount = 1;
            prop.ByteMask = byteMask;
            prop.FieldMask = fieldMask;
            prop.Type = "bool";
            prop.Size = 1;
            prop.ElementSize = 1;
            prop.FieldSize = 1;
            return;
        }

        prop.IsBitfield = true;
        prop.ByteMask = byteMask;
        prop.FieldMask = fieldMask;
        prop.BitCount = 1;

        prop.BitIndex = BitfieldBitIndex(byteOffset, byteMask);
        prop.Type = BitfieldUnderlyingType(prop.FieldSize);
        prop.Size = prop.FieldSize;
        prop.ElementSize = prop.FieldSize;
    }

    void SortPropertiesForLayout(std::vector<SDKProperty>& props) {
        std::stable_sort(props.begin(), props.end(), [](const SDKProperty& a, const SDKProperty& b) {
            if (a.Offset != b.Offset) {
                return a.Offset < b.Offset;
            }
            if (a.IsBitfield && b.IsBitfield) {
                return a.BitIndex < b.BitIndex;
            }
            if (a.IsBitfield != b.IsBitfield) {

                return a.IsBitfield;
            }
            return false;
        });
    }

    void WriteMembersAccurate(std::ostringstream& out, const std::vector<SDKProperty>& props,
                             int superSize, int structSize, const std::string& scopeName,
                             std::vector<int32_t>* emittedOffsets) {

        int currentOffset = superSize;
        bool lastWasBitfield = false;
        int prevBitOffset = 0;
        int prevBitEndBit = 0;
        uint8_t prevFieldSize = 1;
        std::set<std::string> usedNames;
        if (!scopeName.empty()) usedNames.insert(scopeName);

        for (const auto& prop : props) {
            const int memberOffset = prop.Offset;
            const int memberSize = prop.Size > 0 ? prop.Size : 1;
            const bool isBitfield = prop.IsBitfield;
            const uint8_t fieldSize = (prop.FieldSize > 0) ? prop.FieldSize : 1;

            const int prevBitsPerWord = static_cast<int>(prevFieldSize) * 8;
            if (lastWasBitfield && (memberOffset > prevBitOffset || !isBitfield) && prevBitEndBit < prevBitsPerWord) {
                out << BitPaddingLine(prevBitOffset, prevFieldSize, (uint8_t)prevBitEndBit,
                                      prevBitsPerWord - prevBitEndBit, "Fixing Bit-Field Size For New Byte");
                prevBitEndBit = 0;
                currentOffset = prevBitOffset + prevFieldSize;
            }

            if (memberOffset > currentOffset) {
                out << BytePaddingLine(currentOffset, memberOffset - currentOffset, "Fixing Size After Last Property");
                currentOffset = memberOffset;
            }

            if (emittedOffsets) emittedOffsets->push_back(currentOffset);

            if (isBitfield) {
                if (memberOffset > prevBitOffset) {
                    prevBitEndBit = 0;
                }

                if (prop.BitIndex > prevBitEndBit) {
                    out << BitPaddingLine(memberOffset, fieldSize, (uint8_t)prevBitEndBit,
                                          prop.BitIndex - prevBitEndBit, "Fixing Bit-Field Size Between Bits");
                }

                const std::string memberName = MakeUniqueName(prop.Name, usedNames);
                out << BitfieldMemberLine(memberName, prop.BitCount, memberOffset, prop.BitIndex,
                                          fieldSize, prop.Flags, prop.FlagsText);

                prevBitEndBit = prop.BitIndex + prop.BitCount;
                prevBitOffset = memberOffset;
                prevFieldSize = fieldSize;
                currentOffset = memberOffset + fieldSize;
                lastWasBitfield = true;
            } else {
                const std::string memberName = MakeUniqueName(prop.Name, usedNames);
                out << "    " << RenderFieldDeclaration(prop, memberName) << ";"
                    << MemberComment(prop.Offset, prop.Size, prop.Flags, prop.FlagsText, prop.SignatureHash);

                int actualConsumed = (prop.ArrayDim > 1) ? (prop.ElementSize * prop.ArrayDim) : prop.Size;
                if (actualConsumed <= 0) actualConsumed = 1;
                currentOffset = memberOffset + actualConsumed;
                lastWasBitfield = false;
            }
        }

        const int lastBitsPerWord = static_cast<int>(prevFieldSize) * 8;
        if (lastWasBitfield && prevBitEndBit < lastBitsPerWord) {
            out << BitPaddingLine(prevBitOffset, prevFieldSize, (uint8_t)prevBitEndBit,
                                  lastBitsPerWord - prevBitEndBit, "Fixing Bit-Field Size For Last Byte");
            currentOffset = prevBitOffset + prevFieldSize;
        }

        if (structSize > currentOffset) {
            out << BytePaddingLine(currentOffset, structSize - currentOffset, "Fixing Struct Size After Last Property");
        }
    }

}
