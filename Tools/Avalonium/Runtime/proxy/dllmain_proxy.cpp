#include <atomic>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <string>

#include <windows.h>

namespace fs = std::filesystem;

HMODULE SOriginalDll = nullptr;

extern "C" uintptr_t mProcs[113] = {0};

static std::atomic<bool> g_StopTail{false};
static HANDLE g_TailThread = nullptr;

static std::mutex g_LogMutex;
static FILE* g_LogFile = nullptr;
static std::wstring g_LogPath;

static void LogLine(const char* msg)
{
    if (!msg) return;
    std::lock_guard<std::mutex> lock(g_LogMutex);

    OutputDebugStringA(msg);
    OutputDebugStringA("\n");

    if (g_LogFile) {
        SYSTEMTIME st{};
        GetLocalTime(&st);
        std::fprintf(g_LogFile,
                     "[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s\n",
                     st.wYear, st.wMonth, st.wDay,
                     st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                     msg);
        std::fflush(g_LogFile);
    }
}

static void LogWinError(const char* what, DWORD err = GetLastError())
{
    char buf[512];
    LPSTR msg = nullptr;
    const DWORD len = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&msg), 0, nullptr);
    if (len && msg) {
        _snprintf_s(buf, _TRUNCATE, "%s failed (GetLastError=%lu): %s",
                    what, err, msg);
        LocalFree(msg);
    } else {
        _snprintf_s(buf, _TRUNCATE, "%s failed (GetLastError=%lu)", what, err);
    }
    LogLine(buf);
}

static void OpenLog(HMODULE self)
{
    wchar_t self_path[MAX_PATH] = {0};
    if (GetModuleFileNameW(self, self_path, MAX_PATH) == 0) {
        return;
    }
    fs::path log_path = fs::path(self_path).parent_path() / L"dwmapi_proxy.log";
    g_LogPath = log_path.wstring();

    g_LogFile = _wfsopen(g_LogPath.c_str(), L"w+t", _SH_DENYNO);
    if (!g_LogFile) {
        OutputDebugStringA("dwmapi_proxy: failed to open log file\n");
    }
}

static void CloseLog()
{
    {
        std::lock_guard<std::mutex> lock(g_LogMutex);
        if (g_LogFile) {
            std::fclose(g_LogFile);
            g_LogFile = nullptr;
        }
    }
    g_LogPath.clear();
}

