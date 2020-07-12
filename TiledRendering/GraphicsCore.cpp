#include "stdafx.h"
#include "VectorMath.h"
#include "GraphicsCore.h"
#include "DX12GraphicsCommon.h"
#include "SamplerManager.h"
#include "Utility.h"

#if defined(NTDDI_WIN10_RS2) && (NTDDI_VERSION >= NTDDI_WIN10_RS2)
#include <dxgi1_6.h>
#else
#include <dxgi1_4.h>    // For WARP
#endif

#define SWAP_CHAIN_BUFFER_COUNT 3

DXGI_FORMAT SwapChainFormat = DXGI_FORMAT_R10G10B10A2_UNORM;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) if (x != nullptr) { x->Release(); x = nullptr; }
#endif

using namespace IMath;

//namespace IGameCore
//{
//#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
//    extern HWND g_hWnd;
//#else
//    extern Platform::Agile<Windows::UI::Core::CoreWindow>  g_window;
//#endif
//}

namespace IGraphics
{
	std::mutex GraphicsCore::m_mutex;
	GraphicsCore* GraphicsCore::m_gcInstance;

	GraphicsCore* g_GraphicsCore = GraphicsCore::GetInstance();

	void GraphicsCore::Initialize(void)
	{
		UINT dxgiFactoryFlags = 0;

		// Initialize global variables
		g_ContextManager = std::make_unique<ContextManager>();
		g_CommandManager = std::make_unique<CommandListManager>();


#if defined(_DEBUG)
		// Enable the debug layer (requires the Graphics Tools "optional feature").
		// NOTE: Enabling the debug layer after device creation will invalidate the active device.
		{
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();

				// Enable additional debug layers.
				dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
		}
#endif

		/*
		* D3D12 Device Creation
		*/
		ComPtr<IDXGIFactory4> factory;
		ASSERT_SUCCEEDED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

		static const bool bUseWarpDriver = false;
		if (!bUseWarpDriver)
		{
			unsigned long long MaxSize = 0;

			Microsoft::WRL::ComPtr<ID3D12Device> pDevice;
			ComPtr<IDXGIAdapter1> hardwareAdapter;
			for (uint32_t idx = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(idx, &hardwareAdapter); ++idx)
			{
				DXGI_ADAPTER_DESC1 desc;
				hardwareAdapter->GetDesc1(&desc);
				if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
					continue;

				if (desc.DedicatedVideoMemory > MaxSize&& SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice))))
				{
					hardwareAdapter->GetDesc1(&desc);
					Utility::Printf(L"D3D12-capable hardware found:  %s (%u MB)\n", desc.Description, desc.DedicatedVideoMemory >> 20);
					MaxSize = desc.DedicatedVideoMemory;
				}
			}

			if (MaxSize > 0)
				g_pD3D12Device = pDevice.Detach();
		}

		if (g_pD3D12Device == nullptr)
		{
			Microsoft::WRL::ComPtr<ID3D12Device> pDevice;
			ComPtr<IDXGIAdapter> warpAdapter;
			ASSERT_SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

			ASSERT_SUCCEEDED(D3D12CreateDevice(
				warpAdapter.Get(),
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(&pDevice)
			));
			g_pD3D12Device = pDevice.Detach();
		}


		/*
		* Describe and create the command queue.
		*/
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		ASSERT_SUCCEEDED(g_pD3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_commandQueue)));


		/*
		* D3D12 SwapChain Creation
		*/
		// Describe and create the swap chain.
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
		swapChainDesc.Width = m_DisplayWidth;
		swapChainDesc.Height = m_DisplayHeight;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

		ComPtr<IDXGISwapChain1> swapChain;
		ASSERT_SUCCEEDED(factory->CreateSwapChainForHwnd(
			g_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
			g_hwnd,
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapChain
		));

		// This sample does not support fullscreen transitions.
		ASSERT_SUCCEEDED(factory->MakeWindowAssociation(g_hwnd, DXGI_MWA_NO_ALT_ENTER));

		ASSERT_SUCCEEDED(swapChain.As(&g_pSwapChain));
		s_FrameIndex = g_pSwapChain->GetCurrentBackBufferIndex();

		// Create sync objects
		{
			ASSERT_SUCCEEDED(g_pD3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
			++m_fenceValue[s_FrameIndex];

			// Create an event handle to use for frame synchronization.
			m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (m_fenceEvent == nullptr)
			{
				ASSERT_SUCCEEDED(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = SWAP_CHAIN_BUFFER_COUNT;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ASSERT_SUCCEEDED(g_pD3D12Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvHeap)));
		m_rtvDescriptorSize = IGraphics::g_GraphicsCore->g_pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// Create frame resources.
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

			// Create a RTV for each frame.
			for (UINT n = 0; n < SWAP_CHAIN_BUFFER_COUNT; n++)
			{
				ASSERT_SUCCEEDED(g_pSwapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
				g_pD3D12Device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
				rtvHandle.Offset(1, m_rtvDescriptorSize);

				ASSERT_SUCCEEDED(g_pD3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator[n])));
			}
		}

		return;
		g_CommandManager->Create(g_pD3D12Device.Get());

		swapChainDesc.Width = m_DisplayWidth;
		swapChainDesc.Height = m_DisplayHeight;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.Scaling = DXGI_SCALING_NONE;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		//swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

		ComPtr<IDXGISwapChain1> s_swapChain1;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) // Win32
		ASSERT_SUCCEEDED(factory->CreateSwapChainForHwnd(
			g_CommandManager->GetCommandQueue(), 
			g_hwnd, 
			&swapChainDesc, 
			nullptr, 
			nullptr, 
			&s_swapChain1));
