#pragma once
#include "stdafx.h"

using namespace DirectX;

const float LIGHT_RANGE_MIN = 20.0f;
const float LIGHT_RANGE_MAX = 1000.0f;
const float LIGHT_SPOT_ANGLE_MIN = 5.0f; // Unit in degree, hlsl will convert it to radians
const float LIGHT_SPOT_ANGLE_MAX = 90.0f; // Unit in degree, hlsl will convert it to radians

__declspec(align(16)) struct Light
{
    enum class LightType : uint32_t
    {
        Point = 0,
        Spot = 1,
        Directional = 2
    };

    /**
    * Position for point and spot lights (World space).
    */
    XMFLOAT4 m_PositionWS;
    //--------------------------------------------------------------( 16 bytes )
    /**
    * Direction for spot and directional lights (World space).
    */
    XMFLOAT4 m_DirectionWS;
    //--------------------------------------------------------------( 16 bytes )
    /**
    * Positions for point and spot lights (View space).
    */
    XMFLOAT4 m_PositionVS;
    //--------------------------------------------------------------( 16 bytes )
    /**
    * Direction for spot and directional lights (View space).
    */
    XMFLOAT4 m_DirectionVS;
    //--------------------------------------------------------------( 16 bytes )
    /**
     * Color of the light. Diffuse and specular colors are not separated.
     */
    XMFLOAT4 m_Color;
    //--------------------------------------------------------------( 16 bytes )
    /**
     * The half angle of the spotlight cone.
     */
    float m_SpotlightAngle;
    /**
     * The range of the light.
     */
    float m_Range;

    /**
     * The intensity of the light.
     */
    float m_Intensity;

    /**
     * Disable or enable the light.
     */
    uint32_t    m_Enabled;
    //--------------------------------------------------------------(16 bytes )

    /**
     * True if the light is selected in the editor.
     */
    uint32_t    m_Selected;
    /**
     * The type of the light.
     */
    LightType   m_Type;

    XMFLOAT2 m_Padding;

    //--------------------------------------------------------------(16 bytes )
    //--------------------------------------------------------------( 16 * 7 = 112 bytes )
	Light()
		: m_PositionWS(0, 0, 0, 1)
		, m_DirectionWS(0, 0, -1, 0)
		, m_PositionVS(0, 0, 0, 1)
		, m_DirectionVS(0, 0, 1, 0)
		, m_Color(1, 1, 1, 1)
		, m_SpotlightAngle(45.0f)
		, m_Range(1000.0f)
		, m_Intensity(1.0f)
		, m_Enabled(true)
		, m_Selected(false)
		, m_Type(LightType::Point)
		, m_Padding(XMFLOAT2(0.0f, 0.0f))
    {}
};
