
//#include "TextureConverterEnum.hlsli"


#ifdef FORMAT_R32G32B32A32
Texture2D<float4> inputTexture : register(t0);
#endif
#ifdef FORMAT_D32_FLOAT
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
void GSMain(point GSInput input[1], inout TriangleStream<PSInput> triStream)
{
    PSInput v1 = (PSInput)0;
    PSInput v2 = (PSInput)0;
    PSInput v3 = (PSInput)0;
    PSInput v4 = (PSInput)0;

    v1.position = float4(-0.5f, 0.5f, 0.0f, 1.0f);
    v1.tex = float2(0.0f, 0.0f);

    v2.position = float4(0.5f, 0.5f, 0.0f, 1.0f);
    v2.tex = float2(0.0f, 1.0f);

    v3.position = float4(0.5f, -0.5f, 0.0f, 1.0f);
    v3.tex = float2(1.0f, 1.0f);

    v4.position = float4(-0.5f, -0.5f, 0.0f, 1.0f);
    v4.tex = float2(1.0f, 0.0f);

    triStream.Append(v1);
    triStream.Append(v2);
    triStream.Append(v4);

    triStream.RestartStrip();

    triStream.Append(v2);
    triStream.Append(v3);
    triStream.Append(v4);

    triStream.RestartStrip();
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 color = float4(1.0f, 1.0f, 1.0f, 1.0f);
#ifdef FORMAT_R32G32B32A32
    color = inputTexture.Sample(g_sampler, input.tex);
#endif
#ifdef FORMAT_D32_FLOAT
    float d = inputTexture.Sample(g_sampler, input.tex);
    d = 1.0f - d;
    color.x = d; color.y = d; color.z = d; color.w = 1.0f;
#endif

	return color;
}