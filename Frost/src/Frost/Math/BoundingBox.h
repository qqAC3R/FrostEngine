#pragma once

#include <glm/glm.hpp>
#include "FrustumCamera.h"

namespace Frost::Math
{

	struct BoundingBox
	{
		glm::vec3 Min;
		glm::vec3 Max;

		glm::vec3 Center{ 0.f, 0.f, 0.f };
		glm::vec3 Extents{ 0.f, 0.f, 0.f };

		BoundingBox() = default;
		BoundingBox(const glm::vec3& min, const glm::vec3& max)
			: Min(min), Max(max),
			  Center{ (max + min) * 0.5f },
			  Extents{ max.x - Center.x, max.y - Center.y, max.z - Center.z }
		{
		}

		BoundingBox(const glm::vec3& inCenter, float iI, float iJ, float iK)
			: Center{ inCenter }, Extents{ iI, iJ, iK }
		{
		}

		void Reset()
		{
			Min = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
			Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		}
#if 0
		//see https://gdbooks.gitbooks.io/3dcollisions/content/Chapter2/static_aabb_plan.html
		bool IsOnOrForwardPlan(const Plane& plane) const
		{
			// Compute the projection interval radius of b onto L(t) = b.c + t * p.n
			const float r = Extents.x * std::abs(plane.Normal.x)
				          + Extents.y * std::abs(plane.Normal.y)
				          + Extents.z * std::abs(plane.Normal.z);

			return -r <= plane.GetSignedDistanceToPlan(Center);
		}

		virtual bool IsOnFrustum(const FrustumCamera& camFrustum, const glm::mat4& transform) const
		{
			//Get global scale thanks to our transform
			const glm::vec3 globalCenter{ transform * glm::vec4(Center, 1.f) };

			// Orientations
			glm::vec3 rightOrientation = transform[0];
			glm::vec3 upOrientation = transform[1];
			glm::vec3 forwardOrientation = -transform[2];

			// Scaled orientation
			const glm::vec3 right = rightOrientation * Extents.x;
			const glm::vec3 up = upOrientation * Extents.y;
			const glm::vec3 forward = forwardOrientation * Extents.z;

			const float newIi = std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, right)) +
				std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, up)) +
				std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, forward));

			const float newIj = std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, right)) +
				std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, up)) +
				std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, forward));

			const float newIk = std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, right)) +
				std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, up)) +
				std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, forward));

			const BoundingBox globalAABB(globalCenter, newIi, newIj, newIk);

			return (
				globalAABB.IsOnOrForwardPlan(camFrustum.m_LeftFace) &&
				globalAABB.IsOnOrForwardPlan(camFrustum.m_RightFace) &&
				globalAABB.IsOnOrForwardPlan(camFrustum.m_TopFace) &&
				globalAABB.IsOnOrForwardPlan(camFrustum.m_BottomFace) &&
				globalAABB.IsOnOrForwardPlan(camFrustum.m_NearFace) &&
				globalAABB.IsOnOrForwardPlan(camFrustum.m_FarFace)
			);
		};

		std::array<glm::vec3, 8> GetVertices() const
		{
			std::array<glm::vec3, 8> vertices;
			vertices[0] = { Center.x - Extents.x, Center.y - Extents.y, Center.z - Extents.z };
			vertices[1] = { Center.x + Extents.x, Center.y - Extents.y, Center.z - Extents.z };
			vertices[2] = { Center.x - Extents.x, Center.y + Extents.y, Center.z - Extents.z };
			vertices[3] = { Center.x + Extents.x, Center.y + Extents.y, Center.z - Extents.z };
			vertices[4] = { Center.x - Extents.x, Center.y - Extents.y, Center.z + Extents.z };
			vertices[5] = { Center.x + Extents.x, Center.y - Extents.y, Center.z + Extents.z };
			vertices[6] = { Center.x - Extents.x, Center.y + Extents.y, Center.z + Extents.z };
			vertices[7] = { Center.x + Extents.x, Center.y + Extents.y, Center.z + Extents.z };
			return vertices;
		}
#endif
	};

#if 0
	inline Vector<glm::vec3> ExpandAABB(BoundingBox bb)
	{
		Vector<glm::vec3> b(8);
		b[0] = { bb.Min.x, bb.Min.y, bb.Min.z };
		b[1] = { bb.Max.x, bb.Min.y, bb.Min.z };
		b[2] = { bb.Max.x, bb.Max.y, bb.Min.z };
		b[3] = { bb.Min.x, bb.Max.y, bb.Min.z };
		b[4] = { bb.Min.x, bb.Min.y, bb.Max.z };
		b[5] = { bb.Max.x, bb.Min.y, bb.Max.z };
		b[6] = { bb.Max.x, bb.Max.y, bb.Max.z };
		b[7] = { bb.Min.x, bb.Max.y, bb.Max.z };
		return b;
	}
#endif
}