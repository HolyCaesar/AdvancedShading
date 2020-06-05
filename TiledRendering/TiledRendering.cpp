#include "stdafx.h"
#include "TiledRendering.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void TiledRendering::WinMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam);

	m_modelViewCamera.HandleMessages(hWnd, message, wParam, lParam);
}

TiledRendering::TiledRendering(UINT width, UINT height, std::wstring name) :
	Win32FrameWork(width, height, name),
	//m_frameIndex(0),
	m_pCbvDataBegin(nullptr),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
	//m_rtvDescriptorSize(0),
	m_constantBufferData{}
{
	m_dsvHeapOffset = 0;
	m_cbvSrvUavOffset = 0;
}

void TiledRendering::ShowImGUI()
{
	static bool show_demo_window = true;
	static bool show_another_window = false;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
	if (show_demo_window)
		ImGui::ShowDemoWindow(&show_demo_window);

	// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
	{
		static float f = 0.0f;
		static int counter = 0;

		ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

		ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
		ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
		ImGui::Checkbox("Another Window", &show_another_window);

		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
		ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

		if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
			counter++;
		ImGui::SameLine();
		ImGui::Text("counter = %d", counter);

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}

	// 3. Show another simple window.
	if (show_another_window)
	{
		ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
		ImGui::Text("Hello from another window!");
		if (ImGui::Button("Close Me"))
			show_another_window = false;
		ImGui::End();
	}
}

void TiledRendering::OnInit()
{
	LoadPipeline();
	LoadAssets();
	LoadPreDepthPassAssets();
	m_simpleCS.OnInit();
}

