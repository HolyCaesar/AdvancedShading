#pragma once



class ShadingTechnique
{
public:
	ShadingTechnique();
	~ShadingTechnique();

private:
	ShadingTechnique(const ShadingTechnique& copy) = delete;
	ShadingTechnique(ShadingTechnique&& copy) = delete;
	ShadingTechnique& operator=(const ShadingTechnique& other) = delete;
};

