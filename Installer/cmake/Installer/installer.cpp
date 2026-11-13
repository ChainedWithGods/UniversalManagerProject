#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <atomic>
#include <functional>
#include <cstdint>
#include <algorithm>

#include "state.h"
#include "theme.h"
#include "buttons.h"
#include "WaveText.h"

extern "C" {
#include "7z.h"
#include "7zAlloc.h"
#include "7zCrc.h"
#include "7zFile.h"
}

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "version.lib")

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace fs = std::filesystem;

static const wchar_t* const APP_NAME        = L"Universal Mod Manager";
static const wchar_t* const APP_VERSION     = L"1.4.0.0";
static const wchar_t* const EXE_NAME        = L"UniversalManager.exe";
static const wchar_t* const REG_SUBKEY      = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Universal Mod Manager";
static const wchar_t* const DEFAULT_INSTALL = L"C:\\Program Files\\Universal Mod Manager";
static const wchar_t* const ARG_ELEVATED    = L"--elevated";

#define IDI_APPICON       101

static const UINT64 OVERLAY_MAGIC = 0x524D4D55564E4957ULL;

#define IDT_WAVE_TIMER 1

static const int CLIENT_W   = 720;
static const int CLIENT_H   = 500;
static const int STEPPER_H  = 44;
static const int FOOTER_H   = 62;

InstallerState g;

static UINT WM_INSTALL_PROGRESS = 0;
static UINT WM_INSTALL_DONE     = 0;

static ULONGLONG QueryFreeDiskSpace(const std::wstring& path)
{
    ULARGE_INTEGER freeBytes{};
    std::wstring root = path.size() >= 3 ? path.substr(0, 3) : path;
    GetDiskFreeSpaceExW(root.c_str(), &freeBytes, nullptr, nullptr);
    return freeBytes.QuadPart;
}

static std::wstring FormatSize(ULONGLONG bytes)
{
    wchar_t buf[64]{};
    if (bytes >= 1024ULL * 1024 * 1024) swprintf_s(buf, L"%.1f GB", bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= 1024ULL * 1024)   swprintf_s(buf, L"%.1f MB", bytes / (1024.0 * 1024));
    else swprintf_s(buf, L"%llu KB", bytes / 1024);
    return buf;
}

static bool IsRunningElevated()
{
    BOOL elevated = FALSE;
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION te{};
        DWORD size = sizeof(te);
        if (GetTokenInformation(token, TokenElevation, &te, sizeof(te), &size))
            elevated = te.TokenIsElevated;
        CloseHandle(token);
    }
    return elevated == TRUE;
}

static bool PathNeedsAdmin(const std::wstring& dir)
{
    wchar_t pf[MAX_PATH]{}, pfx86[MAX_PATH]{}, winDir[MAX_PATH]{};
    SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES,    NULL, 0, pf);
    SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILESX86, NULL, 0, pfx86);
    SHGetFolderPathW(NULL, CSIDL_WINDOWS,          NULL, 0, winDir);

    auto startsCI = [](const std::wstring& s, const wchar_t* p) -> bool {
        if (!p || !p[0]) return false;
        size_t len = wcslen(p);
        if (s.size() < len) return false;
        return _wcsnicmp(s.c_str(), p, len) == 0;
    };

    if (startsCI(dir, pf) || startsCI(dir, pfx86) || startsCI(dir, winDir)) return true;

    fs::path p(dir);
    while (!p.empty() && !fs::exists(p)) {
        fs::path parent = p.parent_path();
        if (parent == p) break;
        p = parent;
    }
    if (p.empty() || !fs::exists(p)) return true;

    fs::path probe = p / (L"._umm_probe_" + std::to_wstring(GetCurrentProcessId()) + L".tmp");
    HANDLE h = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE) return true;
    CloseHandle(h);
    DeleteFileW(probe.c_str());
    return false;
}

static bool RelaunchElevated(const std::wstring& installDir)
{
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring args = std::wstring(ARG_ELEVATED) + L" \"" + installDir + L"\"";
    if (!g.createDesktop) args += L" --no-desktop";
    if (!g.createStart)   args += L" --no-startmenu";

    SHELLEXECUTEINFOW sei{};
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb       = L"runas";
    sei.lpFile       = exePath;
    sei.lpParameters = args.c_str();
    sei.nShow        = SW_SHOW;
    if (!ShellExecuteExW(&sei)) return false;
    if (sei.hProcess) CloseHandle(sei.hProcess);
    return true;
}

static bool IsAlreadyInstalled(const std::wstring& dir)
{
    fs::path appExe = fs::path(dir) / EXE_NAME;
    return fs::exists(appExe);
}

static std::wstring GetInstalledVersion(const std::wstring& dir)
{

    HKEY hk = nullptr;
    HKEY root = HKEY_LOCAL_MACHINE;
    std::wstring version;

    if (RegOpenKeyExW(root, REG_SUBKEY, 0, KEY_READ, &hk) == ERROR_SUCCESS) {
        wchar_t buf[256]{};
        DWORD size = sizeof(buf);
        if (RegQueryValueExW(hk, L"DisplayVersion", nullptr, nullptr, (BYTE*)buf, &size) == ERROR_SUCCESS) {
            version = buf;
        }
        RegCloseKey(hk);
    }

    if (version.empty() && RegOpenKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, KEY_READ, &hk) == ERROR_SUCCESS) {
        wchar_t buf[256]{};
        DWORD size = sizeof(buf);
        if (RegQueryValueExW(hk, L"DisplayVersion", nullptr, nullptr, (BYTE*)buf, &size) == ERROR_SUCCESS) {
            version = buf;
        }
        RegCloseKey(hk);
    }

    return version;
}

static void TerminateRunningApp()
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe{sizeof(pe)};
    std::vector<DWORD> pids;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, EXE_NAME) == 0)
                pids.push_back(pe.th32ProcessID);
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    for (DWORD pid : pids) {
        HANDLE hProc = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid);
        if (!hProc) continue;
        struct EnumData { DWORD target; };
        EnumData data{pid};
        EnumWindows([](HWND hWnd, LPARAM lParam) -> BOOL {
            DWORD wndPid = 0;
            GetWindowThreadProcessId(hWnd, &wndPid);
            if (wndPid == ((EnumData*)lParam)->target && IsWindowVisible(hWnd))
                PostMessageW(hWnd, WM_CLOSE, 0, 0);
            return TRUE;
        }, (LPARAM)&data);

        if (WaitForSingleObject(hProc, 2000) != WAIT_OBJECT_0) {
            TerminateProcess(hProc, 0);
            WaitForSingleObject(hProc, 1000);
        }
        CloseHandle(hProc);
    }
}

