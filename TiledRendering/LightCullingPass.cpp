#include "stdafx.h"
#include "LightCullingPass.h"
#include "Utility.h"
#include "DXSampleHelper.h"

void ForwardPlusLightCulling::Init(
	uint32_t ScreenWidth, uint32_t ScreenHeight,
	XMMATRIX inverseProjection,
	ComPtr<ID3D12DescriptorHeap> gCbvSrvUavDescriptorHeap,
	UINT& gCbvSrvUavOffset)
{
	// Create the constant buffer for DispatchParams
	{
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_dispatchParamsCB.pResource)));

		// Describe and create a constant buffer view.
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_dispatchParamsCB.pResource->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(DispatchParams) + 255) & ~255;    // CB size is required to be 256-byte aligned.
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(gCbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), gCbvSrvUavOffset, 32);
		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateConstantBufferView(&cbvDesc, cbvHandle);

		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_dispatchParamsCB.pResource->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDispatchParams)));
		// Update the dispatch parameter constant buffer
		m_dispatchParamsData.numThreadGroups = XMUINT3(1, 1, 1);
		m_dispatchParamsData.numThreads = XMUINT3(1, 1, 1);
		m_dispatchParamsData.blockSize = XMUINT2(1, 1);
		m_dispatchParamsData.padding1 = 1;
		m_dispatchParamsData.padding2 = 1;
		m_dispatchParamsData.padding3 = XMUINT2(1, 1);
		memcpy(m_pCbvDispatchParams, &m_dispatchParamsData, sizeof(m_dispatchParamsData));

		m_dispatchParamsCB.mUsageState = D3D12_RESOURCE_STATE_GENERIC_READ;
		m_dispatchParamsCB.uCbvDescriptorOffset = gCbvSrvUavOffset;
		++gCbvSrvUavOffset;
	}

	// Create the constant buffer for ScreenToViewParams
	{
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_screenToViewParamsCB.pResource)));

		// Describe and create a constant buffer view.
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_screenToViewParamsCB.pResource->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(ScreenToViewParams) + 255) & ~255;    // CB size is required to be 256-byte aligned.
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(gCbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), gCbvSrvUavOffset, 32);
		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateConstantBufferView(&cbvDesc, cbvHandle);

		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_screenToViewParamsCB.pResource->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvScreenToViewParams)));
		// Update the screen to view parameter constant buffer
		m_screenToViewParamsData.InverseProjection = inverseProjection;
		m_screenToViewParamsData.ViewMatrix = XMMatrixIdentity();
		m_screenToViewParamsData.ScreenDimensions = XMUINT2(ScreenWidth, ScreenHeight);
		memcpy(m_pCbvScreenToViewParams, &m_screenToViewParamsData, sizeof(m_screenToViewParamsData));

		m_screenToViewParamsCB.mUsageState = D3D12_RESOURCE_STATE_GENERIC_READ;
		m_screenToViewParamsCB.uCbvDescriptorOffset = gCbvSrvUavOffset;
		++gCbvSrvUavOffset;
	}

	// Load Grid Frustum Asset
	LoadGridFrustumAsset(ScreenWidth, ScreenHeight, inverseProjection, gCbvSrvUavDescriptorHeap, gCbvSrvUavOffset);
	LoadLightCullingAsset(ScreenWidth, ScreenHeight, inverseProjection, gCbvSrvUavDescriptorHeap, gCbvSrvUavOffset);
}

void ForwardPlusLightCulling::Resize()
{

}

void ForwardPlusLightCulling::Destroy()
{

}

void ForwardPlusLightCulling::UpdateConstantBuffer(XMMATRIX viewMatrix)
{
	m_screenToViewParamsData.ViewMatrix = viewMatrix;
}

void ForwardPlusLightCulling::ExecuteCS(ComPtr<ID3D12DescriptorHeap> gCbvSrvuavDescriptorHeap, UINT preDepthPassHeapOffset)
{
	UpdateGridFrustumCB();
	ExecuteGridFrustumCS(gCbvSrvuavDescriptorHeap);
	UpdateLightCullingCB();
	ExecuteLightCullingCS(gCbvSrvuavDescriptorHeap, preDepthPassHeapOffset);
}

