#include "stdafx.h"
#include "LightCullingPass.h"
#include "Utility.h"
#include "DXSampleHelper.h"

void GridFrustumsPass::Init(wstring shader_file, uint32_t ScreenWidth, uint32_t ScreenHeight, 
	XMMATRIX inverseProjection)
{
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};

		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = e_numRootParameters;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_cbvUavSrvHeap)) != S_OK);
		m_cbvUavSrvDescriptorSize = IGraphics::g_GraphicsCore->g_pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	// Create compute shader resource
	{
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

	m_computeRootSignature.Reset(4, 1);
	m_computeRootSignature.InitStaticSampler(0, non_static_sampler);
	m_computeRootSignature[e_rootParameterCB].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, e_cCB);
	m_computeRootSignature[e_rootParameterUAV].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, e_cUAV);
	//m_computeRootSignature[e_rootParameterSRV].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, e_cSRV);
	m_computeRootSignature.Finalize(L"GridFrustumPassRootSignature");


	// Create compute PSO
	ComPtr<ID3D12Resource> csInputUploadHeap;
	{
		ComPtr<ID3DBlob> computeShader;
		ComPtr<ID3DBlob> error;

#if defined(_DEBUG)
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

		ComPtr<ID3DBlob> errorMessages;
		HRESULT hr = D3DCompileFromFile(L"GridFrustumPassCS.hlsl", nullptr, nullptr, "CSMain", "cs_5_1", compileFlags, 0, &computeShader, &errorMessages);
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
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"csShader.hlsl").c_str(), nullptr, nullptr, "CSMain", "cs_5_1", compileFlags, 0, &pixelShader, nullptr));
#endif


		m_computePSO.SetRootSignature(m_computeRootSignature);
		m_computePSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(computeShader.Get()));
		m_computePSO.Finalize();
	}

	ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_computeCommandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), m_computePSO.GetPSO(), IID_PPV_ARGS(&m_computeCommandList)));


	// Prepare for Output of Grid
	m_BlockSizeX = ceil(ScreenWidth * 1.0f / m_TiledSize);
	m_BlockSizeY = ceil(ScreenHeight * 1.0f / m_TiledSize);
	m_CSGridFrustumOutputSB.Create(L"GridFrustumsPassOutputBuffer", m_BlockSizeX * m_BlockSizeY, sizeof(m_BlockSizeX));
	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_CSGridFrustumOutputSB.GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

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
		m_dispatchParamsData.blockSize = XMUINT2(16, 16);
		m_dispatchParamsData.numThreadGroups = XMUINT3(1, 1, 1);
		m_dispatchParamsData.numThreads = XMUINT3(1, 1, 1);
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

	ThrowIfFailed(m_computeCommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_computeCommandList.Get() };
	m_computeCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	WaitForComputeShader();

	ThrowIfFailed(IGraphics::g_GraphicsCore->g_commandList->Close());
	ID3D12CommandList* ppCommandLists1[] = { IGraphics::g_GraphicsCore->g_commandList.Get() };
	IGraphics::g_GraphicsCore->g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists1), ppCommandLists1);

	IGraphics::g_GraphicsCore->WaitForGpu();
}

void GridFrustumsPass::ExecuteOnCS()
{
	ThrowIfFailed(m_computeCommandList->Reset(m_computeCommandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), m_computePSO.GetPSO()));

	m_computeCommandList->SetComputeRootSignature(m_computeRootSignature.GetSignature());
	D3D12_GPU_DESCRIPTOR_HANDLE cbvSrvUavHandle = m_cbvUavSrvHeap->GetGPUDescriptorHandleForHeapStart();

	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvUavSrvHeap.Get() };
	m_computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//m_computeCommandList->SetComputeRootConstantBufferView(e_rootParameterCB, m_computeHeap);
	m_computeCommandList->SetComputeRootDescriptorTable(
		e_rootParameterCB,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, 0, m_cbvUavSrvDescriptorSize));
	m_computeCommandList->SetComputeRootUnorderedAccessView(
		e_rootParameterUAV,
		m_CSGridFrustumOutputSB.GetGpuVirtualAddress());


	m_computeCommandList->Dispatch(m_BlockSizeX, m_BlockSizeY, 1);

	ThrowIfFailed(m_computeCommandList->Close());

	ID3D12CommandList* tmpList = m_computeCommandList.Get();
	m_computeCommandQueue->ExecuteCommandLists(1, &tmpList);

	WaitForComputeShader();
}

void GridFrustumsPass::WaitForComputeShader()
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

void GridFrustumsPass::Destroy()
{

}
