#include "RenderTechnique.h"
#include "RenderPass.h"
#include "CommandContext.h" 

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