static size_t setup_functions()
{
    auto get = [](LPCSTR name) -> uintptr_t {
        return reinterpret_cast<uintptr_t>(GetProcAddress(SOriginalDll, name));
    };

    mProcs[0]   = get("DwmpDxGetWindowSharedSurface");
    mProcs[1]   = get("DwmpDxUpdateWindowSharedSurface");
    mProcs[2]   = get("DwmEnableComposition");
    mProcs[3]   = get(MAKEINTRESOURCEA(103));
    mProcs[4]   = get(MAKEINTRESOURCEA(104));
    mProcs[5]   = get(MAKEINTRESOURCEA(105));
    mProcs[6]   = get(MAKEINTRESOURCEA(106));
    mProcs[7]   = get(MAKEINTRESOURCEA(107));
    mProcs[8]   = get(MAKEINTRESOURCEA(108));
    mProcs[9]   = get(MAKEINTRESOURCEA(109));
    mProcs[10]  = get(MAKEINTRESOURCEA(110));
    mProcs[11]  = get("DllCanUnloadNow");
    mProcs[12]  = get(MAKEINTRESOURCEA(112));
    mProcs[13]  = get(MAKEINTRESOURCEA(113));
    mProcs[14]  = get(MAKEINTRESOURCEA(114));
    mProcs[15]  = get("DllGetClassObject");
    mProcs[16]  = get("DwmAttachMilContent");
    mProcs[17]  = get("DwmDefWindowProc");
    mProcs[18]  = get("DwmDetachMilContent");
    mProcs[19]  = get("DwmEnableBlurBehindWindow");
    mProcs[20]  = get("DwmEnableMMCSS");
    mProcs[21]  = get("DwmExtendFrameIntoClientArea");
    mProcs[22]  = get("DwmFlush");
    mProcs[23]  = get("DwmGetColorizationColor");
    mProcs[24]  = get(MAKEINTRESOURCEA(124));
    mProcs[25]  = get("DwmGetCompositionTimingInfo");
    mProcs[26]  = get("DwmGetGraphicsStreamClient");
    mProcs[27]  = get("DwmpGetColorizationParameters");
    mProcs[28]  = get("DwmpDxgiIsThreadDesktopComposited");
    mProcs[29]  = get("DwmGetGraphicsStreamTransformHint");
    mProcs[30]  = get("DwmGetTransportAttributes");
    mProcs[31]  = get("DwmpSetColorizationParameters");
    mProcs[32]  = get(MAKEINTRESOURCEA(132));
    mProcs[33]  = get("DwmGetUnmetTabRequirements");
    mProcs[34]  = get("DwmGetWindowAttribute");
    mProcs[35]  = get("DwmpRenderFlick");
    mProcs[36]  = get("DwmpAllocateSecurityDescriptor");
    mProcs[37]  = get("DwmpFreeSecurityDescriptor");
    mProcs[38]  = get(MAKEINTRESOURCEA(138));
    mProcs[39]  = get(MAKEINTRESOURCEA(139));
    mProcs[40]  = get(MAKEINTRESOURCEA(140));
    mProcs[41]  = get(MAKEINTRESOURCEA(141));
    mProcs[42]  = get(MAKEINTRESOURCEA(142));
    mProcs[43]  = get("DwmpEnableDDASupport");
    mProcs[44]  = get(MAKEINTRESOURCEA(144));
    mProcs[45]  = get(MAKEINTRESOURCEA(145));
    mProcs[46]  = get("DwmInvalidateIconicBitmaps");
    mProcs[47]  = get(MAKEINTRESOURCEA(147));
    mProcs[48]  = get(MAKEINTRESOURCEA(148));
    mProcs[49]  = get("DwmIsCompositionEnabled");
    mProcs[50]  = get(MAKEINTRESOURCEA(150));
    mProcs[51]  = get(MAKEINTRESOURCEA(151));
    mProcs[52]  = get(MAKEINTRESOURCEA(152));
    mProcs[53]  = get(MAKEINTRESOURCEA(153));
    mProcs[54]  = get(MAKEINTRESOURCEA(154));
    mProcs[55]  = get(MAKEINTRESOURCEA(155));
    mProcs[56]  = get("DwmTetherTextContact");
    mProcs[57]  = get(MAKEINTRESOURCEA(157));
    mProcs[58]  = get(MAKEINTRESOURCEA(158));
    mProcs[59]  = get(MAKEINTRESOURCEA(159));
    mProcs[60]  = get(MAKEINTRESOURCEA(160));
    mProcs[61]  = get(MAKEINTRESOURCEA(161));
    mProcs[62]  = get(MAKEINTRESOURCEA(162));
    mProcs[63]  = get(MAKEINTRESOURCEA(163));
    mProcs[64]  = get(MAKEINTRESOURCEA(164));
    mProcs[65]  = get(MAKEINTRESOURCEA(165));
    mProcs[66]  = get(MAKEINTRESOURCEA(166));
    mProcs[67]  = get(MAKEINTRESOURCEA(167));
    mProcs[68]  = get(MAKEINTRESOURCEA(168));
    mProcs[69]  = get(MAKEINTRESOURCEA(169));
    mProcs[70]  = get(MAKEINTRESOURCEA(170));
    mProcs[71]  = get(MAKEINTRESOURCEA(171));
    mProcs[72]  = get(MAKEINTRESOURCEA(172));
    mProcs[73]  = get(MAKEINTRESOURCEA(173));
    mProcs[74]  = get(MAKEINTRESOURCEA(174));
    mProcs[75]  = get(MAKEINTRESOURCEA(175));
    mProcs[76]  = get(MAKEINTRESOURCEA(176));
    mProcs[77]  = get(MAKEINTRESOURCEA(177));
    mProcs[78]  = get(MAKEINTRESOURCEA(178));
    mProcs[79]  = get(MAKEINTRESOURCEA(179));
    mProcs[80]  = get(MAKEINTRESOURCEA(180));
    mProcs[81]  = get(MAKEINTRESOURCEA(181));
    mProcs[82]  = get(MAKEINTRESOURCEA(182));
    mProcs[83]  = get("DwmpUpdateProxyWindowForCapture");
    mProcs[84]  = get(MAKEINTRESOURCEA(184));
    mProcs[85]  = get(MAKEINTRESOURCEA(185));
    mProcs[86]  = get(MAKEINTRESOURCEA(186));
    mProcs[87]  = get(MAKEINTRESOURCEA(187));
    mProcs[88]  = get(MAKEINTRESOURCEA(188));
    mProcs[89]  = get(MAKEINTRESOURCEA(189));
    mProcs[90]  = get(MAKEINTRESOURCEA(190));
    mProcs[91]  = get(MAKEINTRESOURCEA(191));
    mProcs[92]  = get(MAKEINTRESOURCEA(192));
    mProcs[93]  = get(MAKEINTRESOURCEA(193));
    mProcs[94]  = get(MAKEINTRESOURCEA(194));
    mProcs[95]  = get(MAKEINTRESOURCEA(195));
    mProcs[96]  = get(MAKEINTRESOURCEA(196));
    mProcs[97]  = get(MAKEINTRESOURCEA(197));
    mProcs[98]  = get(MAKEINTRESOURCEA(198));
    mProcs[99]  = get("DwmModifyPreviousDxFrameDuration");
    mProcs[100] = get("DwmQueryThumbnailSourceSize");
    mProcs[101] = get("DwmRegisterThumbnail");
    mProcs[102] = get("DwmRenderGesture");
    mProcs[103] = get("DwmSetDxFrameDuration");
    mProcs[104] = get("DwmSetIconicLivePreviewBitmap");
    mProcs[105] = get("DwmSetIconicThumbnail");
    mProcs[106] = get("DwmSetPresentParameters");
    mProcs[107] = get("DwmSetWindowAttribute");
    mProcs[108] = get("DwmShowContact");
    mProcs[109] = get("DwmTetherContact");
    mProcs[110] = get("DwmTransitionOwnedWindow");
    mProcs[111] = get("DwmUnregisterThumbnail");
    mProcs[112] = get("DwmUpdateThumbnailProperties");

    size_t resolved = 0;
    for (uintptr_t p : mProcs) {
        if (p) ++resolved;
    }
    return resolved;
}

