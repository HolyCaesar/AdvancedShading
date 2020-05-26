#include "stdafx.h"
#include "LightCullingPass.h"
#include "Utility.h"
#include "DXSampleHelper.h"

//*****************************************
// Definitions for the Light Culling Pass
//*****************************************
void LightCullingPass::Init(std::wstring ShaderFile, uint32_t ScreenWidth, uint32_t ScreenHeight,
	XMMATRIX inverseProjection)
{
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};

		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 2; // Two UAVs
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_uavHeap)) != S_OK);
		m_uavDescriptorSize = IGraphics::g_GraphicsCore->g_pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};

		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = e_cCB + 1;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_cbvUavSrvHeap)) != S_OK);
		m_cbvUavSrvDescriptorSize = IGraphics::g_GraphicsCore->g_pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	// Load test root signature
	D3D12_SAMPLER_DESC non_static_sampler;
	non_static_sampler.Filter = D3D12_FILTER_ANISOTROPIC;
	non_static_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	non_static_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	non_static_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	non_static_sampler.MipLODBias = 0.0f;
	non_static_sampler.MaxAnisotropy = 8;
	non_static_sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	non_static_sampler.BorderColor[0] = 1.0f;
	non_static_sampler.BorderColor[1] = 1.0f;
	non_static_sampler.BorderColor[2] = 1.0f;
	non_static_sampler.BorderColor[3] = 1.0f;
	non_static_sampler.MinLOD = 0.0f;
	non_static_sampler.MaxLOD = D3D12_FLOAT32_MAX;

	m_computeRootSignature.Reset(e_numRootParameters, 0);
	//m_computeRootSignature.InitStaticSampler(0, non_static_sampler);
	m_computeRootSignature[e_rootParameterCB].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, e_cCB);
	m_computeRootSignature[e_rootParameterOLightIndexCounterUAV].InitAsBufferUAV(0);
	m_computeRootSignature[e_rootParameterTLightIndexCounterUAV].InitAsBufferUAV(1);

	m_computeRootSignature[e_rootParameterOLightIndexListUAV].InitAsBufferUAV(2);
	m_computeRootSignature[e_rootParameterTLightIndexListUAV].InitAsBufferUAV(3);
	m_computeRootSignature[e_rootParameterOLightGridUAV].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 1);
	m_computeRootSignature[e_rootParameterTLightGridUAV].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5, 1);
	//m_computeRootSignature[e_rootParameterOLightGridUAV].InitAsBufferUAV(4);
	//m_computeRootSignature[e_rootParameterTLightGridUAV].InitAsBufferUAV(5);
	m_computeRootSignature[e_rootParameterFrustumSRV].InitAsBufferSRV(0);
	m_computeRootSignature[e_rootParameterLightsSRV].InitAsBufferSRV(1);
	//m_computeRootSignature[e_rootParameterDepthSRV].InitAsBufferSRV(2);
	m_computeRootSignature[e_rootParameterDepthSRV].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);
	// TODO need to add RW texture to the room signature
	//m_computeRootSignature[e_rootParameterSRV].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, e_cSRV);


	m_computeRootSignature[e_rootParameterDebugUAV].InitAsBufferUAV(6);

	m_computeRootSignature.Finalize(L"LightCullingPassRootSignature");

	// Create compute PSO
	ComPtr<ID3D12Resource> csInputUploadHeap;
	{
		ComPtr<ID3DBlob> computeShader;
		ComPtr<ID3DBlob> error;

#if defined(_DEBUG)
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

		ComPtr<ID3DBlob> errorMessages;
		HRESULT hr = D3DCompileFromFile(L"LightCullingPass.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CS_LightCullingPass", "cs_5_1", compileFlags, 0, &computeShader, &errorMessages);
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


		m_computePSO.SetRootSignature(m_computeRootSignature);
		m_computePSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(computeShader.Get()));
		m_computePSO.Finalize();
	}

	ThrowIfFailed(IGraphics::g_GraphicsCore->m_computeCommandList->Reset(IGraphics::g_GraphicsCore->m_computeCommandAllocator.Get(), m_computePSO.GetPSO()));
	//ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_computeCommandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), m_computePSO.GetPSO(), IID_PPV_ARGS(&m_computeCommandList)));


	// Prepare for Output of Grid
	m_BlockSizeX = ceil(ScreenWidth * 1.0f / m_TiledSize);
	m_BlockSizeY = ceil(ScreenHeight * 1.0f / m_TiledSize);
	// Opaque LightIndexCounter Structured Buffer
	m_oLightIndexCounter.Create(L"O_LightIndexCounter", 1, sizeof(UINT));
	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_oLightIndexCounter.GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	// Transparent LightIndexCounter Structured Buffer
	m_tLightIndexCounter.Create(L"T_LightIndexCounter", 1, sizeof(UINT));
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
	//m_oLightGrid.Create(L"OpageLightGridMap", m_BlockSizeX, m_BlockSizeY, 1, DXGI_FORMAT_R32G32_FLOAT);
	//IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_oLightGrid.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
	CreateGPUTex2DUAVResource(L"OpageLightGridMap", ScreenWidth, ScreenHeight, sizeof(XMFLOAT2), DXGI_FORMAT_R32G32_UINT, m_oLightGrid, 0);

	// Transparent LightGrid
	CreateGPUTex2DUAVResource(L"TransparentLightGridMap", ScreenWidth, ScreenHeight, sizeof(XMFLOAT2), DXGI_FORMAT_R32G32_UINT, m_tLightGrid, 1);
	//m_tLightGrid.Create(L"TransparentLightGridMap", m_BlockSizeX, m_BlockSizeY, 1, DXGI_FORMAT_R32G32_FLOAT);
	//IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_tLightGrid.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	// Debug Buffer
	//m_CSDebugUAV.Create(L"DebugUAV", m_BlockSizeX * m_BlockSizeY, sizeof(float));
	//IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_CSDebugUAV.GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	/*Test SRV*/
	ComPtr<ID3D12Resource> csInputUploadHeapTest;
	{
		uint32_t w(ScreenWidth), h(ScreenHeight);
		vector<float> csInputArray;
		for (int i = 0; i < w * h; ++i)
			csInputArray.push_back(i + 1);
			//csInputArray.push_back(XMFLOAT4(i + 1, i + 1, i + 1, i + 1));

		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R32_FLOAT;
		textureDesc.Width = w;
		textureDesc.Height = h;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;


		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_computeInputTex2D)));

		const UINT64 csInputUploadBufferSize = GetRequiredIntermediateSize(m_computeInputTex2D.Get(), 0, 1);

		//D3D12_RESOURCE_ALLOCATION_INFO info = {};
		//info.SizeInBytes = 1024;
		//info.Alignment = 0;
		//const D3D12_RESOURCE_DESC tempBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(info);
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(csInputUploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&csInputUploadHeapTest)));


		D3D12_SUBRESOURCE_DATA computeData = {};
		computeData.pData = reinterpret_cast<BYTE*>(csInputArray.data());
		//computeData.RowPitch = w * (int)csInputArray.size() * sizeof(float);
		computeData.RowPitch = w * sizeof(float);
		computeData.SlicePitch = computeData.RowPitch * h;

		//ThrowIfFailed(IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex]->Reset());
		//ThrowIfFailed(IGraphics::g_GraphicsCore->g_commandList->Reset(IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), nullptr));

		UpdateSubresources(IGraphics::g_GraphicsCore->g_commandList.Get(), m_computeInputTex2D.Get(), csInputUploadHeapTest.Get(), 0, 0, 1, &computeData);

		// Describe and create a SRV for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		CD3DX12_CPU_DESCRIPTOR_HANDLE csSRVHandle(m_cbvUavSrvHeap->GetCPUDescriptorHandleForHeapStart(), 2, m_cbvUavSrvDescriptorSize);
		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(m_computeInputTex2D.Get(), &srvDesc, csSRVHandle);

		IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_computeInputTex2D.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
	}
	ThrowIfFailed(IGraphics::g_GraphicsCore->g_commandList->Close());
	ID3D12CommandList* ppCommandLists1[] = { IGraphics::g_GraphicsCore->g_commandList.Get() };
	IGraphics::g_GraphicsCore->g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists1), ppCommandLists1);
	IGraphics::g_GraphicsCore->WaitForGpu();
	ThrowIfFailed(IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex]->Reset());
	ThrowIfFailed(IGraphics::g_GraphicsCore->g_commandList->Reset(IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), nullptr));



	// Create the constant buffer for DispatchParams
	{
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_dispatchParamsCB)));

		// Describe and create a constant buffer view.
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_dispatchParamsCB->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(DispatchParams) + 255) & ~255;    // CB size is required to be 256-byte aligned.
		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateConstantBufferView(&cbvDesc, m_cbvUavSrvHeap->GetCPUDescriptorHandleForHeapStart());

		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_dispatchParamsCB->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDispatchParams)));
		m_dispatchParamsData.numThreadGroups = XMUINT3(ceil(1.0f * m_BlockSizeX / m_TiledSize), ceil(1.0f * m_BlockSizeY / m_TiledSize), 1);
		m_dispatchParamsData.numThreads = XMUINT3(m_BlockSizeX, m_BlockSizeY, 1);
		m_dispatchParamsData.blockSize = XMUINT2(m_TiledSize, m_TiledSize);
		m_dispatchParamsData.padding1 = 1;
		m_dispatchParamsData.padding2 = 1;
		m_dispatchParamsData.padding3 = XMUINT2(1, 1);
		memcpy(m_pCbvDispatchParams, &m_dispatchParamsData, sizeof(m_dispatchParamsData));
	}

	// Create the constant buffer for ScreenToViewParams
	{
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_screenToViewParamsCB)));

		// Describe and create a constant buffer view.
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_screenToViewParamsCB->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(ScreenToViewParams) + 255) & ~255;    // CB size is required to be 256-byte aligned.
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(m_cbvUavSrvHeap->GetCPUDescriptorHandleForHeapStart(), 1, m_cbvUavSrvDescriptorSize);
		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateConstantBufferView(&cbvDesc, cbvHandle);

		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_screenToViewParamsCB->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvScreenToViewParams)));
		m_screenToViewParamsData.InverseProjection = inverseProjection;
		m_screenToViewParamsData.ScreenDimensions = XMUINT2(ScreenWidth, ScreenHeight);
		memcpy(m_pCbvScreenToViewParams, &m_screenToViewParamsData, sizeof(m_screenToViewParamsData));
	}

	ThrowIfFailed(IGraphics::g_GraphicsCore->m_computeCommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { IGraphics::g_GraphicsCore->m_computeCommandList.Get() };
	IGraphics::g_GraphicsCore->m_computeCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	IGraphics::g_GraphicsCore->WaitForComputeShaderGpu();
}

