#include "stdafx.h"
#include "SimpleComputeShader.h"

SimpleComputeShader::SimpleComputeShader() :
	m_pcsInputStructureBegin(nullptr),
	m_cbvSrvUavDescriptorSize(0)
{

}

SimpleComputeShader::~SimpleComputeShader()
{

}

void SimpleComputeShader::OnInit()
{
	LoadPipeline();
	LoadComputeShaderResources();
}

void SimpleComputeShader::OnDestroy()
{

}

void SimpleComputeShader::OnExecuteCS()
{
	ThrowIfFailed(m_computeCommandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex]->Reset());
	ThrowIfFailed(m_computeCommandList->Reset(m_computeCommandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), m_computePSO.Get()));

	m_computeCommandList->SetComputeRootSignature(m_computeRootSignature.Get());
	D3D12_GPU_DESCRIPTOR_HANDLE cbvSrvUavHandle = m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();

	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvUavHeap.Get() };
	m_computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//m_computeCommandList->SetComputeRootConstantBufferView(e_rootParameterCB, m_computeHeap);
	m_computeCommandList->SetComputeRootDescriptorTable(
		e_rootParameterSRV,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, e_iSRV, m_cbvSrvUavDescriptorSize));
	m_computeCommandList->SetComputeRootDescriptorTable(
		e_rootParameterUAV,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, e_iUAV, m_cbvSrvUavDescriptorSize));


	D3D12_GPU_DESCRIPTOR_HANDLE testHandle = m_testHeap->GetGPUDescriptorHandleForHeapStart();
	ID3D12DescriptorHeap* ppHeaps1[] = { m_testHeap.Get() };
	m_computeCommandList->SetDescriptorHeaps(_countof(ppHeaps1), ppHeaps1);
	m_computeCommandList->SetComputeRootDescriptorTable(
		2,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(testHandle, 0, m_cbvSrvUavDescriptorSize));

	// Reset the UAV counter for this frame.
	//m_computeCommandList->CopyBufferRegion(m_processedCommandBuffers[m_frameIndex].Get(), CommandBufferCounterOffset, m_processedCommandBufferCounterReset.Get(), 0, sizeof(UINT));

	//D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_computeOutput.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	//m_computeCommandList->ResourceBarrier(1, &barrier);

	m_computeCommandList->Dispatch(128, 1, 1);

	ThrowIfFailed(m_computeCommandList->Close());

	ID3D12CommandList* tmpList = m_computeCommandList.Get();
	m_computeCommandQueue->ExecuteCommandLists(1, &tmpList);

	WaitForComputeShader();
}

