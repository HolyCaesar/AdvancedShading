#pragma once

#include "VectorMath.h"
#include "../MiniEngineMath/Frustum.h"

namespace IMath
{
	class BaseCamera
	{
	public:
		void Update();

		void SetEyeAtUp(Vector3 eye, Vector3 at, Vector3 up);
		void SetLookDirection(Vector3 forward, Vector3 up);
		void SetRotation(Quaternion basisRotation);
		void SetPosition(Vector3 worldPos);
		void SetTransform(const AffineTransform& xform);
		void SetTransform(const OrthogonalTransform& xform);

		const Quaternion GetRotation() const { return m_CameraToWorld.GetRotation(); }
		const Vector3 GetRightVec() const { return m_Basis.GetX(); }
		const Vector3 GetUpVec() const { return m_Basis.GetY(); }
		const Vector3 GetForwardVec() const { return -m_Basis.GetZ(); }
		const Vector3 GetPosition() const { return m_CameraToWorld.GetTranslation(); }

		const Matrix4& GetViewMatrix() const { return m_ViewMatrix; }
		const Matrix4& GetProjMatrix() const { return m_ProjMatrix; }
		const Matrix4& GetViewProjMatrix() const { return m_ViewProjMatrix; }
		const Matrix4& GetReprojectionMatrix() const { return m_ReprojectMatrix; }
		const Frustum& GetViewSpaceFrustum() const { return m_FrustumVS; }
		const Frustum& GetWorldSpaceFrustum() const { return m_FrustumWS; }

	protected:
		BaseCamera() : m_CameraToWorld(kIdentity), m_Basis(kIdentity) {}

		void SetProjMatrix(const Matrix4& ProjMat) { m_ProjMatrix = ProjMat; }

		OrthogonalTransform m_CameraToWorld;

		// Redundant data cached for faster lookups
		Matrix3 m_Basis;

		// Transforms homogeneous coordinates from world space to view space.
		// In this case, view space is defined as + X is to the right, +Y is up, and -Z is forward.
		Matrix4 m_ViewMatrix;

		Matrix4 m_ProjMatrix;

		Matrix4 m_ViewProjMatrix;

		Matrix4 m_PreviousViewProjMatrix;

		Matrix4 m_ReprojectMatrix;

		Frustum m_FrustumVS;
		Frustum m_FrustumWS;
	};

	class Camera : public BaseCamera
	{
	public:
		Camera();

		// Controls the view-to-projection matrix
		void SetPerspectiveMatrix(float verticalFovRadians, float aspectHeightOverWidth, float nearZClip, float farZClip);
		void SetFOV(float verticalFovInRadians) { m_VerticalFOV = verticalFovInRadians; UpdateProjMatrix(); }
		void SetAspectRatio(float heightOverWidth) { m_AspectRatio = heightOverWidth; UpdateProjMatrix(); }
		void SetZRange(float nearZ, float farZ) { m_NearClip = nearZ; m_FarClip = farZ; UpdateProjMatrix(); }
		void ReverseZ(bool enable) { m_ReverseZ = enable; UpdateProjMatrix(); }

		float GetFOV() const { return m_VerticalFOV; }
		float GetNearClip() const { return m_NearClip; }
		float GetFarClip() const { return m_FarClip; }
		float GetClearDepth() const { return m_ReverseZ ? 0.0f : 1.0f; }

	private:
		void UpdateProjMatrix(void);

		float m_VerticalFOV; // Unit in radians
		float m_AspectRatio;
		float m_NearClip;
		float m_FarClip;
		bool m_ReverseZ;    // Invert near and far clip distances so that z = 0 is the far plane
	};

	inline void BaseCamera::SetEyeAtUp(Vector3 eye, Vector3 at, Vector3 up)
	{
		SetLookDirection(at - eye, up);
		SetPosition(eye);
	}

	inline void BaseCamera::SetPosition(Vector3 worldPos)
	{
		m_CameraToWorld.SetTranslation(worldPos);
	}

	inline void BaseCamera::SetTransform(const AffineTransform& xform)
	{
		// By using these functions, we rederive an orthogonal transform.
		SetLookDirection(-xform.GetZ(), xform.GetY());
		SetPosition(xform.GetTranslation());
	}

	inline void BaseCamera::SetRotation(Quaternion basisRotation)
	{
		m_CameraToWorld.SetRotation(Normalize(basisRotation));
		m_Basis = Matrix3(m_CameraToWorld.GetRotation());
	}

	inline Camera::Camera() : m_ReverseZ(true)
	{
		SetPerspectiveMatrix(XM_PIDIV4, 9.0f / 16.0f, 1.0f, 1000.0f);
	}

	inline void Camera::SetPerspectiveMatrix(float verticalFovRadians, float aspectHeightOverWidth, float nearZClip, float farZClip)
	{
		m_VerticalFOV = verticalFovRadians;
		m_AspectRatio = aspectHeightOverWidth;
		m_NearClip = nearZClip;
		m_FarClip = farZClip;

		UpdateProjMatrix();

		m_PreviousViewProjMatrix = m_ViewProjMatrix;
	}
}
