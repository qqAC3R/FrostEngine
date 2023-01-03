#include "frostpch.h"
#include "PhysXUtils.h"

#include "Frost/Physics/PhysicsSettings.h"

#include "Frost/Math/Math.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Frost {
namespace PhysXUtils {

	physx::PxTransform ToPhysXTransform(const TransformComponent& transform)
	{
		physx::PxQuat r = ToPhysXQuat(glm::quat(glm::radians(transform.Rotation)));
		physx::PxVec3 p = ToPhysXVector(transform.Translation);
		return physx::PxTransform(p, r);
	}

	physx::PxTransform ToPhysXTransform(const glm::mat4& transform)
	{
		glm::vec3 translation, rotation, scale;
		Math::DecomposeTransform(transform, translation, rotation, scale);

		physx::PxQuat r = ToPhysXQuat(glm::quat(rotation));
		physx::PxVec3 p = ToPhysXVector(translation);
		return physx::PxTransform(p, r);
	}

	physx::PxTransform ToPhysXTransform(const glm::vec3& translation, const glm::vec3& rotation)
	{
		return physx::PxTransform(ToPhysXVector(translation), ToPhysXQuat(glm::quat(rotation)));
	}

	physx::PxMat44 ToPhysXMatrix(const glm::mat4& matrix) { return *(physx::PxMat44*)&matrix; }
	const physx::PxVec3& ToPhysXVector(const glm::vec3& vector) { return *(physx::PxVec3*)&vector; }
	const physx::PxVec4& ToPhysXVector(const glm::vec4& vector) { return *(physx::PxVec4*)&vector; }
	physx::PxQuat ToPhysXQuat(const glm::quat& quat) { return physx::PxQuat(quat.x, quat.y, quat.z, quat.w); }

	glm::mat4 FromPhysXTransform(const physx::PxTransform& transform)
	{
		glm::quat rotation = FromPhysXQuat(transform.q);
		glm::vec3 position = FromPhysXVector(transform.p);
		return glm::translate(glm::mat4(1.0F), position) * glm::toMat4(rotation);
	}

	glm::mat4 FromPhysXMatrix(const physx::PxMat44& matrix) { return *(glm::mat4*)&matrix; }
	glm::vec3 FromPhysXVector(const physx::PxVec3& vector) { return *(glm::vec3*)&vector; }
	glm::vec4 FromPhysXVector(const physx::PxVec4& vector) { return *(glm::vec4*)&vector; }
	glm::quat FromPhysXQuat(const physx::PxQuat& quat) {
		//return *(glm::quat*)&quat;
		return glm::quat(quat.w, quat.x, quat.y, quat.z);
	}

	CookingResult FromPhysXCookingResult(physx::PxConvexMeshCookingResult::Enum cookingResult)
	{
		switch (cookingResult)
		{
		case physx::PxConvexMeshCookingResult::eSUCCESS:return CookingResult::Success;
		case physx::PxConvexMeshCookingResult::eZERO_AREA_TEST_FAILED: return CookingResult::ZeroAreaTestFailed;
		case physx::PxConvexMeshCookingResult::ePOLYGONS_LIMIT_REACHED: return CookingResult::PolygonLimitReached;
		case physx::PxConvexMeshCookingResult::eFAILURE: return CookingResult::Failure;
		}

		return CookingResult::Failure;
	}

	CookingResult FromPhysXCookingResult(physx::PxTriangleMeshCookingResult::Enum cookingResult)
	{
		switch (cookingResult)
		{
		case physx::PxTriangleMeshCookingResult::eSUCCESS: return CookingResult::Success;
		case physx::PxTriangleMeshCookingResult::eLARGE_TRIANGLE: return CookingResult::LargeTriangle;
		case physx::PxTriangleMeshCookingResult::eFAILURE: return CookingResult::Failure;
		}

		return CookingResult::Failure;
	}

	physx::PxBroadPhaseType::Enum PhysXUtils::GetBroadphaseType(BroadphaseType type)
	{
		switch (type)
		{
			case BroadphaseType::SweepAndPrune: return physx::PxBroadPhaseType::eSAP;
			case BroadphaseType::MultiBoxPrune: return physx::PxBroadPhaseType::eMBP;
			case BroadphaseType::AutomaticBoxPrune: return physx::PxBroadPhaseType::eABP;
		}

		return physx::PxBroadPhaseType::eABP;
	}

	physx::PxFrictionType::Enum PhysXUtils::GetPhysXFrictionType(FrictionType type)
	{
		switch (type)
		{
			case FrictionType::Patch:			return physx::PxFrictionType::ePATCH;
			case FrictionType::OneDirectional:	return physx::PxFrictionType::eONE_DIRECTIONAL;
			case FrictionType::TwoDirectional:	return physx::PxFrictionType::eTWO_DIRECTIONAL;
		}

		return physx::PxFrictionType::ePATCH;
	}

} }