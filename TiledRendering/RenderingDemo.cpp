#include "stdafx.h"
#include "RenderingDemo.h"
#include "RenderPass.h"
#include "Geometry.h"

default_random_engine defEngine(time(0));

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void RenderingDemo::WinMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam);

	m_modelViewCamera.HandleMessages(hWnd, message, wParam, lParam);
}

RenderingDemo::RenderingDemo(UINT width, UINT height, std::wstring name) :
	Win32FrameWork(width, height, name),
	m_pCbvDataBegin(nullptr),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
	m_constantBufferData{}
{
	m_vertexBuffer = make_shared<StructuredBuffer>();
	m_indexBuffer = make_shared<StructuredBuffer>();

	m_renderingOption = GeneralRenderingOption;
}



void RenderingDemo::OnInit()
{
	m_cpuProfiler.AddTimeStamp("LoadPipelineCPU");
	LoadPipeline();
	m_cpuProfiler.EndTimeStamp("LoadPipelineCPU");

	m_cpuProfiler.AddTimeStamp("LoadAssets");
	LoadAssets();
	m_cpuProfiler.EndTimeStamp("LoadAssets");
}

// Load the rendering pipeline dependencies.
void RenderingDemo::LoadPipeline()
{
	UINT dxgiFactoryFlags = 0;

	IGraphics::g_GraphicsCore->g_hwnd = Win32Application::GetHwnd();
	IGraphics::g_GraphicsCore->Initialize();
	IGuiCore::Init(this);

	m_gpuProfiler.Initialize();
}

// Load the sample assets.
void RenderingDemo::LoadAssets()
{
	// Create a root signature consisting of a descriptor table with a single CBV
	{
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		//D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

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

		m_sceneOpaqueRootSignature.Reset(e_numRootParameters, 1);
		m_sceneOpaqueRootSignature.InitStaticSampler(0, non_static_sampler);
		//m_testRootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
		//m_sceneOpaqueRootSignature[e_rootParameterCB].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, e_cCB, D3D12_SHADER_VISIBILITY_ALL);
		m_sceneOpaqueRootSignature[e_rootParameterCB].InitAsConstantBuffer(0);
		m_sceneOpaqueRootSignature[e_ModelTexRootParameterSRV].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
		m_sceneOpaqueRootSignature[e_LightGridRootParameterSRV].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, D3D12_SHADER_VISIBILITY_PIXEL);
		m_sceneOpaqueRootSignature[e_LightIndexRootParameterSRV].InitAsBufferSRV(2);
		m_sceneOpaqueRootSignature[e_LightBufferRootParameterSRV].InitAsBufferSRV(3);
		m_sceneOpaqueRootSignature.Finalize(L"SceneRootSignature", rootSignatureFlags);
	}

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;
		ComPtr<ID3DBlob> pixelShaderDepthPass;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

#if defined(_DEBUG)
		ComPtr<ID3DBlob> errorMessages;
		HRESULT hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_1", compileFlags, 0, &vertexShader, &errorMessages);
		if (FAILED(hr) && errorMessages)
		{
			const char* errorMsg = (const char*)errorMessages->GetBufferPointer();
			//MessageBox(nullptr, errorMsg, L"Shader Compilation Error", MB_RETRYCANCEL);
			wstring str;
			for (int i = 0; i < 150; i++) str += errorMsg[i];
			MessageBox(nullptr, str.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
			exit(0);
		}
		errorMessages.Reset();
		errorMessages = nullptr;

		hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_1", compileFlags, 0, &pixelShader, &errorMessages);
		if (FAILED(hr) && errorMessages)
		{
			const char* errorMsg = (const char*)errorMessages->GetBufferPointer();
			//MessageBox(nullptr, errorMsg, L"Shader Compilation Error", MB_RETRYCANCEL);
			wstring str;
			for (int i = 0; i < 150; i++) str += errorMsg[i];
			MessageBox(nullptr, str.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
			exit(0);
		}
		errorMessages.Reset();
		errorMessages = nullptr;

		hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS_SceneDepth", "ps_5_1", compileFlags, 0, &pixelShaderDepthPass, &errorMessages);
		if (FAILED(hr) && errorMessages)
		{
			const char* errorMsg = (const char*)errorMessages->GetBufferPointer();
			//MessageBox(nullptr, errorMsg, L"Shader Compilation Error", MB_RETRYCANCEL);
			wstring str;
			for (int i = 0; i < 150; i++) str += errorMsg[i];
			MessageBox(nullptr, str.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
			exit(0);
		}

#else
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_1", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_1", compileFlags, 0, &pixelShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS_SceneDepth", "ps_5_1", compileFlags, 0, &pixelShaderDepthPass, nullptr));
#endif

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			//{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		m_scenePSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
		m_scenePSO.SetRootSignature(m_sceneOpaqueRootSignature);
		m_scenePSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(vertexShader.Get()));
		m_scenePSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(pixelShader.Get()));
		m_scenePSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
		m_scenePSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
		m_scenePSO.SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(TRUE, D3D12_DEPTH_WRITE_MASK_ALL,
			D3D12_COMPARISON_FUNC_LESS, TRUE, 0xFF, 0xFF,
			D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_INCR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS,
			D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_DECR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS));
		m_scenePSO.SetSampleMask(UINT_MAX);
		m_scenePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_scenePSO.SetRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
		m_scenePSO.Finalize();
	}

	// Create the vertex buffer.
	m_pModel = make_shared<Model>();
	{
		m_pModel->Load("bunny.obj");
		m_vertexBuffer->Create(L"BunnyVertexBuffer", m_pModel->m_vecVertexData.size(), sizeof(Vertex), m_pModel->m_vecVertexData.data());
		m_indexBuffer->Create(L"BunnyIndexBuffer", m_pModel->m_vecIndexData.size(), sizeof(uint32_t), m_pModel->m_vecIndexData.data());
		m_modelTexture.Create(L"RabbitTexture", TextureWidth, TextureHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
	}

	// Create Depth Buffer
	m_sceneDepthBuffer.Create(L"FinalSceneDepthPass", m_width, m_height, DXGI_FORMAT_D32_FLOAT);

	// Camera Setup
	{
		// Setup the camera's view parameters
		static const XMVECTORF32 s_Eye = { 0.0f, 0.0f, -10.0f, 0.f };
		static const XMVECTORF32 s_At = { 0.0f, 0.0f, 0.0f, 0.f };
		m_modelViewCamera.SetViewParams(s_Eye, s_At);
		// Setup the camera's projection parameters
		float fAspectRatio = m_width / (float)m_height;
		m_modelViewCamera.SetProjParams(XM_PI / 4, fAspectRatio, 0.01f, 10000.0f);
		m_modelViewCamera.SetWindow(m_width, m_height);
		m_modelViewCamera.SetButtonMasks(MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON);
	}

	// Generate lights
	m_Lights = make_shared<StructuredBuffer>();
	GenerateLights(1);

	// General Shading Technique
	LoadGeneralShadingTech("GeneralShadingTechnique");
	LoadDefferredShadingTech("DefferredShadingTechnique");
	LoadTiledForwardShadingTech("TiledShadingTechnique");

	// Gui Resource allocation
	// Must invoked after LoadGeneralShadingTech, LoadDefferredShadingTech, and LoadTiledForwardShadingTech
	// due to the function depandency
	// TODO:
	IGuiCore::g_imGuiTexConverter->AddInputRes(
		"SceneDepthView", 
		m_width, m_height, 
		DXGI_FORMAT_D32_FLOAT, 
		dynamic_pointer_cast<DX12ShadingPass>(m_generalRenderingTech.GetPass(0))->GetDepthBuffer().get());
	ThrowIfFailed(IGuiCore::g_imGuiTexConverter->Finalize());
}

