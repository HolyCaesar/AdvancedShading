
#define threadBlockSize 64 


//StructuredBuffer<float4> csInput : register(t0);    // SRV: Wrapped constant buffers
Texture2D<float4> csInput : register(t0);
RWTexture2D<float4> csOutput : register(u0);
//AppendStructuredBuffer<float4> csOutput: register(u0);    // UAV: Processed indirect commands

[numthreads(threadBlockSize, 1, 1)]
//void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex, uint3 threadID : SV_DispatchThreadID)
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    // Each thread of the CS operates on one of the indirect commands.
    uint index = (groupId.x * threadBlockSize) + groupIndex;
    uint2 idx = uint2(index / (2 * threadBlockSize), index % (2 * threadBlockSize));

    float4 val = csInput.Load(uint3(index, 0, 0));
    //csOutput[idx] = float4(2.0f, 2.0f, 2.0f, 2.0f);
    csOutput[uint2(index, 0)] = val;
}