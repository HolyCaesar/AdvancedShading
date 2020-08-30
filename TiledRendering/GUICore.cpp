#include "stdafx.h"
#include "GUICore.h"

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "Win32FrameWork.h"
#include "TiledRendering.h"

#include <iostream>
#include <ctype.h>          // toupper
#include <limits.h>         // INT_MIN, INT_MAX
#include <math.h>           // sqrtf, powf, cosf, sinf, floorf, ceilf
#include <stdio.h>          // vsnprintf, sscanf, printf
#include <stdlib.h>         // NULL, malloc, free, atoi


#if defined(_MSC_VER) && _MSC_VER <= 1500 // MSVC 2008 or earlier
#include <stddef.h>         // intptr_t
#else
#include <stdint.h>         // intptr_t
#endif

// Play it nice with Windows users (Update: May 2018, Notepad now supports Unix-style carriage returns!)
#ifdef _WIN32
#define IM_NEWLINE  "\r\n"
#else
#define IM_NEWLINE  "\n"
#endif

// Helpers
#if defined(_MSC_VER) && !defined(snprintf)
#define snprintf    _snprintf
#endif
#if defined(_MSC_VER) && !defined(vsnprintf)
#define vsnprintf   _vsnprintf
#endif



namespace IGuiCore
{
	using namespace std;
	// App pointer
	Win32FrameWork* g_appPtr = nullptr;
	DX12TextureConverter* g_imGuiTexConverter = nullptr;

	//
	// \Non Global variable 
	//
	// Descriptor Heap for ImGUI
	ComPtr<ID3D12DescriptorHeap> imGuiHeap;
	uint32_t g_heapPtr;
	unordered_map<SRVList, DX12Resource> g_srvDict;


	//std::vector<UINT8> GenerateTextureData()
	std::vector<float> GenerateTextureData()
	{
		uint32_t TextureWidth = 80;
		uint32_t TextureHeight = 45;
		uint32_t TexturePixelSize = 2;

		const UINT rowPitch = TextureWidth * TexturePixelSize;
		const UINT cellPitch = rowPitch >> 3;        // The width of a cell in the checkboard texture.
		const UINT cellHeight = TextureWidth >> 3;    // The height of a cell in the checkerboard texture.
		const UINT textureSize = rowPitch * TextureHeight;

		//std::vector<UINT8> data(textureSize);
		std::vector<float> data(textureSize);
		//UINT8* pData = &data[0];
		float* pData = &data[0];

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

	void TestCode()
	{
		ComPtr<ID3D12Resource> textureUploadHeap;
		uint32_t TextureWidth = 80;
		uint32_t TextureHeight = 45;
		uint32_t TexturePixelSize = 4;

		// Describe and create a Texture2D.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		//textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Format = DXGI_FORMAT_R32G32_UINT;
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
			IID_PPV_ARGS(&g_srvDict[TestSRV].pResource)));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(g_srvDict[TestSRV].pResource.Get(), 0, 1);

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
		//std::vector<UINT8> texture = GenerateTextureData();
		std::vector<float> texture = GenerateTextureData();

		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = &texture[0];
		textureData.RowPitch = TextureWidth * TexturePixelSize;
		textureData.SlicePitch = textureData.RowPitch * TextureHeight;

		// Create a temporary command queue to do the copy with
		ID3D12Fence* fence = NULL;
		HRESULT hr = IGraphics::g_GraphicsCore->g_pD3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		IM_ASSERT(SUCCEEDED(hr));

		HANDLE event = CreateEvent(0, 0, 0, 0);
		IM_ASSERT(event != NULL);

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 1;