void RenderingDemo::LoadGeneralShadingTech(string name)
{
	m_generalRenderingTech.SetName(name);

	// Initialize a default render pass for general rendering technique
	shared_ptr<DX12ShadingPass> generalPass(new DX12ShadingPass(),
		[](DX12ShadingPass* ptr) { ptr->Destroy(); });

	// RootSignature
	shared_ptr<DX12RootSignature> generalShadingRS = make_shared<DX12RootSignature>();

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
	//D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

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

	UINT numRootParameters(2), numRootParamIdx(0), numSampler(1);
	generalShadingRS->Reset(numRootParameters, numSampler);
	generalShadingRS->InitStaticSampler(0, non_static_sampler);
	(*generalShadingRS)[numRootParamIdx++].InitAsConstantBuffer(0);
	(*generalShadingRS)[numRootParamIdx++].InitAsBufferSRV(0);
	wstring rsName(name.begin(), name.end());
	generalShadingRS->Finalize(rsName.c_str(), rootSignatureFlags);


	// Shaders and pipline 
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	wstring shaderName = L"GeneralShading.hlsl";

	ShaderMacros nullShaderMacros;
	// Load vertex shader
	vertexShader = DX12Aux::LoadShaderFromFile(ShaderType::VertexShader, "GeneralShading", "VSMain", L"GeneralShading.hlsl", nullShaderMacros, "vs_5_1");
	// Load pixel shader
	pixelShader = DX12Aux::LoadShaderFromFile(ShaderType::PixelShader, "GeneralShading", "PSMain", L"GeneralShading.hlsl", nullShaderMacros, "ps_5_1");



	// Define the vertex input layout.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	shared_ptr<GraphicsPSO> generalShadingPSO = make_shared<GraphicsPSO>();
	generalShadingPSO->SetRootSignature(*generalShadingRS);
	generalShadingPSO->SetInputLayout(_countof(inputElementDescs), inputElementDescs);
	generalShadingPSO->SetVertexShader(CD3DX12_SHADER_BYTECODE(vertexShader.Get()));
	generalShadingPSO->SetPixelShader(CD3DX12_SHADER_BYTECODE(pixelShader.Get()));
	generalShadingPSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	generalShadingPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	generalShadingPSO->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(TRUE, D3D12_DEPTH_WRITE_MASK_ALL,
		D3D12_COMPARISON_FUNC_LESS, TRUE, 0xFF, 0xFF,
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_INCR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS,
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_DECR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS));
	generalShadingPSO->SetSampleMask(UINT_MAX);
	generalShadingPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	generalShadingPSO->SetRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
	//generalShadingPSO->SetRenderTargetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_D32_FLOAT);
	generalShadingPSO->Finalize();

	// Add shaders to the renderpass.	(Although it is not nessary to store shaders because they will bind to PSO
	generalPass->AddShader("GeneralShading_VS", ShaderType::VertexShader, vertexShader);
	generalPass->AddShader("GeneralShading_PS", ShaderType::PixelShader, pixelShader);

	generalPass->FinalizeRootSignature(generalShadingRS);
	generalPass->FinalizePSO(generalShadingPSO);

	// Add Buffers
	generalPass->AddConstantBuffer(0, L"GeneralConstBuffer", sizeof(m_constantBufferData), &m_constantBufferData);
	generalPass->AddStructuredBufferSRV(1, L"GeneralLightBuffer", m_Lights);

	// Use buack buffer of the swap chain
	generalPass->SetEnableOwnRenderTarget(false);
	generalPass->SetDepthBuffer(L"GeneralLightDepthBuf", m_width, m_height, DXGI_FORMAT_D32_FLOAT);
	//generalPass->SetDepthBuffer(m_sceneDepthBuffer);

	generalPass->SetVertexBuffer(m_vertexBuffer);
	generalPass->SetIndexBuffer(m_indexBuffer);

	generalPass->SetViewPortAndScissorRect(m_viewport, m_scissorRect);

	generalPass->AddGpuProfiler(&m_gpuProfiler);
	generalPass->SetGPUQueryStatus(true);

	generalPass->SetName("GeneralShading");
	m_generalRenderingTech.AddPass(generalPass);
}

void RenderingDemo::LoadDefferredShadingTech(string name)
{
	m_deferredRenderingTech.SetName(name);

	// RootSignature
	shared_ptr<DX12RootSignature> rs = make_shared<DX12RootSignature>();

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
	//D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	// Define non-static sampler
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

	// Root signature for deferred phase
	UINT numRootParameters(2), numRootParamIdx(0), numSampler(1);
	rs->Reset(numRootParameters, numSampler);
	rs->InitStaticSampler(0, non_static_sampler);
	(*rs)[numRootParamIdx++].InitAsConstantBuffer(0);
	(*rs)[numRootParamIdx++].InitAsBufferSRV(0);
	wstring rsName(name.begin(), name.end());
	rsName += L"DeferredPhase";
	rs->Finalize(rsName.c_str(), rootSignatureFlags);

	// Root signature for rendering phase
	shared_ptr<DX12RootSignature> rs_render = make_shared<DX12RootSignature>();
	numRootParameters = 8, numRootParamIdx = 0, numSampler = 1;
	rs_render->Reset(numRootParameters, numSampler);
	rs_render->InitStaticSampler(0, non_static_sampler);
	(*rs_render)[numRootParamIdx++].InitAsConstantBuffer(0);
	(*rs_render)[numRootParamIdx++].InitAsConstantBuffer(1);
	(*rs_render)[numRootParamIdx++].InitAsBufferSRV(0);
	(*rs_render)[numRootParamIdx++].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, D3D12_SHADER_VISIBILITY_PIXEL); // LightAccumulation texture
	(*rs_render)[numRootParamIdx++].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1, D3D12_SHADER_VISIBILITY_PIXEL); // Diffuse texture
	(*rs_render)[numRootParamIdx++].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 1, D3D12_SHADER_VISIBILITY_PIXEL); // Specular texture
	(*rs_render)[numRootParamIdx++].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 1, D3D12_SHADER_VISIBILITY_PIXEL); // normal texture
	(*rs_render)[numRootParamIdx++].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 1, D3D12_SHADER_VISIBILITY_PIXEL); // normal texture
	wstring rsNameRendering(name.begin(), name.end());
	rsNameRendering += L"RenderingPhase";
	rs_render->Finalize(rsNameRendering.c_str(), rootSignatureFlags);



	// Shaders and pipline
	ComPtr<ID3DBlob> vertexShader, renderVertexShader;
	ComPtr<ID3DBlob> deferredPixelShader;
	ComPtr<ID3DBlob> renderPixelShader;

