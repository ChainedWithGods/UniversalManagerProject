#include <windows.h>
#include <stdio.h>

#if defined(PROXY_DWMAPI)
extern "C" {
__declspec(dllexport) HRESULT WINAPI DwmDefWindowProc(HWND, UINT, WPARAM, LPARAM, LRESULT*);
__declspec(dllexport) HRESULT WINAPI DwmEnableBlurBehindWindow(HWND, const void*);
__declspec(dllexport) HRESULT WINAPI DwmEnableComposition(UINT);
__declspec(dllexport) HRESULT WINAPI DwmExtendFrameIntoClientArea(HWND, const void*);
__declspec(dllexport) HRESULT WINAPI DwmGetColorizationColor(DWORD*, BOOL*);
__declspec(dllexport) HRESULT WINAPI DwmGetCompositionTimingInfo(HWND, void*);
__declspec(dllexport) HRESULT WINAPI DwmGetWindowAttribute(HWND, DWORD, void*, DWORD);
__declspec(dllexport) HRESULT WINAPI DwmInvalidateIconicBitmaps(HWND);
__declspec(dllexport) HRESULT WINAPI DwmIsCompositionEnabled(BOOL*);
__declspec(dllexport) HRESULT WINAPI DwmModifyPreviousDxFrameDuration(HWND, INT);
__declspec(dllexport) HRESULT WINAPI DwmQueryThumbnailSourceSize(HANDLE, void*);
__declspec(dllexport) HRESULT WINAPI DwmRegisterThumbnail(HWND, HWND, HANDLE*);
__declspec(dllexport) HRESULT WINAPI DwmSetDxFrameDuration(HWND, INT);
__declspec(dllexport) HRESULT WINAPI DwmSetIconicLivePreviewBitmap(HWND, HBITMAP, const POINT*, DWORD);
__declspec(dllexport) HRESULT WINAPI DwmSetIconicThumbnail(HWND, HBITMAP, DWORD);
__declspec(dllexport) HRESULT WINAPI DwmSetWindowAttribute(HWND, DWORD, const void*, DWORD);
__declspec(dllexport) HRESULT WINAPI DwmUnregisterThumbnail(HANDLE);
__declspec(dllexport) HRESULT WINAPI DwmUpdateThumbnailProperties(HANDLE, const void*);
}

#elif defined(PROXY_DINPUT8)
  #pragma comment(linker, "/EXPORT:DirectInput8Create=dinput8.DirectInput8Create")
  #pragma comment(linker, "/EXPORT:DllCanUnloadNow=dinput8.DllCanUnloadNow")
  #pragma comment(linker, "/EXPORT:DllGetClassObject=dinput8.DllGetClassObject")
  #pragma comment(linker, "/EXPORT:DllRegisterServer=dinput8.DllRegisterServer")
  #pragma comment(linker, "/EXPORT:DllUnregisterServer=dinput8.DllUnregisterServer")
  #pragma comment(linker, "/EXPORT:GetdfDIJoystick=dinput8.GetdfDIJoystick")

#elif defined(PROXY_DXGI)
  #pragma comment(linker, "/EXPORT:CreateDXGIFactory=dxgi.CreateDXGIFactory")
  #pragma comment(linker, "/EXPORT:CreateDXGIFactory1=dxgi.CreateDXGIFactory1")
  #pragma comment(linker, "/EXPORT:CreateDXGIFactory2=dxgi.CreateDXGIFactory2")
  #pragma comment(linker, "/EXPORT:DXGIDeclareAdapterRemovalSupport=dxgi.DXGIDeclareAdapterRemovalSupport")
  #pragma comment(linker, "/EXPORT:DXGIDisableVBlankVirtualization=dxgi.DXGIDisableVBlankVirtualization")
  #pragma comment(linker, "/EXPORT:DXGIGetDebugInterface1=dxgi.DXGIGetDebugInterface1")