static bool CreateShortcut(const fs::path& targetExe, const fs::path& lnkPath)
{
    IShellLinkW* psl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl)))
        return false;
    psl->SetPath(targetExe.c_str());
    psl->SetWorkingDirectory(targetExe.parent_path().c_str());
    psl->SetIconLocation(targetExe.c_str(), 0);
    std::error_code ec;
    fs::create_directories(lnkPath.parent_path(), ec);
    IPersistFile* ppf = nullptr;
    bool ok = false;
    if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
        ok = SUCCEEDED(ppf->Save(lnkPath.c_str(), TRUE));
        ppf->Release();
    }
    psl->Release();
    return ok;
}

static void CreateShortcuts(const std::wstring& installDir, bool desktop, bool startMenu)
{
    fs::path exePath = fs::path(installDir) / EXE_NAME;
    std::wstring lnkName = std::wstring(APP_NAME) + L".lnk";
    if (desktop) {
        wchar_t path[MAX_PATH]{};
        int csidl = g.isElevated ? CSIDL_COMMON_DESKTOPDIRECTORY : CSIDL_DESKTOPDIRECTORY;
        if (SUCCEEDED(SHGetFolderPathW(NULL, csidl, NULL, 0, path)))
            CreateShortcut(exePath, fs::path(path) / lnkName);
    }
    if (startMenu) {
        wchar_t path[MAX_PATH]{};
        int csidl = g.isElevated ? CSIDL_COMMON_PROGRAMS : CSIDL_PROGRAMS;
        if (SUCCEEDED(SHGetFolderPathW(NULL, csidl, NULL, 0, path)))
            CreateShortcut(exePath, fs::path(path) / APP_NAME / lnkName);
    }
}

static void WriteRegistry(const std::wstring& installDir, ULONGLONG sizeKB)
{
    HKEY hk = nullptr;
    HKEY root = g.isElevated ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    if (RegCreateKeyExW(root, REG_SUBKEY, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hk, nullptr) != ERROR_SUCCESS)
        return;
    auto SetSZ = [&](const wchar_t* name, const std::wstring& val) {
        RegSetValueExW(hk, name, 0, REG_SZ, (const BYTE*)val.c_str(), (DWORD)((val.size() + 1) * sizeof(wchar_t)));
    };
    auto SetDW = [&](const wchar_t* name, DWORD val) {
        RegSetValueExW(hk, name, 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
    };
    std::wstring exePath    = (fs::path(installDir) / EXE_NAME).wstring();
    std::wstring uninstPath = (fs::path(installDir) / L"uninstall.exe").wstring();
    SetSZ(L"DisplayName",     APP_NAME);
    SetSZ(L"DisplayVersion",  APP_VERSION);
    SetSZ(L"Publisher",       APP_NAME);
    SetSZ(L"InstallLocation", installDir);
    SetSZ(L"UninstallString", L"\"" + uninstPath + L"\"");
    SetSZ(L"DisplayIcon",     exePath + L",0");
    SetDW(L"NoModify", 1);
    SetDW(L"NoRepair", 1);
    SetDW(L"EstimatedSize", (DWORD)sizeKB);
    RegCloseKey(hk);
}

typedef struct {
    ISeekInStream  vt;
    HANDLE         hFile;
    UINT64         baseOffset;
    UINT64         archiveSize;
    UINT64         pos;
} CFileOverlayStream;

static SRes FileOverlay_Read(const ISeekInStream *p, void *buf, size_t *size)
{
    CFileOverlayStream *s = (CFileOverlayStream*)p;
    UINT64 rem = s->archiveSize - s->pos;
    if ((UINT64)*size > rem) *size = (size_t)rem;
    if (*size == 0) return SZ_OK;

    LARGE_INTEGER li;
    li.QuadPart = (LONGLONG)(s->baseOffset + s->pos);
    if (!SetFilePointerEx(s->hFile, li, nullptr, FILE_BEGIN))
        return SZ_ERROR_READ;

    DWORD bytesRead = 0;
    if (!ReadFile(s->hFile, buf, (DWORD)*size, &bytesRead, nullptr))
        return SZ_ERROR_READ;

    *size = bytesRead;
    s->pos += bytesRead;
    return SZ_OK;
}

static SRes FileOverlay_Seek(const ISeekInStream *p, Int64 *pos, ESzSeek origin)
{
    CFileOverlayStream *s = (CFileOverlayStream*)p;
    INT64 newPos = 0;
    switch (origin) {
        case SZ_SEEK_SET: newPos = *pos; break;
        case SZ_SEEK_CUR: newPos = (INT64)s->pos + *pos; break;
        case SZ_SEEK_END: newPos = (INT64)s->archiveSize + *pos; break;
        default: return SZ_ERROR_FAIL;
    }
    if (newPos < 0 || (UINT64)newPos > s->archiveSize) return SZ_ERROR_FAIL;
    s->pos = (UINT64)newPos;
    *pos   = newPos;
    return SZ_OK;
}

static bool LocateOverlay(HANDLE hFile, UINT64* outBase, UINT64* outSize)
{

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(hFile, &fileSize)) return false;
    if (fileSize.QuadPart < 16) return false;

#pragma pack(push, 1)
    struct Footer { UINT64 overlaySize; UINT64 magic; };
#pragma pack(pop)
    Footer footer{};

    LARGE_INTEGER footerPos;
    footerPos.QuadPart = fileSize.QuadPart - (LONGLONG)sizeof(Footer);
    if (!SetFilePointerEx(hFile, footerPos, nullptr, FILE_BEGIN)) return false;

    DWORD bytesRead = 0;
    if (!ReadFile(hFile, &footer, sizeof(Footer), &bytesRead, nullptr)) return false;
    if (bytesRead != sizeof(Footer)) return false;

    if (footer.magic != OVERLAY_MAGIC) return false;
    if (footer.overlaySize == 0) return false;
    if ((LONGLONG)(footer.overlaySize + sizeof(Footer)) > fileSize.QuadPart) return false;

    *outBase = (UINT64)fileSize.QuadPart - sizeof(Footer) - footer.overlaySize;
    *outSize = footer.overlaySize;
    return true;
}