void ForwardPlusLightCulling::CreateGPUTex2DUAVResource(
	wstring name, uint32_t width, uint32_t height,
	uint32_t elementSize, DXGI_FORMAT format,
	ComPtr<ID3D12DescriptorHeap> gCbvSrvUavDescriptorHeap,
	UINT& heapOffset, DX12Resource& pResource, void* initData)
{
	D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&pResource.pResource)));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
		gCbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		heapOffset,
		32);
	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(pResource.pResource.Get(), &srvDesc, srvHandle);
	pResource.uSrvDescriptorOffset = heapOffset;
	++heapOffset;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = width * height;
	uavDesc.Buffer.StructureByteStride = elementSize;
	uavDesc.Buffer.CounterOffsetInBytes = width * height * elementSize;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(
		gCbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 
		heapOffset, 
		32);
	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateUnorderedAccessView(pResource.pResource.Get(), nullptr, nullptr, uavHandle);
	pResource.mUsageState = D3D12_RESOURCE_STATE_COMMON;
	pResource.uUavDescriptorOffset = heapOffset;
	++heapOffset;

	pResource.pResource->SetName(name.c_str());

	//// Create Counter Buffer
	//ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
	//	&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
	//	D3D12_HEAP_FLAG_NONE,
	//	&resourceDesc,
	//	D3D12_RESOURCE_STATE_GENERIC_READ,
	//	nullptr,
	//	IID_PPV_ARGS(&pResource.pResourceUAVCounter)));

	//UINT8* pMappedCounterReset = nullptr;
	//CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
	//ThrowIfFailed(pResource.pResourceUAVCounter->Map(0, &readRange, reinterpret_cast<void**>(&pMappedCounterReset)));
	//ZeroMemory(pMappedCounterReset, sizeof(XMUINT2));
	//pResource.pResourceUAVCounter->Unmap(0, nullptr);
}

void ForwardPlusLightCulling::UpdateLightBuffer(vector<Light>& lightList)
{
	m_Lights.Destroy();
	m_Lights.Create(L"LightLists", lightList.size(), sizeof(Light), lightList.data());
	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_Lights.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
}