#elif defined(PROXY_D3D12)
  #pragma comment(linker, "/EXPORT:D3D12CreateDevice=d3d12.D3D12CreateDevice")
  #pragma comment(linker, "/EXPORT:D3D12CreateRootSignatureDeserializer=d3d12.D3D12CreateRootSignatureDeserializer")
  #pragma comment(linker, "/EXPORT:D3D12CreateVersionedRootSignatureDeserializer=d3d12.D3D12CreateVersionedRootSignatureDeserializer")
  #pragma comment(linker, "/EXPORT:D3D12EnableExperimentalFeatures=d3d12.D3D12EnableExperimentalFeatures")
  #pragma comment(linker, "/EXPORT:D3D12GetDebugInterface=d3d12.D3D12GetDebugInterface")
  #pragma comment(linker, "/EXPORT:D3D12GetInterface=d3d12.D3D12GetInterface")
  #pragma comment(linker, "/EXPORT:D3D12PIXGPUCaptureNextFrames=d3d12.D3D12PIXGPUCaptureNextFrames")
  #pragma comment(linker, "/EXPORT:D3D12PIXGetCaptureState=d3d12.D3D12PIXGetCaptureState")
  #pragma comment(linker, "/EXPORT:D3D12PIXSetMarker=d3d12.D3D12PIXSetMarker")
  #pragma comment(linker, "/EXPORT:D3D12SetApplicationInfo=d3d12.D3D12SetApplicationInfo")
  #pragma comment(linker, "/EXPORT:D3D12SetTracingLevel=d3d12.D3D12SetTracingLevel")
  #pragma comment(linker, "/EXPORT:GetBehaviorValue=d3d12.GetBehaviorValue")

#elif defined(PROXY_D3D11)
  #pragma comment(linker, "/EXPORT:D3D11CreateDevice=d3d11.D3D11CreateDevice")
  #pragma comment(linker, "/EXPORT:D3D11CreateDeviceAndSwapChain=d3d11.D3D11CreateDeviceAndSwapChain")
  #pragma comment(linker, "/EXPORT:D3D11On12CreateDevice=d3d11.D3D11On12CreateDevice")
  #pragma comment(linker, "/EXPORT:D3D11CreateDeviceForD3D12=d3d11.D3D11CreateDeviceForD3D12")
  #pragma comment(linker, "/EXPORT:D3DKMTCreateAdapter=d3d11.D3DKMTCreateAdapter")
  #pragma comment(linker, "/EXPORT:D3DPerformance_BeginEvent=d3d11.D3DPerformance_BeginEvent")
  #pragma comment(linker, "/EXPORT:D3DPerformance_EndEvent=d3d11.D3DPerformance_EndEvent")
  #pragma comment(linker, "/EXPORT:D3DPerformance_GetStatus=d3d11.D3DPerformance_GetStatus")
  #pragma comment(linker, "/EXPORT:D3DPerformance_SetMarker=d3d11.D3DPerformance_SetMarker")

#elif defined(PROXY_D3D9)
  #pragma comment(linker, "/EXPORT:D3D9GetSWInfo=d3d9.D3D9GetSWInfo")
  #pragma comment(linker, "/EXPORT:DebugSetMute=d3d9.DebugSetMute")
  #pragma comment(linker, "/EXPORT:Direct3D9EnableMaximizedWindowedModeShim=d3d9.Direct3D9EnableMaximizedWindowedModeShim")
  #pragma comment(linker, "/EXPORT:Direct3DCreate9=d3d9.Direct3DCreate9")
  #pragma comment(linker, "/EXPORT:Direct3DCreate9Ex=d3d9.Direct3DCreate9Ex")
  #pragma comment(linker, "/EXPORT:Direct3DCreate9On12=d3d9.Direct3DCreate9On12")
  #pragma comment(linker, "/EXPORT:Direct3DCreate9On12Ex=d3d9.Direct3DCreate9On12Ex")
  #pragma comment(linker, "/EXPORT:Direct3DShaderValidator9=d3d9.Direct3DShaderValidator9")
  #pragma comment(linker, "/EXPORT:PSGPError=d3d9.PSGPError")
  #pragma comment(linker, "/EXPORT:PSGPSampleTexture=d3d9.PSGPSampleTexture")

