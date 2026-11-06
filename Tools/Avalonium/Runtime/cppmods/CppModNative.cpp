#include "CppModNative.h"

#include "../core/PatternScanner.h"
#include "../utils/SafeMemory.h"

#include <windows.h>

namespace Avalonium::CppMods::Native
{
    namespace
    {
        HMODULE ResolveModule(void* module)
        {
            if (module) return static_cast<HMODULE>(module);
            return GetModuleHandleA(nullptr);
        }

        constexpr int32_t kMaxVtableSlot = 0x1000;
    }

    void* ModuleHandle(const char* moduleName)
    {
        if (!moduleName || !*moduleName) return GetModuleHandleA(nullptr);
        return GetModuleHandleA(moduleName);
    }

    void* FindExport(void* module, const char* symbolName)
    {
        if (!symbolName || !*symbolName) return nullptr;
        HMODULE handle = ResolveModule(module);
        if (!handle) return nullptr;
        return PatternScan::FindExport(handle, symbolName);
    }

    uint64_t FindPattern(void* module, const char* pattern, bool executableOnly)
    {
        if (!pattern || !*pattern) return 0;
        HMODULE handle = ResolveModule(module);
        if (!handle) return 0;
        const uintptr_t found =
            executableOnly ? PatternScan::FindPatternInExecutableSections(handle, pattern)
                           : PatternScan::FindPatternInModule(handle, pattern);
        return static_cast<uint64_t>(found);
    }

    uint64_t ResolveRelative(uint64_t instruction, int32_t displacementOffset,
                             int32_t instructionLength)
    {
        if (!instruction) return 0;
        int32_t displacement = 0;
        if (!SafeMem_Read(reinterpret_cast<const void*>(
                              static_cast<uintptr_t>(instruction) + displacementOffset),
                          &displacement, sizeof(displacement)))
        {
            return 0;
        }
        return PatternScan::ResolveRelative(static_cast<uintptr_t>(instruction), displacement,
                                            instructionLength);
    }

    void* GetVtable(void* object)
    {
        if (!object) return nullptr;
        void* vtable = nullptr;
        if (!SafeMem_Read(object, &vtable, sizeof(vtable)) || !vtable) return nullptr;
        return vtable;
    }

    void* GetVirtualFunction(void* object, int32_t slot)
    {
        if (slot < 0 || slot > kMaxVtableSlot) return nullptr;
        void* vtable = GetVtable(object);
        if (!vtable) return nullptr;

        void* function = nullptr;
        if (!SafeMem_Read(reinterpret_cast<uint8_t*>(vtable) +
                              static_cast<size_t>(slot) * sizeof(void*),
                          &function, sizeof(function)))
        {
            return nullptr;
        }
        return function;
    }
}
