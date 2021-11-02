#include "frostpch.h"
#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Frost
{
	Camera::Camera(float fov, float aspectRatio, float nearClip, float farClip)
		: m_ProjectionMatrix(glm::perspective(fov, aspectRatio, nearClip, farClip))
	{
	}
}