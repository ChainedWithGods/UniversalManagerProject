#include "RuntimeConfig.h"

#include "EngineDiscovery.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

namespace {
RuntimeFeatureConfig g_Config;
bool g_ConfigLoaded = false;

struct IniEntry {
    std::string key;
    std::string defaultValue;
    std::vector<std::string> comments;
};

const std::vector<IniEntry>& GetIniSchema() {
    static const std::vector<IniEntry> schema = {
        { "InGameGUI", "false",
          { "Enable/Disable InGame Overlay" } },
        { "UnrealEngineConsole", "false",
          { "Disable/Enable Unreal Engine Console" } },
        { "SkipPatternScan", "false",
          { "Skip the default GUObjectArray AOB pattern scan and go straight to the obfuscation scan.",
            "Useful for heavily obfuscated / protected games where the pattern scan always times out (~5s)." } },
        { "EnableDiscoveryCache", "false",
          { "Enable encrypted discovery cache system to accelerate initialization time." } },
        { "VerboseLog", "false",
          { "Enable verbose internal-trace logging (Discovery [Debug] probes, layout heuristics, retry/fallback messages).",
            "When false the log is filtered down to high-level lifecycle and discovery results." } },
        { "GraphicsBackend", "auto",
          { "Rendering backend for the overlay: auto, dx12, dx11" } },
        { "DumpAllDeveloperCommands", "false",
          { "Dump all developer commands and modules." } },
        { "SDKValidation", "false",
          { "Run the advanced SDK validation (startup + post-dump requests for the same path are merged)" } },
        { "SDKPath", "",
          { "Optional folder containing SDKMetadata.json for startup validation." } },
        { "GenerateBuildSystem", "false",
          { "Automatically run GenerateBuild.py after dumping the SDK" } },
        { "BuildScriptPath", "",
          { "Path to GenerateBuild.py. Leave empty to auto-discover from the bundled Helpers folder." } },
        { "SDKLibclangGeneration", "false",
          { "Run the libclang AST generation + verification pipeline after SDK dumping." } },
        { "LibclangPath", "",
          { "Full path to libclang.dll. Leave empty to auto-discover Visual Studio LLVM or libclang package." } },
        { "EnableLuaMods", "false",
          { "Enable Lua scripting and mod loader runtime." } },
        { "LuaModsPath", "",
          { "Custom directory for Lua mods. Leave empty for default 'Mods/Avalonium' folder next to Avalonium.dll." } },
        { "EnableCppMods", "false",
          { "Enable native C++ mods. A mod is a folder under the mods directory with dlls/main.dll or dlls/<ModName>.dll exporting start_mod and uninstall_mod." } },
        { "CppModsPath", "",
          { "Custom directory for native C++ mods. Leave empty for default 'Mods/Avalonium' folder next to Avalonium.dll." } },
        { "AutoDiscoverAESKey", "false",
          { "Auto-discover and verify AES-256 encryption key from live memory at startup." } },
        { "ServerPort", "20030",
          { "IPC / IOCP server port for tool communication.",
            "Used to enable external tools to communicate with Avalonium (Universal Manager, uses 20030 by default)." } },
        { "InitDelayMinutes", "0",
          { "Set Initialization Delay in minutes" } },
        { "InitDelaySeconds", "0",
          { "Set Initialization Delay in seconds" } },
    };
    return schema;
}

std::string TrimWhitespace(const std::string& s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return std::string();
    }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string GetConfigPath(HMODULE module) {
    if (!module) {
        return "Avalonium.ini";
    }

    char dllPath[MAX_PATH] = {};
    if (!GetModuleFileNameA(module, dllPath, MAX_PATH)) {
        return "Avalonium.ini";
    }

    std::string configPath(dllPath);
    const size_t separator = configPath.find_last_of("\\/");
    if (separator == std::string::npos) {
        return "Avalonium.ini";
    }

    return configPath.substr(0, separator + 1) + "Avalonium.ini";
}

std::string FindHelpersDirectory(HMODULE module) {
    if (!module) {
        return {};
    }

    char dllPath[MAX_PATH] = {};
    if (!GetModuleFileNameA(module, dllPath, MAX_PATH)) {
        return {};
    }

    std::string current(dllPath);
    const size_t separator = current.find_last_of("\\/");
    if (separator == std::string::npos) {
        return {};
    }
    current.resize(separator);

    for (int depth = 0; depth < 6; ++depth) {
        const std::string candidate = current + "\\Helpers";
        const DWORD attr = GetFileAttributesA(candidate.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            return candidate + "\\";
        }

        const size_t parent = current.find_last_of("\\/");
        if (parent == std::string::npos || parent == 0) {
            break;
        }
        current.resize(parent);
    }
    return {};
}

std::string ResolveScriptPath(const std::string& userValue,
                              const std::string& helpersDir,
                              const char* scriptName) {
    if (!userValue.empty()) {
        return userValue;
    }
    if (helpersDir.empty() || scriptName == nullptr) {
        return {};
    }

    const std::string candidate = helpersDir + scriptName;
    const DWORD attr = GetFileAttributesA(candidate.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return {};
    }
    return candidate;
}

std::string TrimCopy(std::string value) {
    const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char ch) {
        return !isSpace(ch);
    }));

    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char ch) {
        return !isSpace(ch);
    }).base(), value.end());

    return value;
}

