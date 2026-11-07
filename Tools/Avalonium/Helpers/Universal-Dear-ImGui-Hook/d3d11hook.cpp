#include "stdafx.h"

namespace hooks_dx11 {
    using Microsoft::WRL::ComPtr;

    PresentFn       oPresentD3D11 = nullptr;
    Present1Fn      oPresent1D3D11 = nullptr;
    ResizeBuffersFn oResizeBuffersD3D11 = nullptr;
    SetFullscreenStateFn oSetFullscreenStateD3D11 = nullptr;
    ResizeTargetFn oResizeTargetD3D11 = nullptr;
    static void* pPresentTarget = nullptr;
    static void* pPresent1Target = nullptr;
    static void* pResizeBuffersTarget = nullptr;
    static void* pSetFullscreenStateTarget = nullptr;
    static void* pResizeTargetTarget = nullptr;


    static ID3D11Device*            gDevice = nullptr;
    static ID3D11DeviceContext*     gContext = nullptr;
    static IDXGISwapChain*          gSwapChain = nullptr;
    static ID3D11RenderTargetView*  gRTV = nullptr;
    static HWND                     gOutputWindow = nullptr;
    static bool                     gInitialized = false;
    static bool                     gWin32Initialized = false;
    static bool                     gDx11BackendInitialized = false;
    static bool                     gTransitionPending = false;
    static bool                     gDetectedNonDx11SwapChain = false;

    static ULONGLONG                gLastRenderTargetFailureLogTick = 0;
    static ULONGLONG                gLastRenderTargetMissingLogTick = 0;
    static ULONGLONG                gLastRenderSkippedLogTick = 0;

    static bool ShouldLogRecurringIssue(ULONGLONG& lastTick)
    {
        const ULONGLONG now = GetTickCount64();
        if (now - lastTick < 1000)
            return false;
        lastTick = now;
        return true;
    }