static bool Extract7zOverlay(const std::wstring& destDir,
    std::function<void(int pct, const std::wstring& file)> onProgress)
{

    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    HANDLE hFile = CreateFileW(exePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    UINT64 overlayBase = 0, overlaySize = 0;
    if (!LocateOverlay(hFile, &overlayBase, &overlaySize)) {
        CloseHandle(hFile);
        return false;
    }

    CrcGenerateTable();

    CFileOverlayStream fileStream;
    fileStream.vt.Read    = FileOverlay_Read;
    fileStream.vt.Seek    = FileOverlay_Seek;
    fileStream.hFile      = hFile;
    fileStream.baseOffset = overlayBase;
    fileStream.archiveSize = overlaySize;
    fileStream.pos        = 0;

    ISzAlloc allocImp     = { SzAlloc, SzFree };
    ISzAlloc allocTempImp = { SzAllocTemp, SzFreeTemp };

    CLookToRead2 lookStream;
    LookToRead2_CreateVTable(&lookStream, False);
    lookStream.buf = (Byte*)ISzAlloc_Alloc(&allocImp, 1 << 18);
    if (!lookStream.buf) { CloseHandle(hFile); return false; }
    lookStream.bufSize = 1 << 18;
    lookStream.realStream = &fileStream.vt;
    LookToRead2_INIT(&lookStream);

    CSzArEx db;
    SzArEx_Init(&db);

    SRes res = SzArEx_Open(&db, &lookStream.vt, &allocImp, &allocTempImp);
    if (res != SZ_OK) {
        ISzAlloc_Free(&allocImp, lookStream.buf);
        CloseHandle(hFile);
        return false;
    }

    std::error_code ec;
    fs::create_directories(destDir, ec);

    UInt32 blockIndex    = 0xFFFFFFFF;
    Byte*  outBuffer     = nullptr;
    size_t outBufferSize = 0;
    std::vector<UInt16> utf16Buf;

    for (UInt32 i = 0; i < db.NumFiles; i++)
    {
        size_t len = SzArEx_GetFileNameUtf16(&db, i, nullptr);
        if (len > utf16Buf.size()) utf16Buf.resize(len);
        SzArEx_GetFileNameUtf16(&db, i, utf16Buf.data());

        std::wstring relPath((wchar_t*)utf16Buf.data());
        for (auto& ch : relPath) { if (ch == L'/') ch = L'\\'; }

        fs::path fullPath = fs::path(destDir) / relPath;

        if (SzArEx_IsDir(&db, i)) {
            fs::create_directories(fullPath, ec);
            continue;
        }

        if (fullPath.has_parent_path())
            fs::create_directories(fullPath.parent_path(), ec);

        size_t offset           = 0;
        size_t outSizeProcessed = 0;

        res = SzArEx_Extract(&db, &lookStream.vt, i,
                             &blockIndex, &outBuffer, &outBufferSize,
                             &offset, &outSizeProcessed,
                             &allocImp, &allocTempImp);

        if (res != SZ_OK) break;

        HANDLE hOut = CreateFileW(fullPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD bytesWritten = 0;
            WriteFile(hOut, outBuffer + offset, (DWORD)outSizeProcessed, &bytesWritten, nullptr);

            if (SzBitWithVals_Check(&db.MTime, i)) {
                const CNtfsFileTime* nft = &db.MTime.Vals[i];
                FILETIME ft;
                ft.dwLowDateTime  = (DWORD)(nft->Low);
                ft.dwHighDateTime = (DWORD)(nft->High);
                SetFileTime(hOut, nullptr, nullptr, &ft);
            }
            CloseHandle(hOut);
        }

        if (onProgress && db.NumFiles > 0) {
            int pct = 5 + (int)(((double)(i + 1) / db.NumFiles) * 85.0);
            onProgress(pct, fullPath.filename().wstring());
        }
    }

    ISzAlloc_Free(&allocImp, outBuffer);
    SzArEx_Free(&db, &allocImp);
    ISzAlloc_Free(&allocImp, lookStream.buf);
    CloseHandle(hFile);

    if (res == SZ_OK && onProgress)
        onProgress(92, L"");

    return (res == SZ_OK);
}

static bool Extract7zFile(const fs::path& archivePath, const fs::path& destDir,
    std::function<void(const std::wstring& file)> onProgress = nullptr)
{
    CFileInStream fileStream;
    FileInStream_CreateVTable(&fileStream);
    fileStream.wres = 0;

    if (InFile_OpenW(&fileStream.file, archivePath.c_str()) != 0)
        return false;

    ISzAlloc allocImp     = { SzAlloc, SzFree };
    ISzAlloc allocTempImp = { SzAllocTemp, SzFreeTemp };

    CLookToRead2 lookStream;
    LookToRead2_CreateVTable(&lookStream, False);
    lookStream.buf = (Byte*)ISzAlloc_Alloc(&allocImp, 1 << 18);
    if (!lookStream.buf) { File_Close(&fileStream.file); return false; }
    lookStream.bufSize    = 1 << 18;
    lookStream.realStream = &fileStream.vt;
    LookToRead2_INIT(&lookStream);

    CSzArEx db;
    SzArEx_Init(&db);

    SRes res = SzArEx_Open(&db, &lookStream.vt, &allocImp, &allocTempImp);
    if (res != SZ_OK) {
        ISzAlloc_Free(&allocImp, lookStream.buf);
        File_Close(&fileStream.file);
        return false;
    }

    std::error_code ec;
    fs::create_directories(destDir, ec);

    UInt32 blockIndex    = 0xFFFFFFFF;
    Byte*  outBuffer     = nullptr;
    size_t outBufferSize = 0;
    std::vector<UInt16> utf16Buf;

    for (UInt32 i = 0; i < db.NumFiles; i++)
    {
        size_t len = SzArEx_GetFileNameUtf16(&db, i, nullptr);
        if (len > utf16Buf.size()) utf16Buf.resize(len);
        SzArEx_GetFileNameUtf16(&db, i, utf16Buf.data());

        std::wstring relPath((wchar_t*)utf16Buf.data());
        for (auto& ch : relPath) { if (ch == L'/') ch = L'\\'; }

        fs::path fullPath = destDir / relPath;

        if (SzArEx_IsDir(&db, i)) {
            fs::create_directories(fullPath, ec);
            continue;
        }

        if (fullPath.has_parent_path())
            fs::create_directories(fullPath.parent_path(), ec);

        size_t offset           = 0;
        size_t outSizeProcessed = 0;

        res = SzArEx_Extract(&db, &lookStream.vt, i,
                             &blockIndex, &outBuffer, &outBufferSize,
                             &offset, &outSizeProcessed,
                             &allocImp, &allocTempImp);
        if (res != SZ_OK) break;

        HANDLE hOut = CreateFileW(fullPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD bytesWritten = 0;
            WriteFile(hOut, outBuffer + offset, (DWORD)outSizeProcessed, &bytesWritten, nullptr);
            if (SzBitWithVals_Check(&db.MTime, i)) {
                const CNtfsFileTime* nft = &db.MTime.Vals[i];
                FILETIME ft;
                ft.dwLowDateTime  = (DWORD)(nft->Low);
                ft.dwHighDateTime = (DWORD)(nft->High);
                SetFileTime(hOut, nullptr, nullptr, &ft);
            }
            CloseHandle(hOut);
        }

        if (onProgress) onProgress(fullPath.filename().wstring());
    }

    ISzAlloc_Free(&allocImp, outBuffer);
    SzArEx_Free(&db, &allocImp);
    ISzAlloc_Free(&allocImp, lookStream.buf);
    File_Close(&fileStream.file);

    return (res == SZ_OK);
}

static void InstallWorker()
{

    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    auto report = [&](int pct, const std::wstring& msg) {
        g.progress.store(pct);
        g.statusMsg = msg;
        if (g.hWnd) PostMessageW(g.hWnd, WM_INSTALL_PROGRESS, (WPARAM)pct, 0);
    };

    report(0,  L"Closing running instances...");
    TerminateRunningApp();
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    report(3, L"Creating installation folder...");
    std::error_code ec;
    fs::create_directories(g.installDir, ec);
    if (ec) {
        g.installOk = false;
        if (g.hWnd) PostMessageW(g.hWnd, WM_INSTALL_DONE, 0, 0);
        if (SUCCEEDED(hrCom)) CoUninitialize();
        return;
    }

    report(5, L"Unpacking payload components...");
    bool ok = Extract7zOverlay(g.installDir, [&](int pct, const std::wstring& file) {
        int scaled = 5 + (int)((double)pct * 0.85);
        report(scaled, file.empty() ? L"Extracting files..." : (L"Extracting: " + file));
    });
    if (!ok) {
        g.installOk = false;
        if (g.hWnd) PostMessageW(g.hWnd, WM_INSTALL_DONE, 0, 0);
        if (SUCCEEDED(hrCom)) CoUninitialize();
        return;
    }

    struct ToolArchive {
        const wchar_t* archiveRel;
        const wchar_t* destRel;
        const wchar_t* label;
    };

    static const ToolArchive kToolArchives[] = {
        { L"Tools\\PakViewer.7z",
          L"Tools\\PakViewer",
          L"PakViewer" },
        { L"Tools\\ModConflictDetector.7z",
          L"Tools\\ModConflictDetector",
          L"ModConflictDetector" },
        { L"Tools\\Avalonium\\Runtime\\DLL.7z",
          L"Tools\\Avalonium\\Runtime\\DLL",
          L"Avalonium Runtime DLL" },
    };

    for (const auto& ta : kToolArchives)
    {
        fs::path archivePath = fs::path(g.installDir) / ta.archiveRel;
        if (!fs::exists(archivePath)) continue;

        report(g.progress.load(), std::wstring(L"Unpacking ") + ta.label + L"...");

        fs::path destPath = fs::path(g.installDir) / ta.destRel;
        bool toolOk = Extract7zFile(archivePath, destPath,
            [&](const std::wstring& file) {
                report(g.progress.load(),
                    file.empty() ? (std::wstring(L"Extracting ") + ta.label + L"...")
                                 : (L"Extracting: " + file));
            });

        if (toolOk) {

            std::error_code ec2;
            fs::remove(archivePath, ec2);
        }
    }

    report(92, L"Configuring system shortcuts...");
    CreateShortcuts(g.installDir, g.createDesktop, g.createStart);

    report(96, L"Registering uninstaller in Windows...");
    ULONGLONG totalKB = 0;
    for (auto& e : fs::recursive_directory_iterator(g.installDir, ec)) {
        if (!e.is_directory(ec)) totalKB += e.file_size(ec) / 1024;
    }
    WriteRegistry(g.installDir, totalKB);

    report(100, L"Installation completed successfully!");
    g.installOk = true;
    if (g.hWnd) PostMessageW(g.hWnd, WM_INSTALL_DONE, 0, 0);

    if (SUCCEEDED(hrCom)) CoUninitialize();
}

static void DrawStepper(HDC hdc, int w)
{

    RECT rBorder = { 0, STEPPER_H - 1, w, STEPPER_H };
    FillRect_(hdc, rBorder, CLR_BORDER);

    struct StepInfo {
        const wchar_t* name;
        int circleX;
        int textX;
        int textW;
    };

    const StepInfo steps[4] = {
        { L"Welcome",  96, 122, 60 },
        { L"Folder",  252, 278, 48 },
        { L"Install", 400, 426, 48 },
        { L"Done",    548, 574, 40 }
    };

    int cy = STEPPER_H / 2;

    HPEN linePen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HGDIOBJ oldPen = SelectObject(hdc, linePen);
    MoveToEx(hdc, 192, cy, nullptr); LineTo(hdc, 240, cy);
    MoveToEx(hdc, 336, cy, nullptr); LineTo(hdc, 388, cy);
    MoveToEx(hdc, 484, cy, nullptr); LineTo(hdc, 536, cy);
    SelectObject(hdc, oldPen);
    DeleteObject(linePen);

    for (int i = 0; i < 4; i++) {
        int cx = steps[i].circleX + 10;
        bool isCompleted = (g.page > i) || (g.page == 3 && g.installOk);
        bool isActive    = (g.page == i) && !(g.page == 3 && g.installOk);

        if (isCompleted) {

            RECT rc = { cx - 10, cy - 10, cx + 10, cy + 10 };
            FillCircleAA(hdc, cx, cy, 10, CLR_SUCCESS, CLR_SUCCESS, 1);

            HPEN chkPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
            HGDIOBJ op = SelectObject(hdc, chkPen);
            MoveToEx(hdc, cx - 4, cy, nullptr);
            LineTo(hdc, cx - 1, cy + 3);
            LineTo(hdc, cx + 5, cy - 3);
            SelectObject(hdc, op);
            DeleteObject(chkPen);

            RECT rt = { steps[i].textX, cy - 10, steps[i].textX + steps[i].textW + 12, cy + 10 };
            DrawTextLeft(hdc, steps[i].name, rt, CLR_TEXT, g_fontBody);
        } else if (isActive) {

            RECT rc = { cx - 10, cy - 10, cx + 10, cy + 10 };
            FillCircleAA(hdc, cx, cy, 10, CLR_ACCENT_BG, CLR_ACCENT, 1);

            wchar_t numStr[2] = { (wchar_t)(L'1' + i), 0 };
            DrawTextLeft(hdc, numStr, rc, RGB(255, 255, 255), g_fontStep, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            RECT rt = { steps[i].textX, cy - 10, steps[i].textX + steps[i].textW + 12, cy + 10 };
            DrawTextLeft(hdc, steps[i].name, rt, CLR_TEXT, g_fontBold);
        } else {

            RECT rc = { cx - 10, cy - 10, cx + 10, cy + 10 };
            FillCircleAA(hdc, cx, cy, 10, CLR_STEP_INACT, CLR_BORDER, 1);

            wchar_t numStr[2] = { (wchar_t)(L'1' + i), 0 };
            DrawTextLeft(hdc, numStr, rc, CLR_MUTED, g_fontStep, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            RECT rt = { steps[i].textX, cy - 10, steps[i].textX + steps[i].textW + 12, cy + 10 };
            DrawTextLeft(hdc, steps[i].name, rt, CLR_MUTED, g_fontBody);
        }
    }
}

static void DrawPage0_Welcome(HDC hdc, const RECT& rc)
{

    RECT rHero = { 32, 58, CLIENT_W - 32, 142 };
    DrawRoundRect(hdc, rHero, 8, CLR_NO_FILL, CLR_BORDER, 1);

    HICON hIcon = (HICON)LoadImageW(g.hInst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
                                     256, 256, LR_DEFAULTCOLOR);
    if (hIcon) {

        int oldMode = SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, 0, 0, nullptr);

        DrawIconEx(hdc, 50, 72, hIcon, 64, 64, 0, nullptr, DI_NORMAL);

        SetStretchBltMode(hdc, oldMode);
        DestroyIcon(hIcon);
    }

    RECT rHeroTitle = { 126, 73, 410, 103 };
    {
        COLORREF grad[5];
        wavetext::CozyTitleGradient(grad);
        wavetext::DrawWave(hdc, APP_NAME, rHeroTitle, g_fontHero, grad, 5);
    }

    RECT rBadge = { 412, 78, 506, 100 };
    FillRoundRectAA(hdc, rBadge, 11, CLR_ACCENT_BG, CLR_ACCENT, 1);
    DrawTextLeft(hdc, L"Version 1.4.0", rBadge, RGB(255, 255, 255), g_fontBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT rPublisher = { 126, 106, CLIENT_W - 52, 126 };
    {
        COLORREF grad[5];
        wavetext::CozySubtitleGradient(grad);
        wavetext::DrawWave(hdc, L"Universal Manager Project", rPublisher, g_fontSub, grad, 5);
    }

    RECT rCard = { 32, 154, CLIENT_W - 32, 424 };
    DrawRoundRect(hdc, rCard, 8, CLR_NO_FILL, CLR_BORDER, 1);

    RECT rTitle = { 54, 170, CLIENT_W - 54, 192 };
    DrawTextLeft(hdc, L"Welcome to the Setup Wizard", rTitle, CLR_TEXT, g_fontTitle);

    RECT rSub = { 54, 196, CLIENT_W - 54, 216 };
    DrawTextLeft(hdc, L"Next-Gen Modding Platform • Unreal Engine 4.0 → 5.8", rSub, CLR_TEXT_SEC, g_fontSub);

    struct BulletItem {
        COLORREF dotColor;
        const wchar_t* title;
        int y;
    };

    const BulletItem bullets[4] = {
        {
            CLR_CYAN_GLOW,
            L"Game Presets Manager, Conflict Detection, 3D Viewer, Asset browser",
            228
        },
        {
            CLR_ACCENT,
            L"Unreal Engine Console, Lua Modding Scripting, AES-Keys Extraction, Live Memory Editor",
            260
        },
        {
            CLR_PURPLE_DOT,
            L"Mods Collections, Favorites Mods, Installed Mods, Disabled Mods, Mods Folder Switching",
            292
        },
        {
            CLR_CYAN,
            L"Intelignce System Intilialization, SDK Generation, USMAP Generation, In-Game GUI",
            324
        }
    };

    for (const auto& b : bullets) {
        int dotY = b.y + 7;
        RECT rDot = { 56, dotY, 62, dotY + 6 };
        DrawRoundRect(hdc, rDot, 3, b.dotColor, b.dotColor, 1);

        RECT rText = { 72, b.y, CLIENT_W - 54, b.y + 20 };
        DrawTextLeft(hdc, b.title, rText, CLR_TEXT, g_fontBold);
    }

    RECT rInset = { 54, 368, CLIENT_W - 54, 408 };
    DrawRoundRect(hdc, rInset, 6, CLR_INSET_BG, CLR_BORDER, 1);

    RECT rInsetText = { 68, 368, CLIENT_W - 68, 408 };
    DrawTextLeft(hdc, L"Click Next to continue, or Cancel to exit Setup.", rInsetText, CLR_MUTED, g_fontSub);
}

static void DrawPage1_ChooseDir(HDC hdc, const RECT& rc)
{

    RECT rSubHead = { 32, 58, CLIENT_W - 32, 82 };
    DrawTextLeft(hdc, L"Select Installation Location", rSubHead, CLR_TEXT, g_fontTitle);
    RECT rSubDesc = { 32, 84, CLIENT_W - 32, 106 };
    DrawTextLeft(hdc, L"Setup will install the application into the following folder.", rSubDesc, CLR_TEXT_SEC, g_fontSub);

    RECT rCard = { 32, 116, CLIENT_W - 32, 424 };
    DrawRoundRect(hdc, rCard, 8, CLR_NO_FILL, CLR_BORDER, 1);

    RECT rLabel = { 54, 134, CLIENT_W - 54, 154 };
    DrawTextLeft(hdc, L"Destination Directory:", rLabel, CLR_TEXT, g_fontBold);

    RECT rEditOutline = { 52, 158, CLIENT_W - 162, 194 };
    DrawRoundRect(hdc, rEditOutline, 6, CLR_INPUT_BG, CLR_BORDER_LT, 1);

    RECT rShortcutsBox = { 52, 212, CLIENT_W - 52, 344 };
    DrawRoundRect(hdc, rShortcutsBox, 6, CLR_NO_FILL, CLR_BORDER, 1);

    RECT rScLabel = { 68, 224, CLIENT_W - 68, 244 };
    DrawTextLeft(hdc, L"Additional Shortcuts:", rScLabel, CLR_TEXT_SEC, g_fontBold);

    ULONGLONG fr = QueryFreeDiskSpace(g.installDir);
    RECT rReq = { 54, 380, 260, 404 };
    DrawTextLeft(hdc, L"Space required: ~850 MB", rReq, CLR_MUTED, g_fontSub);

    std::wstring frStr = L"Space available: " + FormatSize(fr);
    RECT rAvail = { CLIENT_W - 254, 380, CLIENT_W - 54, 404 };
    COLORREF diskColor = (fr >= 1024ULL * 1024 * 900) ? CLR_CYAN : RGB(0xFB, 0xBF, 0x24);
    DrawTextLeft(hdc, frStr.c_str(), rAvail, diskColor, g_fontBold, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

static void DrawPage2_Installing(HDC hdc, const RECT& rc)
{

    RECT rSubHead = { 32, 58, CLIENT_W - 32, 82 };
    DrawTextLeft(hdc, L"Installing Universal Mod Manager...", rSubHead, CLR_TEXT, g_fontTitle);
    RECT rSubDesc = { 32, 84, CLIENT_W - 32, 106 };
    DrawTextLeft(hdc, L"Please wait while components are extracted and registered on your computer.", rSubDesc, CLR_TEXT_SEC, g_fontSub);

    RECT rCard = { 32, 116, CLIENT_W - 32, 424 };
    DrawRoundRect(hdc, rCard, 8, CLR_NO_FILL, CLR_BORDER, 1);

    RECT rStatusTitle = { 54, 146, CLIENT_W - 160, 170 };
    DrawTextLeft(hdc, L"EXTRACTING COMPONENTS", rStatusTitle, CLR_DIM, g_fontBadge);

    int curPct = std::clamp(g.progress.load(), 0, 100);
    wchar_t pctBuf[16]{};
    swprintf_s(pctBuf, L"%d%%", curPct);
    RECT rPctBadge = { CLIENT_W - 180, 142, CLIENT_W - 54, 170 };
    DrawTextLeft(hdc, pctBuf, rPctBadge, CLR_ACCENT, g_fontTitle, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    RECT rMsg = { 54, 184, CLIENT_W - 54, 210 };
    DrawTextLeft(hdc, g.statusMsg.c_str(), rMsg, CLR_TEXT_SEC, g_fontBody, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT rTrack = { 54, 222, CLIENT_W - 54, 242 };
    DrawRoundRect(hdc, rTrack, 8, CLR_INPUT_BG, CLR_BORDER_LT, 1);

    int fillW = (int)((rTrack.right - rTrack.left - 4) * (curPct / 100.0));
    if (fillW > 6) {
        RECT rFill = { rTrack.left + 2, rTrack.top + 2, rTrack.left + 2 + fillW, rTrack.bottom - 2 };
        DrawRoundRect(hdc, rFill, 6, CLR_ACCENT_BG, CLR_ACCENT, 1);
    }
}

static void DrawPage3_Done(HDC hdc, const RECT& rc)
{
    int cx = CLIENT_W / 2;
    int cy = 118;
    int r  = 26;
    RECT circleR = { cx - r, cy - r, cx + r, cy + r };

    if (g.installOk) {
        FillCircleAA(hdc, cx, cy, r, CLR_SUCCESS, CLR_SUCCESS, 1);

        DrawLineAA(hdc, cx - 9, cy,      cx - 2, cy + 8, RGB(255, 255, 255), 3);
        DrawLineAA(hdc, cx - 2, cy + 8,  cx + 10, cy - 6, RGB(255, 255, 255), 3);

        RECT rTitle = { 32, 158, CLIENT_W - 32, 188 };
        DrawTextLeft(hdc, L"Installation Complete!", rTitle, CLR_TEXT, g_fontHero, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT rSub = { 32, 192, CLIENT_W - 32, 216 };
        DrawTextLeft(hdc, L"Universal Mod Manager has been successfully installed on your computer.", rSub, CLR_MUTED, g_fontSub, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT rSummary = { 64, 234, CLIENT_W - 64, 304 };
        DrawRoundRect(hdc, rSummary, 8, CLR_NO_FILL, CLR_BORDER, 1);
        RECT rSumTitle = { 84, 246, CLIENT_W - 84, 266 };
        DrawTextLeft(hdc, L"INSTALLATION DIRECTORY", rSumTitle, CLR_DIM, g_fontBadge);
        RECT rSumPath = { 84, 270, CLIENT_W - 84, 294 };
        DrawTextLeft(hdc, g.installDir.c_str(), rSumPath, CLR_TEXT_SEC, g_fontBody, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        FillCircleAA(hdc, cx, cy, r, RGB(0xEF, 0x44, 0x44), RGB(0xEF, 0x44, 0x44), 1);

        DrawLineAA(hdc, cx - 8, cy - 8, cx + 8, cy + 8, RGB(255, 255, 255), 3);
        DrawLineAA(hdc, cx + 8, cy - 8, cx - 8, cy + 8, RGB(255, 255, 255), 3);

        RECT rTitle = { 32, 158, CLIENT_W - 32, 188 };
        DrawTextLeft(hdc, L"Installation Failed", rTitle, RGB(0xF8, 0x71, 0x71), g_fontHero, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT rSub = { 32, 192, CLIENT_W - 32, 222 };
        DrawTextLeft(hdc, L"An error occurred during extraction. Check disk permissions and space.", rSub, CLR_MUTED, g_fontSub, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void DestroyPageControls()
{
    auto D = [](HWND& h) { if (h) { DestroyWindow(h); h = nullptr; } };
    D(g_hEditPath);
    D(g_hBtnBrowse);
    D(g_hChkDesktop);
    D(g_hChkStart);
    D(g_hChkLaunch);
}

static void BuildPage1Controls(HWND hWnd)
{

    g_hEditPath = CreateWindowExW(0, L"EDIT", g.installDir.c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        64, 169, CLIENT_W - 240, 18, hWnd, (HMENU)IDC_EDIT_PATH, g.hInst, nullptr);
    SendMessageW(g_hEditPath, WM_SETFONT, (WPARAM)g_fontBody, TRUE);

    g_hBtnBrowse = MakeButton(hWnd, L"Browse...", CLIENT_W - 150, 159, 98, 34, IDC_BTN_BROWSE);

    g_hChkDesktop = MakeOwnerDrawCheck(hWnd, L"Create a Desktop shortcut",
        66, 254, CLIENT_W - 136, 32, IDC_CHK_DESKTOP);
    g_hChkStart   = MakeOwnerDrawCheck(hWnd, L"Create a Start Menu shortcut",
        66, 294, CLIENT_W - 136, 32, IDC_CHK_STARTMENU);
}

static void BuildPage2Controls(HWND)
{

}

static void BuildPage3Controls(HWND hWnd)
{
    if (g.installOk) {
        g_hChkLaunch = MakeOwnerDrawCheck(hWnd, L"Launch Universal Mod Manager now",
            64, 324, CLIENT_W - 128, 34, IDC_CHK_LAUNCH);
    }
}

static void SyncPageState()
{
    if (g_hEditPath) {
        wchar_t buf[MAX_PATH]{};
        GetWindowTextW(g_hEditPath, buf, MAX_PATH);
        g.installDir = buf;
    }
}

static void NavigateTo(int page)
{
    SyncPageState();
    DestroyPageControls();
    g.page = page;

    ShowWindow(g_hBtnBack, (page == 1) ? SW_SHOW : SW_HIDE);
    EnableWindow(g_hBtnBack, (page == 1));

    ShowWindow(g_hBtnCancel, (page <= 2) ? SW_SHOW : SW_HIDE);
    EnableWindow(g_hBtnCancel, (page != 2));

    EnableWindow(g_hBtnNext, (page != 2));
    const wchar_t* nextLbl = (page == 1) ? L"Install" : (page == 2) ? L"Installing..." : (page == 3) ? L"Finish" : L"Next >";
    SetWindowTextW(g_hBtnNext, nextLbl);

    switch (page) {
    case 1: BuildPage1Controls(g.hWnd); break;
    case 2: BuildPage2Controls(g.hWnd); break;
    case 3: BuildPage3Controls(g.hWnd); break;
    }

    InvalidateRect(g.hWnd, nullptr, TRUE);
    UpdateWindow(g.hWnd);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_INSTALL_PROGRESS) {

        RECT rCard = { 32, 116, CLIENT_W - 32, 424 };
        InvalidateRect(hWnd, &rCard, FALSE);
        return 0;
    }
    if (msg == WM_INSTALL_DONE) {
        NavigateTo(3);
        return 0;
    }

    switch (msg) {
    case WM_CREATE:
    {
        g.hWnd = hWnd;
        EnableDarkTitle(hWnd);
        SetRoundedCorners(hWnd);

        int bY = CLIENT_H - FOOTER_H + (FOOTER_H - 36) / 2;

        g_hBtnCancel = MakeButton(hWnd, L"Cancel", 32, bY, 96, 36, IDC_BTN_CANCEL);

        g_hBtnNext   = MakeButton(hWnd, L"Next >", CLIENT_W - 32 - 110, bY, 110, 36, IDC_BTN_NEXT);

        g_hBtnBack   = MakeButton(hWnd, L"< Back", CLIENT_W - 32 - 110 - 10 - 96, bY, 96, 36, IDC_BTN_BACK);

        ShowWindow(g_hBtnBack, SW_HIDE);

        SetTimer(hWnd, IDT_WAVE_TIMER, 33, nullptr);

        if (g.startOnInstallPage) {
            NavigateTo(2);
            std::thread(InstallWorker).detach();
        } else {
            NavigateTo(0);
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        int w = rc.right, h = rc.bottom;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, bmp);

        FillGradientV(memDC, rc, CLR_BG_GRAD_TOP, CLR_BG_GRAD_BOTTOM);

        DrawStepper(memDC, w);

        switch (g.page) {
        case 0: DrawPage0_Welcome(memDC, rc); break;
        case 1: DrawPage1_ChooseDir(memDC, rc); break;
        case 2: DrawPage2_Installing(memDC, rc); break;
        case 3: DrawPage3_Done(memDC, rc); break;
        }

        RECT rFBorder = { 0, h - FOOTER_H, w, h - FOOTER_H + 1 };
        FillRect_(memDC, rFBorder, CLR_BORDER);

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(bmp);
        DeleteDC(memDC);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_INPUT_BG);
        static HBRUSH hBr = CreateSolidBrush(CLR_INPUT_BG);
        return (LRESULT)hBr;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, CLR_MUTED);
        SetBkMode(hdc, TRANSPARENT);
        static HBRUSH hBr = CreateSolidBrush(CLR_BG);
        return (LRESULT)hBr;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if (dis->CtlType == ODT_BUTTON) PaintODButton(dis);
        return TRUE;
    }

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);

        if (id == IDC_CHK_DESKTOP) {
            g.createDesktop = !g.createDesktop;
            InvalidateRect(g_hChkDesktop, nullptr, FALSE);
            return 0;
        }
        if (id == IDC_CHK_STARTMENU) {
            g.createStart = !g.createStart;
            InvalidateRect(g_hChkStart, nullptr, FALSE);
            return 0;
        }
        if (id == IDC_CHK_LAUNCH) {
            g.launchApp = !g.launchApp;
            InvalidateRect(g_hChkLaunch, nullptr, FALSE);
            return 0;
        }

        if (id == IDC_BTN_CANCEL) {
            if (g.page == 2) return 0;
            DestroyWindow(hWnd);
            return 0;
        }
        if (id == IDC_BTN_BACK && g.page == 1) {
            NavigateTo(0);
            return 0;
        }
        if (id == IDC_BTN_NEXT) {
            if (g.page == 0) {
                NavigateTo(1);
            } else if (g.page == 1) {
                SyncPageState();

                fs::path selectedPath(g.installDir);

                if (_wcsicmp(selectedPath.filename().c_str(), APP_NAME) != 0) {

                    selectedPath = selectedPath / APP_NAME;
                    g.installDir = selectedPath.wstring();
                }

                if (IsAlreadyInstalled(g.installDir)) {
                    std::wstring version = GetInstalledVersion(g.installDir);
                    std::wstring msg = L"Universal Mod Manager is already installed in this location";
                    if (!version.empty()) {
                        msg += L" (Version " + version + L")";
                    }
                    msg += L".\n\nDo you want to:\n\n";
                    msg += L"• YES - Reinstall/Update (existing files will be replaced)\n";
                    msg += L"• NO - Choose a different installation folder\n";
                    msg += L"• CANCEL - Exit the installer";

                    int result = MessageBoxW(hWnd, msg.c_str(), APP_NAME,
                                            MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2);

                    if (result == IDCANCEL) {
                        DestroyWindow(hWnd);
                        return 0;
                    } else if (result == IDNO) {
                        return 0;
                    }

                }

                if (!g.isElevated && PathNeedsAdmin(g.installDir)) {
                    if (!RelaunchElevated(g.installDir)) {
                        MessageBoxW(hWnd,
                            L"Administrator privileges are required to install into this folder.\n\n"
                            L"Please choose a different destination, or run the installer as administrator.",
                            APP_NAME, MB_ICONWARNING | MB_OK);
                        return 0;
                    }
                    DestroyWindow(hWnd);
                    return 0;
                }
                NavigateTo(2);
                std::thread(InstallWorker).detach();
            } else if (g.page == 3) {
                SyncPageState();
                if (g.installOk && g.launchApp) {
                    fs::path appExe = fs::path(g.installDir) / EXE_NAME;
                    ShellExecuteW(nullptr, L"open", appExe.c_str(), nullptr,
                                  appExe.parent_path().c_str(), SW_SHOW);
                }
                DestroyWindow(hWnd);
            }
            return 0;
        }
        if (id == IDC_BTN_BROWSE && g_hEditPath) {
            IFileDialog* pfd = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileDialog, (void**)&pfd))) {
                DWORD opts = 0; pfd->GetOptions(&opts);
                pfd->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
                pfd->SetTitle(L"Choose installation folder");
                if (SUCCEEDED(pfd->Show(hWnd))) {
                    IShellItem* psi = nullptr;
                    if (SUCCEEDED(pfd->GetResult(&psi))) {
                        PWSTR path = nullptr;
                        if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &path))) {

                            fs::path selectedPath(path);
                            if (_wcsicmp(selectedPath.filename().c_str(), APP_NAME) != 0) {
                                selectedPath = selectedPath / APP_NAME;
                            }
                            g.installDir = selectedPath.wstring();
                            SetWindowTextW(g_hEditPath, g.installDir.c_str());
                            CoTaskMemFree(path);
                            InvalidateRect(hWnd, nullptr, FALSE);
                        }
                        psi->Release();
                    }
                }
                pfd->Release();
            }
            return 0;
        }
        return 0;
    }

    case WM_CLOSE:
        if (g.page == 2) return 0;
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, IDT_WAVE_TIMER);
        PostQuitMessage(0);
        return 0;

    case WM_TIMER:
        if (wParam == IDT_WAVE_TIMER && g.page == 0) {

            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int)
{

    typedef BOOL(WINAPI* SetProcessDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        auto setDpi = (SetProcessDpiAwarenessContextProc)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (setDpi) setDpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }

    g.hInst = hInstance;
    WM_INSTALL_PROGRESS = RegisterWindowMessageW(L"UMM_INSTALL_PROGRESS");
    WM_INSTALL_DONE     = RegisterWindowMessageW(L"UMM_INSTALL_DONE");

    std::wstring cmdLine = lpCmdLine ? lpCmdLine : L"";
    size_t elevPos = cmdLine.find(ARG_ELEVATED);
    if (elevPos != std::wstring::npos) {
        g.isElevated = true;
        g.startOnInstallPage = true;
        size_t q1 = cmdLine.find(L'"', elevPos);
        if (q1 != std::wstring::npos) {
            size_t q2 = cmdLine.find(L'"', q1 + 1);
            if (q2 != std::wstring::npos)
                g.installDir = cmdLine.substr(q1 + 1, q2 - q1 - 1);
        }
        if (cmdLine.find(L"--no-desktop")   != std::wstring::npos) g.createDesktop = false;
        if (cmdLine.find(L"--no-startmenu") != std::wstring::npos) g.createStart   = false;
    } else {
        g.isElevated = IsRunningElevated();
        wchar_t pf[MAX_PATH]{};
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, 0, pf))) {
            g.installDir = (fs::path(pf) / APP_NAME).wstring();
        }
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icex{sizeof(icex), ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS | ICC_BAR_CLASSES};
    InitCommonControlsEx(&icex);

    GdiplusInit();

    wavetext::Init();

    g_fontHero  = CreateFontPt(18, FW_BOLD);
    g_fontTitle = CreateFontPt(13, FW_BOLD);
    g_fontSub   = CreateFontPt(9,  FW_NORMAL);
    g_fontBody  = CreateFontPt(9,  FW_NORMAL);
    g_fontBold  = CreateFontPt(9,  FW_BOLD);
    g_fontSmall = CreateFontPt(8,  FW_NORMAL);
    g_fontBadge = CreateFontPt(8,  FW_BOLD);
    g_fontStep  = CreateFontPt(8,  FW_BOLD);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hIconSm       = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(CLR_BG);
    wc.lpszClassName = L"UMMSetupWnd";
    RegisterClassExW(&wc);

    RECT wr = { 0, 0, CLIENT_W, CLIENT_H };

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    DWORD exStyle = WS_EX_APPWINDOW;
    AdjustWindowRectEx(&wr, style, FALSE, exStyle);
    int winW = wr.right - wr.left;
    int winH = wr.bottom - wr.top;
    int sW = GetSystemMetrics(SM_CXSCREEN), sH = GetSystemMetrics(SM_CYSCREEN);

    HWND hWnd = CreateWindowExW(exStyle, L"UMMSetupWnd",
        L"Universal Mod Manager Setup",
        style,
        (sW - winW) / 2, (sH - winH) / 2, winW, winH,
        nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return 1;

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_fontHero);
    DeleteObject(g_fontTitle);
    DeleteObject(g_fontSub);
    DeleteObject(g_fontBody);
    DeleteObject(g_fontBold);
    DeleteObject(g_fontSmall);
    DeleteObject(g_fontBadge);
    DeleteObject(g_fontStep);
    wavetext::Shutdown();
    GdiplusShutdown();
    CoUninitialize();
    return (int)msg.wParam;
}
