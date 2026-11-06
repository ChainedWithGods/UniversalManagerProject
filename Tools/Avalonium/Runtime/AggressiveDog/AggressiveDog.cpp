#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
constexpr DWORD kFatalCodes[] = { 0xC0000409, 0xC00000FD, 0xC0000005, 0x80000003 };
constexpr size_t kMaxSnapshots = 4;
constexpr size_t kMaxEvents = 256;
struct Options {
    DWORD pid = 0, tid = 0, maxDepth = 64, otherThreadDepth = 8;
    std::string pdb, output, ini, log;
    bool dumpRegisters = true, catchFastFail = true, writeMiniDump = true;
    bool terminationBreakpoint = true;
    bool attachDebugger = false;
};

struct SnapshotModule {
    DWORD64 base = 0;
    DWORD64 size = 0;
    std::string name;
    std::string path;
};
struct SnapshotThread {
    DWORD tid = 0;
    DWORD64 ip = 0, sp = 0;
    bool contextOk = false;
    std::string location;
};
struct Snapshot {
    ULONGLONG capturedAtMs = 0;
    std::vector<SnapshotThread> threads;
    std::vector<SnapshotModule> modules;
};

struct Evidence {
    std::mutex mutex;
    std::deque<Snapshot> snapshots;
    std::deque<std::string> events;
    std::atomic<bool> stop{false};
    std::atomic<int> terminationArmed{0};
    std::atomic<int> terminationHits{0};
    std::atomic<DWORD> lastTerminationStatus{0};
    std::atomic<bool> lastTerminationStatusKnown{false};
    std::atomic<int> firstChanceExceptions{0};
    std::atomic<int> secondChanceExceptions{0};
    std::string imagePath;
    ULONGLONG attachedAtMs = 0;
    bool debuggerAttached = false;
    bool terminationBreakpointEnabled = true;
};

bool ReadBoolSetting(const char* key, bool fallback, const std::string& iniPath) {
    char value[32] = {};
    GetPrivateProfileStringA("AggressiveDog", key, "", value, static_cast<DWORD>(sizeof(value)), iniPath.c_str());
    std::string normalized(value);
    for (char& ch : normalized) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (normalized == "true" || normalized == "yes" || normalized == "on" || normalized == "1") return true;
    if (normalized == "false" || normalized == "no" || normalized == "off" || normalized == "0") return false;
    return fallback;
}

void AppendStatus(const Options& options, const std::string& message) {
    std::error_code ec;
    std::filesystem::create_directories(options.output, ec);
    const auto path = std::filesystem::path(options.output) / "AggressiveDog-status.log";
    std::ofstream file(path, std::ios::app);
    if (!file) return;
    SYSTEMTIME now = {}; GetLocalTime(&now);
    file << now.wYear << '-' << now.wMonth << '-' << now.wDay << ' '
         << now.wHour << ':' << now.wMinute << ':' << now.wSecond << '.' << now.wMilliseconds
         << " " << message << '\n';
}

bool Number(const char* s, DWORD& value) {
    if (!s || !*s) return false;
    char* end = nullptr;
    const unsigned long parsed = strtoul(s, &end, 10);
    if (!end || *end || parsed == 0 || parsed > MAXDWORD) return false;
    value = static_cast<DWORD>(parsed);
    return true;
}

bool Parse(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (i + 1 >= argc) return false;
        const char* value = argv[++i];
        if (key == "--pid") { if (!Number(value, o.pid)) return false; }
        else if (key == "--tid") { if (!Number(value, o.tid)) return false; }
        else if (key == "--pdb") o.pdb = value;
        else if (key == "--out") o.output = value;
        else if (key == "--ini") o.ini = value;
        else if (key == "--log") o.log = value;
        else return false;
    }
    if (o.pid == 0 || o.tid == 0 || o.output.empty()) return false;
    if (!o.ini.empty()) {
        o.dumpRegisters = ReadBoolSetting("DumpRegisters", true, o.ini);
        o.catchFastFail = ReadBoolSetting("CatchFastFail", true, o.ini);
        o.writeMiniDump = ReadBoolSetting("WriteMiniDump", true, o.ini);
        o.terminationBreakpoint = ReadBoolSetting("TerminationBreakpoint", true, o.ini);
        o.attachDebugger = ReadBoolSetting("AttachDebugger", false, o.ini);
        const int depth = GetPrivateProfileIntA("AggressiveDog", "MaxStackDepth", 64, o.ini.c_str());
        o.maxDepth = static_cast<DWORD>(std::clamp(depth, 1, 256));
        const int otherDepth = GetPrivateProfileIntA("AggressiveDog", "OtherThreadStackDepth", 8, o.ini.c_str());
        o.otherThreadDepth = static_cast<DWORD>(std::clamp(otherDepth, 0, 64));
    }
    return true;
}

bool IsFatal(DWORD code, bool catchFastFail) {
    for (DWORD candidate : kFatalCodes) if (candidate == code) return code != 0xC0000409 || catchFastFail;
    return false;
}

std::string ModuleForAddress(HANDLE process, DWORD64 address, DWORD64& base) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetProcessId(process));
    if (snapshot == INVALID_HANDLE_VALUE) return "unknown";
    MODULEENTRY32 entry = {}; entry.dwSize = sizeof(entry);
    std::string result = "unknown"; base = 0;
    if (Module32First(snapshot, &entry)) do {
        const DWORD64 start = reinterpret_cast<DWORD64>(entry.modBaseAddr);
        if (address >= start && address < start + entry.modBaseSize) {
            base = start; result = entry.szModule; break;
        }
    } while (Module32Next(snapshot, &entry));
    CloseHandle(snapshot);
    return result;
}

ULONGLONG NowMs() { return GetTickCount64(); }

std::string DescribeLocationInList(DWORD64 address, const std::vector<SnapshotModule>& modules,
                                   DWORD64& base) {
    base = 0;
    for (const SnapshotModule& module : modules) {
        if (address >= module.base && address < module.base + module.size) {
            base = module.base;
            char text[512] = {};
            std::snprintf(text, sizeof(text), "%s + 0x%llX", module.name.c_str(),
                          static_cast<unsigned long long>(address - base));
            return text;
        }
    }
    char text[32] = {};
    std::snprintf(text, sizeof(text), "0x%llX", static_cast<unsigned long long>(address));
    return text;
}

void RecordEvent(Evidence& evidence, const std::string& text) {
    SYSTEMTIME now = {}; GetLocalTime(&now);
    char stamp[40] = {};
    std::snprintf(stamp, sizeof(stamp), "%02u:%02u:%02u.%03u",
                  now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
    std::lock_guard<std::mutex> lock(evidence.mutex);
    evidence.events.push_back(std::string(stamp) + "  " + text);
    while (evidence.events.size() > kMaxEvents) evidence.events.pop_front();
}

std::vector<SnapshotModule> SnapshotModules(HANDLE process) {
    std::vector<SnapshotModule> modules;
    HMODULE handles[1024] = {};
    DWORD needed = 0;
    if (!EnumProcessModulesEx(process, handles, sizeof(handles), &needed, LIST_MODULES_ALL))
        return modules;
    const DWORD count = (std::min<DWORD>)(needed / sizeof(HMODULE), 1024);
    for (DWORD i = 0; i < count; ++i) {
        MODULEINFO info = {};
        if (!GetModuleInformation(process, handles[i], &info, sizeof(info))) continue;
        SnapshotModule module;
        module.base = reinterpret_cast<DWORD64>(info.lpBaseOfDll);
        module.size = info.SizeOfImage;
        char name[MAX_PATH] = {};
        if (GetModuleBaseNameA(process, handles[i], name, MAX_PATH)) module.name = name;
        char path[MAX_PATH] = {};
        if (GetModuleFileNameExA(process, handles[i], path, MAX_PATH)) module.path = path;
        modules.push_back(std::move(module));
    }
    return modules;
}

Snapshot CaptureSnapshot(DWORD pid, const std::vector<SnapshotModule>& modules) {
    Snapshot snapshot;
    snapshot.capturedAtMs = NowMs();
    snapshot.modules = modules;
    HANDLE threads = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (threads == INVALID_HANDLE_VALUE) return snapshot;
    THREADENTRY32 entry = {}; entry.dwSize = sizeof(entry);
    if (Thread32First(threads, &entry)) do {
        if (entry.th32OwnerProcessID != pid) continue;
        SnapshotThread record;
        record.tid = entry.th32ThreadID;
        HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, record.tid);
        if (thread) {
            CONTEXT context = {}; context.ContextFlags = CONTEXT_CONTROL;
            if (GetThreadContext(thread, &context)) {
#ifdef _M_X64
                record.ip = context.Rip;
                record.sp = context.Rsp;
#else
                record.ip = context.Eip;
                record.sp = context.Esp;
#endif
                record.contextOk = true;
                DWORD64 base = 0;
                record.location = DescribeLocationInList(record.ip, snapshot.modules, base);
            }
            CloseHandle(thread);
        }
        snapshot.threads.push_back(std::move(record));
    } while (Thread32Next(threads, &entry));
    CloseHandle(threads);
    return snapshot;
}

