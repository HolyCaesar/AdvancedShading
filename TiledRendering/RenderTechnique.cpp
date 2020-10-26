#include "RenderTechnique.h"
#include "RenderPass.h"
#include "DX12AuxiliaryLib.h"
#include "GpuBuffer.h"
#include "GpuResource.h"

using namespace std;

RenderTechnique::RenderTechnique()
{

}

RenderTechnique::~RenderTechnique()
{

}

uint64_t RenderTechnique::AddPass(shared_ptr<RenderPass> pass)
{
	uint64_t idx = static_cast<uint64_t>(m_renderPassList.size());
	m_renderPassList.push_back(pass);
	return idx;
}

shared_ptr<RenderPass> RenderTechnique::GetPass(uint64_t passIdx)
{
	return m_renderPassList[passIdx];
}

/***************************/
/*    GeneralRendering     */
/***************************/
GeneralRendering::GeneralRendering()
{

}

GeneralRendering::~GeneralRendering()
{

}

void GeneralRendering::Init(
	string name, 
	uint64_t width, 
	uint64_t height,
	shared_ptr<StructuredBuffer> pLightBuffer)
{
	m_name = name;

	// Initialize a default render pass for general rendering technique
	shared_ptr<DX12ShadingPass> generalPass(new DX12ShadingPass(), 
		[](DX12ShadingPass* ptr) { ptr->Destroy(); });

	// RootSignature
	shared_ptr<DX12RootSignature> generalShadingRS;

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
	{
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


//#if defined(_DEBUG)
//		ComPtr<ID3DBlob> errorMessages;
//		HRESULT hr = D3DCompileFromFile(shaderName.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_1", compileFlags, 0, &vertexShader, &errorMessages);
//		if (FAILED(hr) && errorMessages)
//		{
//			const char* errorMsg = (const char*)errorMessages->GetBufferPointer();
//			//MessageBox(nullptr, errorMsg, L"Shader Compilation Error", MB_RETRYCANCEL);
//			wstring str;
//			for (int i = 0; i < 150; i++) str += errorMsg[i];
//			MessageBox(nullptr, str.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
//			exit(0);
//		}
//		errorMessages.Reset();
//		errorMessages = nullptr;
//
//		hr = D3DCompileFromFile(shaderName.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_1", compileFlags, 0, &pixelShader, &errorMessages);
//		if (FAILED(hr) && errorMessages)
//		{
//			const char* errorMsg = (const char*)errorMessages->GetBufferPointer();
//			//MessageBox(nullptr, errorMsg, L"Shader Compilation Error", MB_RETRYCANCEL);
//			wstring str;
//			for (int i = 0; i < 150; i++) str += errorMsg[i];
//			MessageBox(nullptr, str.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
//			exit(0);
//		}
//		errorMessages.Reset();
//		errorMessages = nullptr;
//#else
//		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(shaderName.c_str()).c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_1", compileFlags, 0, &vertexShader, nullptr));
//		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(shaderName.c_str()).c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_1", compileFlags, 0, &pixelShader, nullptr));
//#endif

		// Add shaders to the renderpass.	
		generalPass->AddShader("GeneralShading_VS", ShaderType::VertexShader, vertexShader);
		generalPass->AddShader("GeneralShading_PS", ShaderType::PixelShader, pixelShader);

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		shared_ptr<GraphicsPSO> generalShadingPSO;
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
		generalShadingPSO->Finalize();

		generalPass->FinalizeRootSignature(generalShadingRS);
		generalPass->FinalizePSO(generalShadingPSO);
	}
	
	// Initialize constant buffer
	m_perFrameCBuffer.viewMatrix = XMMatrixIdentity();
	m_perFrameCBuffer.worldMatrix = XMMatrixIdentity();
	m_perFrameCBuffer.worldViewProjMatrix = XMMatrixIdentity();
		
	// Add Buffers
	generalPass->AddConstantBuffer(0, L"GeneralConstBuffer", sizeof(m_perFrameCBuffer), &m_perFrameCBuffer);
	generalPass->AddStructuredBufferSRV(1, L"GeneralLightBuffer", pLightBuffer);
}

void GeneralRendering::UpdatePerFrameContBuffer(
	UINT passID,
	XMMATRIX worldMatrix,
	XMMATRIX viewMatrix,
	XMMATRIX worldViewProjMatrix)
{
	m_perFrameCBuffer.worldMatrix = worldMatrix;
	m_perFrameCBuffer.viewMatrix = viewMatrix;
	m_perFrameCBuffer.worldViewProjMatrix = worldViewProjMatrix;
}

void GeneralRendering::UpdateLightBuffer(UINT passID, shared_ptr<StructuredBuffer> pLightBuffer)
{
	auto generalPass = m_renderPassList[passID];
}

void GeneralRendering::Render(GraphicsContext& gfxContext)
{
	for (auto& p : m_renderPassList)
	{
		auto pass = dynamic_pointer_cast<DX12ShadingPass>(p);
		pass->PreRender(gfxContext);
		pass->Render(gfxContext);
		pass->PostRender(gfxContext);
	}
}

void GeneralRendering::Destroy()
{
	m_name = "";
}
