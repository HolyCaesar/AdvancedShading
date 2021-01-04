#include "RenderPass.h"
#include "CommandContext.h"
#include "GpuBuffer.h"
#include "GpuResource.h"


using namespace std;

/*****************************/
/*   RenderPass definitions   */
/*****************************/
RenderPass::RenderPass() :
	m_bEnabled(false),
	m_bEnableGPUQuery(false),
	m_cpuProfiler(nullptr),
	m_gpuProfiler(nullptr)
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

void RenderPass::AddShader(
	const std::string name,
	ShaderType type,
	ComPtr<ID3DBlob> pShader)
{
	m_shaders[type] = pShader;
}

/*******************************/
/*DX12ShadingPass definitions*/
/*******************************/
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
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		//D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

		// Number of root parameters (none sampler)
		int totalRootParams(0), totalSamplers(0);
		totalRootParams = 
			m_colorBufferSRVMap.resPool.size() + m_colorBufferUAVMap.resPool.size() +
			m_depthBufferSRVMap.resPool.size() + m_constantBufferMap.resPool.size() +
			m_structuredBufferSRVMap.resPool.size() + m_structuredBufferUAVMap.resPool.size();
		totalSamplers = m_samplers.size();

		// Generate
		m_rootSignature->Reset(totalRootParams, totalSamplers);

		for (auto& s : m_samplers)
		{
			m_rootSignature->InitStaticSampler(s.first, s.second.second);
		}

		UINT registerSlot = 0;
		for (auto& constBuf : m_constantBufferMap.resPool)
		{
			auto & [rootIdx, bufSize, bufPtr] = constBuf;
			(*m_rootSignature)[rootIdx].InitAsConstantBuffer(registerSlot);
			++registerSlot;
		}

		registerSlot = 0;
		for (auto& colorBuf : m_colorBufferSRVMap.resPool)
		{
			// TODO: the shader visibility is hard code here, may need to allow customization later
			(*m_rootSignature)[colorBuf.first].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, registerSlot, 1, D3D12_SHADER_VISIBILITY_ALL);
			++registerSlot;
		}

		for (auto& depthBuf : m_depthBufferSRVMap.resPool)
		{
			// TODO: the shader visibility is hard code here, may need to allow customization later
			(*m_rootSignature)[depthBuf.first].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, registerSlot, 1, D3D12_SHADER_VISIBILITY_ALL);
			++registerSlot;
		}

		for (auto& structBuf : m_structuredBufferSRVMap.resPool)
		{
			// TODO: the shader visibility is hard code here, may need to allow customization later
			(*m_rootSignature)[structBuf.first].InitAsBufferSRV(registerSlot);
			++registerSlot;
		}

		registerSlot = 0;
		for (auto& colorBuf : m_colorBufferUAVMap.resPool)
		{
			// TODO: the shader visibility is hard code here, may need to allow customization later
			(*m_rootSignature)[colorBuf.first].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, registerSlot, 1, D3D12_SHADER_VISIBILITY_ALL);
			++registerSlot;
		}

		for (auto& structBuf : m_structuredBufferUAVMap.resPool)
		{
			// TODO: the shader visibility is hard code here, may need to allow customization later
			(*m_rootSignature)[structBuf.first].InitAsBufferUAV(registerSlot);
			++registerSlot;
		}

		// Finalize root signature
		string rsName = m_renderPassName + "-RootSignature";
		wstring w_rsName(rsName.begin(), rsName.end());
		m_rootSignature->Finalize(w_rsName, rootSignatureFlags);
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

void DX12ShadingPass::Destroy()
{
	m_colorBufferSRVMap.Clear();
	m_colorBufferUAVMap.Clear();
	m_structuredBufferSRVMap.Clear();
	m_structuredBufferUAVMap.Clear();
	m_depthBufferSRVMap.Clear();
	m_constantBufferMap.Clear();

	m_bEnabled = false;
	m_renderPassName = "";

	m_shaders.clear();
	m_samplers.clear();
}

