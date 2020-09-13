#pragma once

#include "DXUT.h"
#include "DXUTmisc.h"
#include "DXUTCamera.h"
#include "DXUTSettingsDlg.h"
#include "Win32FrameWork.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#include "GraphicsCore.h"
#include "Camera.h"
#include "Model.h"
#include "GpuBuffer.h"
#include "DX12RootSignature.h"

// Experimental classes 
#include "SimpleComputeShader.h"

#define DX12_ENABLE_DEBUG_LAYER

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif


using namespace DirectX;
using Microsoft::WRL::ComPtr;

class RawExample : public Win32FrameWork
{
public:
    RawExample(UINT width, UINT height, std::wstring name);

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();

    virtual void WinMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    void ShowImGUI();

private:
    static const UINT FrameCount = 3;

    static const UINT TextureWidth = 256;
    static const UINT TextureHeight = 256;
    static const UINT TexturePixelSize = 4;    // The number of bytes used to represent a pixel in the texture.

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT3 color;
        XMFLOAT2 uv;
    };

    __declspec(align(16)) struct CBuffer
    {
        XMMATRIX worldMatrix;
        XMMATRIX worldViewProjMatrix;
        //XMFLOAT4 offset;
    };

    // Pipeline objects.
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeapTex2D;
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;

    // App resources.
    StructuredBuffer m_vertexBuffer;
    StructuredBuffer m_indexBuffer;
    ComPtr<ID3D12Resource> m_texture;

    ComPtr<ID3D12Resource> m_depthBuffer;
    ComPtr<ID3D12DescriptorHeap> m_DSVHeap;

    ComPtr<ID3D12Resource> m_constantBuffer;
    CBuffer m_constantBufferData;
    UINT8* m_pCbvDataBegin;

    // DXUT Model-View Camera
    CModelViewerCamera m_modelViewCamera;
    IMath::Camera m_perspectiveCamera;

    // Model
    shared_ptr<Model> m_pModel;

    // Compute Shader
    SimpleComputeShader m_simpleCS;

    void LoadPipeline();
    void LoadAssets();
    void LoadImGUI();
    void PopulateCommandList();
    std::vector<UINT8> GenerateTextureData(); // For test purpose


