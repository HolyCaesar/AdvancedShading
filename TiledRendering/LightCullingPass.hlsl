#include "CommonIncl.hlsl"

#define BLOCK_SIZE 16 

struct ComputeShaderInput
{
    uint3 groupID           : SV_GroupID;           // 3D index of the thread group in the dispatch.
    uint3 groupThreadID     : SV_GroupThreadID;     // 3D index of local thread ID in a thread group.
    uint3 dispatchThreadID  : SV_DispatchThreadID;  // 3D index of global thread ID in the dispatch.
    uint  groupIndex        : SV_GroupIndex;        // Flattened local index of the thread within a thread group.
};

cbuffer DispatchParams : register(b0)
{
    uint3 numThreadGroups;  // Number of groups dispatched
    uint padding1;
    uint3 numThreads;       // Totla number of threads dispatched
    uint padding2;
    uint2 blockSize;        // 
    uint2 padding3;
}

cbuffer ScreenToViewParams : register(b1)
{
    float4x4 InverseProjection;
    uint2 ScreenDimensions;
    uint2 Padding;
}

//struct FrustumIn
//{
//    float4 plane[4];
//};

float4 ClipToView(float4 clip)
{
    float4 view = mul(InverseProjection, clip);
    // Perspective projection
    view = view / view.w;
    return view;
}

float4 ScreenToView(float4 screenCoordinates)
{
    // Convert to normalized texture coordinates
    float2 texCoord = screenCoordinates.xy / ScreenDimensions;
    // Convert to clip space
    // Reference:
    // https://msdn.microsoft.com/en-us/library/windows/desktop/bb205126(v=vs.85).aspx
    float4 clip = float4(float2(texCoord.x, 1.0f - texCoord.y) * 2.0f - 1.0f, screenCoordinates.z, screenCoordinates.w);

    return ClipToView(clip);
}

StructuredBuffer<Frustum> in_Frustums : register(t0);
StructuredBuffer<Light> Lights : register(t1);
Texture2D<float> DepthTextureVS : register(t2);

// Global counter for current index into the light index list.
// "o_" prefix indicates light lists for opaque geometry while 
// "t_" prefix indicates light lists for transparent geometry.
globallycoherent RWStructuredBuffer<uint> o_LightIndexCounter : register(u0);
globallycoherent RWStructuredBuffer<uint> t_LightIndexCounter : register(u1);

// Light index lists and light grids
RWStructuredBuffer<uint> o_LightIndexList : register(u2);
RWStructuredBuffer<uint> t_LightIndexList : register(u3);
RWTexture2D<uint2> o_LightGrid : register(u4);
RWTexture2D<uint2> t_LightGrid : register(u5);

// Debug UAV
RWStructuredBuffer<float4> debugBuffer : register(u6);

// Group shared variables
groupshared uint uMinDepth;
groupshared uint uMaxDepth;
groupshared Frustum GroupFrustum;

// Opaque geometry light lists
groupshared uint o_LightCount;
groupshared uint o_LightIndexStartOffset;
groupshared uint o_LightList[1024];

// Transparent geometry light lists
groupshared uint t_LightCount;
groupshared uint t_LightIndexStartOffset;
groupshared uint t_LightList[1024];

void o_AppendLight(uint lightIndex)
{
    uint index;
    InterlockedAdd(o_LightCount, 1, index);
    if (index < 1024)
    {
        o_LightList[index] = lightIndex;
    }
}

void t_AppendLight(uint lightIndex)
{
    uint index;
    InterlockedAdd(t_LightCount, 1, index);
    if (index < 1024)
    {
        t_LightList[index] = lightIndex;
    }
}

