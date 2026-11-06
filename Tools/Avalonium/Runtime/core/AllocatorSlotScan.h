#pragma once

#include <cstdint>

namespace EngineDiscovery {

struct AllocatorCallSiteVote {
  int slot = -1;
  bool constantAlignment = false;
};

bool AnalyzeAllocatorCallSite(uintptr_t scan, uintptr_t end, int reg,
                              AllocatorCallSiteVote &vote);

}
