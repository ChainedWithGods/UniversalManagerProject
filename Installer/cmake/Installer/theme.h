#pragma once

#include <windows.h>

inline constexpr COLORREF CLR_BG          = RGB(0xFF, 0xFA, 0xF5);
inline constexpr COLORREF CLR_STEPPER_BG  = RGB(0xFE, 0xF1, 0xE1);
inline constexpr COLORREF CLR_FOOTER      = RGB(0xF6, 0xEA, 0xD7);
inline constexpr COLORREF CLR_CARD        = RGB(0xFF, 0xFA, 0xF5);
inline constexpr COLORREF CLR_CARD_HOV    = RGB(0xFF, 0xEE, 0xDF);
inline constexpr COLORREF CLR_INSET_BG    = RGB(0xFE, 0xF1, 0xE1);
inline constexpr COLORREF CLR_INPUT_BG    = RGB(0xFF, 0xFC, 0xF7);
inline constexpr COLORREF CLR_BORDER      = RGB(0xB3, 0x94, 0x7D);
inline constexpr COLORREF CLR_BORDER_LT   = RGB(0xE8, 0xC4, 0xA0);
inline constexpr COLORREF CLR_ACCENT      = RGB(0xA6, 0x77, 0x42);
inline constexpr COLORREF CLR_ACCENT_BG   = RGB(0xB5, 0x83, 0x4A);
inline constexpr COLORREF CLR_ACCENT_HOV  = RGB(0xB5, 0x83, 0x4A);
inline constexpr COLORREF CLR_ACCENT_DOWN = RGB(0x8B, 0x5C, 0x2D);
inline constexpr COLORREF CLR_CYAN        = RGB(0x38, 0xBD, 0xF8);
inline constexpr COLORREF CLR_CYAN_GLOW   = RGB(0x22, 0xD3, 0xEE);
inline constexpr COLORREF CLR_PURPLE_DOT  = RGB(0x8B, 0x5C, 0xF6);
inline constexpr COLORREF CLR_SUCCESS     = RGB(0x61, 0x9E, 0x80);
inline constexpr COLORREF CLR_SUCCESS_HOV = RGB(0x7A, 0xB5, 0x96);
inline constexpr COLORREF CLR_SUCCESS_DOWN= RGB(0x4A, 0x7A, 0x62);
inline constexpr COLORREF CLR_TEXT        = RGB(0x00, 0x00, 0x00);
inline constexpr COLORREF CLR_TEXT_SEC    = RGB(0x2C, 0x2C, 0x2C);
inline constexpr COLORREF CLR_MUTED       = RGB(0x4E, 0x4E, 0x5A);
inline constexpr COLORREF CLR_DIM         = RGB(0x7A, 0x7A, 0x7A);
inline constexpr COLORREF CLR_STEP_INACT  = RGB(0xE8, 0xD7, 0xC0);
inline constexpr COLORREF CLR_BTN_SEC     = RGB(0xFE, 0xF1, 0xE1);
inline constexpr COLORREF CLR_BTN_SEC_HOV = RGB(0xFF, 0xEE, 0xDF);
inline constexpr COLORREF CLR_BTN_SEC_DOWN= RGB(0xF6, 0xEA, 0xD7);
inline constexpr COLORREF CLR_BTN_SEC_DIS = RGB(0xFA, 0xEE, 0xDC);

inline constexpr COLORREF CLR_CAPTION     = CLR_BG;
inline constexpr COLORREF CLR_CAPTION_TXT = CLR_TEXT;
inline constexpr COLORREF CLR_CAPTION_BD  = CLR_BORDER;

inline constexpr COLORREF CLR_FEAT_CYAN = RGB(0x38, 0xBD, 0xF8);
inline constexpr COLORREF CLR_FEAT_GOLD = RGB(0xD6, 0xA9, 0x63);
inline constexpr COLORREF CLR_FEAT_ROSE = RGB(0xD7, 0x7E, 0x98);
inline constexpr COLORREF CLR_FEAT_SAGE = RGB(0x61, 0x9E, 0x80);
inline constexpr COLORREF CLR_FEAT_RUST = RGB(0xE0, 0x9D, 0x3E);

inline constexpr COLORREF CLR_BG_GRAD_TOP    = RGB(0xFF, 0xFA, 0xF5);
inline constexpr COLORREF CLR_BG_GRAD_BOTTOM = RGB(0xFF, 0xEE, 0xDF);

inline constexpr COLORREF CLR_NO_FILL = (COLORREF)0xFFFFFFFFUL;

inline constexpr COLORREF CLR_INFO   = RGB(0x3B, 0x82, 0xF6);
inline constexpr COLORREF CLR_WARN   = RGB(0xF9, 0x73, 0x16);
inline constexpr COLORREF CLR_DANGER = RGB(0xEF, 0x44, 0x44);

inline constexpr COLORREF CLR_HIGHLIGHT = RGB(0xD6, 0xA9, 0x63);

inline constexpr COLORREF CLR_LINK      = RGB(0x4A, 0x7A, 0x62);

extern HFONT g_fontHero;
extern HFONT g_fontTitle;
extern HFONT g_fontSub;
extern HFONT g_fontBody;
extern HFONT g_fontBold;
extern HFONT g_fontSmall;
extern HFONT g_fontBadge;
extern HFONT g_fontStep;

HFONT CreateFontPt(int ptSize, int weight = FW_NORMAL, const wchar_t* face = L"Segoe UI");
void  FillRect_(HDC hdc, const RECT& r, COLORREF c);

void  DrawRoundRect(HDC hdc, const RECT& r, int radius,
                    COLORREF fillColor,
                    COLORREF borderColor = (COLORREF)0xFFFFFFFFUL,
                    int borderWidth = 1);

void  FillGradientV(HDC hdc, const RECT& r, COLORREF top, COLORREF bottom);

void  DrawRoundRectGradient(HDC hdc, const RECT& r, int radius,
                            COLORREF topColor, COLORREF bottomColor,
                            COLORREF borderColor = (COLORREF)0xFFFFFFFFUL,
                            int borderWidth = 1);

void  FillCircleAA(HDC hdc, int cx, int cy, int radius,
                   COLORREF fillColor,
                   COLORREF borderColor = (COLORREF)0xFFFFFFFFUL,
                   int borderWidth = 1);

void  FillRoundRectAA(HDC hdc, const RECT& r, int radius,
                      COLORREF fillColor,
                      COLORREF borderColor = (COLORREF)0xFFFFFFFFUL,
                      int borderWidth = 1);

void  DrawLineAA(HDC hdc, int x1, int y1, int x2, int y2,
                 COLORREF color, int width = 2);

void  GdiplusInit();
void  GdiplusShutdown();

void  DrawTextLeft(HDC hdc, const wchar_t* text, RECT r,
                   COLORREF color, HFONT font,
                   DWORD flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE);

void EnableDarkTitle(HWND hWnd);
void SetRoundedCorners(HWND hWnd);
