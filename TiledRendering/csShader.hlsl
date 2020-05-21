
#define threadBlockSize 64 

struct pjb
{
    float4 data;
};

struct frustum
{
    pjb p[4];
};

//StructuredBuffer<float4> csInput : register(t0);    // SRV: Wrapped constant buffers
Texture2D<float4> csInput : register(t0);
StructuredBuffer<float4> csInputStructure : register(t1);

RWTexture2D<float4> csOutput : register(u0);
RWStructuredBuffer<frustum> csOutputSB : register(u1);
RWTexture2D<uint2> testOutput : register(u2);
//AppendStructuredBuffer<float4> csOutput: register(u0);    // UAV: Processed indirect commands

[numthreads(threadBlockSize, 1, 1)]
//void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex, uint3 threadID : SV_DispatchThreadID)
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    // Each thread of the CS operates on one of the indirect commands.
    uint index = (groupId.x * threadBlockSize) + groupIndex;
    uint2 idx = uint2(index / (2 * threadBlockSize), index % (2 * threadBlockSize));

    float4 val1 = csInput.Load(uint3(index, 0, 0));
    float4 val2 = csInputStructure[index];
    //csOutput[idx] = float4(2.0f, 2.0f, 2.0f, 2.0f);
    csOutput[uint2(index, 0)] = val1 + val2;
    testOutput[uint2(index, 0)] = uint2(1, 1);

    frustum f;
    f.p[0].data = float4(index, index, index, index);
    f.p[1].data = float4(index+1, index, index, index);
    f.p[2].data = float4(index+2, index, index, index);
    f.p[3].data = float4(index+3, index, index, index);
    csOutputSB[index] = f;
}