#include "stdafx.h"
#include "TiledRendering.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void TiledRendering::WinMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam);

	m_modelViewCamera.HandleMessages(hWnd, message, wParam, lParam);
}

TiledRendering::TiledRendering(UINT width, UINT height, std::wstring name) :
	Win32FrameWork(width, height, name),
	m_pCbvDataBegin(nullptr),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
	m_constantBufferData{}
{
}



void TiledRendering::OnInit()
{
	LoadPipeline();
	LoadAssets();
	//m_simpleCS.OnInit();

}

// Load the rendering pipeline dependencies.
void TiledRendering::LoadPipeline()
{
	UINT dxgiFactoryFlags = 0;

	IGraphics::g_GraphicsCore->g_hwnd = Win32Application::GetHwnd();
	IGraphics::g_GraphicsCore->Initialize();
	IGuiCore::Init(this);
}

// Load the sample assets.
void TiledRendering::LoadAssets()
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

		/*Root Signature for the depth pass*/
		m_preDepthPassRootSignature.Reset(1, 0);
		m_preDepthPassRootSignature[0].InitAsConstantBuffer(0);
		//m_preDepthPassRootSignature[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_VERTEX);
		m_preDepthPassRootSignature.Finalize(L"PreDepthPassRootSignature", rootSignatureFlags);
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

		/*PSO for depth pass*/
		m_preDepthPassPSO = m_scenePSO;
		m_preDepthPassPSO.SetRootSignature(m_preDepthPassRootSignature);
		m_preDepthPassPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(pixelShaderDepthPass.Get()));
		m_preDepthPassPSO.Finalize();
	}

	// Create the vertex buffer.
	m_pModel = make_shared<Model>();
	{
		m_pModel->Load("bunny.obj");
		m_vertexBuffer.Create(L"BunnyVertexBuffer", m_pModel->m_vecVertexData.size(), sizeof(Vertex), m_pModel->m_vecVertexData.data());
		m_indexBuffer.Create(L"BunnyIndexBuffer", m_pModel->m_vecIndexData.size(), sizeof(uint32_t), m_pModel->m_vecIndexData.data());
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

	// Load pre-depth pass resources
	m_preDepthPassRTV.Create(L"PreDepthPassRTV", m_width, m_height, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
	m_preDepthPass.Create(L"PreSceneDepthPass", m_width, m_height, DXGI_FORMAT_D32_FLOAT);


	m_LightCullingPass.SetTiledSize(16);
	m_LightCullingPass.Init(
		m_width, 
		m_height,
		XMMatrixInverse(nullptr, m_modelViewCamera.GetProjMatrix()));

	// Generate lights
	GenerateLights(1);
	m_LightCullingPass.UpdateLightBuffer(m_lightsList);

	/*Test Area*/
	// Gui Resource allocation
	IGuiCore::g_imGuiTexConverter->AddInputRes("SceneDepthView", m_width, m_height, sizeof(float), DXGI_FORMAT_D32_FLOAT, &m_preDepthPass);
	ThrowIfFailed(IGuiCore::g_imGuiTexConverter->Finalize());

}

std::vector<UINT8> TiledRendering::GenerateTextureData()
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
void TiledRendering::OnUpdate()
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

	m_LightCullingPass.UpdateConstantBuffer(m_modelViewCamera.GetViewMatrix());
}

