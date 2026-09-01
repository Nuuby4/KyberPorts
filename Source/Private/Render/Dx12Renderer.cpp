// Copyright Nuuby. All Rights Reserved.

#include "Render/Dx12Renderer.h"

#include <Utilities/ErrorUtils.h>
#include <Hook/HookManager.h>
#include <Base/Log.h>

#include "Render/MainWindow.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx12.h"
#include <d3d12.h>
#include <dxgi1_5.h>
#include <tchar.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HWND hWnd = 0;
static WNDPROC lpPrevWndFunc;

namespace Kyber
{
Dx12Renderer* g_renderer = nullptr;

LRESULT CALLBACK WndProcHk(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (g_renderer && g_renderer->m_initialized)
    {
        if (Msg == WM_KEYUP && (wParam == VK_INSERT || (g_mainWindow->IsEnabled() && wParam == VK_ESCAPE)))
        {
            if (g_mainWindow)
            {
                g_mainWindow->Toggle();
            }
        }

        if (g_mainWindow && g_mainWindow->IsEnabled())
        {
            ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam);
        }
    }

    return CallWindowProc(lpPrevWndFunc, hWnd, Msg, wParam, lParam);
}

HRESULT ExecuteCommandListsHk(ID3D12CommandQueue* pInstance, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists)
{
    static const auto trampoline = HookManager::Call(ExecuteCommandListsHk);

    if (g_renderer && !g_renderer->m_currentCommandQueue)
    {
        D3D12_COMMAND_QUEUE_DESC desc = pInstance->GetDesc();
        if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
        {
            KYBER_LOG(LogLevel::DebugPlusPlus, "Captured real D3D12 command queue");
            g_renderer->m_currentCommandQueue = pInstance;
        }
    }

    return trampoline(pInstance, NumCommandLists, ppCommandLists);
}

