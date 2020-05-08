//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

cbuffer ConstBuffer : register(b0)
{
    matrix World;
    matrix WorldViewProj;
    //float4 offset;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 tex :TEXTURE0;
};

//PSInput VSMain(float3 position : POSITION, float3 normal : NORMAL, float2 tex : TEXCOORD)
PSInput VSMain(VSInput input)
{
    PSInput result;

    //result.position = mul(World, float4(input.position, 1.0f));
    result.position = mul(World, float4(input.position, 1.0f));
    result.position = mul(WorldViewProj, result.position);
    result.normal = input.normal;
    result.tex = input.tex;

    return result;
}

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

    //return g_texture.Sample(g_sampler, input.tex);
    return color;
}

float4 PS_SceneDepth(PSInput input) : SV_TARGET
{
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}
