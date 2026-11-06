#include "theme.h"
#include <dwmapi.h>

#include <gdiplus.h>
#ifdef _MSC_VER
#pragma comment(lib, "gdiplus.lib")
#endif

#ifdef _MSC_VER
#pragma comment(lib, "msimg32.lib")
#endif

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif

HFONT g_fontHero  = nullptr;
HFONT g_fontTitle = nullptr;
HFONT g_fontSub   = nullptr;
HFONT g_fontBody  = nullptr;
HFONT g_fontBold  = nullptr;
HFONT g_fontSmall = nullptr;
HFONT g_fontBadge = nullptr;
HFONT g_fontStep  = nullptr;

static constexpr COLORREF kNoBrush = (COLORREF)0xFFFFFFFFUL;

HFONT CreateFontPt(int ptSize, int weight, const wchar_t* face)
{
    HDC hdc = GetDC(nullptr);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(nullptr, hdc);
    int height = -MulDiv(ptSize, dpi, 72);
    return CreateFontW(height, 0, 0, 0, weight,
                       FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, face);
}

void FillRect_(HDC hdc, const RECT& r, COLORREF c)
{
    HBRUSH b = CreateSolidBrush(c);
    FillRect(hdc, &r, b);
    DeleteObject(b);
}

void DrawRoundRect(HDC hdc, const RECT& r, int radius,
                   COLORREF fillColor, COLORREF borderColor, int borderWidth)
{
    HPEN pen = (borderWidth > 0 && borderColor != kNoBrush)
        ? CreatePen(PS_SOLID, borderWidth, borderColor)
        : (HPEN)GetStockObject(NULL_PEN);
    HBRUSH brush = (fillColor != kNoBrush)
        ? CreateSolidBrush(fillColor)
        : (HBRUSH)GetStockObject(NULL_BRUSH);

    HGDIOBJ oldPen   = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    RoundRect(hdc, r.left, r.top, r.right, r.bottom, radius * 2, radius * 2);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);

    if (borderWidth > 0 && borderColor != kNoBrush) DeleteObject(pen);
    if (fillColor != kNoBrush) DeleteObject(brush);
}

void FillGradientV(HDC hdc, const RECT& r, COLORREF top, COLORREF bottom)
{
    if (r.right <= r.left || r.bottom <= r.top) return;

    TRIVERTEX v[2] = {};
    v[0].x = r.left;
    v[0].y = r.top;
    v[0].Red   = GetRValue(top)   << 8;
    v[0].Green = GetGValue(top)   << 8;
    v[0].Blue  = GetBValue(top)   << 8;
    v[0].Alpha = 0;

    v[1].x = r.right;
    v[1].y = r.bottom;
    v[1].Red   = GetRValue(bottom) << 8;
    v[1].Green = GetGValue(bottom) << 8;
    v[1].Blue  = GetBValue(bottom) << 8;
    v[1].Alpha = 0;

    GRADIENT_RECT gr = { 0, 1 };
    GradientFill(hdc, v, 2, &gr, 1, GRADIENT_FILL_RECT_V);
}

void DrawRoundRectGradient(HDC hdc, const RECT& r, int radius,
                           COLORREF topColor, COLORREF bottomColor,
                           COLORREF borderColor, int borderWidth)
{
    HRGN clipRgn = CreateRoundRectRgn(r.left, r.top, r.right + 1, r.bottom + 1,
                                      radius * 2, radius * 2);
    const int saveDC = SaveDC(hdc);
    SelectClipRgn(hdc, clipRgn);
    FillGradientV(hdc, r, topColor, bottomColor);
    RestoreDC(hdc, saveDC);
    DeleteObject(clipRgn);

    if (borderWidth > 0 && borderColor != kNoBrush) {
        HPEN pen = CreatePen(PS_SOLID, borderWidth, borderColor);
        HGDIOBJ oldPen   = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, r.left, r.top, r.right, r.bottom, radius * 2, radius * 2);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }
}

static ULONG_PTR g_gdiplusToken = 0;

