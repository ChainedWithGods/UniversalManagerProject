#pragma once
#include "Types.h"

namespace ConsoleEnabler {
    bool EnableInGameConsole(const EngineState& state);
    void DisableInGameConsole(const EngineState& state);
    bool TryOpenConsole(const EngineState& state);
    void InstallWndProcHook(EngineState& state);
    void RemoveWndProcHook(EngineState& state);

    void Prewarm(const EngineState& state);
    bool IsVirtualKeyHeldWndProc(int vk);
}

namespace runtimeinput {
    bool IsConsoleEnabled();
    bool ShouldReleaseMouseCapture();
    bool TryOpenConsole();
}