// Load the rendering pipeline dependencies.
void TiledRendering::LoadPipeline()
{
	UINT dxgiFactoryFlags = 0;

	IGraphics::g_GraphicsCore->g_hwnd = Win32Application::GetHwnd();
	IGraphics::g_GraphicsCore->Initialize();
	IGraphics::g_GraphicsCore->InitializeCS();

	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};

		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 1;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&imGuiHeap)) != S_OK);

		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 256;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_cbvSrvUavHeap)) != S_OK);
		m_cbvSrvUavDescriptorSize = IGraphics::g_GraphicsCore->g_pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		desc.NumDescriptors = 1;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_dsvHeap)) != S_OK);
	}
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
		m_sceneOpaqueRootSignature[e_rootParameterCB].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, e_cCB, D3D12_SHADER_VISIBILITY_VERTEX);
		m_sceneOpaqueRootSignature[e_ModelTexRootParameterSRV].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, e_cSRV, D3D12_SHADER_VISIBILITY_PIXEL);
		m_sceneOpaqueRootSignature[e_LightGridRootParameterSRV].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, D3D12_SHADER_VISIBILITY_PIXEL);
		m_sceneOpaqueRootSignature[e_LightIndexRootParameterSRV].InitAsBufferSRV(2);
		m_sceneOpaqueRootSignature[e_LightBufferRootParameterSRV].InitAsBufferSRV(3);
		m_sceneOpaqueRootSignature.Finalize(L"SceneRootSignature", rootSignatureFlags);
	}

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

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
#else
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));
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

	// Create the command list.
	ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), m_scenePSO.GetPSO(), IID_PPV_ARGS(&IGraphics::g_GraphicsCore->g_commandList)));
	m_commandList = IGraphics::g_GraphicsCore->g_commandList;

	// Create the vertex buffer.
	m_pModel = make_shared<Model>();
	{
		m_pModel->Load("bunny.obj");
		m_vertexBuffer.Create(L"BunnyVertexBuffer", m_pModel->m_vecVertexData.size(), sizeof(Vertex), m_pModel->m_vecVertexData.data());
		m_indexBuffer.Create(L"BunnyIndexBuffer", m_pModel->m_vecIndexData.size(), sizeof(uint32_t), m_pModel->m_vecIndexData.data());
	}

	// Create Depth Buffer
	m_sceneDepthBuffer.Create(L"RenderPassDepthBuffer", m_width, m_height, DXGI_FORMAT_D32_FLOAT);
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_sceneDepthBuffer.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	{
		D3D12_CLEAR_VALUE optimizedClearValue = {};
		optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		optimizedClearValue.DepthStencil = { 1.0f, 0 };

		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, m_width, m_height,
				1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			//D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_COMMON,
			&optimizedClearValue,
			IID_PPV_ARGS(&m_preDepthPassBuffer.pResource)
		));

		// Update the depth-stencil view.
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = DXGI_FORMAT_D32_FLOAT;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = D3D12_DSV_FLAG_NONE;

		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDepthStencilView(m_preDepthPassBuffer.pResource.Get(), &dsv,
			m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create SRV for the depth buffer
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MipLevels = 1;
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), m_cbvSrvUavOffset, m_cbvSrvUavDescriptorSize);
		m_preDepthPassBuffer.mUsageState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		m_preDepthPassBuffer.uSrvDescriptorOffset = m_cbvSrvUavOffset;
		++m_cbvSrvUavOffset;
		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(m_preDepthPassBuffer.pResource.Get(), &SRVDesc, srvHandle);

		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_preDepthPassBuffer.pResource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
	}

	// Create the constant buffer
	{
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_modelConstantBuffer.pResource)));

		// Describe and create a constant buffer view.
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_modelConstantBuffer.pResource->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(CBuffer) + 255) & ~255;    // CB size is required to be 256-byte aligned.
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), m_cbvSrvUavOffset, m_cbvSrvUavDescriptorSize);
		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateConstantBufferView(&cbvDesc, cbvHandle);
		m_modelConstantBuffer.mUsageState = D3D12_RESOURCE_STATE_GENERIC_READ;
		m_modelConstantBuffer.uCbvDescriptorOffset = m_cbvSrvUavOffset;
		++m_cbvSrvUavOffset;

		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_modelConstantBuffer.pResource->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin)));
		memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
	}

	// Note: ComPtr's are CPU objects but this resource needs to stay in scope until
	// the command list that references it has finished executing on the GPU.
	// We will flush the GPU at the end of this method to ensure the resource is not
	// prematurely destroyed.
	ComPtr<ID3D12Resource> textureUploadHeap;
	{
		// Describe and create a Texture2D.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Width = TextureWidth;
		textureDesc.Height = TextureHeight;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_modelTexture.pResource)));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_modelTexture.pResource.Get(), 0, 1);

		// Create the GPU upload buffer.
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&textureUploadHeap)));

		// Copy data to the intermediate upload heap and then schedule a copy 
		// from the upload heap to the Texture2D.
		std::vector<UINT8> texture = GenerateTextureData();

		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = &texture[0];
		textureData.RowPitch = TextureWidth * TexturePixelSize;
		textureData.SlicePitch = textureData.RowPitch * TextureHeight;

		UpdateSubresources(m_commandList.Get(), m_modelTexture.pResource.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_modelTexture.pResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		// Describe and create a SRV for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), m_cbvSrvUavOffset, m_cbvSrvUavDescriptorSize);
		m_modelTexture.mUsageState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		m_modelTexture.uSrvDescriptorOffset = m_cbvSrvUavOffset;
		++m_cbvSrvUavOffset;
		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(m_modelTexture.pResource.Get(), &srvDesc, srvHandle);
	}

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

	// Grid Frustum Pass Creation
	//m_GridFrustumsPass.SetTiledDimension(16);
	//m_GridFrustumsPass.Init(L"GridFrustumPass.hlsl", m_width, m_height, XMMatrixInverse(nullptr, m_modelViewCamera.GetProjMatrix()));

	//m_LightCullingPass.Init(L"LightCullingPass.hlsl", m_width, m_height, XMMatrixInverse(nullptr, m_modelViewCamera.GetProjMatrix()));

	m_LightCullingPass.SetTiledSize(16);
	m_LightCullingPass.Init(
		m_width, m_height, 
		XMMatrixInverse(nullptr, m_modelViewCamera.GetProjMatrix()),
		m_cbvSrvUavHeap, m_cbvSrvUavOffset);

	// Generate lights
	GenerateLights(1);
	m_LightCullingPass.UpdateLightBuffer(m_lightsList);

	// Close the command list and execute it to begin the initial GPU setup.
	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	IGraphics::g_GraphicsCore->g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	IGraphics::g_GraphicsCore->WaitForGpu();

	LoadImGUI();
}


