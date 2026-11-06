#include "InlineHook.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../Helpers/minhook/src/hde/hde64.h"

namespace Avalonium {

namespace {

constexpr size_t kJumpBytes = 14;

constexpr size_t kCallBytes = 16;

constexpr int64_t kReach = 0x7FFFFFFFLL;

constexpr uintptr_t kSearchRange = 0x7FFF0000ULL;

constexpr uint32_t kMaxProbes = 0x20000;

static_assert(kJumpBytes == kInlineHookPatchBytes,
              "the saved copy has to hold exactly one patch");

void WriteJump(uint8_t* at, const void* destination) {
    at[0] = 0xFF;
    at[1] = 0x25;
    at[2] = 0x00;
    at[3] = 0x00;
    at[4] = 0x00;
    at[5] = 0x00;
    std::memcpy(at + 6, &destination, sizeof(destination));
}

void WriteCall(uint8_t* at, const void* destination) {
    at[0] = 0xFF;
    at[1] = 0x15;
    at[2] = 0x02;
    at[3] = 0x00;
    at[4] = 0x00;
    at[5] = 0x00;
    at[6] = 0xEB;
    at[7] = 0x08;
    std::memcpy(at + 8, &destination, sizeof(destination));
}

bool IsRipRelative(const hde64s& instruction) {
    return (instruction.flags & F_MODRM) != 0 && (instruction.modrm & 0xC7) == 0x05;
}

int ImmediateBytes(const hde64s& instruction) {
    return static_cast<int>((instruction.flags & 0x3C) >> 2);
}

enum class RipCarryRefusal {
    None = 0,
    StackOrControl,
    UndecodedEncoding,
    AddressSizePrefix,
    DisplacementShape,
    NoFreeRegister
};

const char* RipCarryRefusalText(RipCarryRefusal refusal) {
    switch (refusal) {
    case RipCarryRefusal::StackOrControl:
        return "it moves the stack pointer itself (a member of the FF group, or a pop)";
    case RipCarryRefusal::UndecodedEncoding:
        return "it is a VEX or EVEX encoding, whose operands this disassembler does not decode";
    case RipCarryRefusal::AddressSizePrefix:
        return "an address-size prefix makes its operand name the 32-bit EIP";
    case RipCarryRefusal::DisplacementShape:
        return "its addressing byte is not the shape the rewrite re-encodes (a SIB byte, no 32-bit "
               "displacement, or a REX.B that would name the register in place of RIP)";
    case RipCarryRefusal::NoFreeRegister:
        return "every register that could hold the address is one the instruction names";
    case RipCarryRefusal::None:
    default:
        return "it was carried";
    }
}

enum class Carry {
    Copy,
    CopyThroughRegister,

    JumpAbs,
    CallAbs,
    JumpCondAbs
};

struct Instruction {
    size_t OldOffset = 0;
    size_t NewOffset = 0;
    size_t OldLength = 0;
    size_t CarriedLength = 0;
    Carry How = Carry::Copy;

    uintptr_t Destination = 0;
    uint8_t Condition = 0;

    size_t RipDisplacementAt = 0;
    bool RipRelative = false;

    size_t ModrmAt = 0;
    size_t ImmediateAt = 0;
    size_t ImmediateLength = 0;
    uint8_t Scratch = 0;

    RipCarryRefusal CarryRefusal = RipCarryRefusal::None;

