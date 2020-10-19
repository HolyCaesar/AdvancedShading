#include "RenderPass.h"
#include "CommandContext.h"
#include "GpuBuffer.h"
#include "GpuResource.h"


using namespace std;

/*****************************/
/*  RenderPass definitions   */
/*****************************/
RenderPass::RenderPass() :
	m_bEnabled(false)
{
}

RenderPass::~RenderPass()
{
}

void RenderPass::SetEnabled(bool enabled)
{
	m_bEnabled = enabled;
}

bool RenderPass::IsEnabled() const
{
	return m_bEnabled;
}

void RenderPass::SetName(string name)
{
	m_renderPassName = name;
}

string RenderPass::GetName() const
{
	return m_renderPassName;
}

/*****************************/
/*DX12ShadingPass definitions*/
/*****************************/
DX12ShadingPass::DX12ShadingPass()
{
	m_renderPassName = "";
	m_samplerIdx = 0;
}

DX12ShadingPass::~DX12ShadingPass()
{
	m_renderPassName = "";
	m_samplerIdx = 0;
}

HRESULT DX12ShadingPass::LoadShaderFromFile(
	ShaderType type,
	std::string shaderName,
	std::string shaderEntryPoint,
	std::wstring shaderPath,
	const ShaderMacros& shaderMacros,
	std::string shaderHlslVersion)
{
	HRESULT hr = S_OK;

#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	// Load shader macros
	vector<D3D_SHADER_MACRO> vShaderMacro;
	for (auto& iter : shaderMacros)
	{
		string name = iter.first, definition = iter.second;
		char* c_name = new char[name.size() + 1];
		char* c_definition = new char[definition.size() + 1];
		strncpy_s(c_name, name.size() + 1, name.c_str(), name.size());
		strncpy_s(c_definition, definition.size() + 1, definition.c_str(), definition.size());
		vShaderMacro.push_back({ c_name, c_definition });
	}
	vShaderMacro.push_back({ 0, 0 });

	// Load shader
	ComPtr<ID3DBlob> shader;
	ComPtr<ID3DBlob> errorMessages;
#if defined(_DEBUG)
	hr = D3DCompileFromFile(shaderPath.c_str(), vShaderMacro.size() ? vShaderMacro.data() : nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, shaderEntryPoint.c_str(), shaderHlslVersion.c_str(), compileFlags, 0, &shader, &errorMessages);
	if (FAILED(hr) && errorMessages)
	{
		const char* errorMsg = (const char*)errorMessages->GetBufferPointer();
		wstring str;
		for (int i = 0; i < 500; i++) str += errorMsg[i];
		MessageBox(nullptr, str.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
		exit(0);
	}
#else
	ThrowIfFailed(D3DCompileFromFile(shaderPath.c_str(), vShaderMacro.size() ? vShaderMacro.data() : nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, shaderEntryPoint.c_str(), shaderHlslVersion.c_str(), compileFlags, 0, &shader, &errorMessages));
#endif

	return hr;
}

HRESULT DX12ShadingPass::LoadShaderFromString(
	ShaderType type,
	std::string shaderName,
	std::string shaderEntryPoint,
	const std::string& source,
	const ShaderMacros& shaderMacros,
	std::string shaderHlslVersion)
{
	HRESULT hr = S_OK;
	// TODO
	return hr;
}

//void DX12ShadingPass::UpdateRootSignature()
//{
//	m_rootSignature.Reset(e_numRootParameters, 1);
//	m_rootSignature.InitStaticSampler(0, non_static_sampler);
//	m_rootSignature[e_rootParameterCB].InitAsConstantBuffer(0);
//	m_rootSignature[e_ModelTexRootParameterSRV].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
//	m_rootSignature[e_LightGridRootParameterSRV].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, D3D12_SHADER_VISIBILITY_PIXEL);
//	m_rootSignature[e_LightIndexRootParameterSRV].InitAsBufferSRV(2);
//	m_rootSignature[e_LightBufferRootParameterSRV].InitAsBufferSRV(3);
//	m_rootSignature.Finalize(L"SceneRootSignature", rootSignatureFlags);
//}

void DX12ShadingPass::AddSamplerDesc(
	const std::string name,
	SamplerDesc samplerDesc)
{
	m_samplers[m_samplerIdx] = make_pair(name, samplerDesc);
	++m_samplerIdx;
}

void DX12ShadingPass::FinalizeRootSignature(std::shared_ptr<DX12RootSignature> pRS)
{
	if (pRS)
	{
		m_rootSignature = pRS;
	}
	else
	{
		// Number of root parameters (none sampler)
		int totalRootParams(0), totalSamplers(0);
		totalRootParams = m_colorBufferSRVMap.size() + m_colorBufferUAVMap.size() +
			m_depthBufferSRVMap.size() + m_constantBufferMap.size() +
			m_structuredBufferSRVMap.size() + m_structuredBufferUAVMap.size();
		totalSamplers = m_samplers.size();

		// Generate
		m_rootSignature->Reset(totalRootParams, totalSamplers);

		for (auto& s : m_samplers)
		{
			m_rootSignature->InitStaticSampler(s.first, s.second.second);
		}

		UINT registerSlot = 0;
		for (auto& constBuf : m_constantBufferMap)
		{
			(*m_rootSignature)[constBuf.first].InitAsConstantBuffer(registerSlot);
			++registerSlot;
		}

		registerSlot = 0;
		for (auto& colorBuf : m_colorBufferSRVMap)
		{
			// TODO: the shader visibility is hard code here, may need to allow customization later
			(*m_rootSignature)[colorBuf.first].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, registerSlot, 1, D3D12_SHADER_VISIBILITY_ALL);
			++registerSlot;
		}

		for (auto& depthBuf : m_depthBufferSRVMap)
		{
			// TODO: the shader visibility is hard code here, may need to allow customization later
			(*m_rootSignature)[depthBuf.first].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, registerSlot, 1, D3D12_SHADER_VISIBILITY_ALL);
			++registerSlot;
		}

		for (auto& structBuf : m_structuredBufferSRVMap)
		{
			// TODO: the shader visibility is hard code here, may need to allow customization later
			(*m_rootSignature)[structBuf.first].InitAsBufferSRV(registerSlot);
			++registerSlot;
		}

		registerSlot = 0;
		for (auto& colorBuf : m_colorBufferUAVMap)
		{
			// TODO: the shader visibility is hard code here, may need to allow customization later
			(*m_rootSignature)[colorBuf.first].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, registerSlot, 1, D3D12_SHADER_VISIBILITY_ALL);
			++registerSlot;
		}

		for (auto& structBuf : m_structuredBufferUAVMap)
		{
			// TODO: the shader visibility is hard code here, may need to allow customization later
			(*m_rootSignature)[structBuf.first].InitAsBufferUAV(registerSlot);
			++registerSlot;
		}

		// Finalize root signature
		string rsName = m_renderPassName + "-RootSignature";
		wstring w_rsName(rsName.begin(), rsName.end());
		m_rootSignature->Finalize(w_rsName);
	}
}

void DX12ShadingPass::FinalizePSO(std::shared_ptr<DX12PSO> pPSO)
{
	if (pPSO)
	{
		m_piplineState = pPSO;
	}
	else
	{
		// TODO: may need to add a default PSO or this PSO can be 
		// provided 
	}

	string rsName = m_renderPassName + "-RootSignature";
	wstring w_rsName(rsName.begin(), rsName.end());
	m_piplineState->GetPSO()->SetName(w_rsName.c_str());
}

void DX12ShadingPass::Bind(CommandContext& Context)
{

}

void DX12ShadingPass::Destroy()
{

}