void RunSampler(HANDLE process, DWORD pid, Evidence& evidence) {
    std::vector<SnapshotModule> modules;
    ULONGLONG modulesRefreshedAt = 0;
    while (!evidence.stop.load()) {
        if (modules.empty() || NowMs() - modulesRefreshedAt > 2000) {
            modules = SnapshotModules(process);
            modulesRefreshedAt = NowMs();
        }
        Snapshot snapshot = CaptureSnapshot(pid, modules);
        {
            std::lock_guard<std::mutex> lock(evidence.mutex);
            evidence.snapshots.push_back(std::move(snapshot));
            while (evidence.snapshots.size() > kMaxSnapshots) evidence.snapshots.pop_front();
        }
        for (int waited = 0; waited < 500 && !evidence.stop.load(); waited += 25)
            Sleep(25);
    }
}

BOOL CALLBACK ReadRemote(HANDLE process, DWORD64 address, PVOID data, DWORD size, LPDWORD bytesRead) {
    SIZE_T read = 0;
    const BOOL ok = ReadProcessMemory(process, reinterpret_cast<LPCVOID>(address), data, size, &read);
    if (bytesRead) *bytesRead = static_cast<DWORD>(read);
    return ok && read != 0;
}

std::string DescribeFrame(HANDLE process, DWORD64 address) {
    char text[2048] = {};
    DWORD64 base = 0;
    const std::string module = ModuleForAddress(process, address, base);
    if (base) {
        std::snprintf(text, sizeof(text), "  %s + 0x%llX", module.c_str(),
                      static_cast<unsigned long long>(address - base));
    } else {
        std::snprintf(text, sizeof(text), "  0x%llX", static_cast<unsigned long long>(address));
    }

    DWORD64 displacement = 0;
    char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
    auto* symbol = reinterpret_cast<PSYMBOL_INFO>(symbolBuffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO); symbol->MaxNameLen = MAX_SYM_NAME;
    const bool resolved = SymFromAddr(process, address, &displacement, symbol) != FALSE;
    IMAGEHLP_MODULE64 moduleInfo = {}; moduleInfo.SizeOfStruct = sizeof(moduleInfo);
    const bool hasPdb = SymGetModuleInfo64(process, address, &moduleInfo) != FALSE &&
                        moduleInfo.SymType == SymPdb;
    if (!resolved || !hasPdb) return text;

    const size_t used = std::strlen(text);
    std::snprintf(text + used, sizeof(text) - used, "  %s + 0x%llX", symbol->Name,
                  static_cast<unsigned long long>(displacement));
    IMAGEHLP_LINE64 line = {}; line.SizeOfStruct = sizeof(line);
    DWORD lineDisplacement = 0;
    if (SymGetLineFromAddr64(process, address, &lineDisplacement, &line)) {
        const size_t attached = std::strlen(text);
        std::snprintf(text + attached, sizeof(text) - attached, "\n      %s:%lu", line.FileName, line.LineNumber);
    }
    return text;
}

void WriteStack(FILE* file, HANDLE process, HANDLE thread, const CONTEXT& context, DWORD maxDepth) {
    CONTEXT walkContext = context;
    STACKFRAME64 frame = {};
#ifdef _M_X64
    frame.AddrPC.Offset = context.Rip; frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp; frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp; frame.AddrFrame.Mode = AddrModeFlat;
    const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
#else
    frame.AddrPC.Offset = context.Eip; frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Esp; frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Ebp; frame.AddrFrame.Mode = AddrModeFlat;
    const DWORD machine = IMAGE_FILE_MACHINE_I386;
#endif
    DWORD framesWritten = 0;
    DWORD duplicateFramesSkipped = 0;
    DWORD64 previousPc = 0;
    for (DWORD i = 0; i < maxDepth && frame.AddrPC.Offset != 0; ++i) {
        const DWORD64 pc = frame.AddrPC.Offset;
        const std::string description = DescribeFrame(process, pc);
        if (pc == previousPc) {
            ++duplicateFramesSkipped;
        } else {
            std::fprintf(file, "#%02lu 0x%llX%s\n", static_cast<unsigned long>(framesWritten),
                         static_cast<unsigned long long>(pc), description.c_str());
            ++framesWritten;
        }
        previousPc = pc;
        SetLastError(ERROR_SUCCESS);
        if (!StackWalk64(machine, process, thread, &frame, &walkContext, ReadRemote,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
            const DWORD error = GetLastError();
            if (error == ERROR_SUCCESS) {
                std::fprintf(file, "StackWalk64 stopped after %lu frame(s) (end of walk or unwind metadata unavailable).\n", framesWritten);
            } else {
                std::fprintf(file, "StackWalk64 stopped after %lu frame(s) (Win32 error %lu).\n", framesWritten, error);
            }
            return;
        }
    }
    if (framesWritten == 0) std::fprintf(file, "No frames could be unwound from the captured context.\n");
    else if (framesWritten >= maxDepth) std::fprintf(file, "Stopped at configured frame limit (%lu).\n", maxDepth);
    else std::fprintf(file, "Stack walk ended after %lu frame(s).\n", framesWritten);
    if (duplicateFramesSkipped) std::fprintf(file, "Suppressed %lu repeated instruction address(es).\n", duplicateFramesSkipped);
}

std::string ExceptionName(DWORD code);

void WriteCrashAnalysis(FILE* file, DWORD code, DWORD parameters, const ULONG_PTR* info) {
    std::fprintf(file, "\n[AUTOMATED TRIAGE]\nThis section reports recognized exception facts only; it does not claim a root cause.\n");
    if (code == 0xC0000409 && parameters > 0 && info && info[0] == 5) {
        std::fprintf(file, "Observed: Windows fast-fail reason 5 is FAST_FAIL_INVALID_ARG. The process terminated through a runtime fail-fast path. This exception code alone does not identify the caller, invalid argument, or underlying cause, and does not by itself prove a classic stack-buffer overwrite.\n");
    } else {
        std::fprintf(file, "Observed exception: 0x%08lX (%s). The exception code and parameters describe the failure signal; they do not by themselves identify the responsible component or root cause.\n",
                     code, ExceptionName(code).c_str());
    }
    std::fprintf(file, "Correlation guidance: compare the exception address and parameters with the faulting module, captured call stacks, register/context data, memory windows, minidump, and recent runtime log. Treat symbol names and nearby log messages as evidence, not proof of causation.\n");
}

bool CaptureContext(DWORD tid, CONTEXT& context, DWORD& error) {
    error = ERROR_SUCCESS;
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid);
    if (!thread) { error = GetLastError(); return false; }
    context = {}; context.ContextFlags = CONTEXT_FULL;
    const bool ok = GetThreadContext(thread, &context) != FALSE;
    if (!ok) error = GetLastError();
    CloseHandle(thread);
    return ok;
}

std::string ExceptionName(DWORD code) {
    switch (code) {
    case 0xC0000409: return "STATUS_STACK_BUFFER_OVERRUN / __fastfail";
    case 0xC00000FD: return "STATUS_STACK_OVERFLOW";
    case 0xC0000005: return "STATUS_ACCESS_VIOLATION";
    case 0x80000003: return "STATUS_BREAKPOINT";
    case 0xE0421001: return "NtTerminateProcess hardware breakpoint";
    default: return "NTSTATUS";
    }
}

std::string ExitCodeName(DWORD code) {
    switch (code) {
    case 0xC0000409: case 0xC00000FD: case 0xC0000005: case 0x80000003:
        return ExceptionName(code);
    default:
        return "nonzero exit code; no matching exception event was captured";
    }
}

std::string FastFailName(ULONG_PTR reason) {
    switch (reason) {
    case 0: return "LEGACY_GS_VIOLATION";
    case 1: return "VTGUARD_CHECK_FAILURE";
    case 2: return "STACK_COOKIE_CHECK_FAILURE";
    case 3: return "CORRUPT_LIST_ENTRY";
    case 4: return "INCORRECT_STACK";
    case 5: return "INVALID_ARG";
    case 6: return "GSCOOKIE_INIT";
    case 7: return "FATAL_APP_EXIT";
    case 8: return "RANGE_CHECK_FAILURE";
    case 10: return "UNHANDLED_EXCEPTION";
    default: return "unknown FAST_FAIL code";
    }
}

