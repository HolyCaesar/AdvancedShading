#pragma once
#include "stdafx.h"
#include "Object.h"
#include "DX12RootSignature.h"
#include "DX12PipelineState.h"
#include "DX12GraphicsCommon.h"
#include "SamplerManager.h"
#include "GpuBuffer.h"
#include "GpuResource.h"

#include <string>
#include <unordered_map>
#include <map>

class CommandContext;

using Microsoft::WRL::ComPtr;

// TODO: need to implement a pass class that is dedicated to just one pass
// A pass has two types: compute and rendering

class RenderPass : public Object
{
public:
	typedef std::map< std::string, std::string > ShaderMacros;
public:
	RenderPass();
	virtual ~RenderPass();

	// Enable or disable the pass. If a pass is disabled, the technique will skip it.
	virtual void SetEnabled(bool enabled);
	virtual bool IsEnabled() const;

	virtual void SetName(std::string name);
	virtual std::string GetName() const;

	inline void AddColorBufferSRV(
		uint64_t rootIndex,
		std::wstring buffName,
		std::shared_ptr<ColorBuffer> colorBufPtr)
	{
		std::string bufName(buffName.begin(), buffName.end());
		m_colorBufferSRVMap[rootIndex] = { bufName, colorBufPtr };
	}

	inline void AddColorBufferUAV(
		uint64_t rootIndex,
		std::wstring buffName,
		std::shared_ptr<ColorBuffer> colorBufPtr)
	{
		std::string bufName(buffName.begin(), buffName.end());
		m_colorBufferUAVMap[rootIndex] = { bufName, colorBufPtr };
	}

	inline void AddDepthBufferSRV(
		uint64_t rootIndex,
		std::wstring buffName,
		std::shared_ptr<DepthBuffer> depthBufPtr)
	{
		std::string bufName(buffName.begin(), buffName.end());
		m_depthBufferSRVMap[rootIndex] = { bufName, depthBufPtr };
	}

	inline void AddStructuredBufferSRV(
		uint64_t rootIndex,
		std::wstring buffName,
		std::shared_ptr<StructuredBuffer> structBufPtr)
	{
		std::string bufName(buffName.begin(), buffName.end());
		m_structuredBufferSRVMap[rootIndex] = { bufName, structBufPtr };
	}

	inline void AddStructuredBufferUAV(
		uint64_t rootIndex,
		std::wstring buffName,
		std::shared_ptr<StructuredBuffer> structBufPtr)
	{
		std::string bufName(buffName.begin(), buffName.end());
		m_structuredBufferUAVMap[rootIndex] = { bufName, structBufPtr };
	}

	inline bool AddConstantBuffer(
		uint64_t rootIndex,
		std::wstring buffName,
		std::shared_ptr<void> constBufPtr)
	{
		std::string bufName(buffName.begin(), buffName.end());
		m_constantBufferMap[rootIndex] = { bufName, constBufPtr };
	}

	// Finalize root signature
	virtual void FinalizeRootSignature(std::shared_ptr<DX12RootSignature> pRS = nullptr) = 0;

	// Finalize pipeline state object
	virtual void FinalizePSO(std::shared_ptr<DX12PSO> pPSO = nullptr) = 0;

	// Bind to graphics pipeline
	virtual void Bind(CommandContext& Context) = 0;

	// Destroy Resources
	virtual void Destroy() = 0;

protected:
	// RenderPass name
	std::string m_renderPassName;

	bool m_bEnabled;

	// Buffer Dictionary
	std::unordered_map<uint64_t, std::pair<std::string, std::shared_ptr<ColorBuffer>>>				m_colorBufferSRVMap;
	std::unordered_map<uint64_t, std::pair<std::string, std::shared_ptr<ColorBuffer>>>				m_colorBufferUAVMap;
	std::unordered_map<uint64_t, std::pair<std::string, std::shared_ptr<StructuredBuffer>>>		m_structuredBufferSRVMap;
	std::unordered_map<uint64_t, std::pair<std::string, std::shared_ptr<StructuredBuffer>>>		m_structuredBufferUAVMap;
	std::unordered_map<uint64_t, std::pair<std::string, std::shared_ptr<DepthBuffer>>>				m_depthBufferSRVMap;
	std::unordered_map<uint64_t, std::pair<std::string, std::shared_ptr<void>>>							m_constantBufferMap;

	std::shared_ptr<DX12RootSignature> m_rootSignature;
	std::shared_ptr<DX12PSO>					m_piplineState;
};

class DX12ShadingPass : public RenderPass
{
public:
	enum ShaderType
	{
		UnknownShaderType = 0,
		VertexShader,
		TessellationControlShader,      // Hull Shader in DirectX
		TessellationEvaluationShader,   // Domain Shader in DirectX
		GeometryShader,
		PixelShader,
	};
public:
	DX12ShadingPass();
	virtual ~DX12ShadingPass();

	HRESULT LoadShaderFromFile(
		ShaderType type,
		std::string shaderName,
		std::string shaderEntryPoint,
		std::wstring shaderPath,
		const ShaderMacros& shaderMacros,
		std::string shaderHlslVersion);

	HRESULT LoadShaderFromString(
		ShaderType type,
		std::string shaderName,
		std::string shaderEntryPoint,
		const std::string& source,
		const ShaderMacros& shaderMacros,
		std::string shaderHlslVersion);

	void AddSamplerDesc(
		const std::string name, 
		SamplerDesc samplerDesc);

	void FinalizeRootSignature(std::shared_ptr<DX12RootSignature> pRS = nullptr);

	void FinalizePSO(std::shared_ptr<DX12PSO> pPSO = nullptr);

	// Bind to graphics pipeline
	void Bind(CommandContext& Context);

	// Destroy Resources
	void Destroy();

	void PreRender();

	void Render();

	void PostRender();

protected:
	std::unordered_map<ShaderType, ComPtr<ID3DBlob>> m_shaders;

	uint64_t m_samplerIdx;
	std::map<int, std::pair<std::string, SamplerDesc>> m_samplers;

};

class DX12ComputePass : public RenderPass
{
public:
	DX12ComputePass();
	virtual ~DX12ComputePass();

	HRESULT LoadComputeShader();

	void FinalizeRootSignature(std::shared_ptr<DX12RootSignature> pRS = nullptr);

	void Compute();
protected:
	ComPtr<ID3DBlob> m_shader;
};