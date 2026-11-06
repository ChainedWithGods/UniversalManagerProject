#pragma once

#include <windows.h>

namespace OverlayIntegration {
void SetMainModule(HMODULE module);
bool Start();
void Stop(bool processExiting = false);
}
