#include "stdafx.h"
#include "VectorMath.h"
#include "GraphicsCore.h"
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

				if (desc.DedicatedVideoMemory > MaxSize && SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice))))
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
	}

	void GraphicsCore::Terminate(void)
	{
//		g_CommandManager.IdleGPU();
//#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
//		s_SwapChain1->SetFullscreenState(FALSE, nullptr);
//#endif
	}

	void GraphicsCore::Shutdown()
	{
#if defined(_DEBUG)
		ID3D12DebugDevice* debugInterface;
		if (SUCCEEDED(g_pD3D12Device->QueryInterface(&debugInterface)))
		{
			debugInterface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
			debugInterface->Release();
		}
#endif

		SAFE_RELEASE(g_pD3D12Device);
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
}
