#include "stdafx.h"
#include "LightCullingPass.h"
#include "Utility.h"
#include "DXSampleHelper.h"

void ForwardPlusLightCulling::Init(
	uint32_t ScreenWidth, uint32_t ScreenHeight,
	XMMATRIX inverseProjection)
{
	// Create the constant buffer for DispatchParams
	{
		// Update the dispatch parameter constant buffer
		m_dispatchParamsData.numThreadGroups = XMUINT3(1, 1, 1);
		m_dispatchParamsData.numThreads = XMUINT3(1, 1, 1);
		m_dispatchParamsData.blockSize = XMUINT2(1, 1);
		m_dispatchParamsData.padding1 = 1;
		m_dispatchParamsData.padding2 = 1;
		m_dispatchParamsData.padding3 = XMUINT2(1, 1);
	}

	// Create the constant buffer for ScreenToViewParams
	{
		// Update the screen to view parameter constant buffer
		m_screenToViewParamsData.InverseProjection = inverseProjection;
		m_screenToViewParamsData.ViewMatrix = XMMatrixIdentity();
		m_screenToViewParamsData.ScreenDimensions = XMUINT2(ScreenWidth, ScreenHeight);
	}

	// Load Grid Frustum Asset
	LoadGridFrustumAsset(ScreenWidth, ScreenHeight, inverseProjection);
	LoadLightCullingAsset(ScreenWidth, ScreenHeight, inverseProjection);
}

void ForwardPlusLightCulling::ResizeBuffers()
{
	m_CSGridFrustumOutputSB.Destroy();
	m_CSDebugUAV.Destroy();

	m_oLightIndexCounter.Destroy();
	m_tLightIndexCounter.Destroy();
	m_oLightIndexList.Destroy();
	m_tLightIndexList.Destroy();
	m_testUAVBuffer.Destroy();

	m_oLightGrid.Destroy();
	m_tLightGrid.Destroy();
	m_testUAVTex2D.Destroy();
}

void ForwardPlusLightCulling::Destroy()
{
	m_CSGridFrustumOutputSB.Destroy();
	m_CSDebugUAV.Destroy();

	m_oLightIndexCounter.Destroy();
	m_tLightIndexCounter.Destroy();
	m_oLightIndexList.Destroy();
	m_tLightIndexList.Destroy();
	m_testUAVBuffer.Destroy();

	m_oLightGrid.Destroy();
	m_tLightGrid.Destroy();
	m_testUAVTex2D.Destroy();
	
	m_Lights->Destroy();
}

void ForwardPlusLightCulling::UpdateConstantBuffer(XMMATRIX viewMatrix)
{
	m_screenToViewParamsData.ViewMatrix = viewMatrix;
}

void ForwardPlusLightCulling::ExecuteCS(GraphicsContext& gfxContext, DepthBuffer& preDepthPass)
{
	UpdateGridFrustumCB();
	ExecuteGridFrustumCS(gfxContext);
	UpdateLightCullingCB();
	ExecuteLightCullingCS(gfxContext, preDepthPass);
}

void ForwardPlusLightCulling::UpdateLightBuffer(vector<Light>& lightList)
{
	IGraphics::g_GraphicsCore->g_CommandManager->IdleGPU();
	m_Lights->Destroy();
	m_Lights->Create(L"LightLists", lightList.size(), sizeof(Light), lightList.data());
	//IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_Lights.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
}

