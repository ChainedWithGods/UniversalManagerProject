#pragma once

#include <cstdint>
#include <string>
#include <windows.h>

struct RuntimeFeatureConfig {
    bool EnableInGameGui = false;
    bool EnableUnrealConsole = false;
    bool EnableWndProcHook = true;
    bool DumpAllDeveloperCommands = false;

    bool EnableSDKValidation = false;

    std::string SDKPath;

    bool GenerateBuildSystem = false;

    std::string BuildScriptPath;

    bool EnableSDKLibclangGeneration = false;

    std::string LibclangPath;

    bool SkipPatternScan = false;

    bool EnableDiscoveryCache = false;

    bool VerboseLog = false;

    std::string GraphicsBackend = "auto";

    bool EnableLuaMods = false;
    std::string LuaModsPath;

    bool EnableCppMods = false;
    std::string CppModsPath;

    bool AutoDiscoverAESKey = false;

    unsigned int ServerPort = 20030;

    unsigned int InitDelayMinutes = 0;
    unsigned int InitDelaySeconds = 0;
    std::string FilePath;
};

namespace RuntimeConfig {
const RuntimeFeatureConfig& Load(HMODULE module);
const RuntimeFeatureConfig& Get();
}