HRESULT PresentHk(IDXGISwapChain3* pInstance, UINT syncInterval, UINT flags)
{
    static const auto trampoline = HookManager::Call(PresentHk);

    static MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    if (::PeekMessage(&msg, hWnd, 0U, 0U, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    if (!g_renderer)
    {
        return trampoline(pInstance, syncInterval, flags);
    }

    if (!g_renderer->m_currentDevice)
    {
        KYBER_LOG(LogLevel::Debug, "Initializing Current Rendering Device");
        pInstance->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(&g_renderer->m_currentDevice));
    }

    if (!g_renderer->m_currentCommandQueue)
    {
        return trampoline(pInstance, syncInterval, flags);
    }

    if (!g_renderer->m_initialized)
    {
        if (!g_renderer->InitializeImGui(pInstance))
        {
            return trampoline(pInstance, syncInterval, flags);
        }
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_mainWindow)
    {
        g_mainWindow->Draw();
    }

    ImGui::Render();

    UINT backBufferIndex = pInstance->GetCurrentBackBufferIndex();
    Dx12Renderer::FrameContext& frame = g_renderer->m_frameContexts[backBufferIndex];

    frame.CommandAllocator->Reset();
    g_renderer->m_commandList->Reset(frame.CommandAllocator, nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = frame.BackBuffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    g_renderer->m_commandList->ResourceBarrier(1, &barrier);

    g_renderer->m_commandList->OMSetRenderTargets(1, &frame.RtvHandle, FALSE, nullptr);
    ID3D12DescriptorHeap* heaps[] = { g_renderer->m_srvDescHeap };
    g_renderer->m_commandList->SetDescriptorHeaps(1, heaps);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_renderer->m_commandList);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_renderer->m_commandList->ResourceBarrier(1, &barrier);

    g_renderer->m_commandList->Close();

    ID3D12CommandList* lists[] = { g_renderer->m_commandList };
    g_renderer->m_currentCommandQueue->ExecuteCommandLists(1, lists);

    return trampoline(pInstance, syncInterval, flags);
}

HRESULT ResizeBuffersHk(IDXGISwapChain3* pInstance, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
    static const auto trampoline = HookManager::Call(ResizeBuffersHk);

    if (g_renderer && g_renderer->m_initialized)
    {
        g_renderer->ReleaseRenderTargets();
    }

    HRESULT hr = trampoline(pInstance, BufferCount, Width, Height, NewFormat, SwapChainFlags);

    if (g_renderer && g_renderer->m_initialized)
    {
        g_renderer->CreateRenderTargets(pInstance);
    }

    return hr;
}

Dx12Renderer::Dx12Renderer()
{
    if (g_renderer)
    {
        ErrorUtils::ThrowException("Renderer is already initialized.");
    }

    KYBER_LOG(LogLevel::Debug, "Initializing Renderer");

    hWnd = FindWindow(nullptr, "Dead Space");

    if (!hWnd)
    {
        KYBER_LOG(LogLevel::Error, "Failed to find Window");
        return;
    }

    g_mainWindow = new MainWindow();

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "DummyClass";
    RegisterClassExA(&wc);

    HWND dummyHwnd =
        CreateWindowExA(0, wc.lpszClassName, "Dummy", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    if (!dummyHwnd)
    {
        KYBER_LOG(LogLevel::Error, "Failed to create dummy window");
        return;
    }

    ID3D12Device* pDummyDevice = nullptr;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDummyDevice))) || !pDummyDevice)
    {
        KYBER_LOG(LogLevel::Error, "Failed to create dummy D3D12 device");
        DestroyWindow(dummyHwnd);
        return;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ID3D12CommandQueue* pDummyQueue = nullptr;
    if (FAILED(pDummyDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pDummyQueue))) || !pDummyQueue)
    {
        KYBER_LOG(LogLevel::Error, "Failed to create dummy command queue");
        pDummyDevice->Release();
        DestroyWindow(dummyHwnd);
        return;
    }

    IDXGIFactory4* pFactory = nullptr;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&pFactory))) || !pFactory)
    {
        KYBER_LOG(LogLevel::Error, "Failed to create DXGI factory");
        pDummyQueue->Release();
        pDummyDevice->Release();
        DestroyWindow(dummyHwnd);
        return;
    }

    DXGI_SWAP_CHAIN_DESC1 swapDesc{};
    swapDesc.BufferCount = 2;
    swapDesc.Width = 100;
    swapDesc.Height = 100;
    swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.SampleDesc.Count = 1;

    IDXGISwapChain1* pSwapChain1 = nullptr;
    if (FAILED(pFactory->CreateSwapChainForHwnd(pDummyQueue, dummyHwnd, &swapDesc, nullptr, nullptr, &pSwapChain1)) || !pSwapChain1)
    {
        KYBER_LOG(LogLevel::Error, "Failed to create dummy swap chain");
        pFactory->Release();
        pDummyQueue->Release();
        pDummyDevice->Release();
        DestroyWindow(dummyHwnd);
        return;
    }

    IDXGISwapChain3* pSwapChain3 = nullptr;
    pSwapChain1->QueryInterface(IID_PPV_ARGS(&pSwapChain3));

    auto pSwapChainTable = *reinterpret_cast<PVOID**>(pSwapChain3);
    auto pPresent = pSwapChainTable[8];
    auto pResizeBuffers = pSwapChainTable[13];

    auto pQueueTable = *reinterpret_cast<PVOID**>(pDummyQueue);
    auto pExecuteCommandLists = pQueueTable[10];

    pSwapChain3->Release();
    pSwapChain1->Release();
    pFactory->Release();
    pDummyQueue->Release();
    pDummyDevice->Release();
    DestroyWindow(dummyHwnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    HookManager::CreateHook(pPresent, PresentHk);
    HookManager::CreateHook(pResizeBuffers, ResizeBuffersHk);
    HookManager::CreateHook(pExecuteCommandLists, ExecuteCommandListsHk);
    Hook::ApplyQueuedActions();

    lpPrevWndFunc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProcHk)));

    if (!lpPrevWndFunc)
    {
        KYBER_LOG(LogLevel::Error, "Failed to subclass window");
    }

    g_renderer = this;
}

Dx12Renderer::~Dx12Renderer()
{
    if (hWnd && lpPrevWndFunc)
    {
        SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(lpPrevWndFunc));
        lpPrevWndFunc = nullptr;
    }

    ShutdownDeviceObjects();
    g_renderer = nullptr;
}

