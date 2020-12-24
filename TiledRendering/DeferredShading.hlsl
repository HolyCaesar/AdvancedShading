#include "CommonIncl.hlsli"

cbuffer ConstBuffer : register(b0)
{
    matrix World;
    matrix ViewMatrix;
    matrix WorldViewProj;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;  // Clip space position
    float3 positionVS : TEXCOORD1;  // view space position
    float3 normalVS : NORMAL;
    float2 texCoord :TEXTURE0;           // Texture coordinate
};


//PSInput VSMain(float3 position : POSITION, float3 normal : NORMAL, float2 tex : TEXCOORD)
PSInput VSMain(VSInput input)
{
    PSInput result;

    result.position = mul(WorldViewProj, float4(input.position, 1.0f));
    result.positionVS = mul(ViewMatrix, float4(input.position, 1.0f));
    result.normalVS = mul(World, float4(input.normal, 1.0f)).xyz;
    result.texCoord = input.texCoord;

    return result;
}

struct PSOutput
{
	float4 lightAccumulation : SV_Target0;
	float4 diffuse : SV_Target1;
	float4 specular : SV_Target2;
	float4 normalVS : SV_Target3;
};

StructuredBuffer<Light> g_Lights : register(t0);
SamplerState g_sampler : register(s0);

[earlydepthstencil]
PSOutput PSGeometry(PSInput input)
{
	PSOutput output;

	// TODO may need to add material here in the future
	// I just hard code some parameters here for the bunny's material
    float4 color = float4(0.0f, 0.0f, 0.0f, 1.0f);

    const float4 eyePos = { 0, 0, 0, 1 };
    float4 posVS = float4(input.positionVS, 1.0f);
    float4 viewVS = normalize(eyePos - posVS);

	float4 objDiffuse = float4(0.2f, 0.5f, 0.12f, 1.0f);
	output.diffuse = objDiffuse;
	
	float4 globalAmbient = float4(0.1f, 0.1f, 0.1f, 1.0f);
	float4 ambient = float4(0.0f, 0.8f, 0.4f, 1.0f);
	ambient *= globalAmbient;
	
	float4 emissive = float4(0.3f, 0.21f, 0.05f, 1.0f);
	output.lightAccumulation = ambient + emissive; // Store light accumulation

	float4 N = normalize(float4(input.normalVS, 0.0f));
	output.normalVS = N; // Store normal

	float4 objSpecular = float4(0.5f, 0.5f, 0.5f, 1.0f);
	float specularPower = 0.3f;
	output.specular = float4(objSpecular.rgb, log2(specularPower) / 10.5f);

	return output;
}

Texture2D<float4> lightAccumulation : register(t1);
Texture2D<float4> Diffuse : register(t2);
Texture2D<float4> Specular : register(t3);
Texture2D<float4> normalVS : register(t4);

[earlydepthstencil]
float4 PSMain(PSInput input) : SV_TARGET
{
	return float4(0.0f, 1.0f, 0.0f, 1.0f);
}
