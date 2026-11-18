#include "InlineHook.h"
#include "SafeMemory.h"

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

constexpr size_t kSlotJumpBytes = 6;

struct SlotExchangeResult {
    bool Accessible = false;
    bool Exchanged = false;
    DWORD ProtectionError = 0;
};

SlotExchangeResult ExchangeSlotPointer(void** slot, void* expected, void* replacement) {
    SlotExchangeResult result;
    MEMORY_BASIC_INFORMATION info = {};
    if (VirtualQuery(slot, &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return result;
    }

    const bool writable = (info.Protect & (PAGE_READWRITE | PAGE_WRITECOPY |
                                          PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
    const bool executable = (info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                                            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
    DWORD previous = 0;
    if (!writable && !VirtualProtect(slot, sizeof(void*),
                                     executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE, &previous)) {
        return result;
    }

    __try {
        result.Exchanged = InterlockedCompareExchangePointer(slot, replacement, expected) == expected;
        result.Accessible = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    DWORD ignored = 0;
    if (!writable && !VirtualProtect(slot, sizeof(void*), previous, &ignored)) {
        result.ProtectionError = GetLastError();
    }
    return result;
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

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            THREADENTRY32 entry = {};
            entry.dwSize = sizeof(entry);
            if (Thread32First(snapshot, &entry)) {
                do {
                    if (entry.th32OwnerProcessID == GetCurrentProcessId() &&
                        entry.th32ThreadID != self) {
                        m_ThreadIds.push_back(entry.th32ThreadID);
                    }
                } while (Thread32Next(snapshot, &entry));
            }
            CloseHandle(snapshot);
        }

        m_Threads.reserve(m_ThreadIds.size());
        for (DWORD id : m_ThreadIds) {
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

    std::vector<DWORD> m_ThreadIds;
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

bool IsExecutableAddress(uintptr_t address) {
    if (address < 0x10000ULL || address > 0x00007FFFFFFEFFFFULL) return false;

    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
    if (mbi.State != MEM_COMMIT) return false;

    const DWORD protection = mbi.Protect & 0xFF;
    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
           protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

bool IsInsideLoadedModule(uintptr_t address) {
    HMODULE module = nullptr;
    return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCSTR>(address), &module) != FALSE && module != nullptr;
}

constexpr int kMaxForwarderHops = 8;

enum class EntryShape {
    Body,
    Follow,
    Chain
};

struct EntryResolution {
    EntryShape Shape = EntryShape::Body;
    uintptr_t Body = 0;
    int Hops = 0;

    uintptr_t ChainDetour = 0;
    uintptr_t ChainSlot = 0;
    uint8_t ChainBytes[kSlotJumpBytes] = {};
    size_t ChainJumpLength = 0;
    std::string Note;
};

bool ResolveEntry(uintptr_t entry, EntryResolution* out) {
    uintptr_t current = entry;
    int hops = 0;

    for (;;) {
        uint8_t code[6] = {};
        if (!SafeMem_Read(reinterpret_cast<const void*>(current), code, sizeof(code))) {
            char text[192];
            std::snprintf(text, sizeof(text),
                          "the bytes at %p could not be read, so what the entry runs cannot be "
                          "determined",
                          reinterpret_cast<void*>(current));
            out->Note = text;
            return false;
        }

        const bool isSlotJump = code[0] == 0xFF && code[1] == 0x25;
        const bool isNearJump = code[0] == 0xE9;
        const bool isShortJump = code[0] == 0xEB;
        if (!isSlotJump && !isNearJump && !isShortJump) {
            out->Shape = hops > 0 ? EntryShape::Follow : EntryShape::Body;
            out->Body = current;
            out->Hops = hops;
            return true;
        }

        uintptr_t next = 0;
        uintptr_t jumpSlot = 0;
        size_t jumpLength = 0;
        const char* form = isSlotJump ? "FF 25" : (isNearJump ? "E9" : "EB");

        bool isHookPatch = false;

        if (isSlotJump) {

            int32_t displacement = 0;
            std::memcpy(&displacement, code + 2, sizeof(displacement));
            const uintptr_t slot = current + 6 + static_cast<uintptr_t>(static_cast<intptr_t>(displacement));
            jumpLength = (displacement == 0) ? 14 : 6;
            jumpSlot = slot;

            void* destination = nullptr;
            if (!SafeMem_Read(reinterpret_cast<const void*>(slot), &destination, sizeof(destination)) ||
                !destination) {
                char text[192];
                std::snprintf(text, sizeof(text),
                              "the forwarder at %p reads its destination from %p, which holds no "
                              "pointer, so what the entry runs cannot be determined",
                              reinterpret_cast<void*>(current), reinterpret_cast<void*>(slot));
                out->Note = text;
                return false;
            }
            next = reinterpret_cast<uintptr_t>(destination);

            isHookPatch = !IsInsideLoadedModule(slot);
        } else if (isNearJump) {
            jumpLength = 5;
            int32_t displacement = 0;
            std::memcpy(&displacement, code + 1, sizeof(displacement));
            next = current + 5 + static_cast<uintptr_t>(static_cast<intptr_t>(displacement));
            isHookPatch = !IsInsideLoadedModule(next);
        } else {
            jumpLength = 2;
            next = current + 2 + static_cast<uintptr_t>(static_cast<intptr_t>(static_cast<int8_t>(code[1])));
            isHookPatch = !IsInsideLoadedModule(next);
        }

        if (next == current) {
            char text[192];
            std::snprintf(text, sizeof(text), "the forwarder at %p (%s) jumps to itself",
                          reinterpret_cast<void*>(current), form);
            out->Note = text;
            return false;
        }
        if (!IsExecutableAddress(next)) {
            char text[192];
            std::snprintf(text, sizeof(text),
                          "the forwarder at %p (%s) names %p, which is not executable memory",
                          reinterpret_cast<void*>(current), form, reinterpret_cast<void*>(next));
            out->Note = text;
            return false;
        }

        if (isHookPatch) {

            out->Shape = EntryShape::Chain;
            out->Body = current;
            out->Hops = hops;
            out->ChainDetour = next;
            out->ChainSlot = jumpSlot;
            std::memcpy(out->ChainBytes, code, sizeof(code));
            out->ChainJumpLength = jumpLength;
            return true;
        }

        if (hops >= kMaxForwarderHops) {
            char text[192];
            std::snprintf(text, sizeof(text),
                          "the entry is still a jump after %d forwarder(s), so the chain is a loop "
                          "rather than a way to a body",
                          hops);
            out->Note = text;
            return false;
        }

        current = next;
        ++hops;
    }
}

void DescribeAddressOwner(uintptr_t address, char* out, size_t capacity) {
    HMODULE module = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(address), &module) &&
        module) {
        char path[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
        if (length > 0) {
            const char* name = path;
            for (DWORD i = 0; i < length; ++i) {
                if (path[i] == '\\' || path[i] == '/') name = path + i + 1;
            }
            std::snprintf(out, capacity, "%s", name);
            return;
        }
    }

    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)) != sizeof(mbi)) {
        std::snprintf(out, capacity, "an address no module owns");
        return;
    }
    if (mbi.Type == MEM_MAPPED) {
        std::snprintf(out, capacity, "a mapped view at %p", mbi.AllocationBase);
        return;
    }
    std::snprintf(out, capacity, "an allocation at %p", mbi.AllocationBase);
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
    m_Entry = nullptr;
    m_ForwarderHops = 0;

    const ULONGLONG resolveStarted = GetTickCount64();
    EntryResolution entry;
    const uintptr_t entryAddress = reinterpret_cast<uintptr_t>(target);
    if (!ResolveEntry(entryAddress, &entry)) {
        return fail(entry.Note.empty() ? "what the entry runs could not be determined"
                                       : entry.Note.c_str());
    }

    if (entry.Shape == EntryShape::Chain) {
        if (entry.ChainJumpLength != kSlotJumpBytes || !entry.ChainSlot) {
            return fail("the existing hook does not use a six-byte external-slot jump");
        }
        if ((entry.ChainSlot % alignof(void*)) != 0) {
            return fail("the existing jump slot is not aligned for an atomic pointer exchange");
        }
        if (entry.ChainDetour == reinterpret_cast<uintptr_t>(detour)) {
            return fail("the existing jump slot already points to this detour");
        }
        if (SafeMem_Compare(reinterpret_cast<const void*>(entry.Body), entry.ChainBytes,
                            sizeof(entry.ChainBytes)) != 0) {
            return fail("the entry changed while resolving its jump slot");
        }

        auto slot = reinterpret_cast<void**>(entry.ChainSlot);
        void* const predecessor = reinterpret_cast<void*>(entry.ChainDetour);
        void* const previousOriginal = originalOut ? *originalOut : nullptr;
        void* const previousTrampoline = m_Trampoline;
        m_Trampoline = predecessor;
        if (originalOut) *originalOut = predecessor;

        const SlotExchangeResult exchanged = ExchangeSlotPointer(slot, predecessor, detour);
        if (!exchanged.Exchanged) {
            m_Trampoline = previousTrampoline;
            if (originalOut) *originalOut = previousOriginal;
            const char* failure = exchanged.Accessible ? "the existing jump slot changed before installation"
                                                       : "the existing jump slot could not be updated";
            if (exchanged.ProtectionError) {
                char text[256];
                std::snprintf(text, sizeof(text),
                              "%s; slot page protection restoration failed (Win32 %lu)",
                              failure, exchanged.ProtectionError);
                return fail(text);
            }
            return fail(failure);
        }

        m_Target = reinterpret_cast<void*>(entry.Body);
        m_Entry = reinterpret_cast<void*>(entryAddress);
        m_ForwarderHops = entry.Hops;
        m_Chained = true;
        m_ChainedSlot = slot;
        m_Detour = detour;
        m_StolenBytes = 0;
        m_Installed = true;

        char summary[384];
        std::snprintf(summary, sizeof(summary),
                      "chained through existing jump slot %p at %p; Original()=%p; "
                      "entry bytes unchanged, no allocation or thread suspension; %llu ms",
                      static_cast<void*>(slot), m_Target, predecessor,
                      static_cast<unsigned long long>(GetTickCount64() - resolveStarted));
        m_Summary = summary;
        if (exchanged.ProtectionError) {
            char warning[128];
            std::snprintf(warning, sizeof(warning),
                          "; slot page protection restoration failed (Win32 %lu)",
                          exchanged.ProtectionError);
            m_Summary += warning;
        }
        return true;
    }

    uint8_t* const start = reinterpret_cast<uint8_t*>(entry.Body);
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

    m_Target = start;
    m_Entry = reinterpret_cast<void*>(entryAddress);
    m_ForwarderHops = entry.Hops;
    m_Chained = entry.Shape == EntryShape::Chain;
    m_Detour = detour;
    m_StolenBytes = oldLength;
    m_Installed = true;

    const uintptr_t trampolineAddress = reinterpret_cast<uintptr_t>(trampoline);
    const uintptr_t distance = trampolineAddress > base ? trampolineAddress - base : base - trampolineAddress;
    char summary[512];
    const int written = std::snprintf(summary, sizeof(summary),
                  "displaced %zu byte(s) in %zu instruction(s), %zu re-addressed, %zu carried on a "
                  "register; trampoline %p (0x%llX from the target, reach beyond 2GB would corrupt: "
                  "%s); threads moved: %d",
                  oldLength, instructions.size(), reAddressed, carriedInRegister, trampoline,
                  static_cast<unsigned long long>(distance),
                  needsReach ? "required, and satisfied" : "not required", strandedMoved);
    m_Summary.assign(summary, written > 0 ? static_cast<size_t>(written) : 0);

    if (entry.Shape == EntryShape::Follow) {
        char owner[96] = {};
        DescribeAddressOwner(reinterpret_cast<uintptr_t>(start), owner, sizeof(owner));
        char forwarded[384];
        std::snprintf(forwarded, sizeof(forwarded),
                      "; the entry %p is a forwarder, followed %d hop(s) to the body at %p (%s), which "
                      "is what was patched - calls arriving through the entry still land on it",
                      reinterpret_cast<void*>(entryAddress), entry.Hops,
                      static_cast<void*>(start), owner);
        m_Summary += forwarded;
    }
    return true;
}

void InlineHook::Remove() {
    if (!m_Installed || !m_Target) return;

    DWORD protectionError = 0;
    if (m_ChainedSlot) {
        const SlotExchangeResult restored = ExchangeSlotPointer(m_ChainedSlot, m_Detour, m_Trampoline);
        if (!restored.Exchanged) {
            if (restored.ProtectionError) {
                char warning[160];
                std::snprintf(warning, sizeof(warning),
                              "jump slot removal deferred; page protection restoration failed (Win32 %lu)",
                              restored.ProtectionError);
                m_Summary = warning;
            }
            return;
        }
        protectionError = restored.ProtectionError;
    } else {
        DWORD previous = 0;
        if (VirtualProtect(m_Target, kJumpBytes, PAGE_EXECUTE_READWRITE, &previous)) {
            {
                ThreadFreeze freeze(m_Target, kJumpBytes, nullptr, nullptr);
                std::memcpy(m_Target, m_Saved, kJumpBytes);
            }
            VirtualProtect(m_Target, kJumpBytes, previous, &previous);
            FlushInstructionCache(GetCurrentProcess(), m_Target, kJumpBytes);
        }
    }

    m_Target = nullptr;
    m_Entry = nullptr;
    m_ForwarderHops = 0;
    m_Chained = false;
    m_Detour = nullptr;
    if (!m_ChainedSlot) m_Trampoline = nullptr;
    m_ChainedSlot = nullptr;
    m_StolenBytes = 0;
    m_Installed = false;
    m_Summary.clear();
    m_PlacementNotes.clear();
    if (protectionError) {
        char warning[128];
        std::snprintf(warning, sizeof(warning),
                      "jump slot restored, but page protection restoration failed (Win32 %lu)",
                      protectionError);
        m_Summary = warning;
    }
}

}