/****************/
// Grid Frustum
/****************/
void ForwardPlusLightCulling::LoadGridFrustumAsset(
	uint32_t ScreenWidth, uint32_t ScreenHeight, 
	XMMATRIX inverseProjection,
	ComPtr<ID3D12DescriptorHeap> gDescriptorHeap,
	UINT& gCbvSrvUavOffset)
{
	// Root Signature
	{
		m_GridFrustumRootSignature.Reset(e_GridFrustumNumRootParameters, 0);
		m_GridFrustumRootSignature[e_GridFrustumDispatchRootParameterCB].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1);
		m_GridFrustumRootSignature[e_GridFrustumScreenToViewRootParameterCB].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
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

	ThrowIfFailed(IGraphics::g_GraphicsCore->m_computeCommandList->Reset(IGraphics::g_GraphicsCore->m_computeCommandAllocator.Get(), m_GridFrustumComputePSO.GetPSO()));

	// Grid result UAV
	m_BlockSizeX = ceil(ScreenWidth * 1.0f / m_TiledSize);
	m_BlockSizeY = ceil(ScreenHeight * 1.0f / m_TiledSize);
	m_CSGridFrustumOutputSB.Create(L"GridFrustumsPassOutputBuffer", m_BlockSizeX * m_BlockSizeY, sizeof(Frustum));
	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_CSGridFrustumOutputSB.GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	// Debug Buffer
	m_CSDebugUAV.Create(L"DebugUAV", m_BlockSizeX * m_BlockSizeY, sizeof(float));
	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_CSDebugUAV.GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	// Update the dispatch parameter constant buffer
	m_dispatchParamsData.numThreadGroups = XMUINT3(ceil(1.0f * m_BlockSizeX / m_TiledSize), ceil(1.0f * m_BlockSizeY / m_TiledSize), 1);
	m_dispatchParamsData.numThreads = XMUINT3(m_BlockSizeX, m_BlockSizeY, 1);
	m_dispatchParamsData.blockSize = XMUINT2(m_TiledSize, m_TiledSize);
	m_dispatchParamsData.padding1 = 1;
	m_dispatchParamsData.padding2 = 1;
	m_dispatchParamsData.padding3 = XMUINT2(1, 1);
	memcpy(m_pCbvDispatchParams, &m_dispatchParamsData, sizeof(m_dispatchParamsData));

	// Update the screen to view parameter constant buffer
	m_screenToViewParamsData.InverseProjection = inverseProjection;
	m_screenToViewParamsData.ScreenDimensions = XMUINT2(ScreenWidth, ScreenHeight);
	memcpy(m_pCbvScreenToViewParams, &m_screenToViewParamsData, sizeof(m_screenToViewParamsData));

	ThrowIfFailed(IGraphics::g_GraphicsCore->m_computeCommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { IGraphics::g_GraphicsCore->m_computeCommandList.Get() };
	IGraphics::g_GraphicsCore->m_computeCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	IGraphics::g_GraphicsCore->WaitForComputeShaderGpu();
}

void ForwardPlusLightCulling::UpdateGridFrustumCB()
{
	m_dispatchParamsData.numThreadGroups = XMUINT3(ceil(1.0f * m_BlockSizeX / m_TiledSize), ceil(1.0f * m_BlockSizeY / m_TiledSize), 1);
	m_dispatchParamsData.numThreads = XMUINT3(m_BlockSizeX, m_BlockSizeY, 1);
	m_dispatchParamsData.blockSize = XMUINT2(m_TiledSize, m_TiledSize);
	memcpy(m_pCbvDispatchParams, &m_dispatchParamsData, sizeof(m_dispatchParamsData));
}

void ForwardPlusLightCulling::ExecuteGridFrustumCS(ComPtr<ID3D12DescriptorHeap> gCbvSrvUavDescriptorHeap)
{

	ThrowIfFailed(IGraphics::g_GraphicsCore->m_computeCommandList->Reset(IGraphics::g_GraphicsCore->m_computeCommandAllocator.Get(), m_GridFrustumComputePSO.GetPSO()));
	ComPtr<ID3D12GraphicsCommandList> m_computeCommandList = IGraphics::g_GraphicsCore->m_computeCommandList;

	m_computeCommandList->SetComputeRootSignature(m_GridFrustumRootSignature.GetSignature());
	D3D12_GPU_DESCRIPTOR_HANDLE cbvSrvUavHandle = gCbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	ID3D12DescriptorHeap* ppHeaps[] = { gCbvSrvUavDescriptorHeap.Get() };
	m_computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	m_computeCommandList->SetComputeRootDescriptorTable(
		e_GridFrustumDispatchRootParameterCB,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, m_dispatchParamsCB.uCbvDescriptorOffset, 32));
	m_computeCommandList->SetComputeRootDescriptorTable(
		e_GridFrustumScreenToViewRootParameterCB,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, m_screenToViewParamsCB.uCbvDescriptorOffset, 32));
	m_computeCommandList->SetComputeRootUnorderedAccessView(
		e_GridFrustumRootParameterUAV,
		m_CSGridFrustumOutputSB.GetGpuVirtualAddress());
	m_computeCommandList->SetComputeRootUnorderedAccessView(
		e_GridFrustumRootParameterDebugUAV,
		m_CSDebugUAV.GetGpuVirtualAddress());

	m_computeCommandList->Dispatch(m_dispatchParamsData.numThreadGroups.x, m_dispatchParamsData.numThreadGroups.y, 1);
	//m_computeCommandList->Dispatch(1, 1, 1);

	ThrowIfFailed(m_computeCommandList->Close());

	ID3D12CommandList* tmpList = m_computeCommandList.Get();
	IGraphics::g_GraphicsCore->m_computeCommandQueue->ExecuteCommandLists(1, &tmpList);

	IGraphics::g_GraphicsCore->WaitForComputeShaderGpu();
}


