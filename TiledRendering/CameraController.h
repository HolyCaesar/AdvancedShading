#pragma once

#include "VectorMath.h"
#include <unordered_map>

namespace IMath
{
	class Camera;
}

namespace ICore
{
	using namespace IMath;

	class CameraController
	{
	public:
		CameraController(Camera& camera, Vector3 worldUp);

		void Update(float elapsedTime);
		
		void SlowMovement(bool enable) { m_FineMovement = enable; }
		void SlowRotation(bool enable) { m_FineRotation = enable; }

		void EnableMomentum(bool enable) { m_Momentum = enable; }

		Vector3 GetWorldEast() { return m_WorldEast; }
		Vector3 GetWorldUp() { return m_WorldUp; }
		Vector3 GetWorldNorth() { return m_WorldNorth; }
		float GetCurrentHeading() { return m_CurrentHeading; }
		float GetCurrentPitch() { return m_CurrentPitch; }

		void SetCurrentHeading(float heading) { m_CurrentHeading = heading; }
		void SetCurrentPitch(float pitch) { m_CurrentPitch = pitch; }

		// TODO: I will implement a UserInput to handle different types of input
		// For this demo, keyboard and mouse event is enough
		void KeyEvent(char key, bool isPressed);

	private:
		std::unordered_map<char, bool> isKeyPressed;
		CameraController& operator=(const CameraController&) { return *this; }

		void ApplyMomentum(float& oldValue, float& newValue, float deltaTime);

		Vector3 m_WorldUp;
		Vector3 m_WorldNorth;
		Vector3 m_WorldEast;
		Camera& m_TargetCamera;
		float m_HorizontalLookSensitivity;
		float m_VerticalLookSensitivity;
		float m_MoveSpeed;
		float m_StrafeSpeed;
		float m_MouseSensitivityX;
		float m_MouseSensitivityY;

		float m_CurrentHeading;
		float m_CurrentPitch;

		bool m_FineMovement;
		bool m_FineRotation;
		bool m_Momentum;

		float m_LastYaw;
		float m_LastPitch;
		float m_LastForward;
		float m_LastStrafe;
		float m_LastAscent;
	};
}

