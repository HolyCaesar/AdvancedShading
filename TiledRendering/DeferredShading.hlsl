#include "CommonIncl.hlsli"

cbuffer ConstBuffer : register(b0)
{
    matrix World;
    matrix ViewMatrix;
    matrix WorldViewProj;
};

cbuffer ScreenToViewParams : register(b1)
{
	matrix InverseProjection;
	matrix ViewMatrix2;
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

Texture2D<float4> LightAccumulation : register(t1);
Texture2D<float4> Diffuse : register(t2);
Texture2D<float4> Specular : register(t3);
Texture2D<float4> NormalVS : register(t4);
Texture2D<float>   Depth : register(t5);

[earlydepthstencil]
float4 PSMain(PSInput input) : SV_TARGET
{
	float4 eyePos = float4(0, 0, 0, 1);

	int2 texCoord = input.texCoord;
	float depth = Depth.Load(int3(texCoord, 0));

	float4 Pt = ScreenToView(float4(texCoord, depth, 1.0f));

	float4 V = normalize(eyePos - Pt);

	//float4 lightAcc = LightAccumulation.Load(int3(texCoord, 0));
	float4 lightAcc = LightAccumulation.Load(int3(624, 380, 0));
	float4 diffuse = Diffuse.Load(int3(texCoord, 0));
	float4 specular = Specular.Load(int3(texCoord, 0));
	float4 N = normalize(NormalVS.Load(int3(texCoord, 0)));

	float specPower = exp2(specular.a * 10.5f);

	return lightAcc;
	return float4(1.0, 1.0, 0.0, 1.0);

	// TODO: need a variable in constant buffer 
	// showing the number of lights
	LightingResult lit = (LightingResult)0;

	for (int i = 0; i < 1; ++i)
	{
		Light light = g_Lights[i];

		switch (light.Type)
		{
		case POINT_LIGHT:
		{
			lit = DoPointLight(light, V, Pt, N, ViewMatrix);
			break;
		}
		case DIRECTIONAL_LIGHT:
		{
			lit = DoDirectionalLight(light, V, Pt, N);
			break;
		}
		case SPOT_LIGHT:
		{
			lit = DoSpotLight(light, V, Pt, N);
			break;
		}
		}
	}
	//return float4(0.0f, 1.0f, 0.0f, 1.0f);
	return lightAcc;
	return saturate(lightAcc + diffuse * lit.lightDiffuse + specular * lit.lightSpecular);
}
