#pragma once

#include "../TiledRendering/VectorMath.h"

namespace IMath
{
    class BoundingSphere
    {
    public:
        BoundingSphere() {}
        BoundingSphere(Vector3 center, Scalar radius);
        explicit BoundingSphere(Vector4 sphere);

        Vector3 GetCenter(void) const;
        Scalar GetRadius(void) const;

    private:

        Vector4 m_repr;
    };

    //=======================================================================================================
    // Inline implementations
    //

    inline BoundingSphere::BoundingSphere(Vector3 center, Scalar radius)
    {
        m_repr = Vector4(center);
        m_repr.SetW(radius);
    }

    inline BoundingSphere::BoundingSphere(Vector4 sphere)
        : m_repr(sphere)
    {
    }

    inline Vector3 BoundingSphere::GetCenter(void) const
    {
        return Vector3(m_repr);
    }

    inline Scalar BoundingSphere::GetRadius(void) const
    {
        return m_repr.GetW();
    }

} // namespace Math