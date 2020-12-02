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
/*    Deferred Shading     */
/***************************/
DeferredRendering::DeferredRendering()
{

}

DeferredRendering::~DeferredRendering()
{

}

void DeferredRendering::Render(GraphicsContext& gfxContext)
{
	for (auto& p : m_renderPassList)
	{
		auto pass = dynamic_pointer_cast<DX12ShadingPass>(p);
		pass->PreRender(gfxContext);
		pass->Render(gfxContext);
		pass->PostRender(gfxContext);
	}
}

void DeferredRendering::Resize(uint64_t width, uint64_t height)
{
	ASSERT(width != 0 and height != 0);
	if (!width or !height) return;

	for (auto& rp : m_renderPassList)
	{
		auto dx12ShadingPass = dynamic_pointer_cast<DX12ShadingPass>(rp);
		dx12ShadingPass->Resize(width, height);
	}
}

void DeferredRendering::Destroy()
{
	m_name = "";
}

/*******************************/
/*    Tiled Forward Shading    */
/******************************/
TiledForwardRendering::TiledForwardRendering()
{

}

TiledForwardRendering::~TiledForwardRendering()
{

}

void TiledForwardRendering::Render(GraphicsContext& gfxContext)
{
	for (auto& p : m_renderPassList)
	{
		auto pass = dynamic_pointer_cast<DX12ShadingPass>(p);
		pass->PreRender(gfxContext);
		pass->Render(gfxContext);
		pass->PostRender(gfxContext);
	}
}

void TiledForwardRendering::Resize(uint64_t width, uint64_t height)
{
	ASSERT(width != 0 and height != 0);
	if (!width or !height) return;

	for (auto& rp : m_renderPassList)
	{
		auto dx12ShadingPass = dynamic_pointer_cast<DX12ShadingPass>(rp);
		dx12ShadingPass->Resize(width, height);
	}
}

void TiledForwardRendering::Destroy()
{
	m_name = "";
}


/***************************/
/*    General Shading       */
/***************************/
GeneralRendering::GeneralRendering()
{

}

GeneralRendering::~GeneralRendering()
{

}

void GeneralRendering::UpdatePerFrameContBuffer(
	UINT passID,
	XMMATRIX worldMatrix,
	XMMATRIX viewMatrix,
	XMMATRIX worldViewProjMatrix)
{
	// TODO
}

void GeneralRendering::Resize(uint64_t width, uint64_t height)
{
	ASSERT(width != 0 and height != 0);
	if (!width or !height) return;

	for (auto& rp : m_renderPassList)
	{
		auto dx12ShadingPass = dynamic_pointer_cast<DX12ShadingPass>(rp);
		dx12ShadingPass->Resize(width, height);
	}
}

void GeneralRendering::UpdateLightBuffer(UINT passID, shared_ptr<StructuredBuffer> pLightBuffer)
{
	auto generalPass = m_renderPassList[passID];
	// TODO
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
