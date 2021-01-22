#pragma once
#include <memory>
#include <unordered_map>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "DX12ResStruct.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "Utility.h"

#include "TextureConverterEnum.hlsli"

class Win32FrameWork;
class GraphicsContext;

using Microsoft::WRL::ComPtr;
using namespace std;

class DX12TextureConverter
{
public:
	DX12TextureConverter();
	~DX12TextureConverter();

	void CleanUp();
	void Convert(GraphicsContext& gfxContext);

	void AddInputRes(
		string name, uint32_t width, uint32_t height, 
		DXGI_FORMAT format, GpuResource* input,
		bool enable = false);

	void Resize(uint32_t width, uint32_t height);

	HRESULT Finalize();

	DX12Resource* GetOutputResSRV(string name)
	{
		return &m_outputRes[name];
	}

private:
	CD3DX12_VIEWPORT	m_viewport;
	CD3DX12_RECT		m_scissorRect;

	DX12RootSignature	m_sceneRootSignature;
	unordered_map<string, GraphicsPSO> m_psoContainer;

	unordered_map<string, GpuResource*> m_inputRes;
	unordered_map<string, DX12Resource> m_outputRes;

	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	uint32_t m_rtvHeapPtr;
	uint32_t m_rtvDescriptorSize;

private:
	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT3 color;
		XMFLOAT2 uv;
	};

	__declspec(align(16)) struct CBuffer
	{
		XMMATRIX worldMatrix;
		XMMATRIX viewMatrix;
		XMMATRIX worldViewProjMatrix;
	};

	StructuredBuffer m_vertexBuffer;
	StructuredBuffer m_indexBuffer;

	CBuffer m_constantBufferData;

private:
	void CreateTex2DResources(
		string name, uint32_t width, uint32_t height,
		DXGI_FORMAT format, DX12Resource* pResource);
};

namespace IGuiCore
{
	extern Win32FrameWork* g_appPtr;
	extern DX12TextureConverter* g_imGuiTexConverter;


	extern bool g_bEnableGui;
	extern bool g_bShowMainMenuBar;

	void Init(Win32FrameWork* appPtr);
	void ShowImGUI();
	void RenderImGUI(GraphicsContext& gfxContext);
	void Terminate();

	// Main GUI entry
	void ShowMainGui();

	// Customized Functions
	void LightControlWidget();
	void ShadingTechWidget();
	void ShowCpuProfilerWindow();

	void ShowForwardTechWidget();
	void ShowDeferredTechWidget();
	void ShowGeneralTechWidget();

	// Helper Functions
	void ShowAboutWindow(bool* p_open);
	void ShowMainMenuFile();
	void ShowMainMenuBar();
}
