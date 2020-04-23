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

    __declspec( align( 16 ) ) struct CBuffer 
    {
        XMMATRIX worldMatrix;
        XMMATRIX worldViewProjMatrix;
        //XMFLOAT4 offset;
    };

    // Pipeline objects.
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeapTex2D;
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;

    // App resources.
    StructuredBuffer m_vertexBuffer;
    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    ComPtr<ID3D12Resource> m_texture;

    ComPtr<ID3D12Resource> m_depthBuffer;
    ComPtr<ID3D12DescriptorHeap> m_DSVHeap;

    ComPtr<ID3D12Resource> m_constantBuffer;
    CBuffer m_constantBufferData;
    UINT8* m_pCbvDataBegin;

    // DXUT Model-View Camera
    CModelViewerCamera m_modelViewCamera;
    IMath::Camera m_perspectiveCamera;

    // Model
    shared_ptr<Model> m_pModel;

    // Compute Shader
    ComPtr<ID3D12RootSignature> m_computeRootSignature;
    ComPtr<ID3D12PipelineState> m_computePSO;
    ComPtr<ID3D12DescriptorHeap> m_cbvSrvUavHeap;
    UINT m_cbvSrvUavDescriptorSize;

    ComPtr<ID3D12CommandAllocator> m_computeCommandAllocator[SWAP_CHAIN_BUFFER_COUNT];
    ComPtr<ID3D12CommandQueue> m_computeCommandQueue;
    ComPtr<ID3D12GraphicsCommandList> m_computeCommandList;

    ComPtr<ID3D12Resource> m_computeInputTex2D;
    ComPtr<ID3D12Resource> m_computeInputStructureBuffer;
    ComPtr<ID3D12Resource> m_computeOutput;
    XMFLOAT4 m_csInputStructureBuffer;
    UINT8* m_pcsInputStructureBegin;

    ComPtr<ID3D12Fence> m_computeFence;
    uint64_t m_computeFenceValue;
    HANDLE m_computeFenceEvent;

    // Indexes for the root parameter table
    enum RootParameters : uint32_t
    {
        //e_rootParameterCB = 0,
        //e_rootParameterSampler,
        e_rootParameterUAV = 0,
        e_rootParameterSRV,
        e_numRootParameters
    };

    // indexes of resources into the descriptor heap
    enum DescriptorHeapCount : uint32_t
    {
        //e_cCB = 1,
        e_cUAV = 1,
        e_cSRV = 2,
    };
    enum DescriptorHeapIndex : uint32_t
    {
        //e_iCB = 0,
        //e_iUAV = e_iCB + e_cCB,
        //e_iSRV = e_iUAV + e_cUAV,
        //e_iHeapEnd = e_iSRV + e_cSRV
        e_iUAV = 0,
        e_iSRV = e_iUAV + e_cUAV,
        e_iHeapEnd = e_iSRV + e_cSRV
    };


    void LoadPipeline();
    void LoadAssets();
    void LoadImGUI();
    void PopulateCommandList();
    std::vector<UINT8> GenerateTextureData(); // For test purpose

    void LoadComputeShaderResources();
    void PopulateComputeCommandList();


    void WaitForComputeShader();
};
