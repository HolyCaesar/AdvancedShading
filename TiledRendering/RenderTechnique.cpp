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

void GeneralRendering::UpdatePerFrameContBuffer(
	UINT passID,
	XMMATRIX worldMatrix,
	XMMATRIX viewMatrix,
	XMMATRIX worldViewProjMatrix)
{
	// TODO
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
