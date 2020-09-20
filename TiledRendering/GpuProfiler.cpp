#include "GpuProfiler.h"
#include "GraphicsCore.h"

// It's called an unnamed namespace / anonymous namespace. 
// It's use is to make functions / objects / etc accessible only within that file.It's almost the same as static in C.
namespace
{
    ID3D12QueryHeap* sm_QueryHeap = nullptr;
    ID3D12Resource* sm_ReadBackBuffer = nullptr;
    uint64_t* sm_TimeStampBuffer = nullptr;
    uint64_t sm_Fence = 0;
    uint32_t sm_MaxNumTimers = 0;
    uint32_t sm_NumTimers = 1;
    uint64_t sm_ValidTimeStart = 0;
    uint64_t sm_ValidTimeEnd = 0;
    double sm_GpuTickDelta = 0.0;
}

void GpuTimeCore::Initialize(uint32_t MaxNumTimers)
{
    uint64_t GpuFrequency;
    IGraphics::g_GraphicsCore->g_CommandManager->GetCommandQueue()->GetTimestampFrequency(&GpuFrequency);
    sm_GpuTickDelta = 1.0 / static_cast<double>(GpuFrequency);

    D3D12_HEAP_PROPERTIES HeapProps;
    HeapProps.Type = D3D12_HEAP_TYPE_READBACK;
    HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    HeapProps.VisibleNodeMask = 1;
    HeapProps.CreationNodeMask = 1;

    D3D12_RESOURCE_DESC BufferDesc;
    BufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    BufferDesc.Alignment = 0;
    BufferDesc.Width = sizeof(uint64_t) * MaxNumTimers * 2;
    BufferDesc.Height = 1;
    BufferDesc.DepthOrArraySize = 1;
    BufferDesc.MipLevels = 1;
    BufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    BufferDesc.SampleDesc.Count = 1;
    BufferDesc.SampleDesc.Quality = 0;

    ASSERT_SUCCEEDED(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateCommittedResource(
        &HeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&sm_ReadBackBuffer)));
    sm_ReadBackBuffer->SetName(L"GpuTimeStamp Buffer");

    D3D12_QUERY_HEAP_DESC QueryHeapDesc;
    QueryHeapDesc.Count = MaxNumTimers * 2;
    QueryHeapDesc.NodeMask = 1;
    QueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    ASSERT_SUCCEEDED(IGraphics::g_GraphicsCore->g_pD3D12Device->CreateQueryHeap(
        &QueryHeapDesc, IID_PPV_ARGS(&sm_QueryHeap)));
    sm_QueryHeap->SetName(L"GpuTimeStamp QueryHeap");

    sm_MaxNumTimers = (uint32_t)MaxNumTimers;
}

void GpuTimeCore::Shutdown()
{
    if (sm_ReadBackBuffer != nullptr)
        sm_ReadBackBuffer->Release();
    if (sm_QueryHeap != nullptr)
        sm_QueryHeap->Release();
}

uint32_t GpuTimeCore::NewTimer(void)
{
    return sm_NumTimers++;
}

void GpuTimeCore::StartTimer(CommandContext& Context, uint32_t TimerIdx)
{
    Context.InsertTimeStamp(sm_QueryHeap, TimerIdx * 2); // Multiply by 2 because each time stamp has a start and end. e.g. id = 1, id2 and id3 for start and end respectivly
}

void GpuTimeCore::StopTimer(CommandContext& Context, uint32_t TimerIdx)
{
    Context.InsertTimeStamp(sm_QueryHeap, TimerIdx * 2 + 1);
}

void GpuTimeCore::BeginReadBack(void)
{
    IGraphics::g_GraphicsCore->g_CommandManager->WaitForFence(sm_Fence);

    D3D12_RANGE Range;
    Range.Begin = 0;
    Range.End = (sm_NumTimers * 2) * sizeof(uint64_t);
    ASSERT_SUCCEEDED(sm_ReadBackBuffer->Map(0, &Range, reinterpret_cast<void**>(&sm_TimeStampBuffer)));

    sm_ValidTimeStart = sm_TimeStampBuffer[0];
    sm_ValidTimeEnd = sm_TimeStampBuffer[1];

    // On the first frame, with random values in the timestamp query heap, we can avoid a misstart.
    if (sm_ValidTimeEnd < sm_ValidTimeStart)
    {
        sm_ValidTimeStart = 0ull;
        sm_ValidTimeEnd = 0ull;
    }
}

void GpuTimeCore::EndReadBack(void)
{
    // Unmap with an empty range to indicate nothing was written by the CPU
    D3D12_RANGE EmptyRange = {};
    sm_ReadBackBuffer->Unmap(0, &EmptyRange);
    sm_TimeStampBuffer = nullptr;

    CommandContext& Context = CommandContext::Begin();
    Context.InsertTimeStamp(sm_QueryHeap, 1);
    Context.ResolveTimeStamps(sm_ReadBackBuffer, sm_QueryHeap, sm_NumTimers * 2);
    Context.InsertTimeStamp(sm_QueryHeap, 0);
    sm_Fence = Context.Finish();
}

float GpuTimeCore::GetTime(uint32_t TimerIdx)
{
    ASSERT(sm_TimeStampBuffer != nullptr, "Time stamp readback buffer is not mapped");
    ASSERT(TimerIdx < sm_NumTimers, "Invalid GPU timer index");

    uint64_t TimeStamp1 = sm_TimeStampBuffer[TimerIdx * 2];
    uint64_t TimeStamp2 = sm_TimeStampBuffer[TimerIdx * 2 + 1];

    if (TimeStamp1 < sm_ValidTimeStart || TimeStamp2 > sm_ValidTimeEnd || TimeStamp2 <= TimeStamp1)
        return 0.0f;

    return static_cast<float>(sm_GpuTickDelta * (TimeStamp2 - TimeStamp1));
}