/****************/
// Grid Frustum
/****************/
void ForwardPlusLightCulling::LoadGridFrustumAsset(
	uint32_t ScreenWidth, uint32_t ScreenHeight, 
	XMMATRIX inverseProjection)
{
	// Root Signature
	{
		m_GridFrustumRootSignature.Reset(e_GridFrustumNumRootParameters, 0);
		m_GridFrustumRootSignature[e_GridFrustumDispatchRootParameterCB].InitAsConstantBuffer(0);
		m_GridFrustumRootSignature[e_GridFrustumScreenToViewRootParameterCB].InitAsConstantBuffer(1);
		m_GridFrustumRootSignature[e_GridFrustumRootParameterUAV].InitAsBufferUAV(0);
		m_GridFrustumRootSignature[e_GridFrustumRootParameterDebugUAV].InitAsBufferUAV(1);
		m_GridFrustumRootSignature.Finalize(L"GridFrustumPassRootSignature");
	}

	// Create compute PSO
	{
		ComPtr<ID3DBlob> computeShader;
		ComPtr<ID3DBlob> error;

#if defined(_DEBUG)
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

		ComPtr<ID3DBlob> errorMessages;
		HRESULT hr = D3DCompileFromFile(L"GridFrustumPass.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CS_GridFrustumPass", "cs_5_1", compileFlags, 0, &computeShader, &errorMessages);
		if (FAILED(hr) && errorMessages)
		{
			const char* errorMsg = (const char*)errorMessages->GetBufferPointer();
			//MessageBox(nullptr, errorMsg, L"Shader Compilation Error", MB_RETRYCANCEL);
			wstring str;
			for (int i = 0; i < 3000; i++) str += errorMsg[i];
			MessageBox(nullptr, str.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
			exit(0);
		}
#else
		UINT compileFlags = 0;
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"GridFrustumPass.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CS_GridFrustumPass", "cs_5_1", compileFlags, 0, &pixelShader, nullptr));
#endif

		m_GridFrustumComputePSO.SetRootSignature(m_GridFrustumRootSignature);
		m_GridFrustumComputePSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(computeShader.Get()));
		m_GridFrustumComputePSO.Finalize();
	}

	// Grid result UAV
	m_BlockSizeX = ceil(ScreenWidth * 1.0f / m_TiledSize);
	m_BlockSizeY = ceil(ScreenHeight * 1.0f / m_TiledSize);
	m_CSGridFrustumOutputSB.Create(L"GridFrustumsPassOutputBuffer", m_BlockSizeX * m_BlockSizeY, sizeof(Frustum));

	// Debug Buffer
	m_CSDebugUAV.Create(L"DebugUAV", m_BlockSizeX * m_BlockSizeY, sizeof(float));

	// Update the dispatch parameter constant buffer
	m_dispatchParamsData.numThreadGroups = XMUINT3(ceil(1.0f * m_BlockSizeX / m_TiledSize), ceil(1.0f * m_BlockSizeY / m_TiledSize), 1);
	m_dispatchParamsData.numThreads = XMUINT3(m_BlockSizeX, m_BlockSizeY, 1);
	m_dispatchParamsData.blockSize = XMUINT2(m_TiledSize, m_TiledSize);
	m_dispatchParamsData.padding1 = 1;
	m_dispatchParamsData.padding2 = 1;
	m_dispatchParamsData.padding3 = XMUINT2(1, 1);

	// Update the screen to view parameter constant buffer
	m_screenToViewParamsData.InverseProjection = inverseProjection;
	m_screenToViewParamsData.ScreenDimensions = XMUINT2(ScreenWidth, ScreenHeight);
}

void ForwardPlusLightCulling::UpdateGridFrustumCB()
{
	m_dispatchParamsData.numThreadGroups = XMUINT3(ceil(1.0f * m_BlockSizeX / m_TiledSize), ceil(1.0f * m_BlockSizeY / m_TiledSize), 1);
	m_dispatchParamsData.numThreads = XMUINT3(m_BlockSizeX, m_BlockSizeY, 1);
	m_dispatchParamsData.blockSize = XMUINT2(m_TiledSize, m_TiledSize);
}