bool ParseBoolValue(const std::string& value, bool fallback) {
    std::string normalized = TrimCopy(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") {
        return true;
    }

    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") {
        return false;
    }

    return fallback;
}

unsigned int ParseUIntValue(const std::string& value, unsigned int fallback) {
    std::string normalized = TrimCopy(value);
    if (normalized.empty()) {
        return fallback;
    }
    try {

        const unsigned long parsed = std::stoul(normalized, nullptr, 0);
        return static_cast<unsigned int>(parsed);
    } catch (...) {
        return fallback;
    }
}

void EnsureDefaultConfigExists(const std::string& configPath) {
    const DWORD attributes = GetFileAttributesA(configPath.c_str());
    const bool fileExists =
        (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);

    const std::vector<IniEntry>& schema = GetIniSchema();
    char aggressiveEnabled[16] = {};
    char aggressiveLogDirectory[512] = {};
    char aggressiveDumpRegisters[16] = {};
    char aggressiveCatchFastFail[16] = {};
    char aggressiveTerminationBreakpoint[16] = {};
    char aggressiveAttachDebugger[16] = {};
    char aggressiveMaxStackDepth[16] = {};
    char aggressiveOtherThreadStackDepth[16] = {};
    char aggressiveWriteMiniDump[16] = {};
    GetPrivateProfileStringA("AggressiveDog", "Enabled", "false", aggressiveEnabled, sizeof(aggressiveEnabled), configPath.c_str());
    GetPrivateProfileStringA("AggressiveDog", "LogDirectory", "CrashHandler/Crashes", aggressiveLogDirectory, sizeof(aggressiveLogDirectory), configPath.c_str());
    GetPrivateProfileStringA("AggressiveDog", "DumpRegisters", "true", aggressiveDumpRegisters, sizeof(aggressiveDumpRegisters), configPath.c_str());
    GetPrivateProfileStringA("AggressiveDog", "CatchFastFail", "true", aggressiveCatchFastFail, sizeof(aggressiveCatchFastFail), configPath.c_str());
    GetPrivateProfileStringA("AggressiveDog", "TerminationBreakpoint", "true", aggressiveTerminationBreakpoint, sizeof(aggressiveTerminationBreakpoint), configPath.c_str());
    GetPrivateProfileStringA("AggressiveDog", "AttachDebugger", "false", aggressiveAttachDebugger, sizeof(aggressiveAttachDebugger), configPath.c_str());
    GetPrivateProfileStringA("AggressiveDog", "MaxStackDepth", "64", aggressiveMaxStackDepth, sizeof(aggressiveMaxStackDepth), configPath.c_str());
    GetPrivateProfileStringA("AggressiveDog", "OtherThreadStackDepth", "8", aggressiveOtherThreadStackDepth, sizeof(aggressiveOtherThreadStackDepth), configPath.c_str());
    GetPrivateProfileStringA("AggressiveDog", "WriteMiniDump", "true", aggressiveWriteMiniDump, sizeof(aggressiveWriteMiniDump), configPath.c_str());
    std::map<std::string, std::string> existing;
    if (fileExists) {
        std::ifstream in(configPath);
        if (in.is_open()) {
            std::string line;
            while (std::getline(in, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                const std::string trimmed = TrimWhitespace(line);
                if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '[') {
                    continue;
                }
                const auto eq = trimmed.find('=');
                if (eq == std::string::npos) {
                    continue;
                }
                const std::string key = TrimWhitespace(trimmed.substr(0, eq));
                const std::string value = TrimWhitespace(trimmed.substr(eq + 1));

                for (const auto& entry : schema) {
                    if (entry.key == key) {
                        existing[key] = value;
                        break;
                    }
                }
            }
        }
    }

    if (FILE* file = fopen(configPath.c_str(), "w")) {
        fputs(";Avalonium runtime configuration\n", file);
        fputs("\n", file);

        for (const auto& entry : schema) {
            std::string value;
            const auto it = existing.find(entry.key);
            if (it != existing.end()) {
                value = it->second;
            } else {
                value = entry.defaultValue;
            }

            fprintf(file, "%s=%s\n", entry.key.c_str(), value.c_str());
            for (const auto& comment : entry.comments) {
                fprintf(file, ";%s\n", comment.c_str());
            }
            fputs("\n", file);
        }
        fclose(file);
    }

    if (FILE* file = fopen(configPath.c_str(), "a")) {
        fputs("\n[AggressiveDog]\n"
              ";External crash watchdog for Avalonium. When enabled, Avalonium launches\n"
              ";CrashHandler\\AggressiveDog.exe beside Avalonium.dll to monitor this process.\n"
              ";It records crash details for diagnosis; it does not prevent or fix a crash.\n"
              ";A successful normal exit does not create a crash report.\n"
              ";Enable or disable the watchdog.\n",
              file);
        fprintf(file, "Enabled=%s\n"
                ";Folder for watchdog text reports, status log, and optional minidumps.\n"
                "LogDirectory=%s\n"
                ";Include the faulting thread's CPU registers in the text report.\n"
                "DumpRegisters=%s\n"
                ";Capture Windows fast-fail events in addition to standard fatal exceptions.\n"
                "CatchFastFail=%s\n"
                ";Arm a hardware breakpoint on NtTerminateProcess so abnormal terminations are caught.\n"
                ";Disable when a protected game treats debug registers as tampering.\n"
                "TerminationBreakpoint=%s\n"
                ";Attach as a debugger for exception-level crash reports. Off by default because\n"
                ";protected games can treat an attached debugger as tampering and terminate.\n"
                "AttachDebugger=%s\n"
                ";Maximum call-stack frames for the faulting thread (1-256).\n"
                "MaxStackDepth=%s\n"
                ";Maximum stack frames per other thread (0-64; 0 disables them).\n"
                "OtherThreadStackDepth=%s\n"
                ";Write a minidump alongside the text report (can use substantial disk space).\n"
                "WriteMiniDump=%s\n",
                aggressiveEnabled, aggressiveLogDirectory, aggressiveDumpRegisters, aggressiveCatchFastFail,
                aggressiveTerminationBreakpoint, aggressiveAttachDebugger, aggressiveMaxStackDepth, aggressiveOtherThreadStackDepth, aggressiveWriteMiniDump);
        fclose(file);
    }
}