    bool KeepsRelative = false;
};

constexpr uint8_t kScratchRegisters[3] = { 3 , 6 , 7  };

constexpr size_t kPushBytes = 1;
constexpr size_t kPopBytes = 1;
constexpr size_t kLoadRegisterBytes = 10;

RipCarryRefusal CarryThroughRegister(const hde64s& decoded, Instruction& instruction) {

    if (decoded.opcode == 0xFF || decoded.opcode == 0x8F) return RipCarryRefusal::StackOrControl;

    if (decoded.opcode == 0xC4 || decoded.opcode == 0xC5 || decoded.opcode == 0x62) {
        return RipCarryRefusal::UndecodedEncoding;
    }

    if ((decoded.flags & F_PREFIX_67) != 0) return RipCarryRefusal::AddressSizePrefix;

    if ((decoded.flags & F_SIB) != 0 || (decoded.flags & F_DISP32) == 0 || decoded.rex_b != 0) {
        return RipCarryRefusal::DisplacementShape;
    }

    const size_t immediates = static_cast<size_t>(ImmediateBytes(decoded));
    const size_t length = static_cast<size_t>(decoded.len);
    if (length < immediates + 5) return RipCarryRefusal::DisplacementShape;

    const uint8_t taken = static_cast<uint8_t>(decoded.modrm_reg | (decoded.rex_r ? 8 : 0));
    for (uint8_t scratch : kScratchRegisters) {
        if (scratch == taken) continue;

        instruction.How = Carry::CopyThroughRegister;
        instruction.Scratch = scratch;
        instruction.ModrmAt = length - immediates - 5;
        instruction.ImmediateAt = length - immediates;
        instruction.ImmediateLength = immediates;
        instruction.CarriedLength =
            kPushBytes + kLoadRegisterBytes + (length - 4) + kPopBytes;
        return RipCarryRefusal::None;
    }
    return RipCarryRefusal::NoFreeRegister;
}

class ThreadFreeze {
public:

    explicit ThreadFreeze(const void* patchAt, size_t patchSize, const void* movedTo,
                          const std::vector<Instruction>* copiedFrom) {
        const DWORD self = GetCurrentThreadId();
        std::vector<DWORD> others;

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            THREADENTRY32 entry = {};
            entry.dwSize = sizeof(entry);
            if (Thread32First(snapshot, &entry)) {
                do {
                    if (entry.th32OwnerProcessID == GetCurrentProcessId() &&
                        entry.th32ThreadID != self) {
                        others.push_back(entry.th32ThreadID);
                    }
                } while (Thread32Next(snapshot, &entry));
            }
            CloseHandle(snapshot);
        }

        m_Threads.reserve(others.size());
        for (DWORD id : others) {
            HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                                       FALSE, id);
            if (!thread) continue;
            if (SuspendThread(thread) != static_cast<DWORD>(-1)) {
                m_Threads.push_back(thread);
            } else {
                CloseHandle(thread);
            }
        }

        if (patchAt && patchSize) {
            for (HANDLE thread : m_Threads) {
                if (MoveStrandedCounter(thread, patchAt, patchSize, movedTo, copiedFrom)) ++m_StrandedMoved;
            }
        }
    }

    ~ThreadFreeze() {
        for (HANDLE thread : m_Threads) {
            ResumeThread(thread);
            CloseHandle(thread);
        }
    }

    ThreadFreeze(const ThreadFreeze&) = delete;
    ThreadFreeze& operator=(const ThreadFreeze&) = delete;

    int StrandedMoved() const { return m_StrandedMoved; }

private:
    static bool MoveStrandedCounter(HANDLE thread, const void* patchAt, size_t patchSize,
                                    const void* movedTo,
                                    const std::vector<Instruction>* copiedFrom) {
#if defined(_M_X64) || defined(__x86_64__)
        if (!movedTo) return false;

        CONTEXT context = {};
        context.ContextFlags = CONTEXT_CONTROL;
        if (!GetThreadContext(thread, &context)) return false;

        const uintptr_t address = static_cast<uintptr_t>(context.Rip);
        const uintptr_t start = reinterpret_cast<uintptr_t>(patchAt);
        if (address < start || address >= start + patchSize) return false;

        uintptr_t resumedAt = 0;
        if (copiedFrom) {
            const size_t offset = static_cast<size_t>(address - start);
            for (const Instruction& instruction : *copiedFrom) {
                if (instruction.OldOffset != offset) continue;
                resumedAt = reinterpret_cast<uintptr_t>(movedTo) + instruction.NewOffset;
                break;
            }
        } else {
            resumedAt = reinterpret_cast<uintptr_t>(movedTo) + static_cast<size_t>(address - start);
        }
        if (!resumedAt) return false;

        context.Rip = static_cast<DWORD64>(resumedAt);
        return SetThreadContext(thread, &context) != FALSE;
#else
        (void)thread; (void)patchAt; (void)patchSize; (void)movedTo; (void)copiedFrom;
        return false;
#endif
    }