		ID3D12CommandQueue* cmdQueue = NULL;
		hr = IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue));
		IM_ASSERT(SUCCEEDED(hr));

		ID3D12CommandAllocator* cmdAlloc = NULL;
		hr = IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
		IM_ASSERT(SUCCEEDED(hr));

		ID3D12GraphicsCommandList* cmdList = NULL;
		hr = IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, NULL, IID_PPV_ARGS(&cmdList));
		IM_ASSERT(SUCCEEDED(hr));

		//cmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, NULL);
		//cmdList->ResourceBarrier(1, &barrier);

		UpdateSubresources(cmdList, g_srvDict[TestSRV].pResource.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_srvDict[TestSRV].pResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));


		hr = cmdList->Close();
		IM_ASSERT(SUCCEEDED(hr));

		// Execute the copy
		cmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&cmdList);
		hr = cmdQueue->Signal(fence, 1);
		IM_ASSERT(SUCCEEDED(hr));

		// Wait for everything to complete
		fence->SetEventOnCompletion(1, event);
		WaitForSingleObject(event, INFINITE);

		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
			imGuiHeap->GetCPUDescriptorHandleForHeapStart(),
			TestSRV,
			32);

		// Describe and create a SRV for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		//srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 0, 0, 0);
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(g_srvDict[TestSRV].pResource.Get(), &srvDesc, srvHandle);

		// Tear down our temporary command queue and release the upload resource
		cmdList->Release();
		cmdAlloc->Release();
		cmdQueue->Release();
		CloseHandle(event);
		fence->Release();
	}

	void TestCopy()
	{
		uint32_t TextureWidth = 80;
		uint32_t TextureHeight = 45;
		uint32_t TexturePixelSize = 4;

		// Describe and create a Texture2D.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		//textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Format = DXGI_FORMAT_R32G32_UINT;
		textureDesc.Width = TextureWidth;
		textureDesc.Height = TextureHeight;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		//ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
		//	&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		//	D3D12_HEAP_FLAG_NONE,
		//	&textureDesc,
		//	D3D12_RESOURCE_STATE_COPY_DEST,
		//	nullptr,
		//	IID_PPV_ARGS(&g_srvDict[TestSRV1].pResource)));


		CreateGuiTexture2DSRV(L"TestSao", TextureWidth, TextureHeight,
			sizeof(XMFLOAT4), textureDesc.Format, TestSRV1);

		// Create a temporary command queue to do the copy with
		ID3D12Fence* fence = NULL;
		HRESULT hr = IGraphics::g_GraphicsCore->g_pD3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		IM_ASSERT(SUCCEEDED(hr));

		HANDLE event = CreateEvent(0, 0, 0, 0);
		IM_ASSERT(event != NULL);

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 1;

		ID3D12CommandQueue* cmdQueue = NULL;
		hr = IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue));
		IM_ASSERT(SUCCEEDED(hr));

		ID3D12CommandAllocator* cmdAlloc = NULL;
		hr = IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
		IM_ASSERT(SUCCEEDED(hr));

		ID3D12GraphicsCommandList* cmdList = NULL;
		hr = IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, NULL, IID_PPV_ARGS(&cmdList));
		IM_ASSERT(SUCCEEDED(hr));



		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_srvDict[TestSRV].pResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE));
		D3D12_TEXTURE_COPY_LOCATION DestLocation =
		{
			g_srvDict[TestSRV1].pResource.Get(),
			D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
			0
		};

		D3D12_TEXTURE_COPY_LOCATION SrcLocation =
		{
			g_srvDict[TestSRV].pResource.Get(),
			D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
			0
		};

		cmdList->CopyTextureRegion(&DestLocation, 0, 0, 0, &SrcLocation, nullptr);


		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_srvDict[TestSRV].pResource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_srvDict[TestSRV1].pResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));



		hr = cmdList->Close();
		IM_ASSERT(SUCCEEDED(hr));

		// Execute the copy
		cmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&cmdList);
		hr = cmdQueue->Signal(fence, 1);
		IM_ASSERT(SUCCEEDED(hr));

		// Wait for everything to complete
		fence->SetEventOnCompletion(1, event);
		WaitForSingleObject(event, INFINITE);

		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
			imGuiHeap->GetCPUDescriptorHandleForHeapStart(),
			TestSRV1,
			32);

		// Describe and create a SRV for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		//srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 0, 0, 0);
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(g_srvDict[TestSRV1].pResource.Get(), &srvDesc, srvHandle);

		// Tear down our temporary command queue and release the upload resource
		cmdList->Release();
		cmdAlloc->Release();
		cmdQueue->Release();
		CloseHandle(event);
		fence->Release();
	}








	void Init(Win32FrameWork* appPtr)
	{
		//g_appPtr.reset(appPtr);
		g_appPtr = appPtr;
		g_heapPtr = 1; // 0 slot is reserved for imGUI

		// Create descriptor heaps.
		{
			// Describe and create a render target view (RTV) descriptor heap.
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};

			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = 255;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&imGuiHeap)) != S_OK);
		}

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
		ImGui_ImplDX12_Init(IGraphics::g_GraphicsCore->g_pD3D12Device.Get(), SWAP_CHAIN_BUFFER_COUNT,
			DXGI_FORMAT_R8G8B8A8_UNORM, imGuiHeap.Get(),
			imGuiHeap->GetCPUDescriptorHandleForHeapStart(),
			imGuiHeap->GetGPUDescriptorHandleForHeapStart());

		TestCode();
		TestCopy();

		g_imGuiTexConverter = new DX12TextureConverter();
	}

	void ShowImGUI()
	{
		static bool show_demo_window = true;
		static bool show_another_window = false;
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

		// Start the Dear ImGui frame
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();


		IGuiCore::ShowMainGui();
		return;

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

	void RenderImGUI(GraphicsContext& gfxContext)
	{
		ImGui::Render();
		gfxContext.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, imGuiHeap.Get());
		//m_commandList->SetDescriptorHeaps(1, imGuiHeap.GetAddressOf());
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), gfxContext.GetCommandList());
	}

	void Terminate() 
	{ 
		g_appPtr = nullptr; 
		delete g_imGuiTexConverter;
	}

	// Control Variables
	bool g_bEnableGui = true;
	bool g_bShowMainMenuBar = true;

	void ShowMainMenuBar()
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				ShowMainMenuFile();
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Edit"))
			{
				if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
				if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
				ImGui::Separator();
				if (ImGui::MenuItem("Cut", "CTRL+X")) {}
				if (ImGui::MenuItem("Copy", "CTRL+C")) {}
				if (ImGui::MenuItem("Paste", "CTRL+V")) {}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
	}

	void ShowMainGui()
	{
		// Exceptionally add an extra assert here for people confused about initial Dear ImGui setup
		// Most ImGui functions would normally just crash if the context is missing.
		IM_ASSERT(ImGui::GetCurrentContext() != NULL && "Missing dear imgui context. Refer to examples app!");

		// Decide which module to display
		if (!g_bEnableGui) return;
		//if (g_bShowMainMenuBar) ShowMainMenuBar();

		// Demonstrate the various window flags. Typically you would just use the default!
		static bool no_titlebar = false;
		static bool no_scrollbar = false;
		static bool no_menu = false;
		static bool no_move = false;
		static bool no_resize = false;
		static bool no_collapse = false;
		static bool no_close = false;
		static bool no_nav = false;
		static bool no_background = false;
		static bool no_bring_to_front = false;

		ImGuiWindowFlags window_flags = 0;
		if (no_titlebar)        window_flags |= ImGuiWindowFlags_NoTitleBar;
		if (no_scrollbar)       window_flags |= ImGuiWindowFlags_NoScrollbar;
		if (!no_menu)           window_flags |= ImGuiWindowFlags_MenuBar;
		if (no_move)            window_flags |= ImGuiWindowFlags_NoMove;
		if (no_resize)          window_flags |= ImGuiWindowFlags_NoResize;
		if (no_collapse)        window_flags |= ImGuiWindowFlags_NoCollapse;
		if (no_nav)             window_flags |= ImGuiWindowFlags_NoNav;
		if (no_background)      window_flags |= ImGuiWindowFlags_NoBackground;
		if (no_bring_to_front)  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
		if (no_close)           g_bEnableGui = NULL; // Don't pass our bool* to Begin // TODO: g_bEnableGui is a pointer

		// We specify a default position/size in case there's no data in the .ini file.
		// We only do it to make the demo applications a little more welcoming, but typically this isn't required.
		ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);

		// Main body of the Demo window starts here.
		if (!ImGui::Begin("Rendering Demo", &g_bEnableGui, window_flags))
		{
			// Early out if the window is collapsed, as an optimization.
			ImGui::End();
			return;
		}

		ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

		/**************************/
		// Menu Bar
		/**************************/
		// Rendering Technique apps (accessible from the "Tools" menu)
		//static bool show_app_metrics = false;
		//static bool show_app_style_editor = false;
		static bool show_app_about = false;
		if (show_app_about) ShowAboutWindow(&show_app_about);

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("Menu"))
			{
				ShowMainMenuFile();
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Examples"))
			{
				//ImGui::MenuItem("Main menu bar", NULL, &show_app_main_menu_bar);
				//ImGui::MenuItem("Console", NULL, &show_app_console);
				//ImGui::MenuItem("Log", NULL, &show_app_log);
				//ImGui::MenuItem("Simple layout", NULL, &show_app_layout);
				//ImGui::MenuItem("Property editor", NULL, &show_app_property_editor);
				//ImGui::MenuItem("Long text display", NULL, &show_app_long_text);
				//ImGui::MenuItem("Auto-resizing window", NULL, &show_app_auto_resize);
				//ImGui::MenuItem("Constrained-resizing window", NULL, &show_app_constrained_resize);
				//ImGui::MenuItem("Simple overlay", NULL, &show_app_simple_overlay);
				//ImGui::MenuItem("Manipulating window titles", NULL, &show_app_window_titles);
				//ImGui::MenuItem("Custom rendering", NULL, &show_app_custom_rendering);
				//ImGui::MenuItem("Documents", NULL, &show_app_documents);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Tools"))
			{
				//ImGui::MenuItem("Metrics", NULL, &show_app_metrics);
				//ImGui::MenuItem("Style Editor", NULL, &show_app_style_editor);
				ImGui::MenuItem("About Dear ImGui", NULL, &show_app_about);
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		/**************************/
		// Window options
		/**************************/
		if (ImGui::CollapsingHeader("Window options"))
		{
			ImGui::Checkbox("No titlebar", &no_titlebar); ImGui::SameLine(150);
			ImGui::Checkbox("No scrollbar", &no_scrollbar); ImGui::SameLine(300);
			ImGui::Checkbox("No menu", &no_menu);
			ImGui::Checkbox("No move", &no_move); ImGui::SameLine(150);
			ImGui::Checkbox("No resize", &no_resize); ImGui::SameLine(300);
			ImGui::Checkbox("No collapse", &no_collapse);
			ImGui::Checkbox("No close", &no_close); ImGui::SameLine(150);
			ImGui::Checkbox("No nav", &no_nav); ImGui::SameLine(300);
			ImGui::Checkbox("No background", &no_background);
			ImGui::Checkbox("No bring to front", &no_bring_to_front);
		}

		/**************************/
		// Functional options
		/**************************/
		static bool show_tiled_forward_rendering = false;
		static bool show_normal_rendering = false;
		static bool show_deferred_rendering_ = false;
		if (ImGui::CollapsingHeader("Rendering Technique"))
		{
			static int rendering_technique = 2;
			ImGui::RadioButton("Normal Rendering", &rendering_technique, 0); //ImGui::SameLine();
			ImGui::RadioButton("Deferred Rendering", &rendering_technique, 1); //ImGui::SameLine();
			ImGui::RadioButton("Tiled Forward Rendering", &rendering_technique, 2);

			switch (rendering_technique)
			{
			case 0:
			{
				// TODO
				show_tiled_forward_rendering = false;
				show_normal_rendering = true;
				show_deferred_rendering_ = false;
				break;
			}
			case 1:
			{
				// TODO
				show_tiled_forward_rendering = false;
				show_normal_rendering = false;
				show_deferred_rendering_ = true;
				break;
			}
			case 2:
			{
				// TODO: some intialization to the main program
				show_tiled_forward_rendering = true;
				show_normal_rendering = false;
				show_deferred_rendering_ = false;
				break;
			}
			}
		}

		/**************************/
		// Tiled Forward rendering 
		/**************************/
		if (show_tiled_forward_rendering) ShowForwardPlusWidgets();

		// End of ShowDemoWindow()
		ImGui::End();
	}

	//
	// Customized Functions
	//
	void ShowForwardPlusWidgets()
	{
		if (ImGui::CollapsingHeader("Tiled Forward Rendering"))
		{
			return;
		}

		if (ImGui::TreeNode("Grid Frustum Pass"))
		{
			ImGui::Separator();
			ImGui::Text("Resources Preview");
			auto appPtr = reinterpret_cast<TiledRendering*>(g_appPtr);
			//auto appPtr = dynamic_pointer_cast<TiledRendering>(g_appPtr);
			// Checkout the tutorial https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples
			CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(
				imGuiHeap->GetGPUDescriptorHandleForHeapStart(),
				2,
				32);
			CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle1(
				imGuiHeap->GetGPUDescriptorHandleForHeapStart(),
				3,
				32);
			CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle2(
				imGuiHeap->GetGPUDescriptorHandleForHeapStart(),
				1,
				32);
			ImVec2 uv_min = ImVec2(0.0f, 0.0f);                 // Top-left
			ImVec2 uv_max = ImVec2(1.0f, 1.0f);                 // Lower-right
			ImGuiIO& io = ImGui::GetIO();
			ImGui::Image((ImTextureID)io.Fonts->TexID, ImVec2(512, 128), uv_min, uv_max);
			ImGui::Image((ImTextureID)srvHandle.ptr, ImVec2(128, 128), uv_min, uv_max);
			ImGui::Image((ImTextureID)srvHandle2.ptr, ImVec2(128, 128), uv_min, uv_max);
			ImGui::Image((ImTextureID)srvHandle1.ptr, ImVec2(128, 128), uv_min, uv_max);

			ImGui::Separator();
			bool hasChange = false;
			static int LightCnt = 1;
			ImGui::Text("Light Number:");
			hasChange = hasChange || ImGui::SliderInt("", &LightCnt, 1, 10); // LightCnt cannot be lower than 0

			static float LowerLightSpawnPtec4a[4] = { -10.0f, -10.0f, -10.0f, 0.0f };
			static float UpperLightSpawnPtec4a[4] = { 10.0f, 10.0f, 10.0f, 0.0f };
			hasChange = hasChange || ImGui::InputFloat3("Light Spawn Lower Point:", LowerLightSpawnPtec4a);
			hasChange = hasChange || ImGui::InputFloat3("Light Spawn Upper Point:", UpperLightSpawnPtec4a);

			static float MinLightRange = 0.0f;
			static float MaxLightRange = 100.0f;
			hasChange = hasChange || ImGui::InputFloat("Min Light Range", &MinLightRange, 0.01f, 10.0f, "%.3f", 0);
			if (MaxLightRange < MinLightRange) MaxLightRange = MinLightRange;
			hasChange = hasChange || ImGui::InputFloat("Max Light Range", &MaxLightRange, 0.01f, 10.0f, "%.3f");

			static float MinSpotLightAngle = 0.0f;
			static float MaxSpotLightAngle = 30.0f;
			hasChange = hasChange || ImGui::InputFloat("Min Spot Light Angle", &MinSpotLightAngle, 0.01f, 90.0f, "%.3f");
			if (MaxSpotLightAngle < MinSpotLightAngle) MaxSpotLightAngle = MinSpotLightAngle;
			hasChange = hasChange || ImGui::InputFloat("Max Spot Light Angle", &MaxSpotLightAngle, 0.01f, 90.0f, "%.3f");

			if (hasChange)
			{
				// TODO update light forward rendering code
				appPtr->GenerateLights((uint32_t)LightCnt,
					XMFLOAT3(LowerLightSpawnPtec4a[0], LowerLightSpawnPtec4a[1], LowerLightSpawnPtec4a[2]),
					XMFLOAT3(UpperLightSpawnPtec4a[0], UpperLightSpawnPtec4a[1], UpperLightSpawnPtec4a[2]),
					MinLightRange, MaxLightRange, MinSpotLightAngle, MaxSpotLightAngle);

				hasChange = false;
			}

			ImGui::TreePop();
		}
	}

	//
	// Helper Functions
	//
	void ShowMainMenuFile()
	{
		ImGui::MenuItem("(dummy menu)", NULL, false, false);
		if (ImGui::MenuItem("New")) {}
		if (ImGui::MenuItem("Open", "Ctrl+O")) {}
		if (ImGui::BeginMenu("Open Recent"))
		{
			ImGui::MenuItem("fish_hat.c");
			ImGui::MenuItem("fish_hat.inl");
			ImGui::MenuItem("fish_hat.h");
			if (ImGui::BeginMenu("More.."))
			{
				ImGui::MenuItem("Hello");
				ImGui::MenuItem("Sailor");
				if (ImGui::BeginMenu("Recurse.."))
				{
					ShowMainMenuFile();
					ImGui::EndMenu();
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem("Save", "Ctrl+S")) {}
		if (ImGui::MenuItem("Save As..")) {}

		ImGui::Separator();
		if (ImGui::BeginMenu("Options"))
		{
			static bool enabled = true;
			ImGui::MenuItem("Enabled", "", &enabled);
			ImGui::BeginChild("child", ImVec2(0, 60), true);
			for (int i = 0; i < 10; i++)
				ImGui::Text("Scrolling Text %d", i);
			ImGui::EndChild();
			static float f = 0.5f;
			static int n = 0;
			ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
			ImGui::InputFloat("Input", &f, 0.1f);
			ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Colors"))
		{
			float sz = ImGui::GetTextLineHeight();
			for (int i = 0; i < ImGuiCol_COUNT; i++)
			{
				const char* name = ImGui::GetStyleColorName((ImGuiCol)i);
				ImVec2 p = ImGui::GetCursorScreenPos();
				ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), ImGui::GetColorU32((ImGuiCol)i));
				ImGui::Dummy(ImVec2(sz, sz));
				ImGui::SameLine();
				ImGui::MenuItem(name);
			}
			ImGui::EndMenu();
		}

		// Here we demonstrate appending again to the "Options" menu (which we already created above)
		// Of course in this demo it is a little bit silly that this function calls BeginMenu("Options") twice.
		// In a real code-base using it would make senses to use this feature from very different code locations.
		if (ImGui::BeginMenu("Options")) // <-- Append!
		{
			static bool b = true;
			ImGui::Checkbox("SomeOption", &b);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Disabled", false)) // Disabled
		{
			IM_ASSERT(0);
		}
		if (ImGui::MenuItem("Checked", NULL, true)) {}
		if (ImGui::MenuItem("Quit", "Alt+F4")) {}
	}

	void ShowAboutWindow(bool* p_open)
	{
		if (!ImGui::Begin("About", p_open, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::End();
			return;
		}
		ImGui::Text("This Demo is developed by Zhihang Deng");
		ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
		ImGui::Separator();
		ImGui::Text("By Omar Cornut and all Dear ImGui contributors.");
		ImGui::Text("Dear ImGui is licensed under the MIT License, see LICENSE for more information.");


		ImGui::End();
	}

	void CreateGuiTexture2DSRV(wstring name, uint32_t width, uint32_t height,
		uint32_t elementSize, DXGI_FORMAT format, SRVList srvOffset)
	{
		D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_NONE);
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&g_srvDict[srvOffset].pResource)));

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		//srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 0, 0, 0);
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
			imGuiHeap->GetCPUDescriptorHandleForHeapStart(),
			srvOffset,
			32);
		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(g_srvDict[srvOffset].pResource.Get(), &srvDesc, srvHandle);
		g_srvDict[srvOffset].uSrvDescriptorOffset = g_heapPtr;
		++g_heapPtr;

		//D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		//uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		//uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		//uavDesc.Buffer.FirstElement = 0;
		//uavDesc.Buffer.NumElements = width * height;
		//uavDesc.Buffer.StructureByteStride = elementSize;
		//uavDesc.Buffer.CounterOffsetInBytes = width * height * elementSize;
		//uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		//CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(
		//	imGuiHeap->GetCPUDescriptorHandleForHeapStart(),
		//	srvOffset,
		//	32);
		//IGraphics::g_GraphicsCore->g_pD3D12Device->CreateUnorderedAccessView(pResource.pResource.Get(), nullptr, nullptr, uavHandle);
		//pResource.mUsageState = D3D12_RESOURCE_STATE_COMMON;
		//pResource.uUavDescriptorOffset = g_heapPtr;
		//++g_heapPtr;

		g_srvDict[srvOffset].pResource->SetName(name.c_str());
	}
}