std::string ProcessImagePath(HANDLE process) {
    char path[32768] = {};
    DWORD length = static_cast<DWORD>(sizeof(path) / sizeof(path[0]));
    if (!QueryFullProcessImageNameA(process, 0, path, &length)) return "unavailable";
    return std::string(path, length);
}

void WriteProcessDetails(FILE* file, HANDLE process, DWORD pid) {
    std::fprintf(file, "\n[PROCESS]\nPID: %lu\nImage: %s\n", pid, ProcessImagePath(process).c_str());
    HANDLE processSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (processSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 entry = {}; entry.dwSize = sizeof(entry);
        if (Process32First(processSnapshot, &entry)) do {
            if (entry.th32ProcessID == pid) {
                std::fprintf(file, "Parent PID: %lu\nThread count at capture: %lu\n",
                             entry.th32ParentProcessID, entry.cntThreads);
                break;
            }
        } while (Process32Next(processSnapshot, &entry));
        CloseHandle(processSnapshot);
    } else {
        std::fprintf(file, "Process snapshot: unavailable (Win32 error %lu)\n", GetLastError());
    }
    FILETIME created = {}, exited = {}, kernel = {}, user = {};
    if (GetProcessTimes(process, &created, &exited, &kernel, &user)) {
        SYSTEMTIME started = {};
        if (FileTimeToSystemTime(&created, &started)) {
            std::fprintf(file, "Started (UTC): %04u-%02u-%02u %02u:%02u:%02u.%03u\n",
                         started.wYear, started.wMonth, started.wDay, started.wHour,
                         started.wMinute, started.wSecond, started.wMilliseconds);
        }
        FILETIME currentTime = {};
        GetSystemTimeAsFileTime(&currentTime);
        ULARGE_INTEGER startTicks = {}, currentTicks = {};
        startTicks.LowPart = created.dwLowDateTime; startTicks.HighPart = created.dwHighDateTime;
        currentTicks.LowPart = currentTime.dwLowDateTime; currentTicks.HighPart = currentTime.dwHighDateTime;
        if (currentTicks.QuadPart >= startTicks.QuadPart) {
            const double uptimeSeconds = (currentTicks.QuadPart - startTicks.QuadPart) / 10000000.0;
            std::fprintf(file, "Uptime at capture: %.3f s\n", uptimeSeconds);
        }
        ULARGE_INTEGER kernelTime = {}, userTime = {};
        kernelTime.LowPart = kernel.dwLowDateTime; kernelTime.HighPart = kernel.dwHighDateTime;
        userTime.LowPart = user.dwLowDateTime; userTime.HighPart = user.dwHighDateTime;
        std::fprintf(file, "CPU time: kernel %.3f s, user %.3f s\n",
                     kernelTime.QuadPart / 10000000.0, userTime.QuadPart / 10000000.0);
    } else {
        std::fprintf(file, "Process timing: unavailable (Win32 error %lu)\n", GetLastError());
    }

    BOOL wow64 = FALSE;
    if (IsWow64Process(process, &wow64)) {
        SYSTEM_INFO system = {}; GetNativeSystemInfo(&system);
        const char* native = system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? "x64" :
                             system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64 ? "ARM64" : "other";
        std::fprintf(file, "Architecture: %s process on %s Windows\n", wow64 ? "32-bit" : "native-width", native);
    } else {
        std::fprintf(file, "Architecture: unavailable (Win32 error %lu)\n", GetLastError());
    }
}

void WriteExceptionDetails(FILE* file, DWORD code, DWORD parameters, const ULONG_PTR* info,
                           DWORD64 faultAddress) {
    std::fprintf(file, "\n[EXCEPTION DETAILS]\nCode: 0x%08lX (%s)\nException instruction address: 0x%llX\nParameter count: %lu\n",
                 code, ExceptionName(code).c_str(), static_cast<unsigned long long>(faultAddress), parameters);
    const DWORD count = info ? std::min<DWORD>(parameters, EXCEPTION_MAXIMUM_PARAMETERS) : 0;
    for (DWORD i = 0; i < count; ++i) {
        const ULONG_PTR value = info ? info[i] : 0;
        std::fprintf(file, "Parameter[%lu]: 0x%llX (%llu)\n", i,
                     static_cast<unsigned long long>(value), static_cast<unsigned long long>(value));
    }
    if (!info && parameters > 0) {
        std::fprintf(file, "Exception parameters unavailable.\n");
    } else if (code == EXCEPTION_ACCESS_VIOLATION && parameters >= 2) {
        const char* operation = info[0] == 0 ? "read" : info[0] == 1 ? "write" : info[0] == 8 ? "execute" : "unknown operation";
        std::fprintf(file, "Access violation: attempted %s at 0x%llX\n", operation,
                     static_cast<unsigned long long>(info[1]));
    } else if (code == 0xC0000409 && parameters > 0) {
        std::fprintf(file, "FastFail: %s (reason 0x%llX)\n", FastFailName(info[0]).c_str(),
                     static_cast<unsigned long long>(info[0]));
    }
}

void WriteMemoryWindow(FILE* file, HANDLE process, const char* label, DWORD64 address,
                       DWORD before, DWORD after) {
    std::fprintf(file, "\n[%s] center=0x%llX\n", label, static_cast<unsigned long long>(address));
    const DWORD64 start = address > before ? address - before : 0;
    const DWORD length = before + after;
    std::vector<unsigned char> bytes(length, 0);
    SIZE_T bytesRead = 0;
    const BOOL readOk = ReadProcessMemory(process, reinterpret_cast<LPCVOID>(start), bytes.data(), length, &bytesRead);
    const DWORD readError = readOk ? ERROR_SUCCESS : GetLastError();
    if (bytesRead == 0) {
        std::fprintf(file, "Unavailable (ReadProcessMemory error %lu)\n", readError);
        return;
    }
    for (SIZE_T offset = 0; offset < bytesRead; offset += 16) {
        std::fprintf(file, "0x%llX  ", static_cast<unsigned long long>(start + offset));
        const SIZE_T rowEnd = std::min<SIZE_T>(offset + 16, bytesRead);
        for (SIZE_T i = offset; i < rowEnd; ++i) {
            if (start + i == address) std::fprintf(file, "[");
            std::fprintf(file, "%02X", bytes[i]);
            if (start + i == address) std::fprintf(file, "]"); else std::fprintf(file, " ");
        }
        std::fputc('\n', file);
    }
    if (!readOk) std::fprintf(file, "Partial memory capture: ReadProcessMemory error %lu after %llu byte(s).\n",
                              readError, static_cast<unsigned long long>(bytesRead));
}

void WriteModuleList(FILE* file, DWORD pid, DWORD64 faultingBase) {
    std::fprintf(file, "\n[LOADED MODULES - ALPHABETICAL]\n* marks the module containing the exception instruction.\n");
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::fprintf(file, "Unavailable (CreateToolhelp32Snapshot error %lu)\n", GetLastError());
        return;
    }
    MODULEENTRY32 entry = {}; entry.dwSize = sizeof(entry);
    std::vector<MODULEENTRY32> modules;
    if (Module32First(snapshot, &entry)) do {
        modules.push_back(entry);
    } while (Module32Next(snapshot, &entry));
    else std::fprintf(file, "Enumeration failed (Win32 error %lu)\n", GetLastError());
    std::sort(modules.begin(), modules.end(), [](const MODULEENTRY32& left, const MODULEENTRY32& right) {
        return _stricmp(left.szModule, right.szModule) < 0;
    });
    for (const MODULEENTRY32& module : modules) {
        const DWORD64 base = reinterpret_cast<DWORD64>(module.modBaseAddr);
        std::fprintf(file, "%s%s | base=0x%llX | size=0x%lX | %s\n",
                     base == faultingBase ? "* " : "  ", module.szModule,
                     static_cast<unsigned long long>(base), module.modBaseSize, module.szExePath);
    }
    const DWORD count = static_cast<DWORD>(modules.size());
    std::fprintf(file, "Module count: %lu\n", count);
    CloseHandle(snapshot);
}

