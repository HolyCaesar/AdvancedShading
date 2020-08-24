#pragma once
#include "stdafx.h"

using Microsoft::WRL::ComPtr;

struct DX12Resource
{
    DX12Resource() :
        uSrvDescriptorOffset(-1),
        uUavDescriptorOffset(-1),
        uCbvDescriptorOffset(-1),
        uRtvDescriptorOffset(-1),
        pResource(nullptr),
        mUsageState(D3D12_RESOURCE_STATE_COMMON),
        mFormat(DXGI_FORMAT_UNKNOWN)
    {
    }

    ComPtr<ID3D12Resource> pResource;
    ComPtr<ID3D12Resource> pResourceUAVCounter;
    UINT uSrvDescriptorOffset;
    UINT uUavDescriptorOffset;
    UINT uCbvDescriptorOffset;
    UINT uRtvDescriptorOffset;
    D3D12_RESOURCE_STATES mUsageState;
    DXGI_FORMAT mFormat;
};

