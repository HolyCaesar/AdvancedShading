#pragma once

#include "DXUT.h"
#include "DXUTmisc.h"
#include "DXUTCamera.h"
#include "DXUTSettingsDlg.h"
#include "Win32FrameWork.h"

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

class SimpleComputeShader
{
public:
	SimpleComputeShader();
	~SimpleComputeShader();

    void OnInit();
    void OnExecuteCS();
    void OnDestroy();

private:
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
    ComPtr<ID3D12Resource> m_computeOutputSB;
    UINT8* m_pcsInputStructureBegin;


    ComPtr<ID3D12DescriptorHeap> m_testHeap;
    ComPtr<ID3D12Resource> m_test;


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
        e_cUAV = 2,
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
    void LoadComputeShaderResources();
    void WaitForComputeShader();
};