void SimpleComputeShader::LoadPipeline()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = e_iHeapEnd;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_cbvSrvUavHeap)) != S_OK);
	m_cbvSrvUavDescriptorSize = IGraphics::g_GraphicsCore->g_pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 1;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_testHeap)) != S_OK);

	// Create compute shader resource
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_computeCommandQueue)));

	for (int n = 0; n < SWAP_CHAIN_BUFFER_COUNT; ++n)
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_computeCommandAllocator[n])));

	ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_computeFence)));
	m_computeFenceValue = 1;

	// Create an event handle to use for frame synchronization.
	m_computeFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_computeFenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void SimpleComputeShader::LoadComputeShaderResources()
{
	// Create compute root signature
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1; // Use highest version
		if (FAILED(IGraphics::g_GraphicsCore->g_pD3D12Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

		// Create compute root signature
		CD3DX12_DESCRIPTOR_RANGE1 ranges[e_numRootParameters + 1];
		//ranges[e_rootParameterSampler].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
		ranges[e_rootParameterSRV].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, e_cSRV, 0);
		ranges[e_rootParameterUAV].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, e_cUAV, 0);
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);

		CD3DX12_ROOT_PARAMETER1 computeRootParameters[e_numRootParameters + 1];
		//computeRootParameters[e_rootParameterCB].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
		//computeRootParameters[e_rootParameterSampler].InitAsDescriptorTable(1, &ranges[e_rootParameterSampler]);
		computeRootParameters[e_rootParameterSRV].InitAsDescriptorTable(1, &ranges[e_rootParameterSRV]);
		computeRootParameters[e_rootParameterUAV].InitAsDescriptorTable(1, &ranges[e_rootParameterUAV]);
		computeRootParameters[2].InitAsDescriptorTable(1, &ranges[2]);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
		computeRootSignatureDesc.Init_1_1(_countof(computeRootParameters), computeRootParameters);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&computeRootSignatureDesc, featureData.HighestVersion, &signature, &error));
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_computeRootSignature)));
		NAME_D3D12_OBJECT(m_computeRootSignature);
	}

	// Create compute PSO
	ComPtr<ID3D12Resource> csInputUploadHeap;
	{
		ComPtr<ID3DBlob> computeShader;
		ComPtr<ID3DBlob> error;

#if defined(_DEBUG)
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

		ComPtr<ID3DBlob> errorMessages;
		HRESULT hr = D3DCompileFromFile(L"csShader.hlsl", nullptr, nullptr, "CSMain", "cs_5_1", compileFlags, 0, &computeShader, &errorMessages);
		if (FAILED(hr) && errorMessages)
		{
			const char* errorMsg = (const char*)errorMessages->GetBufferPointer();
			//MessageBox(nullptr, errorMsg, L"Shader Compilation Error", MB_RETRYCANCEL);
			wstring str;
			for (int i = 0; i < 2000; i++) str += errorMsg[i];
			MessageBox(nullptr, str.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
			exit(0);
		}
#else
		UINT compileFlags = 0;
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"csShader.hlsl").c_str(), nullptr, nullptr, "CSMain", "cs_5_1", compileFlags, 0, &pixelShader, nullptr));
#endif

		D3D12_COMPUTE_PIPELINE_STATE_DESC descComputePSO = {};
		descComputePSO.pRootSignature = m_computeRootSignature.Get();
		descComputePSO.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get());

		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateComputePipelineState(&descComputePSO, IID_PPV_ARGS(&m_computePSO)));
		NAME_D3D12_OBJECT(m_computePSO);
	}

	ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_computeCommandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), m_computePSO.Get(), IID_PPV_ARGS(&m_computeCommandList)));

	uint32_t w(128), h(1);

	vector<XMFLOAT4> csInputArray;
	for (int i = 0; i < w * h; ++i)
		csInputArray.push_back(XMFLOAT4(i + 1, i + 1, i + 1, i + 1));

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
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
		IID_PPV_ARGS(&csInputUploadHeap)));


	D3D12_SUBRESOURCE_DATA computeData = {};
	computeData.pData = reinterpret_cast<BYTE*>(csInputArray.data());
	computeData.RowPitch = w * (int)csInputArray.size() * sizeof(XMFLOAT4);
	computeData.SlicePitch = computeData.RowPitch * h;

	ThrowIfFailed(IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex]->Reset());
	ThrowIfFailed(IGraphics::g_GraphicsCore->g_commandList->Reset(IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), nullptr));

	UpdateSubresources(IGraphics::g_GraphicsCore->g_commandList.Get(), m_computeInputTex2D.Get(), csInputUploadHeap.Get(), 0, 0, 1, &computeData);
	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_computeInputTex2D.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	// Describe and create a SRV for the texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	CD3DX12_CPU_DESCRIPTOR_HANDLE csSRVHandle(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), e_iSRV, m_cbvSrvUavDescriptorSize);
	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(m_computeInputTex2D.Get(), &srvDesc, csSRVHandle);


	// create compute shader RWTexture2D UAV
	textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, w, h, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ThrowIfFailed(
		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&m_computeOutput)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE csUAVHandle(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), e_iUAV, m_cbvSrvUavDescriptorSize);
	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(m_computeOutput.Get(), &srvDesc, csUAVHandle);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = w * h;
	uavDesc.Buffer.StructureByteStride = sizeof(XMFLOAT4);
	uavDesc.Buffer.CounterOffsetInBytes = w * h * sizeof(XMFLOAT4);
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateUnorderedAccessView(m_computeOutput.Get(), nullptr, nullptr, csUAVHandle);


	// Test RWTexture2D
	textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32_UINT, 128, 128, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ThrowIfFailed(
		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&m_test)));


	CD3DX12_CPU_DESCRIPTOR_HANDLE testHandle(m_testHeap->GetCPUDescriptorHandleForHeapStart(), 0, m_cbvSrvUavDescriptorSize);
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(m_test.Get(), &srvDesc, testHandle);

	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = 128 * 128;
	uavDesc.Buffer.StructureByteStride = sizeof(XMFLOAT2);
	uavDesc.Buffer.CounterOffsetInBytes = 128 * 128 * sizeof(XMFLOAT2);
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateUnorderedAccessView(m_test.Get(), nullptr, nullptr, testHandle);


	// Create compute shader UAV (structure buffer)
	D3D12_RESOURCE_DESC Desc = {};
	Desc.Alignment = 0;
	Desc.DepthOrArraySize = 1;
	Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	Desc.Format = DXGI_FORMAT_UNKNOWN;
	Desc.Height = 1;
	Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	Desc.MipLevels = 1;
	Desc.SampleDesc.Count = 1;
	Desc.SampleDesc.Quality = 0;
	Desc.Width = (UINT64)(w * h * sizeof(float));

	ThrowIfFailed(
		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&Desc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&m_computeOutputSB)));

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	UAVDesc.Buffer.CounterOffsetInBytes = 0;
	UAVDesc.Buffer.NumElements = (UINT)(w * h);
	UAVDesc.Buffer.StructureByteStride = sizeof(float);
	UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	CD3DX12_CPU_DESCRIPTOR_HANDLE csUAVHandle1(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), e_iUAV + 1, m_cbvSrvUavDescriptorSize);
	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateUnorderedAccessView(m_computeOutputSB.Get(), nullptr, &UAVDesc, csUAVHandle1);


	// Create a structure buffer for the compute shader
	const UINT csInputStructure = w * h * sizeof(XMFLOAT4);
	ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(csInputStructure),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_computeInputStructureBuffer)));

	CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
	ThrowIfFailed(m_computeInputStructureBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pcsInputStructureBegin)));
	memcpy(m_pcsInputStructureBegin, csInputArray.data(), w * h * sizeof(XMFLOAT4));

	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = w * h;
	srvDesc.Buffer.StructureByteStride = sizeof(XMFLOAT4);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc.Buffer.FirstElement = 0;

	csSRVHandle.Offset(1, m_cbvSrvUavDescriptorSize);
	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(m_computeInputStructureBuffer.Get(), &srvDesc, csSRVHandle);


	ThrowIfFailed(m_computeCommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_computeCommandList.Get() };
	m_computeCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	WaitForComputeShader();

	ThrowIfFailed(IGraphics::g_GraphicsCore->g_commandList->Close());
	ID3D12CommandList* ppCommandLists1[] = { IGraphics::g_GraphicsCore->g_commandList.Get() };
	IGraphics::g_GraphicsCore->g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists1), ppCommandLists1);

	IGraphics::g_GraphicsCore->WaitForGpu();
}

void SimpleComputeShader::WaitForComputeShader()
{
	// Signal and increment the fence value.
	const UINT64 fence = m_computeFenceValue;
	ThrowIfFailed(m_computeCommandQueue->Signal(m_computeFence.Get(), fence));
	m_computeFenceValue++;

	// Wait until the previous frame is finished.
	if (m_computeFence->GetCompletedValue() < fence)
	{
		ThrowIfFailed(m_computeFence->SetEventOnCompletion(fence, m_computeFenceEvent));
		WaitForSingleObject(m_computeFenceEvent, INFINITE);
	}
}
