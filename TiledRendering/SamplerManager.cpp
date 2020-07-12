#include "SamplerManager.h"
#include "GraphicsCore.h"
#include "Hash.h"
#include <map>

using namespace std;
using namespace IGraphics;

namespace
{
    map< size_t, D3D12_CPU_DESCRIPTOR_HANDLE > s_SamplerCache;
}

D3D12_CPU_DESCRIPTOR_HANDLE SamplerDesc::CreateDescriptor()
{
    size_t hashValue = Utility::HashState(this);
    auto iter = s_SamplerCache.find(hashValue);
    if (iter != s_SamplerCache.end())
    {
        return iter->second;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE Handle = IGraphics::g_GraphicsCore->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    IGraphics::g_GraphicsCore->g_pD3D12Device->CreateSampler(this, Handle);
    return Handle;
}

void SamplerDesc::CreateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& Handle)
{
    IGraphics::g_GraphicsCore->g_pD3D12Device->CreateSampler(this, Handle);
}
