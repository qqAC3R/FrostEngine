#pragma once

#include "Frost/Renderer/Camera.h"

#include "Frost/Core/Timestep.h"
#include "Frost/Events/Event.h"
#include "Frost/Events/KeyEvent.h"
#include "Frost/Events/MouseEvent.h"

#include "Frost/Math/FrustumCamera.h"

#include <glm/glm.hpp>

namespace Frost
{
	enum class CameraMode
	{
		NONE, FLYCAM, ARCBALL
	};

	class EditorCamera : public Camera
	{
	public:
		EditorCamera() = default;
		EditorCamera(float fov, float aspectRatio, float nearClip, float farClip);

		void Focus(const glm::vec3& focusPoint);
		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);

		bool IsActive() const { return m_IsActive; }
		void SetActive(bool active) { m_IsActive = active; }

		inline float GetDistance() const { return m_Distance; }
		inline void SetDistance(float distance) { m_Distance = distance; }

		const glm::vec3& GetFocalPoint() const { return m_FocalPoint; }

		void SetViewportSize(uint32_t width, uint32_t height);

		virtual const glm::mat4& GetViewMatrix() const override { return m_ViewMatrix; }
		virtual const glm::mat4& GetViewProjection() const override { return m_ViewProjecitonMatrix; }
		virtual const glm::mat4& GetViewProjectionVK() const override { return m_ViewProjecitonMatrix_Vk; }
		virtual const glm::vec3& GetPosition() const override { return m_Position; }

		glm::vec3 GetUpDirection() const;
		glm::vec3 GetRightDirection() const;
		glm::vec3 GetForwardDirection() const;


		glm::quat GetOrientation() const;

		float GetPitch() const { return m_Pitch; }
		float GetYaw() const { return m_Yaw; }
		float& GetCameraSpeed() { return m_Speed; }
		float GetCameraSpeed() const { return m_Speed; }
	private:
		void UpdateCameraView();

		bool OnMouseScroll(MouseScrolledEvent& e);
		bool OnKeyPressed(KeyPressedEvent& e);
		bool OnKeyReleased(KeyReleasedEvent& e);

		void MousePan(const glm::vec2& delta);
		void MouseRotate(const glm::vec2& delta);
		void MouseZoom(float delta);

		glm::vec3 CalculatePosition() const;

		std::pair<float, float> PanSpeed() const;
		float RotationSpeed() const;
		float ZoomSpeed() const;
	private:
		glm::mat4 m_ViewMatrix;
		glm::vec3 m_Position, m_WorldRotation, m_FocalPoint;
		
		glm::mat4 m_ViewProjecitonMatrix;
		glm::mat4 m_ViewProjecitonMatrix_Vk;

		bool m_IsActive = false;
		bool m_Panning, m_Rotating;
		glm::vec2 m_InitialMousePosition{};
		glm::vec3 m_InitialFocalPoint, m_InitialRotation;

		float m_Distance;
		float m_Speed{ 0.002f };
		float m_LastSpeed = 0.f;

		float m_Pitch, m_Yaw;
		float m_PitchDelta{}, m_YawDelta{};
		glm::vec3 m_PositionDelta{};
		glm::vec3 m_RightDirection{};
		glm::vec3 m_UpDirection{};

		CameraMode m_CameraMode{ CameraMode::FLYCAM };

		float m_MinFocusDistance = 100.0f;

		uint32_t m_ViewportWidth = 1600, m_ViewportHeight = 900;

		friend class EditorLayer;
		friend class RenderQueue;
		friend class FrustumCamera;
	};
}