bool Dx12Renderer::InitializeImGui(IDXGISwapChain3* swapChain)
{
    // Most of this was taken directly from https://github.com/ocornut/imgui, the win32_directx12 example

    DXGI_SWAP_CHAIN_DESC swapDesc{};
    swapChain->GetDesc(&swapDesc);
    m_bufferCount = swapDesc.BufferCount;
    m_frameContexts.resize(m_bufferCount);

    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = m_bufferCount;
    if (FAILED(m_currentDevice->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_rtvDescHeap))))
    {
        KYBER_LOG(LogLevel::Error, "Failed to create descriptor heap");
        return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.NumDescriptors = kSrvHeapCapacity;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_currentDevice->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&m_srvDescHeap))))
    {
        KYBER_LOG(LogLevel::Error, "Failed to create descriptor heap");
        return false;
    }

    m_srvDescSize = m_currentDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_srvFreeIndices.resize(kSrvHeapCapacity);
    for (UINT i = 0; i < kSrvHeapCapacity; ++i)
    {
        m_srvFreeIndices[i] = i;
    }

    UINT rtvDescSize = m_currentDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < m_bufferCount; ++i)
    {
        if (FAILED(m_currentDevice->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_frameContexts[i].CommandAllocator))))
        {
            KYBER_LOG(LogLevel::Error, "Failed to create command allocator");
            return false;
        }

        m_frameContexts[i].RtvHandle = rtvHandle;
        rtvHandle.ptr += rtvDescSize;
    }

    if (FAILED(m_currentDevice->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_frameContexts[0].CommandAllocator, nullptr, IID_PPV_ARGS(&m_commandList))))
    {
        KYBER_LOG(LogLevel::Error, "Failed to create command list");
        return false;
    }
    m_commandList->Close();

    if (!CreateRenderTargets(swapChain))
    {
        return false;
    }

    ImGui_ImplWin32_Init(hWnd);

    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device = m_currentDevice;
    initInfo.CommandQueue = m_currentCommandQueue;
    initInfo.NumFramesInFlight = static_cast<int>(m_bufferCount);
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    initInfo.SrvDescriptorHeap = m_srvDescHeap;
    initInfo.UserData = this;
    initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
                                        D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle) {
        auto* self = static_cast<Dx12Renderer*>(info->UserData);
        IM_ASSERT(!self->m_srvFreeIndices.empty() && "Out of SRV descriptors - raise kSrvHeapCapacity");

        UINT index = self->m_srvFreeIndices.back();
        self->m_srvFreeIndices.pop_back();

        D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = self->m_srvDescHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = self->m_srvDescHeap->GetGPUDescriptorHandleForHeapStart();
        outCpuHandle->ptr = cpuStart.ptr + index * self->m_srvDescSize;
        outGpuHandle->ptr = gpuStart.ptr + index * self->m_srvDescSize;
    };
    initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
                                       D3D12_GPU_DESCRIPTOR_HANDLE /*gpuHandle*/) {
        auto* self = static_cast<Dx12Renderer*>(info->UserData);
        D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = self->m_srvDescHeap->GetCPUDescriptorHandleForHeapStart();
        UINT index = static_cast<UINT>((cpuHandle.ptr - cpuStart.ptr) / self->m_srvDescSize);
        self->m_srvFreeIndices.push_back(index);
    };

    ImGui_ImplDX12_Init(&initInfo);

    m_initialized = true;
    KYBER_LOG(LogLevel::Debug, "Renderer initialized");
    return true;
}

bool Dx12Renderer::CreateRenderTargets(IDXGISwapChain3* swapChain)
{
    for (UINT i = 0; i < m_bufferCount; ++i)
    {
        ID3D12Resource* backBuffer = nullptr;
        if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer))))
        {
            KYBER_LOG(LogLevel::Error, "Failed to get swapchain buffer");
            return false;
        }

        m_currentDevice->CreateRenderTargetView(backBuffer, nullptr, m_frameContexts[i].RtvHandle);
        m_frameContexts[i].BackBuffer = backBuffer;
    }
    return true;
}

void Dx12Renderer::ReleaseRenderTargets()
{
    for (auto& frame : m_frameContexts)
    {
        if (frame.BackBuffer)
        {
            frame.BackBuffer->Release();
            frame.BackBuffer = nullptr;
        }
    }
}

void Dx12Renderer::ShutdownDeviceObjects()
{
    if (!m_initialized)
    {
        return;
    }

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    ReleaseRenderTargets();

    for (auto& frame : m_frameContexts)
    {
        if (frame.CommandAllocator)
        {
            frame.CommandAllocator->Release();
        }
    }
    m_frameContexts.clear();

    if (m_commandList)
        m_commandList->Release();
    if (m_rtvDescHeap)
        m_rtvDescHeap->Release();
    if (m_srvDescHeap)
        m_srvDescHeap->Release();
    if (m_currentDevice)
        m_currentDevice->Release();

    m_initialized = false;
}

} // namespace Kyber
