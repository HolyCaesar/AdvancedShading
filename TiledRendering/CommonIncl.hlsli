
#define NUM_LIGHTS 8
#ifdef NUM_LIGHTS
#pragma message("NUM_LIGHTS undefined. Default to 8.")
#define NUM_LIGHTS 8
#endif

#define POINT_LIGHT 0
#define SPOT_LIGHT 1
#define DIRECTIONAL_LIGHT 2

typedef float4 Plane;

struct Material
{
    float4  GlobalAmbient;
    //-------------------------- ( 16 bytes )
    float4  AmbientColor;
    //-------------------------- ( 16 bytes )
    float4  EmissiveColor;
    //-------------------------- ( 16 bytes )
    float4  DiffuseColor;
    //-------------------------- ( 16 bytes )
    float4  SpecularColor;
    //-------------------------- ( 16 bytes )
    // Reflective value.
    float4  Reflectance;
    //-------------------------- ( 16 bytes )
    float   Opacity;
    float   SpecularPower;
    // For transparent materials, IOR > 0.
    float   IndexOfRefraction;
    bool    HasAmbientTexture;
    //-------------------------- ( 16 bytes )
    bool    HasEmissiveTexture;
    bool    HasDiffuseTexture;
    bool    HasSpecularTexture;
    bool    HasSpecularPowerTexture;
    //-------------------------- ( 16 bytes )
    bool    HasNormalTexture;
    bool    HasBumpTexture;
    bool    HasOpacityTexture;
    float   BumpIntensity;
    //-------------------------- ( 16 bytes )
    float   SpecularScale;
    float   AlphaThreshold;
    float2  Padding;
    //--------------------------- ( 16 bytes )
};  //--------------------------- ( 16 * 10 = 160 bytes

struct Light
{
    /**
    * Position for point and spot lights (World space).
    */
    float4   PositionWS;
    //--------------------------------------------------------------( 16 bytes )
    /**
    * Direction for spot and directional lights (World space).
    */
    float4   DirectionWS;
    //--------------------------------------------------------------( 16 bytes )
    /**
    * Position for point and spot lights (View space).
    */
    float4   PositionVS;
    //--------------------------------------------------------------( 16 bytes )
    /**
    * Direction for spot and directional lights (View space).
    */
    float4   DirectionVS;
    //--------------------------------------------------------------( 16 bytes )
    /**
    * Color of the light. Diffuse and specular colors are not seperated.
    */
    float4   Color;
    //--------------------------------------------------------------( 16 bytes )
    /**
    * The half angle of the spotlight cone.
    */
    float    SpotlightAngle;
    /**
    * The range of the light.
    */
    float    Range;

    /**
     * The intensity of the light.
     */
    float    Intensity;

    /**
    * Disable or enable the light.
    */
    bool    Enabled;
    //--------------------------------------------------------------( 16 bytes )

    /**
     * Is the light selected in the editor?
     */
    bool    Selected;

    /**
    * The type of the light.
    */
    uint    Type;
    float2  Padding;
    //--------------------------------------------------------------( 16 bytes )
    //--------------------------------------------------------------( 16 * 7 = 112 bytes )
};

//struct Plane
//{
//    float3 N;   // Plane normal.
//    float  d;   // Distance to origin.
//};


struct Sphere
{
    float3 c;   // Center point.
    float  r;   // Radius.
};

struct Cone
{
    float3 T;   // Cone tip.
    float  h;   // Height of the cone.
    float3 d;   // Direction of the cone.
    float  r;   // bottom radius of the cone.
};

// Four planes of a view frustum (in view space).
// The planes are:
//  * Left,
//  * Right,
//  * Top,
//  * Bottom.
// The back and/or front planes can be computed from depth values in the 
// light culling compute shader.
struct Frustum
{
    Plane planes[4];   // left, right, top, bottom frustum planes.
};

Plane ComputePlane(float3 p0, float3 p1, float3 p2)
{
    Plane plane;
    float3 v0 = p1 - p0;
    float3 v1 = p2 - p0;

    //plane.N = normalize(cross(v0, v1));
    //plane.d = dot(plane.N, p0);

    float3 planeNormal = normalize(cross(v0, v1));
    float planeConst = dot(planeNormal, p0);
    plane = float4(planeNormal, planeConst);
    return plane;
}

// Check to see if a sphere is fully behind (inside the negative halfspace of) a plane.
// Source: Real-time collision detection, Christer Ericson (2005)
bool SphereInsidePlane(Sphere sphere, Plane plane)
{
    //return dot(plane.N, sphere.c) - plane.d < -sphere.r;
    return dot(plane.xyz, sphere.c) - plane.w < -sphere.r;
}