void DX12ShadingPass::PreRender(GraphicsContext& gfxContext)
{
	if (m_bEnableGPUQuery and m_gpuProfiler)
		m_gpuProfiler->Start(m_renderPassName + " GPU Time", gfxContext);


	// Transist resource to the right state
	for(auto& colorSRV : m_colorBufferSRVMap.resPool)
		gfxContext.TransitionResource(*(colorSRV.second), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	for(auto& colorUAV : m_colorBufferUAVMap.resPool)
		gfxContext.TransitionResource(*(colorUAV.second), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	for(auto& depthSRV : m_depthBufferSRVMap.resPool)
		gfxContext.TransitionResource(*(depthSRV.second), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	for(auto& strutSRV : m_structuredBufferSRVMap.resPool)
		gfxContext.TransitionResource(*(strutSRV.second), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	for(auto& strutUAV : m_structuredBufferUAVMap.resPool)
		gfxContext.TransitionResource(*(strutUAV.second), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	if (m_bEnableOwnRenderTarget)
	{
		for(auto& rtv : m_renderTargets)
			gfxContext.TransitionResource(*(rtv.m_pRTV), rtv.beforeState);
		gfxContext.TransitionResource(*m_depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}
	else // Otherwise just use the backbuffer
	{
		UINT backBufferIndex = IGraphics::g_GraphicsCore->g_CurrentBuffer;
		gfxContext.TransitionResource(IGraphics::g_GraphicsCore->g_DisplayPlane[backBufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET);
		gfxContext.TransitionResource(*m_depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}

	gfxContext.FlushResourceBarriers();
}

void DX12ShadingPass::Render(GraphicsContext& gfxContext)
{
	gfxContext.SetRootSignature((*m_rootSignature));
	gfxContext.SetPipelineState((*m_piplineState));

	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxContext.SetIndexBuffer(m_pIndexBuffer->IndexBufferView());
	gfxContext.SetVertexBuffer(0, m_pVertexBuffer->VertexBufferView());

	// TODO: right now just use the back buffer of the swap chain
	if (m_bEnableOwnRenderTarget)
	{
		// !!! Needs to setup depth stencil target before setting up render targets
		// SetDepthStencilTarget use SetRenderTargets with no rtv binding
		// If we used SetRenderTargets first, then no rtv can be bound to the pipline.
		gfxContext.ClearDepth(*m_depthBuffer);
		gfxContext.SetDepthStencilTarget(m_depthBuffer->GetDSV());

		vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
		for (auto& rtv : m_renderTargets)
			rtvs.push_back(rtv.m_pRTV->GetRTV());
		gfxContext.SetRenderTargets(rtvs.size(), rtvs.data(), m_depthBuffer->GetDSV());

		for(auto& rtv : m_renderTargets)
			gfxContext.ClearColor(*(rtv.m_pRTV));
	}
	else // otherwise just use the backbuffer
	{
		UINT backBufferIndex = IGraphics::g_GraphicsCore->g_CurrentBuffer;
		D3D12_CPU_DESCRIPTOR_HANDLE RTVs[] =
		{
			IGraphics::g_GraphicsCore->g_DisplayPlane[backBufferIndex].GetRTV()
		};

		gfxContext.ClearDepth(*m_depthBuffer);
		gfxContext.SetDepthStencilTarget(m_depthBuffer->GetDSV());
		gfxContext.SetRenderTargets(_countof(RTVs), RTVs, m_depthBuffer->GetDSV());
		gfxContext.ClearColor(IGraphics::g_GraphicsCore->g_DisplayPlane[backBufferIndex]);
	}

	gfxContext.SetViewportAndScissor(m_viewport, m_scissorRect);

	for (auto& constBuf : m_constantBufferMap.resPool)
	{
		auto& [rootIdx, bufSize, bufPtr] = constBuf;
		gfxContext.SetDynamicConstantBufferView(rootIdx, bufSize, bufPtr);
	}

	for (auto& cBuf : m_colorBufferSRVMap.resPool) // color SRV
		gfxContext.SetDynamicDescriptor(cBuf.first, 0, cBuf.second->GetSRV());

	for (auto& dBuf : m_depthBufferSRVMap.resPool) // depth SRV
		gfxContext.SetDynamicDescriptor(dBuf.first, 0, dBuf.second->GetDepthSRV());

	for (auto& sBuf : m_structuredBufferSRVMap.resPool) // structured SRV
		gfxContext.SetBufferSRV(sBuf.first, *sBuf.second);

	for (auto& cBuf : m_colorBufferUAVMap.resPool) // color UAV
		gfxContext.SetDynamicDescriptor(cBuf.first, 0, cBuf.second->GetUAV());

	for (auto& sBuf : m_structuredBufferUAVMap.resPool) // structured UAV 
		gfxContext.SetBufferUAV(sBuf.first, *sBuf.second);

	// Reset the UAV counter for this frame.
	for (auto& cBuf : m_colorBufferUAVMap.resPool) // color UAV
		gfxContext.ClearUAV(*cBuf.second);

	// TODO sizeof(UINT) is hardcode here, need to make is a parameter or should
	// be returned by the structure buffer itself
	for (auto& sBuf : m_structuredBufferUAVMap.resPool) // structured UAV 
		gfxContext.CopyBufferRegion(*sBuf.second, 0, sBuf.second->GetCounterBuffer(), 0, sizeof(UINT));

	gfxContext.DrawIndexed(m_pVertexBuffer->GetElementCount(), 0, 0);
}

void DX12ShadingPass::PostRender(GraphicsContext& gfxContext)
{
	if (m_bEnableOwnRenderTarget)
	{
		for(auto& rtv : m_renderTargets)
			gfxContext.TransitionResource(*(rtv.m_pRTV), rtv.afterState);
	}
	else // Otherwise just use the back buffer
	{
		UINT backBufferIndex = IGraphics::g_GraphicsCore->g_CurrentBuffer;
		gfxContext.TransitionResource(IGraphics::g_GraphicsCore->g_DisplayPlane[backBufferIndex], D3D12_RESOURCE_STATE_PRESENT);
	}

	gfxContext.FlushResourceBarriers();

	if (m_bEnableGPUQuery and m_gpuProfiler)
		m_gpuProfiler->Stop(m_renderPassName + " GPU Time", gfxContext);
}

void DX12ShadingPass::Resize(uint64_t width, uint64_t height)
{
	wstring name(m_renderPassName.begin(), m_renderPassName.end());
	for (auto& rtv : m_renderTargets)
	{
		// TODO, right now the color buffer doesn't store the name of the resource
		// need to add it later
		DXGI_FORMAT format = rtv.m_pRTV->GetFormat();
		rtv.m_pRTV->Destroy();
		rtv.m_pRTV->Create(name + L" RTV", width, height, 1, format);
	}

	DXGI_FORMAT format = m_depthBuffer->GetFormat();
	m_depthBuffer->Destroy();
	m_depthBuffer->Create(name + L"DSV", width, height, format);

	m_viewport.Width = width;
	m_viewport.Height = height;

	m_scissorRect.right = width;
	m_scissorRect.bottom = height;
}

/********************************/
/*DX12ShadingPass definitions*/
/********************************/
DX12ComputePass::DX12ComputePass()
{
	m_renderPassName = "Compute Pass's Name is Not Assigned";
	m_xKernelSize = m_yKernelSize = m_zKernelSize;
}

DX12ComputePass::DX12ComputePass(const std::string name)
{
	m_renderPassName = name;
	m_xKernelSize = m_yKernelSize = m_zKernelSize;
}

DX12ComputePass::~DX12ComputePass()
{

}

void DX12ComputePass::FinalizeRootSignature(std::shared_ptr<DX12RootSignature> pRS)
{
	if (pRS)
	{
		m_rootSignature = pRS;
	}
	else
	{
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		//D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

		// Number of root parameters (none sampler)
		int totalRootParams(0), totalSamplers(0);
		totalRootParams = 
			m_colorBufferSRVMap.resPool.size() + m_colorBufferUAVMap.resPool.size() +
			m_depthBufferSRVMap.resPool.size() + m_constantBufferMap.resPool.size() +
			m_structuredBufferSRVMap.resPool.size() + m_structuredBufferUAVMap.resPool.size();
		//totalSamplers = m_samplers.size();

		// Generate
		m_rootSignature->Reset(totalRootParams, totalSamplers);

		//for (auto& s : m_samplers)
		//{
		//	m_rootSignature->InitStaticSampler(s.first, s.second.second);
		//}

		UINT registerSlot = 0;
		for (auto& constBuf : m_constantBufferMap.resPool)
		{
			auto& [rootIdx, bufSize, bufPtr] = constBuf;
			(*m_rootSignature)[rootIdx].InitAsConstantBuffer(registerSlot);
			++registerSlot;
		}

		registerSlot = 0;
		for (auto& colorBuf : m_colorBufferSRVMap.resPool)
		{
			// TODO: the shader visibility is hard code here, may need to allow customization later
			(*m_rootSignature)[colorBuf.first].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, registerSlot, 1, D3D12_SHADER_VISIBILITY_ALL);
			++registerSlot;
		}

		for (auto& depthBuf : m_depthBufferSRVMap.resPool)
		{
			// TODO: the shader visibility is hard code here, may need to allow customization later
			(*m_rootSignature)[depthBuf.first].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, registerSlot, 1, D3D12_SHADER_VISIBILITY_ALL);
			++registerSlot;
		}

		for (auto& structBuf : m_structuredBufferSRVMap.resPool)
		{
			// TODO: the shader visibility is hard code here, may need to allow customization later
			(*m_rootSignature)[structBuf.first].InitAsBufferSRV(registerSlot);
			++registerSlot;
		}

		registerSlot = 0;
		for (auto& colorBuf : m_colorBufferUAVMap.resPool)
		{
			// TODO: the shader visibility is hard code here, may need to allow customization later
			(*m_rootSignature)[colorBuf.first].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, registerSlot, 1, D3D12_SHADER_VISIBILITY_ALL);
			++registerSlot;
		}

		for (auto& structBuf : m_structuredBufferUAVMap.resPool)
		{
			// TODO: the shader visibility is hard code here, may need to allow customization later
			(*m_rootSignature)[structBuf.first].InitAsBufferUAV(registerSlot);
			++registerSlot;
		}

		// Finalize root signature
		string rsName = m_renderPassName + "-RootSignature";
		wstring w_rsName(rsName.begin(), rsName.end());
		m_rootSignature->Finalize(w_rsName, rootSignatureFlags);
	}
}

void DX12ComputePass::FinalizePSO(std::shared_ptr<DX12PSO> pPSO)
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

void DX12ComputePass::PreCompute(GraphicsContext& gfxContext)
{
	ComputeContext& computeContext = gfxContext.GetComputeContext();

	// Transist resource to the right state
	for (auto& colorSRV : m_colorBufferSRVMap.resPool)
		computeContext.TransitionResource(*(colorSRV.second), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	for (auto& colorUAV : m_colorBufferUAVMap.resPool)
		computeContext.TransitionResource(*(colorUAV.second), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	for (auto& depthSRV : m_depthBufferSRVMap.resPool)
		computeContext.TransitionResource(*(depthSRV.second), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	for (auto& strutSRV : m_structuredBufferSRVMap.resPool)
		computeContext.TransitionResource(*(strutSRV.second), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	for (auto& strutUAV : m_structuredBufferUAVMap.resPool)
		computeContext.TransitionResource(*(strutUAV.second), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	computeContext.FlushResourceBarriers();
}

void DX12ComputePass::Compute(GraphicsContext& gfxContext)
{
	ComputeContext& computeContext = gfxContext.GetComputeContext();

	computeContext.SetRootSignature(*m_rootSignature);
	computeContext.SetPipelineState(*m_piplineState);

	for (auto& constBuf : m_constantBufferMap.resPool)
	{
		auto& [rootIdx, bufSize, bufPtr] = constBuf;
		computeContext.SetDynamicConstantBufferView(rootIdx, bufSize, bufPtr);
	}

	for (auto& cBuf : m_colorBufferSRVMap.resPool) // color SRV
		computeContext.SetDynamicDescriptor(cBuf.first, 0, cBuf.second->GetSRV());

	for(auto& dBuf : m_depthBufferSRVMap.resPool) // depth SRV
		computeContext.SetDynamicDescriptor(dBuf.first, 0, dBuf.second->GetDepthSRV());

	for (auto& sBuf : m_structuredBufferSRVMap.resPool) // structured SRV
		computeContext.SetBufferSRV(sBuf.first, *sBuf.second);

	for(auto& cBuf : m_colorBufferUAVMap.resPool) // color UAV
		computeContext.SetDynamicDescriptor(cBuf.first, 0, cBuf.second->GetUAV());

	for (auto& sBuf : m_structuredBufferUAVMap.resPool) // structured UAV 
		computeContext.SetBufferUAV(sBuf.first, *sBuf.second);

	// Reset the UAV counter for this frame.
	for (auto& cBuf : m_colorBufferUAVMap.resPool) // color UAV
		computeContext.ClearUAV(*cBuf.second);

	// TODO sizeof(UINT) is hardcode here, need to make is a parameter or should
	// be returned by the structure buffer itself
	for (auto& sBuf : m_structuredBufferUAVMap.resPool) // structured UAV 
		computeContext.CopyBufferRegion(*sBuf.second, 0, sBuf.second->GetCounterBuffer(), 0, sizeof(UINT));

	computeContext.Dispatch(m_xKernelSize, m_yKernelSize, m_zKernelSize);
}

void DX12ComputePass::Destroy()
{
	m_colorBufferSRVMap.Clear();
	m_colorBufferUAVMap.Clear();
	m_structuredBufferSRVMap.Clear();
	m_structuredBufferUAVMap.Clear();
	m_depthBufferSRVMap.Clear();
	m_constantBufferMap.Clear();

	m_bEnabled = false;
	m_renderPassName = "";

	m_shaders.clear();

	m_xKernelSize = m_yKernelSize = m_zKernelSize;
}