void WriteThreadList(FILE* file, HANDLE process, DWORD pid, DWORD crashingTid, DWORD otherThreadDepth) {
    std::fprintf(file, "\n[OTHER THREAD SNAPSHOTS]\nOther-thread stack depth: %lu\n", otherThreadDepth);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::fprintf(file, "Unavailable (CreateToolhelp32Snapshot error %lu)\n", GetLastError());
        return;
    }
    THREADENTRY32 entry = {}; entry.dwSize = sizeof(entry);
    DWORD count = 0;
    if (Thread32First(snapshot, &entry)) do {
        if (entry.th32OwnerProcessID != pid) continue;
        HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | SYNCHRONIZE,
                                   FALSE, entry.th32ThreadID);
        const DWORD wait = thread ? WaitForSingleObject(thread, 0) : WAIT_FAILED;
        const int priority = thread ? GetThreadPriority(thread) : THREAD_PRIORITY_ERROR_RETURN;
        const std::string priorityText = priority == THREAD_PRIORITY_ERROR_RETURN ? "?" : std::to_string(priority);
        const char* state = wait == WAIT_TIMEOUT ? "alive" : wait == WAIT_OBJECT_0 ? "terminated" : "unavailable";
        std::fprintf(file, "\n[TID %lu%s] priority=%s state=%s\n", entry.th32ThreadID,
                     entry.th32ThreadID == crashingTid ? " faulting thread (full stack above)" : "",
                     priorityText.c_str(), state);
        if (entry.th32ThreadID != crashingTid && otherThreadDepth > 0) {
            CONTEXT context = {}; context.ContextFlags = CONTEXT_FULL;
            if (thread && GetThreadContext(thread, &context)) {
#ifdef _M_X64
                const DWORD64 pc = context.Rip;
                const DWORD64 sp = context.Rsp;
#else
                const DWORD64 pc = context.Eip;
                const DWORD64 sp = context.Esp;
#endif
                std::fprintf(file, "IP=0x%llX SP=0x%llX%s\n",
                             static_cast<unsigned long long>(pc), static_cast<unsigned long long>(sp),
                             DescribeFrame(process, pc).c_str());
                WriteStack(file, process, thread, context, otherThreadDepth);
            } else {
                std::fprintf(file, "Context unavailable (Win32 error %lu).\n", thread ? GetLastError() : ERROR_ACCESS_DENIED);
            }
        }
        if (thread) CloseHandle(thread);
        ++count;
    } while (Thread32Next(snapshot, &entry));
    else std::fprintf(file, "Enumeration failed (Win32 error %lu)\n", GetLastError());
    std::fprintf(file, "Enumerated process thread count: %lu\n", count);
    CloseHandle(snapshot);
}

bool WriteMiniDump(HANDLE process, DWORD pid, const std::string& reportPath,
                   std::string& dumpPath, DWORD& error) {
    std::filesystem::path path(reportPath);
    path.replace_extension(".dmp");
    dumpPath = path.string();
    HANDLE dumpFile = CreateFileA(dumpPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (dumpFile == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        return false;
    }
    const MINIDUMP_TYPE type = static_cast<MINIDUMP_TYPE>(
        MiniDumpWithDataSegs | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules |
        MiniDumpWithIndirectlyReferencedMemory);
    const BOOL dumped = MiniDumpWriteDump(process, pid, dumpFile, type, nullptr, nullptr, nullptr);
    error = dumped ? ERROR_SUCCESS : GetLastError();
    if (dumped && !FlushFileBuffers(dumpFile)) {
        error = GetLastError();
    }
    const bool closed = CloseHandle(dumpFile) != FALSE;
    if (!closed && error == ERROR_SUCCESS) error = GetLastError();
    return dumped != FALSE && error == ERROR_SUCCESS;
}

void WriteRecentRuntimeLog(FILE* file, const std::string& logPath, size_t maxLines = 120) {
    std::fprintf(file, "\n[RECENT RUNTIME LOG]\nPath: %s\n[!] marks lines matching generic error indicators.\n",
                 logPath.empty() ? "(not provided)" : logPath.c_str());
    if (logPath.empty()) {
        std::fprintf(file, "Unavailable: launcher did not supply a runtime log path.\n");
        return;
    }
    std::ifstream input(logPath);
    if (!input) {
        std::fprintf(file, "Unavailable: could not open the runtime log.\n");
        return;
    }
    std::deque<std::pair<size_t, std::string>> tail;
    size_t totalLines = 0;
    std::string line;
    while (std::getline(input, line)) {
        ++totalLines;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() > 2048) line = line.substr(0, 2048) + " ...[line truncated]";
        tail.emplace_back(totalLines, line);
        if (tail.size() > maxLines) tail.pop_front();
    }
    if (input.bad()) std::fprintf(file, "Note: a read error occurred while collecting the log tail.\n");
    std::fprintf(file, "Showing %llu of %llu line(s), maximum %llu.\n",
                 static_cast<unsigned long long>(tail.size()),
                 static_cast<unsigned long long>(totalLines),
                 static_cast<unsigned long long>(maxLines));
    if (tail.empty()) {
        std::fprintf(file, "(log is empty)\n");
        return;
    }
    for (const auto& entry : tail) {
        std::string normalized = entry.second;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        const bool relevant = normalized.find("error") != std::string::npos ||
                              normalized.find("failed") != std::string::npos ||
                              normalized.find("failure") != std::string::npos ||
                              normalized.find("exception") != std::string::npos ||
                              normalized.find("crash") != std::string::npos ||
                              normalized.find("fatal") != std::string::npos ||
                              normalized.find("assert") != std::string::npos ||
                              normalized.find("access violation") != std::string::npos ||
                              normalized.find("fast-fail") != std::string::npos;
        std::fprintf(file, "| %s%6llu | %s\n", relevant ? "[!] " : "    ",
                     static_cast<unsigned long long>(entry.first), entry.second.c_str());
    }
}

void WriteEventTail(FILE* file, Evidence& evidence, size_t maxLines) {
    std::deque<std::string> events;
    {
        std::lock_guard<std::mutex> lock(evidence.mutex);
        events = evidence.events;
    }
    std::fprintf(file, "\n[WATCHDOG EVENT TAIL]\n");
    if (events.empty()) {
        std::fprintf(file, "(no debug events recorded)\n");
        return;
    }
    const size_t start = events.size() > maxLines ? events.size() - maxLines : 0;
    if (start) std::fprintf(file, "(%llu earlier event(s) omitted)\n",
                            static_cast<unsigned long long>(start));
    for (size_t i = start; i < events.size(); ++i)
        std::fprintf(file, "%s\n", events[i].c_str());
}

