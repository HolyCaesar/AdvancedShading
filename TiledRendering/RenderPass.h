#pragma once
#include "stdafx.h"
#include "Object.h"
#include "DX12RootSignature.h"
#include "DX12PipelineState.h"
#include "DX12GraphicsCommon.h"
#include "SamplerManager.h"
#include "DX12AuxiliaryLib.h"
#include "GpuBuffer.h"
#include "GpuResource.h"

#include <string>
#include <unordered_map>
#include <map>
#include <tuple>

class CommandContext;

using Microsoft::WRL::ComPtr;
using namespace DX12Aux;

// A pass has two types: compute and rendering
// Pack the memory usage for cache efficiency
template <class T>
struct RenderPassRes
{
	vector<T> resPool;
	unordered_map<std::string, int> mapping; // Root Index and the index of the resource in the resPool

	void Clear()
	{
		resPool.clear();
		mapping.clear();
	}
};

class RenderPass : public Object
{
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
		uint64_t bufSize,
		const void* constBufPtr)
	{
		std::string bufName(buffName.begin(), buffName.end());
		m_constantBufferMap.resPool.push_back({ rootIndex, bufSize, constBufPtr });
		m_constantBufferMap.mapping[bufName] =  static_cast<int>(m_constantBufferMap.resPool.size()) - 1;
	}

	void AddShader(
		const std::string name,
		ShaderType type,
		ComPtr<ID3DBlob> pShader);

	// Finalize root signature
	virtual void FinalizeRootSignature(std::shared_ptr<DX12RootSignature> pRS = nullptr) = 0;

	// Finalize pipeline state object
	virtual void FinalizePSO(std::shared_ptr<DX12PSO> pPSO = nullptr) = 0;

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

	RenderPassRes< std::tuple<uint64_t, uint64_t, const void*>> m_constantBufferMap;

	std::shared_ptr<DX12RootSignature> m_rootSignature;
	std::shared_ptr<DX12PSO>					m_piplineState;

	std::unordered_map<ShaderType, ComPtr<ID3DBlob>> m_shaders;
private:
	RenderPass(const RenderPass& copy) = delete;
	RenderPass(RenderPass&& copy) = delete;
	RenderPass& operator=(const RenderPass& other) = delete;
};

class DX12ShadingPass : public RenderPass
{
public:
	DX12ShadingPass();
	virtual ~DX12ShadingPass();

	void AddSamplerDesc(
		const std::string name, 
		SamplerDesc samplerDesc);

	void FinalizeRootSignature(std::shared_ptr<DX12RootSignature> pRS = nullptr);

	void FinalizePSO(std::shared_ptr<DX12PSO> pPSO = nullptr);

	// Destroy Resources
	void Destroy();

	void PreRender(GraphicsContext& gfxContext);

	void Render(GraphicsContext& gfxContext);

	void PostRender(GraphicsContext& gfxContext);

	void SetViewPortAndScissorRect(CD3DX12_VIEWPORT vp, CD3DX12_RECT rect)
	{
		m_viewport = vp;
		m_scissorRect = rect;
	}

	void SetIndexBuffer(std::shared_ptr<StructuredBuffer> idxBuffer)
	{
		m_pIndexBuffer = idxBuffer;
	}

	void SetVertexBuffer(std::shared_ptr<StructuredBuffer> vexBuffer)
	{
		m_pVertexBuffer = vexBuffer;
	}

	void SetRenderTarget(
		const std::wstring& Name,
		uint32_t Width,
		uint32_t Height,
		uint32_t NumMips,
		DXGI_FORMAT Format,
		D3D12_GPU_VIRTUAL_ADDRESS VidMem = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
	{
		m_renderTarget.Create(Name, Width, Height, NumMips, Format, VidMem);
	}

	void SetDepthBuffer(
		const std::wstring& Name, 
		uint32_t Width, 
		uint32_t Height, 
		DXGI_FORMAT Format,
		D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
	{
		m_depthBuffer.Create(Name, Width, Height, Format, VidMemPtr);
	}

protected:
	uint64_t m_samplerIdx;
	std::map<int, std::pair<std::string, SamplerDesc>> m_samplers;

	// DX12 related variables
	D3D12_PRIMITIVE_TOPOLOGY m_primitiveTopology;
	std::shared_ptr<StructuredBuffer> m_pIndexBuffer;
	std::shared_ptr<StructuredBuffer> m_pVertexBuffer;

	ColorBuffer m_renderTarget;
	DepthBuffer m_depthBuffer;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;

//private:
//	DX12ShadingPass(const DX12ShadingPass& copy) = delete;
//	DX12ShadingPass(DX12ShadingPass&& copy) = delete;
//	DX12ShadingPass& operator=(const DX12ShadingPass& other) = delete;
};

class DX12ComputePass : public RenderPass
{
public:
	DX12ComputePass();
	DX12ComputePass(const std::string name);
	virtual ~DX12ComputePass();

	void FinalizeRootSignature(std::shared_ptr<DX12RootSignature> pRS = nullptr);

	// Finalize pipeline state object
	void FinalizePSO(std::shared_ptr<DX12PSO> pPSO = nullptr);

	void PreCompute(GraphicsContext& gfxContext);

	void Compute(GraphicsContext& gfxContext);

	void Destroy();

	void SetKernelDimensions(uint64_t xDim, uint64_t yDim, uint64_t zDim)
	{
		m_xKernelSize = xDim;
		m_yKernelSize = yDim;
		m_zKernelSize = zDim;
	}
protected:
	ComPtr<ID3DBlob> m_shader;

	uint64_t m_xKernelSize;
	uint64_t m_yKernelSize;
	uint64_t m_zKernelSize;

	//private:
//	DX12ComputePass(const DX12ComputePass& copy) = delete;
//	DX12ComputePass(DX12ComputePass&& copy) = delete;
//	DX12ComputePass& operator=(const DX12ComputePass& other) = delete;
};
