#pragma once
#include <memory>
#include "DescriptorHeap.h"

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
		ComPtr<ID3D12CommandQueue> g_commandQueue;
		ComPtr<ID3D12GraphicsCommandList> g_commandList;
		HWND g_hwnd;

		//CommandListManager m_CommandManager;

		float s_FrameTime = 0.0f;
		uint64_t s_FrameIndex = 0;
		int64_t s_FrameStartTick = 0;

	private:
		static GraphicsCore* m_gcInstance;
		static std::mutex m_mutex;

	private:
		GraphicsCore() = default;
		~GraphicsCore() = default;
		GraphicsCore(const GraphicsCore&) = delete;
		GraphicsCore& operator=(const GraphicsCore&) = delete;
	};


	extern GraphicsCore* g_GraphicsCore;
}