bool SymbolizeAddress(HANDLE process, DWORD64 address, std::string& text) {
    char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
    auto* symbol = reinterpret_cast<PSYMBOL_INFO>(symbolBuffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;
    DWORD64 displacement = 0;
    if (!SymFromAddr(process, address, &displacement, symbol)) return false;
    IMAGEHLP_MODULE64 moduleInfo = {}; moduleInfo.SizeOfStruct = sizeof(moduleInfo);
    if (!SymGetModuleInfo64(process, address, &moduleInfo) || moduleInfo.SymType != SymPdb)
        return false;
    char resolved[512] = {};
    std::snprintf(resolved, sizeof(resolved), "%s + 0x%llX", symbol->Name,
                  static_cast<unsigned long long>(displacement));
    text = resolved;
    return true;
}

void WriteTerminationEvidence(FILE* file, Evidence& evidence, HANDLE process) {
    std::deque<Snapshot> snapshots;
    {
        std::lock_guard<std::mutex> lock(evidence.mutex);
        snapshots = evidence.snapshots;
    }
    const int armed = evidence.terminationArmed.load();
    const int hits = evidence.terminationHits.load();
    const bool statusKnown = evidence.lastTerminationStatusKnown.load();
    const DWORD lastStatus = evidence.lastTerminationStatus.load();

    std::fprintf(file, "\n[TERMINATION EVIDENCE]\n");
    if (!evidence.debuggerAttached) {
        std::fprintf(file, "Debugger attach: disabled (passive monitor). Exception events and the NtTerminateProcess breakpoint are not available in this mode; termination is classified from the exit code and the sampled thread state.\n");
    } else if (!evidence.terminationBreakpointEnabled) {
        std::fprintf(file, "Debugger attached; NtTerminateProcess breakpoint disabled by configuration. In-process termination paths are not attributed.\n");
    } else {
        std::fprintf(file, "NtTerminateProcess breakpoint: armed on %d thread(s); %d hit(s).\n", armed, hits);
        if (statusKnown) {
            std::fprintf(file, "Last in-process NtTerminateProcess status: 0x%08lX (%lu).\n",
                         lastStatus, lastStatus);
        } else {
            std::fprintf(file, "No in-process NtTerminateProcess call was observed; the exit did not travel through ntdll's termination entry (external termination, a direct system call, or a job-object kill).\n");
        }
        std::fprintf(file, "Exception events delivered to the watchdog: %d first-chance, %d second-chance.\n",
                     evidence.firstChanceExceptions.load(), evidence.secondChanceExceptions.load());
    }

    WriteEventTail(file, evidence, 40);

    if (snapshots.empty()) {
        std::fprintf(file, "\nNo thread snapshot was captured before the exit.\n");
        return;
    }
    const Snapshot& snapshot = snapshots.back();
    const ULONGLONG age = NowMs() > snapshot.capturedAtMs ? NowMs() - snapshot.capturedAtMs : 0;
    std::fprintf(file, "\n[LAST THREAD SNAPSHOT]\nCaptured %llu ms before the watchdog observed the exit; %zu thread(s), %zu module(s).\n",
                 static_cast<unsigned long long>(age), snapshot.threads.size(), snapshot.modules.size());
    for (const SnapshotThread& thread : snapshot.threads) {
        if (!thread.contextOk) {
            std::fprintf(file, "TID %lu  context unavailable\n", thread.tid);
            continue;
        }
        std::string symbol;
        const bool symbolized = evidence.debuggerAttached &&
                                SymbolizeAddress(process, thread.ip, symbol);
        std::fprintf(file, "TID %lu  IP=%s  SP=0x%llX%s%s\n", thread.tid, thread.location.c_str(),
                     static_cast<unsigned long long>(thread.sp),
                     symbolized ? "  " : "", symbolized ? symbol.c_str() : "");
    }
    std::fprintf(file, "\n[MODULES AT LAST SNAPSHOT - NON-WINDOWS]\n");
    for (const SnapshotModule& module : snapshot.modules) {
        std::string path = module.path;
        for (char& ch : path) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (path.rfind("c:\\windows\\", 0) == 0) continue;
        std::fprintf(file, "  %s | base=0x%llX | size=0x%llX | %s\n", module.name.c_str(),
                     static_cast<unsigned long long>(module.base),
                     static_cast<unsigned long long>(module.size), module.path.c_str());
    }
}

void FreezeThreads(DWORD pid, DWORD exceptTid) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 entry = {}; entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot, &entry)) do {
        if (entry.th32OwnerProcessID != pid || entry.th32ThreadID == exceptTid) continue;
        HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID);
        if (thread) { SuspendThread(thread); CloseHandle(thread); }
    } while (Thread32Next(snapshot, &entry));
    CloseHandle(snapshot);
}

DWORD64 RemoteTerminateProcessAddress(DWORD pid) {
    HMODULE localNtdll = GetModuleHandleA("ntdll.dll");
    FARPROC localFunction = localNtdll ? GetProcAddress(localNtdll, "NtTerminateProcess") : nullptr;
    if (!localFunction || !localNtdll) return 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32 module = {}; module.dwSize = sizeof(module);
    DWORD64 remoteNtdll = 0;
    if (Module32First(snapshot, &module)) do {
        if (_stricmp(module.szModule, "ntdll.dll") == 0) { remoteNtdll = reinterpret_cast<DWORD64>(module.modBaseAddr); break; }
    } while (Module32Next(snapshot, &module));
    CloseHandle(snapshot);
    HMODULE localBase = GetModuleHandleA("ntdll.dll");
    if (!remoteNtdll || !localBase) return 0;
    return remoteNtdll + (reinterpret_cast<DWORD64>(localFunction) - reinterpret_cast<DWORD64>(localBase));
}

struct BreakpointArmStats {
    bool targetResolved = false;
    int found = 0, armed = 0, failed = 0;
};

BreakpointArmStats SetTerminationBreakpoint(DWORD pid) {
    BreakpointArmStats stats;
    const DWORD64 target = RemoteTerminateProcessAddress(pid);
    if (!target) return stats;
    stats.targetResolved = true;
    HANDLE threads = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (threads == INVALID_HANDLE_VALUE) return stats;
    THREADENTRY32 entry = {}; entry.dwSize = sizeof(entry);
    if (Thread32First(threads, &entry)) do {
        if (entry.th32OwnerProcessID != pid) continue;
        ++stats.found;
        HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
        bool armed = false;
        if (thread) {
            CONTEXT context = {}; context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (GetThreadContext(thread, &context)) {
#ifdef _M_X64
                context.Dr0 = target; context.Dr7 |= 1;
#else
                context.Dr0 = static_cast<DWORD>(target); context.Dr7 |= 1;
#endif
                armed = SetThreadContext(thread, &context) != FALSE;
            }
            CloseHandle(thread);
        }
        if (armed) ++stats.armed; else ++stats.failed;
    } while (Thread32Next(threads, &entry));
    CloseHandle(threads);
    return stats;
}

bool ConsumeTerminationBreakpoint(HANDLE process, DWORD tid, DWORD& exitStatus,
                                  bool& exitStatusKnown) {
    exitStatusKnown = false;
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION,
                               FALSE, tid);
    if (!thread) return false;

    CONTEXT context = {};
    context.ContextFlags = CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(thread, &context) || (context.Dr6 & 1) == 0) {
        CloseHandle(thread);
        return false;
    }

#ifdef _M_X64
    (void)process;
    exitStatus = static_cast<DWORD>(context.Rdx);
    exitStatusKnown = true;
#else
    DWORD stackArguments[2] = {};
    SIZE_T bytesRead = 0;
    const auto argumentsAddress = reinterpret_cast<LPCVOID>(
        static_cast<DWORD_PTR>(context.Esp) + sizeof(DWORD));
    if (ReadProcessMemory(process, argumentsAddress, stackArguments, sizeof(stackArguments), &bytesRead) &&
        bytesRead == sizeof(stackArguments)) {
        exitStatus = stackArguments[1];
        exitStatusKnown = true;
    }
#endif

    context.Dr0 = 0;
    context.Dr6 &= ~static_cast<DWORD_PTR>(1);
    context.Dr7 &= ~static_cast<DWORD_PTR>(0xF0003);
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    SetThreadContext(thread, &context);
    CloseHandle(thread);
    return true;
}

bool SetThreadTerminationBreakpoint(HANDLE thread, DWORD pid) {
    const DWORD64 target = RemoteTerminateProcessAddress(pid);
    if (!target) return false;
    CONTEXT context = {}; context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(thread, &context)) return false;
#ifdef _M_X64
    context.Dr0 = target;
#else
    context.Dr0 = static_cast<DWORD>(target);
#endif
    context.Dr7 |= 1;
    return SetThreadContext(thread, &context) != FALSE;
}