#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	wstring shaderName = L"DeferredShading.hlsl";

	ShaderMacros nullShaderMacros;
	// Load vertex shader
	vertexShader = DX12Aux::LoadShaderFromFile(ShaderType::VertexShader, "DeferredShading", "VSMain", L"DeferredShading.hlsl", nullShaderMacros, "vs_5_1");
	// Load vertex shader for rendering
	renderVertexShader = DX12Aux::LoadShaderFromFile(ShaderType::VertexShader, "DeferredShading", "VSMainRendering", L"DeferredShading.hlsl", nullShaderMacros, "vs_5_1");
	// Load pixel shader for deferred phase
	deferredPixelShader = DX12Aux::LoadShaderFromFile(ShaderType::PixelShader, "DeferredShading", "PSGeometry", L"DeferredShading.hlsl", nullShaderMacros, "ps_5_1");
	// Load pixel shader for rendering phase
	renderPixelShader = DX12Aux::LoadShaderFromFile(ShaderType::PixelShader, "DeferredShading", "PSMain", L"DeferredShading.hlsl", nullShaderMacros, "ps_5_1");

	// Define the vertex input layout.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	shared_ptr<GraphicsPSO> deferredPhasePSO = make_shared<GraphicsPSO>();
	deferredPhasePSO->SetRootSignature(*rs);
	deferredPhasePSO->SetInputLayout(_countof(inputElementDescs), inputElementDescs);
	deferredPhasePSO->SetVertexShader(CD3DX12_SHADER_BYTECODE(vertexShader.Get()));
	deferredPhasePSO->SetPixelShader(CD3DX12_SHADER_BYTECODE(deferredPixelShader.Get()));
	deferredPhasePSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	deferredPhasePSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	deferredPhasePSO->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(TRUE, D3D12_DEPTH_WRITE_MASK_ALL,
		D3D12_COMPARISON_FUNC_LESS, TRUE, 0xFF, 0xFF,
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_INCR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS,
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_DECR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS));
	deferredPhasePSO->SetSampleMask(UINT_MAX);
	deferredPhasePSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	//deferredPhasePSO->SetRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
	DXGI_FORMAT rtvFormats[] = { DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT };
	deferredPhasePSO->SetRenderTargetFormats(4, rtvFormats, DXGI_FORMAT_D32_FLOAT);
	deferredPhasePSO->Finalize();

	shared_ptr<GraphicsPSO> renderPhasePSO = make_shared<GraphicsPSO>();
	renderPhasePSO->SetRootSignature(*rs_render);
	renderPhasePSO->SetInputLayout(_countof(inputElementDescs), inputElementDescs);
	renderPhasePSO->SetVertexShader(CD3DX12_SHADER_BYTECODE(renderVertexShader.Get()));
	renderPhasePSO->SetPixelShader(CD3DX12_SHADER_BYTECODE(renderPixelShader.Get()));
	renderPhasePSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	renderPhasePSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	renderPhasePSO->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(TRUE, D3D12_DEPTH_WRITE_MASK_ALL,
		D3D12_COMPARISON_FUNC_LESS, TRUE, 0xFF, 0xFF,
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_INCR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS,
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_DECR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS));
	renderPhasePSO->SetSampleMask(UINT_MAX);
	renderPhasePSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	renderPhasePSO->SetRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
	renderPhasePSO->Finalize();

	// Configure a pass for deferred phase
	shared_ptr<DX12ShadingPass> deferredPass(new DX12ShadingPass(),
		[](DX12ShadingPass* ptr) { ptr->Destroy(); });

	deferredPass->AddShader("DeferredPass_VS", ShaderType::VertexShader, vertexShader);
	deferredPass->AddShader("DeferredPass_PS", ShaderType::PixelShader, deferredPixelShader);

	deferredPass->FinalizeRootSignature(rs);
	deferredPass->FinalizePSO(deferredPhasePSO);

	// Add Buffers
	deferredPass->AddConstantBuffer(0, L"DeferredPhaseConstBuffer", sizeof(m_constantBufferData), &m_constantBufferData);
	deferredPass->AddStructuredBufferSRV(1, L"LightBuffer", m_Lights);

	// Use buack buffer of the swap chain
	deferredPass->SetEnableOwnRenderTarget(true);

	shared_ptr<ColorBuffer> lightAccumulationTex2D(new ColorBuffer(),
		[](ColorBuffer* ptr) {ptr->Destroy(); });
	lightAccumulationTex2D->Create(L"LightAccumulationTex2D", m_width, m_height, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	shared_ptr<ColorBuffer> diffuseTex2D(new ColorBuffer(),
		[](ColorBuffer* ptr) {ptr->Destroy(); });
	diffuseTex2D->Create(L"DiffuseTex2D", m_width, m_height, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	shared_ptr<ColorBuffer> specularTex2D(new ColorBuffer(),
		[](ColorBuffer* ptr) {ptr->Destroy(); });
	specularTex2D->Create(L"SpecularTex2D", m_width, m_height, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	shared_ptr<ColorBuffer> normalVSTex2D(new ColorBuffer(),
		[](ColorBuffer* ptr) {ptr->Destroy(); });
	normalVSTex2D->Create(L"normalVSTex2D", m_width, m_height, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);

	deferredPass->AddRenderTarget(lightAccumulationTex2D, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	deferredPass->AddRenderTarget(diffuseTex2D, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	deferredPass->AddRenderTarget(specularTex2D, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	deferredPass->AddRenderTarget(normalVSTex2D, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	deferredPass->SetDepthBuffer(L"DeferredPhaseLightDepBuf", m_width, m_height, DXGI_FORMAT_D32_FLOAT);

	deferredPass->AddConstantBuffer(0, L"GeneralConstBuffer", sizeof(m_constantBufferData), &m_constantBufferData);

	deferredPass->SetVertexBuffer(m_vertexBuffer);
	deferredPass->SetIndexBuffer(m_indexBuffer);

	deferredPass->SetViewPortAndScissorRect(m_viewport, m_scissorRect);

	deferredPass->AddGpuProfiler(&m_gpuProfiler);
	deferredPass->SetGPUQueryStatus(true);

	deferredPass->SetName("DeferredPass");

	// Configure a pass for deferred rendering phase
	shared_ptr<DX12ShadingPass> renderPass(new DX12ShadingPass(),
		[](DX12ShadingPass* ptr) { ptr->Destroy(); });

	renderPass->AddShader("DeferredRenderPass_VS", ShaderType::VertexShader, renderVertexShader);
	renderPass->AddShader("DeferredRenderPass_PS", ShaderType::PixelShader, renderPixelShader);

	renderPass->FinalizeRootSignature(rs_render);
	renderPass->FinalizePSO(renderPhasePSO);

	// Add Buffers
	renderPass->AddConstantBuffer(0, L"GeneralConstBuffer", sizeof(m_constantBufferData), &m_constantBufferData);
	renderPass->AddConstantBuffer(1, L"ScreenToViewConstBuffer", sizeof(m_screenToViewParamsData), &m_screenToViewParamsData);
	renderPass->AddStructuredBufferSRV(2, L"LightBuffer", m_Lights);
	renderPass->AddColorBufferSRV(3, L"LightAccumulationTex2D", lightAccumulationTex2D);
	renderPass->AddColorBufferSRV(4, L"DiffuseTex2D", diffuseTex2D);
	renderPass->AddColorBufferSRV(5, L"SpecularTex2D", specularTex2D);
	renderPass->AddColorBufferSRV(6, L"NormalVSTex2D", normalVSTex2D);
	renderPass->AddDepthBufferSRV(7, L"DepthMap", deferredPass->GetDepthBuffer());

	// Use buack buffer of the swap chain
	renderPass->SetEnableOwnRenderTarget(false);
	// TODO need to add own render target
	//deferredPass->AddRenderTarget(m_scene)
	renderPass->SetDepthBuffer(L"DeferredRenderPhaseLightDepBuf", m_width, m_height, DXGI_FORMAT_D32_FLOAT);

	
	Square bgSquare;
	bgSquare.SetLength(2);
	vector<Vertex> bgSquareVertices;
	auto& bgVertices = bgSquare.GetVertices();
	auto& bgIndices = bgSquare.GetIndices();
	auto& bgTexCoords = bgSquare.GetTexCoords();
	for (int i = 0; i < bgVertices.size(); i++)
	{
		Vertex v;
		v.color = XMFLOAT3(0.0f, 0.0f, 0.0f);
		v.position = bgVertices[i];
		v.uv = bgTexCoords[i];
		bgSquareVertices.push_back(v);
	}
	shared_ptr<StructuredBuffer> bgVertexBuffer(new StructuredBuffer(),
		[](StructuredBuffer* ptr) {ptr->Destroy(); });
	bgVertexBuffer->Create(L"SquareBGVertices", bgSquareVertices.size(), sizeof(Vertex), bgSquareVertices.data());

	shared_ptr<StructuredBuffer> bgIndicesBuffer(new StructuredBuffer(),
		[](StructuredBuffer* ptr) {ptr->Destroy(); });
	bgIndicesBuffer->Create(L"SquareBGIndices", bgIndices.size(), sizeof(UINT), bgIndices.data());

	renderPass->SetVertexBuffer(bgVertexBuffer);
	renderPass->SetIndexBuffer(bgIndicesBuffer);

	renderPass->SetViewPortAndScissorRect(m_viewport, m_scissorRect);

	renderPass->AddGpuProfiler(&m_gpuProfiler);
	renderPass->SetGPUQueryStatus(true);

	renderPass->SetName("DeferredRenderPass");

	// Add all pass to this render technique
	m_deferredRenderingTech.AddPass(deferredPass);
	m_deferredRenderingTech.AddPass(renderPass);
}

void RenderingDemo::LoadTiledForwardShadingTech(string name)
{
	m_tiledForwardRenderingTech.SetName(name);

	// Initialize parameters
	m_TiledSize = 16; // This is a default value. TODO: need a function to update this parameter
	m_BlockSizeX = ceil(m_width * 1.0f / m_TiledSize);
	m_BlockSizeY = ceil(m_height * 1.0f / m_TiledSize);

	// Constant buffer for frustum culling
	m_dispatchParamsDataFrustum.numThreadGroups = XMUINT3(ceil(1.0f * m_BlockSizeX / m_TiledSize), ceil(1.0f * m_BlockSizeY / m_TiledSize), 1);
	m_dispatchParamsDataFrustum.numThreads = XMUINT3(m_BlockSizeX, m_BlockSizeY, 1);
	m_dispatchParamsDataFrustum.blockSize = XMUINT2(m_TiledSize, m_TiledSize);
	m_dispatchParamsDataFrustum.padding1 = 1;
	m_dispatchParamsDataFrustum.padding2 = 1;
	m_dispatchParamsDataFrustum.padding3 = XMUINT2(1, 1);

	// Constant buffer for light culling
	m_dispatchParamsDataLightCulling.numThreadGroups = XMUINT3(m_BlockSizeX, m_BlockSizeY, 1);
	m_dispatchParamsDataLightCulling.numThreads = XMUINT3(m_BlockSizeX, m_BlockSizeY, 1);
	m_dispatchParamsDataLightCulling.blockSize = XMUINT2(m_TiledSize, m_TiledSize);
	m_dispatchParamsDataLightCulling.padding1 = 1;
	m_dispatchParamsDataLightCulling.padding2 = 1;
	m_dispatchParamsDataLightCulling.padding3 = XMUINT2(1, 1);

	m_screenToViewParamsData.InverseProjection = XMMatrixInverse(nullptr, m_modelViewCamera.GetProjMatrix());
	m_screenToViewParamsData.ViewMatrix = XMMatrixIdentity();
	m_screenToViewParamsData.ScreenDimensions = XMUINT2(m_width, m_height);

	// Create passes
	shared_ptr<DX12ShadingPass> sceneDepthPass(new DX12ShadingPass(),
		[](DX12ShadingPass* ptr) { ptr->Destroy(); });
	shared_ptr<DX12ComputePass> frustumPass(new DX12ComputePass(),
		[](DX12ComputePass* ptr) { ptr->Destroy(); });
	shared_ptr<DX12ComputePass> lightCullingPass(new DX12ComputePass(),
		[](DX12ComputePass* ptr) { ptr->Destroy(); });
	shared_ptr<DX12ShadingPass> renderPass(new DX12ShadingPass(),
		[](DX12ShadingPass* ptr) { ptr->Destroy(); });

	// RootSignature
	shared_ptr<DX12RootSignature> rs_depthPass = make_shared<DX12RootSignature>();
	shared_ptr<DX12RootSignature> rs_frustumPass = make_shared<DX12RootSignature>();
	shared_ptr<DX12RootSignature> rs_lightCullingPass = make_shared<DX12RootSignature>();
	shared_ptr<DX12RootSignature> rs_renderPass = make_shared<DX12RootSignature>();

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
	//D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	// Define non-static sampler
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

	// Root signature for scene depth phase
	UINT numRootParameters(1), numRootParamIdx(0), numSampler(0);
	rs_depthPass->Reset(numRootParameters, numSampler);
	(*rs_depthPass)[numRootParamIdx++].InitAsConstantBuffer(0);
	wstring rsNameRendering(name.begin(), name.end());
	rsNameRendering += L"SceneDepthPass";
	rs_depthPass->Finalize(rsNameRendering.c_str(), rootSignatureFlags);

	// Root signature for grid frustum calculation pass 
	numRootParameters = 4, numRootParamIdx = 0, numSampler = 0;
	rs_frustumPass->Reset(numRootParameters, numSampler);
	(*rs_frustumPass)[numRootParamIdx++].InitAsConstantBuffer(0);
	(*rs_frustumPass)[numRootParamIdx++].InitAsConstantBuffer(1);
	(*rs_frustumPass)[numRootParamIdx++].InitAsBufferUAV(0);
	(*rs_frustumPass)[numRootParamIdx++].InitAsBufferUAV(1);
	rsNameRendering.resize(name.size());
	rsNameRendering += L"GridFrustumPass";
	rs_frustumPass->Finalize(rsNameRendering.c_str(), rootSignatureFlags);

	// Root signature for light culling calculation pass 
	numRootParameters = 13, numRootParamIdx = 0, numSampler = 0;
	rs_lightCullingPass->Reset(numRootParameters, numSampler);
	(*rs_lightCullingPass)[numRootParamIdx++].InitAsConstantBuffer(0);
	(*rs_lightCullingPass)[numRootParamIdx++].InitAsConstantBuffer(1);
	(*rs_lightCullingPass)[numRootParamIdx++].InitAsBufferUAV(0);
	(*rs_lightCullingPass)[numRootParamIdx++].InitAsBufferUAV(1);
	(*rs_lightCullingPass)[numRootParamIdx++].InitAsBufferUAV(2);
	(*rs_lightCullingPass)[numRootParamIdx++].InitAsBufferUAV(3);
	(*rs_lightCullingPass)[numRootParamIdx++].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 1);
	(*rs_lightCullingPass)[numRootParamIdx++].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5, 1);
	(*rs_lightCullingPass)[numRootParamIdx++].InitAsBufferSRV(0);
	(*rs_lightCullingPass)[numRootParamIdx++].InitAsBufferSRV(1);
	(*rs_lightCullingPass)[numRootParamIdx++].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);
	(*rs_lightCullingPass)[numRootParamIdx++].InitAsBufferUAV(6);
	(*rs_lightCullingPass)[numRootParamIdx++].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 7, 1);
	rsNameRendering.resize(name.size());
	rsNameRendering += L"LightCullingPass";
	rs_lightCullingPass->Finalize(rsNameRendering.c_str(), rootSignatureFlags);


	// Root signature for rendering pass
	numRootParameters = 5, numRootParamIdx = 0, numSampler = 1;
	rs_renderPass->Reset(numRootParameters, numSampler);
	rs_renderPass->InitStaticSampler(0, non_static_sampler);
	(*rs_renderPass)[numRootParamIdx++].InitAsConstantBuffer(0);
	(*rs_renderPass)[numRootParamIdx++].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
	(*rs_renderPass)[numRootParamIdx++].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, D3D12_SHADER_VISIBILITY_PIXEL);
	(*rs_renderPass)[numRootParamIdx++].InitAsBufferSRV(2);
	(*rs_renderPass)[numRootParamIdx++].InitAsBufferSRV(3);
	rsNameRendering.resize(name.size());
	rsNameRendering += L"RenderingPass";
	rs_renderPass->Finalize(rsNameRendering.c_str(), rootSignatureFlags);


	// Shaders and pipline
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;
	ComPtr<ID3DBlob> pixelShaderDepth;
	ComPtr<ID3DBlob> frustumComputeShader;
	ComPtr<ID3DBlob> lightCullingComputeShader;
	
#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	wstring shaderName = L"TiledForwardShading.hlsl";
	
	ShaderMacros nullShaderMacros;
	// Load vertex shader
	vertexShader = DX12Aux::LoadShaderFromFile(ShaderType::VertexShader, "TiledForwardRendering", "VSMain", L"shaders.hlsl", nullShaderMacros, "vs_5_1");
	// Load pixel shader for scene depth
	pixelShaderDepth = DX12Aux::LoadShaderFromFile(ShaderType::PixelShader, "TiledForwardRendering", "PS_SceneDepth", L"shaders.hlsl", nullShaderMacros, "ps_5_1");
	// Load pixel shader for deferred phase
	pixelShader = DX12Aux::LoadShaderFromFile(ShaderType::PixelShader, "TiledForwardRendering", "PSMain", L"shaders.hlsl", nullShaderMacros, "ps_5_1");
	// Load frustum compute shader for frustum calculation
	frustumComputeShader = DX12Aux::LoadShaderFromFile(ShaderType::ComputeShader, "TiledForwardRendering", "CS_GridFrustumPass", L"GridFrustumPass.hlsl", nullShaderMacros, "cs_5_1");
	// Load light culling compute shader for frustum calculation
	ShaderMacros LightCullingMacros;
	LightCullingMacros["BLOCK_SIZE"] = to_string(m_TiledSize);
	lightCullingComputeShader = DX12Aux::LoadShaderFromFile(ShaderType::ComputeShader, "TiledForwardRendering", "CS_LightCullingPass", L"LightCullingPass.hlsl", LightCullingMacros, "cs_5_1");

	// Define the vertex input layout.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	
	shared_ptr<GraphicsPSO> scenePSO = make_shared<GraphicsPSO>();
	scenePSO->SetRootSignature(*rs_depthPass);
	scenePSO->SetInputLayout(_countof(inputElementDescs), inputElementDescs);
	scenePSO->SetVertexShader(CD3DX12_SHADER_BYTECODE(vertexShader.Get()));
	scenePSO->SetPixelShader(CD3DX12_SHADER_BYTECODE(pixelShaderDepth.Get()));
	scenePSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	scenePSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	scenePSO->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(TRUE, D3D12_DEPTH_WRITE_MASK_ALL,
		D3D12_COMPARISON_FUNC_LESS, TRUE, 0xFF, 0xFF,
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_INCR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS,
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_DECR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS));
	scenePSO->SetSampleMask(UINT_MAX);
	scenePSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	scenePSO->SetRenderTargetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_D32_FLOAT);
	//DXGI_FORMAT rtvFormats[] = { DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT };
	//scenePSO->SetRenderTargetFormats(4, rtvFormats, DXGI_FORMAT_D32_FLOAT);
	scenePSO->Finalize();

	// Depth pass setup
	sceneDepthPass->AddShader("TiledForwardDepthPass_VS", ShaderType::VertexShader, vertexShader);
	sceneDepthPass->AddShader("TiledForwardDepthPass_PS", ShaderType::PixelShader, pixelShaderDepth);
	sceneDepthPass->FinalizeRootSignature(rs_depthPass);
	sceneDepthPass->FinalizePSO(scenePSO);
	sceneDepthPass->SetEnableOwnRenderTarget(true);
	sceneDepthPass->AddRenderTarget(L"DepthPassRTV", m_width, m_height, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	sceneDepthPass->SetDepthBuffer(L"DepthPassDSV", m_width, m_height, DXGI_FORMAT_D32_FLOAT);

	sceneDepthPass->SetVertexBuffer(m_vertexBuffer);
	sceneDepthPass->SetIndexBuffer(m_indexBuffer);
	sceneDepthPass->SetViewPortAndScissorRect(m_viewport, m_scissorRect);

	sceneDepthPass->AddGpuProfiler(&m_gpuProfiler);
	sceneDepthPass->SetGPUQueryStatus(true);

	sceneDepthPass->SetName("TiledForwardDepthPass");

	// Grid frustum setup
	shared_ptr<ComputePSO> frustumPSO = make_shared<ComputePSO>();
	frustumPSO->SetRootSignature(*rs_frustumPass);
	frustumPSO->SetComputeShader(CD3DX12_SHADER_BYTECODE(frustumComputeShader.Get()));
	frustumPSO->Finalize();

	frustumPass->FinalizeRootSignature(rs_frustumPass);
	frustumPass->FinalizePSO(frustumPSO);

	frustumPass->AddShader("TiledForwardFrustumPass_CS", ShaderType::ComputeShader, frustumComputeShader);
	frustumPass->AddConstantBuffer(0, L"DispatchParameterCB", sizeof(DispatchParams), &m_dispatchParamsDataFrustum);
	frustumPass->AddConstantBuffer(1, L"ScreenToViewParameterCB", sizeof(ScreenToViewParams), &m_screenToViewParamsData);
	shared_ptr<StructuredBuffer> gridFrustumUAV = make_shared<StructuredBuffer>();
	gridFrustumUAV->Create(L"GridFrustumSB", m_BlockSizeX* m_BlockSizeY, sizeof(Frustum));
	frustumPass->AddStructuredBufferUAV(2, L"GridFrustumUAV", gridFrustumUAV);
	shared_ptr<StructuredBuffer> debugFrustumUAV = make_shared<StructuredBuffer>();
	debugFrustumUAV->Create(L"DebugUAV", m_BlockSizeX * m_BlockSizeY, sizeof(float));
	frustumPass->AddStructuredBufferUAV(3, L"DebugUAV", debugFrustumUAV);
	
	frustumPass->SetKernelDimensions(m_dispatchParamsDataFrustum.numThreadGroups.x, m_dispatchParamsDataFrustum.numThreadGroups.y, 1);

	// Light Culling setup
	shared_ptr<ComputePSO> lightCullingPSO = make_shared<ComputePSO>();
	lightCullingPSO->SetRootSignature(*rs_lightCullingPass);
	lightCullingPSO->SetComputeShader(CD3DX12_SHADER_BYTECODE(lightCullingComputeShader.Get()));
	lightCullingPSO->Finalize();

	lightCullingPass->FinalizeRootSignature(rs_lightCullingPass);
	lightCullingPass->FinalizePSO(lightCullingPSO);

	lightCullingPass->AddShader("TiledForwardLightCullingPass_CS", ShaderType::ComputeShader, lightCullingComputeShader);
	lightCullingPass->AddConstantBuffer(0, L"DispatchParameterCB", sizeof(DispatchParams), &m_dispatchParamsDataLightCulling);
	lightCullingPass->AddConstantBuffer(1, L"ScreenToViewParameterCB", sizeof(ScreenToViewParams), &m_screenToViewParamsData);

	vector<UINT> indexCounter(1, 1);
	shared_ptr<StructuredBuffer> oLightIndexCounter = make_shared<StructuredBuffer>();
	oLightIndexCounter->Create(L"oLightIndexCounter", 1, sizeof(UINT), indexCounter.data());
	lightCullingPass->AddStructuredBufferUAV(2, L"oLightIndexCounter", oLightIndexCounter);

	shared_ptr<StructuredBuffer> tLightIndexCounter = make_shared<StructuredBuffer>();
	tLightIndexCounter->Create(L"tLightIndexCounter", 1, sizeof(UINT), indexCounter.data());
	lightCullingPass->AddStructuredBufferUAV(3, L"tLightIndexCounter", tLightIndexCounter);

	shared_ptr<StructuredBuffer> oLightIndexList = make_shared<StructuredBuffer>();
	oLightIndexList->Create(L"oLightIndexList", m_BlockSizeX * m_BlockSizeY * AVERAGE_OVERLAPPING_LIGHTS, sizeof(UINT));
	lightCullingPass->AddStructuredBufferUAV(4, L"oLightIndexList", oLightIndexList);

	shared_ptr<StructuredBuffer> tLightIndexList = make_shared<StructuredBuffer>();
	tLightIndexList->Create(L"tLightIndexList", m_BlockSizeX * m_BlockSizeY * AVERAGE_OVERLAPPING_LIGHTS, sizeof(UINT));
	lightCullingPass->AddStructuredBufferUAV(5, L"tLightIndexList", tLightIndexList);

	shared_ptr<ColorBuffer> oLightGrid(new ColorBuffer(),
		[](ColorBuffer* ptr) { ptr->Destroy(); });
	oLightGrid->Create(L"OpaqueLightGridMap", m_BlockSizeX, m_BlockSizeY, 1, DXGI_FORMAT_R32G32_UINT);
	lightCullingPass->AddColorBufferUAV(6, L"OpaqueLightGridMap", oLightGrid);

	shared_ptr<ColorBuffer> tLightGrid(new ColorBuffer(),
		[](ColorBuffer* ptr) { ptr->Destroy(); });
	tLightGrid->Create(L"TransparentLightGridMap", m_BlockSizeX, m_BlockSizeY, 1, DXGI_FORMAT_R32G32_UINT);
	lightCullingPass->AddColorBufferUAV(7, L"TransparentLightGridMap", tLightGrid);

	lightCullingPass->AddStructuredBufferSRV(8, L"CullingFrustumSRV", gridFrustumUAV);
	lightCullingPass->AddStructuredBufferSRV(9, L"LightsSRV", m_Lights);

	lightCullingPass->AddDepthBufferSRV(10, L"SceneDepthSRV", sceneDepthPass->GetDepthBuffer());

	shared_ptr<StructuredBuffer> testSB = make_shared<StructuredBuffer>();
	testSB->Create(L"DebugUAVSB", m_BlockSizeX* m_BlockSizeY, sizeof(XMFLOAT4));
	lightCullingPass->AddStructuredBufferUAV(11, L"DebugSB", testSB);

	shared_ptr<ColorBuffer> debugTex2D(new ColorBuffer(),
		[](ColorBuffer* ptr) { ptr->Destroy(); });
	debugTex2D->Create(L"DebugUAVTex2D", m_width, m_height, 1, DXGI_FORMAT_R32G32_FLOAT);
	lightCullingPass->AddColorBufferUAV(12, L"DebugTex2D", debugTex2D);

	lightCullingPass->SetKernelDimensions(m_dispatchParamsDataLightCulling.numThreadGroups.x, m_dispatchParamsDataLightCulling.numThreadGroups.y, 1);

	// Final rendering setup.
	renderPass->AddShader("TiledForwardPass_VS", ShaderType::VertexShader, vertexShader);
	renderPass->AddShader("TiledForwardPass_PS", ShaderType::PixelShader, pixelShader);


	shared_ptr<GraphicsPSO> renderPSO = make_shared<GraphicsPSO>();
	*renderPSO = *scenePSO;
	renderPSO->SetRootSignature(*rs_renderPass);
	//renderPSO->SetInputLayout(_countof(inputElementDescs), inputElementDescs);
	renderPSO->SetVertexShader(CD3DX12_SHADER_BYTECODE(vertexShader.Get()));
	renderPSO->SetPixelShader(CD3DX12_SHADER_BYTECODE(pixelShader.Get()));
	//renderPSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	//renderPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	//renderPSO->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(TRUE, D3D12_DEPTH_WRITE_MASK_ALL,
	//	D3D12_COMPARISON_FUNC_LESS, TRUE, 0xFF, 0xFF,
	//	D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_INCR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS,
	//	D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_DECR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS));
	//renderPSO->SetSampleMask(UINT_MAX);
	//renderPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	renderPSO->SetRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
	renderPSO->Finalize();

	renderPass->FinalizeRootSignature(rs_renderPass);
	renderPass->FinalizePSO(renderPSO);

	renderPass->AddConstantBuffer(0, L"ConstBuffer", sizeof(CBuffer), &m_constantBufferData);
	renderPass->AddColorBufferSRV(1, L"tLightGrid", tLightGrid);
	renderPass->AddColorBufferSRV(2, L"oLightGrid", oLightGrid);
	renderPass->AddStructuredBufferSRV(3, L"oLightIndex", oLightIndexList);
	renderPass->AddStructuredBufferSRV(4, L"Lights", m_Lights);

	renderPass->SetEnableOwnRenderTarget(false);
	//renderPass->AddRenderTarget(L"RenderPassRTV", m_width, m_height, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
	renderPass->SetDepthBuffer(L"RenderPassDSV", m_width, m_height, DXGI_FORMAT_D32_FLOAT);

	renderPass->SetVertexBuffer(m_vertexBuffer);
	renderPass->SetIndexBuffer(m_indexBuffer);
	renderPass->SetViewPortAndScissorRect(m_viewport, m_scissorRect);

	renderPass->AddGpuProfiler(&m_gpuProfiler);
	renderPass->SetGPUQueryStatus(true);

	renderPass->SetName("TiledForwardRenderPass");

	m_tiledForwardRenderingTech.AddPass(sceneDepthPass);
	m_tiledForwardRenderingTech.AddPass(frustumPass);
	m_tiledForwardRenderingTech.AddPass(lightCullingPass);
	m_tiledForwardRenderingTech.AddPass(renderPass);
}


