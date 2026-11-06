#pragma once

#include "../core/Types.h"
#include "sdk/Avalonium/ModApi.h"

#include <cstdint>
#include <string>

namespace Avalonium::CppMods
{

    uint32_t RegisterHook(EngineState& state, const std::string& className,
                          const std::string& functionName, AvaloniumHookCallback pre,
                          AvaloniumHookCallback post, void* userData, void* owner,
                          std::string* why);

    uint32_t RegisterHookByFunction(EngineState& state, UFunction* function,
                                    AvaloniumHookCallback pre, AvaloniumHookCallback post,
                                    void* userData, void* owner, std::string* why);

    bool UnregisterHook(uint32_t hookId);
    void UnregisterHooksForOwner(void* owner);

    void DispatchScriptFunction(void* context, void* frame, void* result, bool pre);

    bool WaitForHooksIdle(int timeoutMs);

    void ShutdownHooks();
}