void ForwardPlusLightCulling::ExecuteGridFrustumCS(GraphicsContext& gfxContext)
{
	ComputeContext& computeContext = gfxContext.GetComputeContext();

	computeContext.SetRootSignature(m_GridFrustumRootSignature);
	computeContext.SetPipelineState(m_GridFrustumComputePSO);

	computeContext.SetDynamicConstantBufferView(e_GridFrustumDispatchRootParameterCB, sizeof(m_dispatchParamsData), &m_dispatchParamsData);
	computeContext.SetDynamicConstantBufferView(e_GridFrustumScreenToViewRootParameterCB, sizeof(m_screenToViewParamsData), &m_screenToViewParamsData);

	computeContext.TransitionResource(m_CSGridFrustumOutputSB, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	computeContext.TransitionResource(m_CSDebugUAV, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

	computeContext.SetBufferUAV(e_GridFrustumRootParameterUAV, m_CSGridFrustumOutputSB);
	computeContext.SetBufferUAV(e_GridFrustumRootParameterDebugUAV, m_CSDebugUAV);

	computeContext.Dispatch(m_dispatchParamsData.numThreadGroups.x, m_dispatchParamsData.numThreadGroups.y, 1);

	computeContext.TransitionResource(m_CSGridFrustumOutputSB, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	computeContext.TransitionResource(m_CSDebugUAV, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}


/****************/
// Light Culling 
/****************/
void ForwardPlusLightCulling::LoadLightCullingAsset(
	uint32_t ScreenWidth, uint32_t ScreenHeight,
	XMMATRIX inverseProjection)
{
	m_LightCullingComputeRootSignature.Reset(e_LightCullingNumRootParameters, 0);
	m_LightCullingComputeRootSignature[e_LightCullingDispatchCB].InitAsConstantBuffer(0);
	m_LightCullingComputeRootSignature[e_LightCullingScreenToViewRootCB].InitAsConstantBuffer(1);
	m_LightCullingComputeRootSignature[e_LightCullingOLightIndexCounterUAV].InitAsBufferUAV(0);
	m_LightCullingComputeRootSignature[e_LightCullingTLightIndexCounterUAV].InitAsBufferUAV(1);
	m_LightCullingComputeRootSignature[e_LightCullingOLightIndexListUAV].InitAsBufferUAV(2);
	m_LightCullingComputeRootSignature[e_LightCullingTLightIndexListUAV].InitAsBufferUAV(3);
	m_LightCullingComputeRootSignature[e_LightCullingOLightGridUAV].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 1);
	m_LightCullingComputeRootSignature[e_LightCullingTLightGridUAV].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5, 1);
	m_LightCullingComputeRootSignature[e_LightCullingFrustumSRV].InitAsBufferSRV(0);
	m_LightCullingComputeRootSignature[e_LightCullingLightsSRV].InitAsBufferSRV(1);
	m_LightCullingComputeRootSignature[e_LightCullingDepthSRV].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);
	m_LightCullingComputeRootSignature[e_LightCullingDebugUAV].InitAsBufferUAV(6);
	m_LightCullingComputeRootSignature[e_LightCullingDebugUAVTex2D].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 7, 1);

	m_LightCullingComputeRootSignature.Finalize(L"LightCullingPassRootSignature");

	// Create compute PSO
	ComPtr<ID3D12Resource> csInputUploadHeap;
	{
		ComPtr<ID3DBlob> computeShader;
		ComPtr<ID3DBlob> error;

#if defined(_DEBUG)
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

		vector<D3D_SHADER_MACRO> vShaderMacro;
		string name("BLOCK_SIZE"), definition(to_string(m_TiledSize));
		char* c_name = new char[name.size() + 1];
		char* c_definition = new char[definition.size() + 1];
		strncpy_s(c_name, name.size() + 1, name.c_str(), name.size());
		strncpy_s(c_definition, definition.size() + 1, definition.c_str(), definition.size());
		vShaderMacro.push_back({ c_name, c_definition });
		vShaderMacro.push_back({ 0, 0 });

		ComPtr<ID3DBlob> errorMessages;
		HRESULT hr = D3DCompileFromFile(L"LightCullingPass.hlsl", vShaderMacro.data(), D3D_COMPILE_STANDARD_FILE_INCLUDE, "CS_LightCullingPass", "cs_5_1", compileFlags, 0, &computeShader, &errorMessages);
		if (FAILED(hr) && errorMessages)
		{
			const char* errorMsg = (const char*)errorMessages->GetBufferPointer();
			//MessageBox(nullptr, errorMsg, L"Shader Compilation Error", MB_RETRYCANCEL);
			wstring str;
			for (int i = 0; i < 3000; i++) str += errorMsg[i];
			MessageBox(nullptr, str.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
			exit(0);
		}
#else
		UINT compileFlags = 0;
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"LightCullingPass.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CS_LightCullingPass", "cs_5_1", compileFlags, 0, &pixelShader, nullptr));
#endif

		m_LightCullingComputePSO.SetRootSignature(m_LightCullingComputeRootSignature);
		m_LightCullingComputePSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(computeShader.Get()));
		m_LightCullingComputePSO.Finalize();
	}

	// Prepare for Output of Grid
	m_BlockSizeX = ceil(ScreenWidth * 1.0f / m_TiledSize);
	m_BlockSizeY = ceil(ScreenHeight * 1.0f / m_TiledSize);
	vector<UINT> indexCounter(1, 1);
	// Opaque LightIndexCounter Structured Buffer
	m_oLightIndexCounter.Create(L"O_LightIndexCounter", 1, sizeof(UINT), indexCounter.data());
	// Transparent LightIndexCounter Structured Buffer
	m_tLightIndexCounter.Create(L"T_LightIndexCounter", 1, sizeof(UINT), indexCounter.data());
	// Opaque LightIndexList
	m_oLightIndexList.Create(L"O_LightIndexList", m_BlockSizeX * m_BlockSizeY * AVERAGE_OVERLAPPING_LIGHTS, sizeof(UINT));
	// Transparent LightIndexList
	m_tLightIndexList.Create(L"T_LightIndexList", m_BlockSizeX * m_BlockSizeY * AVERAGE_OVERLAPPING_LIGHTS, sizeof(UINT));
	// Test Debug buffer
	m_testUAVBuffer.Create(L"DebuggerBuffer", m_BlockSizeX * m_BlockSizeY, sizeof(XMFLOAT4));

	// Opaque LightGrid
	m_oLightGrid.Create(L"OpaqueLightGridMap", m_BlockSizeX, m_BlockSizeY, 1, DXGI_FORMAT_R32G32_UINT);
	m_tLightGrid.Create(L"TransparentLightGridMap", m_BlockSizeX, m_BlockSizeY, 1, DXGI_FORMAT_R32G32_UINT);
	m_testUAVTex2D.Create(L"TestMap", ScreenWidth, ScreenHeight, 1, DXGI_FORMAT_R32G32_FLOAT);

}

