#pragma once
#include <memory>
#include "DescriptorHeap.h"
#include "CommandContext.h"
#include "DX12GraphicsCommon.h"

#define SWAP_CHAIN_BUFFER_COUNT 3

class CommandListManager;
class ContextManager;

namespace IGraphics
{
	using namespace Microsoft::WRL;
	class GraphicsCore
	{
	public:
		static GraphicsCore* GetInstance()
		{
			if (m_gcInstance == nullptr)
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				if(!m_gcInstance) m_gcInstance = new GraphicsCore();
			}
			return m_gcInstance;
		}

		void Initialize(void);
		void Resize(uint32_t width, uint32_t height);
		void Terminate(void);
		void Shutdown(void);
		void Present(void);

		uint32_t m_DisplayWidth;
		uint32_t m_DisplayHeight;

		uint64_t GetFrameCount(void);
		// Time elapsed between two frames
		float GetFrameTime(void);

		ComPtr<ID3D12Device> g_pD3D12Device;
		ComPtr<IDXGISwapChain3> g_pSwapChain;
		HWND g_hwnd;

		std::unique_ptr<CommandListManager> g_CommandManager;
		std::unique_ptr<ContextManager>		g_ContextManager;

		ColorBuffer g_DisplayPlane[SWAP_CHAIN_BUFFER_COUNT];
		UINT g_CurrentBuffer = 0;
		DX12RootSignature s_PresentRS;

		DescriptorAllocator g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] =
		{
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			D3D12_DESCRIPTOR_HEAP_TYPE_DSV
		};
		D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1)
		{
			return g_DescriptorAllocator[Type].Allocate(Count);
		}


		//void WaitForGpu();
		//void MoveToNextFrame();
		UINT64 GetRenderFenceValue() { return m_fenceValue[s_FrameIndex]; }

		float s_FrameTime = 0.0f;
		uint64_t s_FrameIndex = 0;
		int64_t s_FrameStartTick = 0;

	private:
		static GraphicsCore* m_gcInstance;
		static std::mutex m_mutex;

		// Synchronization objects.
		HANDLE m_fenceEvent;
		ComPtr<ID3D12Fence> m_fence;
		UINT64 m_fenceValue[SWAP_CHAIN_BUFFER_COUNT];

	private:
		GraphicsCore() = default;
		~GraphicsCore() = default;
		GraphicsCore(const GraphicsCore&) = delete;
		GraphicsCore& operator=(const GraphicsCore&) = delete;

		// Temp resources for compute shader
	public:
		ComPtr<ID3D12CommandAllocator> m_computeCommandAllocator;
		ComPtr<ID3D12CommandQueue> m_computeCommandQueue;
		ComPtr<ID3D12GraphicsCommandList> m_computeCommandList;

		ComPtr<ID3D12Fence> m_computeFence;
		uint64_t m_computeFenceValue;
		HANDLE m_computeFenceEvent;

		void InitializeCS(void);
		void WaitForComputeShaderGpu(void);
	};

	extern GraphicsCore* g_GraphicsCore;
}