// Calculate the view frustum for each tiled in the view space
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void CS_LightCullingPass(ComputeShaderInput Input)
{
    int2 texCoord = Input.dispatchThreadID.xy;
    float fDepth = DepthTextureVS.Load(int3(texCoord, 0)).r;
    //float fDepth = 0.5f;

    uint uDepth = asuint(fDepth);

    if (Input.groupIndex == 0)
    {
        uMinDepth = 0xffffffff;
        uMaxDepth = 0;
        o_LightCount = 0;
        t_LightCount = 0;
        GroupFrustum = in_Frustums[Input.groupID.x + (Input.groupID.y * numThreadGroups.x)];
        debugBuffer[Input.groupID.x + (Input.groupID.y * numThreadGroups.x)] = GroupFrustum.planes[0];
        debugBuffer[Input.groupID.x + (Input.groupID.y * numThreadGroups.x) + 20] = float4(Input.groupID.x, Input.groupID.y, uMinDepth, fDepth);
    }

    GroupMemoryBarrierWithGroupSync();

    InterlockedMin(uMinDepth, uDepth);
    InterlockedMax(uMaxDepth, uDepth);

    GroupMemoryBarrierWithGroupSync();

    float fMinDepth = asfloat(uMinDepth);
    float fMaxDepth = asfloat(uMaxDepth);
    
    // Convert depth values to view space
    float minDepthVS = ScreenToView(float4(0, 0, fMinDepth, 1)).z;
    float maxDepthVS = ScreenToView(float4(0, 0, fMaxDepth, 1)).z;
    float nearClipVS = ScreenToView(float4(0, 0, 0, 1)).z;

    // Clipping plane for minimum depth value (used for testing lights within the bounds of opaque geometry)
    Plane minPlane = { float3(0, 0, -1), -minDepthVS };

    //if (Input.groupIndex == 0)
    //{
    //    int idx = Input.groupID.x + (Input.groupID.y * numThreadGroups.x);
    //    debugBuffer[idx] = float4(uMinDepth, uMaxDepth, 0, 0);
    //}

    // Cull Lights
    // Each thread in a group will cull 1 light until all lights have been culled
    for (uint i = Input.groupIndex; i < NUM_LIGHTS; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        if (Lights[i].Enabled)
        {
            Light light = Lights[i];
            switch (light.Type)
            {
            case POINT_LIGHT:
            {
                Sphere sphere = { light.PositionVS.xyz, light.Range };
                sphere.c = float3(0, 1.0f, 0.5f);
                sphere.r = 5.0f;
                nearClipVS = 0.2f;
                maxDepthVS = 15.0f;

                if (SphereInsideFrustum(sphere, GroupFrustum, nearClipVS, maxDepthVS))
                {
                    // Add light to light list for transparent geometry
                    t_AppendLight(i);

                    if (!SphereInsidePlane(sphere, minPlane))
                    {
                        // Add light to light list for opaque geometry
                        o_AppendLight(i);
                    }
                }
            }
            break;
            case SPOT_LIGHT:
            {
                float coneRadius = tan(radians(light.SpotlightAngle)) * light.Range;
                Cone cone = { light.PositionVS.xyz, light.Range, light.DirectionVS.xyz, coneRadius };
                if (ConeInsideFrustum(cone, GroupFrustum, nearClipVS, maxDepthVS))
                {
                    // Add light to light list for transparent geometry
                    t_AppendLight(i);

                    if (!ConeInsidePlane(cone, minPlane))
                    {
                        // Add light to light list for opaque geometry
                        o_AppendLight(i);
                    }
                }
            }
            break;
            case DIRECTIONAL_LIGHT:
            {
                t_AppendLight(i);
                o_AppendLight(i);
            }
            break;
            }
        }
    }

    // Wait till all threads in group have caught up
    GroupMemoryBarrierWithGroupSync();

    // Update global memory with visible light buffer.
    // First update the light grid (only thread 0 in group needs to do this)
    if (Input.groupIndex == 0)
    {
        // Update light grid for opaque geometry.
        InterlockedAdd(o_LightIndexCounter[0], o_LightCount, o_LightIndexStartOffset);
        o_LightGrid[Input.groupID.xy] = uint2(o_LightIndexStartOffset, o_LightCount);

        InterlockedAdd(t_LightIndexCounter[0], t_LightCount, t_LightIndexStartOffset);
        t_LightGrid[Input.groupID.xy] = uint2(t_LightIndexStartOffset, t_LightCount);
    }

    GroupMemoryBarrierWithGroupSync();

    // Now update the light index list (all threads)
    // For opaque geometry.
    for (i = Input.groupIndex; i < o_LightCount; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        o_LightIndexList[o_LightIndexStartOffset + i] = o_LightList[i];
    }
    // For transparent geometry
    for (i = Input.groupIndex; i < t_LightCount; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        t_LightIndexList[t_LightIndexStartOffset + i] = t_LightList[i];
    }

    // TODO may add some debug buffer here
}