void RenderingDemo::OnResize(uint64_t width, uint64_t height)
{
	// Check for invalid window dimensions
	if (width == 0 || height == 0)
		return;
	m_width = width;
	m_height = height;

	m_viewport.Width = static_cast<float>(width);
	m_viewport.Height = static_cast<float>(height);
	m_scissorRect.right = static_cast<LONG>(width);
	m_scissorRect.bottom = static_cast<LONG>(height);

	IGraphics::g_GraphicsCore->g_CommandManager->IdleGPU();

	// Create Depth Buffer
	m_sceneDepthBuffer.Destroy();
	m_sceneDepthBuffer.Create(L"FinalSceneDepthPass", m_width, m_height, DXGI_FORMAT_D32_FLOAT);

	// Camera Setup
	{
		// Setup the camera's view parameters
		static const XMVECTORF32 s_Eye = { 0.0f, 0.0f, -10.0f, 0.f };
		static const XMVECTORF32 s_At = { 0.0f, 0.0f, 0.0f, 0.f };
		m_modelViewCamera.SetViewParams(s_Eye, s_At);
		// Setup the camera's projection parameters
		float fAspectRatio = m_width / (float)m_height;
		m_modelViewCamera.SetProjParams(XM_PI / 4, fAspectRatio, 0.01f, 10000.0f);
		m_modelViewCamera.SetWindow(m_width, m_height);
		m_modelViewCamera.SetButtonMasks(MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON);
	}

	m_generalRenderingTech.Resize(width, height);
	m_deferredRenderingTech.Resize(width, height);
	m_tiledForwardRenderingTech.Resize(width, height);

	// Gui Resource allocation
	//IGuiCore::g_imGuiTexConverter->CleanUp();
	//IGuiCore::g_imGuiTexConverter->AddInputRes(
	//	"SceneDepthView", 
	//	m_width, m_height, 
	//	DXGI_FORMAT_D32_FLOAT, 
	//	dynamic_pointer_cast<DX12ShadingPass>(m_generalRenderingTech.GetPass(0))->GetDepthBuffer().get()); 
	//ThrowIfFailed(IGuiCore::g_imGuiTexConverter->Finalize());

	IGuiCore::g_imGuiTexConverter->Resize(width, height);
}