// Render the scene.
void TiledRendering::OnRender()
{
	IGuiCore::ShowImGUI();
	////m_simpleCS.OnExecuteCS();

	GraphicsContext& gfxContext = GraphicsContext::Begin(L"Tiled Forward Rendering");

	// Get scene depth in the screen space
	PreDepthPass(gfxContext);

	// Forward Plus Rendering calculation
	m_LightCullingPass.ExecuteCS(gfxContext, m_preDepthPass);


	UINT backBufferIndex = IGraphics::g_GraphicsCore->g_CurrentBuffer;
	gfxContext.TransitionResource(IGraphics::g_GraphicsCore->g_DisplayPlane[backBufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET);
	gfxContext.TransitionResource(m_modelTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	gfxContext.TransitionResource(m_sceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);


	gfxContext.SetRootSignature(m_sceneOpaqueRootSignature);
	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxContext.SetIndexBuffer(m_indexBuffer.IndexBufferView());
	gfxContext.SetVertexBuffer(0, m_vertexBuffer.VertexBufferView());
	gfxContext.SetPipelineState(m_scenePSO);

	gfxContext.SetDynamicConstantBufferView(e_rootParameterCB, sizeof(m_constantBufferData), &m_constantBufferData);
	gfxContext.SetDynamicDescriptor(e_ModelTexRootParameterSRV, 0, m_modelTexture.GetSRV());
	gfxContext.SetDynamicDescriptor(e_LightGridRootParameterSRV, 0, m_LightCullingPass.GetOpaqueLightGrid().GetSRV());
	gfxContext.SetBufferSRV(e_LightIndexRootParameterSRV, m_LightCullingPass.GetOpaqueLightIndex());
	gfxContext.SetBufferSRV(e_LightBufferRootParameterSRV, m_LightCullingPass.GetLightBuffer());

	D3D12_CPU_DESCRIPTOR_HANDLE RTVs[] =
	{
		IGraphics::g_GraphicsCore->g_DisplayPlane[backBufferIndex].GetRTV()
	};

	gfxContext.ClearDepth(m_sceneDepthBuffer);
	gfxContext.SetDepthStencilTarget(m_sceneDepthBuffer.GetDSV());
	gfxContext.SetRenderTargets(_countof(RTVs), RTVs, m_sceneDepthBuffer.GetDSV());
	gfxContext.ClearColor(IGraphics::g_GraphicsCore->g_DisplayPlane[backBufferIndex]);

	gfxContext.SetViewportAndScissor(m_viewport, m_scissorRect);

	//gfxContext.CopyBuffer(IGuiCore::g_srvDict[IGuiCore::OpaqueLightGridSRV], m_LightCullingPass.GetOpaqueLightGrid());

	gfxContext.DrawIndexed(m_pModel->m_vecIndexData.size(), 0, 0);


	IGuiCore::RenderImGUI(gfxContext);
	//IGuiCore::g_imGuiTexConverter->Convert(gfxContext);

	gfxContext.TransitionResource(IGraphics::g_GraphicsCore->g_DisplayPlane[backBufferIndex], D3D12_RESOURCE_STATE_PRESENT);

	gfxContext.Finish();

	IGraphics::g_GraphicsCore->Present();
}

void TiledRendering::OnDestroy()
{
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	m_LightCullingPass.Destroy();
	m_pModel.reset();
}

void TiledRendering::PreDepthPass(GraphicsContext& gfxContext)
{
	gfxContext.SetRootSignature(m_preDepthPassRootSignature);
	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxContext.SetIndexBuffer(m_indexBuffer.IndexBufferView());
	gfxContext.SetVertexBuffer(0, m_vertexBuffer.VertexBufferView());

	gfxContext.SetDynamicConstantBufferView(e_rootParameterCB, sizeof(m_constantBufferData), &m_constantBufferData);

	gfxContext.TransitionResource(m_preDepthPassRTV, D3D12_RESOURCE_STATE_RENDER_TARGET);
	gfxContext.TransitionResource(m_preDepthPass, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

	D3D12_CPU_DESCRIPTOR_HANDLE RTVs[] =
	{
		m_preDepthPassRTV.GetRTV()
	};
	gfxContext.SetPipelineState(m_preDepthPassPSO);
	gfxContext.ClearDepth(m_preDepthPass);
	gfxContext.SetDepthStencilTarget(m_preDepthPass.GetDSV());
	gfxContext.SetRenderTargets(_countof(RTVs), RTVs, m_preDepthPass.GetDSV());
	gfxContext.ClearColor(m_preDepthPassRTV);

	gfxContext.SetViewportAndScissor(m_viewport, m_scissorRect);

	gfxContext.DrawIndexed(m_pModel->m_vecIndexData.size(), 0, 0);

	gfxContext.TransitionResource(m_preDepthPass, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);
}

// Lights generation
void TiledRendering::GenerateLights(uint32_t numLights,
	XMFLOAT3 minPoint, XMFLOAT3 maxPoint,
	float minLightRange, float maxLightRange,
	float minSpotLightAngle, float maxSpotLightAngle)
{
	srand(time(0));

	uint32_t lightsPerDimension = static_cast<uint32_t>(ceil(cbrt(numLights)));
	// TODO hard code the light spawing space here
	XMFLOAT3 bounds(maxPoint.x - minPoint.x, maxPoint.y - minPoint.y, maxPoint.z - minPoint.z);

	m_lightsList.clear();
	m_lightsList.resize(numLights);
	for (int i = 0; i < numLights; ++i)
	{
		Light& light = m_lightsList[i];
		// Uniformly distribute lights
		XMFLOAT3 pos;
		pos.x = (i % lightsPerDimension) / static_cast<float>(lightsPerDimension);
		pos.y = (static_cast<uint32_t>(floor(i / static_cast<float>(lightsPerDimension))) % lightsPerDimension) / static_cast<float>(lightsPerDimension);
		pos.z = (static_cast<uint32_t>(floor(i / static_cast<float>(lightsPerDimension) / static_cast<float>(lightsPerDimension))) % lightsPerDimension) / static_cast<float>(lightsPerDimension);

		light.m_PositionWS = XMFLOAT4(
			pos.x * bounds.x + minPoint.x, 
			pos.y * bounds.y + minPoint.y, 
			pos.z * bounds.z + minPoint.z, 
			1.0f);
		// Random distribution (TODO may need a switch to choose which method to use)
		//pos.x = (rand() / INT_MAX) * bounds.x;
		//pos.y = (rand() / INT_MAX) * bounds.y;
		//pos.z = (rand() / INT_MAX) * bounds.z;
		//light.m_PositionWS = XMFLOAT4(pos.x, pos.y, pos.z, 1.0f);

		// TODO may need a uniform random generator here
		light.m_Color.x = min(1.0f, (1.0f * rand() / INT_MAX) + 0.1);
		light.m_Color.y = min(1.0f, (1.0f * rand() / INT_MAX) + 0.1);
		light.m_Color.z = min(1.0f, (1.0f * rand() / INT_MAX) + 0.1);
		light.m_Color.w = 1.0f;

		light.m_DirectionWS = XMFLOAT4(
			-light.m_PositionWS.x,
			-light.m_PositionWS.y,
			-light.m_PositionWS.z,
			0.0f);

		light.m_Range = minLightRange + (1.0f * rand() / INT_MAX) * maxLightRange;
		light.m_SpotlightAngle = minSpotLightAngle + (1.0f * rand() / INT_MAX) * maxSpotLightAngle;

		// Use probablity to generate three different types lights
		float fLightPropability = (1.0f * rand() / INT_MAX);
		// TODO hard coding the light types for debugging purpose
		light.m_Type = Light::LightType::Point;



		// Test
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
	}

	UpdateLightsBuffer();
}

void TiledRendering::UpdateLightsBuffer()
{
	XMMATRIX viewMatrix = m_modelViewCamera.GetViewMatrix();

	for (auto& light : m_lightsList)
	{
		XMVECTOR posVecWS = XMLoadFloat4(&light.m_PositionWS);
		XMStoreFloat4(&light.m_PositionVS, XMVector4Transform(posVecWS, viewMatrix));

		XMVECTOR dirVecWS = XMLoadFloat4(&light.m_DirectionWS);
		XMStoreFloat4(&light.m_DirectionVS, XMVector4Transform(dirVecWS, viewMatrix));
	}
	m_LightCullingPass.UpdateLightBuffer(m_lightsList);
}