bool WriteReport(const Options& options, Evidence& evidence, HANDLE process, const DEBUG_EVENT& event, DWORD exceptionCode,
                 DWORD64 faultAddress, DWORD parameters, const ULONG_PTR* info, bool pdbLoaded,
                 const std::string& symbolDetails, std::string& reportPath, std::string& artifactStatus,
                 const CONTEXT* supplied = nullptr) {
    std::error_code ec;
    std::filesystem::create_directories(options.output, ec);
    SYSTEMTIME now = {}; GetLocalTime(&now);
    char fileName[MAX_PATH] = {};
    std::snprintf(fileName, sizeof(fileName), "AggressiveDog_%04u-%02u-%02u_%02u-%02u-%02u_%03u_pid%lu_tid%lu.txt",
                  now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
                  now.wMilliseconds, options.pid, event.dwThreadId);
    const auto path = std::filesystem::path(options.output) / fileName;
    reportPath = path.string();
    std::string dumpPath;
    DWORD dumpError = ERROR_SUCCESS;
    const bool dumpWritten = options.writeMiniDump &&
        WriteMiniDump(process, options.pid, reportPath, dumpPath, dumpError);
    artifactStatus = !options.writeMiniDump ? "minidump disabled" :
        dumpWritten ? "minidump written: " + dumpPath :
        "minidump failed (Win32 error " + std::to_string(dumpError) + "): " + dumpPath;
    FILE* file = nullptr;
    if (fopen_s(&file, path.string().c_str(), "w") != 0 || !file) return false;
    char processName[MAX_PATH] = "unknown";
    HANDLE processSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (processSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 entry = {}; entry.dwSize = sizeof(entry);
        if (Process32First(processSnap, &entry)) do { if (entry.th32ProcessID == options.pid) { lstrcpynA(processName, entry.szExeFile, MAX_PATH); break; } } while (Process32Next(processSnap, &entry));
        CloseHandle(processSnap);
    }
    std::fprintf(file, "AGGRESSIVEDOG CRASH WATCHDOG REPORT\nReport version: 2\nCaptured (local): %04u-%02u-%02u %02u:%02u:%02u.%03u\n",
                 now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
    std::fprintf(file, "\n[QUICK SUMMARY]\nProcess: %s (PID %lu)\nException: 0x%08lX (%s)\nException instruction address: 0x%llX\nFaulting thread: %lu\nChance: %s\nWatchdog PID: %lu\nSymbols: %s\nSymbol file: %s\nSymbol details: %s\n",
                 processName, options.pid, exceptionCode, ExceptionName(exceptionCode).c_str(),
                 static_cast<unsigned long long>(faultAddress), event.dwThreadId,
                 event.u.Exception.dwFirstChance ? "first chance" : "second chance / unhandled",
                 GetCurrentProcessId(), pdbLoaded ? "Avalonium PDB loaded" : "Avalonium PDB unavailable",
                 options.pdb.empty() ? "(not provided)" : options.pdb.c_str(), symbolDetails.c_str());
    std::fprintf(file, "\n[WATCHDOG SETTINGS]\nFaulting-thread stack depth: %lu\nOther-thread stack depth: %lu\nDump registers: %s\nCatch fast-fail: %s\nWrite minidump: %s\nRuntime log: %s\nOutput directory: %s\nINI: %s\n",
                 options.maxDepth, options.otherThreadDepth, options.dumpRegisters ? "true" : "false",
                 options.catchFastFail ? "true" : "false", options.writeMiniDump ? "true" : "false",
                 options.log.empty() ? "(not provided)" : options.log.c_str(), options.output.c_str(),
                 options.ini.empty() ? "(not provided)" : options.ini.c_str());
    WriteEventTail(file, evidence, 16);
    WriteRecentRuntimeLog(file, options.log);
    std::fprintf(file, "\n[ARTIFACTS]\nText report: %s\n", reportPath.c_str());
    if (!options.writeMiniDump) {
        std::fprintf(file, "Minidump: disabled by configuration\n");
    } else if (dumpWritten) {
        std::fprintf(file, "Minidump: written successfully\nMinidump path: %s\n",
                     dumpPath.c_str());
    } else {
        std::fprintf(file, "Minidump: failed (Win32 error %lu)\nMinidump path: %s\n",
                     dumpError, dumpPath.c_str());
    }
    WriteProcessDetails(file, process, options.pid);
    WriteExceptionDetails(file, exceptionCode, parameters, info, faultAddress);
    DWORD64 moduleBase = 0;
    const std::string module = ModuleForAddress(process, faultAddress, moduleBase);
    std::fprintf(file, "\n[FAULTING MODULE]\nModule: %s\nBase: 0x%llX\nOffset: 0x%llX\n",
                 module.c_str(), static_cast<unsigned long long>(moduleBase),
                 static_cast<unsigned long long>(moduleBase ? faultAddress - moduleBase : 0));
    CONTEXT context = {};
    bool haveContext = false;
    DWORD contextError = ERROR_SUCCESS;
    if (supplied) { context = *supplied; haveContext = true; }
    else haveContext = CaptureContext(event.dwThreadId, context, contextError);
    if (haveContext) {
        std::fprintf(file, "\n[THREAD CONTEXT]\nSource: %s\n", supplied ? "debug event" : "GetThreadContext");
        if (options.dumpRegisters) {
#ifdef _M_X64
            std::fprintf(file, "RAX=%llX RBX=%llX RCX=%llX RDX=%llX RSI=%llX RDI=%llX\nRBP=%llX RSP=%llX RIP=%llX R8=%llX R9=%llX R10=%llX R11=%llX R12=%llX R13=%llX R14=%llX R15=%llX\nEFLAGS=%08lX CS=%04X SS=%04X DS=%04X ES=%04X FS=%04X GS=%04X\n",
                context.Rax, context.Rbx, context.Rcx, context.Rdx, context.Rsi, context.Rdi,
                context.Rbp, context.Rsp, context.Rip, context.R8, context.R9, context.R10,
                context.R11, context.R12, context.R13, context.R14, context.R15, context.EFlags,
                static_cast<unsigned>(context.SegCs), static_cast<unsigned>(context.SegSs),
                static_cast<unsigned>(context.SegDs), static_cast<unsigned>(context.SegEs),
                static_cast<unsigned>(context.SegFs), static_cast<unsigned>(context.SegGs));
#else
            std::fprintf(file, "EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX ESI=%08lX EDI=%08lX\nEBP=%08lX ESP=%08lX EIP=%08lX EFLAGS=%08lX\n",
                context.Eax, context.Ebx, context.Ecx, context.Edx, context.Esi, context.Edi,
                context.Ebp, context.Esp, context.Eip, context.EFlags);
#endif
        }
        std::fprintf(file, "\n[CALL STACK]\nConfigured maximum depth: %lu\n", options.maxDepth);
        HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, event.dwThreadId);
        if (thread) { WriteStack(file, process, thread, context, options.maxDepth); CloseHandle(thread); }
        else std::fprintf(file, "Unavailable: OpenThread failed with Win32 error %lu\n", GetLastError());
        WriteMemoryWindow(file, process, "INSTRUCTION BYTES", faultAddress, 16, 32);
#ifdef _M_X64
        WriteMemoryWindow(file, process, "STACK MEMORY", context.Rsp, 32, 160);
#else
        WriteMemoryWindow(file, process, "STACK MEMORY", context.Esp, 32, 160);
#endif
    } else {
        std::fprintf(file, "\n[THREAD CONTEXT]\nUnavailable: could not capture TID %lu (Win32 error %lu)\n",
                     event.dwThreadId, contextError);
        std::fprintf(file, "\n[CALL STACK]\nSkipped because the thread context is unavailable.\n");
    }
    WriteCrashAnalysis(file, exceptionCode, parameters, info);
    WriteThreadList(file, process, options.pid, event.dwThreadId, options.otherThreadDepth);
    WriteModuleList(file, options.pid, moduleBase);
    const bool flushed = std::fflush(file) == 0 && std::ferror(file) == 0;
    const bool closed = std::fclose(file) == 0;
    return flushed && closed;
}

bool WriteExitReport(const Options& options, Evidence& evidence, HANDLE process, DWORD exitCode) {
    std::error_code ec;
    std::filesystem::create_directories(options.output, ec);
    SYSTEMTIME now = {}; GetLocalTime(&now);
    char fileName[MAX_PATH] = {};
    std::snprintf(fileName, sizeof(fileName), "AggressiveDog_exit_%04u-%02u-%02u_%02u-%02u-%02u_%03u_pid%lu.txt",
                  now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds, options.pid);
    const auto path = std::filesystem::path(options.output) / fileName;
    FILE* file = nullptr;
    if (fopen_s(&file, path.string().c_str(), "w") != 0 || !file) return false;
    std::string image = ProcessImagePath(process);
    if (image == "unavailable") image = evidence.imagePath;
    const char* exceptionLine = evidence.debuggerAttached
        ? "Fatal exception event: none; the target exited without a fatal exception event observed by the watchdog.\n"
        : "Fatal exception event: not observable in passive mode (debugger attach disabled).\n";
    std::fprintf(file, "AGGRESSIVEDOG PROCESS EXIT REPORT\nReport version: 3\nCaptured (local): %04u-%02u-%02u %02u:%02u:%02u.%03u\n\n[PROCESS]\nPID: %lu\nImage: %s\nWatchdog PID: %lu\nWatchdog attached: %llu ms before exit\nMode: %s\n\n[EXIT]\nExit code: 0x%08lX (%lu decimal)\nExit classification: %s\n%s",
                 now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
                 options.pid, image.c_str(), GetCurrentProcessId(),
                 static_cast<unsigned long long>(evidence.attachedAtMs && NowMs() >= evidence.attachedAtMs ?
                                                 NowMs() - evidence.attachedAtMs : 0),
                 evidence.debuggerAttached ? "debugger" : "passive monitor",
                 exitCode, exitCode, ExitCodeName(exitCode).c_str(), exceptionLine);
    WriteTerminationEvidence(file, evidence, process);
    std::fprintf(file, "\n[WATCHDOG SETTINGS]\nFaulting-thread stack depth: %lu\nOther-thread stack depth: %lu\nDump registers: %s\nCatch fast-fail: %s\nWrite minidump: %s\nRuntime log: %s\nOutput directory: %s\nINI: %s\n",
                 options.maxDepth, options.otherThreadDepth, options.dumpRegisters ? "true" : "false",
                 options.catchFastFail ? "true" : "false", options.writeMiniDump ? "true" : "false",
                 options.log.empty() ? "(not provided)" : options.log.c_str(), options.output.c_str(),
                 options.ini.empty() ? "(not provided)" : options.ini.c_str());
    WriteRecentRuntimeLog(file, options.log);
    const bool flushed = std::fflush(file) == 0 && std::ferror(file) == 0;
    const bool closed = std::fclose(file) == 0;
    return flushed && closed;
}