std::vector<UINT8> RenderingDemo::GenerateTextureData()
{
	const UINT rowPitch = TextureWidth * TexturePixelSize;
	const UINT cellPitch = rowPitch >> 3;        // The width of a cell in the checkboard texture.
	const UINT cellHeight = TextureWidth >> 3;    // The height of a cell in the checkerboard texture.
	const UINT textureSize = rowPitch * TextureHeight;

	std::vector<UINT8> data(textureSize);
	UINT8* pData = &data[0];

	for (UINT n = 0; n < textureSize; n += TexturePixelSize)
	{
		UINT x = n % rowPitch;
		UINT y = n / rowPitch;
		UINT i = x / cellPitch;
		UINT j = y / cellHeight;

		if (i % 2 == j % 2)
		{
			pData[n] = 0x00;        // R
			pData[n + 1] = 0x00;    // G
			pData[n + 2] = 0x00;    // B
			pData[n + 3] = 0xff;    // A
		}
		else
		{
			pData[n] = 0xff;        // R
			pData[n + 1] = 0xff;    // G
			pData[n + 2] = 0xff;    // B
			pData[n + 3] = 0xff;    // A
		}
	}

	return data;
}

// Update frame-based values.
void RenderingDemo::OnUpdate()
{
	m_modelViewCamera.FrameMove(ImGui::GetIO().DeltaTime);

	static XMMATRIX rotMat = XMMatrixRotationY(0.001f);
	static XMMATRIX world = XMMatrixIdentity();

	world = rotMat * world;
	//XMMATRIX world = XMMatrixIdentity(), view = m_modelViewCamera.GetViewMatrix(), proj = m_modelViewCamera.GetProjMatrix();
	XMMATRIX view = m_modelViewCamera.GetViewMatrix(), proj = m_modelViewCamera.GetProjMatrix();
	m_constantBufferData.worldMatrix = world;
	m_constantBufferData.viewMatrix = view;
	m_constantBufferData.worldViewProjMatrix = (world * view * proj);

	m_screenToViewParamsData.InverseProjection = XMMatrixInverse(nullptr, proj);
	m_screenToViewParamsData.ScreenDimensions = XMUINT2(m_width, m_height);
	m_screenToViewParamsData.ViewMatrix = view;
	UINT lightCount = m_Lights->GetElementCount();
	m_screenToViewParamsData.Padding = XMUINT2(lightCount, lightCount);
	m_Lights->GetElementCount();

	m_generalRenderingTech.UpdatePerFrameContBuffer(
		0,
		m_constantBufferData.worldMatrix,
		m_constantBufferData.viewMatrix,
		m_constantBufferData.worldViewProjMatrix);

	// TODO add contBuffer update for other two techniques
}

