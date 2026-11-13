#include "stdafx.h"
#include "d3d12hook_api.h"

#include <atomic>
#include <mutex>
#include <vector>

namespace hooks { void Remove(); }

namespace d3d12hook {
    PresentD3D12            oPresentD3D12 = nullptr;
    Present1Fn              oPresent1D3D12 = nullptr;
    ExecuteCommandListsFn   oExecuteCommandListsD3D12 = nullptr;
    ResizeBuffersFn         oResizeBuffersD3D12 = nullptr;
    ResizeBuffers1Fn        oResizeBuffers1D3D12 = nullptr;
    SetFullscreenStateFn    oSetFullscreenStateD3D12 = nullptr;
    ResizeTargetFn          oResizeTargetD3D12 = nullptr;


    static ID3D12Device* gDevice = nullptr;
    static ID3D12CommandQueue* gCommandQueue = nullptr;
    static ID3D12CommandQueue* gCandidateCommandQueue = nullptr;
    static IDXGISwapChain3* gSwapChain = nullptr;

    static ID3D12DescriptorHeap* gHeapRTV = nullptr;
    static ID3D12DescriptorHeap* gHeapSRV = nullptr;
    static ID3D12GraphicsCommandList* gCommandList = nullptr;
    static ID3D12Fence* gOverlayFence = nullptr;
    static HANDLE                   gFenceEvent = nullptr;
    static UINT64                  gOverlayFenceValue = 0;
    static uintx_t                 gBufferCount = 0;
    static HWND                    gOutputWindow = nullptr;

    struct FrameContext {
        ID3D12CommandAllocator* allocator;
        ID3D12Resource* renderTarget;
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
        UINT64 FenceValue;
    };
    static FrameContext* gFrameContexts = nullptr;
    static bool                   gInitialized = false;
    static bool                   gShutdown = false;
    static bool                   gAfterFirstPresent = false;
    static bool                   gWin32Initialized = false;
    static bool                   gDx12BackendInitialized = false;
    static bool                   gSawCompatibleSwapChain = false;
    static UINT                   gMissingCommandQueueFrames = 0;
    static UINT                   gFramesToSkipAfterReinit = 0;
    static ULONGLONG              gLastRejectedWindowLogTick = 0;
    static ULONGLONG              gLastDx12ActivityTick = 0;

    // Some engines alternate between several direct queues every frame; only
    // report switches to queues we have not seen before.
    static constexpr UINT         kRecentDirectQueueCapacity = 8;
    static ID3D12CommandQueue*    gRecentDirectQueues[kRecentDirectQueueCapacity] = {};
    static UINT                   gRecentDirectQueueCount = 0;

    void release();

    // Utility to log HRESULTs
    inline void LogHRESULT(const char* label, HRESULT hr) {
        if (FAILED(hr)) {
            ErrorLog("[d3d12hook] ERROR: %s: hr=0x%08X\n", label, hr);
        } else {
            DebugLog("[d3d12hook] %s: hr=0x%08X\n", label, hr);
        }
    }

    static void NoteDx12Activity()
    {
        gLastDx12ActivityTick = GetTickCount64();
    }

    // ---------------------------------------------------------------------
    // Overlay SRV heap
    //
    // The ImGui DX12 backend binds a GPU descriptor handle per draw, so every
    // texture drawn through it needs a descriptor in the heap that Present
    // binds. The heap is shared by the font atlas and any mod texture view.
    // ---------------------------------------------------------------------
    static constexpr UINT kSrvHeapCapacity = 4096;
    static std::mutex gSrvMutex;
    static UINT gSrvNextSlot = 0;
    static std::vector<UINT> gSrvFreeSlots;
    static UINT gSrvSlotSize = 0;
    static uint32_t gTextureEpoch = 1;

    static void ResetSrvSlots()
    {
        std::lock_guard<std::mutex> lock(gSrvMutex);
        gSrvNextSlot = 0;
        gSrvFreeSlots.clear();
    }

