#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <thread>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace fs = std::filesystem;

static const wchar_t* const APP_NAME = L"Universal Mod Manager";
static const wchar_t* const EXE_NAME = L"UniversalManager.exe";
static const wchar_t* const REG_UNINSTALL_SUBKEY = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Universal Mod Manager";
static const wchar_t* const WORKER_ARG = L"--worker";

typedef DWORD(WINAPI* pfnRmStartSession)(DWORD*, DWORD, WCHAR[]);
typedef DWORD(WINAPI* pfnRmRegisterResources)(DWORD, UINT, LPCWSTR[], UINT, const void*, UINT, const void*);
typedef DWORD(WINAPI* pfnRmGetList)(DWORD, UINT*, UINT*, void*, DWORD*);
typedef DWORD(WINAPI* pfnRmShutdown)(DWORD, DWORD, void*);
typedef DWORD(WINAPI* pfnRmEndSession)(DWORD);

#ifndef CCH_RM_SESSION_KEY
#define CCH_RM_SESSION_KEY 32
#endif

#ifndef RmForceShutdown
#define RmForceShutdown 0x1
#endif

static void EnableDarkMode(HWND hWnd)
{
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hWnd, 20, &darkMode, sizeof(darkMode));
    DwmSetWindowAttribute(hWnd, 19, &darkMode, sizeof(darkMode));
}

static void TerminateRunningApp(const fs::path& installDir)
{
    HMODULE hRm = LoadLibraryW(L"rstrtmgr.dll");
    if (!hRm) return;

    auto pRmStartSession = (pfnRmStartSession)GetProcAddress(hRm, "RmStartSession");
    auto pRmRegisterResources = (pfnRmRegisterResources)GetProcAddress(hRm, "RmRegisterResources");
    auto pRmGetList = (pfnRmGetList)GetProcAddress(hRm, "RmGetList");
    auto pRmShutdown = (pfnRmShutdown)GetProcAddress(hRm, "RmShutdown");
    auto pRmEndSession = (pfnRmEndSession)GetProcAddress(hRm, "RmEndSession");

    if (pRmStartSession && pRmRegisterResources && pRmGetList && pRmShutdown && pRmEndSession)
    {
        DWORD dwSession = 0;
        WCHAR szSessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };

        if (pRmStartSession(&dwSession, 0, szSessionKey) == ERROR_SUCCESS)
        {
            fs::path mainExe = installDir / EXE_NAME;
            LPCWSTR rgsFiles[] = { mainExe.c_str() };

            if (pRmRegisterResources(dwSession, 1, rgsFiles, 0, NULL, 0, NULL) == ERROR_SUCCESS)
            {
                UINT pnProcInfoNeeded = 0;
                UINT pnProcInfo = 0;
                DWORD dwRebootFlags = 0;

                if (pRmGetList(dwSession, &pnProcInfoNeeded, &pnProcInfo, NULL, &dwRebootFlags) == ERROR_MORE_DATA && pnProcInfoNeeded > 0)
                {
                    pRmShutdown(dwSession, RmForceShutdown, NULL);
                }
            }
            pRmEndSession(dwSession);
        }
    }
    FreeLibrary(hRm);
}

static void RemoveShortcuts()
{
    wchar_t path[MAX_PATH];

    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, path)))
    {
        std::error_code ec;
        fs::remove(fs::path(path) / (std::wstring(APP_NAME) + L".lnk"), ec);
    }

    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, 0, path)))
    {
        std::error_code ec;
        fs::remove(fs::path(path) / (std::wstring(APP_NAME) + L".lnk"), ec);
    }

    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, path)))
    {
        std::error_code ec;
        fs::remove_all(fs::path(path) / APP_NAME, ec);
        fs::remove(fs::path(path) / (std::wstring(APP_NAME) + L".lnk"), ec);
    }

    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_PROGRAMS, NULL, 0, path)))
    {
        std::error_code ec;
        fs::remove_all(fs::path(path) / APP_NAME, ec);
        fs::remove(fs::path(path) / (std::wstring(APP_NAME) + L".lnk"), ec);
    }
}