#else // UWP
		ASSERT_SUCCEEDED(dxgiFactory->CreateSwapChainForCoreWindow(g_CommandManager.GetCommandQueue(), (IUnknown*)GameCore::g_window.Get(), &swapChainDesc, nullptr, &s_SwapChain1));
#endif

		for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
		{
			ComPtr<ID3D12Resource> DisplayPlane;
			ASSERT_SUCCEEDED(s_swapChain1->GetBuffer(i, IID_PPV_ARGS(&DisplayPlane)));
			g_DisplayPlane[i].CreateFromSwapChain(L"Primary SwapChain Buffer", DisplayPlane.Detach());
		}

		// Common state was moved to GraphicsCommon.*
		InitializeCommonState();

		s_PresentRS.Reset(4, 2);
		s_PresentRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2);
		s_PresentRS[1].InitAsConstants(0, 6, D3D12_SHADER_VISIBILITY_ALL);
		s_PresentRS[2].InitAsBufferSRV(2, D3D12_SHADER_VISIBILITY_PIXEL);
		s_PresentRS[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
		s_PresentRS.InitStaticSampler(0, SamplerLinearClampDesc);
		s_PresentRS.InitStaticSampler(1, SamplerPointClampDesc);
		s_PresentRS.Finalize(L"Present");
	}

	void GraphicsCore::Terminate(void)
	{
		g_CommandManager->IdleGPU();
		//#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
		//		s_SwapChain1->SetFullscreenState(FALSE, nullptr);
		//#endif
	}

	void GraphicsCore::Shutdown()
	{
		WaitForGpu();

		CloseHandle(m_fenceEvent);

		g_CommandManager->Shutdown();

#if defined(_DEBUG)
		ID3D12DebugDevice* debugInterface;
		if (SUCCEEDED(g_pD3D12Device->QueryInterface(&debugInterface)))
		{
			debugInterface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
			debugInterface->Release();
		}
#endif
		SAFE_RELEASE(g_pD3D12Device);
		if (m_gcInstance) delete m_gcInstance;
	}

	void GraphicsCore::Present(void)
	{
		// TODO
	}

	void GraphicsCore::Resize(uint32_t width, uint32_t height)
	{
		// TODO
	}

	uint64_t GraphicsCore::GetFrameCount(void)
	{
		return s_FrameIndex;
	}

	float GraphicsCore::GetFrameTime(void)
	{
		return s_FrameTime;
	}


	// Sync functions
	void GraphicsCore::WaitForGpu()
	{
		// Schedule a Signal command in the queue
		ASSERT_SUCCEEDED(g_commandQueue->Signal(m_fence.Get(), m_fenceValue[s_FrameIndex]));

		// Wait Until the fence has been processed
		ASSERT_SUCCEEDED(m_fence->SetEventOnCompletion(m_fenceValue[s_FrameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

		// Increment the fence value for the current frame.
		++m_fenceValue[s_FrameIndex];
	}

	// PRepare to render the next frame.
	void GraphicsCore::MoveToNextFrame()
	{
		// Schedule a Singal command in the queue.
		const UINT64 currentFenceValue = m_fenceValue[s_FrameIndex];
		ASSERT_SUCCEEDED(g_commandQueue->Signal(m_fence.Get(), currentFenceValue));

		// Update the frame index;
		s_FrameIndex = g_pSwapChain->GetCurrentBackBufferIndex();

		// If the next frame is not ready to be rendered yet, wait until it is ready.
		if (m_fence->GetCompletedValue() < m_fenceValue[s_FrameIndex])
		{
			ASSERT_SUCCEEDED(m_fence->SetEventOnCompletion(m_fenceValue[s_FrameIndex], m_fenceEvent));
			WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
		}

		// Set the fence value for the next frame.
		m_fenceValue[s_FrameIndex] = currentFenceValue + 1;
	}

	// Temp Compute shader definitions
	void GraphicsCore::InitializeCS(void)
	{
		// Create compute shader resource
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
		SUCCEEDED(g_pD3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_computeCommandQueue)));

		SUCCEEDED(g_pD3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_computeCommandAllocator)));

		SUCCEEDED(g_pD3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_computeFence)));
		m_computeFenceValue = 1;

		// Create an event handle to use for frame synchronization.
		m_computeFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_computeFenceEvent == nullptr)
		{
			SUCCEEDED(HRESULT_FROM_WIN32(GetLastError()));
		}

		// CommandList
		SUCCEEDED(g_pD3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_computeCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_computeCommandList)));
		m_computeCommandList->Close();
	}

	void GraphicsCore::WaitForComputeShaderGpu()
	{
		// Signal and increment the fence value.
		const UINT64 fence = m_computeFenceValue;
		SUCCEEDED(m_computeCommandQueue->Signal(m_computeFence.Get(), fence));
		m_computeFenceValue++;

		// Wait until the previous frame is finished.
		if (m_computeFence->GetCompletedValue() < fence)
		{
			SUCCEEDED(m_computeFence->SetEventOnCompletion(fence, m_computeFenceEvent));
			WaitForSingleObject(m_computeFenceEvent, INFINITE);
		}
	}
}

// Old Implementation
//void TiledRendering::WaitForPreviousFrame()
//{
//    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
//    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
//    // sample illustrates how to use fences for efficient resource usage and to
//    // maximize GPU utilization.
//
//    // Signal and increment the fence value.
//    const UINT64 fence = m_fenceValue;
//    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
//    m_fenceValue++;
//
//    // Wait until the previous frame is finished.
//    if (m_fence->GetCompletedValue() < fence)
//    {
//        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
//        WaitForSingleObject(m_fenceEvent, INFINITE);
//    }
//
//    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
//}


