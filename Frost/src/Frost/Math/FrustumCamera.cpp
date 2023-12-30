#include "frostpch.h"
#include "FrustumCamera.h"

#include "Frost/Renderer/EditorCamera.h"

namespace Frost
{
#if 0
	void FrustumCamera::UpdateFrustum(const EditorCamera& camera)
	{
		*this = CreateFrustumFromCamera(camera);
	}

	FrustumCamera FrustumCamera::CreateFrustumFromCamera(const EditorCamera& camera)
	{
		FrustumCamera frustum;
		const glm::vec3 cameraPosition = camera.GetPosition();
		const glm::vec3 frontVector = camera.m_WorldRotation;
		const glm::vec3 rightVector = camera.m_RightDirection;
		const glm::vec3 upVector = camera.m_UpDirection;

		const float halfVSide = camera.m_FarClip * tanf(camera.m_FOV * .5f);
		const float halfHSide = halfVSide * camera.m_AspectRatio;
		const glm::vec3 frontMultFar = camera.m_FarClip * frontVector;

		frustum.m_NearFace = { cameraPosition + camera.m_NearClip * frontVector, frontVector };
		frustum.m_FarFace = { cameraPosition + frontMultFar, -frontVector };

		frustum.m_RightFace = { cameraPosition, glm::cross(upVector, frontMultFar + rightVector * halfHSide) };
		frustum.m_LeftFace = { cameraPosition, glm::cross(frontMultFar - rightVector * halfHSide, upVector) };

		frustum.m_TopFace = { cameraPosition, glm::cross(rightVector, frontMultFar - upVector * halfVSide) };
		frustum.m_BottomFace = { cameraPosition, glm::cross(frontMultFar + upVector * halfVSide, rightVector) };

		return frustum;
	}
#endif
}