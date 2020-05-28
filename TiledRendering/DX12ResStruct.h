#pragma once
#include "stdafx.h"

using Microsoft::WRL::ComPtr;

struct DX12Resource
{
    DX12Resource() :
        uSrvDescriptorOffset(0),
        uUavDescriptorOffset(0),
        uCbvDescriptorOffset(0),
        pResource(nullptr),
        mUsageState(D3D12_RESOURCE_STATE_COMMON)
    {
    }

    ComPtr<ID3D12Resource> pResource;
    ComPtr<ID3D12Resource> pResourceUAVCounter;
    UINT uSrvDescriptorOffset;
    UINT uUavDescriptorOffset;
    UINT uCbvDescriptorOffset;
    D3D12_RESOURCE_STATES mUsageState;
};