#else
  #error "Must define exactly one PROXY_<DLL> preprocessor macro (PROXY_DWMAPI, PROXY_D3D12, etc.)"
#endif

static HMODULE g_manager_dll = nullptr;

#if defined(PROXY_DWMAPI)
static HMODULE g_real_dwmapi = nullptr;

static FARPROC GetRealDwmapiProc(const char* name) {
  if (g_real_dwmapi == nullptr) {
    wchar_t sysdir[MAX_PATH] = {0};
    const UINT n = GetSystemDirectoryW(sysdir, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
      return nullptr;
    }
    wchar_t path[MAX_PATH] = {0};
    wcscpy_s(path, sysdir);
    wcscat_s(path, L"\\dwmapi.dll");
    g_real_dwmapi = LoadLibraryW(path);
    if (g_real_dwmapi == nullptr) {
      return nullptr;
    }
  }
  return GetProcAddress(g_real_dwmapi, name);
}

template <typename Fn>
static Fn GetRealDwmapi(const char* name) {
  return reinterpret_cast<Fn>(GetRealDwmapiProc(name));
}

extern "C" HRESULT WINAPI DwmDefWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* plresult) {
  using Fn = HRESULT (WINAPI*)(HWND, UINT, WPARAM, LPARAM, LRESULT*);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmDefWindowProc");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(hwnd, msg, wparam, lparam, plresult);
}

extern "C" HRESULT WINAPI DwmEnableBlurBehindWindow(HWND hwnd, const void* pBlurBehind) {
  using Fn = HRESULT (WINAPI*)(HWND, const void*);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmEnableBlurBehindWindow");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(hwnd, pBlurBehind);
}

extern "C" HRESULT WINAPI DwmEnableComposition(UINT uCompositionAction) {
  using Fn = HRESULT (WINAPI*)(UINT);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmEnableComposition");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(uCompositionAction);
}

extern "C" HRESULT WINAPI DwmExtendFrameIntoClientArea(HWND hwnd, const void* pMarInset) {
  using Fn = HRESULT (WINAPI*)(HWND, const void*);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmExtendFrameIntoClientArea");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(hwnd, pMarInset);
}

extern "C" HRESULT WINAPI DwmGetColorizationColor(DWORD* pcrColorization, BOOL* pfOpaqueBlend) {
  using Fn = HRESULT (WINAPI*)(DWORD*, BOOL*);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmGetColorizationColor");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(pcrColorization, pfOpaqueBlend);
}

extern "C" HRESULT WINAPI DwmGetCompositionTimingInfo(HWND hwnd, void* pTimingInfo) {
  using Fn = HRESULT (WINAPI*)(HWND, void*);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmGetCompositionTimingInfo");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(hwnd, pTimingInfo);
}

extern "C" HRESULT WINAPI DwmGetWindowAttribute(HWND hwnd, DWORD dwAttribute, void* pvAttribute, DWORD cbAttribute) {
  using Fn = HRESULT (WINAPI*)(HWND, DWORD, void*, DWORD);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmGetWindowAttribute");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(hwnd, dwAttribute, pvAttribute, cbAttribute);
}

extern "C" HRESULT WINAPI DwmInvalidateIconicBitmaps(HWND hwnd) {
  using Fn = HRESULT (WINAPI*)(HWND);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmInvalidateIconicBitmaps");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(hwnd);
}

extern "C" HRESULT WINAPI DwmIsCompositionEnabled(BOOL* pfEnabled) {
  using Fn = HRESULT (WINAPI*)(BOOL*);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmIsCompositionEnabled");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(pfEnabled);
}

