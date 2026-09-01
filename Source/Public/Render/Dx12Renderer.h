// Copyright Nuuby. All Rights Reserved.

#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <Windows.h>
#include <vector>

namespace Kyber
{
class Dx12Renderer
{
public:
    Dx12Renderer();
    ~Dx12Renderer();

    ID3D12Device* m_currentDevice = nullptr;
    ID3D12CommandQueue* m_currentCommandQueue = nullptr;

    struct FrameContext
    {
        ID3D12CommandAllocator* CommandAllocator = nullptr;
        ID3D12Resource* BackBuffer = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle{};
    };

    ID3D12GraphicsCommandList* m_commandList = nullptr;
    ID3D12DescriptorHeap* m_rtvDescHeap = nullptr;
    ID3D12DescriptorHeap* m_srvDescHeap = nullptr;
    std::vector<FrameContext> m_frameContexts;
    UINT m_bufferCount = 0;
    bool m_initialized = false;

    UINT m_srvDescSize = 0;
    std::vector<UINT> m_srvFreeIndices;
    static constexpr UINT kSrvHeapCapacity = 64;

    bool InitializeImGui(IDXGISwapChain3* swapChain);
    bool CreateRenderTargets(IDXGISwapChain3* swapChain);
    void ReleaseRenderTargets();
    void ShutdownDeviceObjects();
};

extern Dx12Renderer* g_renderer;

} // namespace Kyber
