#pragma once

#include "GraphicsCore.h"
#include "DX12RootSignature.h"
#include "DX12PipelineState.h"
#include "GpuResource.h"
#include "GpuBuffer.h"

using namespace std;

class GridFrustumsPass
{
public:
    GridFrustumsPass(uint32_t TiledSize) :
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

private:
    uint32_t m_TiledSize;
    uint32_t m_BlockSizeX;
    uint32_t m_BlockSizeY;
    
    DX12RootSignature m_computeRootSignature;
    ComputePSO m_computePSO;

    ComPtr<ID3D12CommandAllocator> m_computeCommandAllocator[SWAP_CHAIN_BUFFER_COUNT];
    ComPtr<ID3D12CommandQueue> m_computeCommandQueue;
    ComPtr<ID3D12GraphicsCommandList> m_computeCommandList;

    // Grid Frustum Calculation Pass Resources
    //ComPtr<ID3D12Resource> m_GridFrustumInput;
    //ComPtr<ID3D12Resource> m_GridFrustumOutputSB;
    StructuredBuffer m_CSGridFrustumOutputSB;

    ComPtr<ID3D12DescriptorHeap> m_cbvUavSrvHeap;
    UINT m_cbvUavSrvDescriptorSize;
    __declspec( align( 16 ) ) struct DispatchParams
    {
        XMUINT3 numThreadGroups;  // Number of groups dispatched
        XMUINT3 numThreads;       // Totla number of threads dispatched
        XMUINT2 blockSize;        // 
    };
    ComPtr<ID3D12Resource> m_dispatchParamsCB;
    DispatchParams m_dispatchParamsData;
    UINT8* m_pCbvDispatchParams;

    __declspec( align( 16 ) ) struct ScreenToViewParams
    {
        XMMATRIX InverseProjection;
        XMUINT2 ScreenDimensions;
    };
    ComPtr<ID3D12Resource> m_screenToViewParamsCB;
    ScreenToViewParams m_screenToViewParamsData;
    UINT8* m_pCbvScreenToViewParams;


    ComPtr<ID3D12Fence> m_computeFence;
    uint64_t m_computeFenceValue;
    HANDLE m_computeFenceEvent;

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
        e_cUAV = 1,
        //e_cSRV = 0,
    };
    enum DescriptorHeapIndex : uint32_t
    {
        e_iCB = 0,
        //e_iUAV = e_iCB + e_cCB,
        //e_iSRV = e_iUAV + e_cUAV,
        //e_iHeapEnd = e_iSRV + e_cSRV
        e_iUAV = e_iCB + e_cCB,
        //e_iSRV = e_iUAV + e_cUAV,
        //e_iHeapEnd = e_iSRV + e_cSRV
        e_iHeapEnd = e_iUAV + e_cUAV
    };

    void WaitForComputeShader();
};

class LightCullingPass
{
public:
	LightCullingPass();
	~LightCullingPass();


private:
	DX12RootSignature m_computeRootSignature;
	ComputePSO m_computePSO;

	ComPtr<ID3D12CommandAllocator> m_computeCommandAllocator[SWAP_CHAIN_BUFFER_COUNT];
	ComPtr<ID3D12CommandQueue> m_computeCommandQueue;
	ComPtr<ID3D12GraphicsCommandList> m_computeCommandList;

	// Grid Frustum Calculation Pass Resources
	ComPtr<ID3D12Resource> m_GridFrustumInput;
	ComPtr<ID3D12Resource> m_GridFrustumOutputSB;


	ComPtr<ID3D12Resource> m_computeFence;
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

private:

};

