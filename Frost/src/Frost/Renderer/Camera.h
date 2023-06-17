#pragma once

#include <glm/glm.hpp>

namespace Frost
{
	class Camera
	{
	public:
		enum class Type
		{
			None,
			Editor,
			Runtime
		};

	public:
		Camera() = default;
		Camera(float fov, float aspectRatio, float nearClip, float farClip);
		virtual ~Camera() = default;

		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		void SetProjectionMatrix(const glm::mat4& projectionMatrix) { m_ProjectionMatrix = projectionMatrix; }

		virtual const glm::mat4& GetViewMatrix() const = 0;
		virtual const glm::mat4& GetViewProjection() const = 0;
		virtual const glm::mat4& GetViewProjectionVK() const = 0;
		virtual const glm::vec3& GetPosition() const = 0;

		void SetFOV(float fov) { m_FOV = fov; }
		void SetNearClip(float nearClip) { m_NearClip = nearClip; }
		void SetFarClip(float farClip) { m_FarClip = farClip; }

		void Resize(float aspectRatio);

		float GetExposure() const { return m_Exposure; }
		float& GetExposure() { return m_Exposure; }
		
		float& GetFarClip() { return m_FarClip; }
		float GetFarClip() const { return m_FarClip; }

		float& GetNearClip() { return m_NearClip; }
		float GetNearClip() const { return m_NearClip; }
		
		float& GetCameraFOV() { return m_FOV; }
		float GetCameraFOV() const { return m_FOV; }

		float GetDOF() const { return m_DOF; }
		float& GetDOF() { return m_DOF; }

		void SetViewportSize(uint32_t width, uint32_t height);
		uint32_t GetViewportWidth() const { return m_ViewportWidth; }
		uint32_t GetViewportHeight() const { return m_ViewportHeight; }

		Camera::Type GetCameraType() const { return m_CameraType; }
	public:
		void RecalculateProjectionMatrix();
	protected:
		glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);

		uint32_t m_ViewportWidth = 1600, m_ViewportHeight = 900;

		float m_FOV = 70.0f;
		float m_NearClip, m_FarClip, m_AspectRatio;

		float m_Exposure = 0.7f;
		float m_DOF = 0.0f; // This is just the lod of the skybox ("dof" is a fancy name here)

		Camera::Type m_CameraType = Camera::Type::None;
	};
}