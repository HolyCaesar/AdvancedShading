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

//struct PSInput
//{
//    float4 position : SV_POSITION;
//    float4 color : COLOR;
//    float2 tex :TEXTURE0;
//};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 tex :TEXTURE0;
};

PSInput VSMain(float3 position : POSITION, float3 normal : NORMAL, float2 tex : TEXCOORD)
{
    PSInput result;

    result.position = mul(World, float4(position, 1.0f));
    result.position = mul(WorldViewProj, result.position);
    result.normal = normal;
    result.tex = tex;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    //return input.color;
    //return g_texture.Sample(g_sampler, input.tex);
    return float4(1.0f, 1.0f, 0.0f, 1.0f);
}