bool TryReadRootLevelBoolSetting(const std::string& configPath,
                                 std::initializer_list<const char*> keys,
                                 bool& outValue) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = TrimCopy(line);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') {
            continue;
        }

        if (trimmed.front() == '[') {
            continue;
        }

        const size_t equalsPos = trimmed.find('=');
        if (equalsPos == std::string::npos) {
            continue;
        }

        const std::string key = TrimCopy(trimmed.substr(0, equalsPos));
        const std::string value = trimmed.substr(equalsPos + 1);

        for (const char* expectedKey : keys) {
            if (_stricmp(key.c_str(), expectedKey) == 0) {
                outValue = ParseBoolValue(value, outValue);
                return true;
            }
        }
    }

    return false;
}

bool ReadBoolSettingWithAliases(const std::string& configPath,
                                const char* section,
                                std::initializer_list<const char*> keys,
                                bool fallback) {
    for (const char* key : keys) {
        char buffer[32] = {};
        GetPrivateProfileStringA(section, key, "", buffer, static_cast<DWORD>(sizeof(buffer)), configPath.c_str());
        if (buffer[0] != '\0') {
            return ParseBoolValue(buffer, fallback);
        }
    }

    bool rootLevelValue = fallback;
    if (TryReadRootLevelBoolSetting(configPath, keys, rootLevelValue)) {
        return rootLevelValue;
    }

    return fallback;
}

