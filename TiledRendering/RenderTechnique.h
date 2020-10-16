#pragma once
#include "Object.h"
#include <string>

class CommandContext;

class RenderTechnique : public Object
{
public:
	RenderTechnique();
	~RenderTechnique();

	virtual void Init(std::string name, uint64_t width, uint64_t height) = 0;

	virtual void Render(CommandContext& Context) = 0;

	virtual void Destroy() = 0;

private:
	std::string m_name;

	// TODO: do we need a pass class since a render technique may need multiple passes
	// or I can let each render technique to control how many passes they need.

private:
	RenderTechnique(const RenderTechnique& copy) = delete;
	RenderTechnique(RenderTechnique&& copy) = delete;
	RenderTechnique& operator=(const RenderTechnique& other) = delete;
};