void LightCullingPass::ExecuteOnCS(StructuredBuffer& FrustumIn, 
	ComPtr<ID3D12DescriptorHeap>& depthBufferHeap,
	uint32_t depthBufferOffset)
{
	ThrowIfFailed(IGraphics::g_GraphicsCore->m_computeCommandList->Reset(IGraphics::g_GraphicsCore->m_computeCommandAllocator.Get(), m_computePSO.GetPSO()));
	ComPtr<ID3D12GraphicsCommandList> m_computeCommandList = IGraphics::g_GraphicsCore->m_computeCommandList;

	m_computeCommandList->SetComputeRootSignature(m_computeRootSignature.GetSignature());
	D3D12_GPU_DESCRIPTOR_HANDLE cbvSrvUavHandle = m_cbvUavSrvHeap->GetGPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE uavHandle = m_uavHeap->GetGPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE dsvHandle = depthBufferHeap->GetGPUDescriptorHandleForHeapStart();

	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvUavSrvHeap.Get() };
	m_computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//m_computeCommandList->SetComputeRootConstantBufferView(e_rootParameterCB, m_computeHeap);
	m_computeCommandList->SetComputeRootDescriptorTable(
		e_rootParameterCB,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, 0, m_cbvUavSrvDescriptorSize));
	m_computeCommandList->SetComputeRootDescriptorTable(
		e_rootParameterDepthSRV,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, 2, m_cbvUavSrvDescriptorSize));

	// OLightIndexCounterUAV
	m_computeCommandList->SetComputeRootUnorderedAccessView(
		e_rootParameterOLightIndexCounterUAV,
		m_oLightIndexCounter.GetGpuVirtualAddress());
	// TLightIndexCounterUAV
	m_computeCommandList->SetComputeRootUnorderedAccessView(
		e_rootParameterTLightIndexCounterUAV,
		m_tLightIndexCounter.GetGpuVirtualAddress());
	// OLightIndexList
	m_computeCommandList->SetComputeRootUnorderedAccessView(
		e_rootParameterOLightIndexListUAV,
		m_oLightIndexList.GetGpuVirtualAddress());
	// TLightIndexList
	m_computeCommandList->SetComputeRootUnorderedAccessView(
		e_rootParameterTLightIndexListUAV,
		m_tLightIndexList.GetGpuVirtualAddress());

	// Test Buffer
	m_computeCommandList->SetComputeRootUnorderedAccessView(
		e_rootParameterDebugUAV,
		m_testUAVBuffer.GetGpuVirtualAddress());
	//// Test Frustum SRV
	//m_computeCommandList->SetComputeRootShaderResourceView(
	//	e_rootParameterFrustumSRV,
	//	m_testSB.GetGpuVirtualAddress());

	// Frustum SRV
	m_computeCommandList->SetComputeRootShaderResourceView(
		e_rootParameterFrustumSRV,
		FrustumIn.GetGpuVirtualAddress());
	// Lights SRV
	m_computeCommandList->SetComputeRootShaderResourceView(
		e_rootParameterLightsSRV,
		m_Lights.GetGpuVirtualAddress());

	//ID3D12DescriptorHeap* ppHeaps2[] = { m_srvHeap.Get() };
	//m_computeCommandList->SetDescriptorHeaps(_countof(ppHeaps2), ppHeaps2);
	//uint32_t dsvDescriptorSize = IGraphics::g_GraphicsCore->g_pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//m_computeCommandList->SetComputeRootDescriptorTable(
	//	e_rootParameterDepthSRV,
	//	m_srvHeap->GetGPUDescriptorHandleForHeapStart());

	// Depth SRV
	//ID3D12DescriptorHeap* ppHeaps2[] = { depthBufferHeap.Get() };
	//m_computeCommandList->SetDescriptorHeaps(_countof(ppHeaps2), ppHeaps2);
	//uint32_t dsvDescriptorSize = IGraphics::g_GraphicsCore->g_pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//m_computeCommandList->SetComputeRootDescriptorTable(
	//	e_rootParameterDepthSRV,
	//	CD3DX12_GPU_DESCRIPTOR_HANDLE(dsvHandle, depthBufferOffset, dsvDescriptorSize));



	//ID3D12DescriptorHeap* ppHeaps3[] = { m_uavHeap.Get() };
	//m_computeCommandList->SetDescriptorHeaps(_countof(ppHeaps3), ppHeaps3);
	//// OLightGrid
	//m_computeCommandList->SetComputeRootDescriptorTable(
	//	e_rootParameterOLightGridUAV,
	//	CD3DX12_GPU_DESCRIPTOR_HANDLE(uavHandle, 0, m_uavDescriptorSize));
	//// TLightGrid
	//m_computeCommandList->SetComputeRootDescriptorTable(
	//	e_rootParameterTLightGridUAV,
	//	CD3DX12_GPU_DESCRIPTOR_HANDLE(uavHandle, 1, m_uavDescriptorSize));

	//m_computeCommandList->SetComputeRootUnorderedAccessView(
	//	2,
	//	m_CSDebugUAV.GetGpuVirtualAddress());

	m_computeCommandList->Dispatch(m_dispatchParamsData.numThreadGroups.x, m_dispatchParamsData.numThreadGroups.y, 1);
	//m_computeCommandList->Dispatch(1, 1, 1);

	ThrowIfFailed(m_computeCommandList->Close());

	ID3D12CommandList* tmpList = m_computeCommandList.Get();
	IGraphics::g_GraphicsCore->m_computeCommandQueue->ExecuteCommandLists(1, &tmpList);

	IGraphics::g_GraphicsCore->WaitForComputeShaderGpu();
}