//	void TestCode()
//	{
//		ComPtr<ID3D12Resource> textureUploadHeap;
//		uint32_t TextureWidth = 80;
//		uint32_t TextureHeight = 45;
//		uint32_t TexturePixelSize = 4;
//
//		// Describe and create a Texture2D.
//		D3D12_RESOURCE_DESC textureDesc = {};
//		textureDesc.MipLevels = 1;
//		//textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
//		textureDesc.Format = DXGI_FORMAT_R32G32_UINT;
//		textureDesc.Width = TextureWidth;
//		textureDesc.Height = TextureHeight;
//		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
//		textureDesc.DepthOrArraySize = 1;
//		textureDesc.SampleDesc.Count = 1;
//		textureDesc.SampleDesc.Quality = 0;
//		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
//
//		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
//			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
//			D3D12_HEAP_FLAG_NONE,
//			&textureDesc,
//			D3D12_RESOURCE_STATE_COPY_DEST,
//			nullptr,
//			IID_PPV_ARGS(&g_srvDict[TestSRV].pResource)));
//
//		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(g_srvDict[TestSRV].pResource.Get(), 0, 1);
//
//		// Create the GPU upload buffer.
//		ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
//			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
//			D3D12_HEAP_FLAG_NONE,
//			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
//			D3D12_RESOURCE_STATE_GENERIC_READ,
//			nullptr,
//			IID_PPV_ARGS(&textureUploadHeap)));
//
//		// Copy data to the intermediate upload heap and then schedule a copy 
//		// from the upload heap to the Texture2D.
//		//std::vector<UINT8> texture = GenerateTextureData();
//		std::vector<float> texture = GenerateTextureData();
//
//		D3D12_SUBRESOURCE_DATA textureData = {};
//		textureData.pData = &texture[0];
//		textureData.RowPitch = TextureWidth * TexturePixelSize;
//		textureData.SlicePitch = textureData.RowPitch * TextureHeight;
//
//		// Create a temporary command queue to do the copy with
//		ID3D12Fence* fence = NULL;
//		HRESULT hr = IGraphics::g_GraphicsCore->g_pD3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
//		IM_ASSERT(SUCCEEDED(hr));
//
//		HANDLE event = CreateEvent(0, 0, 0, 0);
//		IM_ASSERT(event != NULL);
//
//		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
//		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
//		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
//		queueDesc.NodeMask = 1;
//
//		ID3D12CommandQueue* cmdQueue = NULL;
//		hr = IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue));
//		IM_ASSERT(SUCCEEDED(hr));
//
//		ID3D12CommandAllocator* cmdAlloc = NULL;
//		hr = IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
//		IM_ASSERT(SUCCEEDED(hr));
//
//		ID3D12GraphicsCommandList* cmdList = NULL;
//		hr = IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, NULL, IID_PPV_ARGS(&cmdList));
//		IM_ASSERT(SUCCEEDED(hr));
//
//		//cmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, NULL);
//		//cmdList->ResourceBarrier(1, &barrier);
//
//		UpdateSubresources(cmdList, g_srvDict[TestSRV].pResource.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
//		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_srvDict[TestSRV].pResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
//
//
//		hr = cmdList->Close();
//		IM_ASSERT(SUCCEEDED(hr));
//
//		// Execute the copy
//		cmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&cmdList);
//		hr = cmdQueue->Signal(fence, 1);
//		IM_ASSERT(SUCCEEDED(hr));
//
//		// Wait for everything to complete
//		fence->SetEventOnCompletion(1, event);
//		WaitForSingleObject(event, INFINITE);
//
//		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
//			imGuiHeap->GetCPUDescriptorHandleForHeapStart(),
//			TestSRV,
//			32);
//
//		// Describe and create a SRV for the texture.
//		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
//		//srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
//		srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 0, 0, 0);
//		srvDesc.Format = textureDesc.Format;
//		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
//		srvDesc.Texture2D.MipLevels = 1;
//		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(g_srvDict[TestSRV].pResource.Get(), &srvDesc, srvHandle);
//
//		// Tear down our temporary command queue and release the upload resource
//		cmdList->Release();
//		cmdAlloc->Release();
//		cmdQueue->Release();
//		CloseHandle(event);
//		fence->Release();
//	}
//
//	void TestCopy()
//	{
//		uint32_t TextureWidth = 80;
//		uint32_t TextureHeight = 45;
//		uint32_t TexturePixelSize = 4;
//
//		// Describe and create a Texture2D.
//		D3D12_RESOURCE_DESC textureDesc = {};
//		textureDesc.MipLevels = 1;
//		//textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
//		textureDesc.Format = DXGI_FORMAT_R32G32_UINT;
//		textureDesc.Width = TextureWidth;
//		textureDesc.Height = TextureHeight;
//		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
//		textureDesc.DepthOrArraySize = 1;
//		textureDesc.SampleDesc.Count = 1;
//		textureDesc.SampleDesc.Quality = 0;
//		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
//
//		//ThrowIfFailed(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
//		//	&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
//		//	D3D12_HEAP_FLAG_NONE,
//		//	&textureDesc,
//		//	D3D12_RESOURCE_STATE_COPY_DEST,
//		//	nullptr,
//		//	IID_PPV_ARGS(&g_srvDict[TestSRV1].pResource)));
//
//
//		CreateGuiTexture2DSRV(L"TestSao", TextureWidth, TextureHeight,
//			sizeof(XMFLOAT4), textureDesc.Format, TestSRV1);
//
//		// Create a temporary command queue to do the copy with
//		ID3D12Fence* fence = NULL;
//		HRESULT hr = IGraphics::g_GraphicsCore->g_pD3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
//		IM_ASSERT(SUCCEEDED(hr));
//
//		HANDLE event = CreateEvent(0, 0, 0, 0);
//		IM_ASSERT(event != NULL);
//
//		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
//		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
//		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
//		queueDesc.NodeMask = 1;
//
//		ID3D12CommandQueue* cmdQueue = NULL;
//		hr = IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue));
//		IM_ASSERT(SUCCEEDED(hr));
//
//		ID3D12CommandAllocator* cmdAlloc = NULL;
//		hr = IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
//		IM_ASSERT(SUCCEEDED(hr));
//
//		ID3D12GraphicsCommandList* cmdList = NULL;
//		hr = IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, NULL, IID_PPV_ARGS(&cmdList));
//		IM_ASSERT(SUCCEEDED(hr));
//
//
//
//		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_srvDict[TestSRV].pResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE));
//		D3D12_TEXTURE_COPY_LOCATION DestLocation =
//		{
//			g_srvDict[TestSRV1].pResource.Get(),
//			D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
//			0
//		};
//
//		D3D12_TEXTURE_COPY_LOCATION SrcLocation =
//		{
//			g_srvDict[TestSRV].pResource.Get(),
//			D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
//			0
//		};
//
//		cmdList->CopyTextureRegion(&DestLocation, 0, 0, 0, &SrcLocation, nullptr);
//
//
//		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_srvDict[TestSRV].pResource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
//		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_srvDict[TestSRV1].pResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
//
//
//
//		hr = cmdList->Close();
//		IM_ASSERT(SUCCEEDED(hr));
//
//		// Execute the copy
//		cmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&cmdList);
//		hr = cmdQueue->Signal(fence, 1);
//		IM_ASSERT(SUCCEEDED(hr));
//
//		// Wait for everything to complete
//		fence->SetEventOnCompletion(1, event);
//		WaitForSingleObject(event, INFINITE);
//
//		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
//			imGuiHeap->GetCPUDescriptorHandleForHeapStart(),
//			TestSRV1,
//			32);
//
//		// Describe and create a SRV for the texture.
//		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
//		//srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
//		srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 0, 0, 0);
//		srvDesc.Format = textureDesc.Format;
//		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
//		srvDesc.Texture2D.MipLevels = 1;
//		IGraphics::g_GraphicsCore->g_pD3D12Device->CreateShaderResourceView(g_srvDict[TestSRV1].pResource.Get(), &srvDesc, srvHandle);
//
//		// Tear down our temporary command queue and release the upload resource
//		cmdList->Release();
//		cmdAlloc->Release();
//		cmdQueue->Release();
//		CloseHandle(event);
//		fence->Release();
//	}
//};


