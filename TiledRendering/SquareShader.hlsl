
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
    GSInput output;

    output.position = float4(input.position, 1.0f);
    output.tex = float2(0.0f, 0.0f);

    return output;
}

[maxvertexcount(6)]
PSInput GSMain(triangle GSInput input[3], inout TriangleStream<PSInput> triStream)
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

Texture2D<float4> inputTex2D : register(t0);
SamplerState g_sampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
    return inputTex2D.Sample(g_sampler, input.tex);
}