bool TryReadRootLevelStringSetting(const std::string& configPath,
                                   std::initializer_list<const char*> keys,
                                   std::string& outValue) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = TrimCopy(line);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') {
            continue;
        }

        if (trimmed.front() == '[') {
            continue;
        }

        const size_t equalsPos = trimmed.find('=');
        if (equalsPos == std::string::npos) {
            continue;
        }

        const std::string key = TrimCopy(trimmed.substr(0, equalsPos));
        const std::string value = TrimCopy(trimmed.substr(equalsPos + 1));

        for (const char* expectedKey : keys) {
            if (_stricmp(key.c_str(), expectedKey) == 0) {
                outValue = value;
                return true;
            }
        }
    }

    return false;
}

std::string ReadStringSettingWithAliases(const std::string& configPath,
                                         const char* section,
                                         std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        char buffer[2048] = {};
        GetPrivateProfileStringA(section, key, "", buffer, static_cast<DWORD>(sizeof(buffer)), configPath.c_str());
        const std::string value = TrimCopy(buffer);
        if (!value.empty()) {
            return value;
        }
    }

    std::string rootLevelValue;
    if (TryReadRootLevelStringSetting(configPath, keys, rootLevelValue)) {
        return rootLevelValue;
    }

    return {};
}

bool TryReadRootLevelUIntSetting(const std::string& configPath,
                                 std::initializer_list<const char*> keys,
                                 unsigned int& outValue) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = TrimCopy(line);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') {
            continue;
        }

        if (trimmed.front() == '[') {
            continue;
        }

        const size_t equalsPos = trimmed.find('=');
        if (equalsPos == std::string::npos) {
            continue;
        }

        const std::string key = TrimCopy(trimmed.substr(0, equalsPos));
        const std::string value = trimmed.substr(equalsPos + 1);

        for (const char* expectedKey : keys) {
            if (_stricmp(key.c_str(), expectedKey) == 0) {
                outValue = ParseUIntValue(value, outValue);
                return true;
            }
        }
    }

    return false;
}

unsigned int ReadUIntSettingWithAliases(const std::string& configPath,
                                        const char* section,
                                        std::initializer_list<const char*> keys,
                                        unsigned int fallback) {
    for (const char* key : keys) {
        char buffer[32] = {};
        GetPrivateProfileStringA(section, key, "", buffer, static_cast<DWORD>(sizeof(buffer)), configPath.c_str());
        if (buffer[0] != '\0') {
            return ParseUIntValue(buffer, fallback);
        }
    }

    unsigned int rootLevelValue = fallback;
    if (TryReadRootLevelUIntSetting(configPath, keys, rootLevelValue)) {
        return rootLevelValue;
    }

    return fallback;
}
}