void TiledRendering::LoadPreDepthPassAssets()
{
	ThrowIfFailed(IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex]->Reset());
	ThrowIfFailed(m_commandList->Reset(IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), m_scenePSO.GetPSO()));

	// Create a root signature consisting of a descriptor table with a single CBV
	{
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		//D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

		m_preDepthPassRootSignature.Reset(1, 0);
		m_preDepthPassRootSignature[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_VERTEX);
		m_preDepthPassRootSignature.Finalize(L"PreDepthPassRootSignature", rootSignatureFlags);
	}

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

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
			wstring str;
			for (int i = 0; i < 3000; i++) str += errorMsg[i];
			MessageBox(nullptr, str.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
			exit(0);
		}
		errorMessages.Reset();
		errorMessages = nullptr;

		hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS_SceneDepth", "ps_5_1", compileFlags, 0, &pixelShader, &errorMessages);
		if (FAILED(hr) && errorMessages)
		{
			const char* errorMsg = (const char*)errorMessages->GetBufferPointer();
			wstring str;
			for (int i = 0; i < 3000; i++) str += errorMsg[i];
			MessageBox(nullptr, str.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
			exit(0);
		}
#else
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));
#endif

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		m_preDepthPassPSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
		m_preDepthPassPSO.SetRootSignature(m_preDepthPassRootSignature);
		m_preDepthPassPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(vertexShader.Get()));
		m_preDepthPassPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(pixelShader.Get()));
		m_preDepthPassPSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
		m_preDepthPassPSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
		m_preDepthPassPSO.SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(TRUE, D3D12_DEPTH_WRITE_MASK_ALL,
			D3D12_COMPARISON_FUNC_LESS, TRUE, 0xFF, 0xFF,
			D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_INCR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS,
			D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_DECR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS));
		m_preDepthPassPSO.SetSampleMask(UINT_MAX);
		m_preDepthPassPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_preDepthPassPSO.SetRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
		m_preDepthPassPSO.Finalize();
	}

	// Create Depth Buffer
	m_preDepthPass.Create(L"PreDepthPassDepthBuffer", m_width, m_height, DXGI_FORMAT_D32_FLOAT);
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_preDepthPass.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	m_preDepthPassRTV.Create(L"PreDepthPassRTV", m_width, m_height, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_preDepthPassRTV.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Close the command list and execute it to begin the initial GPU setup.
	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	IGraphics::g_GraphicsCore->g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	IGraphics::g_GraphicsCore->WaitForGpu();
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

void TiledRendering::LoadImGUI()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// Setup Platform/Renderer bindings
	ImGui_ImplWin32_Init(Win32Application::GetHwnd());
	ImGui_ImplDX12_Init(IGraphics::g_GraphicsCore->g_pD3D12Device.Get(), FrameCount,
		DXGI_FORMAT_R8G8B8A8_UNORM, imGuiHeap.Get(),
		imGuiHeap->GetCPUDescriptorHandleForHeapStart(),
		imGuiHeap->GetGPUDescriptorHandleForHeapStart());
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
	memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
}

// Render the scene.
void TiledRendering::OnRender()
{
	ShowImGUI();

	// Get depth in the screen space
	PreDepthPass();

	m_LightCullingPass.ExecuteCS(m_cbvSrvUavHeap, m_preDepthPassBuffer.uSrvDescriptorOffset);

	////m_simpleCS.OnExecuteCS();

	// Record all the commands we need to render the scene into the command list.
	PopulateCommandList();


	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	IGraphics::g_GraphicsCore->g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame.
	ThrowIfFailed(IGraphics::g_GraphicsCore->g_pSwapChain->Present(1, 0));

	//WaitForPreviousFrame();
	IGraphics::g_GraphicsCore->MoveToNextFrame();
}

void TiledRendering::OnDestroy()
{
	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	//WaitForPreviousFrame();
	IGraphics::g_GraphicsCore->WaitForGpu();


	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();


	m_pModel.reset();

	//CloseHandle(m_fenceEvent);
	IGraphics::g_GraphicsCore->Shutdown();
}