    static void CreateRenderTarget()
    {
        ID3D11Texture2D* pBackBuffer = nullptr;
        if (gSwapChain && SUCCEEDED(gSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer))))
        {
            HRESULT hr = gDevice->CreateRenderTargetView(pBackBuffer, nullptr, &gRTV);
            pBackBuffer->Release();
            if (SUCCEEDED(hr))
            {
                DebugLog("[d3d11hook] CreateRenderTarget success RTV@%p\n", gRTV);
            }
            else
            {
                if (ShouldLogRecurringIssue(gLastRenderTargetFailureLogTick))
                    DebugLog("[d3d11hook] CreateRenderTarget failed: 0x%08X\n", hr);
            }
        }
        else
        {
            if (ShouldLogRecurringIssue(gLastRenderTargetFailureLogTick))
                DebugLog("[d3d11hook] CreateRenderTarget failed: swapchain buffer unavailable\n");
        }
    }

    static void CleanupRenderTarget()
    {
        if (gRTV)
        {
            gRTV->Release();
            gRTV = nullptr;
        }
    }

    static void ReleaseSwapChainReferences()
    {
        if (!gContext || !gRTV)
            return;

        ID3D11RenderTargetView* nullRTV = nullptr;
        gContext->OMSetRenderTargets(1, &nullRTV, nullptr);
    }

    static void ResetState(bool removeInputHook = false)


    {
        if (removeInputHook && gOutputWindow)
            inputhook::Remove(gOutputWindow);

        if (gDx11BackendInitialized)
        {
            ImGui_ImplDX11_Shutdown();
            gDx11BackendInitialized = false;
        }

        if (gWin32Initialized)
        {
            ImGui_ImplWin32_Shutdown();
            gWin32Initialized = false;
        }

        if (ImGui::GetCurrentContext())
            ImGui::DestroyContext();

        CleanupRenderTarget();

        if (gContext)
        {
            gContext->Release();
            gContext = nullptr;
        }

        if (gDevice)
        {
            gDevice->Release();
            gDevice = nullptr;
        }

        gSwapChain = nullptr;
        gOutputWindow = nullptr;
        gInitialized = false;
    }


    static bool IsUsableOutputWindow(HWND hwnd)
    {
        if (!hwnd || !IsWindow(hwnd))
            return false;

        if (GetAncestor(hwnd, GA_ROOT) != hwnd)
            return false;

        if (!IsWindowVisible(hwnd) || IsIconic(hwnd))
            return false;

        RECT clientRect{};
        if (!GetClientRect(hwnd, &clientRect))
            return false;

        return (clientRect.right - clientRect.left) >= 320 && (clientRect.bottom - clientRect.top) >= 200;
    }

    static void SuspendOverlayForTransition(IDXGISwapChain* pSwapChain, const char* reason, bool releaseRenderTarget)
    {
        const bool trackedSwapChain = pSwapChain && gSwapChain && pSwapChain == gSwapChain;
        const bool hasOverlayState = gInitialized || gContext || gDevice || gRTV || gOutputWindow;
        if (!trackedSwapChain || !hasOverlayState)
            return;

        if (menu::isOpen)
        {
            menu::RememberStateForTransition();
            menu::isOpen = false;
            DebugLog("[d3d11hook] Closing menu for %s transition\n", reason);
        }

        gTransitionPending = true;

        if (releaseRenderTarget)
        {
            ReleaseSwapChainReferences();
            if (gRTV)
            {
                DebugLog("[d3d11hook] Releasing render target for %s transition\n", reason);
                CleanupRenderTarget();
            }
        }
    }



    static void RenderFrame(IDXGISwapChain* pSwapChain)

    {
        static bool loggedPresent = false;
        if (!loggedPresent)
        {
            DebugLog("[d3d11hook] RenderFrame via Present hook %p\n", oPresentD3D11);
            loggedPresent = true;
        }

        if (!pSwapChain)
            return;

        DXGI_SWAP_CHAIN_DESC desc{};
        const bool hasDesc = SUCCEEDED(pSwapChain->GetDesc(&desc));
        const bool usableWindow = hasDesc && IsUsableOutputWindow(desc.OutputWindow);

        if (gInitialized)
        {
            const bool swapChainChanged = pSwapChain != gSwapChain;
            const bool windowChanged = usableWindow && gOutputWindow && desc.OutputWindow != gOutputWindow;
            if (swapChainChanged || windowChanged)
            {
                DebugLog("[d3d11hook] Rebinding overlay for swapchain %p window %p (previous swapchain %p window %p)\n",
                    pSwapChain, hasDesc ? desc.OutputWindow : nullptr, gSwapChain, gOutputWindow);
                ResetState(windowChanged || swapChainChanged);
            }
            else if (!gRTV)
            {
                if (gTransitionPending)
                {
                    if (!usableWindow)
                        return;

                    gSwapChain = pSwapChain;
                    gOutputWindow = desc.OutputWindow;
                    CreateRenderTarget();
                    if (gRTV)
                    {
                        gTransitionPending = false;
                        DebugLog("[d3d11hook] Render target restored after transition for window %p.\n", gOutputWindow);
                    }
                    else
                    {
                        return;
                    }
                }
                else
                {
                    if (ShouldLogRecurringIssue(gLastRenderTargetMissingLogTick))
                        DebugLog("[d3d11hook] Render target missing unexpectedly; attempting recreation.\n");
                    gSwapChain = pSwapChain;
                    if (hasDesc)
                        gOutputWindow = desc.OutputWindow;
                    CreateRenderTarget();
                    if (!gRTV)
                        return;
                }
            }
        }



        if (!gInitialized)
        {
            if (!gDevice)
            {
                HRESULT hr = pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&gDevice);
                if (FAILED(hr))
                {
                    IUnknown* testDev = nullptr;
                    if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&testDev)))
                    {
                        testDev->Release();
                        DebugLog("[d3d11hook] Swapchain is using ID3D12Device (not D3D11); signaling immediate fallback.\n");
                    }
                    else
                    {
                        DebugLog("[d3d11hook] Swapchain %p does not support ID3D11Device (hr=0x%08X); signaling immediate fallback.\n", pSwapChain, hr);
                    }
                    gDetectedNonDx11SwapChain = true;
                    return;
                }
            }

            if (!usableWindow)
            {
                static ULONGLONG lastLogTime = 0;
                if (GetTickCount64() - lastLogTime > 1000) {
                    DebugLog("[d3d11hook] RenderFrame: usableWindow is false (hasDesc=%d)\n", hasDesc);
                    lastLogTime = GetTickCount64();
                }
                return;
            }

            static ULONGLONG s_LastReinitLog = 0;
            static IDXGISwapChain* s_LastLoggedSwapChain = nullptr;
            if (s_LastLoggedSwapChain != pSwapChain || GetTickCount64() - s_LastReinitLog > 1000) {
                DebugLog("[d3d11hook] Attempting initialization on swapchain %p\n", pSwapChain);
                s_LastReinitLog = GetTickCount64();
                s_LastLoggedSwapChain = pSwapChain;
            }

            gSwapChain = pSwapChain;
            gOutputWindow = desc.OutputWindow;
            gDevice->GetImmediateContext(&gContext);

            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            menu::PrepareContext();
            ImGui::StyleColorsDark();

            const bool win32Ready = ImGui_ImplWin32_Init(gOutputWindow);
            gWin32Initialized = win32Ready;
            const bool dx11Ready = win32Ready && ImGui_ImplDX11_Init(gDevice, gContext);
            gDx11BackendInitialized = dx11Ready;
            if (!dx11Ready)
            {
                ErrorLog("[d3d11hook] ImGui backend initialization failed (win32Ready=%d).\n", win32Ready);
                ResetState(false);
                return;
            }

            inputhook::Init(gOutputWindow);
            CreateRenderTarget();
            if (!gRTV)
            {
                ErrorLog("[d3d11hook] Render target creation failed; waiting for another swapchain.\n");
                ResetState(true);
                return;
            }

            gInitialized = true;
            gTransitionPending = false;
            DebugLog("[d3d11hook] ImGui initialized for window %p.\n", gOutputWindow);
        }

        if (IsMenuToggleRequested())
        {
            menu::isOpen = !menu::isOpen;
            DebugLog("[d3d11hook] Toggle menu: %d\n", menu::isOpen);
        }

        if (WasUninjectHotkeyPressed())
        {
            DebugLog("[d3d11hook] Detach hotkey detected.\n");
            Uninject();
            return;
        }

        if (gInitialized)
        {
            menu::RestoreStateAfterTransition();
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            menu::OnFrame();
            menu::RenderOverlay();
            if (menu::isOpen)
                menu::Init();

            ImGui::EndFrame();
            ImGui::Render();
            if (gContext && gRTV)
            {
                ID3D11RenderTargetView* previousRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
                ID3D11DepthStencilView* previousDSV = nullptr;
                gContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, previousRTVs, &previousDSV);
                gContext->OMSetRenderTargets(1, &gRTV, nullptr);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                gContext->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, previousRTVs, previousDSV);
                for (ID3D11RenderTargetView* rtv : previousRTVs)
                {
                    if (rtv)
                        rtv->Release();
                }
                if (previousDSV)
                    previousDSV->Release();
            }
            else
            {
                if (ShouldLogRecurringIssue(gLastRenderSkippedLogTick))
                    DebugLog("[d3d11hook] Render skipped: device context or render target invalid\n");
            }
        }

    }

    HRESULT __stdcall hookPresentD3D11(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
    {
        RenderFrame(pSwapChain);
        return oPresentD3D11(pSwapChain, SyncInterval, Flags);
    }

    HRESULT __stdcall hookPresent1D3D11(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pPresentParameters)
    {
        RenderFrame(pSwapChain);
        return oPresent1D3D11(pSwapChain, SyncInterval, Flags, pPresentParameters);
    }

    HRESULT __stdcall hookResizeBuffersD3D11(
        IDXGISwapChain* pSwapChain,
        UINT BufferCount,
        UINT Width,
        UINT Height,
        DXGI_FORMAT NewFormat,
        UINT SwapChainFlags)
    {
        const bool trackedSwapChain = pSwapChain && gSwapChain && pSwapChain == gSwapChain;
        if (trackedSwapChain)
        {
            DebugLog("[d3d11hook] ResizeBuffers called: %ux%u Buffers=%u Format=%u Flags=0x%X\n",
                Width, Height, BufferCount, static_cast<unsigned int>(NewFormat), SwapChainFlags);
            SuspendOverlayForTransition(pSwapChain, "ResizeBuffers", true);

        }

        HRESULT hr = oResizeBuffersD3D11(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

        if (SUCCEEDED(hr) && trackedSwapChain)
        {
            DebugLog("[d3d11hook] ResizeBuffers succeeded; overlay will reinitialize on next present\n");
        }
        else if (FAILED(hr) && trackedSwapChain)
        {
            DebugLog("[d3d11hook] ResizeBuffers failed: 0x%08X\n", hr);
        }

        return hr;
    }

    HRESULT __stdcall hookSetFullscreenStateD3D11(
        IDXGISwapChain* pSwapChain,
        BOOL Fullscreen,
        IDXGIOutput* pTarget)
    {
        const bool trackedSwapChain = pSwapChain && gSwapChain && pSwapChain == gSwapChain;
        if (trackedSwapChain)
        {
            DebugLog("[d3d11hook] SetFullscreenState called: Fullscreen=%d Target=%p\n", Fullscreen, pTarget);
            SuspendOverlayForTransition(pSwapChain, "SetFullscreenState", false);

        }

        return oSetFullscreenStateD3D11(pSwapChain, Fullscreen, pTarget);
    }

    HRESULT __stdcall hookResizeTargetD3D11(
        IDXGISwapChain* pSwapChain,
        const DXGI_MODE_DESC* pNewTargetParameters)
    {
        const bool trackedSwapChain = pSwapChain && gSwapChain && pSwapChain == gSwapChain;
        if (trackedSwapChain)
        {
            if (pNewTargetParameters)
            {
                DebugLog("[d3d11hook] ResizeTarget called: %ux%u Format=%u Refresh=%u/%u\n",
                    pNewTargetParameters->Width,
                    pNewTargetParameters->Height,
                    static_cast<unsigned int>(pNewTargetParameters->Format),
                    pNewTargetParameters->RefreshRate.Numerator,
                    pNewTargetParameters->RefreshRate.Denominator);
            }
            else
            {
                DebugLog("[d3d11hook] ResizeTarget called with null mode desc\n");
            }
            SuspendOverlayForTransition(pSwapChain, "ResizeTarget", false);

        }

        return oResizeTargetD3D11(pSwapChain, pNewTargetParameters);
    }


    void Init()
    {
        DebugLog("[d3d11hook] Init starting\n");

        WNDCLASSEXW wc = {
            sizeof(WNDCLASSEXW), CS_CLASSDC, DefWindowProcW,
            0L, 0L, GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr,
            L"DummyDX11", nullptr
        };
        RegisterClassExW(&wc);
        HWND hwnd = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
            0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        IDXGISwapChain* swapChain = nullptr;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &featureLevel, 1,
            D3D11_SDK_VERSION, &sd, &swapChain, &device, nullptr, &context);

        if (SUCCEEDED(hr))
        {
            void** vtbl = *reinterpret_cast<void***>(swapChain);
            pPresentTarget = vtbl[8];
            pSetFullscreenStateTarget = vtbl[10];
            pResizeBuffersTarget = vtbl[13];
            pResizeTargetTarget = vtbl[14];
            MH_CreateHook(pPresentTarget, hookPresentD3D11, reinterpret_cast<void**>(&oPresentD3D11));
            MH_CreateHook(pSetFullscreenStateTarget, hookSetFullscreenStateD3D11, reinterpret_cast<void**>(&oSetFullscreenStateD3D11));
            MH_CreateHook(pResizeBuffersTarget, hookResizeBuffersD3D11, reinterpret_cast<void**>(&oResizeBuffersD3D11));
            MH_CreateHook(pResizeTargetTarget, hookResizeTargetD3D11, reinterpret_cast<void**>(&oResizeTargetD3D11));

            IDXGISwapChain1* swapChain1 = nullptr;
            void* present1Addr = nullptr;
            if (SUCCEEDED(swapChain->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&swapChain1)))
            {
                void** vtbl1 = *reinterpret_cast<void***>(swapChain1);
                present1Addr = vtbl1[22];
                pPresent1Target = vtbl1[22];
                MH_CreateHook(pPresent1Target, hookPresent1D3D11, reinterpret_cast<void**>(&oPresent1D3D11));
                MH_EnableHook(pPresent1Target);
                swapChain1->Release();
            }

            MH_EnableHook(pPresentTarget);
            MH_EnableHook(pSetFullscreenStateTarget);
            MH_EnableHook(pResizeBuffersTarget);
            MH_EnableHook(pResizeTargetTarget);
            DebugLog("[d3d11hook] Hooks placed Present@%p Present1@%p SetFullscreenState@%p ResizeBuffers@%p ResizeTarget@%p\n", pPresentTarget, present1Addr, pSetFullscreenStateTarget, pResizeBuffersTarget, pResizeTargetTarget);

            swapChain->Release();
            device->Release();
            context->Release();
        }
        else
        {
            DebugLog("[d3d11hook] D3D11CreateDeviceAndSwapChain failed: 0x%08X\n", hr);
        }

        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
    }

    void release()
    {
        DebugLog("[d3d11hook] Releasing resources\n");

        gTransitionPending = false;
        ResetState(true);



        if (pPresentTarget) {
            MH_DisableHook(pPresentTarget);
            MH_RemoveHook(pPresentTarget);
            pPresentTarget = nullptr;
        }
        if (pPresent1Target) {
            MH_DisableHook(pPresent1Target);
            MH_RemoveHook(pPresent1Target);
            pPresent1Target = nullptr;
        }
        if (pSetFullscreenStateTarget) {
            MH_DisableHook(pSetFullscreenStateTarget);
            MH_RemoveHook(pSetFullscreenStateTarget);
            pSetFullscreenStateTarget = nullptr;
        }
        if (pResizeBuffersTarget) {
            MH_DisableHook(pResizeBuffersTarget);
            MH_RemoveHook(pResizeBuffersTarget);
            pResizeBuffersTarget = nullptr;
        }
        if (pResizeTargetTarget) {
            MH_DisableHook(pResizeTargetTarget);
            MH_RemoveHook(pResizeTargetTarget);
            pResizeTargetTarget = nullptr;
        }
    }


    void PrepareForInit()
    {
        gDetectedNonDx11SwapChain = false;
        ResetState(true);
    }

    bool ShouldFallbackImmediately()
    {
        return gDetectedNonDx11SwapChain;
    }

    bool IsInitialized()
    {
        return gInitialized;
    }
}
