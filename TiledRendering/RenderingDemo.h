#pragma once

#include "DXUT.h"
#include "DXUTmisc.h"
#include "DXUTCamera.h"
#include "DXUTSettingsDlg.h"
#include "Win32FrameWork.h"

#include "GraphicsCore.h"
#include "Lights.h"
#include "Camera.h"
#include "Model.h"
#include "GpuBuffer.h"
#include "DX12RootSignature.h"
#include "DX12PipelineState.h"
#include "LightCullingPass.h"
#include "DX12ResStruct.h"
#include "GUICore.h"

#include "CPUProfiler.h"
#include "GpuProfiler.h"

#include "RenderTechnique.h"

// Experimental classes 
#include "SimpleComputeShader.h"

#define DX12_ENABLE_DEBUG_LAYER

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif



using namespace DirectX;
using Microsoft::WRL::ComPtr;

class RenderingDemo : public Win32FrameWork
{
public:
	enum RenderTechniqueOption
	{
		GeneralRenderingOption = 0,
		TiledForwardRenderingOption,
		DefferredRenderingOption
	};
public:
	RenderingDemo(UINT width, UINT height, std::wstring name);

	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnResize(uint64_t width, uint64_t height);
	virtual void OnDestroy();

	// GenerateLights
	void GenerateLights(uint32_t numLights,
		XMFLOAT3 minPoint = XMFLOAT3(-10.0f, -10.0f, -10.0f),
		XMFLOAT3 maxPoint = XMFLOAT3(10.0f, 10.0f, 10.0f),
		float minLightRange = LIGHT_RANGE_MIN, float maxLightRange = LIGHT_RANGE_MAX,
		float minSpotLightAngle = LIGHT_SPOT_ANGLE_MIN,
		float maxSpotLightAngle = LIGHT_SPOT_ANGLE_MAX);

	virtual void WinMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	// Profiling
	CPUProfiler         m_cpuProfiler;
	GpuProfiler         m_gpuProfiler;

	// Rendering Technique Option
	RenderTechniqueOption m_renderingOption;
private:
	static const UINT   TextureWidth = 256;
	static const UINT   TextureHeight = 256;
	static const UINT   TexturePixelSize = 4;    // The number of bytes used to represent a pixel in the texture.

	// Pipeline objects.
	CD3DX12_VIEWPORT		m_viewport;
	CD3DX12_RECT					m_scissorRect;

	DepthBuffer						m_sceneDepthBuffer;

	// DXUT Model-View Camera
	CModelViewerCamera  m_modelViewCamera;
	IMath::Camera       m_perspectiveCamera;

	// Lights
	shared_ptr<StructuredBuffer> m_Lights;

private:
	DX12RootSignature   m_sceneOpaqueRootSignature;
	DX12RootSignature   m_sceneTransparentRootSignature;
	GraphicsPSO				m_scenePSO;

	// Scene 
	enum SceneRootParameters : uint32_t
	{
		e_rootParameterCB = 0,
		e_ModelTexRootParameterSRV,
		e_LightGridRootParameterSRV,
		e_LightIndexRootParameterSRV,
		e_LightBufferRootParameterSRV,
		e_numRootParameters
	};
	// indexes of resources into the descriptor heap
	enum SceneDescriptorHeapCount : uint32_t
	{
		e_cCB = 1,
		e_cSRV = 1,
		e_cUAV = 0,
	};
	enum DescriptorHeapIndex : uint32_t
	{
		e_iCB = 0,
		e_iSRV = e_iCB + e_cCB,
		e_iUAV = e_iSRV + e_cSRV,
		e_iHeapEnd = e_cUAV + e_iUAV
	};

private:
	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT3 color;
		XMFLOAT2 uv;
	};

	__declspec(align(16)) struct CBuffer
	{
		XMMATRIX worldMatrix;
		XMMATRIX viewMatrix;
		XMMATRIX worldViewProjMatrix;
		//XMFLOAT4 offset;
	};

	__declspec(align(16)) struct CbufferLight
	{
		uint32_t lightNum;
		uint32_t paddings[3];
	};

	// App resources.
	shared_ptr<StructuredBuffer>    m_vertexBuffer;
	shared_ptr<StructuredBuffer>    m_indexBuffer;
	ColorBuffer				m_modelTexture;

	CBuffer						m_constantBufferData;
	CbufferLight				m_lightConstantBuffer;

	// Model
	shared_ptr<Model>   m_pModel;

private:
	// Light Culling Pass
	vector<Light>       m_lightsList;

private:
	void LoadPipeline();
	void LoadAssets();
	std::vector<UINT8> GenerateTextureData(); // For test purpose

	// Update Lights Buffer
	void UpdateLightsBuffer();

	//
	// General Shading Technique resources
	//
private:
	void LoadGeneralShadingTech(string name);

	GeneralRendering	m_generalRenderingTech;

	//
	// Deferred Shading Technique resources
	//
private:
	void LoadDefferredShadingTech(string name);

	DeferredRendering m_deferredRenderingTech;

	//
	// Deferred Shading Technique resources
	//
private:
	void LoadTiledForwardShadingTech(string name);

	TiledForwardRendering m_tiledForwardRenderingTech;

	__declspec(align(16)) struct DispatchParams
	{
		XMUINT3 numThreadGroups;  // Number of groups dispatched
		UINT padding1;
		XMUINT3 numThreads;       // Totla number of threads dispatched
		UINT padding2;
		XMUINT2 blockSize;        // threads in x and y dimension of a thread group
		XMUINT2 padding3;
	};
	DispatchParams m_dispatchParamsDataFrustum;
	DispatchParams m_dispatchParamsDataLightCulling;

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

	const uint32_t AVERAGE_OVERLAPPING_LIGHTS = 200;

	uint32_t m_TiledSize;
	uint32_t m_BlockSizeX;
	uint32_t m_BlockSizeY;
};

