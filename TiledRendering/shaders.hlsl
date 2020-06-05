#include "CommonIncl.hlsli"

#ifndef BLOCK_SIZE
#pragma message( "BLOCK_SIZE undefined. Default to 16.")
#define BLOCK_SIZE 16 // should be defined by the application.
#endif



cbuffer ConstBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix WorldViewProj;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;  // Clip space position
    float3 positionVS : TEXCOORD1;  // view space position
    float3 normal : NORMAL;
    float2 tex :TEXTURE0;           // Texture coordinate
};


//PSInput VSMain(float3 position : POSITION, float3 normal : NORMAL, float2 tex : TEXCOORD)
PSInput VSMain(VSInput input)
{
    PSInput result;

    //result.position = mul(World, float4(input.position, 1.0f));
    result.position = mul(World, float4(input.position, 1.0f));
    result.position = mul(WorldViewProj, result.position);
    result.positionVS = mul(View, float4(input.position, 1.0f));
    result.normal = input.normal;
    result.tex = input.tex;

    return result;
}

Texture2D g_texture : register(t0);
Texture2D<uint2> g_lightGrid : register(t1);
StructuredBuffer<uint> g_lightIndex : register(t2);
StructuredBuffer<Light> g_Lights : register(t3);
SamplerState g_sampler : register(s0);

//[earlydepthstencil]
float4 PSMain(PSInput input) : SV_TARGET
{
    float3 lightPos = float3(2.0f, 5.0f, 2.0f);
    //lightPos = vCameraPos.xyz;
    float3 lightDir = float3(0.0f, 0.0f, 0.0f) - lightPos;
    float3 lightD = -lightDir;
    lightD = normalize(lightD);

    float lightIntensity = saturate(dot(normalize(input.normal), lightD));

    float4 color = float4(0.0f, 0.0f, 0.0f, 1.0f);
    float4 diffuseColor = float4(0.5f, 0.3f, 0.6f, 1.0f);
    if (lightIntensity > 0.0f)
    {
        // Determine the final diffuse color based on the diffuse color and the amount of light intensity.
        color += (diffuseColor * lightIntensity);
    }

    color = float4(0.0f, 0.0f, 0.0f, 1.0f);
    const float4 eyePos = { 0, 0, 0, 1 };
    float4 posVS = float4(input.positionVS, 1.0f);
    float4 viewVS = normalize(eyePos - posVS);

    uint2 tileIndex = uint2(floor(input.position.xy / BLOCK_SIZE));
    uint startOffset = g_lightGrid[tileIndex].x;
    uint lightCount = g_lightGrid[tileIndex].y;

    //return float4(startOffset, lightCount, 0, 1);

    LightingResult lit = (LightingResult)0;

    for (uint i = 0; i < lightCount; ++i)
    {
        uint lightIndex = g_lightIndex[startOffset + i];
        Light light = g_Lights[lightIndex];

        LightingResult res = (LightingResult)0;

        if (!light.Enabled) continue;
        // Removed out of range lights
        if (light.Type != DIRECTIONAL_LIGHT && length(light.PositionVS - posVS) > light.Range) continue;

        switch (light.Type)
        {
        case POINT_LIGHT:
        {
            res = DoPointLight(light, viewVS, posVS, float4(input.normal, 0.0f));
        }
        break;
        case SPOT_LIGHT:
        {
            res = DoSpotLight(light, viewVS, posVS, float4(input.normal, 0.0f));
        }
        break;
        case DIRECTIONAL_LIGHT:
        {
            res = DoDirectionalLight(light, viewVS, posVS, float4(input.normal, 0.0f));
        }
        break;
        }
        lit.lightDiffuse += res.lightDiffuse;
        lit.lightSpecular += res.lightSpecular;
    }

    color = saturate(lit.lightDiffuse);

    return color;
    //return g_texture.Sample(g_sampler, input.tex);
}

//[earlydepthstencil]
float4 PS_SceneDepth(PSInput input) : SV_TARGET
{
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}
