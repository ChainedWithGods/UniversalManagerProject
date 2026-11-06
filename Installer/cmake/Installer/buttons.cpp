#include "buttons.h"
#include <commctrl.h>

HWND g_hBtnBack   = nullptr;
HWND g_hBtnNext   = nullptr;
HWND g_hBtnCancel = nullptr;

HWND g_hEditPath   = nullptr;
HWND g_hBtnBrowse  = nullptr;
HWND g_hChkDesktop = nullptr;
HWND g_hChkStart   = nullptr;
HWND g_hChkLaunch  = nullptr;

static LRESULT CALLBACK ButtonSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
    switch (uMsg) {
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hWnd, 0 };
        TrackMouseEvent(&tme);
        if (!GetPropW(hWnd, L"HOT")) {
            SetPropW(hWnd, L"HOT", (HANDLE)1);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        break;
    }
    case WM_MOUSELEAVE: {
        RemovePropW(hWnd, L"HOT");
        InvalidateRect(hWnd, nullptr, FALSE);
        break;
    }
    case WM_NCDESTROY:
        RemovePropW(hWnd, L"HOT");
        break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static void PaintODCheckbox(DRAWITEMSTRUCT* dis, bool hot)
{
    HDC  hdc = dis->hDC;
    RECT r   = dis->rcItem;
    int  id  = dis->CtlID;

    bool checked = false;
    if (id == IDC_CHK_DESKTOP)   checked = g.createDesktop;
    if (id == IDC_CHK_STARTMENU) checked = g.createStart;
    if (id == IDC_CHK_LAUNCH)    checked = g.launchApp;

    if (hot) {
        DrawRoundRect(hdc, r, 6, CLR_CARD_HOV, CLR_BORDER_LT, 1);
    } else {
        FillRect_(hdc, r, CLR_CARD);
    }

    int boxSize = 18;
    int boxX    = r.left + 8;
    int boxY    = r.top + (r.bottom - r.top - boxSize) / 2;
    RECT boxR   = { boxX, boxY, boxX + boxSize, boxY + boxSize };

    if (checked) {
        COLORREF boxBg = hot ? CLR_ACCENT_HOV : CLR_ACCENT_BG;
        FillRoundRectAA(hdc, boxR, 5, boxBg, CLR_ACCENT, 1);

        DrawLineAA(hdc, boxX + 4, boxY + 9,  boxX + 7, boxY + 13, RGB(255, 255, 255), 2);
        DrawLineAA(hdc, boxX + 7, boxY + 13, boxX + 14, boxY + 5,  RGB(255, 255, 255), 2);
    } else {
        COLORREF boxBorder = hot ? CLR_ACCENT : CLR_BORDER_LT;
        FillRoundRectAA(hdc, boxR, 5, CLR_INPUT_BG, boxBorder, 1);
    }

    wchar_t txt[128]{};
    GetWindowTextW(dis->hwndItem, txt, 128);
    RECT textR = { boxX + boxSize + 12, r.top, r.right - 8, r.bottom };
    SetTextColor(hdc, hot ? CLR_TEXT : CLR_TEXT_SEC);
    SetBkMode   (hdc, TRANSPARENT);
    HGDIOBJ of  = SelectObject(hdc, g_fontBody);
    DrawTextW   (hdc, txt, -1, &textR, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, of);
}

static void PaintODButtonImpl(DRAWITEMSTRUCT* dis)
{
    HDC  hdc  = dis->hDC;
    RECT r    = dis->rcItem;
    bool sel  = (dis->itemState & ODS_SELECTED) != 0;
    bool dis_ = (dis->itemState & ODS_DISABLED) != 0;
    bool hot  = (dis->itemState & ODS_HOTLIGHT) != 0 || GetPropW(dis->hwndItem, L"HOT") != nullptr;

    int id = dis->CtlID;

    if (id == IDC_CHK_DESKTOP || id == IDC_CHK_STARTMENU || id == IDC_CHK_LAUNCH) {
        PaintODCheckbox(dis, hot);
        return;
    }

    bool isPrimary = (id == IDC_BTN_NEXT);
    bool isSuccess = isPrimary && (g.page == 3 && g.installOk);

    COLORREF bg, border, fg;
    if (dis_) {
        bg     = CLR_BTN_SEC_DIS;
        border = CLR_BORDER_LT;
        fg     = CLR_DIM;
    } else if (isSuccess) {
        bg     = sel ? CLR_SUCCESS_DOWN : (hot ? CLR_SUCCESS_HOV : CLR_SUCCESS);
        border = hot ? RGB(0x6E, 0xE7, 0xB7) : CLR_SUCCESS;
        fg     = RGB(0xFF, 0xFF, 0xFF);
    } else if (isPrimary) {
        bg     = sel ? CLR_ACCENT_DOWN : (hot ? CLR_ACCENT_HOV : CLR_ACCENT_BG);
        border = hot ? RGB(0x7D, 0xD3, 0xFC) : CLR_ACCENT_BG;
        fg     = RGB(0xFF, 0xFF, 0xFF);
    } else {
        bg     = sel ? CLR_BTN_SEC_DOWN : (hot ? CLR_BTN_SEC_HOV : CLR_BTN_SEC);
        border = hot ? CLR_ACCENT : CLR_BORDER_LT;
        fg     = hot ? CLR_TEXT : CLR_TEXT_SEC;
    }

    DrawRoundRect(hdc, r, 7, bg, border, 1);

    wchar_t txt[64]{};
    GetWindowTextW(dis->hwndItem, txt, 64);
    SetTextColor(hdc, fg);
    SetBkMode   (hdc, TRANSPARENT);
    HGDIOBJ of  = SelectObject(hdc, isPrimary ? g_fontBold : g_fontBody);
    DrawTextW   (hdc, txt, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, of);
}

HWND MakeButton(HWND parent, const wchar_t* text, int x, int y, int w, int h, int id)
{
    HWND hw = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | BS_OWNERDRAW,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, g.hInst, nullptr);
    SetWindowSubclass(hw, ButtonSubclassProc, 0, 0);
    return hw;
}

HWND MakeOwnerDrawCheck(HWND parent, const wchar_t* text, int x, int y, int w, int h, int id)
{
    HWND hw = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | BS_OWNERDRAW,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, g.hInst, nullptr);
    SetWindowSubclass(hw, ButtonSubclassProc, 0, 0);
    return hw;
}

void PaintODButton(DRAWITEMSTRUCT* dis)
{
    PaintODButtonImpl(dis);
}
