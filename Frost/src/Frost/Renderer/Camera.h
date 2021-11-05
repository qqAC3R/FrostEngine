#pragma once

#include <glm/glm.hpp>

namespace Frost
{
	class Camera
	{
	public:
		Camera() = default;
		Camera(float fov, float aspectRatio, float nearClip, float farClip);
		virtual ~Camera() = default;

		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		void SetProjectionMatrix(const glm::mat4& projectionMatrix) { m_ProjectionMatrix = projectionMatrix; }

		float GetExposure() const { return m_Exposure; }
		float& GetExposure() { return m_Exposure; }

		float GetDOF() const { return m_DOF; }
		float& GetDOF() { return m_DOF; }
	protected:
		glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
		float m_Exposure = 0.8f;
		float m_DOF = 1.0f; // This is just the lod of the skybox ("dof" is a fancy name here)
	};
}