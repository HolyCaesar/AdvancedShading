#pragma once

#include "GraphicsCore.h"
#include "DX12RootSignature.h"
#include "DX12PipelineState.h"
#include "DX12ResStruct.h"
#include "GpuResource.h"
#include "GpuBuffer.h"
#include "Lights.h"

using namespace std;

class GridFrustumsPass
{
public:
    GridFrustumsPass(uint32_t TiledSize = 16) :
        m_TiledSize(TiledSize),
        m_BlockSizeX(1), m_BlockSizeY(1)
    {
    }
    ~GridFrustumsPass()
    {
        Destroy();
    }


    // TiledLength * TiledLength must be a multiply of 64
    void SetTiledDimension(uint32_t TiledSize) { m_TiledSize = TiledSize; }

    void Init(std::wstring ShaderFile, uint32_t ScreenWidth, uint32_t ScreenHeight, 
        XMMATRIX inverseProjection = XMMatrixIdentity());
    void ExecuteOnCS();
    void Destroy();

    // Grid Frustum Calculation Pass Resources
    StructuredBuffer m_CSGridFrustumOutputSB;
    StructuredBuffer m_CSDebugUAV;

    ComPtr<ID3D12Resource> m_computeInputTex2D;

private:
    uint32_t m_TiledSize;
    uint32_t m_BlockSizeX;
    uint32_t m_BlockSizeY;
    
    DX12RootSignature m_computeRootSignature;
    ComputePSO m_computePSO;

    ComPtr<ID3D12DescriptorHeap> m_cbvUavSrvHeap;
    UINT m_cbvUavSrvDescriptorSize;
    __declspec( align( 16 ) ) struct DispatchParams
    {
        XMUINT3 numThreadGroups;  // Number of groups dispatched
        UINT padding1;
        XMUINT3 numThreads;       // Totla number of threads dispatched
        UINT padding2;
        XMUINT2 blockSize;        // 
        XMUINT2 padding3;
    };
    ComPtr<ID3D12Resource> m_dispatchParamsCB;
    DispatchParams m_dispatchParamsData;
    UINT8* m_pCbvDispatchParams;

    __declspec( align( 16 ) ) struct ScreenToViewParams
    {
        XMMATRIX InverseProjection;
        XMUINT2 ScreenDimensions;
        XMUINT2 Padding;
    };
    ComPtr<ID3D12Resource> m_screenToViewParamsCB;
    ScreenToViewParams m_screenToViewParamsData;
    UINT8* m_pCbvScreenToViewParams;

    // Indexes for the root parameter table
    enum RootParameters : uint32_t
    {
        e_rootParameterCB = 0,
        //e_rootParameterSampler,
        e_rootParameterUAV,
        //e_rootParameterSRV,
        e_numRootParameters
    };
    // indexes of resources into the descriptor heap
    enum DescriptorHeapCount : uint32_t
    {
        e_cCB = 2,
        e_cUAV = 2,
        //e_cSRV = 0,
    };
    enum DescriptorHeapIndex : uint32_t
    {
        e_iCB = 0,
        e_iUAV = e_iCB + e_cCB,
        e_iHeapEnd = e_iUAV + e_cUAV
    };

    __declspec( align( 16 ) ) struct Frustum
    {
        XMFLOAT4 planes[4];   // left, right, top, bottom frustum planes.
    };

    //void WaitForComputeShader();
};

// Light Culling Pass
class LightCullingPass
{
public:
    LightCullingPass(uint32_t TiledSize = 16) :
        m_TiledSize(TiledSize),
        m_BlockSizeX(1), m_BlockSizeY(1)
    {}
    ~LightCullingPass()
    {
        Destroy();
    }

    void Init(std::wstring ShaderFile, uint32_t ScreenWidth, uint32_t ScreenHeight,
        XMMATRIX inverseProjection = XMMatrixIdentity());
    void UpdateLightBuffer(vector<Light>& lighList);
    void ExecuteOnCS(
        StructuredBuffer& FrustumIn, 
        ComPtr<ID3D12DescriptorHeap>& depthBufferHeap, 
        uint32_t depthBufferOffset
    );
    void Destroy();

private:
    uint32_t m_TiledSize;
    uint32_t m_BlockSizeX;
    uint32_t m_BlockSizeY;
    const uint32_t AVERAGE_OVERLAPPING_LIGHTS = 200;

	DX12RootSignature m_computeRootSignature;
	ComputePSO m_computePSO;

	// Light Culling Resource
    StructuredBuffer m_oLightIndexCounter;
    StructuredBuffer m_tLightIndexCounter;
    StructuredBuffer m_oLightIndexList;
    StructuredBuffer m_tLightIndexList;

    ComPtr<ID3D12DescriptorHeap> m_uavHeap;
    UINT m_uavDescriptorSize;
    ComPtr<ID3D12Resource> m_oLightGrid;
    ComPtr<ID3D12Resource> m_tLightGrid;
    
    //ColorBuffer m_oLightGrid;
    //ColorBuffer m_tLightGrid;

    StructuredBuffer m_Lights; // The light structures should be provided by other classes, not this one

    StructuredBuffer m_testUAVBuffer;

    ComPtr<ID3D12Resource> m_computeInputTex2D;




    ComPtr<ID3D12DescriptorHeap> m_cbvUavSrvHeap;
    UINT m_cbvUavSrvDescriptorSize;
    __declspec(align(16)) struct DispatchParams
    {
        XMUINT3 numThreadGroups;  // Number of groups dispatched
        UINT padding1;
        XMUINT3 numThreads;       // Totla number of threads dispatched
        UINT padding2;
        XMUINT2 blockSize;        // 
        XMUINT2 padding3;
    };
    ComPtr<ID3D12Resource> m_dispatchParamsCB;
    DispatchParams m_dispatchParamsData;
    UINT8* m_pCbvDispatchParams;

