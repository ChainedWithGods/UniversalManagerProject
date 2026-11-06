#include "WaveText.h"
#include "theme.h"

#include <gdiplus.h>
#ifdef _MSC_VER
#pragma comment(lib, "gdiplus.lib")
#endif

#include <vector>
#include <cmath>

static ULONGLONG s_startTime = 0;

void wavetext::Init()    { s_startTime = GetTickCount64(); }
void wavetext::Shutdown(){ s_startTime = 0; }

void wavetext::CozyTitleGradient(COLORREF out5[5])
{
    out5[0] = RGB(0x00, 0x00, 0x00);
    out5[1] = RGB(0x9E, 0x5A, 0x24);
    out5[2] = RGB(0xD4, 0x98, 0x3D);
    out5[3] = RGB(0x9E, 0x5A, 0x24);
    out5[4] = RGB(0x00, 0x00, 0x00);
}

void wavetext::CozySubtitleGradient(COLORREF out5[5])
{
    out5[0] = RGB(0x00, 0x00, 0x00);
    out5[1] = RGB(0x54, 0x38, 0x20);
    out5[2] = RGB(0x8C, 0x56, 0x24);
    out5[3] = RGB(0x54, 0x38, 0x20);
    out5[4] = RGB(0x00, 0x00, 0x00);
}

static inline Gdiplus::Color GdipColor(COLORREF c)
{
    return Gdiplus::Color(255, GetRValue(c), GetGValue(c), GetBValue(c));
}

void wavetext::DrawWave(HDC hdc, const wchar_t* text, const RECT& r,
                        HFONT font, const COLORREF* gradient, int count,
                        int periodMs)
{
    if (!text || !*text || !gradient || count < 2) return;
    if (periodMs <= 0) periodMs = 6000;

    GdiplusInit();

    const float textW = (float)(r.right - r.left);
    if (textW <= 1.0f) return;

    const ULONGLONG now = GetTickCount64();
    const float tRaw   = (float)((now - s_startTime) % (ULONGLONG)periodMs)
                       / (float)periodMs;
    const float phase  = 0.5f - 0.5f * cosf(tRaw * 6.2831853f);

    std::vector<Gdiplus::Color> colors(count + 1);
    std::vector<float>          positions(count + 1);
    for (int i = 0; i < count; i++) {
        colors[i]    = GdipColor(gradient[i]);
        positions[i] = (float)i / (float)count;
    }
    colors[count]    = GdipColor(gradient[0]);
    positions[count] = 1.0f;

    const float brushW = textW * 2.0f;
    const float shift  = -textW * phase;

    Gdiplus::LinearGradientBrush brush(
        Gdiplus::PointF((Gdiplus::REAL)(r.left + shift),         (Gdiplus::REAL)r.top),
        Gdiplus::PointF((Gdiplus::REAL)(r.left + shift + brushW), (Gdiplus::REAL)r.top),
        colors[0],
        colors[count]);
    brush.SetInterpolationColors(colors.data(), positions.data(), (INT)(count + 1));

    Gdiplus::Font gdiFont(hdc, font);

    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

    const Gdiplus::RectF layout(
        (Gdiplus::REAL)r.left,
        (Gdiplus::REAL)r.top,
        (Gdiplus::REAL)textW,
        (Gdiplus::REAL)(r.bottom - r.top));
    g.DrawString(text, -1, &gdiFont, layout,
                 Gdiplus::StringFormat::GenericTypographic(), &brush);
}
