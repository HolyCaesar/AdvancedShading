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

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 tex :TEXTURE0;
};

PSInput VSMain(float4 position : POSITION, float4 color : COLOR, float2 tex : TEXCOORD)
{
    PSInput result;

    result.position = mul(World, position);
    result.position = mul(WorldViewProj, result.position);
    result.color = color;
    result.tex = tex;

    //result.position = position;
    //result.color = color;

    //result.position = position + offset;
    //result.color = color;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    //return input.color;
    return g_texture.Sample(g_sampler, input.tex);
}
