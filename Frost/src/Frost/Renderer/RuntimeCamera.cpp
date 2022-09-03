#include "frostpch.h"
#include "RuntimeCamera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Frost/Math/Math.h"

namespace Frost
{
	RuntimeCamera::RuntimeCamera(float fov, float aspectRatio, float nearClip, float farClip)
		: Camera(fov, aspectRatio, nearClip, farClip), m_Position()
	{
		m_CameraType = Camera::Type::Runtime;
	}

	void RuntimeCamera::SetTransform(const glm::mat4& transform)
	{
		glm::vec3 translation, rotation, scale;
		Math::DecomposeTransform(transform, translation, rotation, scale);

		m_Position = translation;

		UpdateViewMatrix(m_Position, rotation);

		m_ViewProjecitonMatrix = m_ProjectionMatrix * m_ViewMatrix;

		m_ViewProjecitonMatrix_Vk = m_ProjectionMatrix;
		m_ViewProjecitonMatrix_Vk[1][1] *= -1;
		m_ViewProjecitonMatrix_Vk = m_ViewProjecitonMatrix_Vk * m_ViewMatrix;
	}

	void RuntimeCamera::SetViewportSize(uint32_t width, uint32_t height)
	{
		if (m_ViewportWidth == width && m_ViewportHeight == height) return;

		m_ViewportWidth = width;
		m_ViewportHeight = height;
		Resize((float)width / height);
	}

	void RuntimeCamera::UpdateViewMatrix(const glm::vec3& pos, const glm::vec3& rotation)
	{
		glm::quat orientation;

		// Yaw
		orientation = glm::angleAxis(rotation.x, glm::vec3(0.0f, 1.0f, 0.0f));

		// Pitch
		orientation *= glm::angleAxis(rotation.y, glm::vec3(1.0f, 0.0f, 0.0f) * orientation);
		
		// Roll
		orientation *= glm::angleAxis(rotation.z, glm::vec3(0.0f, 0.0f, 1.0f) * orientation);

		// Compute view matrix
		m_ViewMatrix = glm::translate(glm::mat4_cast(orientation), -pos);
	}

}