// Render the scene.
void RenderingDemo::OnRender()
{
	GpuTimeCore::BeginReadBack();

	IGuiCore::ShowImGUI();

	GraphicsContext& gfxContext = GraphicsContext::Begin(L"Tiled Forward Rendering");

	m_gpuProfiler.Start("TotalGpuTime", gfxContext);

	// TODO: tmp solution, need to removed in the future 
	//if (m_renderingOption == TiledForwardRenderingOption)
	//{
	//	m_gpuProfiler.Start("PreDepthGpuTime", gfxContext);
	//	// Get scene depth in the screen space
	//	PreDepthPass(gfxContext);
	//	m_gpuProfiler.Stop("PreDepthGpuTime", gfxContext);

	//	m_gpuProfiler.Start("ForwardRendering", gfxContext);
	//	// Forward Plus Rendering calculation
	//	m_LightCullingPass.ExecuteCS(gfxContext, m_preDepthPass);
	//	m_gpuProfiler.Stop("ForwardRendering", gfxContext);
	//}

	gfxContext.TransitionResource(m_modelTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


	switch (m_renderingOption)
	{
	case GeneralRenderingOption:
		m_generalRenderingTech.Render(gfxContext);
		break;
	case DefferredRenderingOption:
		m_deferredRenderingTech.Render(gfxContext);
		break;
	case TiledForwardRenderingOption:
		//TiledForwardRenderingTechnique(gfxContext);
		m_tiledForwardRenderingTech.Render(gfxContext);
		break;
	default:
		break;
	}

	UINT backBufferIndex = IGraphics::g_GraphicsCore->g_CurrentBuffer;

	D3D12_CPU_DESCRIPTOR_HANDLE RTVs[] =
	{
		IGraphics::g_GraphicsCore->g_DisplayPlane[backBufferIndex].GetRTV()
	};

	gfxContext.TransitionResource(IGraphics::g_GraphicsCore->g_DisplayPlane[backBufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET);
	gfxContext.TransitionResource(m_sceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

	gfxContext.ClearDepth(m_sceneDepthBuffer);
	gfxContext.SetDepthStencilTarget(m_sceneDepthBuffer.GetDSV());
	gfxContext.SetRenderTargets(_countof(RTVs), RTVs, m_sceneDepthBuffer.GetDSV());
	//gfxContext.ClearColor(IGraphics::g_GraphicsCore->g_DisplayPlane[backBufferIndex]);

	m_gpuProfiler.Start("ImGUIPass", gfxContext);
	IGuiCore::RenderImGUI(gfxContext);
	//IGuiCore::g_imGuiTexConverter->Convert(gfxContext);
	m_gpuProfiler.Stop("ImGUIPass", gfxContext);

	gfxContext.TransitionResource(IGraphics::g_GraphicsCore->g_DisplayPlane[backBufferIndex], D3D12_RESOURCE_STATE_PRESENT);
	m_gpuProfiler.Stop("TotalGpuTime", gfxContext);

	gfxContext.Finish();

	GpuTimeCore::EndReadBack();

	IGraphics::g_GraphicsCore->Present();
}

void RenderingDemo::OnDestroy()
{
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	m_lightsList.clear();
	m_pModel.reset();

	m_sceneDepthBuffer.Destroy();
	m_vertexBuffer->Destroy();
	m_indexBuffer->Destroy();
	m_modelTexture.Destroy();

	m_generalRenderingTech.Destroy();
	m_deferredRenderingTech.Destroy();
	m_tiledForwardRenderingTech.Destroy();
}

// Lights generation
void RenderingDemo::GenerateLights(uint32_t numLights,
	XMFLOAT3 minPoint, XMFLOAT3 maxPoint,
	float minLightRange, float maxLightRange,
	float minSpotLightAngle, float maxSpotLightAngle)
{
	uniform_real_distribution<double> dblDistro(0.0, 1.0);

	XMFLOAT3 bounds(maxPoint.x - minPoint.x, maxPoint.y - minPoint.y, maxPoint.z - minPoint.z);

	m_lightsList.clear();
	m_lightsList.resize(numLights);
	for (int i = 0; i < numLights; ++i)
	{
		Light& light = m_lightsList[i];
		XMFLOAT3 pos;

		pos.x = dblDistro(defEngine);
		pos.y = dblDistro(defEngine);
		pos.z = dblDistro(defEngine);

		light.m_PositionWS = XMFLOAT4(
			pos.x * bounds.x + minPoint.x,
			pos.y * bounds.y + minPoint.y,
			pos.z * bounds.z + minPoint.z,
			1.0f);

		light.m_Color.x = min(1.0f, dblDistro(defEngine) + 0.001f);
		light.m_Color.y = min(1.0f, dblDistro(defEngine) + 0.001f);
		light.m_Color.z = min(1.0f, dblDistro(defEngine) + 0.001f);
		light.m_Color.w = 1.0f;

		light.m_DirectionWS = XMFLOAT4(
			-light.m_PositionWS.x,
			-light.m_PositionWS.y,
			-light.m_PositionWS.z,
			0.0f);

		light.m_Range = minLightRange + dblDistro(defEngine) * (maxLightRange - minLightRange);
		light.m_SpotlightAngle = minSpotLightAngle + dblDistro(defEngine) * maxSpotLightAngle;

		// Use probablity to generate three different types lights
		float fLightPropability = dblDistro(defEngine);
		// TODO hard coding the light types for debugging purpose
		light.m_Type = Light::LightType::Point;
		light.m_Enabled = true;


#if defined(_DEBUG_LIGHT)
		// Hardcode light properties for debugging
		light.m_PositionWS = XMFLOAT4(0.0f, 0.0f, -2.0f, 1.0f);

		light.m_Color.x = 0.5f;
		light.m_Color.y = 0.1f;
		light.m_Color.z = 0.7f;
		light.m_Color.w = 1.0f;

		light.m_DirectionWS = XMFLOAT4(
			-light.m_PositionWS.x,
			-light.m_PositionWS.y,
			-light.m_PositionWS.z,
			0.0f);

		light.m_Range = 100.0f;

		light.m_SpotlightAngle = 0.745f;
#endif

	}

	UpdateLightsBuffer();

	// TODO: Might be useful later
	// Uniformly distribute lights on the spawning cube's surface 
	//uint32_t lightsPerDimension = static_cast<uint32_t>(ceil(cbrt(numLights)));
	//pos.x = (i % lightsPerDimension) / static_cast<float>(lightsPerDimension);
	//pos.y = (static_cast<uint32_t>(floor(i / static_cast<float>(lightsPerDimension))) % lightsPerDimension) / static_cast<float>(lightsPerDimension);
	//pos.z = (static_cast<uint32_t>(floor(i / static_cast<float>(lightsPerDimension) / static_cast<float>(lightsPerDimension))) % lightsPerDimension) / static_cast<float>(lightsPerDimension);
}

void RenderingDemo::UpdateLightsBuffer()
{
	XMMATRIX viewMatrix = m_modelViewCamera.GetViewMatrix();

	for (auto& light : m_lightsList)
	{
		XMVECTOR posVecWS = XMLoadFloat4(&light.m_PositionWS);
		XMStoreFloat4(&light.m_PositionVS, XMVector4Transform(posVecWS, viewMatrix));

		XMVECTOR dirVecWS = XMLoadFloat4(&light.m_DirectionWS);
		XMStoreFloat4(&light.m_DirectionVS, XMVector4Transform(dirVecWS, viewMatrix));
	}

	IGraphics::g_GraphicsCore->g_CommandManager->IdleGPU();
	m_Lights->Destroy();
	m_Lights->Create(L"LightLists", m_lightsList.size(), sizeof(Light), m_lightsList.data());
}