bool LoadSymbols(HANDLE process, const Options& options, std::string& details) {
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
    if (!SymInitialize(process, nullptr, TRUE)) {
        details = "SymInitialize failed (Win32 error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    if (options.pdb.empty()) {
        details = "No PDB path was supplied";
        return false;
    }
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, options.pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        details = "Module enumeration failed (Win32 error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    bool loaded = false;
    bool foundModule = false;
    MODULEENTRY32 module = {}; module.dwSize = sizeof(module);
    if (Module32First(snapshot, &module)) do {
        if (_stricmp(module.szModule, "Avalonium.dll") == 0) {
            foundModule = true;
            loaded = SymLoadModuleEx(process, nullptr, options.pdb.c_str(), "Avalonium.dll",
                                     reinterpret_cast<DWORD64>(module.modBaseAddr), module.modBaseSize, nullptr, 0) != 0;
            if (loaded) {
                char base[32] = {};
                std::snprintf(base, sizeof(base), "%llX",
                    static_cast<unsigned long long>(reinterpret_cast<DWORD64>(module.modBaseAddr)));
                details = std::string("Avalonium.dll symbols loaded at base 0x") + base;
            } else {
                details = "SymLoadModuleEx failed (Win32 error " + std::to_string(GetLastError()) + ")";
            }
            break;
        }
    } while (Module32Next(snapshot, &module));
    CloseHandle(snapshot);
    if (!foundModule) details = "Avalonium.dll was not present in the target module list";
    return loaded;
}

std::string Narrow(const wchar_t* text, int length) {
    if (!text || length <= 0) return {};
    char buffer[1024] = {};
    const int written = WideCharToMultiByte(CP_UTF8, 0, text, length, buffer,
                                            static_cast<int>(sizeof(buffer)) - 1, nullptr, nullptr);
    if (written <= 0) return {};
    return std::string(buffer, static_cast<size_t>(written));
}

std::string ReadDebugImageName(HANDLE process, const LOAD_DLL_DEBUG_INFO& info) {
#ifdef _M_X64
    if (!info.lpImageName) return {};
    ULONG_PTR namePointer = 0;
    if (!ReadProcessMemory(process, info.lpImageName, &namePointer, sizeof(namePointer), nullptr) || !namePointer)
        return {};
    struct RemoteUnicodeString { USHORT length; USHORT maximumLength; ULONG padding; ULONG_PTR buffer; } remote = {};
    if (!ReadProcessMemory(process, reinterpret_cast<LPCVOID>(namePointer), &remote, sizeof(remote), nullptr))
        return {};
    if (!remote.buffer || remote.length == 0 || remote.length > 1024) return {};
    wchar_t text[512] = {};
    const SIZE_T bytes = (std::min<SIZE_T>)(remote.length, sizeof(text) - sizeof(wchar_t));
    if (!ReadProcessMemory(process, reinterpret_cast<LPCVOID>(remote.buffer), text, bytes, nullptr)) return {};
    return Narrow(text, static_cast<int>(bytes / sizeof(wchar_t)));
#else
    (void)process; (void)info;
    return {};
#endif
}

std::string ReadDebugStringText(HANDLE process, const OUTPUT_DEBUG_STRING_INFO& info) {
    if (!info.lpDebugStringData || info.nDebugStringLength == 0) return {};
    if (info.fUnicode) {
        wchar_t text[256] = {};
        const SIZE_T bytes = (std::min<SIZE_T>(info.nDebugStringLength, sizeof(text) - sizeof(wchar_t)));
        if (!ReadProcessMemory(process, info.lpDebugStringData, text, bytes, nullptr)) return {};
        return Narrow(text, static_cast<int>(bytes / sizeof(wchar_t)));
    }
    char text[512] = {};
    const SIZE_T bytes = (std::min<SIZE_T>(info.nDebugStringLength, sizeof(text) - 1));
    if (!ReadProcessMemory(process, info.lpDebugStringData, text, bytes, nullptr)) return {};
    return std::string(text, bytes);
}