    static bool AllocateSrvSlot(D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
                                D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
    {
        if (!gDevice || !gHeapSRV || gSrvSlotSize == 0 || !cpu || !gpu) return false;

        std::lock_guard<std::mutex> lock(gSrvMutex);
        UINT slot = 0;
        if (!gSrvFreeSlots.empty())
        {
            slot = gSrvFreeSlots.back();
            gSrvFreeSlots.pop_back();
        }
        else if (gSrvNextSlot < kSrvHeapCapacity)
        {
            slot = gSrvNextSlot++;
        }
        else
        {
            return false;
        }

        *cpu = gHeapSRV->GetCPUDescriptorHandleForHeapStart();
        cpu->ptr += static_cast<SIZE_T>(slot) * gSrvSlotSize;
        *gpu = gHeapSRV->GetGPUDescriptorHandleForHeapStart();
        gpu->ptr += static_cast<UINT64>(slot) * gSrvSlotSize;
        return true;
    }

    static void FreeSrvSlot(D3D12_CPU_DESCRIPTOR_HANDLE cpu)
    {
        if (!gHeapSRV || gSrvSlotSize == 0) return;
        const SIZE_T base = gHeapSRV->GetCPUDescriptorHandleForHeapStart().ptr;
        if (cpu.ptr < base) return;
        const UINT slot = static_cast<UINT>((cpu.ptr - base) / gSrvSlotSize);
        if (slot >= kSrvHeapCapacity) return;

        std::lock_guard<std::mutex> lock(gSrvMutex);
        gSrvFreeSlots.push_back(slot);
    }

    static void ImGuiSrvAlloc(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
                              D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
    {
        if (!AllocateSrvSlot(cpu, gpu))
        {
            cpu->ptr = 0;
            gpu->ptr = 0;
        }
    }

    static void ImGuiSrvFree(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                             D3D12_GPU_DESCRIPTOR_HANDLE)
    {
        FreeSrvSlot(cpu);
    }

    static DXGI_FORMAT ViewableFormat(DXGI_FORMAT format)
    {
        switch (format)
        {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case DXGI_FORMAT_BC1_TYPELESS: return DXGI_FORMAT_BC1_UNORM_SRGB;
        case DXGI_FORMAT_BC2_TYPELESS: return DXGI_FORMAT_BC2_UNORM_SRGB;
        case DXGI_FORMAT_BC3_TYPELESS: return DXGI_FORMAT_BC3_UNORM_SRGB;
        case DXGI_FORMAT_BC7_TYPELESS: return DXGI_FORMAT_BC7_UNORM_SRGB;
        default: return format;
        }
    }

    static uint64_t CreateTextureViewFromResource(void* resourcePointer)
    {
        if (!gDx12BackendInitialized || !gDevice || !gHeapSRV || !resourcePointer) return 0;

        auto* resource = static_cast<ID3D12Resource*>(resourcePointer);
        D3D12_RESOURCE_DESC desc{};
        __try
        {
            desc = resource->GetDesc();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return 0;
        }

        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
            desc.SampleDesc.Count != 1 || desc.MipLevels == 0)
        {
            return 0;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
        if (!AllocateSrvSlot(&cpu, &gpu)) return 0;

        D3D12_SHADER_RESOURCE_VIEW_DESC view{};
        view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        view.Format = ViewableFormat(desc.Format);
        if (desc.DepthOrArraySize > 1)
        {
            view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            view.Texture2DArray.MostDetailedMip = 0;
            view.Texture2DArray.MipLevels = desc.MipLevels;
            view.Texture2DArray.FirstArraySlice = 0;
            view.Texture2DArray.ArraySize = desc.DepthOrArraySize;
            view.Texture2DArray.PlaneSlice = 0;
            view.Texture2DArray.ResourceMinLODClamp = 0.0f;
        }
        else
        {
            view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            view.Texture2D.MostDetailedMip = 0;
            view.Texture2D.MipLevels = desc.MipLevels;
            view.Texture2D.PlaneSlice = 0;
            view.Texture2D.ResourceMinLODClamp = 0.0f;
        }

        __try
        {
            gDevice->CreateShaderResourceView(resource, &view, cpu);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            FreeSrvSlot(cpu);
            return 0;
        }

        return gpu.ptr;
    }

    bool TextureBridgeReady()
    {
        return gDx12BackendInitialized && gDevice && gHeapSRV;
    }

    uint32_t TextureEpoch()
    {
        return gTextureEpoch;
    }

    uint64_t CreateTextureView(void* resource)
    {
        return CreateTextureViewFromResource(resource);
    }

    static void ReleaseCandidateCommandQueue()
    {
        if (gCandidateCommandQueue)
        {
            gCandidateCommandQueue->Release();
            gCandidateCommandQueue = nullptr;
        }
    }

    static bool IsDirectCommandQueue(ID3D12CommandQueue* queue)
    {
        return queue && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT;
    }

    static void RememberCandidateCommandQueue(ID3D12CommandQueue* queue)
    {
        if (!IsDirectCommandQueue(queue) || queue == gCandidateCommandQueue)
            return;

        queue->AddRef();
        ReleaseCandidateCommandQueue();
        gCandidateCommandQueue = queue;
        DebugLog("[d3d12hook] Remembered candidate CommandQueue=%p\n", queue);
    }

    static void ResetRecentDirectQueues()
    {
        for (UINT i = 0; i < gRecentDirectQueueCount; ++i)
            gRecentDirectQueues[i] = nullptr;
        gRecentDirectQueueCount = 0;
    }

    static bool IsKnownDirectQueue(ID3D12CommandQueue* queue)
    {
        for (UINT i = 0; i < gRecentDirectQueueCount; ++i)
        {
            if (gRecentDirectQueues[i] == queue)
                return true;
        }

        if (gRecentDirectQueueCount == kRecentDirectQueueCapacity)
        {
            for (UINT i = 1; i < kRecentDirectQueueCapacity; ++i)
                gRecentDirectQueues[i - 1] = gRecentDirectQueues[i];
            --gRecentDirectQueueCount;
        }

        gRecentDirectQueues[gRecentDirectQueueCount++] = queue;
        return false;
    }

    static bool AreComObjectsEqual(IUnknown* a, IUnknown* b)
    {
        if (a == b) return true;
        if (!a || !b) return false;
        Microsoft::WRL::ComPtr<IUnknown> unkA, unkB;
        if (FAILED(a->QueryInterface(IID_PPV_ARGS(&unkA)))) return false;
        if (FAILED(b->QueryInterface(IID_PPV_ARGS(&unkB)))) return false;
        return unkA.Get() == unkB.Get();
    }

    static bool IsQueueCompatibleWithTrackedDevice(ID3D12CommandQueue* queue)
    {
        if (!queue || !gDevice)
            return false;

        ID3D12Device* queueDevice = nullptr;
        const HRESULT hr = queue->GetDevice(__uuidof(ID3D12Device), (void**)&queueDevice);
        if (FAILED(hr) || !queueDevice)
            return false;

        const bool sameDevice = (queueDevice == gDevice) || AreComObjectsEqual(queueDevice, gDevice);
        queueDevice->Release();
        return sameDevice;
    }

    static void ReplaceTrackedCommandQueue(ID3D12CommandQueue* queue, const char* reason, bool announce = true)
    {
        if (!IsDirectCommandQueue(queue) || !IsQueueCompatibleWithTrackedDevice(queue))
            return;

        if (queue == gCommandQueue)
        {
            gMissingCommandQueueFrames = 0;
            return;
        }

        queue->AddRef();
        if (gCommandQueue)
            gCommandQueue->Release();
        gCommandQueue = queue;
        gMissingCommandQueueFrames = 0;

        if (announce)
            DebugLog("[d3d12hook] Updated CommandQueue=%p (%s)\n", gCommandQueue, reason ? reason : "unknown");
    }

    static bool TryPromoteCandidateCommandQueue()
    {
        if (gCommandQueue || !gCandidateCommandQueue || !gDevice)
            return gCommandQueue != nullptr;

        if (!IsQueueCompatibleWithTrackedDevice(gCandidateCommandQueue))
            return false;

        gCandidateCommandQueue->AddRef();
        gCommandQueue = gCandidateCommandQueue;
        gMissingCommandQueueFrames = 0;
        DebugLog("[d3d12hook] Promoted candidate CommandQueue=%p for tracked device %p\n", gCommandQueue, gDevice);
        return true;
    }


    static void ReleaseFrameContexts()

    {
        if (!gFrameContexts)
            return;

        for (UINT i = 0; i < gBufferCount; ++i)
        {
            if (gFrameContexts[i].renderTarget)
            {
                gFrameContexts[i].renderTarget->Release();
                gFrameContexts[i].renderTarget = nullptr;
            }
            if (gFrameContexts[i].allocator)
            {
                gFrameContexts[i].allocator->Release();
                gFrameContexts[i].allocator = nullptr;
            }
        }

        delete[] gFrameContexts;
        gFrameContexts = nullptr;
        gBufferCount = 0;
    }

    static void DestroyOverlayResources(bool removeInputHook)
    {
        if (removeInputHook && globals::mainWindow)
            inputhook::Remove(globals::mainWindow);

        if (gDx12BackendInitialized)
        {
            ImGui_ImplDX12_Shutdown();
            gDx12BackendInitialized = false;
        }

        if (gWin32Initialized)
        {
            ImGui_ImplWin32_Shutdown();
            gWin32Initialized = false;
        }

        if (ImGui::GetCurrentContext())
            ImGui::DestroyContext();

        if (gCommandList)
        {
            gCommandList->Release();
            gCommandList = nullptr;
        }

        if (gHeapRTV)
        {
            gHeapRTV->Release();
            gHeapRTV = nullptr;
        }

        if (gHeapSRV)
        {
            gHeapSRV->Release();
            gHeapSRV = nullptr;
            ++gTextureEpoch;
            ResetSrvSlots();
        }

        ReleaseFrameContexts();

        if (gOverlayFence)
        {
            gOverlayFence->Release();
            gOverlayFence = nullptr;
        }

        if (gFenceEvent)
        {
            CloseHandle(gFenceEvent);
            gFenceEvent = nullptr;
        }

        gSwapChain = nullptr;
        gOutputWindow = nullptr;
        gOverlayFenceValue = 0;
        gFramesToSkipAfterReinit = 0;
        gInitialized = false;

    }

    static void WaitForOverlayGpuIdle()
    {
        if (!gOverlayFence || !gFenceEvent || gOverlayFenceValue == 0)
            return;

        const UINT64 targetFence = gOverlayFenceValue;
        if (gOverlayFence->GetCompletedValue() >= targetFence)
            return;

        const HRESULT hr = gOverlayFence->SetEventOnCompletion(targetFence, gFenceEvent);
        if (FAILED(hr))
        {
            LogHRESULT("SetEventOnCompletion(reset)", hr);
            return;
        }

        const DWORD waitRes = WaitForSingleObject(gFenceEvent, 2000);
        if (waitRes == WAIT_TIMEOUT)
            DebugLog("[d3d12hook] WaitForOverlayGpuIdle timeout\n");
        else if (waitRes != WAIT_OBJECT_0)
            DebugLog("[d3d12hook] WaitForOverlayGpuIdle failed: %lu\n", GetLastError());
    }

    static void ResetState(bool removeInputHook, bool releaseQueueAndDevice)
    {
        WaitForOverlayGpuIdle();
        DestroyOverlayResources(removeInputHook);

        if (releaseQueueAndDevice)
        {
            if (gCommandQueue)
            {
                gCommandQueue->Release();
                gCommandQueue = nullptr;
            }

            ReleaseCandidateCommandQueue();
            ResetRecentDirectQueues();

            if (gDevice)
            {
                gDevice->Release();
                gDevice = nullptr;
            }

            gSawCompatibleSwapChain = false;
        }

        gAfterFirstPresent = false;

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

    static bool gDetectedNonDx12SwapChain = false;
    static ULONGLONG gFirstNonDx12DetectionTick = 0;
    static constexpr ULONGLONG kNonDx12ConfirmMs = 200;

    static bool TryCaptureDeviceFromSwapChain(IDXGISwapChain3* pSwapChain)
    {
        if (!pSwapChain)
            return false;

        ID3D12Device* swapChainDevice = nullptr;
        const HRESULT hr = pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&swapChainDevice);
        if (FAILED(hr))
        {
            ID3D11Device* d3d11Device = nullptr;
            if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&d3d11Device)))
            {
                d3d11Device->Release();
                gDetectedNonDx12SwapChain = true;
                DebugLog("[d3d12hook] D3D11 swapchain confirmed; signaling immediate DX11 fallback.\n");
            }
            return false;
        }

