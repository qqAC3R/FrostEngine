#pragma once

#include <glm/glm.hpp>

namespace Frost
{
	// Forward declaration
	class EditorCamera;

	struct Plane
	{
		// Distance from origin to the nearest point in the plan
		glm::vec3 Normal = { 0.f, 1.f, 0.f };
		float Distance = 0.f;

		Plane() = default;

		Plane(const glm::vec3& p1, const glm::vec3& norm)
			: Normal(glm::normalize(norm)), Distance(glm::dot(Normal, p1))
		{
		}

		float GetSignedDistanceToPlan(const glm::vec3& point) const { return glm::dot(Normal, point) - Distance; }
	};

	class FrustumCamera
	{
	public:
		FrustumCamera() = default;

		void UpdateFrustum(const EditorCamera& camera);

		FrustumCamera CreateFrustumFromCamera(const EditorCamera& camera);
	public:
		Plane m_TopFace;
		Plane m_BottomFace;

		Plane m_RightFace;
		Plane m_LeftFace;

		Plane m_FarFace;
		Plane m_NearFace;
	};
}