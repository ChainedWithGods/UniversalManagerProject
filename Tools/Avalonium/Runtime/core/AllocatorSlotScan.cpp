#include "AllocatorSlotScan.h"

#include "../../Helpers/minhook/src/hde/hde64.h"

#include <algorithm>

namespace EngineDiscovery {
namespace {

constexpr int kMaxInstructions = 24;
constexpr int kMaxBranchDepth = 2;
constexpr uintptr_t kMaxSpanBytes = 256;
constexpr int64_t kMaxAlignmentConstant = 0x1000;
constexpr int kTagManager = 1;
constexpr int kTagVtable = 2;

struct WalkState {
  int tags[16] = {};
  bool constantAlignment = false;
  bool alignmentRegisterSet = false;
};

int BaseRegister(const hde64s &d, int rexB) {
  if (d.modrm_rm == 4 && (d.flags & F_SIB) && d.sib_index == 4)
    return d.sib_base | (rexB ? 8 : 0);
  return d.modrm_rm | (rexB ? 8 : 0);
}

bool WrittenRegister(const hde64s &d, int rexR, int rexB, int &dest) {
  if (d.flags & F_MODRM) {
    switch (d.opcode) {
      case 0x8B:
      case 0x8D:
      case 0x03:
      case 0x0B:
      case 0x13:
      case 0x1B:
      case 0x23:
      case 0x2B:
      case 0x33:
      case 0x3B:
        dest = d.modrm_reg | (rexR ? 8 : 0);
        return true;
      case 0x89:
      case 0xC7:
      case 0x01:
      case 0x09:
      case 0x11:
      case 0x19:
      case 0x21:
      case 0x29:
      case 0x31:
      case 0x39:
        if (d.modrm_mod == 3) {
          dest = d.modrm_rm | (rexB ? 8 : 0);
          return true;
        }
        return false;
      case 0x0F:
        if (d.opcode2 == 0xB6 || d.opcode2 == 0xB7 || d.opcode2 == 0xBE ||
            d.opcode2 == 0xBF) {
          dest = d.modrm_reg | (rexR ? 8 : 0);
          return true;
        }
        return false;
      default:
        break;
    }
  }
  if (d.opcode >= 0xB8 && d.opcode <= 0xBF) {
    dest = (d.opcode & 0x07) | (rexB ? 8 : 0);
    return true;
  }
  if (d.opcode >= 0x58 && d.opcode <= 0x5F) {
    dest = (d.opcode & 0x07) | (rexB ? 8 : 0);
    return true;
  }
  return false;
}

bool Walk(uintptr_t pc, uintptr_t start, uintptr_t limit, int budget,
          int depth, WalkState state, AllocatorCallSiteVote &out);

bool FollowBranch(uintptr_t target, uintptr_t start, uintptr_t limit,
                  int budget, int depth, const WalkState &state,
                  AllocatorCallSiteVote &out) {
  if (depth >= kMaxBranchDepth || target < start || target >= limit)
    return false;
  return Walk(target, start, limit, budget, depth + 1, state, out);
}

bool Walk(uintptr_t pc, uintptr_t start, uintptr_t limit, int budget,
          int depth, WalkState state, AllocatorCallSiteVote &out) {
  while (budget-- > 0) {
    if (pc + 16 > limit)
      return false;

    hde64s d = {};
    if (!hde64_disasm(reinterpret_cast<const void *>(pc), &d) || d.len == 0 ||
        (d.flags & F_ERROR) || pc + d.len > limit)
      return false;

    const int rexB = d.rex_b ? 1 : 0;
    const int rexR = d.rex_r ? 1 : 0;
    const bool rexW = d.rex_w != 0;

    if (d.opcode == 0xFF && (d.flags & F_MODRM) &&
        (d.modrm_reg == 2 || d.modrm_reg == 4)) {
      if (d.modrm_mod == 3)
        return false;
      const int base = BaseRegister(d, rexB);
      int64_t disp = 0;
      if (d.modrm_mod == 1)
        disp = static_cast<int8_t>(d.disp.disp8);
      else if (d.modrm_mod == 2)
        disp = static_cast<int32_t>(d.disp.disp32);
      if (state.tags[base] == kTagVtable) {
        if (disp >= 0 && (disp % 8) == 0 && (disp / 8) < 32) {
          out.slot = static_cast<int>(disp / 8);
          out.constantAlignment =
              state.constantAlignment && !state.alignmentRegisterSet;
          return true;
        }
      }
      return false;
    }

    if (d.opcode == 0xE8)
      return false;
    if (d.opcode == 0xE9 || d.opcode == 0xEB) {
      const int64_t rel = d.opcode == 0xE9
                              ? static_cast<int32_t>(d.imm.imm32)
                              : static_cast<int8_t>(d.imm.imm8);
      return FollowBranch(pc + d.len + rel, start, limit, budget, depth, state,
                          out);
    }
    if (d.opcode == 0xC3 || d.opcode == 0xC2)
      return false;
    if (d.opcode >= 0x70 && d.opcode <= 0x7F) {
      const int64_t rel = static_cast<int8_t>(d.imm.imm8);
      if (FollowBranch(pc + d.len + rel, start, limit, budget, depth, state, out))
        return true;
      pc += d.len;
      continue;
    }
    if (d.opcode == 0x0F && d.opcode2 >= 0x80 && d.opcode2 <= 0x8F) {
      const int64_t rel = static_cast<int32_t>(d.imm.imm32);
      if (FollowBranch(pc + d.len + rel, start, limit, budget, depth, state, out))
        return true;
      pc += d.len;
      continue;
    }

    const int destR8 = 8;
    if ((d.opcode == 0x31 || d.opcode == 0x33) && d.modrm_mod == 3 &&
        (d.modrm_rm | (rexB ? 8 : 0)) == (d.modrm_reg | (rexR ? 8 : 0))) {
      const int reg = d.modrm_rm | (rexB ? 8 : 0);
      if (reg == destR8)
        state.constantAlignment = true;
      if (reg == 9)
        state.alignmentRegisterSet = true;
    }
    if (d.opcode >= 0xB8 && d.opcode <= 0xBF &&
        ((d.opcode & 0x07) | (rexB ? 8 : 0)) == destR8) {
      const int64_t value = rexW ? static_cast<int64_t>(d.imm.imm64)
                                 : static_cast<int64_t>(d.imm.imm32);
      if (value >= 0 && value <= kMaxAlignmentConstant)
        state.constantAlignment = true;
    }
    if (d.opcode == 0xC7 && d.modrm_mod == 3 &&
        (d.modrm_rm | (rexB ? 8 : 0)) == destR8) {
      const int64_t value = static_cast<int32_t>(d.imm.imm32);
      if (value >= 0 && value <= kMaxAlignmentConstant)
        state.constantAlignment = true;
    }

    int dest = -1;
    if (d.opcode == 0x8B && rexW && d.modrm_mod == 0) {
      const int base = BaseRegister(d, rexB);
      if (state.tags[base] == kTagManager)
        state.tags[d.modrm_reg | (rexR ? 8 : 0)] = kTagVtable;
      else
        state.tags[d.modrm_reg | (rexR ? 8 : 0)] = 0;
    } else if (d.opcode == 0x8B && d.modrm_mod == 3) {
      state.tags[d.modrm_reg | (rexR ? 8 : 0)] =
          state.tags[d.modrm_rm | (rexB ? 8 : 0)];
    } else if (WrittenRegister(d, rexR, rexB, dest)) {
      if (dest >= 0 && dest < 16)
        state.tags[dest] = 0;
      if (dest == 9)
        state.alignmentRegisterSet = true;
    }

    pc += d.len;
  }
  return false;
}

}

bool AnalyzeAllocatorCallSite(uintptr_t scan, uintptr_t end, int reg,
                              AllocatorCallSiteVote &vote) {
  vote.slot = -1;
  vote.constantAlignment = false;
  if (reg < 0 || reg >= 16)
    return false;

  const uintptr_t limit = (std::min)(end, scan + kMaxSpanBytes);
  WalkState state;
  state.tags[reg] = kTagManager;

  return Walk(scan + 7, scan, limit, kMaxInstructions, 0, state, vote);
}

}