namespace RuntimeConfig {
const RuntimeFeatureConfig& Load(HMODULE module) {
    if (g_ConfigLoaded) {
        return g_Config;
    }
    EngineDiscovery::SetLogSection("config");

    g_ConfigLoaded = true;
    g_Config = {};
    g_Config.FilePath = GetConfigPath(module);

    EnsureDefaultConfigExists(g_Config.FilePath);

    const std::string helpersDir = FindHelpersDirectory(module);

    g_Config.EnableInGameGui = ReadBoolSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "InGameGUI", "IsInGameGUI" },
        false);

    g_Config.EnableUnrealConsole = ReadBoolSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "UnrealEngineConsole", "IsUnrealEngineConsole", "UEConsole" },
        false);

    g_Config.DumpAllDeveloperCommands = ReadBoolSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "DumpAllDeveloperCommands", "DumpAllDeveloperCommand", "DumpAllCommands" },
        false);

    g_Config.EnableWndProcHook = true;

    g_Config.EnableSDKValidation = ReadBoolSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "SDKValidation", "EnableSDKValidation", "RunSDKValidation" },
        false);

    g_Config.SDKPath = ReadStringSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "SDKPath", "ValidationSDKPath" });

    g_Config.GenerateBuildSystem = ReadBoolSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "GenerateBuildSystem", "RunGenerateBuild" },
        false);

    g_Config.BuildScriptPath = ResolveScriptPath(
        ReadStringSettingWithAliases(
            g_Config.FilePath,
            "Features",
            { "BuildScriptPath", "GenerateBuildScriptPath" }),
        helpersDir,
        "GenerateBuild.py");

    g_Config.EnableSDKLibclangGeneration = ReadBoolSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "SDKLibclangGeneration", "EnableSDKLibclangGeneration", "RunSDKLibclangGeneration" },
        false);

    g_Config.LibclangPath = ReadStringSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "LibclangPath", "LibClangPath", "SDKLibclangPath" });

    g_Config.SkipPatternScan = ReadBoolSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "SkipPatternScan", "SkipGUObjectArrayPatternScan", "UseObfuscationScan" },
        false);

    g_Config.EnableDiscoveryCache = ReadBoolSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "EnableDiscoveryCache", "DiscoveryCache", "UseDiscoveryCache", "EnableSnapshotCache" },
        false);

    g_Config.VerboseLog = ReadBoolSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "VerboseLog", "EnableVerboseLog", "VerboseLogging", "EnableVerboseLogging" },
        false);
    EngineDiscovery::SetRuntimeVerboseLogging(g_Config.VerboseLog);

    g_Config.GraphicsBackend = ReadStringSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "GraphicsBackend", "PreferredBackend", "Backend", "Renderer" });
    if (g_Config.GraphicsBackend.empty()) {
        g_Config.GraphicsBackend = "auto";
    }

    g_Config.EnableLuaMods = ReadBoolSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "EnableLuaMods", "LuaMods", "Lua" },
        true);

    g_Config.LuaModsPath = ReadStringSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "LuaModsPath", "ModsPath", "LuaPath" });

    g_Config.EnableCppMods = ReadBoolSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "EnableCppMods", "CppMods", "NativeMods" },
        true);

    g_Config.CppModsPath = ReadStringSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "CppModsPath", "CppModPath", "NativeModsPath" });

    g_Config.AutoDiscoverAESKey = ReadBoolSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "AutoDiscoverAESKey", "FindAESKey", "AutoAESKey", "DiscoverAESKey" },
        true);

    g_Config.ServerPort = ReadUIntSettingWithAliases(
        g_Config.FilePath,
        "Features",
        { "ServerPort", "IPCServerPort", "IOCPPort", "Port" },
        20030);

    g_Config.InitDelayMinutes = ReadUIntSettingWithAliases(
        g_Config.FilePath,
        "Init",
        { "InitDelayMinutes", "InitDelayMins", "InitDelayMin", "InitDelayMinute" },
        0);

    g_Config.InitDelaySeconds = ReadUIntSettingWithAliases(
        g_Config.FilePath,
        "Init",
        { "InitDelaySeconds", "InitDelaySecs", "InitDelaySec", "InitDelaySecond" },
        0);

    constexpr unsigned int kMaxTotalSeconds = 60 * 60;
    unsigned int totalSeconds =
        g_Config.InitDelayMinutes * 60u + g_Config.InitDelaySeconds;
    if (totalSeconds > kMaxTotalSeconds) {
        totalSeconds = kMaxTotalSeconds;
        g_Config.InitDelayMinutes = totalSeconds / 60u;
        g_Config.InitDelaySeconds = totalSeconds % 60u;
    }const char* buildScriptSource = g_Config.BuildScriptPath.empty()
    ? "(unset)"
    : (!g_Config.BuildScriptPath.empty() && g_Config.BuildScriptPath == helpersDir + "GenerateBuild.py")
          ? "(auto)"
          : "(set)";

    char message[1024] = {};
    snprintf(message, sizeof(message),
             "[Avalonium] Loaded runtime config from %s\n",
             g_Config.FilePath.c_str());
    EngineDiscovery::WriteLogLine(message);

    auto writeSetting = [](const char *key, const char *value) {
        char line[1024] = {};
        snprintf(line, sizeof(line), "[Config] %s: %s\n", key, value);
        EngineDiscovery::WriteLogLine(line);
    };
    const char *on = "enabled";
    const char *off = "disabled";
    char value[1024] = {};

    writeSetting("in-game overlay", g_Config.EnableInGameGui ? on : off);
    writeSetting("Unreal Engine console", g_Config.EnableUnrealConsole ? on : off);
    writeSetting("Lua mod runtime", g_Config.EnableLuaMods ? on : off);
    writeSetting("C++ mod runtime", g_Config.EnableCppMods ? on : off);
    writeSetting("in-game console command dump", g_Config.DumpAllDeveloperCommands ? on : off);
    writeSetting("SDK validation", g_Config.EnableSDKValidation ? on : off);
    writeSetting("SDK libclang generation", g_Config.EnableSDKLibclangGeneration ? on : off);
    writeSetting("SDK header build script", buildScriptSource);
    writeSetting("AES key auto-discovery", g_Config.AutoDiscoverAESKey ? on : off);
    writeSetting("discovery cache", g_Config.EnableDiscoveryCache ? on : off);

    writeSetting("graphics backend", g_Config.GraphicsBackend.c_str());
    writeSetting("pattern scan", g_Config.SkipPatternScan ? "skipped" : "enabled");
    writeSetting("verbose log", g_Config.VerboseLog ? on : off);
    snprintf(value, sizeof(value), "%u m %u s", g_Config.InitDelayMinutes,
             g_Config.InitDelaySeconds);
    writeSetting("initialisation delay", value);
    snprintf(value, sizeof(value), "%u", g_Config.ServerPort);
    writeSetting("IPC server port", value);
    writeSetting("SDK validation path", g_Config.SDKPath.empty() ? "(unset)" : g_Config.SDKPath.c_str());
    writeSetting("Lua mods directory",
                 g_Config.LuaModsPath.empty() ? "(default)" : g_Config.LuaModsPath.c_str());
    writeSetting("C++ mods directory",
                 g_Config.CppModsPath.empty() ? "(default)" : g_Config.CppModsPath.c_str());
    writeSetting("libclang",
                 g_Config.LibclangPath.empty() ? "(auto-discover at runtime)"
                                               : g_Config.LibclangPath.c_str());

    if (g_Config.VerboseLog) {
        EngineDiscovery::WriteLogLine(
            "[Avalonium] Verbose logging active via Avalonium.ini\n");
    }

    if (!helpersDir.empty()) {
        char helperMsg[1024] = {};
        snprintf(helperMsg, sizeof(helperMsg),
                 "[Avalonium] Auto-discovered Helpers folder at %s\n", helpersDir.c_str());
        EngineDiscovery::WriteLogLine(helperMsg);
    } else {
        EngineDiscovery::WriteLogLine(
            "[Avalonium] No bundled Helpers folder found; optional helper-script paths must be set in Avalonium.ini\n");
    }

    EngineDiscovery::SetLogSection("runtime");
    return g_Config;
}

const RuntimeFeatureConfig& Get() {
    if (!g_ConfigLoaded) {
        Load(nullptr);
    }
    return g_Config;
}
}