int Run(const Options& options) {
    AppendStatus(options, "Starting watchdog for PID " + std::to_string(options.pid) +
                          ", TID " + std::to_string(options.tid) + ", stack depth " +
                          std::to_string(options.maxDepth) + ", other-thread depth " +
                          std::to_string(options.otherThreadDepth) + ", register dump " +
                          (options.dumpRegisters ? "enabled" : "disabled") + ", fast-fail capture " +
                          (options.catchFastFail ? "enabled" : "disabled") + ", mode " +
                          (options.attachDebugger ? "debugger." : "passive monitor."));
    Evidence evidence;
    evidence.debuggerAttached = options.attachDebugger;
    evidence.terminationBreakpointEnabled = options.attachDebugger && options.terminationBreakpoint;
    const DWORD access = options.attachDebugger
        ? (PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_SUSPEND_RESUME)
        : (PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE);
    HANDLE process = OpenProcess(access, FALSE, options.pid);
    if (!process) {
        AppendStatus(options, "OpenProcess failed with Win32 error " + std::to_string(GetLastError()) + ".");
        return 2;
    }
    evidence.imagePath = ProcessImagePath(process);
    evidence.attachedAtMs = NowMs();

    if (!options.attachDebugger) {
        AppendStatus(options, "Passive monitor active (debugger attach disabled).");
        std::thread sampler([process, pid = options.pid, &evidence]() { RunSampler(process, pid, evidence); });
        auto stopSampler = [&evidence, &sampler]() {
            evidence.stop = true;
            if (sampler.joinable()) sampler.join();
        };
        const DWORD wait = WaitForSingleObject(process, INFINITE);
        DWORD exitCode = 0;
        const bool haveExitCode = GetExitCodeProcess(process, &exitCode) != FALSE;
        stopSampler();
        if (wait != WAIT_OBJECT_0 || !haveExitCode) {
            AppendStatus(options, "Passive monitor lost the target (Win32 error " + std::to_string(GetLastError()) + ").");
            CloseHandle(process);
            return 4;
        }
        {
            char text[96] = {};
            std::snprintf(text, sizeof(text), "process exit code 0x%08lX (%lu)", exitCode, exitCode);
            RecordEvent(evidence, text);
        }
        AppendStatus(options, "Target exited with code " + std::to_string(exitCode) + " (passive monitor).");
        if (exitCode != 0) {
            const bool reportWritten = WriteExitReport(options, evidence, process, exitCode);
            AppendStatus(options, reportWritten ? "Nonzero process exit report written (passive evidence)." :
                         "Nonzero process exit report could not be written.");
        }
        CloseHandle(process);
        return 0;
    }

    if (!DebugActiveProcess(options.pid)) {
        const DWORD error = GetLastError();
        AppendStatus(options, "DebugActiveProcess failed with Win32 error " + std::to_string(error) + ".");
        CloseHandle(process);
        return 3;
    }
    DebugSetProcessKillOnExit(FALSE);
    std::string symbolDetails;
    const bool pdbLoaded = LoadSymbols(process, options, symbolDetails);
    AppendStatus(options, std::string("Debugger attached; ") + symbolDetails + ".");

    BreakpointArmStats arm;
    if (options.terminationBreakpoint) {
        arm = SetTerminationBreakpoint(options.pid);
        evidence.terminationArmed = arm.armed;
        char text[256] = {};
        std::snprintf(text, sizeof(text),
                      "watchdog attached; NtTerminateProcess breakpoint %s (armed %d of %d, %d failed)",
                      arm.targetResolved ? "resolved" : "unresolved", arm.armed, arm.found, arm.failed);
        RecordEvent(evidence, text);
        AppendStatus(options, std::string(text) + ".");
    } else {
        RecordEvent(evidence, "watchdog attached; NtTerminateProcess breakpoint disabled by configuration");
        AppendStatus(options, "Debugger attached; NtTerminateProcess breakpoint disabled by configuration.");
    }

    std::thread sampler([process, pid = options.pid, &evidence]() { RunSampler(process, pid, evidence); });
    auto stopSampler = [&evidence, &sampler]() {
        evidence.stop = true;
        if (sampler.joinable()) sampler.join();
    };

    bool initialBreakpointSeen = false;
    bool crashWritten = false;
    int recordedExceptions = 0;
    DEBUG_EVENT event = {};
    while (WaitForDebugEventEx(&event, INFINITE)) {
        DWORD continueStatus = DBG_CONTINUE;
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
            const auto& exception = event.u.Exception.ExceptionRecord;
            const DWORD code = exception.ExceptionCode;
            const bool firstChance = event.u.Exception.dwFirstChance != 0;
            if (firstChance) evidence.firstChanceExceptions++;
            else evidence.secondChanceExceptions++;
            if (recordedExceptions < 32) {
                const DWORD64 address = reinterpret_cast<DWORD64>(exception.ExceptionAddress);
                DWORD64 moduleBase = 0;
                const std::string module = ModuleForAddress(process, address, moduleBase);
                char text[512] = {};
                std::snprintf(text, sizeof(text), "exception 0x%08lX (%s) %s at %s+0x%llX tid=%lu",
                              code, ExceptionName(code).c_str(), firstChance ? "first-chance" : "second-chance",
                              module.c_str(),
                              static_cast<unsigned long long>(moduleBase ? address - moduleBase : address),
                              event.dwThreadId);
                RecordEvent(evidence, text);
                ++recordedExceptions;
            } else if (recordedExceptions == 32) {
                RecordEvent(evidence, "further exception events suppressed; totals are in [TERMINATION EVIDENCE]");
                ++recordedExceptions;
            }

            const bool initBreakpoint = code == EXCEPTION_BREAKPOINT && !initialBreakpointSeen;
            if (initBreakpoint) { initialBreakpointSeen = true; }
            else if (code == EXCEPTION_SINGLE_STEP) {
                DWORD terminationExitStatus = 0;
                bool terminationExitStatusKnown = false;
                if (ConsumeTerminationBreakpoint(process, event.dwThreadId, terminationExitStatus,
                                                 terminationExitStatusKnown)) {
                    evidence.terminationHits++;
                    evidence.lastTerminationStatus = terminationExitStatus;
                    evidence.lastTerminationStatusKnown = terminationExitStatusKnown;
                    char statusText[32] = {};
                    if (terminationExitStatusKnown)
                        std::snprintf(statusText, sizeof(statusText), "0x%08lX", terminationExitStatus);
                    else
                        std::snprintf(statusText, sizeof(statusText), "unavailable");
                    char text[192] = {};
                    std::snprintf(text, sizeof(text), "in-process NtTerminateProcess hit; status=%s tid=%lu",
                                  statusText, event.dwThreadId);
                    RecordEvent(evidence, text);
                    if (terminationExitStatusKnown && terminationExitStatus == 0) {
                        AppendStatus(options, "Observed NtTerminateProcess with success exit status 0; suppressed crash report.");
                    } else {
                        FreezeThreads(options.pid, event.dwThreadId);
                        std::string reportPath;
                        std::string artifactStatus;
                        const bool reportWritten = WriteReport(options, evidence, process, event, 0xE0421001,
                            reinterpret_cast<DWORD64>(exception.ExceptionAddress), exception.NumberParameters,
                            exception.ExceptionInformation, pdbLoaded, symbolDetails, reportPath, artifactStatus);
                        AppendStatus(options, reportWritten ? "Abnormal or unknown NtTerminateProcess exit status (" +
                                     (terminationExitStatusKnown ? std::to_string(terminationExitStatus) :
                                                                  std::string("unavailable")) +
                                     "); report written: " + reportPath + "; " + artifactStatus :
                                     "Abnormal or unknown NtTerminateProcess exit status; report could not be fully written: " +
                                     reportPath + "; " + artifactStatus);
                        crashWritten = crashWritten || reportWritten;
                    }

                    continueStatus = DBG_CONTINUE;
                } else {
                    continueStatus = DBG_EXCEPTION_NOT_HANDLED;
                }
            } else if (IsFatal(code, options.catchFastFail) || !firstChance) {

                if (!firstChance) FreezeThreads(options.pid, event.dwThreadId);
                std::string reportPath;
                std::string artifactStatus;
                const bool reportWritten = WriteReport(options, evidence, process, event,
                    code,
                    reinterpret_cast<DWORD64>(exception.ExceptionAddress), exception.NumberParameters,
                    exception.ExceptionInformation, pdbLoaded, symbolDetails, reportPath, artifactStatus);
                AppendStatus(options, reportWritten ? "Crash report written: " + reportPath + "; " + artifactStatus :
                             "Crash report could not be fully written: " + reportPath +
                             "; " + artifactStatus + " (check LogDirectory path, permissions, and free disk space).");
                crashWritten = crashWritten || reportWritten;

                continueStatus = DBG_EXCEPTION_NOT_HANDLED;
            } else continueStatus = DBG_EXCEPTION_NOT_HANDLED;
        } else if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
            const DWORD exitCode = event.u.ExitProcess.dwExitCode;
            {
                char text[96] = {};
                std::snprintf(text, sizeof(text), "process exit code 0x%08lX (%lu)", exitCode, exitCode);
                RecordEvent(evidence, text);
            }
            stopSampler();
            AppendStatus(options, "Target exited with code " + std::to_string(exitCode) + ".");
            if (exitCode != 0 && !crashWritten) {
                const bool reportWritten = WriteExitReport(options, evidence, process, exitCode);
                AppendStatus(options, reportWritten ? "Nonzero process exit report written (evidence appended)." :
                             "Nonzero process exit report could not be written.");
            }
            if (exitCode == 0) DebugActiveProcessStop(options.pid);
            CloseHandle(process);
            return crashWritten ? 1 : 0;
        } else if (event.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT && event.u.CreateThread.hThread) {

            const bool armed = options.terminationBreakpoint &&
                               SetThreadTerminationBreakpoint(event.u.CreateThread.hThread, options.pid);
            if (armed) evidence.terminationArmed++;
            const char* state = !options.terminationBreakpoint ? "disabled" : armed ? "armed" : "not armed";
            char text[160] = {};
            std::snprintf(text, sizeof(text), "thread %lu created; termination breakpoint %s",
                          event.dwThreadId, state);
            RecordEvent(evidence, text);
        } else if (event.dwDebugEventCode == EXIT_THREAD_DEBUG_EVENT) {
            char text[128] = {};
            std::snprintf(text, sizeof(text), "thread %lu exited; code 0x%08lX",
                          event.dwThreadId, event.u.ExitThread.dwExitCode);
            RecordEvent(evidence, text);
        } else if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT) {
            const std::string name = ReadDebugImageName(process, event.u.LoadDll);
            char text[640] = {};
            std::snprintf(text, sizeof(text), "module loaded: %s (base=0x%llX)",
                          name.empty() ? "name unavailable" : name.c_str(),
                          static_cast<unsigned long long>(reinterpret_cast<DWORD64>(event.u.LoadDll.lpBaseOfDll)));
            RecordEvent(evidence, text);
        } else if (event.dwDebugEventCode == UNLOAD_DLL_DEBUG_EVENT) {
            char text[128] = {};
            std::snprintf(text, sizeof(text), "module unloaded: base=0x%llX",
                          static_cast<unsigned long long>(reinterpret_cast<DWORD64>(event.u.UnloadDll.lpBaseOfDll)));
            RecordEvent(evidence, text);
        } else if (event.dwDebugEventCode == OUTPUT_DEBUG_STRING_EVENT) {
            std::string message = ReadDebugStringText(process, event.u.DebugString);
            if (message.size() > 400) message = message.substr(0, 400) + " ...";
            for (char& ch : message) if (ch == '\r' || ch == '\n') ch = ' ';
            RecordEvent(evidence, "debug string: " + message);
        } else if (event.dwDebugEventCode == RIP_EVENT) {
            char text[128] = {};
            std::snprintf(text, sizeof(text), "rip info: type=%lu error=0x%08lX",
                          event.u.RipInfo.dwType, event.u.RipInfo.dwError);
            RecordEvent(evidence, text);
        }
        ContinueDebugEvent(event.dwProcessId, event.dwThreadId, continueStatus);
    }
    stopSampler();
    AppendStatus(options, "WaitForDebugEventEx failed with Win32 error " + std::to_string(GetLastError()) + ".");
    DebugActiveProcessStop(options.pid);
    CloseHandle(process);
    return 4;
}
}

int main(int argc, char** argv) {
    Options options;
    if (!Parse(argc, argv, options)) return 1;
    return Run(options);
}