//void LightCullingPass::WaitForComputeShader()
//{
//	// Signal and increment the fence value.
//	const UINT64 fence = m_computeFenceValue;
//	ThrowIfFailed(m_computeCommandQueue->Signal(m_computeFence.Get(), fence));
//	m_computeFenceValue++;
//
//	// Wait until the previous frame is finished.
//	if (m_computeFence->GetCompletedValue() < fence)
//	{
//		ThrowIfFailed(m_computeFence->SetEventOnCompletion(fence, m_computeFenceEvent));
//		WaitForSingleObject(m_computeFenceEvent, INFINITE);
//	}
//}

void LightCullingPass::Destroy()
{
	// TODO
}

void LightCullingPass::UpdateLightBuffer(vector<Light>& lightList)
{
	m_Lights.Destroy();
	m_Lights.Create(L"LightLists", lightList.size(), sizeof(Light), lightList.data());
	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_Lights.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
}

void LightCullingPass::CreateGPUTex2DSRVResource(
	wstring name, uint32_t width, uint32_t height, 
	uint32_t elementSize, DXGI_FORMAT format, 
	ComPtr<ID3D12Resource> pResource, uint32_t heapOffset, void* initData)
{
	// Describe and create a Texture2D.
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.MipLevels = 1;
	//textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resourceDesc.Format = format;
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&pResource)));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(pResource.Get(), 0, 1);

	if (initData)
	{
		ComPtr<ID3D12Resource> uploadHeap;
		// Create the GPU upload buffer.
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadHeap)));

		D3D12_SUBRESOURCE_DATA  textureData = {};
		textureData.pData = initData;
		textureData.RowPitch = width * elementSize;
		textureData.SlicePitch = textureData.RowPitch * height;

		IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pResource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
		UpdateSubresources(IGraphics::g_GraphicsCore->g_commandList.Get(), pResource.Get(), uploadHeap.Get(), 0, 0, 1, &textureData);
		IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));

		ASSERT_SUCCEEDED(IGraphics::g_GraphicsCore->g_commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { IGraphics::g_GraphicsCore->g_commandList.Get() };
		IGraphics::g_GraphicsCore->g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		IGraphics::g_GraphicsCore->WaitForGpu();

		ASSERT_SUCCEEDED(IGraphics::g_GraphicsCore->g_commandList->Reset(IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), nullptr));
	}

	// Describe and create a SRV for the texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = resourceDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_uavHeap->GetCPUDescriptorHandleForHeapStart(), heapOffset, m_uavDescriptorSize);
	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(pResource.Get(), &srvDesc, srvHandle);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = width * height;
	uavDesc.Buffer.StructureByteStride = sizeof(XMFLOAT4);
	uavDesc.Buffer.CounterOffsetInBytes = width * height * sizeof(XMFLOAT4);
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_uavHeap->GetCPUDescriptorHandleForHeapStart(), heapOffset, m_uavDescriptorSize);
	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateUnorderedAccessView(pResource.Get(), nullptr, nullptr, uavHandle);

	pResource->SetName(name.c_str());
}