/****************/
// Light Culling 
/****************/
void ForwardPlusLightCulling::LoadLightCullingAsset(
	uint32_t ScreenWidth, uint32_t ScreenHeight,
	XMMATRIX inverseProjection,
	ComPtr<ID3D12DescriptorHeap> gDescriptorHeap,
	UINT& gCbvSrvUavOffset)
{
	m_LightCullingComputeRootSignature.Reset(e_LightCullingNumRootParameters, 0);
	m_LightCullingComputeRootSignature[e_LightCullingDispatchCB].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1);
	m_LightCullingComputeRootSignature[e_LightCullingScreenToViewRootCB].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
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

	ThrowIfFailed(IGraphics::g_GraphicsCore->m_computeCommandList->Reset(IGraphics::g_GraphicsCore->m_computeCommandAllocator.Get(), m_LightCullingComputePSO.GetPSO()));


	// Prepare for Output of Grid
	m_BlockSizeX = ceil(ScreenWidth * 1.0f / m_TiledSize);
	m_BlockSizeY = ceil(ScreenHeight * 1.0f / m_TiledSize);
	vector<UINT> indexCounter(1, 1);
	// Opaque LightIndexCounter Structured Buffer
	m_oLightIndexCounter.Create(L"O_LightIndexCounter", 1, sizeof(UINT), indexCounter.data());
	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_oLightIndexCounter.GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	// Transparent LightIndexCounter Structured Buffer
	m_tLightIndexCounter.Create(L"T_LightIndexCounter", 1, sizeof(UINT), indexCounter.data());
	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_tLightIndexCounter.GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	// Opaque LightIndexList
	m_oLightIndexList.Create(L"O_LightIndexList", m_BlockSizeX * m_BlockSizeY * AVERAGE_OVERLAPPING_LIGHTS, sizeof(UINT));
	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_oLightIndexList.GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	// Transparent LightIndexList
	m_tLightIndexList.Create(L"T_LightIndexList", m_BlockSizeX * m_BlockSizeY * AVERAGE_OVERLAPPING_LIGHTS, sizeof(UINT));
	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_tLightIndexList.GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	// Test Debug buffer
	m_testUAVBuffer.Create(L"DebuggerBuffer", m_BlockSizeX * m_BlockSizeY, sizeof(XMFLOAT4));
	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_testUAVBuffer.GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	// Opaque LightGrid
	CreateGPUTex2DUAVResource(
		L"OpageLightGridMap", m_BlockSizeX, m_BlockSizeY, 
		sizeof(XMFLOAT2), DXGI_FORMAT_R32G32_UINT, 
		gDescriptorHeap, gCbvSrvUavOffset, 
		m_oLightGrid, nullptr);
	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_oLightGrid.pResource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	m_oLightGrid.mUsageState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	// Transparent LightGrid
	CreateGPUTex2DUAVResource(
		L"TransparentLightGridMap", m_BlockSizeX, m_BlockSizeY, 
		sizeof(XMFLOAT2), DXGI_FORMAT_R32G32_UINT, 
		gDescriptorHeap, gCbvSrvUavOffset, 
		m_tLightGrid, nullptr);
	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_tLightGrid.pResource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	m_tLightGrid.mUsageState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	// Debug texture2D
	CreateGPUTex2DUAVResource(
		L"DebuggerTex2D", ScreenWidth, ScreenHeight, 
		sizeof(XMFLOAT2), DXGI_FORMAT_R32G32_FLOAT,
		gDescriptorHeap, gCbvSrvUavOffset, 
		m_testUAVTex2D, nullptr);
	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_testUAVTex2D.pResource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	m_testUAVTex2D.mUsageState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	ThrowIfFailed(IGraphics::g_GraphicsCore->m_computeCommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { IGraphics::g_GraphicsCore->m_computeCommandList.Get() };
	IGraphics::g_GraphicsCore->m_computeCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	IGraphics::g_GraphicsCore->WaitForComputeShaderGpu();
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
	memcpy(m_pCbvDispatchParams, &m_dispatchParamsData, sizeof(m_dispatchParamsData));
}

