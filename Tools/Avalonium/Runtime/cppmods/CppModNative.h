#pragma once

#include "../core/Types.h"

#include <cstdint>

namespace Avalonium::CppMods::Native
{
    void* ModuleHandle(const char* moduleName);

    void* FindExport(void* module, const char* symbolName);

    uint64_t FindPattern(void* module, const char* pattern, bool executableOnly);

    uint64_t ResolveRelative(uint64_t instruction, int32_t displacementOffset,
                             int32_t instructionLength);

    void* GetVtable(void* object);
    void* GetVirtualFunction(void* object, int32_t slot);
}