extern "C" HRESULT WINAPI DwmModifyPreviousDxFrameDuration(HWND hwnd, INT cRefreshes) {
  using Fn = HRESULT (WINAPI*)(HWND, INT);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmModifyPreviousDxFrameDuration");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(hwnd, cRefreshes);
}

extern "C" HRESULT WINAPI DwmQueryThumbnailSourceSize(HANDLE hThumbnail, void* pSize) {
  using Fn = HRESULT (WINAPI*)(HANDLE, void*);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmQueryThumbnailSourceSize");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(hThumbnail, pSize);
}

extern "C" HRESULT WINAPI DwmRegisterThumbnail(HWND hwndDestination, HWND hwndSource, HANDLE* phThumbnailId) {
  using Fn = HRESULT (WINAPI*)(HWND, HWND, HANDLE*);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmRegisterThumbnail");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(hwndDestination, hwndSource, phThumbnailId);
}

extern "C" HRESULT WINAPI DwmSetDxFrameDuration(HWND hwnd, INT cRefreshes) {
  using Fn = HRESULT (WINAPI*)(HWND, INT);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmSetDxFrameDuration");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(hwnd, cRefreshes);
}

extern "C" HRESULT WINAPI DwmSetIconicLivePreviewBitmap(HWND hwnd, HBITMAP hbmp, const POINT* pptClient, DWORD dwSITFlags) {
  using Fn = HRESULT (WINAPI*)(HWND, HBITMAP, const POINT*, DWORD);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmSetIconicLivePreviewBitmap");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(hwnd, hbmp, pptClient, dwSITFlags);
}

extern "C" HRESULT WINAPI DwmSetIconicThumbnail(HWND hwnd, HBITMAP hbmp, DWORD dwSITFlags) {
  using Fn = HRESULT (WINAPI*)(HWND, HBITMAP, DWORD);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmSetIconicThumbnail");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(hwnd, hbmp, dwSITFlags);
}

extern "C" HRESULT WINAPI DwmSetWindowAttribute(HWND hwnd, DWORD dwAttribute, const void* pvAttribute, DWORD cbAttribute) {
  using Fn = HRESULT (WINAPI*)(HWND, DWORD, const void*, DWORD);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmSetWindowAttribute");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(hwnd, dwAttribute, pvAttribute, cbAttribute);
}

extern "C" HRESULT WINAPI DwmUnregisterThumbnail(HANDLE hThumbnailId) {
  using Fn = HRESULT (WINAPI*)(HANDLE);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmUnregisterThumbnail");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(hThumbnailId);
}

extern "C" HRESULT WINAPI DwmUpdateThumbnailProperties(HANDLE hThumbnailId, const void* ptnProperties) {
  using Fn = HRESULT (WINAPI*)(HANDLE, const void*);
  static Fn fn = nullptr;
  if (!fn) fn = GetRealDwmapi<Fn>("DwmUpdateThumbnailProperties");
  if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return fn(hThumbnailId, ptnProperties);
}
#endif

DWORD WINAPI ManagerLoaderThread(LPVOID ) {

  wchar_t self_dir[MAX_PATH] = {0};
  if (!GetModuleFileNameW(nullptr, self_dir, MAX_PATH)) {
    return 1;
  }

  wchar_t* last_slash = wcsrchr(self_dir, L'\\');
  if (last_slash) {
    *(last_slash + 1) = L'\0';
  }

  const wchar_t* names[] = { L"Avalonium.dll", L"Avalonium-Proxied.dll" };
  for (const wchar_t* name : names) {
    wchar_t full_path[MAX_PATH] = {0};
    wcscpy_s(full_path, self_dir);
    wcscat_s(full_path, name);

    g_manager_dll = LoadLibraryExW(full_path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (g_manager_dll != nullptr) {
      break;
    }
  }

  return 0;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(module);
    HANDLE thread = CreateThread(nullptr, 0, ManagerLoaderThread, nullptr, 0, nullptr);
    if (thread != nullptr) {
      CloseHandle(thread);
    }
  }
  (void) reserved;
  return TRUE;
}