        NoteDx12Activity();
        gFirstNonDx12DetectionTick = 0;
        gSawCompatibleSwapChain = true;
        if (!gDevice)
        {
            gDevice = swapChainDevice;
            TryPromoteCandidateCommandQueue();
            return true;
        }

        const bool sameDevice = (swapChainDevice == gDevice) || AreComObjectsEqual(swapChainDevice, gDevice);
        swapChainDevice->Release();
        if (sameDevice)
            TryPromoteCandidateCommandQueue();
        return sameDevice;
    }


    static void LogRejectedWindow(bool hasDesc, HWND hwnd)
    {
        const ULONGLONG now = GetTickCount64();
        if ((now - gLastRejectedWindowLogTick) < 1000)
            return;

        DebugLog("[d3d12hook] Ignoring unusable output window %p (hasDesc=%d)\n", hwnd, hasDesc);
        gLastRejectedWindowLogTick = now;
    }

    static void LogMissingCommandQueueFrame()
    {
        if (gShutdown)
            return;

        ++gMissingCommandQueueFrames;
        if (gMissingCommandQueueFrames == 1)
        {
            DebugLog("[d3d12hook] CommandQueue not yet captured, skipping frame\n");
        }
        else if ((gMissingCommandQueueFrames % 120) == 0)
        {
            DebugLog("[d3d12hook] Still waiting for CommandQueue capture (skipped %u frames)\n", gMissingCommandQueueFrames);
        }
    }

    static void SuspendOverlayForTransition(IDXGISwapChain3* pSwapChain, const char* reason)
    {
        const bool trackedSwapChain = pSwapChain && gSwapChain && pSwapChain == gSwapChain;
        const bool hasOverlayState = gInitialized || gFrameContexts || gHeapRTV || gHeapSRV || gCommandList || gOutputWindow;
        if (!trackedSwapChain || !hasOverlayState)
            return;

        if (menu::isOpen)
        {
            menu::RememberStateForTransition();
            menu::SetRestoreDelayFrames(45);
            menu::isOpen = false;
            DebugLog("[d3d12hook] Closing menu for %s transition and delaying menu restore\n", reason);
        }



        ResetState(false, false);
    }

    void PrepareForInit()

    {
        gShutdown = false;
        gMissingCommandQueueFrames = 0;
        gLastRejectedWindowLogTick = 0;
        gDetectedNonDx12SwapChain = false;
        gFirstNonDx12DetectionTick = 0;
        ResetState(true, true);
    }

    bool ShouldFallbackImmediately()
    {
        return gDetectedNonDx12SwapChain;
    }

    bool IsInitializationPending()
    {
        if (gDetectedNonDx12SwapChain)
            return false;

        const bool recentDx12Activity =
            gLastDx12ActivityTick != 0 &&
            (GetTickCount64() - gLastDx12ActivityTick) <= 5000;
        return !gShutdown && !gInitialized &&
            (gSawCompatibleSwapChain || gCommandQueue != nullptr || gCandidateCommandQueue != nullptr || recentDx12Activity);
    }


    long __fastcall hookPresentD3D12(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags) {
        if (IsMenuToggleRequested()) {
            menu::isOpen = !menu::isOpen;
            DebugLog("[d3d12hook] Toggle menu: isOpen=%d\n", menu::isOpen);
        }

        if (WasUninjectHotkeyPressed()) {
            DebugLog("[d3d12hook] Detach hotkey detected.\n");
            Uninject();
            return oPresentD3D12(pSwapChain, SyncInterval, Flags);
        }

        if (gShutdown)
            return oPresentD3D12(pSwapChain, SyncInterval, Flags);

        gAfterFirstPresent = true;

        DXGI_SWAP_CHAIN_DESC desc = {};
        const HRESULT descHr = pSwapChain ? pSwapChain->GetDesc(&desc) : E_POINTER;
        const bool hasDesc = SUCCEEDED(descHr);
        const bool usableWindow = hasDesc && IsUsableOutputWindow(desc.OutputWindow);

        TryCaptureDeviceFromSwapChain(pSwapChain);

        if (!gCommandQueue) {
            LogMissingCommandQueueFrame();
            return oPresentD3D12(pSwapChain, SyncInterval, Flags);
        }

        if (gInitialized)
        {
            const bool swapChainChanged = pSwapChain != gSwapChain;
            const bool windowChanged = usableWindow && gOutputWindow && desc.OutputWindow != gOutputWindow;
            if (swapChainChanged || windowChanged)
            {
                DebugLog("[d3d12hook] Rebinding overlay for swapchain %p window %p (previous swapchain %p window %p)\n",
                    pSwapChain, hasDesc ? desc.OutputWindow : nullptr, gSwapChain, gOutputWindow);
                ResetState(windowChanged || swapChainChanged, swapChainChanged);
            }
        }

        if (!gInitialized) {
            if (!TryCaptureDeviceFromSwapChain(pSwapChain))
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);

            if (!hasDesc)
            {
                LogHRESULT("GetDesc", descHr);
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            if (!usableWindow)
            {
                LogRejectedWindow(hasDesc, desc.OutputWindow);
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            DebugLog("[d3d12hook] Initializing ImGui on first Present.\n");
            gSwapChain = pSwapChain;
            gOutputWindow = desc.OutputWindow;
            gBufferCount = desc.BufferCount;
            if (gBufferCount == 0)
            {
                DebugLog("[d3d12hook] Swapchain reported zero buffers; waiting for a valid swapchain.\n");
                gSwapChain = nullptr;
                gOutputWindow = nullptr;
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }
            DebugLog("[d3d12hook] BufferCount=%u\n", gBufferCount);

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            heapDesc.NumDescriptors = gBufferCount;
            HRESULT hr = gDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&gHeapRTV));
            if (FAILED(hr)) {
                LogHRESULT("CreateDescriptorHeap RTV", hr);
                ResetState(false, false);
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.NumDescriptors = kSrvHeapCapacity;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            hr = gDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&gHeapSRV));
            if (SUCCEEDED(hr))
            {
                gSrvSlotSize = gDevice->GetDescriptorHandleIncrementSize(
                    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                ResetSrvSlots();
                ++gTextureEpoch;
            }
            if (FAILED(hr)) {
                LogHRESULT("CreateDescriptorHeap SRV", hr);
                ResetState(false, false);
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            gFrameContexts = new FrameContext[gBufferCount];
            ZeroMemory(gFrameContexts, sizeof(FrameContext) * gBufferCount);

            for (UINT i = 0; i < gBufferCount; ++i) {
                hr = gDevice->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS(&gFrameContexts[i].allocator));
                if (FAILED(hr)) {
                    LogHRESULT("CreateCommandAllocator", hr);
                    ResetState(false, false);
                    return oPresentD3D12(pSwapChain, SyncInterval, Flags);
                }
            }

            const UINT rtvSize = gDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            auto rtvHandle = gHeapRTV->GetCPUDescriptorHandleForHeapStart();
            for (UINT i = 0; i < gBufferCount; ++i) {
                ID3D12Resource* back = nullptr;
                hr = pSwapChain->GetBuffer(i, IID_PPV_ARGS(&back));
                if (FAILED(hr) || !back) {
                    LogHRESULT("GetBuffer", FAILED(hr) ? hr : E_FAIL);
                    ResetState(false, false);
                    return oPresentD3D12(pSwapChain, SyncInterval, Flags);
                }

                gDevice->CreateRenderTargetView(back, nullptr, rtvHandle);
                gFrameContexts[i].renderTarget = back;
                gFrameContexts[i].rtvHandle = rtvHandle;
                rtvHandle.ptr += rtvSize;
            }

            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            ImGui::StyleColorsDark();

            gWin32Initialized = ImGui_ImplWin32_Init(desc.OutputWindow);
            if (gWin32Initialized)
            {
                ImGui_ImplDX12_InitInfo initInfo{};
                initInfo.Device = gDevice;
                initInfo.CommandQueue = gCommandQueue;
                initInfo.NumFramesInFlight = static_cast<int>(gBufferCount);
                initInfo.RTVFormat = desc.BufferDesc.Format;
                initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
                initInfo.SrvDescriptorHeap = gHeapSRV;
                initInfo.SrvDescriptorAllocFn = &ImGuiSrvAlloc;
                initInfo.SrvDescriptorFreeFn = &ImGuiSrvFree;
                initInfo.LegacySingleSrvCpuDescriptor = gHeapSRV->GetCPUDescriptorHandleForHeapStart();
                initInfo.LegacySingleSrvGpuDescriptor = gHeapSRV->GetGPUDescriptorHandleForHeapStart();
                gDx12BackendInitialized = ImGui_ImplDX12_Init(&initInfo);
            }

            if (!gDx12BackendInitialized)
            {
                DebugLog("[d3d12hook] ImGui backend initialization failed.\n");
                ResetState(false, false);
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            inputhook::Init(desc.OutputWindow);

            hr = gDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gOverlayFence));
            if (FAILED(hr)) {
                LogHRESULT("CreateFence", hr);
                ResetState(true, false);
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            gFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (!gFenceEvent) {
                DebugLog("[d3d12hook] Failed to create fence event: %lu\n", GetLastError());
                ResetState(true, false);
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            gInitialized = true;
            gMissingCommandQueueFrames = 0;
            gFramesToSkipAfterReinit = 3;
            DebugLog("[d3d12hook] ImGui initialized.\n");

        }

        if (!gShutdown) {
            if (gFramesToSkipAfterReinit > 0)
            {
                --gFramesToSkipAfterReinit;
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            // Render ImGui
            menu::RestoreStateAfterTransition();
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            menu::OnFrame();
            menu::RenderOverlay();

            if (menu::isOpen) {
                menu::Init();
            }

            UINT frameIdx = pSwapChain->GetCurrentBackBufferIndex();
            if (gBufferCount == 0 || !gFrameContexts) {
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }
            if (frameIdx >= gBufferCount) {
                frameIdx %= gBufferCount;
            }
            FrameContext& ctx = gFrameContexts[frameIdx];

            // Wait for the GPU to finish with THIS frame's allocator before reusing it
            bool canRender = true;
            if (!gOverlayFence || !gFenceEvent) {
                // Missing synchronization objects, skip waiting
            } else if (ctx.FenceValue != 0 && gOverlayFence->GetCompletedValue() < ctx.FenceValue) {
                HRESULT hr = gOverlayFence->SetEventOnCompletion(ctx.FenceValue, gFenceEvent);
                if (SUCCEEDED(hr)) {
                    const DWORD waitTimeoutMs = 2000; // Extended timeout
                    DWORD waitRes = WaitForSingleObject(gFenceEvent, waitTimeoutMs);
                    if (waitRes == WAIT_TIMEOUT) {
                        DebugLog("[d3d12hook] WaitForSingleObject timeout\n");
                        canRender = false;
                    } else if (waitRes != WAIT_OBJECT_0) {
                        DebugLog("[d3d12hook] WaitForSingleObject failed: %lu\n", GetLastError());
                        canRender = false;
                    }
                } else {
                    LogHRESULT("SetEventOnCompletion", hr);
                    canRender = false;
                }
            }

            if (!canRender) {
                ImGui::EndFrame();
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            // Reset allocator and command list using frame-specific allocator
            HRESULT hr = ctx.allocator->Reset();
            if (FAILED(hr)) {
                LogHRESULT("CommandAllocator->Reset", hr);
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            if (!gCommandList) {
                hr = gDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                    ctx.allocator, nullptr, IID_PPV_ARGS(&gCommandList));
                if (FAILED(hr)) {
                    LogHRESULT("CreateCommandList", hr);
                    return oPresentD3D12(pSwapChain, SyncInterval, Flags);
                }
                gCommandList->Close();
            }
            hr = gCommandList->Reset(ctx.allocator, nullptr);
            if (FAILED(hr)) {
                LogHRESULT("CommandList->Reset", hr);
                return oPresentD3D12(pSwapChain, SyncInterval, Flags);
            }

            // Transition to render target
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = ctx.renderTarget;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            gCommandList->ResourceBarrier(1, &barrier);

            gCommandList->OMSetRenderTargets(1, &ctx.rtvHandle, FALSE, nullptr);
            ID3D12DescriptorHeap* heaps[] = { gHeapSRV };
            gCommandList->SetDescriptorHeaps(1, heaps);

            ImGui::Render();
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), gCommandList);

            // Transition back to present
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            gCommandList->ResourceBarrier(1, &barrier);
            gCommandList->Close();

            // Execute
            if (!gCommandQueue) {
                DebugLog("[d3d12hook] CommandQueue not set, skipping ExecuteCommandLists.\n");
            }
            else {
                oExecuteCommandListsD3D12(gCommandQueue, 1, reinterpret_cast<ID3D12CommandList* const*>(&gCommandList));
                if (gOverlayFence) {
                    // Call Signal directly on the command queue to synchronize this frame's allocator.
                    ctx.FenceValue = ++gOverlayFenceValue;
                    HRESULT hr = gCommandQueue->Signal(gOverlayFence, ctx.FenceValue);
                    if (FAILED(hr)) {
                        LogHRESULT("Signal", hr);
                        if (gDevice) {
                            HRESULT reason = gDevice->GetDeviceRemovedReason();
                            DebugLog("[d3d12hook] DeviceRemovedReason=0x%08X\n", reason);
                            if (reason != S_OK) {
                                DebugLog("[d3d12hook] Device lost. Releasing resources.\n");
                                release();
                            }
                        }
                    }
                }
            }
        }

        return oPresentD3D12(pSwapChain, SyncInterval, Flags);

    }

    long __fastcall hookPresent1D3D12(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pParams) {
        if (IsMenuToggleRequested()) {
            menu::isOpen = !menu::isOpen;
            DebugLog("[d3d12hook] Toggle menu: isOpen=%d\n", menu::isOpen);
        }

        if (WasUninjectHotkeyPressed()) {
            DebugLog("[d3d12hook] Detach hotkey detected.\n");
            Uninject();
            return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
        }

        if (gShutdown)
            return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);

        gAfterFirstPresent = true;
        NoteDx12Activity();

        DXGI_SWAP_CHAIN_DESC desc = {};

        const HRESULT descHr = pSwapChain ? pSwapChain->GetDesc(&desc) : E_POINTER;
        const bool hasDesc = SUCCEEDED(descHr);
        const bool usableWindow = hasDesc && IsUsableOutputWindow(desc.OutputWindow);

        TryCaptureDeviceFromSwapChain(pSwapChain);

        if (!gCommandQueue) {
            LogMissingCommandQueueFrame();
            return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
        }

        if (gInitialized)
        {
            const bool swapChainChanged = pSwapChain != gSwapChain;
            const bool windowChanged = usableWindow && gOutputWindow && desc.OutputWindow != gOutputWindow;
            if (swapChainChanged || windowChanged)
            {
                DebugLog("[d3d12hook] Rebinding overlay for swapchain %p window %p (previous swapchain %p window %p)\n",
                    pSwapChain, hasDesc ? desc.OutputWindow : nullptr, gSwapChain, gOutputWindow);
                ResetState(windowChanged || swapChainChanged, swapChainChanged);
            }
        }

        if (!gInitialized) {
            if (!TryCaptureDeviceFromSwapChain(pSwapChain))
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);

            if (!hasDesc)
            {
                LogHRESULT("GetDesc", descHr);
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            if (!usableWindow)
            {
                LogRejectedWindow(hasDesc, desc.OutputWindow);
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            DebugLog("[d3d12hook] Initializing ImGui on first Present1.\n");
            gSwapChain = pSwapChain;
            gOutputWindow = desc.OutputWindow;
            gBufferCount = desc.BufferCount;
            if (gBufferCount == 0)
            {
                DebugLog("[d3d12hook] Swapchain reported zero buffers; waiting for a valid swapchain.\n");
                gSwapChain = nullptr;
                gOutputWindow = nullptr;
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }
            DebugLog("[d3d12hook] BufferCount=%u\n", gBufferCount);

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            heapDesc.NumDescriptors = gBufferCount;
            HRESULT hr = gDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&gHeapRTV));
            if (FAILED(hr)) {
                LogHRESULT("CreateDescriptorHeap RTV", hr);
                ResetState(false, false);
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.NumDescriptors = kSrvHeapCapacity;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            hr = gDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&gHeapSRV));
            if (SUCCEEDED(hr))
            {
                gSrvSlotSize = gDevice->GetDescriptorHandleIncrementSize(
                    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                ResetSrvSlots();
                ++gTextureEpoch;
            }
            if (FAILED(hr)) {
                LogHRESULT("CreateDescriptorHeap SRV", hr);
                ResetState(false, false);
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            gFrameContexts = new FrameContext[gBufferCount];
            ZeroMemory(gFrameContexts, sizeof(FrameContext) * gBufferCount);

            for (UINT i = 0; i < gBufferCount; ++i) {
                hr = gDevice->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS(&gFrameContexts[i].allocator));
                if (FAILED(hr)) {
                    LogHRESULT("CreateCommandAllocator", hr);
                    ResetState(false, false);
                    return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
                }
            }

            const UINT rtvSize = gDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            auto rtvHandle = gHeapRTV->GetCPUDescriptorHandleForHeapStart();
            for (UINT i = 0; i < gBufferCount; ++i) {
                ID3D12Resource* back = nullptr;
                hr = pSwapChain->GetBuffer(i, IID_PPV_ARGS(&back));
                if (FAILED(hr) || !back) {
                    LogHRESULT("GetBuffer", FAILED(hr) ? hr : E_FAIL);
                    ResetState(false, false);
                    return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
                }

                gDevice->CreateRenderTargetView(back, nullptr, rtvHandle);
                gFrameContexts[i].renderTarget = back;
                gFrameContexts[i].rtvHandle = rtvHandle;
                rtvHandle.ptr += rtvSize;
            }

            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            ImGui::StyleColorsDark();

            gWin32Initialized = ImGui_ImplWin32_Init(desc.OutputWindow);
            if (gWin32Initialized)
            {
                ImGui_ImplDX12_InitInfo initInfo{};
                initInfo.Device = gDevice;
                initInfo.CommandQueue = gCommandQueue;
                initInfo.NumFramesInFlight = static_cast<int>(gBufferCount);
                initInfo.RTVFormat = desc.BufferDesc.Format;
                initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
                initInfo.SrvDescriptorHeap = gHeapSRV;
                initInfo.SrvDescriptorAllocFn = &ImGuiSrvAlloc;
                initInfo.SrvDescriptorFreeFn = &ImGuiSrvFree;
                initInfo.LegacySingleSrvCpuDescriptor = gHeapSRV->GetCPUDescriptorHandleForHeapStart();
                initInfo.LegacySingleSrvGpuDescriptor = gHeapSRV->GetGPUDescriptorHandleForHeapStart();
                gDx12BackendInitialized = ImGui_ImplDX12_Init(&initInfo);
            }

            if (!gDx12BackendInitialized)
            {
                DebugLog("[d3d12hook] ImGui backend initialization failed.\n");
                ResetState(false, false);
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            inputhook::Init(desc.OutputWindow);

            hr = gDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gOverlayFence));
            if (FAILED(hr)) {
                LogHRESULT("CreateFence", hr);
                ResetState(true, false);
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            gFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (!gFenceEvent) {
                DebugLog("[d3d12hook] Failed to create fence event: %lu\n", GetLastError());
                ResetState(true, false);
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            gInitialized = true;
            gMissingCommandQueueFrames = 0;
            gFramesToSkipAfterReinit = 3;
            DebugLog("[d3d12hook] ImGui initialized.\n");

        }

        if (!gShutdown) {
            if (gFramesToSkipAfterReinit > 0)
            {
                --gFramesToSkipAfterReinit;
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            // Render ImGui
            menu::RestoreStateAfterTransition();
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            menu::OnFrame();
            menu::RenderOverlay();

            if (menu::isOpen) {
                menu::Init();
            }

            UINT frameIdx = pSwapChain->GetCurrentBackBufferIndex();
            if (gBufferCount == 0 || !gFrameContexts) {
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }
            if (frameIdx >= gBufferCount) {
                frameIdx %= gBufferCount;
            }
            FrameContext& ctx = gFrameContexts[frameIdx];

            // Wait for the GPU to finish with THIS frame's allocator before reusing it
            bool canRender = true;
            if (!gOverlayFence || !gFenceEvent) {
                // Missing synchronization objects, skip waiting
            } else if (ctx.FenceValue != 0 && gOverlayFence->GetCompletedValue() < ctx.FenceValue) {
                HRESULT hr = gOverlayFence->SetEventOnCompletion(ctx.FenceValue, gFenceEvent);
                if (SUCCEEDED(hr)) {
                    const DWORD waitTimeoutMs = 2000; // Extended timeout
                    DWORD waitRes = WaitForSingleObject(gFenceEvent, waitTimeoutMs);
                    if (waitRes == WAIT_TIMEOUT) {
                        DebugLog("[d3d12hook] WaitForSingleObject timeout\n");
                        canRender = false;
                    } else if (waitRes != WAIT_OBJECT_0) {
                        DebugLog("[d3d12hook] WaitForSingleObject failed: %lu\n", GetLastError());
                        canRender = false;
                    }
                } else {
                    LogHRESULT("SetEventOnCompletion", hr);
                    canRender = false;
                }
            }

            if (!canRender) {
                ImGui::EndFrame();
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            // Reset allocator and command list using frame-specific allocator
            HRESULT hr = ctx.allocator->Reset();
            if (FAILED(hr)) {
                LogHRESULT("CommandAllocator->Reset", hr);
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            if (!gCommandList) {
                hr = gDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                    ctx.allocator, nullptr, IID_PPV_ARGS(&gCommandList));
                if (FAILED(hr)) {
                    LogHRESULT("CreateCommandList", hr);
                    return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
                }
                gCommandList->Close();
            }
            hr = gCommandList->Reset(ctx.allocator, nullptr);
            if (FAILED(hr)) {
                LogHRESULT("CommandList->Reset", hr);
                return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
            }

            // Transition to render target
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = ctx.renderTarget;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            gCommandList->ResourceBarrier(1, &barrier);

            gCommandList->OMSetRenderTargets(1, &ctx.rtvHandle, FALSE, nullptr);
            ID3D12DescriptorHeap* heaps[] = { gHeapSRV };
            gCommandList->SetDescriptorHeaps(1, heaps);

            ImGui::Render();
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), gCommandList);

            // Transition back to present
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            gCommandList->ResourceBarrier(1, &barrier);
            gCommandList->Close();

            // Execute
            if (!gCommandQueue) {
                DebugLog("[d3d12hook] CommandQueue not set, skipping ExecuteCommandLists.\n");
            }
            else {
                oExecuteCommandListsD3D12(gCommandQueue, 1, reinterpret_cast<ID3D12CommandList* const*>(&gCommandList));
                if (gOverlayFence) {
                    // Call Signal directly on the command queue to synchronize this frame's allocator.
                    ctx.FenceValue = ++gOverlayFenceValue;
                    HRESULT hr = gCommandQueue->Signal(gOverlayFence, ctx.FenceValue);
                    if (FAILED(hr)) {
                        LogHRESULT("Signal", hr);
                        if (gDevice) {
                            HRESULT reason = gDevice->GetDeviceRemovedReason();
                            DebugLog("[d3d12hook] DeviceRemovedReason=0x%08X\n", reason);
                            if (reason != S_OK) {
                                DebugLog("[d3d12hook] Device lost. Releasing resources.\n");
                                release();
                            }
                        }
                    }
                }
            }
        }

        return oPresent1D3D12(pSwapChain, SyncInterval, Flags, pParams);
    }

    void STDMETHODCALLTYPE hookExecuteCommandListsD3D12(
        ID3D12CommandQueue* _this,
        UINT                          NumCommandLists,
        ID3D12CommandList* const* ppCommandLists) {
        if (!gShutdown && IsDirectCommandQueue(_this)) {
            NoteDx12Activity();
            if (!gCommandQueue)
                RememberCandidateCommandQueue(_this);

            ID3D12Device* queueDevice = nullptr;
            if (SUCCEEDED(_this->GetDevice(__uuidof(ID3D12Device), (void**)&queueDevice))) {
                if (!gDevice) {
                    gDevice = queueDevice;
                    queueDevice->AddRef();
                }

                const bool sameDevice = (queueDevice == gDevice) || AreComObjectsEqual(queueDevice, gDevice);
                if (sameDevice) {
                    const bool knownQueue = IsKnownDirectQueue(_this);
                    const bool hadTrackedQueue = (gCommandQueue != nullptr);
                    const bool sameTrackedQueue = (_this == gCommandQueue);

                    if (!hadTrackedQueue) {
                        if (!TryPromoteCandidateCommandQueue() && IsQueueCompatibleWithTrackedDevice(_this)) {
                            ReplaceTrackedCommandQueue(_this, "ExecuteCommandLists capture");
                        }

                        if (gCommandQueue == _this) {
                            if (gMissingCommandQueueFrames > 0)
                                DebugLog("[d3d12hook] Captured CommandQueue=%p after %u skipped frames\n", _this, gMissingCommandQueueFrames);
                            else
                                DebugLog("[d3d12hook] Captured CommandQueue=%p\n", _this);
                        }
                    }
                    else if (!sameTrackedQueue && IsQueueCompatibleWithTrackedDevice(_this)) {
                        ReplaceTrackedCommandQueue(_this, "ExecuteCommandLists queue switch", !knownQueue);
                    }
                }
                else {
                    DebugLog("[d3d12hook] Skipping capture: CommandQueue uses different device (%p != %p)\n", queueDevice, gDevice);
                }

                queueDevice->Release();
            }
        }
        gAfterFirstPresent = false;
        oExecuteCommandListsD3D12(_this, NumCommandLists, ppCommandLists);
    }



    HRESULT STDMETHODCALLTYPE hookResizeBuffersD3D12(
        IDXGISwapChain3* pSwapChain,
        UINT BufferCount,
        UINT Width,
        UINT Height,
        DXGI_FORMAT NewFormat,
        UINT SwapChainFlags)
    {
        const bool trackedSwapChain = pSwapChain && gSwapChain && pSwapChain == gSwapChain;
        if (trackedSwapChain)
        {
            DebugLog("[d3d12hook] ResizeBuffers called: %ux%u Buffers=%u Format=%u Flags=0x%X\n",
                Width, Height, BufferCount, static_cast<unsigned int>(NewFormat), SwapChainFlags);
            SuspendOverlayForTransition(pSwapChain, "ResizeBuffers");
        }

        const HRESULT hr = oResizeBuffersD3D12(
            pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

        if (SUCCEEDED(hr) && trackedSwapChain)
            DebugLog("[d3d12hook] ResizeBuffers succeeded; overlay will reinitialize on next present\n");
        else if (FAILED(hr) && trackedSwapChain)
            DebugLog("[d3d12hook] ResizeBuffers failed: 0x%08X\n", hr);

        return hr;
    }

    HRESULT STDMETHODCALLTYPE hookResizeBuffers1D3D12(
        IDXGISwapChain3* pSwapChain,
        UINT BufferCount,
        UINT Width,
        UINT Height,
        DXGI_FORMAT NewFormat,
        UINT SwapChainFlags,
        const UINT* pCreationNodeMask,
        IUnknown* const* ppPresentQueue)
    {
        const bool trackedSwapChain = pSwapChain && gSwapChain && pSwapChain == gSwapChain;
        const UINT effectiveQueueCount = (BufferCount != 0) ? BufferCount : static_cast<UINT>(gBufferCount);
        ID3D12CommandQueue* replacementQueue = nullptr;

        if (trackedSwapChain)
        {
            IUnknown* firstPresentQueue = (ppPresentQueue && effectiveQueueCount > 0) ? ppPresentQueue[0] : nullptr;
            DebugLog("[d3d12hook] ResizeBuffers1 called: %ux%u Buffers=%u Format=%u Flags=0x%X FirstQueue=%p NodeMask=%p\n",
                Width, Height, BufferCount, static_cast<unsigned int>(NewFormat), SwapChainFlags, firstPresentQueue, pCreationNodeMask);

            if (ppPresentQueue && effectiveQueueCount > 0)
            {
                for (UINT i = 0; i < effectiveQueueCount; ++i)
                {
                    IUnknown* presentQueue = ppPresentQueue[i];
                    if (!presentQueue)
                        continue;

                    ID3D12CommandQueue* queue = nullptr;
                    const HRESULT queueHr = presentQueue->QueryInterface(__uuidof(ID3D12CommandQueue), (void**)&queue);
                    if (SUCCEEDED(queueHr) && queue)
                    {
                        if (IsDirectCommandQueue(queue) && IsQueueCompatibleWithTrackedDevice(queue))
                        {
                            replacementQueue = queue;
                            DebugLog("[d3d12hook] ResizeBuffers1 selected present queue[%u]=%p\n", i, queue);
                            break;
                        }
                        queue->Release();
                    }
                }
            }

            SuspendOverlayForTransition(pSwapChain, "ResizeBuffers1");
        }

        const HRESULT hr = oResizeBuffers1D3D12(
            pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags, pCreationNodeMask, ppPresentQueue);

        if (SUCCEEDED(hr) && trackedSwapChain)
        {
            if (replacementQueue)
                ReplaceTrackedCommandQueue(replacementQueue, "ResizeBuffers1");
            DebugLog("[d3d12hook] ResizeBuffers1 succeeded; overlay will reinitialize on next present\n");
        }
        else if (FAILED(hr) && trackedSwapChain)
        {
            DebugLog("[d3d12hook] ResizeBuffers1 failed: 0x%08X\n", hr);
        }

        if (replacementQueue)
            replacementQueue->Release();

        return hr;
    }


    HRESULT STDMETHODCALLTYPE hookSetFullscreenStateD3D12(
        IDXGISwapChain3* pSwapChain,
        BOOL Fullscreen,
        IDXGIOutput* pTarget)
    {
        const bool trackedSwapChain = pSwapChain && gSwapChain && pSwapChain == gSwapChain;
        if (trackedSwapChain)
        {
            DebugLog("[d3d12hook] SetFullscreenState called: Fullscreen=%d Target=%p\n", Fullscreen, pTarget);
            SuspendOverlayForTransition(pSwapChain, "SetFullscreenState");
        }

        return oSetFullscreenStateD3D12(pSwapChain, Fullscreen, pTarget);
    }

    HRESULT STDMETHODCALLTYPE hookResizeTargetD3D12(
        IDXGISwapChain3* pSwapChain,
        const DXGI_MODE_DESC* pNewTargetParameters)
    {
        const bool trackedSwapChain = pSwapChain && gSwapChain && pSwapChain == gSwapChain;
        if (trackedSwapChain)
        {
            if (pNewTargetParameters)
            {
                DebugLog("[d3d12hook] ResizeTarget called: %ux%u Format=%u Refresh=%u/%u\n",
                    pNewTargetParameters->Width,
                    pNewTargetParameters->Height,
                    static_cast<unsigned int>(pNewTargetParameters->Format),
                    pNewTargetParameters->RefreshRate.Numerator,
                    pNewTargetParameters->RefreshRate.Denominator);
            }
            else
            {
                DebugLog("[d3d12hook] ResizeTarget called with null mode desc\n");
            }
            SuspendOverlayForTransition(pSwapChain, "ResizeTarget");
        }

        return oResizeTargetD3D12(pSwapChain, pNewTargetParameters);
    }


    void release() {
        DebugLog("[d3d12hook] Releasing resources and hooks.\n");
        gShutdown = true;
        gMissingCommandQueueFrames = 0;
        gLastRejectedWindowLogTick = 0;
        ResetState(true, true);

        // Disable hooks installed for D3D12
        hooks::Remove();
    }

    bool IsInitialized()
    {
        return gInitialized;
    }
}

extern "C" bool D3D12Hook_TextureBridgeReady()
{
    return d3d12hook::TextureBridgeReady();
}

extern "C" uint32_t D3D12Hook_TextureEpoch()
{
    return d3d12hook::TextureEpoch();
}

extern "C" uint64_t D3D12Hook_CreateTextureView(void* resource)
{
    return d3d12hook::CreateTextureView(resource);
}