void GdiplusInit()
{
    if (g_gdiplusToken != 0) return;
    Gdiplus::GdiplusStartupInput gpsi;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gpsi, nullptr);
}

void GdiplusShutdown()
{
    if (g_gdiplusToken == 0) return;
    Gdiplus::GdiplusShutdown(g_gdiplusToken);
    g_gdiplusToken = 0;
}

static inline Gdiplus::Color GdipColor(COLORREF c)
{
    return Gdiplus::Color(255, GetRValue(c), GetGValue(c), GetBValue(c));
}

void FillCircleAA(HDC hdc, int cx, int cy, int radius,
                  COLORREF fillColor, COLORREF borderColor, int borderWidth)
{
    GdiplusInit();
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    const Gdiplus::REAL d = (Gdiplus::REAL)(radius * 2);
    const Gdiplus::REAL x = (Gdiplus::REAL)(cx - radius);
    const Gdiplus::REAL y = (Gdiplus::REAL)(cy - radius);

    Gdiplus::SolidBrush brush(GdipColor(fillColor));
    g.FillEllipse(&brush, x, y, d, d);

    if (borderWidth > 0 && borderColor != kNoBrush) {
        Gdiplus::Pen pen(GdipColor(borderColor), (Gdiplus::REAL)borderWidth);

        const Gdiplus::REAL half = (Gdiplus::REAL)borderWidth * 0.5f;
        g.DrawEllipse(&pen, x + half, y + half, d - borderWidth, d - borderWidth);
    }
}

void FillRoundRectAA(HDC hdc, const RECT& r, int radius,
                     COLORREF fillColor, COLORREF borderColor, int borderWidth)
{
    GdiplusInit();
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    Gdiplus::GraphicsPath path;
    const int    rr   = radius * 2;
    const int    w    = r.right  - r.left;
    const int    h    = r.bottom - r.top;
    const int    left = r.left;
    const int    top  = r.top;
    path.AddArc(left,                 top,                  rr, rr, 180,  90);
    path.AddArc(left + w - rr,        top,                  rr, rr, 270,  90);
    path.AddArc(left + w - rr,        top + h - rr,         rr, rr,   0,  90);
    path.AddArc(left,                 top + h - rr,         rr, rr,  90,  90);
    path.CloseFigure();

    Gdiplus::SolidBrush brush(GdipColor(fillColor));
    g.FillPath(&brush, &path);

    if (borderWidth > 0 && borderColor != kNoBrush) {
        Gdiplus::Pen pen(GdipColor(borderColor), (Gdiplus::REAL)borderWidth);
        g.DrawPath(&pen, &path);
    }
}

void DrawLineAA(HDC hdc, int x1, int y1, int x2, int y2,
                COLORREF color, int width)
{
    GdiplusInit();
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    Gdiplus::Pen pen(GdipColor(color), (Gdiplus::REAL)width);
    pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    g.DrawLine(&pen,
               (Gdiplus::REAL)x1, (Gdiplus::REAL)y1,
               (Gdiplus::REAL)x2, (Gdiplus::REAL)y2);
}

void DrawTextLeft(HDC hdc, const wchar_t* text, RECT r,
                  COLORREF color, HFONT font, DWORD flags)
{
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    HGDIOBJ old = SelectObject(hdc, font);
    DrawTextW(hdc, text, -1, &r, flags);
    SelectObject(hdc, old);
}

void EnableDarkTitle(HWND hWnd)
{

    BOOL dark = FALSE;
    DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    const COLORREF captionColor = CLR_CAPTION;
    const COLORREF textColor    = CLR_CAPTION_TXT;
    const COLORREF borderColor  = CLR_CAPTION_BD;
    DwmSetWindowAttribute(hWnd, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));
    DwmSetWindowAttribute(hWnd, DWMWA_TEXT_COLOR,    &textColor,    sizeof(textColor));
    DwmSetWindowAttribute(hWnd, DWMWA_BORDER_COLOR,  &borderColor,  sizeof(borderColor));
}

void SetRoundedCorners(HWND hWnd)
{
    DWORD corner = 2;
    DwmSetWindowAttribute(hWnd, 33, &corner, sizeof(corner));
}