void LightCullingPass::CreateGPUTex2DUAVResource(
	wstring name, uint32_t width, uint32_t height,
	uint32_t elementSize, DXGI_FORMAT format,
	ComPtr<ID3D12Resource> pResource, uint32_t heapOffset, void* initData)
{
	D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(&pResource)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_uavHeap->GetCPUDescriptorHandleForHeapStart(), heapOffset, m_uavDescriptorSize);

	//D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	//srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	//srvDesc.Format = format;
	//srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	//srvDesc.Texture2D.MipLevels = 1;
	//IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(pResource.Get(), &srvDesc, uavHandle);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = width * height;
	uavDesc.Buffer.StructureByteStride = elementSize;
	uavDesc.Buffer.CounterOffsetInBytes = width * height * elementSize;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateUnorderedAccessView(pResource.Get(), nullptr, nullptr, uavHandle);

	pResource->SetName(name.c_str());



	//// Describe and create a SRV for the texture.
	//D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	//srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	//srvDesc.Format = textureDesc.Format;
	//srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	//srvDesc.Texture2D.MipLevels = 1;
	//CD3DX12_CPU_DESCRIPTOR_HANDLE csSRVHandle(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), e_iSRV, m_cbvSrvUavDescriptorSize);
	//IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(m_computeInputTex2D.Get(), &srvDesc, csSRVHandle);


	//// create compute shader UAV
	//textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, w, h, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	//ThrowIfFailed(
	//	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
	//		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
	//		D3D12_HEAP_FLAG_NONE,
	//		&textureDesc,
	//		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
	//		nullptr,
	//		IID_PPV_ARGS(&m_computeOutput)));

	//CD3DX12_CPU_DESCRIPTOR_HANDLE csUAVHandle(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), e_iUAV, m_cbvSrvUavDescriptorSize);
	//IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(m_computeOutput.Get(), &srvDesc, csUAVHandle);

	//D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	//uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	//uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	//uavDesc.Buffer.FirstElement = 0;
	//uavDesc.Buffer.NumElements = w * h;
	//uavDesc.Buffer.StructureByteStride = sizeof(XMFLOAT4);
	//uavDesc.Buffer.CounterOffsetInBytes = w * h * sizeof(XMFLOAT4);
	//uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	//IGraphics::g_GraphicsCore->g_pD3D12Device->CreateUnorderedAccessView(m_computeOutput.Get(), nullptr, nullptr, csUAVHandle);
}





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

		m_screenToViewParamsCB.mUsageState = D3D12_RESOURCE_STATE_GENERIC_READ;
		m_screenToViewParamsCB.uCbvDescriptorOffset = gCbvSrvUavOffset;
		++gCbvSrvUavOffset;
	}

	// Load Grid Frustum Asset
	LoadGridFrustumAsset(ScreenWidth, ScreenHeight, inverseProjection, gCbvSrvUavDescriptorHeap, gCbvSrvUavOffset);
}

void ForwardPlusLightCulling::Resize()
{

}

void ForwardPlusLightCulling::Destroy()
{

}

void ForwardPlusLightCulling::ExecuteCS(ComPtr<ID3D12DescriptorHeap> gCbvSrvuavDescriptorHeap)
{
	ExecuteGridFrustumCS(gCbvSrvuavDescriptorHeap);
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