static void RemoveRegistryKeys()
{
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, REG_UNINSTALL_SUBKEY);
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, REG_UNINSTALL_SUBKEY);

    RegDeleteTreeW(HKEY_CURRENT_USER, REG_UNINSTALL_SUBKEY);
    RegDeleteKeyW(HKEY_CURRENT_USER, REG_UNINSTALL_SUBKEY);
}

static std::wstring GetInstallDirFromRegistry()
{
    wchar_t pathBuffer[MAX_PATH] = { 0 };
    DWORD bufferSize = sizeof(pathBuffer);
    HKEY hKey;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_UNINSTALL_SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        RegQueryValueExW(hKey, L"InstallLocation", NULL, NULL, (LPBYTE)pathBuffer, &bufferSize);
        RegCloseKey(hKey);
    }

    if (wcslen(pathBuffer) == 0 && RegOpenKeyExW(HKEY_LOCAL_MACHINE, REG_UNINSTALL_SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        bufferSize = sizeof(pathBuffer);
        RegQueryValueExW(hKey, L"InstallLocation", NULL, NULL, (LPBYTE)pathBuffer, &bufferSize);
        RegCloseKey(hKey);
    }

    return std::wstring(pathBuffer);
}

static bool RemoveInstallFolderFully(const fs::path& installDir)
{
    std::error_code ec;
    if (!fs::exists(installDir, ec)) return true;

    for (const auto& entry : fs::recursive_directory_iterator(installDir, fs::directory_options::skip_permission_denied, ec))
    {
        SetFileAttributesW(entry.path().c_str(), FILE_ATTRIBUTE_NORMAL);
    }
    SetFileAttributesW(installDir.c_str(), FILE_ATTRIBUTE_NORMAL);

    fs::remove_all(installDir, ec);

    for (int retry = 0; retry < 10; ++retry)
    {
        if (!fs::exists(installDir, ec)) break;
        SetFileAttributesW(installDir.c_str(), FILE_ATTRIBUTE_NORMAL);
        if (RemoveDirectoryW(installDir.c_str()))
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    SHChangeNotify(SHCNE_RMDIR, SHCNF_PATHW, installDir.c_str(), NULL);
    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, installDir.parent_path().c_str(), NULL);

    if (fs::exists(installDir, ec))
    {
        MoveFileExW(installDir.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        return false;
    }

    return true;
}

static bool RelaunchFromTemp(const std::wstring& installDir, bool silent)
{
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    wchar_t tempDir[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tempDir);

    wchar_t tempExe[MAX_PATH]{};
    swprintf_s(tempExe, L"%sUMM_Uninstaller_%lu.exe", tempDir, GetCurrentProcessId());

    if (!CopyFileW(exePath, tempExe, FALSE)) return false;

    std::wstring args = std::wstring(WORKER_ARG) + L" \"" + installDir + L"\"";
    if (silent) args += L" /SILENT";

    SHELLEXECUTEINFOW sei{};
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb       = L"open";
    sei.lpFile       = tempExe;
    sei.lpParameters = args.c_str();
    sei.lpDirectory  = tempDir;
    sei.nShow        = SW_SHOW;
    if (!ShellExecuteExW(&sei))
    {
        DeleteFileW(tempExe);
        return false;
    }
    if (sei.hProcess) CloseHandle(sei.hProcess);
    return true;
}

struct ProgressCallbackData
{
    HWND hDialog;
    std::wstring installDir;
    std::wstring selfExePath;
    bool success;
};

static HRESULT CALLBACK TaskDialogCallback(HWND hWnd, UINT uNotification, WPARAM wParam, LPARAM lParam, LONG_PTR dwRefData)
{
    if (uNotification == TDN_CREATED)
    {
        EnableDarkMode(hWnd);
        SendMessageW(hWnd, TDM_SET_MARQUEE_PROGRESS_BAR, TRUE, 0);
        SendMessageW(hWnd, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 30);

        ProgressCallbackData* data = reinterpret_cast<ProgressCallbackData*>(dwRefData);
        data->hDialog = hWnd;

        std::thread([data]() {

            std::this_thread::sleep_for(std::chrono::milliseconds(400));

            SendMessageW(data->hDialog, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, (LPARAM)L"Closing running applications...");
            TerminateRunningApp(data->installDir);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            SendMessageW(data->hDialog, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, (LPARAM)L"Removing shortcuts...");
            RemoveShortcuts();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            SendMessageW(data->hDialog, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, (LPARAM)L"Cleaning Windows registry...");
            RemoveRegistryKeys();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            SendMessageW(data->hDialog, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, (LPARAM)L"Deleting installed files and folder...");
            data->success = RemoveInstallFolderFully(data->installDir);
            std::this_thread::sleep_for(std::chrono::milliseconds(400));

            PostMessageW(data->hDialog, TDM_CLICK_BUTTON, IDOK, 0);
        }).detach();
    }
    return S_OK;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int)
{

    wchar_t tempDirBuf[MAX_PATH]{};
    if (GetTempPathW(MAX_PATH, tempDirBuf) > 0)
    {
        SetCurrentDirectoryW(tempDirBuf);
    }

    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icex);

    std::wstring cmdLine = lpCmdLine ? lpCmdLine : L"";
    bool silent = (cmdLine.find(L"/SILENT") != std::wstring::npos) ||
                  (cmdLine.find(L"/VERYSILENT") != std::wstring::npos) ||
                  (cmdLine.find(L"-silent") != std::wstring::npos);

    wchar_t currentExePathBuf[MAX_PATH];
    GetModuleFileNameW(NULL, currentExePathBuf, MAX_PATH);
    fs::path currentExePath = currentExePathBuf;

    size_t workerPos = cmdLine.find(WORKER_ARG);
    if (workerPos != std::wstring::npos)
    {
        std::wstring installDirStr;
        size_t argStart = workerPos + wcslen(WORKER_ARG);
        while (argStart < cmdLine.size() && (cmdLine[argStart] == L' ' || cmdLine[argStart] == L'\t'))
            argStart++;

        if (argStart < cmdLine.size() && cmdLine[argStart] == L'"')
        {
            size_t quoteEnd = cmdLine.find(L'"', argStart + 1);
            if (quoteEnd != std::wstring::npos)
                installDirStr = cmdLine.substr(argStart + 1, quoteEnd - argStart - 1);
            else
                installDirStr = cmdLine.substr(argStart + 1);
        }
        else if (argStart < cmdLine.size())
        {
            size_t spaceEnd = cmdLine.find_first_of(L" \t", argStart);
            if (spaceEnd != std::wstring::npos)
                installDirStr = cmdLine.substr(argStart, spaceEnd - argStart);
            else
                installDirStr = cmdLine.substr(argStart);
        }

        if (installDirStr.empty())
            return 1;

        ProgressCallbackData progressData = {};
        progressData.installDir = installDirStr;
        progressData.selfExePath = currentExePath.wstring();
        progressData.success = true;

        if (!silent)
        {
            TASKDIALOGCONFIG tdProgress = { sizeof(tdProgress) };
            tdProgress.hInstance = hInstance;
            tdProgress.dwFlags = TDF_SHOW_PROGRESS_BAR | TDF_CALLBACK_TIMER;
            tdProgress.pszWindowTitle = APP_NAME;
            tdProgress.pszMainIcon = TD_INFORMATION_ICON;
            tdProgress.pszMainInstruction = L"Uninstalling Universal Mod Manager...";
            tdProgress.pszContent = L"Preparing uninstallation...";
            tdProgress.pfCallback = TaskDialogCallback;
            tdProgress.lpCallbackData = reinterpret_cast<LONG_PTR>(&progressData);

            TaskDialogIndirect(&tdProgress, NULL, NULL, NULL);

            TASKDIALOGCONFIG tdDone = { sizeof(tdDone) };
            tdDone.hInstance = hInstance;
            tdDone.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW;
            tdDone.pszWindowTitle = APP_NAME;
            tdDone.pszMainIcon = TD_INFORMATION_ICON;
            tdDone.pszMainInstruction = L"Uninstall Complete";
            tdDone.pszContent = L"Universal Mod Manager was successfully removed from your computer.";
            tdDone.dwCommonButtons = TDCBF_OK_BUTTON;
            tdDone.pfCallback = [](HWND hWnd, UINT msg, WPARAM, LPARAM, LONG_PTR) -> HRESULT {
                if (msg == TDN_CREATED) EnableDarkMode(hWnd);
                return S_OK;
            };

            TaskDialogIndirect(&tdDone, NULL, NULL, NULL);
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            TerminateRunningApp(installDirStr);
            RemoveShortcuts();
            RemoveRegistryKeys();
            RemoveInstallFolderFully(installDirStr);
        }

        MoveFileExW(currentExePath.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        return 0;
    }

    std::wstring installDirStr = GetInstallDirFromRegistry();
    if (installDirStr.empty())
    {
        installDirStr = currentExePath.parent_path().wstring();
    }

    if (!silent)
    {
        TASKDIALOGCONFIG tdConfirm = { sizeof(tdConfirm) };
        tdConfirm.hInstance = hInstance;
        tdConfirm.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
        tdConfirm.pszWindowTitle = APP_NAME;
        tdConfirm.pszMainIcon = TD_WARNING_ICON;
        tdConfirm.pszMainInstruction = L"Uninstall Universal Mod Manager";
        tdConfirm.pszContent = L"Are you sure you want to completely remove Universal Mod Manager and all of its components?";
        tdConfirm.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
        tdConfirm.nDefaultButton = IDNO;
        tdConfirm.pfCallback = [](HWND hWnd, UINT msg, WPARAM, LPARAM, LONG_PTR) -> HRESULT {
            if (msg == TDN_CREATED) EnableDarkMode(hWnd);
            return S_OK;
        };

        int nButton = 0;
        if (FAILED(TaskDialogIndirect(&tdConfirm, &nButton, NULL, NULL)) || nButton != IDYES)
        {
            return 0;
        }
    }

    if (RelaunchFromTemp(installDirStr, silent))
    {
        return 0;
    }

    ProgressCallbackData progressData = {};
    progressData.installDir = installDirStr;
    progressData.selfExePath = currentExePath.wstring();
    progressData.success = true;

    if (!silent)
    {
        TASKDIALOGCONFIG tdProgress = { sizeof(tdProgress) };
        tdProgress.hInstance = hInstance;
        tdProgress.dwFlags = TDF_SHOW_PROGRESS_BAR | TDF_CALLBACK_TIMER;
        tdProgress.pszWindowTitle = APP_NAME;
        tdProgress.pszMainIcon = TD_INFORMATION_ICON;
        tdProgress.pszMainInstruction = L"Uninstalling Universal Mod Manager...";
        tdProgress.pszContent = L"Preparing uninstallation...";
        tdProgress.pfCallback = TaskDialogCallback;
        tdProgress.lpCallbackData = reinterpret_cast<LONG_PTR>(&progressData);

        TaskDialogIndirect(&tdProgress, NULL, NULL, NULL);

        TASKDIALOGCONFIG tdDone = { sizeof(tdDone) };
        tdDone.hInstance = hInstance;
        tdDone.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW;
        tdDone.pszWindowTitle = APP_NAME;
        tdDone.pszMainIcon = TD_INFORMATION_ICON;
        tdDone.pszMainInstruction = L"Uninstall Complete";
        tdDone.pszContent = L"Universal Mod Manager was successfully removed from your computer.";
        tdDone.dwCommonButtons = TDCBF_OK_BUTTON;
        tdDone.pfCallback = [](HWND hWnd, UINT msg, WPARAM, LPARAM, LONG_PTR) -> HRESULT {
            if (msg == TDN_CREATED) EnableDarkMode(hWnd);
            return S_OK;
        };

        TaskDialogIndirect(&tdDone, NULL, NULL, NULL);
    }
    else
    {
        TerminateRunningApp(installDirStr);
        RemoveShortcuts();
        RemoveRegistryKeys();
        RemoveInstallFolderFully(installDirStr);
    }

    return 0;
}
