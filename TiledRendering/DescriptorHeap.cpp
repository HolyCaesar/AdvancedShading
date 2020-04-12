#include "stdafx.h"
#include "Utility.h"
#include "GraphicsCore.h"
#include "DescriptorHeap.h"

std::mutex DescriptorAllocator::sm_AllocationMutex;
std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> DescriptorAllocator::sm_DescriptorHeapPool;


void DescriptorAllocator::DestroyAll(void)
{
	sm_DescriptorHeapPool.clear();
}

ID3D12DescriptorHeap* DescriptorAllocator::RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
	std::lock_guard<std::mutex> LockGuard(sm_AllocationMutex);

	D3D12_DESCRIPTOR_HEAP_DESC Desc;
	Desc.Type = Type;
	Desc.NumDescriptors = sm_NumDescriptorsPerHeap;
	Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	Desc.NodeMask = 1;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pHeap;
	ASSERT_SUCCEEDED(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&pHeap)));
	sm_DescriptorHeapPool.emplace_back(pHeap);
	return pHeap.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::Allocate(uint32_t Count)
{
	if (m_CurrentHeap == nullptr or m_RemainingFreeHandles < Count)
	{
		m_CurrentHeap = RequestNewHeap(m_Type);
		m_CurrentHandle = m_CurrentHeap->GetCPUDescriptorHandleForHeapStart();
		m_RemainingFreeHandles = sm_NumDescriptorsPerHeap;

		if (m_DescriptorSize == 0)
			m_DescriptorSize = IGraphics::g_GraphicsCore->g_pD3D12Device->GetDescriptorHandleIncrementSize(m_Type);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE ret = m_CurrentHandle;
	// Advance the pointer
	m_CurrentHandle.ptr += Count * m_DescriptorSize;
	m_RemainingFreeHandles -= Count;
	return ret;
}

void ShaderVisibleDescriptorHeap::Create(const std::wstring& DebugHeapName)
{
	ASSERT_SUCCEEDED(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDescriptorHeap(&m_HeapDesc, IID_PPV_ARGS(m_Heap.ReleaseAndGetAddressOf())));
#ifdef RELEASE
	(void)DebugHeapName;
#else
	m_Heap->SetName(DebugHeapName.c_str());
#endif

	m_DescriptorSize = IGraphics::g_GraphicsCore->g_pD3D12Device->GetDescriptorHandleIncrementSize(m_HeapDesc.Type);
	m_NumFreeDescriptors = m_HeapDesc.NumDescriptors;
	m_FirstHandle = DescriptorHandle(m_Heap->GetCPUDescriptorHandleForHeapStart(), m_Heap->GetGPUDescriptorHandleForHeapStart());
	m_NextFreeHandle = m_FirstHandle;
}

DescriptorHandle ShaderVisibleDescriptorHeap::Allocate(uint32_t Count)
{
	ASSERT(HasAvailableSpace(Count), "Descriptor Heap out of space. Increase heap size.");
	DescriptorHandle ret = m_NextFreeHandle;
	m_NextFreeHandle += Count * m_DescriptorSize;
	return ret;
}

bool ShaderVisibleDescriptorHeap::ValidateHandle(const DescriptorHandle& DHandle) const
{
	if (DHandle.GetCpuHandle().ptr < m_FirstHandle.GetCpuHandle().ptr or
		DHandle.GetCpuHandle().ptr >= m_FirstHandle.GetCpuHandle().ptr + m_HeapDesc.NumDescriptors * m_DescriptorSize)
		return false;

	if (DHandle.GetGpuHandle().ptr - m_FirstHandle.GetGpuHandle().ptr !=
		DHandle.GetCpuHandle().ptr - m_FirstHandle.GetCpuHandle().ptr)
		return false;
	return true;
}
