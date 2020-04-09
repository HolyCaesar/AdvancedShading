#pragma once

#include "stdafx.h"

class GpuResource
{
public:
    GpuResource()
        : m_GpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL),
        m_UserAllocatedMemory(nullptr),
        m_UsageState(D3D12_RESOURCE_STATE_COMMON),
        m_TransitioningState((D3D12_RESOURCE_STATES)-1)
    {
    }

    GpuResource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES CurrentState) :
        m_GpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL),
        m_UserAllocatedMemory(nullptr),
        m_pResource(pResource),
        m_UsageState(CurrentState),
        m_TransitioningState((D3D12_RESOURCE_STATES)-1)
    {
    }

    virtual void Destroy()
    {
        m_pResource = nullptr;
        m_GpuVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
        if (m_UserAllocatedMemory != nullptr)
        {
            //VirtualFree(m_UserAllocatedMemory, 0, MEM_RELEASE);
            m_UserAllocatedMemory = nullptr;
        }
    }

    ID3D12Resource* operator->() { return m_pResource.Get(); }
    const ID3D12Resource* operator->() const { return m_pResource.Get();  }

    ID3D12Resource* GetResource() { return m_pResource.Get(); }
    const ID3D12Resource* GetResource() const { return m_pResource.Get(); }

    D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress() const { return m_GpuVirtualAddress; }

protected:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource;
    D3D12_RESOURCE_STATES m_UsageState;
    D3D12_RESOURCE_STATES m_TransitioningState;
    D3D12_GPU_VIRTUAL_ADDRESS m_GpuVirtualAddress;

    void* m_UserAllocatedMemory;
};

class PixelBuffer : public GpuResource
{
public:
    PixelBuffer() :
        m_Width(0),
        m_Height(0),
        m_ArraySize(0),
        m_Format(DXGI_FORMAT_UNKNOWN),
        m_BankRotation(0)
    {
    }

    ~PixelBuffer() {}

    // Has no effect on Windows
    void SetBankRotation(uint32_t RotationAmount) { m_BankRotation = RotationAmount; }

    // Write the raw pixel buffer contents to a file
    // Note that data is preceded by a 16-byte header:  { DXGI_FORMAT, Pitch (in pixels), Width (in pixels), Height }
    void ExportToFile(const std::wstring& FilePath);

    uint32_t GetWidth(void) const { return m_Width; }
    uint32_t GetHeight(void) const { return m_Height; }
    uint32_t GetDepth(void) const { return m_ArraySize; }
    const DXGI_FORMAT& GetFormat(void) const { return m_Format; }

protected:
    D3D12_RESOURCE_DESC Tex2DDescription(uint32_t Width, uint32_t Height, uint32_t DepthOrArraySize, uint32_t NumMips, DXGI_FORMAT format, UINT Flags);

    void AssociateWithResource(ID3D12Device* D3D12Device, const std::wstring& Name, ID3D12Resource* Resource, D3D12_RESOURCE_STATES CurrentState);

    void CreateTextureResource(ID3D12Device* Device, const std::wstring& Name, const D3D12_RESOURCE_DESC& ResourceDesc,
        D3D12_CLEAR_VALUE ClearValue, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

    static DXGI_FORMAT GetBaseFormat(DXGI_FORMAT Format);
    static DXGI_FORMAT GetUAVFormat(DXGI_FORMAT Format);
    static DXGI_FORMAT GetDSVFormat(DXGI_FORMAT Format);
    static DXGI_FORMAT GetDepthFormat(DXGI_FORMAT Format);
    static DXGI_FORMAT GetStencilFormat(DXGI_FORMAT Format);
    static size_t BytesPerPixel(DXGI_FORMAT Format);

    uint32_t m_Width;
    uint32_t m_Height;
    uint32_t m_ArraySize;
    DXGI_FORMAT m_Format;
    uint32_t m_BankRotation;
};
