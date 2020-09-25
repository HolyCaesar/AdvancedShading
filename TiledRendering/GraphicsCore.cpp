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

		g_CurrentBuffer = 0;

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
			else
				Utility::Print("WARNING:  Unable to enable D3D12 debug validation layer\n");
		}
#endif

		/*
		* D3D12 Device Creation
		*/
		ComPtr<IDXGIFactory4> factory;
		ASSERT_SUCCEEDED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

		//ComPtr<IDXGIAdapter3> testAdapter;
		//ASSERT_SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&testAdapter)));
		//DXGI_QUERY_VIDEO_MEMORY_INFO info;
		//testAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
		//UINT64 testRes = info.CurrentUsage;
		//testRes += 1;

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
#ifndef RELEASE
		else
		{
			bool DeveloperModeEnabled = false;

			// Look in the Windows Registry to determine if Developer Mode is enabled
			HKEY hKey;
			LSTATUS result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppModelUnlock", 0, KEY_READ, &hKey);
			if (result == ERROR_SUCCESS)
			{
				DWORD keyValue, keySize = sizeof(DWORD);
				result = RegQueryValueEx(hKey, L"AllowDevelopmentWithoutDevLicense", 0, NULL, (byte*)&keyValue, &keySize);
				if (result == ERROR_SUCCESS && keyValue == 1)
					DeveloperModeEnabled = true;
				RegCloseKey(hKey);
			}

			WARN_ONCE_IF_NOT(DeveloperModeEnabled, "Enable Developer Mode on Windows 10 to get consistent profiling results");

			// Prevent the GPU from overclocking or underclocking to get consistent timings
			if (DeveloperModeEnabled)
				g_pD3D12Device->SetStablePowerState(TRUE);
		}
#endif 

		//// We like to do read-modify-write operations on UAVs during post processing.  To support that, we
		//// need to either have the hardware do typed UAV loads of R11G11B10_FLOAT or we need to manually
		//// decode an R32_UINT representation of the same buffer.  This code determines if we get the hardware
		//// load support.
		//D3D12_FEATURE_DATA_D3D12_OPTIONS FeatureData = {};
		//if (SUCCEEDED(g_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &FeatureData, sizeof(FeatureData))))
		//{
		//	if (FeatureData.TypedUAVLoadAdditionalFormats)
		//	{
		//		D3D12_FEATURE_DATA_FORMAT_SUPPORT Support =
		//		{
		//			DXGI_FORMAT_R11G11B10_FLOAT, D3D12_FORMAT_SUPPORT1_NONE, D3D12_FORMAT_SUPPORT2_NONE
		//		};

		//		if (SUCCEEDED(g_Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &Support, sizeof(Support))) &&
		//			(Support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0)
		//		{
		//			g_bTypedUAVLoadSupport_R11G11B10_FLOAT = true;
		//		}

		//		Support.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

		//		if (SUCCEEDED(g_Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &Support, sizeof(Support))) &&
		//			(Support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0)
		//		{
		//			g_bTypedUAVLoadSupport_R16G16B16A16_FLOAT = true;
		//		}
		//	}
		//}

		g_CommandManager->Create(g_pD3D12Device.Get());

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = m_DisplayWidth;
		swapChainDesc.Height = m_DisplayHeight;
		//swapChainDesc.Format = SwapChainFormat;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.Scaling = DXGI_SCALING_NONE;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		//swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;


		ComPtr<IDXGISwapChain1> swapChain = nullptr;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) // Win32
		ASSERT_SUCCEEDED(factory->CreateSwapChainForHwnd(
			g_CommandManager->GetCommandQueue(),
			g_hwnd,
			&swapChainDesc,
			nullptr,
			nullptr,
			swapChain.GetAddressOf()));
#else // UWP
		ASSERT_SUCCEEDED(dxgiFactory->CreateSwapChainForCoreWindow(g_CommandManager.GetCommandQueue(), (IUnknown*)GameCore::g_window.Get(), &swapChainDesc, nullptr, &s_SwapChain1));
