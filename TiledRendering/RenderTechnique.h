#pragma once
#include "Object.h"
#include <string>
#include <DirectXMath.h>
#include "CommandContext.h"

class RenderPass;
class StructuredBuffer;

// TODO: The current design allow me to customize my own render technique with a fixed pattern
// I can also customize the passes and pass them into render technique
class RenderTechnique : public Object
{
public:
	RenderTechnique();
	virtual ~RenderTechnique();

	virtual void Render(GraphicsContext& Context) = 0;

	virtual void Destroy() = 0;

	virtual uint64_t AddPass(std::shared_ptr<RenderPass> pass);
	std::shared_ptr<RenderPass> GetPass(uint64_t passIdx);

	virtual void SetName(std::string name)
	{
		m_name = name;
	}
	virtual std::string GetName()
	{
		return m_name;
	}

protected:
	std::string m_name;

	std::vector<std::shared_ptr<RenderPass>> m_renderPassList;

private:
	RenderTechnique(const RenderTechnique& copy) = delete;
	RenderTechnique(RenderTechnique&& copy) = delete;
	RenderTechnique& operator=(const RenderTechnique& other) = delete;
};

class DeferredRendering : public RenderTechnique
{
public:
	DeferredRendering();
	virtual ~DeferredRendering();

	void Render(GraphicsContext& gfxContext);

	void Resize(uint64_t width, uint64_t height);

	void Destroy();

private:
	DeferredRendering(const DeferredRendering& copy) = delete;
	DeferredRendering(DeferredRendering&& copy) = delete;
	DeferredRendering& operator=(const DeferredRendering& other) = delete;
};

class GeneralRendering : public RenderTechnique
{
public:
	GeneralRendering();
	virtual ~GeneralRendering();

	void Render(GraphicsContext& Context);
	
	void Resize(uint64_t width, uint64_t height);

	void Destroy();

	void UpdatePerFrameContBuffer(
		UINT passID,
		XMMATRIX worldMatrix,
		XMMATRIX viewMatrix,
		XMMATRIX worldViewProjMatrix);

	void UpdateLightBuffer(UINT passID, std::shared_ptr<StructuredBuffer> pLightBuffer);

private:
	GeneralRendering(const GeneralRendering& copy) = delete;
	GeneralRendering(GeneralRendering&& copy) = delete;
	GeneralRendering& operator=(const GeneralRendering& other) = delete;
};