// Check to see if a ponit is fully behind (inside the negative halfspace of) a plane
bool PointInsidePlane(float3 p, Plane plane)
{
    //return dot(plane.N, p) - plane.d < 0;
    return dot(plane.xyz, p) - plane.w < 0;
}

// Check to see if a cone if fully behind (inside the negative halfspace of) a plane.
bool ConeInsidePlane(Cone cone, Plane plane)
{
    // Compute the farthest point on the end of the cone to the positive space of the plane.
    //float3 m = cross(cross(plane.N, cone.d), cone.d);
    float3 m = cross(cross(plane.xyz, cone.d), cone.d);
    float3 Q = cone.T + cone.d * cone.h - m * cone.r;

    // The cone is in the negative halfspace of the plane if both
    // the tip of the cone and the farthest point on the end of the cone to the 
    // positive halfspace of the plane are both inside the negative halfspace 
    // of the plane.
    return PointInsidePlane(cone.T, plane) && PointInsidePlane(Q, plane);
}

// Check to see of a light is partially contained within the frustum
bool SphereInsideFrustum(Sphere sphere, Frustum frustum, float zNear, float zFar)
{
    bool result = true;
    return true;

    // First check depth: If the sphere is either fully in front of the near clipping plane, or fully behind the far clipping plane, then the light can be discarded
    // Note: Here, the view vector points in the -Z axis
    // so the far depth value will be approaching -infinity.
    if (sphere.c.z - sphere.r > zNear || sphere.c.z + sphere.r < zFar)
    {
        result = false;
    }

    for (int i = 0; i < 4 && result; ++i)
    {
        if (SphereInsidePlane(sphere, frustum.planes[i]))
        {
            result = false;
        }
    }
    return result;
}

bool ConeInsideFrustum(Cone cone, Frustum frustum, float zNear, float zFar)
{
    bool result = true;

    Plane nearPlane = { float3(0, 0, -1), -zNear };
    Plane farPlane = { float3(0, 0, 1), zFar };

    // First check the near and far clipping planes
    if (ConeInsidePlane(cone, nearPlane) || ConeInsidePlane(cone, farPlane))
    {
        result = false;
    }

    // Then check frustum planes
    for (int i = 0; i < 4 && result; ++i)
    {
        if (ConeInsidePlane(cone, frustum.planes[i]))
        {
            result = false;
        }
    }
    return result;
}

struct LightingResult
{
    float4 lightDiffuse;
    float4 lightSpecular;
};

float4 DoDiffuse(Light light, float4 LightDir, float4 Nor)
{
    float LightEnergy = max(dot(Nor, LightDir), 0);
    return light.Color * LightEnergy;
}

float4 DoSpecular(Light light, float4 ViewDir, float4 LightDir, float4 Nor)
{
    float4 R = normalize(reflect(-LightDir, Nor));
    float RdotV = max(dot(R, ViewDir), 0);

    // TODO the hard core power here will be replaced by the material power in the future
    return light.Color * pow(RdotV, 0.5f);
}

float DoAttenuation(Light light, float d)
{
    return 1.0f - smoothstep(light.Range * 0.75f, light.Range, d);
}

float DoSpotCone(Light light, float4 LightDir)
{
    float minCos = cos(radians(light.SpotlightAngle));
    float maxCos = lerp(minCos, 1, 0.5f);
    float cosAngle = dot(light.DirectionVS, -LightDir);
    return smoothstep(minCos, maxCos, cosAngle);
}

LightingResult DoPointLight(Light light, float4 ViewDir, float4 Pos, float4 Nor)
{
    LightingResult res;
    
    float4 L = light.PositionVS - Pos;
    float distance = length(L);
    L = L / distance;

    float attenuation = DoAttenuation(light, distance);

    res.lightDiffuse = DoDiffuse(light, L, Nor) * attenuation * light.Intensity;
    // TODO need material information to process this
    //res.Specular = DoSpecular(light)
}

LightingResult DoDirectionalLight(Light light, float4 ViewDir, float4 Pos, float4 Nor)
{
    LightingResult res;

    float4 L = normalize(-light.DirectionVS);

    res.lightDiffuse = DoDiffuse(light, L, Nor) * light.Intensity;

    return res;
}

LightingResult DoSpotLight(Light light, float4 ViewDir, float4 Pos, float4 Nor)
{
    LightingResult res;

    float4 L = light.PositionVS - Pos;
    float distance = length(L);
    L = L / distance;

    float attenuation = DoAttenuation(light, distance);
    float spotIntensity = DoSpotCone(light, L);

    res.lightDiffuse = DoDiffuse(light, L, Nor) * attenuation * spotIntensity * light.Intensity;

    return res;
}