    std::vector<HANDLE> m_Threads;
    int m_StrandedMoved = 0;
};

void* ReserveNear(uintptr_t target, size_t size, uintptr_t low, uintptr_t high, uint32_t* probesOut) {
    SYSTEM_INFO info = {};
    GetSystemInfo(&info);
    const uintptr_t granularity = info.dwAllocationGranularity ? info.dwAllocationGranularity : 0x10000;
    const uintptr_t lowest = reinterpret_cast<uintptr_t>(info.lpMinimumApplicationAddress);
    const uintptr_t highest = reinterpret_cast<uintptr_t>(info.lpMaximumApplicationAddress);

    uint32_t probes = 0;
    for (uintptr_t distance = granularity; distance <= kSearchRange; distance += granularity) {
        const uintptr_t candidates[2] = {
            (target >= distance) ? target - distance : 0,
            target + distance
        };

        for (uintptr_t candidate : candidates) {
            const uintptr_t aligned = candidate & ~(granularity - 1);
            if (aligned < low || aligned > high) continue;
            if (aligned < lowest || aligned > highest) continue;
            if (++probes > kMaxProbes) {
                if (probesOut) *probesOut = probes;
                return nullptr;
            }

            MEMORY_BASIC_INFORMATION mbi = {};
            if (VirtualQuery(reinterpret_cast<void*>(aligned), &mbi, sizeof(mbi)) != sizeof(mbi)) continue;
            if (mbi.State != MEM_FREE || mbi.RegionSize < size) continue;

            void* reserved = VirtualAlloc(reinterpret_cast<void*>(aligned), size,
                                          MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (reserved) {
                if (probesOut) *probesOut = probes;
                return reserved;
            }
        }
    }

    if (probesOut) *probesOut = probes;
    return nullptr;
}

}

InlineHook::~InlineHook() {
    Remove();
}

