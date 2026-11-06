#pragma once

#include <windows.h>
#include <string>
#include <atomic>

struct InstallerState
{
    int              page = 0;
    std::wstring     installDir         = L"C:\\Program Files\\Universal Mod Manager";
    bool             createDesktop      = true;
    bool             createStart        = true;
    bool             launchApp          = true;
    bool             isElevated         = false;
    bool             startOnInstallPage = false;
    bool             installOk          = false;

    HWND             hWnd               = nullptr;
    HINSTANCE        hInst              = nullptr;

    std::atomic<int> progress{0};
    std::atomic<bool> installDone{false};

    std::wstring     statusMsg          = L"Preparing installation...";
};

extern InstallerState g;
