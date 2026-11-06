#pragma once

#include <windows.h>
#include "state.h"
#include "theme.h"

#define IDC_BTN_BACK      1001
#define IDC_BTN_NEXT      1002
#define IDC_BTN_CANCEL    1003
#define IDC_EDIT_PATH     1004
#define IDC_BTN_BROWSE    1005
#define IDC_CHK_DESKTOP   1006
#define IDC_CHK_STARTMENU 1007
#define IDC_CHK_LAUNCH    1008

extern HWND g_hBtnBack;
extern HWND g_hBtnNext;
extern HWND g_hBtnCancel;

extern HWND g_hEditPath;
extern HWND g_hBtnBrowse;
extern HWND g_hChkDesktop;
extern HWND g_hChkStart;
extern HWND g_hChkLaunch;

HWND MakeButton        (HWND parent, const wchar_t* text, int x, int y, int w, int h, int id);
HWND MakeOwnerDrawCheck(HWND parent, const wchar_t* text, int x, int y, int w, int h, int id);

void PaintODButton(DRAWITEMSTRUCT* dis);