bool InlineHook::Install(void* target, void* detour, void** originalOut, std::string* reason) {
    auto fail = [reason](const char* text) {
        if (reason) *reason = text;
        return false;
    };

    if (m_Installed) return fail("a patch is already installed");
    if (!target || !detour) return fail("no target or detour");

    m_PlacementNotes.clear();

    uint8_t* const start = static_cast<uint8_t*>(target);
    const uintptr_t base = reinterpret_cast<uintptr_t>(start);

    std::vector<Instruction> instructions;
    size_t oldLength = 0;
    size_t newLength = 0;

    while (oldLength < kJumpBytes) {
        hde64s decoded = {};
        const unsigned length = hde64_disasm(start + oldLength, &decoded);
        if (length == 0 || (decoded.flags & F_ERROR) || decoded.len != length) {
            char text[128];
            std::snprintf(text, sizeof(text),
                          "the instruction at +0x%zX of the target could not be decoded", oldLength);
            return fail(text);
        }

        Instruction instruction;
        instruction.OldOffset = oldLength;
        instruction.NewOffset = newLength;
        instruction.OldLength = decoded.len;

        const int64_t next = static_cast<int64_t>(base) + static_cast<int64_t>(oldLength) +
                             static_cast<int64_t>(decoded.len);

        if (decoded.opcode == 0xE8) {
            instruction.How = Carry::CallAbs;
            instruction.CarriedLength = kCallBytes;
            instruction.Destination = static_cast<uintptr_t>(next + static_cast<int32_t>(decoded.imm.imm32));
        } else if (decoded.opcode == 0xE9 || decoded.opcode == 0xEB) {
            instruction.How = Carry::JumpAbs;
            instruction.CarriedLength = kJumpBytes;
            instruction.Destination = static_cast<uintptr_t>(
                next + ((decoded.opcode == 0xEB) ? static_cast<int8_t>(decoded.imm.imm8)
                                                 : static_cast<int32_t>(decoded.imm.imm32)));
        } else if ((decoded.opcode & 0xF0) == 0x70) {
            instruction.How = Carry::JumpCondAbs;
            instruction.CarriedLength = kJumpBytes + 2;
            instruction.Condition = decoded.opcode & 0x0F;
            instruction.Destination = static_cast<uintptr_t>(next + static_cast<int8_t>(decoded.imm.imm8));
        } else if (decoded.opcode == 0x0F && (decoded.opcode2 & 0xF0) == 0x80) {
            instruction.How = Carry::JumpCondAbs;
            instruction.CarriedLength = kJumpBytes + 2;
            instruction.Condition = decoded.opcode2 & 0x0F;
            instruction.Destination = static_cast<uintptr_t>(next + static_cast<int32_t>(decoded.imm.imm32));
        } else if ((decoded.opcode & 0xFC) == 0xE0) {
            instruction.How = Carry::Copy;
            instruction.CarriedLength = decoded.len;
            instruction.KeepsRelative = true;
            instruction.Destination = static_cast<uintptr_t>(next + static_cast<int8_t>(decoded.imm.imm8));
        } else {
            instruction.How = Carry::Copy;
            instruction.CarriedLength = decoded.len;
            if (IsRipRelative(decoded)) {
                instruction.RipRelative = true;
                instruction.RipDisplacementAt =
                    decoded.len - static_cast<size_t>(ImmediateBytes(decoded)) - 4;
                instruction.Destination = static_cast<uintptr_t>(next + static_cast<int32_t>(decoded.disp.disp32));

                instruction.CarryRefusal = CarryThroughRegister(decoded, instruction);
            }
        }

        oldLength += decoded.len;
        newLength += instruction.CarriedLength;
        instructions.push_back(instruction);

        const bool endsHere = decoded.opcode == 0xC2 || decoded.opcode == 0xC3 ||
                              decoded.opcode == 0xCA || decoded.opcode == 0xCB ||
                              (decoded.opcode == 0xFF && (decoded.flags & F_MODRM) &&
                               decoded.modrm_reg == 4);
        if (endsHere) {
            if (oldLength < kJumpBytes) return fail("the function ends before the patch fits");
            break;
        }
        if (instructions.size() > 64) return fail("the prologue did not reach an end");
    }

    for (const Instruction& instruction : instructions) {
        if (!instruction.KeepsRelative) continue;
        if (instruction.Destination < base || instruction.Destination >= base + oldLength) {
            return fail("a LOOP/JECXZ in the prologue branches out of the block");
        }
        const size_t destinationOffset = static_cast<size_t>(instruction.Destination - base);
        for (const Instruction& other : instructions) {
            if (other.OldOffset <= instruction.OldOffset || other.OldOffset >= destinationOffset) continue;
            if (other.CarriedLength != other.OldLength) {
                return fail("a LOOP/JECXZ in the prologue is crossed by an instruction that grew");
            }
        }
    }

    for (const Instruction& instruction : instructions) {
        if (!instruction.RipRelative || instruction.CarryRefusal == RipCarryRefusal::None) continue;

        char offset[32];
        std::snprintf(offset, sizeof(offset), "%zX", instruction.OldOffset);

        char bytes[64] = {};
        size_t bytesUsed = 0;
        const size_t shown = instruction.OldLength < 15 ? instruction.OldLength : 15;
        for (size_t i = 0; i < shown && bytesUsed + 4 < sizeof(bytes); ++i) {
            const int written = std::snprintf(bytes + bytesUsed, sizeof(bytes) - bytesUsed, "%s%02X",
                                              i == 0 ? "" : " ", start[instruction.OldOffset + i]);
            if (written <= 0) break;
            bytesUsed += static_cast<size_t>(written);
        }

        if (!m_PlacementNotes.empty()) m_PlacementNotes += "; ";
        m_PlacementNotes += "the operand at +0x";
        m_PlacementNotes += offset;
        m_PlacementNotes += " (";
        m_PlacementNotes += bytes;
        m_PlacementNotes += ") is carried as it is: ";
        m_PlacementNotes += RipCarryRefusalText(instruction.CarryRefusal);
        m_PlacementNotes += "; the trampoline has to stay within reach of it";
    }

    bool needsReach = false;
    size_t reAddressed = 0;
    size_t carriedInRegister = 0;
    int64_t windowLow = 0;
    int64_t windowHigh = INT64_MAX;
    for (const Instruction& instruction : instructions) {
        if (!instruction.RipRelative) continue;
        if (instruction.How == Carry::CopyThroughRegister) {

            ++carriedInRegister;
            continue;
        }
        ++reAddressed;
        needsReach = true;

        const int64_t end = static_cast<int64_t>(instruction.NewOffset + instruction.CarriedLength);
        const int64_t destination = static_cast<int64_t>(instruction.Destination);

        windowLow = std::max(windowLow, destination - kReach - end);
        windowHigh = std::min(windowHigh, destination + kReach + 1 - end);
    }
    if (needsReach && windowLow > windowHigh) {
        return fail("the re-addressed operands leave nowhere within reach for a trampoline");
    }

    const size_t trampolineBytes = newLength + kJumpBytes;
    uint8_t* trampoline = nullptr;
    uint32_t probes = 0;

    if (!needsReach) {

        trampoline = static_cast<uint8_t*>(
            VirtualAlloc(nullptr, trampolineBytes, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!trampoline) return fail("VirtualAlloc failed");
    } else {
        const uintptr_t low = static_cast<uintptr_t>(
            std::max<int64_t>(windowLow, static_cast<int64_t>(base > kSearchRange ? base - kSearchRange : 0)));
        const uintptr_t high = static_cast<uintptr_t>(
            std::min<int64_t>(windowHigh, static_cast<int64_t>(base) + static_cast<int64_t>(kSearchRange)));

        void* reserved = (low <= high) ? ReserveNear(base, trampolineBytes, low, high, &probes) : nullptr;
        if (!reserved) {
            char text[320];
            std::snprintf(text, sizeof(text),
                          "no free page within reach of an operand the prologue carries as it is "
                          "(%u allocation granularity steps walked); installing would corrupt it",
                          probes);
            return fail(text);
        }
        trampoline = static_cast<uint8_t*>(reserved);
    }

    auto resolve = [&](const Instruction& instruction) -> const void* {
        if (instruction.Destination >= base && instruction.Destination < base + oldLength) {
            const size_t offset = static_cast<size_t>(instruction.Destination - base);
            for (const Instruction& candidate : instructions) {
                if (candidate.OldOffset == offset) return trampoline + candidate.NewOffset;
            }
        }
        return reinterpret_cast<const void*>(instruction.Destination);
    };

    for (const Instruction& instruction : instructions) {
        uint8_t* const out = trampoline + instruction.NewOffset;
        const uint8_t* const in = start + instruction.OldOffset;

        switch (instruction.How) {
        case Carry::JumpAbs:
            WriteJump(out, resolve(instruction));
            break;
        case Carry::CallAbs:
            WriteCall(out, resolve(instruction));
            break;
        case Carry::JumpCondAbs:

            out[0] = static_cast<uint8_t>(0x70 | ((instruction.Condition ^ 1) & 0x0F));
            out[1] = static_cast<uint8_t>(kJumpBytes);
            WriteJump(out + 2, resolve(instruction));
            break;
        case Carry::CopyThroughRegister: {

            out[0] = static_cast<uint8_t>(0x50 + instruction.Scratch);
            out[1] = 0x48;
            out[2] = static_cast<uint8_t>(0xB8 + instruction.Scratch);
            std::memcpy(out + 3, &instruction.Destination, sizeof(uintptr_t));

            uint8_t* const body = out + kPushBytes + kLoadRegisterBytes;
            std::memcpy(body, in, instruction.ModrmAt);

            body[instruction.ModrmAt] =
                static_cast<uint8_t>((in[instruction.ModrmAt] & 0xF8) | instruction.Scratch);
            std::memcpy(body + instruction.ModrmAt + 1, in + instruction.ImmediateAt,
                        instruction.ImmediateLength);
            body[instruction.ModrmAt + 1 + instruction.ImmediateLength] =
                static_cast<uint8_t>(0x58 + instruction.Scratch);
            break;
        }
        case Carry::Copy:
        default: {
            std::memcpy(out, in, instruction.OldLength);
            if (instruction.RipRelative) {
                const int64_t newNext = static_cast<int64_t>(
                    reinterpret_cast<uintptr_t>(out) + instruction.CarriedLength);
                const int64_t displacement = static_cast<int64_t>(instruction.Destination) - newNext;
                if (displacement < -kReach - 1 || displacement > kReach) {

                    VirtualFree(trampoline, 0, MEM_RELEASE);
                    return fail("the trampoline is out of reach of an operand it has to re-address");
                }
                const auto narrow = static_cast<int32_t>(displacement);
                std::memcpy(out + instruction.RipDisplacementAt, &narrow, sizeof(narrow));
            }
            break;
        }
        }
    }

    WriteJump(trampoline + newLength, start + oldLength);
    FlushInstructionCache(GetCurrentProcess(), trampoline, trampolineBytes);

    std::memcpy(m_Saved, start, kJumpBytes);

    DWORD previous = 0;
    if (!VirtualProtect(start, kJumpBytes, PAGE_EXECUTE_READWRITE, &previous)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return fail("VirtualProtect failed on the target");
    }

    if (originalOut) *originalOut = trampoline;
    m_Trampoline = trampoline;

    int strandedMoved = 0;
    {
        ThreadFreeze freeze(start, kJumpBytes, trampoline, &instructions);
        WriteJump(start, detour);
        strandedMoved = freeze.StrandedMoved();
    }

    VirtualProtect(start, kJumpBytes, previous, &previous);
    FlushInstructionCache(GetCurrentProcess(), start, kJumpBytes);

    m_Target = target;
    m_Detour = detour;
    m_StolenBytes = oldLength;
    m_Installed = true;

    const uintptr_t trampolineAddress = reinterpret_cast<uintptr_t>(trampoline);
    const uintptr_t distance = trampolineAddress > base ? trampolineAddress - base : base - trampolineAddress;
    char summary[320];
    std::snprintf(summary, sizeof(summary),
                  "displaced %zu byte(s) in %zu instruction(s), %zu re-addressed, %zu carried on a "
                  "register; trampoline %p (0x%llX from the target, reach beyond 2GB would corrupt: "
                  "%s); threads moved: %d",
                  oldLength, instructions.size(), reAddressed, carriedInRegister, trampoline,
                  static_cast<unsigned long long>(distance),
                  needsReach ? "required, and satisfied" : "not required", strandedMoved);
    m_Summary = summary;
    return true;
}

void InlineHook::Remove() {
    if (!m_Installed || !m_Target) return;

    DWORD previous = 0;
    if (VirtualProtect(m_Target, kJumpBytes, PAGE_EXECUTE_READWRITE, &previous)) {
        {
            ThreadFreeze freeze(m_Target, kJumpBytes, nullptr, nullptr);
            std::memcpy(m_Target, m_Saved, kJumpBytes);
        }
        VirtualProtect(m_Target, kJumpBytes, previous, &previous);
        FlushInstructionCache(GetCurrentProcess(), m_Target, kJumpBytes);
    }

    m_Target = nullptr;
    m_Detour = nullptr;
    m_Trampoline = nullptr;
    m_StolenBytes = 0;
    m_Installed = false;
    m_Summary.clear();
    m_PlacementNotes.clear();
}

}
