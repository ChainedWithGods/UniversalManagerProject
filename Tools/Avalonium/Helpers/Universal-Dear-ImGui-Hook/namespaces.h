#pragma once

namespace globals {
        extern HMODULE mainModule;
        extern HWND mainWindow;
        extern int uninjectKey;
        extern int openMenuKey;

        // Rendering backend currently in use
        enum class Backend {
                None,
                DX11,
                DX12
        };
        extern Backend activeBackend;
        // Preferred backend to hook. None means auto with fallback order
        extern Backend preferredBackend;
        extern bool enableDebugLog;
        void SetDebugLogging(bool enable);
}

namespace hooks {
        extern void Init();
}

namespace inputhook {
        extern void Init(HWND hWindow);
        extern void Remove(HWND hWindow);
        static LRESULT APIENTRY WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
}

namespace mousehooks {
        void Init();
        void Remove();
}

#ifdef UNIVERSAL_MANAGER_BUILD
namespace runtimeinput {
        bool IsConsoleEnabled();
        bool ShouldReleaseMouseCapture();
        bool TryOpenConsole();
}
#endif

namespace d3d12hook {
        typedef HRESULT(STDMETHODCALLTYPE* PresentD3D12)(
                IDXGISwapChain3 * pSwapChain, UINT SyncInterval, UINT Flags);
        typedef HRESULT(STDMETHODCALLTYPE* Present1Fn)(
                IDXGISwapChain3 * pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pParams);
        extern PresentD3D12 oPresentD3D12;
        extern Present1Fn   oPresent1D3D12;

	typedef void(STDMETHODCALLTYPE* ExecuteCommandListsFn)(
		ID3D12CommandQueue * _this, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists);
        extern ExecuteCommandListsFn oExecuteCommandListsD3D12;

        typedef HRESULT(STDMETHODCALLTYPE* ResizeBuffersFn)(
                IDXGISwapChain3* pSwapChain,
                UINT BufferCount,
                UINT Width,
                UINT Height,
                DXGI_FORMAT NewFormat,
                UINT SwapChainFlags);
        typedef HRESULT(STDMETHODCALLTYPE* ResizeBuffers1Fn)(
                IDXGISwapChain3* pSwapChain,
                UINT BufferCount,
                UINT Width,
                UINT Height,
                DXGI_FORMAT NewFormat,
                UINT SwapChainFlags,
                const UINT* pCreationNodeMask,
                IUnknown* const* ppPresentQueue);
        typedef HRESULT(STDMETHODCALLTYPE* SetFullscreenStateFn)(
                IDXGISwapChain3* pSwapChain,
                BOOL Fullscreen,
                IDXGIOutput* pTarget);
        typedef HRESULT(STDMETHODCALLTYPE* ResizeTargetFn)(
                IDXGISwapChain3* pSwapChain,
                const DXGI_MODE_DESC* pNewTargetParameters);
        extern ResizeBuffersFn oResizeBuffersD3D12;
        extern ResizeBuffers1Fn oResizeBuffers1D3D12;
        extern SetFullscreenStateFn oSetFullscreenStateD3D12;
        extern ResizeTargetFn oResizeTargetD3D12;

        extern HRESULT STDMETHODCALLTYPE hookResizeBuffersD3D12(
                IDXGISwapChain3* pSwapChain,
                UINT BufferCount,
                UINT Width,
                UINT Height,
                DXGI_FORMAT NewFormat,
                UINT SwapChainFlags);
        extern HRESULT STDMETHODCALLTYPE hookResizeBuffers1D3D12(
                IDXGISwapChain3* pSwapChain,
                UINT BufferCount,
                UINT Width,
                UINT Height,
                DXGI_FORMAT NewFormat,
                UINT SwapChainFlags,
                const UINT* pCreationNodeMask,
                IUnknown* const* ppPresentQueue);
        extern HRESULT STDMETHODCALLTYPE hookSetFullscreenStateD3D12(
                IDXGISwapChain3* pSwapChain,
                BOOL Fullscreen,
                IDXGIOutput* pTarget);
        extern HRESULT STDMETHODCALLTYPE hookResizeTargetD3D12(
                IDXGISwapChain3* pSwapChain,
                const DXGI_MODE_DESC* pNewTargetParameters);

        extern long __fastcall hookPresentD3D12(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags);
        extern long __fastcall hookPresent1D3D12(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pParams);
        extern void STDMETHODCALLTYPE hookExecuteCommandListsD3D12(
                ID3D12CommandQueue* _this,
                UINT                          NumCommandLists,
                ID3D12CommandList* const* ppCommandLists);

        extern void PrepareForInit();
        extern void release();
        bool IsInitialized();
        bool IsInitializationPending();
        bool ShouldFallbackImmediately();
}

namespace hooks_dx11 {
    using PresentFn   = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
    using Present1Fn  = HRESULT(__stdcall*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
    using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    using SetFullscreenStateFn = HRESULT(__stdcall*)(IDXGISwapChain*, BOOL, IDXGIOutput*);
    using ResizeTargetFn = HRESULT(__stdcall*)(IDXGISwapChain*, const DXGI_MODE_DESC*);

    extern PresentFn       oPresentD3D11;
    extern Present1Fn      oPresent1D3D11;
    extern ResizeBuffersFn oResizeBuffersD3D11;
    extern SetFullscreenStateFn oSetFullscreenStateD3D11;
    extern ResizeTargetFn oResizeTargetD3D11;

    HRESULT __stdcall hookPresentD3D11(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
    HRESULT __stdcall hookPresent1D3D11(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pPresentParameters);
    HRESULT __stdcall hookResizeBuffersD3D11(
        IDXGISwapChain* pSwapChain,
        UINT BufferCount,
        UINT Width,
        UINT Height,
        DXGI_FORMAT NewFormat,
        UINT SwapChainFlags);
    HRESULT __stdcall hookSetFullscreenStateD3D11(
        IDXGISwapChain* pSwapChain,
        BOOL Fullscreen,
        IDXGIOutput* pTarget);
    HRESULT __stdcall hookResizeTargetD3D11(
        IDXGISwapChain* pSwapChain,
        const DXGI_MODE_DESC* pNewTargetParameters);

    void PrepareForInit();
    void Init();
    void release();
    bool IsInitialized();
    bool ShouldFallbackImmediately();
}

namespace menu {
        extern bool isOpen;
        extern void OnFrame();
        extern void Init();
        extern void RenderOverlay();   // Always called every frame, regardless of isOpen
        extern void PrepareContext();
        extern void RememberStateForTransition();
        extern void SetRestoreDelayFrames(UINT frames);
        extern void RestoreStateAfterTransition();
}




// Helper to unload the DLL and remove all hooks
void Uninject();
