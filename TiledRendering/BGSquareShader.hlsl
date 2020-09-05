
//#include "TextureConverterEnum.hlsli"


#if defined(FORMAT_R32G32B32A32)
Texture2D<float4> inputTexture : register(t0);
#endif
#if defined(FORMAT_D32_FLOAT)
Texture2D<float> inputTexture : register(t0);
#endif

SamplerState g_sampler : register(s0);

cbuffer ConstBuffer : register(b0)
{
    matrix World;
    matrix ViewMatrix;
    matrix WorldViewProj;
};

struct VSInput
{
    float3 position : POSITION;
};

struct GSInput
{
    float4 position : POSITION;
    float2 tex : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;  // Clip space position
    float2 tex :TEXTURE0;           // Texture coordinate
};

GSInput VSMain(VSInput input)
{
    GSInput output = (GSInput)0;
    output.position = float4(input.position, 1.0f);
    output.tex = float2(0.0f, 0.0f);
    return output;
}

[maxvertexcount(6)]
void GSMain(triangle GSInput input[3], inout TriangleStream<PSInput> triStream)
{
    PSInput v1 = (PSInput)0;
    v1.position = input[0].position;
    v1.tex = float2(0.0f, 0.0f);

    PSInput v2 = (PSInput)0;
    v2.position = input[1].position;
    v2.tex = float2(1.0f, 0.0f);

    PSInput v3 = (PSInput)0;
    v3.position = input[2].position;
    v3.tex = float2(1.0f, 1.0f);

    triStream.Append(v1);
    triStream.Append(v2);
    triStream.Append(v3);

    triStream.RestartStrip();

    v2.position.x = -1.0f;
    v2.position.y = -1.0f;
    v2.tex = float2(0.0f, 1.0f);

    triStream.Append(v1);
    triStream.Append(v3);
    triStream.Append(v2);
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 color = float4(1.0f, 1.0f, 1.0f, 1.0f);
#if defined(FORMAT_R32G32B32A32)
    color = inputTexture.Sample(g_sampler, input.tex);
#endif
#if defined(FORMAT_D32_FLOAT)
    float d = inputTexture.Sample(g_sampler, input.tex);
    d = saturate((1.0f - d) * 1000.0f);
    color.x = d; color.y = d; color.z = d; color.w = 1.0f;
#endif

	return color;
}

//
// \ For testing purpose
//
//float4 PSMain(PSInput input) : SV_TARGET
//{
//    float4 color = float4(1.0f, 1.0f, 1.0f, 1.0f);
//    float d = inputTexture.Sample(g_sampler, input.tex);
//    d = saturate((1.0f - d) * 1000.0f);
//    color.x = d; color.y = d; color.z = d; color.w = 1.0f;
//    return color;
//}