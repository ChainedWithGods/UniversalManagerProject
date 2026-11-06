#include "Launcher.h"
#include "../core/Logger.h"

#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace {
std::string ModuleDirectory(HMODULE module) {
    char path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return {};
    std::string result(path, length);
    const size_t slash = result.find_last_of("\\/");
    if (slash == std::string::npos) return {};
    result.resize(slash);
    return result;
}

std::string Trim(std::string value) {
    const size_t first = value.find_first_not_of(" \t\r\n\"");
    if (first == std::string::npos) return {};
    const size_t last = value.find_last_not_of(" \t\r\n\"");
    return value.substr(first, last - first + 1);
}

bool ReadBoolSetting(const char* section, const char* key, bool fallback, const std::string& iniPath) {
    char value[32] = {};
    GetPrivateProfileStringA(section, key, "", value, static_cast<DWORD>(sizeof(value)), iniPath.c_str());
    std::string normalized = Trim(value);
    for (char& ch : normalized) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (normalized == "true" || normalized == "yes" || normalized == "on" || normalized == "1") return true;
    if (normalized == "false" || normalized == "no" || normalized == "off" || normalized == "0") return false;
    return fallback;
}

std::string ResolveOutput(const std::string& directory, const std::string& value) {
    std::string output = Trim(value);
    if (output.empty()) output = "CrashHandler/Crashes";
    if (output.size() > 1 && output[1] == ':') return output;
    if (output.rfind("\\\\", 0) == 0 || output[0] == '\\' || output[0] == '/') return output;
    for (char& ch : output) if (ch == '/') ch = '\\';
    return directory + "\\" + output;
}

std::string Quote(const std::string& value) {
    std::string quoted = "\"";
    for (char ch : value) {
        if (ch == '"') quoted += '\\';
        quoted += ch;
    }
    quoted += '"';
    return quoted;
}

void LogLaunch(const std::string& directory, const char* message, DWORD error = ERROR_SUCCESS) {
    const std::string path = directory + "\\AggressiveDog-launch.log";
    FILE* file = nullptr;
    if (fopen_s(&file, path.c_str(), "a") != 0 || !file) return;
    SYSTEMTIME now = {};
    GetLocalTime(&now);
    std::fprintf(file, "%04u-%02u-%02u %02u:%02u:%02u %s", now.wYear, now.wMonth, now.wDay,
                 now.wHour, now.wMinute, now.wSecond, message);
    if (error != ERROR_SUCCESS) std::fprintf(file, " (Win32 error %lu)", error);
    std::fputc('\n', file);
    std::fclose(file);
}
}

void AggressiveDog::LaunchIfEnabled(HMODULE module) {
    const std::string directory = ModuleDirectory(module);
    if (directory.empty()) return;
    const std::string iniPath = directory + "\\Avalonium.ini";
    const bool enabled = ReadBoolSetting("AggressiveDog", "Enabled", false, iniPath);
    LogLaunch(directory, enabled ? "Enabled=true; preparing watchdog launch." :
                                  "Enabled=false; watchdog launch skipped.");
    if (!enabled) return;

    char exePath[MAX_PATH] = {};
    std::string candidate = directory + "\\CrashHandler\\AggressiveDog.exe";
    DWORD attr = GetFileAttributesA(candidate.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) || candidate.size() >= MAX_PATH) {
        LogLaunch(directory, "AggressiveDog.exe was not found in CrashHandler beside Avalonium.dll.", GetLastError());
        return;
    }
    lstrcpynA(exePath, candidate.c_str(), MAX_PATH);

    char pdbPath[MAX_PATH] = {};
    char outputValue[MAX_PATH] = {};
    const std::string pdb = directory + "\\CrashHandler\\Avalonium.pdb";
    GetPrivateProfileStringA("AggressiveDog", "LogDirectory", "CrashHandler/Crashes", outputValue,
                             static_cast<DWORD>(sizeof(outputValue)), iniPath.c_str());
    const std::string output = ResolveOutput(directory, outputValue);
    if (pdb.size() >= MAX_PATH || output.size() >= MAX_PATH) {
        LogLaunch(directory, "PDB or output path exceeds MAX_PATH.", ERROR_FILENAME_EXCED_RANGE);
        return;
    }
    lstrcpynA(pdbPath, pdb.c_str(), MAX_PATH);
    const bool writeMiniDump = ReadBoolSetting("AggressiveDog", "WriteMiniDump", true, iniPath);
    const std::string runtimeLog = EngineDiscovery::GetLogPath();
    LogLaunch(directory, ("Watchdog executable: " + std::string(exePath) +
                          "; PDB: " + pdb + "; output: " + output + "; INI: " + iniPath +
                          "; runtime log: " + runtimeLog +
                          "; minidump: " + (writeMiniDump ? "enabled" : "disabled")).c_str());

    std::string command = Quote(exePath) + " --pid " + std::to_string(GetCurrentProcessId()) +
                          " --tid " + std::to_string(GetCurrentThreadId()) +
                          " --pdb " + Quote(pdbPath) + " --out " + Quote(output) +
                          " --ini " + Quote(iniPath) + " --log " + Quote(runtimeLog);
    std::vector<char> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back('\0');
    STARTUPINFOA startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (CreateProcessA(exePath, mutableCommand.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, directory.c_str(),
                       &startup, &process)) {
        const DWORD watchdogPid = process.dwProcessId;
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        char message[128] = {};
        std::snprintf(message, sizeof(message), "CreateProcess succeeded; watchdog PID %lu, target PID %lu, target TID %lu.",
                      watchdogPid, GetCurrentProcessId(), GetCurrentThreadId());
        LogLaunch(directory, message);
    } else {
        const DWORD error = GetLastError();
        LogLaunch(directory, "CreateProcess failed.", error);
    }
}
