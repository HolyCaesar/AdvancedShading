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
#include "Lights.h"
#include "Camera.h"
#include "Model.h"
#include "GpuBuffer.h"
#include "DX12RootSignature.h"
#include "DX12PipelineState.h"
#include "LightCullingPass.h"

// Experimental classes 
#include "SimpleComputeShader.h"

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
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ComPtr<ID3D12DescriptorHeap> m_cbvSrvHeap;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    UINT m_cbvSrvUavDescriptorSize;
    DX12RootSignature m_rootSignature;
    GraphicsPSO m_pipelineState;

    // App resources.
    StructuredBuffer m_vertexBuffer;
    StructuredBuffer m_indexBuffer;
    ComPtr<ID3D12Resource> m_texture;

    DepthBuffer m_depthBufferFinal;
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

    // Pre-Depth pass resources
    DepthBuffer m_preDepthPass;
    ColorBuffer m_preDepthPassRTV;
    DX12RootSignature m_preDepthPassRootSignature;
    GraphicsPSO m_preDepthPassPSO;
    
    // Grid FrustumsPass Calculation
    GridFrustumsPass m_GridFrustumsPass;

    // Light Culling Pass
    vector<Light> m_lightsList;
    LightCullingPass m_LightCullingPass;
    

    // Compute Shader Demo
    SimpleComputeShader m_simpleCS;


    void LoadPipeline();
    void LoadAssets();
    void LoadImGUI();
    void PopulateCommandList();
    std::vector<UINT8> GenerateTextureData(); // For test purpose

    void LoadPreDepthPassAssets();
    void PreDepthPass();

    // GenerateLights
    void GenerateLights(uint32_t numLights);
    // Update Lights Buffer
    void UpdateLightsBuffer();

    enum RootParameters : uint32_t
    {
        e_rootParameterCB = 0,
        e_rootParameterSRV,
        e_numRootParameters
    };
    // indexes of resources into the descriptor heap
    enum DescriptorHeapCount : uint32_t
    {
        e_cCB = 1,
        e_cSRV = 2,
    };
    enum DescriptorHeapIndex : uint32_t
    {
        e_iCB = 0,
        e_iSRV = e_iCB + e_cCB,
        e_iHeapEnd = e_cSRV + e_iSRV
    };
};
