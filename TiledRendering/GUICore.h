#pragma once
#include <memory>
#include <unordered_map>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "DX12ResStruct.h"

class Win32FrameWork;
class GraphicsContext;
class DX12TextureConverter;

namespace IGuiCore
{
	using Microsoft::WRL::ComPtr;
	using namespace std;

	// Texture Resource
	enum SRVList
	{
		OpaqueLightGridSRV = 1,
		TestSRV = 2,
		TestSRV1 = 3
	};
	extern unordered_map<SRVList, DX12Resource> g_srvDict;


	extern Win32FrameWork* g_appPtr;
	extern DX12TextureConverter* g_imGuiTexConverter;

	extern bool g_bEnableGui;
	extern bool g_bShowMainMenuBar;



	void Init(Win32FrameWork* appPtr);
	void ShowImGUI();
	void RenderImGUI(GraphicsContext& gfxContext);
	void Terminate();

	void ShowMainMenuBar();
	void ShowMainGui();

	// Customized Functions
	void ShowForwardPlusWidgets();

	// Helper Functions
	void ShowAboutWindow(bool* p_open);
	void ShowMainMenuFile();
	void CreateGuiTexture2DSRV(wstring name, uint32_t width, uint32_t height,
		uint32_t elementSize, DXGI_FORMAT format, SRVList);
}

