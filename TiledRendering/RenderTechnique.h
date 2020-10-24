#pragma once
#include "Object.h"
#include <string>

class CommandContext;
class RenderPass;

class RenderTechnique : public Object
{
public:
	RenderTechnique();
	virtual ~RenderTechnique();

	virtual void Init(std::string name, uint64_t width, uint64_t height) = 0;

	virtual void Render(CommandContext& Context) = 0;

	virtual void Destroy() = 0;

	virtual uint64_t AddPass(std::shared_ptr<RenderPass> pass);
	std::shared_ptr<RenderPass> GetPass(uint64_t passIdx);

private:
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

	void Init(std::string name, uint64_t width, uint64_t height);

	void Render(CommandContext& Context);

	void Destroy();

private:

private:
	DeferredRendering(const DeferredRendering& copy) = delete;
	DeferredRendering(DeferredRendering&& copy) = delete;
	DeferredRendering& operator=(const DeferredRendering& other) = delete;
};