////std::vector<UINT8> GenerateTextureData()
//std::vector<float> GenerateTextureData()
//{
//    uint32_t TextureWidth = 80;
//    uint32_t TextureHeight = 45;
//    uint32_t TexturePixelSize = 2;
//
//    const UINT rowPitch = TextureWidth * TexturePixelSize;
//    const UINT cellPitch = rowPitch >> 3;        // The width of a cell in the checkboard texture.
//    const UINT cellHeight = TextureWidth >> 3;    // The height of a cell in the checkerboard texture.
//    const UINT textureSize = rowPitch * TextureHeight;
//
//    //std::vector<UINT8> data(textureSize);
//    std::vector<float> data(textureSize);
//    //UINT8* pData = &data[0];
//    float* pData = &data[0];
//
//    for (UINT n = 0; n < textureSize; n += TexturePixelSize)
//    {
//        UINT x = n % rowPitch;
//        UINT y = n / rowPitch;
//        UINT i = x / cellPitch;
//        UINT j = y / cellHeight;
//
//        if (i % 2 == j % 2)
//        {
//            pData[n] = 0x00;        // R
//            pData[n + 1] = 0x00;    // G
//            pData[n + 2] = 0x00;    // B
//            pData[n + 3] = 0xff;    // A
//        }
//        else
//        {
//            pData[n] = 0xff;        // R
//            pData[n + 1] = 0xff;    // G
//            pData[n + 2] = 0xff;    // B
//            pData[n + 3] = 0xff;    // A
//        }
//    }
//
//    return data;
//}