void TiledRendering::PreDepthPass()
{
	ThrowIfFailed(IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex]->Reset());
	ThrowIfFailed(m_commandList->Reset(IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), m_preDepthPassPSO.GetPSO()));

	// Transmit the state of the depth buffer from SRV to DSV
	m_commandList->ResourceBarrier(
		1, 
		&CD3DX12_RESOURCE_BARRIER::Transition(m_preDepthPassBuffer.pResource.Get(), m_preDepthPassBuffer.mUsageState, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	m_preDepthPassBuffer.mUsageState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

	// Set necessary state.
	m_commandList->SetGraphicsRootSignature(m_preDepthPassRootSignature.GetSignature());

	ID3D12DescriptorHeap* ppHeaps1[] = { m_cbvSrvUavHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps1), ppHeaps1);

	D3D12_GPU_DESCRIPTOR_HANDLE cbvSrvUavHandle = m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
	m_commandList->SetGraphicsRootDescriptorTable(
		e_rootParameterCB,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, m_modelConstantBuffer.uCbvDescriptorOffset, m_cbvSrvUavDescriptorSize));

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_preDepthPassRTV.GetRTV();
	auto dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	//auto dsvHandle = m_preDepthPass.GetDSV();
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	//m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, m_preDepthPass.GetClearDepth(), m_preDepthPass.GetClearStencil(), 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBuffer.VertexBufferView());
	m_commandList->IASetIndexBuffer(&m_indexBuffer.IndexBufferView());

	m_commandList->DrawIndexedInstanced((UINT)(m_pModel->m_vecIndexData.size()), 1, 0, 0, 0);

	// Transmit the state of the depth buffer from SRV to DSV
	m_commandList->ResourceBarrier(
		1, 
		&CD3DX12_RESOURCE_BARRIER::Transition(m_preDepthPassBuffer.pResource.Get(), m_preDepthPassBuffer.mUsageState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
	m_preDepthPassBuffer.mUsageState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	ThrowIfFailed(m_commandList->Close());

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	IGraphics::g_GraphicsCore->g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	IGraphics::g_GraphicsCore->WaitForGpu();
}

void TiledRendering::PopulateCommandList()
{
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	ThrowIfFailed(IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex]->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ThrowIfFailed(m_commandList->Reset(IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), m_scenePSO.GetPSO()));

	m_LightCullingPass.ResourceTransitionForRender(m_commandList);

	// Set necessary state.
	m_commandList->SetGraphicsRootSignature(m_sceneOpaqueRootSignature.GetSignature());

	ID3D12DescriptorHeap* ppHeaps1[] = { m_cbvSrvUavHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps1), ppHeaps1);

	D3D12_GPU_DESCRIPTOR_HANDLE cbvSrvUavHandle = m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
	m_commandList->SetGraphicsRootDescriptorTable(
		e_ModelTexRootParameterSRV,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, m_modelTexture.uSrvDescriptorOffset, m_cbvSrvUavDescriptorSize));
	m_commandList->SetGraphicsRootDescriptorTable(
		e_rootParameterCB,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, m_modelConstantBuffer.uCbvDescriptorOffset, m_cbvSrvUavDescriptorSize));
	m_commandList->SetGraphicsRootDescriptorTable(
		e_LightGridRootParameterSRV,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, m_LightCullingPass.GetOpaqueLightGridSRVHeapOffset(), m_cbvSrvUavDescriptorSize));
	m_commandList->SetGraphicsRootShaderResourceView(
		e_LightIndexRootParameterSRV,
		m_LightCullingPass.GetOpaqueLightLightIndexList());
	m_commandList->SetGraphicsRootShaderResourceView(
		e_LightBufferRootParameterSRV,
		m_LightCullingPass.GetLightsBuffer());

	// Indicate that the back buffer will be used as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(IGraphics::g_GraphicsCore->m_renderTargets[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(IGraphics::g_GraphicsCore->m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), IGraphics::g_GraphicsCore->s_FrameIndex, IGraphics::g_GraphicsCore->m_rtvDescriptorSize);
	//auto dsvHandle = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();
	auto dsvHandle = m_sceneDepthBuffer.GetDSV();
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);
	//m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	//m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, m_sceneDepthBuffer.GetClearDepth(), m_sceneDepthBuffer.GetClearStencil(), 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBuffer.VertexBufferView());
	m_commandList->IASetIndexBuffer(&m_indexBuffer.IndexBufferView());

	m_commandList->DrawIndexedInstanced((UINT)(m_pModel->m_vecIndexData.size()), 1, 0, 0, 0);
	//m_commandList->DrawInstanced((UINT)(m_pModel->m_vecVertexData.size()), 1, 0, 0);

	ImGui::Render();
	m_commandList->SetDescriptorHeaps(1, imGuiHeap.GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

	// Indicate that the back buffer will now be used to present.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(IGraphics::g_GraphicsCore->m_renderTargets[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	m_LightCullingPass.ResourceTransitionForCS(m_commandList);

	ThrowIfFailed(m_commandList->Close());
}

// Lights generation
void TiledRendering::GenerateLights(uint32_t numLights)
{
	uint32_t lightsPerDimension = static_cast<uint32_t>(ceil(cbrt(numLights)));
	// TODO hard code the light spawing space here
	XMFLOAT3 minPoint(-10.0f, -10.0f, -10.0f);
	XMFLOAT3 maxPoint(10.0f, 10.0f, 10.0f);
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
		light.m_Color.x = max(1.0f, (1.0f * rand() / INT_MAX) + 0.1);
		light.m_Color.y = max(1.0f, (1.0f * rand() / INT_MAX) + 0.1);
		light.m_Color.z = max(1.0f, (1.0f * rand() / INT_MAX) + 0.1);
		light.m_Color.w = 1.0f;

		light.m_DirectionWS = XMFLOAT4(
			-light.m_PositionWS.x,
			-light.m_PositionWS.y,
			-light.m_PositionWS.z,
			0.0f);
		float minRange(0.1f), maxRange(1000.0f);
		light.m_Range = minRange + (1.0f * rand() / INT_MAX) * maxRange;
		float minSpotAngle(0.1f * XM_PI / 180.0f), maxSpotAngle(30.0f * XM_PI / 180.0f);
		light.m_SpotlightAngle = minSpotAngle + (1.0f * rand() / INT_MAX) * maxSpotAngle;

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

		light.m_Range = 1.0f;

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
}