#endif


		//#if CONDITIONALLY_ENABLE_HDR_OUTPUT && defined(NTDDI_WIN10_RS2) && (NTDDI_VERSION >= NTDDI_WIN10_RS2)
		//	{
		//		IDXGISwapChain4* swapChain = (IDXGISwapChain4*)s_SwapChain1;
		//		ComPtr<IDXGIOutput> output;
		//		ComPtr<IDXGIOutput6> output6;
		//		DXGI_OUTPUT_DESC1 outputDesc;
		//		UINT colorSpaceSupport;
		//
		//		// Query support for ST.2084 on the display and set the color space accordingly
		//		if (SUCCEEDED(swapChain->GetContainingOutput(&output)) &&
		//			SUCCEEDED(output.As(&output6)) &&
		//			SUCCEEDED(output6->GetDesc1(&outputDesc)) &&
		//			outputDesc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 &&
		//			SUCCEEDED(swapChain->CheckColorSpaceSupport(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, &colorSpaceSupport)) &&
		//			(colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) &&
		//			SUCCEEDED(swapChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)))
		//		{
		//			g_bEnableHDROutput = true;
		//		}
		//	}
		//#endif

		if (swapChain == nullptr) return;

		ASSERT_SUCCEEDED(swapChain.As(&g_pSwapChain));
		s_FrameIndex = g_pSwapChain->GetCurrentBackBufferIndex();

		// This sample does not support fullscreen transitions.
		ASSERT_SUCCEEDED(factory->MakeWindowAssociation(g_hwnd, DXGI_MWA_NO_ALT_ENTER));

		for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
		{
			ComPtr<ID3D12Resource> DisplayPlane;
			ASSERT_SUCCEEDED(g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&DisplayPlane)));
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
		//WaitForGpu();
		CommandContext::DestroyAllContexts();
		DX12PSO::DestroyAll();
		DX12RootSignature::DestroyAll();
		DescriptorAllocator::DestroyAll();

		for (UINT i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
			g_DisplayPlane[i].Destroy();

		//CloseHandle(m_fenceEvent);
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
		//if (g_bEnableHDROutput)
		//	PreparePresentHDR();
		//else
		//	PreparePresentLDR();

		g_CurrentBuffer = (g_CurrentBuffer + 1) % SWAP_CHAIN_BUFFER_COUNT;

		//UINT PresentInterval = s_EnableVSync ? std::min(4, (int)Round(s_FrameTime * 60.0f)) : 0;

		//s_SwapChain1->Present(PresentInterval, 0);
		g_pSwapChain->Present(0, 0);

		// Test robustness to handle spikes in CPU time
		//if (s_DropRandomFrames)
		//{
		//    if (std::rand() % 25 == 0)
		//        BusyLoopSleep(0.010);
		//}

		//int64_t CurrentTick = SystemTime::GetCurrentTick();

		//if (s_EnableVSync)
		//{
		//	// With VSync enabled, the time step between frames becomes a multiple of 16.666 ms.  We need
		//	// to add logic to vary between 1 and 2 (or 3 fields).  This delta time also determines how
		//	// long the previous frame should be displayed (i.e. the present interval.)
		//	s_FrameTime = (s_LimitTo30Hz ? 2.0f : 1.0f) / 60.0f;
		//	if (s_DropRandomFrames)
		//	{
		//		if (std::rand() % 50 == 0)
		//			s_FrameTime += (1.0f / 60.0f);
		//	}
		//}
		//else
		//{
		//	// When running free, keep the most recent total frame time as the time step for
		//	// the next frame simulation.  This is not super-accurate, but assuming a frame
		//	// time varies smoothly, it should be close enough.
		//	s_FrameTime = (float)SystemTime::TimeBetweenTicks(s_FrameStartTick, CurrentTick);
		//}

		//s_FrameStartTick = CurrentTick;

		++s_FrameIndex;
		//TemporalEffects::Update((uint32_t)s_FrameIndex);

		//SetNativeResolution();
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


	//// Sync functions
	//void GraphicsCore::WaitForGpu()
	//{
	//	// Schedule a Signal command in the queue
	//	ASSERT_SUCCEEDED(g_commandQueue->Signal(m_fence.Get(), m_fenceValue[s_FrameIndex]));

	//	// Wait Until the fence has been processed
	//	ASSERT_SUCCEEDED(m_fence->SetEventOnCompletion(m_fenceValue[s_FrameIndex], m_fenceEvent));
	//	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	//	// Increment the fence value for the current frame.
	//	++m_fenceValue[s_FrameIndex];
	//}

	//// PRepare to render the next frame.
	//void GraphicsCore::MoveToNextFrame()
	//{
	//	// Schedule a Singal command in the queue.
	//	const UINT64 currentFenceValue = m_fenceValue[s_FrameIndex];
	//	ASSERT_SUCCEEDED(g_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	//	// Update the frame index;
	//	s_FrameIndex = g_pSwapChain->GetCurrentBackBufferIndex();

	//	// If the next frame is not ready to be rendered yet, wait until it is ready.
	//	if (m_fence->GetCompletedValue() < m_fenceValue[s_FrameIndex])
	//	{
	//		ASSERT_SUCCEEDED(m_fence->SetEventOnCompletion(m_fenceValue[s_FrameIndex], m_fenceEvent));
	//		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	//	}

	//	// Set the fence value for the next frame.
	//	m_fenceValue[s_FrameIndex] = currentFenceValue + 1;
	//}
}