static void load_original_dll()
{
    wchar_t sysdir[MAX_PATH] = {0};
    const UINT n = GetSystemDirectoryW(sysdir, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        LogWinError("GetSystemDirectoryW");
        return;
    }

    std::wstring dll_path = std::wstring(sysdir) + L"\\dwmapi.dll";
    SOriginalDll = LoadLibraryW(dll_path.c_str());
    if (!SOriginalDll) {
        char buf[512];
        _snprintf_s(buf, _TRUNCATE, "Failed to load system dwmapi.dll at %ls",
                    dll_path.c_str());
        LogWinError(buf);
    }
}

static DWORD WINAPI ManagerLoaderThread(LPVOID param)
{
    HMODULE self = static_cast<HMODULE>(param);
    wchar_t self_path[MAX_PATH] = {0};
    if (GetModuleFileNameW(self, self_path, MAX_PATH) == 0) {
        LogWinError("GetModuleFileNameW");
        return 1;
    }

    const fs::path self_dir = fs::path(self_path).parent_path();

    static constexpr const wchar_t* kCandidates[] = {
        L"Avalonium.dll",
        L"Avalonium-Proxied.dll",
    };

    for (const wchar_t* name : kCandidates) {
        const fs::path full = self_dir / name;
        char msg[MAX_PATH * 2];
        _snprintf_s(msg, _TRUNCATE, "Trying to load %ls", full.c_str());
        LogLine(msg);

        HMODULE h = LoadLibraryExW(full.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (h) {
            _snprintf_s(msg, _TRUNCATE,
                        "Loaded %ls successfully (handle=%p)", name, h);
            LogLine(msg);
            return 0;
        }
        LogWinError("LoadLibraryExW");
    }

    char msg[256];
    _snprintf_s(msg, _TRUNCATE,
                "Avalonium.dll not found in %ls",
                self_dir.c_str());
    LogLine(msg);
    return 0;
}

static DWORD WINAPI LogTailThread(LPVOID param)
{
    HMODULE self = static_cast<HMODULE>(param);
    wchar_t self_path[MAX_PATH] = {0};
    if (GetModuleFileNameW(self, self_path, MAX_PATH) == 0) {
        return 1;
    }
    const fs::path um_log = fs::path(self_path).parent_path() / L"Avalonium.log";

    HANDLE file = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 300 && !g_StopTail; ++i) {
        file = CreateFileW(um_log.c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) break;
        Sleep(100);
    }
    if (file == INVALID_HANDLE_VALUE || g_StopTail) {
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        return 0;
    }

    LARGE_INTEGER fileSize{};
    GetFileSizeEx(file, &fileSize);
    long long lastPos = 0;

    constexpr DWORD kBufSize = 4096;
    char buffer[kBufSize];
    std::string lineBuf;

    {
        char msg[MAX_PATH * 2];
        _snprintf_s(msg, _TRUNCATE, "Tailing %ls", um_log.c_str());
        LogLine(msg);
    }

    while (!g_StopTail) {
        Sleep(50);

        GetFileSizeEx(file, &fileSize);
        const long long curSize = fileSize.QuadPart;

        if (curSize < lastPos) {
            CloseHandle(file);
            file = CreateFileW(um_log.c_str(), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE) {
                Sleep(500);
                file = CreateFileW(um_log.c_str(), GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file == INVALID_HANDLE_VALUE) continue;
            }
            GetFileSizeEx(file, &fileSize);
            lastPos = 0;
            lineBuf.clear();
            continue;
        }

        if (curSize == lastPos) continue;

        LARGE_INTEGER dist{};
        dist.QuadPart = lastPos;
        SetFilePointerEx(file, dist, nullptr, FILE_BEGIN);

        const long long remaining = curSize - lastPos;
        const DWORD toRead = static_cast<DWORD>(remaining < (long long)kBufSize - 1
                                                     ? remaining
                                                     : (long long)kBufSize - 1);
        DWORD bytesRead = 0;
        if (!ReadFile(file, buffer, toRead, &bytesRead, nullptr) || bytesRead == 0) {
            continue;
        }
        buffer[bytesRead] = '\0';
        lastPos += bytesRead;

        lineBuf.append(buffer, bytesRead);
        size_t nl;
        while ((nl = lineBuf.find('\n')) != std::string::npos) {
            std::string line = lineBuf.substr(0, nl);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) {
                LogLine(line.c_str());
            }
            lineBuf.erase(0, nl + 1);
        }
    }

    if (!lineBuf.empty()) {
        std::string line = lineBuf;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) {
            LogLine(line.c_str());
        }
    }

    CloseHandle(file);
    return 0;
}

