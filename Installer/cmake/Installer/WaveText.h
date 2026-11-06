#pragma once
#include <windows.h>

namespace wavetext {

    void Init();

    void Shutdown();

    void DrawWave(HDC hdc, const wchar_t* text, const RECT& r,
                  HFONT font,
                  const COLORREF* gradient, int count,
                  int periodMs = 6000);

    void CozyTitleGradient(COLORREF out5[5]);

    void CozySubtitleGradient(COLORREF out5[5]);
}
