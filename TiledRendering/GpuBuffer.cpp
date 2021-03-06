#include "stdafx.h"
#include "GpuBuffer.h"
#include "Utility.h"
#include "GraphicsCore.h"

using namespace IGraphics;

void GpuBuffer::Create(const std::wstring& name, uint32_t NumElements, uint32_t ElementSize, const void* initialData)
{
	Destroy();

	m_ElementCount = NumElements;
	m_ElementSize = ElementSize;
	m_BufferSize = NumElements * ElementSize;

	D3D12_RESOURCE_DESC ResourceDesc = DescribeBuffer();

	m_UsageState = D3D12_RESOURCE_STATE_COMMON;

	D3D12_HEAP_PROPERTIES heapProps;
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	ASSERT_SUCCEEDED(
		g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&heapProps, D3D12_HEAP_FLAG_NONE,
			&ResourceDesc, m_UsageState, nullptr, IID_PPV_ARGS(&m_pResource)
		)
	);

	m_GpuVirtualAddress = m_pResource->GetGPUVirtualAddress();


	//if (initialData)
	//{
	//	ComPtr<ID3D12Resource> uploadHeap;
	//	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_pResource.Get(), 0, 1);

	//	// Create the GPU upload buffer.
	//	ASSERT_SUCCEEDED(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
	//		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
	//		D3D12_HEAP_FLAG_NONE,
	//		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
	//		D3D12_RESOURCE_STATE_GENERIC_READ,
	//		nullptr,
	//		IID_PPV_ARGS(&uploadHeap)));

	//	D3D12_SUBRESOURCE_DATA rawData = {};
	//	rawData.pData = reinterpret_cast<const BYTE*>(initialData);
	//	rawData.RowPitch = m_BufferSize;
	//	rawData.SlicePitch = rawData.RowPitch;

	//	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_pResource.Get(), m_UsageState, D3D12_RESOURCE_STATE_COPY_DEST));
	//	m_UsageState = D3D12_RESOURCE_STATE_COPY_DEST;
	//	UpdateSubresources(IGraphics::g_GraphicsCore->g_commandList.Get(), m_pResource.Get(), uploadHeap.Get(), 0, 0, 1, &rawData);
	//	IGraphics::g_GraphicsCore->g_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_pResource.Get(), m_UsageState, D3D12_RESOURCE_STATE_GENERIC_READ));
	//	m_UsageState = D3D12_RESOURCE_STATE_GENERIC_READ;

	//	ASSERT_SUCCEEDED(IGraphics::g_GraphicsCore->g_commandList->Close());
	//	ID3D12CommandList* ppCommandLists[] = { IGraphics::g_GraphicsCore->g_commandList.Get() };
	//	IGraphics::g_GraphicsCore->g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	//	IGraphics::g_GraphicsCore->WaitForGpu();

	//	ASSERT_SUCCEEDED(IGraphics::g_GraphicsCore->g_commandList->Reset(IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), nullptr));


	//	// Will replace above code with the code below
	//	//CommandContext::InitializeBuffer(*this, initialData, m_BufferSize);
	//}

	if (initialData)
		CommandContext::InitializeBuffer(*this, initialData, m_BufferSize);

#ifdef RELEASE
	(name);
#else
	m_pResource->SetName(name.c_str());
#endif

	CreateDerivedViews();
}

// Sub-Allocate a buffer out of a pre-allocated heap.  If initial data is provided, it will be copied into the buffer using the default command context.
void GpuBuffer::CreatePlaced(const std::wstring& name, ID3D12Heap* pBackingHeap, uint32_t HeapOffset, uint32_t NumElements, uint32_t ElementSize,
	const void* initialData)
{
	m_ElementCount = NumElements;
	m_ElementSize = ElementSize;
	m_BufferSize = NumElements * ElementSize;

	D3D12_RESOURCE_DESC ResourceDesc = DescribeBuffer();

	m_UsageState = D3D12_RESOURCE_STATE_COMMON;

	ASSERT_SUCCEEDED(g_GraphicsCore->g_pD3D12Device->CreatePlacedResource(pBackingHeap, HeapOffset, &ResourceDesc, m_UsageState, nullptr, IID_PPV_ARGS(&m_pResource)));

	m_GpuVirtualAddress = m_pResource->GetGPUVirtualAddress();

	if (initialData)
	{
		// TODO need to implement uploaded heap
	}

#ifdef RELEASE
	(name);
#else
	m_pResource->SetName(name.c_str());
#endif

	CreateDerivedViews();

}

//D3D12_CPU_DESCRIPTOR_HANDLE GpuBuffer::CreateConstantBufferView(uint32_t Offset, uint32_t Size) const
//{
	// TODO Implemented AllocateDescriptor function and AlignUp.

	//ASSERT(Offset + Size <= m_BufferSize);

	//Size = Math::AlignUp(Size, 16);

	//D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDesc;
	//CBVDesc.BufferLocation = m_GpuVirtualAddress + (size_t)Offset;
	//CBVDesc.SizeInBytes = Size;

	//D3D12_CPU_DESCRIPTOR_HANDLE hCBV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//g_GraphicsCore->g_pD3D12Device->CreateConstantBufferView(&CBVDesc, hCBV);

//	D3D12_CPU_DESCRIPTOR_HANDLE hCBV(nullptr);
//	return hCBV;
//}

D3D12_RESOURCE_DESC GpuBuffer::DescribeBuffer(void)
{
	ASSERT(m_BufferSize != 0);

	D3D12_RESOURCE_DESC Desc = {};
	Desc.Alignment = 0;
	Desc.DepthOrArraySize = 1;
	Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	Desc.Flags = m_ResourceFlags;
	Desc.Format = DXGI_FORMAT_UNKNOWN;
	Desc.Height = 1;
	Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	Desc.MipLevels = 1;
	Desc.SampleDesc.Count = 1;
	Desc.SampleDesc.Quality = 0;
	Desc.Width = (UINT64)m_BufferSize;
	return Desc;
}

void ByteAddressBuffer::CreateDerivedViews(void)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.Buffer.NumElements = (UINT)m_BufferSize / 4;
	SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

	if (m_SRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_SRV = IGraphics::g_GraphicsCore->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(m_pResource.Get(), &SRVDesc, m_SRV);

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	UAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	UAVDesc.Buffer.NumElements = (UINT)m_BufferSize / 4;
	UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

	if (m_UAV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_UAV = IGraphics::g_GraphicsCore->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateUnorderedAccessView(m_pResource.Get(), nullptr, &UAVDesc, m_UAV);
}

void StructuredBuffer::CreateDerivedViews(void)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.Buffer.NumElements = m_ElementCount;
	SRVDesc.Buffer.StructureByteStride = m_ElementSize;
	SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	if (m_SRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_SRV = IGraphics::g_GraphicsCore->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(m_pResource.Get(), &SRVDesc, m_SRV);

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	UAVDesc.Buffer.CounterOffsetInBytes = 0;
	UAVDesc.Buffer.NumElements = m_ElementCount;
	UAVDesc.Buffer.StructureByteStride = m_ElementSize;
	UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	m_CounterBuffer.Create(L"StructuredBuffer::Counter", 1, 4);

	if (m_UAV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_UAV = IGraphics::g_GraphicsCore->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateUnorderedAccessView(m_pResource.Get(), m_CounterBuffer.GetResource(), &UAVDesc, m_UAV);
}
