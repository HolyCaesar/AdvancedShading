// Implementation of light culling compute shader is based on the presentation
// "DirectX 11 Rendering in Battlefield 3" (2011) by Johan Andersson, DICE.
// Retrieved from: http://www.slideshare.net/DICEStudio/directx-11-rendering-in-battlefield-3
// Retrieved: July 13, 2015
// And "Forward+: A Step Toward Film-Style Shading in Real Time", Takahiro Harada (2012)
// published in "GPU Pro 4", Chapter 5 (2013) Taylor & Francis Group, LLC.

#include "CommonIncl.hlsli"

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
    matrix InverseProjection;
    matrix ViewMatrix;
    uint2 ScreenDimensions;
    uint2 Padding;
}

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

RWStructuredBuffer<Frustum> out_Frustums : register(u0);
RWStructuredBuffer<uint> debugUAV : register(u1);
//Texture2D<float> testBuffer : register(t0);

// Calculate the view frustum for each tiled in the view space
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void CS_GridFrustumPass(ComputeShaderInput Input)
{
    const float3 eyePos = float3(0.0f, 0.0f, 0.0f);

    uint tiledBlockSize = BLOCK_SIZE;
    // Compute 4 points on the far clipping plane to use as the frustum vertices
    // Left-hand coordinate system
    float4 tiledVerticesInScreenSpace[4];
    tiledVerticesInScreenSpace[0] = float4(Input.dispatchThreadID.xy * tiledBlockSize, 1.0f, 1.0f);
    tiledVerticesInScreenSpace[1] = float4(float2(Input.dispatchThreadID.x + 1, Input.dispatchThreadID.y) * tiledBlockSize, 1.0f, 1.0f);
    tiledVerticesInScreenSpace[2] = float4(float2(Input.dispatchThreadID.x, Input.dispatchThreadID.y + 1) * tiledBlockSize, 1.0f, 1.0f);
    tiledVerticesInScreenSpace[3] = float4(float2(Input.dispatchThreadID.x + 1, Input.dispatchThreadID.y + 1) * tiledBlockSize, 1.0f, 1.0f);


    float3 tiledVerticesInViewSpace[4];
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        tiledVerticesInViewSpace[i] = ScreenToView(tiledVerticesInScreenSpace[i]).xyz;
    }

    // Build the frustum planes from the view space points
    Frustum frustum;
    // Left plane
    frustum.planes[0] = ComputePlane(eyePos, tiledVerticesInViewSpace[2], tiledVerticesInViewSpace[0]);
    // Right plane
    frustum.planes[1] = ComputePlane(eyePos, tiledVerticesInViewSpace[1], tiledVerticesInViewSpace[3]);
    // Top plane
    frustum.planes[2] = ComputePlane(eyePos, tiledVerticesInViewSpace[0], tiledVerticesInViewSpace[1]);
    // Bottom plane
    frustum.planes[3] = ComputePlane(eyePos, tiledVerticesInViewSpace[3], tiledVerticesInViewSpace[2]);

    // Store the computed frustum in the output buffer
    if (Input.dispatchThreadID.x < numThreads.x && Input.dispatchThreadID.y < numThreads.y)
    {
        uint idx = Input.dispatchThreadID.x + (Input.dispatchThreadID.y * numThreads.x);
        out_Frustums[idx] = frustum;
        //debugUAV[idx] = idx + testBuffer.Load(int3(1, 0, 0), 0).r;
        debugUAV[idx] = idx;
    }
}