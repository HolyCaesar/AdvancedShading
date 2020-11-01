#pragma once

#include "GraphicsCore.h"
#include "DX12RootSignature.h"
#include "DX12PipelineState.h"
#include "DX12ResStruct.h"
#include "GpuResource.h"
#include "GpuBuffer.h"
#include "Lights.h"

using namespace std;

class ForwardPlusLightCulling
{
public:
    ForwardPlusLightCulling() :
        m_TiledSize(16), m_BlockSizeX(1), m_BlockSizeY(1)
    {
		m_Lights = make_shared<StructuredBuffer>();
	}
    ~ForwardPlusLightCulling()
    {}

    void Init(uint32_t ScreenWidth, uint32_t ScreenHeight, XMMATRIX inverseProjection);
    void ResizeBuffers();
    void Destroy();
    void ExecuteCS(GraphicsContext& gfxContext, DepthBuffer& preDepthPass);

    void SetTiledSize(uint32_t tileSize) { m_TiledSize = tileSize; }
    void UpdateLightBuffer(shared_ptr<StructuredBuffer> pLightsBuffer);
    void UpdateConstantBuffer(XMMATRIX viewMatrix);

    // Get result
    ColorBuffer& GetOpaqueLightGrid() { return m_oLightGrid; }
    ColorBuffer& GetTransparentLightGrid() { return m_tLightGrid; }
    StructuredBuffer& GetOpaqueLightIndex() { return m_oLightIndexList; }
    StructuredBuffer& GetTransparentLightIndex() { return m_tLightIndexList; }
    shared_ptr<StructuredBuffer> GetLightBuffer() { return m_Lights; }
    
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
    DispatchParams m_dispatchParamsData;

    __declspec(align(16)) struct ScreenToViewParams
    {
        XMMATRIX InverseProjection;
        XMMATRIX ViewMatrix;
        XMUINT2 ScreenDimensions;
        XMUINT2 Padding;
    };
    ScreenToViewParams m_screenToViewParamsData;

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

    void LoadGridFrustumAsset(uint32_t ScreenWidth, uint32_t ScreenHeight, XMMATRIX inverseProjection);

    void UpdateGridFrustumCB();
    void ExecuteGridFrustumCS(GraphicsContext& gfxContext);

    // Light Culling
private:
    const uint32_t AVERAGE_OVERLAPPING_LIGHTS = 200;

    DX12RootSignature m_LightCullingComputeRootSignature;
    ComputePSO m_LightCullingComputePSO;

    // Light Culling Resource
    StructuredBuffer m_oLightIndexCounter;
    StructuredBuffer m_tLightIndexCounter;
    StructuredBuffer m_oLightIndexList;
    StructuredBuffer m_tLightIndexList;
    StructuredBuffer m_testUAVBuffer;

    ColorBuffer m_oLightGrid;
    ColorBuffer m_tLightGrid;
    ColorBuffer m_testUAVTex2D;

    shared_ptr<StructuredBuffer> m_Lights; // The light structures should be provided by other classes, not this one

    // Indexes for the root parameter table
    enum LightCullingRootParameters : uint32_t
    {
        e_LightCullingDispatchCB = 0,
        e_LightCullingScreenToViewRootCB,
        e_LightCullingOLightIndexCounterUAV,
        e_LightCullingTLightIndexCounterUAV,
        e_LightCullingOLightIndexListUAV,
        e_LightCullingTLightIndexListUAV,
        e_LightCullingOLightGridUAV,
        e_LightCullingTLightGridUAV,
        e_LightCullingFrustumSRV,
        e_LightCullingLightsSRV,
        e_LightCullingDepthSRV,
        e_LightCullingDebugUAV,
        e_LightCullingDebugUAVTex2D,
        e_LightCullingNumRootParameters
    };

    void LoadLightCullingAsset(uint32_t ScreenWidth, uint32_t ScreenHeight, XMMATRIX inverseProjection);

    void UpdateLightCullingCB();
    void ExecuteLightCullingCS(GraphicsContext& gfxContext, DepthBuffer& preDepthPass);
};
