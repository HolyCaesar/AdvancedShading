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
	float2 tex : TEXCOORD0;
};

struct PSInput
{
	float4 position : SV_POSITION;  // Clip space position
	float3 positionVS : TEXCOORD1;  // view space position
	float3 normal : NORMAL;
	float2 tex :TEXTURE0;           // Texture coordinate
};

PSInput VSMain(VSInput input)
{
	PSInput result;

	result.position = mul(WorldViewProj, float4(input.position, 1.0f));
	result.positionVS = mul(ViewMatrix, float4(input.position, 1.0f));
	result.normal = mul(World, float4(input.normal, 1.0f)).xyz;
	result.tex = input.tex;

	return result;
}

StructuredBuffer<Light> g_Lights : register(t0);
SamplerState g_sampler : register(s0);


struct PSOut
{
	float4 res1 : SV_TARGET0;
	float4 res2 : SV_TARGET1;
};

//PSOut PSMain(PSInput input)
//{
//	PSOut test;
//
//	test.res1 = float4(0.0f, 1.0f, 1.0f, 1.0f);
//	test.res2 = float4(1.0f, 0.0f, 0.0f, 1.0f);
//
//	return test;
//}

float4 PSMain(PSInput input) : SV_TARGET
{
	float4 color = float4(0.0f, 0.0f, 0.0f, 1.0f);
	const float4 eyePos = { 0, 0, 0, 1 };
	float4 posVS = float4(input.positionVS, 1.0f);
	float4 viewVS = normalize(eyePos - posVS);

	LightingResult lit = (LightingResult)0;

	for (uint i = 0; i < 1; ++i)
	{
		Light light = g_Lights[i];

		LightingResult res = (LightingResult)0;

		// TODO: uncommented this will give black image, no idea why this happens
		if (!light.Enabled) continue;

		if (light.Type != DIRECTIONAL_LIGHT && length(mul(ViewMatrix, light.PositionWS) - posVS) > light.Range) continue;

		switch (light.Type)
		{
		case POINT_LIGHT:
		{
			res = DoPointLight(light, viewVS, posVS, float4(input.normal, 0.0f), ViewMatrix);
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
}