//
// \ DX12TextureConverter
//
DX12TextureConverter::DX12TextureConverter()
{
	m_constantBufferData.worldMatrix = XMMatrixIdentity();
	m_constantBufferData.viewMatrix = XMMatrixIdentity();
	m_constantBufferData.worldViewProjMatrix = XMMatrixIdentity();

	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = 255;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		m_rtvDescriptorSize = IGraphics::g_GraphicsCore->g_pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		m_rtvHeapPtr = 0;
	}
}

DX12TextureConverter::~DX12TextureConverter()
{
	for (auto& pso : m_psoContainer)
		pso.second.DestroyAll();

	m_outputRes.clear();
}

void DX12TextureConverter::Convert(GraphicsContext& gfxContext)
{
	gfxContext.SetRootSignature(m_sceneRootSignature);
	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	gfxContext.SetVertexBuffer(0, m_vertexBuffer.VertexBufferView());

	gfxContext.SetDynamicConstantBufferView(0, sizeof(m_constantBufferData), &m_constantBufferData);
	gfxContext.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, IGuiCore::imGuiHeap.Get());


	for (auto& outRes : m_outputRes)
	{
		string resName = outRes.first;
		gfxContext.TransitionResource(outRes.second, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
			m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
			outRes.second.uRtvDescriptorOffset,
			m_rtvDescriptorSize);
		D3D12_CPU_DESCRIPTOR_HANDLE RTVs[] =
		{
			rtvHandle
		};
		gfxContext.SetPipelineState(m_psoContainer[outRes.first]);
		//gfxContext.ClearDepth(m_preDepthPass);
		//gfxContext.SetDepthStencilTarget(m_preDepthPass.GetDSV());
		gfxContext.SetRenderTargets(_countof(RTVs), RTVs, rtvHandle);
		// TODO clear color

		gfxContext.SetViewportAndScissor(m_viewport, m_scissorRect);
		CD3DX12_GPU_DESCRIPTOR_HANDLE srv(
			IGuiCore::imGuiHeap->GetGPUDescriptorHandleForHeapStart(),
			outRes.second.uSrvDescriptorOffset,
			32);
		auto descriptorPtrColorBuffer = dynamic_cast<ColorBuffer*>(m_inputRes[resName]);
		auto descriptorPtrDepthBuffer = dynamic_cast<DepthBuffer*>(m_inputRes[resName]);
		if (descriptorPtrColorBuffer != nullptr) 
			gfxContext.SetDynamicDescriptor(outRes.second.uSrvDescriptorOffset, 0, descriptorPtrColorBuffer->GetSRV());
		if (descriptorPtrDepthBuffer != nullptr)
			gfxContext.SetDynamicDescriptor(outRes.second.uSrvDescriptorOffset, 0, descriptorPtrDepthBuffer->GetDepthSRV());

		gfxContext.DrawInstanced(4, 1);

		gfxContext.TransitionResource(outRes.second, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	}
}

