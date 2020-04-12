#pragma once

#include "DXUT.h"
#include "DXUTmisc.h"
#include "DXUTCamera.h"
#include "DXUTSettingsDlg.h"
#include "Win32FrameWork.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#include "GraphicsCore.h"
#include "Camera.h"
#include "Model.h"
#include "GpuBuffer.h"

#define DX12_ENABLE_DEBUG_LAYER

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif


using namespace DirectX;
using Microsoft::WRL::ComPtr;

class TiledRendering : public Win32FrameWork
{
public:
    TiledRendering(UINT width, UINT height, std::wstring name);

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();

    virtual void WinMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    void ShowImGUI();

private:
    static const UINT FrameCount = 3;

    static const UINT TextureWidth = 256;
    static const UINT TextureHeight = 256;
    static const UINT TexturePixelSize = 4;    // The number of bytes used to represent a pixel in the texture.

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT3 color;
        XMFLOAT2 uv;
    };

    struct CBuffer 
    {
        XMMATRIX worldMatrix;
        XMMATRIX worldViewProjMatrix;
        //XMFLOAT4 offset;
    };

    // Pipeline objects.
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    //ComPtr<IDXGISwapChain3> m_swapChain;
    //ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[SWAP_CHAIN_BUFFER_COUNT];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator[SWAP_CHAIN_BUFFER_COUNT];
    //ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeapTex2D;
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    UINT m_rtvDescriptorSize;

    // App resources.
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    ComPtr<ID3D12Resource> m_texture;

    ComPtr<ID3D12Resource> m_depthBuffer;
    ComPtr<ID3D12DescriptorHeap> m_DSVHeap;

    ComPtr<ID3D12Resource> m_constantBuffer;
    CBuffer m_constantBufferData;
    UINT8* m_pCbvDataBegin;


    // Synchronization objects.
    //UINT m_frameIndex;
    //HANDLE m_fenceEvent;
    //ComPtr<ID3D12Fence> m_fence;
    //UINT64 m_fenceValue[SWAP_CHAIN_BUFFER_COUNT];

    // DXUT Model-View Camera
    CModelViewerCamera m_modelViewCamera;
    IMath::Camera m_perspectiveCamera;

    // Model
    shared_ptr<Model> m_pModel;
    StructuredBuffer m_test;

    void LoadPipeline();
    void LoadAssets();
    void LoadImGUI();
    void PopulateCommandList();
    std::vector<UINT8> GenerateTextureData(); // For test purpose
    //void WaitForPreviousFrame();

    //void WaitForGpu();
    //void MoveToNextFrame();

};