void ForwardPlusLightCulling::UpdateLightCullingCB()
{
	//m_dispatchParamsData.numThreadGroups = XMUINT3(ceil(1.0f * m_BlockSizeX / m_TiledSize), ceil(1.0f * m_BlockSizeY / m_TiledSize), 1);
	m_dispatchParamsData.numThreadGroups = XMUINT3(m_BlockSizeX, m_BlockSizeY, 1);
	m_dispatchParamsData.numThreads = XMUINT3(m_BlockSizeX, m_BlockSizeY, 1);
	m_dispatchParamsData.blockSize = XMUINT2(m_TiledSize, m_TiledSize);
	m_dispatchParamsData.padding1 = 1;
	m_dispatchParamsData.padding2 = 1;
	m_dispatchParamsData.padding3 = XMUINT2(1, 1);
}

void ForwardPlusLightCulling::ExecuteLightCullingCS(GraphicsContext& gfxContext, DepthBuffer& preDepthPass)
{
	ComputeContext& computeContext = gfxContext.GetComputeContext();

	computeContext.SetRootSignature(m_LightCullingComputeRootSignature);
	computeContext.SetPipelineState(m_LightCullingComputePSO);

	computeContext.SetDynamicConstantBufferView(e_GridFrustumDispatchRootParameterCB, sizeof(m_dispatchParamsData), &m_dispatchParamsData);
	computeContext.SetDynamicConstantBufferView(e_GridFrustumScreenToViewRootParameterCB, sizeof(m_screenToViewParamsData), &m_screenToViewParamsData);

	computeContext.TransitionResource(m_oLightIndexCounter, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	computeContext.TransitionResource(m_tLightIndexCounter, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	computeContext.TransitionResource(m_oLightIndexList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	computeContext.TransitionResource(m_tLightIndexList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	computeContext.TransitionResource(m_testUAVBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	computeContext.TransitionResource(m_oLightGrid, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	computeContext.TransitionResource(m_tLightGrid, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	computeContext.TransitionResource(*m_Lights, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	computeContext.TransitionResource(m_testUAVTex2D, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);


	computeContext.SetBufferUAV(e_LightCullingOLightIndexCounterUAV, m_oLightIndexCounter);
	computeContext.SetBufferUAV(e_LightCullingTLightIndexCounterUAV, m_tLightIndexCounter);
	computeContext.SetBufferUAV(e_LightCullingOLightIndexListUAV, m_oLightIndexList);
	computeContext.SetBufferUAV(e_LightCullingTLightIndexListUAV, m_tLightIndexList);
	computeContext.SetBufferUAV(e_LightCullingDebugUAV, m_testUAVBuffer);

	computeContext.SetDynamicDescriptor(e_LightCullingOLightGridUAV, 0, m_oLightGrid.GetUAV());
	computeContext.SetDynamicDescriptor(e_LightCullingTLightGridUAV, 0, m_tLightGrid.GetUAV());
	computeContext.SetDynamicDescriptor(e_LightCullingDebugUAVTex2D, 0, m_testUAVTex2D.GetUAV());

	computeContext.SetBufferSRV(e_LightCullingFrustumSRV, m_CSGridFrustumOutputSB);
	computeContext.SetBufferSRV(e_LightCullingLightsSRV, *m_Lights);
	computeContext.SetDynamicDescriptor(e_LightCullingDepthSRV, 0, preDepthPass.GetDepthSRV());


	// Reset the UAV counter for this frame.
	computeContext.ClearUAV(m_oLightGrid);
	computeContext.ClearUAV(m_tLightGrid);
	computeContext.ClearUAV(m_testUAVTex2D);

	computeContext.CopyBufferRegion(m_oLightIndexCounter, 0, m_oLightIndexCounter.GetCounterBuffer(), 0, sizeof(UINT));
	computeContext.CopyBufferRegion(m_tLightIndexCounter, 0, m_tLightIndexCounter.GetCounterBuffer(), 0, sizeof(UINT));

	computeContext.CopyBufferRegion(m_oLightIndexList, 0, m_oLightIndexList.GetCounterBuffer(), 0, sizeof(UINT));
	computeContext.CopyBufferRegion(m_tLightIndexList, 0, m_tLightIndexList.GetCounterBuffer(), 0, sizeof(UINT));

	computeContext.Dispatch(m_dispatchParamsData.numThreadGroups.x, m_dispatchParamsData.numThreadGroups.y, 1);

	//computeContext.TransitionResource(m_oLightIndexCounter, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	//computeContext.TransitionResource(m_tLightIndexCounter, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	computeContext.TransitionResource(m_oLightIndexList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	computeContext.TransitionResource(m_tLightIndexList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	//computeContext.TransitionResource(m_testUAVBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	computeContext.TransitionResource(m_oLightGrid, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	computeContext.TransitionResource(m_tLightGrid, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	computeContext.TransitionResource(m_testUAVTex2D, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
}
