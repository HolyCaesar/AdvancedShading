#pragma once

#include "stdafx.h"
#include "Utility.h"
#include "Color.h"

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

class ColorBuffer : public PixelBuffer
{
public:
    ColorBuffer(Color ClearColor = Color(0.0f, 0.0f, 0.0f, 0.0f))
        : m_ClearColor(ClearColor), m_NumMipMaps(0), m_FragmentCount(1), m_SampleCount(1)
    {
        m_SRVHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
        m_RTVHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
        std::memset(m_UAVHandle, 0xFF, sizeof(m_UAVHandle));
    }

    // Create a color buffer froma swap chain buffer. Unordered access is restricted.
    void CreateFromSwapChain(const std::wstring& Name, ID3D12Resource* BaseResource);

    void Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips,
        DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

    void CreateArray(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t ArrayCount,
        DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

    // Get pre-created CPU-visible descriptor handles
    const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV(void) const { return m_SRVHandle; }
    const D3D12_CPU_DESCRIPTOR_HANDLE& GetRTV(void) const { return m_RTVHandle; }
    const D3D12_CPU_DESCRIPTOR_HANDLE& GetUAV(void) const { return m_UAVHandle[0]; }

    void SetClearColor(Color ClearColor) { m_ClearColor = ClearColor; }
    Color GetClearColor(void) const { return m_ClearColor; }

    void SetMSAAMode(uint32_t NumColorSamples, uint32_t NumCoverageSamples)
    {
        ASSERT(NumCoverageSamples >= NumColorSamples);
        m_FragmentCount = NumColorSamples;
        m_SampleCount = NumCoverageSamples;
    }

    // TODO need to implemented this later, refer to MiniEngine
    //void GenerateMipMaps();
protected:

    D3D12_RESOURCE_FLAGS CombineResourceFlags(void) const
    {
        D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE;

        if (Flags == D3D12_RESOURCE_FLAG_NONE && m_FragmentCount == 1)
            Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        return D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | Flags;
    }

    // Compute the number of texture levels needed to reduce to 1x1.  This uses
    // _BitScanReverse to find the highest set bit.  Each dimension reduces by
    // half and truncates bits.  The dimension 256 (0x100) has 9 mip levels, same
    // as the dimension 511 (0x1FF).
    static inline uint32_t ComputeNumMips(uint32_t Width, uint32_t Height)
    {
        uint32_t HighBit;
        _BitScanReverse((unsigned long*)&HighBit, Width | Height);
        return HighBit + 1;
    }

    void CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips = 1);

    Color m_ClearColor;
    D3D12_CPU_DESCRIPTOR_HANDLE m_SRVHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE m_RTVHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE m_UAVHandle[12];
    uint32_t m_NumMipMaps; // number of texture sublevels
    uint32_t m_FragmentCount;
    uint32_t m_SampleCount;
};

class DepthBuffer : public PixelBuffer
{
public:
    DepthBuffer(float ClearDepth = 0.0f, uint8_t ClearStencil = 0)
        : m_ClearDepth(ClearDepth),
        m_ClearStencil(ClearStencil)
    {
        m_hDSV[0].ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
        m_hDSV[1].ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
        m_hDSV[2].ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
        m_hDSV[3].ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
        m_hDepthSRV.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
        m_hStencilSRV.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
    }

    void Create(const std::wstring& Name, uint32_t Width, uint32_t Height, DXGI_FORMAT Format,
        D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

    void Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumSamples, DXGI_FORMAT Format,
        D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

    // Get pre-created CPU-visible descriptor handles
    const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV() const { return m_hDSV[0]; }
    const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV_DepthReadOnly() const { return m_hDSV[1]; }
    const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV_StencilReadOnly() const { return m_hDSV[2]; }
    const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV_ReadOnly() const { return m_hDSV[3]; }
    const D3D12_CPU_DESCRIPTOR_HANDLE& GetDepthSRV() const { return m_hDepthSRV; }
    const D3D12_CPU_DESCRIPTOR_HANDLE& GetStencilSRV() const { return m_hStencilSRV; }

    float GetClearDepth() const { return m_ClearDepth; }
    uint8_t GetClearStencil() const { return m_ClearStencil; }

private:
    void CreateDerivedViews(ID3D12Device* pDevice, DXGI_FORMAT Format);

    float m_ClearDepth;
    uint8_t m_ClearStencil;
    D3D12_CPU_DESCRIPTOR_HANDLE m_hDSV[4];
    D3D12_CPU_DESCRIPTOR_HANDLE m_hDepthSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE m_hStencilSRV;
};
