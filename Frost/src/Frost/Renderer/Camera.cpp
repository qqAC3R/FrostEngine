#include "frostpch.h"
#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Frost
{
	Camera::Camera(float fov, float aspectRatio, float nearClip, float farClip)
		: m_ProjectionMatrix(glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip)),
		  m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip)
	{
	}

	void Camera::Resize(float aspectRatio)
	{
		m_AspectRatio = aspectRatio;
		RecalculateProjectionMatrix();
	}

	void Camera::RecalculateProjectionMatrix()
	{
		m_ProjectionMatrix = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
	}

}