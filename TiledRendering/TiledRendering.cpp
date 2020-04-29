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
	//LoadComputeShaderResources();
	m_simpleCS.OnInit();
}

// Load the rendering pipeline dependencies.
void TiledRendering::LoadPipeline()
{
	UINT dxgiFactoryFlags = 0;

	IGraphics::g_GraphicsCore->g_hwnd = Win32Application::GetHwnd();
	IGraphics::g_GraphicsCore->Initialize();


	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};

		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 1;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvHeap)) != S_OK);

		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = e_numRootParameters;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_cbvSrvHeap)) != S_OK);
		m_cbvSrvUavDescriptorSize = IGraphics::g_GraphicsCore->g_pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		desc.NumDescriptors = 1;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_DSVHeap)) != S_OK);
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
		
		m_rootSignature.Reset(2, 1);
		m_rootSignature.InitStaticSampler(0, non_static_sampler);
		//m_testRootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
		m_rootSignature[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_VERTEX);
		m_rootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
		m_rootSignature.Finalize(L"TestRootSignature", rootSignatureFlags);
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
		HRESULT hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &errorMessages);
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

		hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &errorMessages);
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
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));
#endif

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			//{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		m_pipelineState.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
		m_pipelineState.SetRootSignature(m_rootSignature);
		m_pipelineState.SetVertexShader(CD3DX12_SHADER_BYTECODE(vertexShader.Get()));
		m_pipelineState.SetPixelShader(CD3DX12_SHADER_BYTECODE(pixelShader.Get()));
		m_pipelineState.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
		m_pipelineState.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
		m_pipelineState.SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(TRUE, D3D12_DEPTH_WRITE_MASK_ALL, 
			D3D12_COMPARISON_FUNC_LESS, TRUE, 0xFF, 0xFF,
			D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_INCR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS,
			D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_DECR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS));
		m_pipelineState.SetSampleMask(UINT_MAX);
		m_pipelineState.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_pipelineState.SetRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
		m_pipelineState.Finalize();

	}

	// Create the command list.
	//ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator[m_frameIndex].Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
	ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), m_pipelineState.GetPSO(), IID_PPV_ARGS(&IGraphics::g_GraphicsCore->g_commandList)));
	m_commandList = IGraphics::g_GraphicsCore->g_commandList;

	// Create the vertex buffer.
	ComPtr<ID3D12Resource> indexUploadHeap;
	m_pModel = make_shared<Model>();
	{
		m_pModel->Load("bunny.obj");
		m_vertexBuffer.Create(L"BunnyVertexBuffer", m_pModel->m_vecVertexData.size(), sizeof(Vertex), m_pModel->m_vecVertexData.data());
		m_indexBuffer.Create(L"BunnyIndexBuffer", m_pModel->m_vecIndexData.size(), sizeof(uint32_t), m_pModel->m_vecIndexData.data());
	}

	// Create Depth Buffer
	m_testDepthBuffer.Create(L"TestDepthBuffer", m_width, m_height, DXGI_FORMAT_D32_FLOAT);
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_testDepthBuffer.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	{
		D3D12_CLEAR_VALUE optimizedClearValue = {};
		optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		optimizedClearValue.DepthStencil = { 1.0f, 0 };

		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height,
			1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&optimizedClearValue,
		IID_PPV_ARGS(&m_depthBuffer)
		));

		// Update the depth-stencil view.
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = DXGI_FORMAT_D32_FLOAT;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = D3D12_DSV_FLAG_NONE;

		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDepthStencilView(m_depthBuffer.Get(), &dsv,
			m_DSVHeap->GetCPUDescriptorHandleForHeapStart());
	}

	// Create the constant buffer
	{
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_constantBuffer)));

		// Describe and create a constant buffer view.
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(CBuffer) + 255) & ~255;    // CB size is required to be 256-byte aligned.
		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateConstantBufferView(&cbvDesc, m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart());

		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin)));
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
			IID_PPV_ARGS(&m_texture)));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);

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

		UpdateSubresources(m_commandList.Get(), m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		// Describe and create a SRV for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), e_iSRV, m_cbvSrvUavDescriptorSize);
		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(m_texture.Get(), &srvDesc, srvHandle);
	}

	// Close the command list and execute it to begin the initial GPU setup.
	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	IGraphics::g_GraphicsCore->g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);



	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		IGraphics::g_GraphicsCore->WaitForGpu();

		// Setup the camera's view parameters
		static const XMVECTORF32 s_Eye = { 0.0f, 0.0f, -6.0f, 0.f };
		static const XMVECTORF32 s_At = { 0.0f, 0.0f, 0.0f, 0.f };
		m_modelViewCamera.SetViewParams(s_Eye, s_At);
		// Setup the camera's projection parameters
		float fAspectRatio = m_width / (float)m_height;
		m_modelViewCamera.SetProjParams(XM_PI / 4, fAspectRatio, 0.01f, 10000.0f);
		m_modelViewCamera.SetWindow(m_width, m_height);
		m_modelViewCamera.SetButtonMasks(MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON);

		LoadImGUI();
	}
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
		DXGI_FORMAT_R8G8B8A8_UNORM, m_srvHeap.Get(),
		m_srvHeap->GetCPUDescriptorHandleForHeapStart(),
		m_srvHeap->GetGPUDescriptorHandleForHeapStart());
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
	m_constantBufferData.worldViewProjMatrix = (world * view * proj);
	memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
}