BOOL WINAPI DllMain(HMODULE hInstDll, DWORD fdwReason, LPVOID lpvReserved)
{
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInstDll);

        OpenLog(hInstDll);
        {
            char msg[1024];
            wchar_t self_path[MAX_PATH] = {0};
            GetModuleFileNameW(hInstDll, self_path, MAX_PATH);
            _snprintf_s(msg, _TRUNCATE,
                        "DLL_PROCESS_ATTACH pid=%lu, dwmapi_proxy at %ls",
                        GetCurrentProcessId(), self_path);
            LogLine(msg);
        }

        load_original_dll();
        if (SOriginalDll) {
            char msg[256];
            _snprintf_s(msg, _TRUNCATE,
                        "Loaded system dwmapi.dll (handle=%p)", SOriginalDll);
            LogLine(msg);
        } else {
            LogLine("WARNING: system dwmapi.dll was not loaded - "
                    "forwarded calls will fail");
        }

        const size_t resolved = setup_functions();
        {
            char msg[128];
            _snprintf_s(msg, _TRUNCATE,
                        "Resolved %zu / %zu exports from system dwmapi.dll",
                        resolved, sizeof(mProcs) / sizeof(mProcs[0]));
            LogLine(msg);
        }

        HANDLE thread = CreateThread(nullptr, 0, ManagerLoaderThread, hInstDll, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        } else {
            LogWinError("CreateThread");
        }

        g_StopTail = false;
        g_TailThread = CreateThread(nullptr, 0, LogTailThread, hInstDll, 0, nullptr);
        if (!g_TailThread) {
            LogWinError("CreateThread (LogTailThread)");
        }

        LogLine("Proxy ready - forwarding dwmapi calls to the system DLL, "
                "and tailing Avalonium.log into this file");
    } else if (fdwReason == DLL_PROCESS_DETACH) {

        g_StopTail = true;
        if (g_TailThread) {
            WaitForSingleObject(g_TailThread, 2000);
            CloseHandle(g_TailThread);
            g_TailThread = nullptr;
        }
        if (SOriginalDll) {
            FreeLibrary(SOriginalDll);
            SOriginalDll = nullptr;
        }
        LogLine("DLL_PROCESS_DETACH");
        CloseLog();
    }
    return TRUE;
}