void ForwardPlusLightCulling::ExecuteLightCullingCS(ComPtr<ID3D12DescriptorHeap> gCbvSrvUavDescriptorHeap, UINT preDepthBufHeapOffset)
{

	ThrowIfFailed(IGraphics::g_GraphicsCore->m_computeCommandList->Reset(IGraphics::g_GraphicsCore->m_computeCommandAllocator.Get(), m_LightCullingComputePSO.GetPSO()));
	ComPtr<ID3D12GraphicsCommandList> m_computeCommandList = IGraphics::g_GraphicsCore->m_computeCommandList;

	m_computeCommandList->SetComputeRootSignature(m_LightCullingComputeRootSignature.GetSignature());
	D3D12_GPU_DESCRIPTOR_HANDLE cbvSrvUavHandle = gCbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	ID3D12DescriptorHeap* ppHeaps[] = { gCbvSrvUavDescriptorHeap.Get() };
	m_computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//m_computeCommandList->SetComputeRootConstantBufferView(e_rootParameterCB, m_computeHeap);
	m_computeCommandList->SetComputeRootDescriptorTable(
		e_LightCullingDispatchCB,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, m_dispatchParamsCB.uCbvDescriptorOffset, 32));
	m_computeCommandList->SetComputeRootDescriptorTable(
		e_LightCullingScreenToViewRootCB,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, m_screenToViewParamsCB.uCbvDescriptorOffset, 32));

	// OLightIndexCounterUAV
	m_computeCommandList->SetComputeRootUnorderedAccessView(
		e_LightCullingOLightIndexCounterUAV,
		m_oLightIndexCounter.GetGpuVirtualAddress());
	// TLightIndexCounterUAV
	m_computeCommandList->SetComputeRootUnorderedAccessView(
		e_LightCullingTLightIndexCounterUAV,
		m_tLightIndexCounter.GetGpuVirtualAddress());
	// OLightIndexList
	m_computeCommandList->SetComputeRootUnorderedAccessView(
		e_LightCullingOLightIndexListUAV,
		m_oLightIndexList.GetGpuVirtualAddress());
	// TLightIndexList
	m_computeCommandList->SetComputeRootUnorderedAccessView(
		e_LightCullingTLightIndexListUAV,
		m_tLightIndexList.GetGpuVirtualAddress());

	m_computeCommandList->SetComputeRootDescriptorTable(
		e_LightCullingOLightGridUAV,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, m_oLightGrid.uUavDescriptorOffset, 32));
	m_computeCommandList->SetComputeRootDescriptorTable(
		e_LightCullingTLightGridUAV,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, m_tLightGrid.uUavDescriptorOffset, 32));

	m_computeCommandList->SetComputeRootDescriptorTable(
		e_LightCullingDebugUAVTex2D,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, m_testUAVTex2D.uUavDescriptorOffset, 32));

	// Reset the UAV counter for this frame.
	m_computeCommandList->CopyBufferRegion(m_oLightIndexCounter.GetResource(), 0, m_oLightIndexCounter.GetCounterBuffer().GetResource(), 0, sizeof(UINT));
	m_computeCommandList->CopyBufferRegion(m_tLightIndexCounter.GetResource(), 0, m_tLightIndexCounter.GetCounterBuffer().GetResource(), 0, sizeof(UINT));

	m_computeCommandList->CopyBufferRegion(m_oLightIndexList.GetResource(), 0, m_oLightIndexList.GetCounterBuffer().GetResource(), 0, sizeof(UINT));
	m_computeCommandList->CopyBufferRegion(m_tLightIndexList.GetResource(), 0, m_tLightIndexList.GetCounterBuffer().GetResource(), 0, sizeof(UINT));

	//m_computeCommandList->CopyBufferRegion(m_oLightGrid.pResource.Get(), 0, m_oLightGrid.pResourceUAVCounter.Get(), 0, sizeof(UINT));
	//m_computeCommandList->CopyBufferRegion(m_tLightGrid.pResource.Get(), 0, m_tLightGrid.pResourceUAVCounter.Get(), 0, sizeof(UINT));


	// Frustum SRV
	m_computeCommandList->SetComputeRootShaderResourceView(
		e_LightCullingFrustumSRV,
		m_CSGridFrustumOutputSB.GetGpuVirtualAddress());
	// Lights SRV
	m_computeCommandList->SetComputeRootShaderResourceView(
		e_LightCullingLightsSRV,
		m_Lights.GetGpuVirtualAddress());
	// Depth SRV
	m_computeCommandList->SetComputeRootDescriptorTable(
		e_LightCullingDepthSRV,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, preDepthBufHeapOffset, 32));

	// Test Buffer
	m_computeCommandList->SetComputeRootUnorderedAccessView(
		e_LightCullingDebugUAV,
		m_testUAVBuffer.GetGpuVirtualAddress());

	//m_computeCommandList->Dispatch(m_dispatchParamsData.numThreadGroups.x, m_dispatchParamsData.numThreadGroups.y, 1);
	m_computeCommandList->Dispatch(80, 45, 1);
	//m_computeCommandList->Dispatch(1, 1, 1);

	ThrowIfFailed(m_computeCommandList->Close());

	ID3D12CommandList* tmpList = m_computeCommandList.Get();
	IGraphics::g_GraphicsCore->m_computeCommandQueue->ExecuteCommandLists(1, &tmpList);

	IGraphics::g_GraphicsCore->WaitForComputeShaderGpu();
}