void DX12TextureConverter::AddInputRes(
	string name, uint32_t width, uint32_t height, uint32_t elementSize,
	DXGI_FORMAT format, GpuResource* input)
{
	m_inputRes[name] = input;
	//m_outputRes[name] = new DX12Resource>();
	m_outputRes[name].mFormat = format;

	// The texture for ImGUI is always set to R32G32B32A32 format for easier visualization
	CreateTex2DResources(name, width, height, elementSize, DXGI_FORMAT_R32G32B32A32_FLOAT, &m_outputRes[name]);
}

HRESULT DX12TextureConverter::Finalize()
{
	HRESULT hr = S_OK;
	// Create a root signature consisting of a descriptor table with a single CBV
	{
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
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

		m_sceneRootSignature.Reset(m_inputRes.size() + 1, 1);
		m_sceneRootSignature.InitStaticSampler(0, non_static_sampler);
		m_sceneRootSignature[0].InitAsConstantBuffer(0);
		m_sceneRootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, m_inputRes.size());
		m_sceneRootSignature.Finalize(L"TextureConverterRS", rootSignatureFlags);
	}

	// Create PSOs
	{
#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> geometryShader;
		ComPtr<ID3DBlob> errorMessages;

		// Common Vertex shader for background square
		hr = D3DCompileFromFile(L"BGSquareShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_1", compileFlags, 0, &vertexShader, &errorMessages);
		if (FAILED(hr) && errorMessages)
		{
			const char* errorMsg = (const char*)errorMessages->GetBufferPointer();
			//MessageBox(nullptr, errorMsg, L"Shader Compilation Error", MB_RETRYCANCEL);
			wstring str;
			for (int i = 0; i < 250; i++) str += errorMsg[i];
			MessageBox(nullptr, str.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
			exit(0);
		}
		errorMessages.Reset();
		errorMessages = nullptr;

		// Common Geometry shader for background square
		hr = D3DCompileFromFile(L"BGSquareShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "GSMain", "gs_5_1", compileFlags, 0, &geometryShader, &errorMessages);
		if (FAILED(hr) && errorMessages)
		{
			const char* errorMsg = (const char*)errorMessages->GetBufferPointer();
			//MessageBox(nullptr, errorMsg, L"Shader Compilation Error", MB_RETRYCANCEL);
			wstring str;
			for (int i = 0; i < 250; i++) str += errorMsg[i];
			MessageBox(nullptr, str.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
			exit(0);
		}
		errorMessages.Reset();
		errorMessages = nullptr;

		// Pixels shaders for 
		for (auto& res : m_outputRes)
		{
			string resName(res.first);
			ComPtr<ID3DBlob> pixelShader;

			D3D_SHADER_MACRO macro[] = { "None", "0", nullptr, nullptr };
			switch (res.second.mFormat)
			{
			case DXGI_FORMAT_R32G32B32A32_FLOAT:
			{
				macro->Name = "FORMAT_R32G32B32A32";
				macro->Definition = to_string(TEXCONVERT_R32G32B32A32_TO_R32G32B32A32).c_str();
				break;
			}
			case DXGI_FORMAT_D32_FLOAT:
			{
				macro->Name = "FORMAT_D32_FLOAT";
				macro->Definition = to_string(TEXCONVERT_D32_TO_R32G32B32A32).c_str();
				break;
			}
			default:
				break;
			}

#if defined(_DEBUG)
			hr = D3DCompileFromFile(L"BGSquareShader.hlsl", macro, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_1", compileFlags, 0, &pixelShader, &errorMessages);
			if (FAILED(hr) && errorMessages)
			{
				const char* errorMsg = (const char*)errorMessages->GetBufferPointer();
				//MessageBox(nullptr, errorMsg, L"Shader Compilation Error", MB_RETRYCANCEL);
				wstring str;
				for (int i = 0; i < 250; i++) str += errorMsg[i];
				MessageBox(nullptr, str.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
				exit(0);
			}
			errorMessages.Reset();
			errorMessages = nullptr;
#else
			ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(ws.c_str()).c_str(), macro, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_1", compileFlags, 0, &pixelShader, nullptr));
#endif

			// Define the vertex input layout.
			D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};

			m_psoContainer[resName].SetInputLayout(_countof(inputElementDescs), inputElementDescs);
			m_psoContainer[resName].SetRootSignature(m_sceneRootSignature);
			m_psoContainer[resName].SetVertexShader(CD3DX12_SHADER_BYTECODE(vertexShader.Get()));
			m_psoContainer[resName].SetGeometryShader(CD3DX12_SHADER_BYTECODE(geometryShader.Get()));
			m_psoContainer[resName].SetPixelShader(CD3DX12_SHADER_BYTECODE(pixelShader.Get()));
			m_psoContainer[resName].SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
			m_psoContainer[resName].SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
			m_psoContainer[resName].SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(TRUE, D3D12_DEPTH_WRITE_MASK_ALL,
				D3D12_COMPARISON_FUNC_LESS, TRUE, 0xFF, 0xFF,
				D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_INCR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS,
				D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_DECR, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS));
			m_psoContainer[resName].SetSampleMask(UINT_MAX);
			m_psoContainer[resName].SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT);
			// TODO need to get input format
			m_psoContainer[resName].SetRenderTargetFormat(m_outputRes[resName].mFormat, DXGI_FORMAT_D32_FLOAT);
			m_psoContainer[resName].Finalize();


			vector<XMFLOAT3> vPoints = 
			{
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(0.0f, 0.0f, 0.0f)
			};
			m_vertexBuffer.Create(L"BunnyVertexBuffer", vPoints.size(), sizeof(XMFLOAT3), vPoints.data());
		}
	}
	return hr;
}

// Private Functions
void DX12TextureConverter::CreateTex2DResources(
	string name, uint32_t width, uint32_t height,
	uint32_t elementSize, DXGI_FORMAT format, DX12Resource* pResource)
{
	D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&pResource->pResource)));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
		IGuiCore::imGuiHeap->GetCPUDescriptorHandleForHeapStart(),
		IGuiCore::g_heapPtr,
		32);
	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(pResource->pResource.Get(), &srvDesc, srvHandle);
	pResource->uSrvDescriptorOffset = IGuiCore::g_heapPtr;
	++IGuiCore::g_heapPtr;

	std::wstring ws(name.size(), L' '); // Overestimate number of code points.
	ws.resize(std::mbstowcs(&ws[0], name.c_str(), name.size())); // Shrink to fit.
	pResource->pResource->SetName(ws.c_str());

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = format;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	// Create RTV resource
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
		m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
		m_rtvHeapPtr,
		m_rtvDescriptorSize);
	IGraphics::g_GraphicsCore->g_pD3D12Device->CreateRenderTargetView(pResource->pResource.Get(), &rtvDesc, rtvHandle);
	pResource->uRtvDescriptorOffset = m_rtvHeapPtr;
	++m_rtvHeapPtr;
}