    __declspec(align(16)) struct ScreenToViewParams
    {
        XMMATRIX InverseProjection;
        XMUINT2 ScreenDimensions;
        XMUINT2 Padding;
    };
    ComPtr<ID3D12Resource> m_screenToViewParamsCB;
    ScreenToViewParams m_screenToViewParamsData;
    UINT8* m_pCbvScreenToViewParams;

    // Indexes for the root parameter table
    enum RootParameters : uint32_t
    {
        e_rootParameterCB = 0,
        //e_rootParameterSampler,
        e_rootParameterOLightIndexCounterUAV,
        e_rootParameterTLightIndexCounterUAV,
        e_rootParameterOLightIndexListUAV,
        e_rootParameterTLightIndexListUAV,
        e_rootParameterOLightGridUAV,
        e_rootParameterTLightGridUAV,
        e_rootParameterFrustumSRV,
        e_rootParameterLightsSRV,
        e_rootParameterDepthSRV,
        e_rootParameterDebugUAV,
        e_numRootParameters
    };
    // indexes of resources into the descriptor heap
    enum DescriptorHeapCount : uint32_t
    {
        e_cCB = 2,
        e_cUAV = 7,
        e_cSRV = 3,
    };
    enum DescriptorHeapIndex : uint32_t
    {
        e_iCB = 0,
        e_iUAV = e_iCB + e_cCB,
        //e_iSRV = e_iUAV + e_cUAV,
        //e_iHeapEnd = e_iSRV + e_cSRV
        //e_iUAV = 0,
        e_iSRV = e_iUAV + e_cUAV,
        e_iHeapEnd = e_iSRV + e_cSRV
    };

private:
    //void WaitForComputeShader();
    void CreateGPUTex2DSRVResource(
        wstring name, 
        uint32_t width, 
        uint32_t height,
        uint32_t elementSize, 
        DXGI_FORMAT format, 
        ComPtr<ID3D12Resource> pResource,
        uint32_t heapOffset,
        void* initData = nullptr
    );
    void CreateGPUTex2DUAVResource(
        wstring name,
        uint32_t width,
        uint32_t height,
        uint32_t elementSize,
        DXGI_FORMAT format,
        ComPtr<ID3D12Resource> pResource,
        uint32_t heapOffset,
        void* initData = nullptr
    );
};






class ForwardPlusLightCulling
{
public:
    ForwardPlusLightCulling() :
        m_TiledSize(16), m_BlockSizeX(1), m_BlockSizeY(1)
    {}
    ~ForwardPlusLightCulling()
    {}

    void Init(
        uint32_t ScreenWidth, uint32_t ScreenHeight,
        XMMATRIX inverseProjection,
        ComPtr<ID3D12DescriptorHeap> gCbvSrvUavDescriptorHeap,
        UINT& gCbvSrvUavOffset);
    void Resize();
    void Destroy();
    void ExecuteCS(ComPtr<ID3D12DescriptorHeap> gCbvSrvuavDescriptorHeap);

    void SetTiledSize(uint32_t tileSize) { m_TiledSize = tileSize; }
    // Common Resources
private:
    uint32_t m_TiledSize;
    uint32_t m_BlockSizeX;
    uint32_t m_BlockSizeY;

    __declspec(align(16)) struct DispatchParams
    {
        XMUINT3 numThreadGroups;  // Number of groups dispatched
        UINT padding1;
        XMUINT3 numThreads;       // Totla number of threads dispatched
        UINT padding2;
        XMUINT2 blockSize;        // threads in x and y dimension of a thread group
        XMUINT2 padding3;
    };
    DX12Resource m_dispatchParamsCB;
    DispatchParams m_dispatchParamsData;
    UINT8* m_pCbvDispatchParams;

    __declspec(align(16)) struct ScreenToViewParams
    {
        XMMATRIX InverseProjection;
        XMUINT2 ScreenDimensions;
        XMUINT2 Padding;
    };
    DX12Resource m_screenToViewParamsCB;
    ScreenToViewParams m_screenToViewParamsData;
    UINT8* m_pCbvScreenToViewParams;

    __declspec(align(16)) struct Frustum
    {
        XMFLOAT4 planes[4];   // left, right, top, bottom frustum planes.
    };

    // Frustum calculation
private:
    DX12RootSignature m_GridFrustumRootSignature;
    ComputePSO m_GridFrustumComputePSO;

    // Compute Shader Resource
    StructuredBuffer m_CSGridFrustumOutputSB;
    StructuredBuffer m_CSDebugUAV;

    // Indexes for the root parameter table
    enum GridFrustumRootParameters : uint32_t
    {
        e_GridFrustumDispatchRootParameterCB = 0,
        e_GridFrustumScreenToViewRootParameterCB,
        e_GridFrustumRootParameterUAV,
        e_GridFrustumRootParameterDebugUAV,
        e_GridFrustumNumRootParameters
    };
    enum GridFrustumDescriptorHeapCount : uint32_t
    {
        e_cGridFrustumCB = 2,
        e_cGridFrustumUAV = 2,
    };

    void LoadGridFrustumAsset(
        uint32_t ScreenWidth, uint32_t ScreenHeight, 
        XMMATRIX inverseProjection,
        ComPtr<ID3D12DescriptorHeap> gDescriptorHeap,
        UINT& gCbvSrvUavOffset);

    void ExecuteGridFrustumCS(ComPtr<ID3D12DescriptorHeap> gCbvSrvuavDescriptorHeap);

    // Light Culling
private:
};