// Render the scene.
void TiledRendering::OnRender()
{
	ShowImGUI();

	m_simpleCS.OnExecuteCS();

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

void TiledRendering::PopulateCommandList()
{
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	ThrowIfFailed(IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex]->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ThrowIfFailed(m_commandList->Reset(IGraphics::g_GraphicsCore->m_commandAllocator[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), m_pipelineState.GetPSO()));


	// Set necessary state.
	m_commandList->SetGraphicsRootSignature(m_rootSignature.GetSignature());

	ID3D12DescriptorHeap* ppHeaps1[] = { m_cbvSrvHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps1), ppHeaps1);

	D3D12_GPU_DESCRIPTOR_HANDLE cbvSrvUavHandle = m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart();
	m_commandList->SetGraphicsRootDescriptorTable(
		e_rootParameterSRV,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, e_iSRV, m_cbvSrvUavDescriptorSize));
	m_commandList->SetGraphicsRootDescriptorTable(
		e_rootParameterCB,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(cbvSrvUavHandle, e_iCB, m_cbvSrvUavDescriptorSize));


	// Indicate that the back buffer will be used as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(IGraphics::g_GraphicsCore->m_renderTargets[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(IGraphics::g_GraphicsCore->m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), IGraphics::g_GraphicsCore->s_FrameIndex, IGraphics::g_GraphicsCore->m_rtvDescriptorSize);
	//auto dsvHandle = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();
	auto dsvHandle = m_testDepthBuffer.GetDSV();
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);
	//m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	//m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, m_testDepthBuffer.GetClearDepth(), m_testDepthBuffer.GetClearStencil(), 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBuffer.VertexBufferView());
	m_commandList->IASetIndexBuffer(&m_indexBuffer.IndexBufferView());


	m_commandList->DrawIndexedInstanced((UINT)(m_pModel->m_vecIndexData.size()), 1, 0, 0, 0);
	//m_commandList->DrawInstanced((UINT)(m_pModel->m_vecVertexData.size()), 1, 0, 0);

	ImGui::Render();
	m_commandList->SetDescriptorHeaps(1, m_srvHeap.GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());


	// Indicate that the back buffer will now be used to present.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(IGraphics::g_GraphicsCore->m_renderTargets[IGraphics::g_GraphicsCore->s_FrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_commandList->Close());
}
