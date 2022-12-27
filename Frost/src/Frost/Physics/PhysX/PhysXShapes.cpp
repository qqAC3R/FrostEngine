#include "frostpch.h"
#include "PhysXShapes.h"

#include "PhysXInternal.h"
#include "PhysXActor.h"

namespace Frost::PhysX
{
	static physx::PxMaterial* CreatePhysicsMaterial()
	{
		return PhysXInternal::GetPhysXHandle().createMaterial(0.6f, 0.6f, 0.6f);
	}

	BoxColliderShape::BoxColliderShape(BoxColliderComponent& component,
		const PhysXActor& actor,
		Entity entity,
		const glm::vec3& offset)
		: ColliderShape(ColliderType::Box), m_Component(component)
	{

		m_Material = CreatePhysicsMaterial();

		glm::vec3 colliderSize = entity.Transform().Scale * m_Component.Size;
		physx::PxBoxGeometry geometry = physx::PxBoxGeometry(colliderSize.x / 2.0f, colliderSize.y / 2.0f, colliderSize.z / 2.0f);

		//physx::PxFilterData filterData;
		//filterData.word0 = BIT(0);
		//filterData.word1 = BIT(0);


		m_Shape = physx::PxRigidActorExt::createExclusiveShape(
			*actor.GetPhysXActor(),
			geometry, 
			*m_Material
		);

		m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !m_Component.IsTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, m_Component.IsTrigger);
		//m_Shape->setLocalPose(PhysXUtils::ToPhysXTransform(offset + m_Component.Offset, glm::vec3(0.0f)));
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTransform(glm::translate(glm::mat4(1.0f), m_Component.Offset)));

		//m_Shape->setSimulationFilterData(filterData);
	}

	BoxColliderShape::~BoxColliderShape()
	{
		m_Material->release();
		m_Shape->release();
	}

	void BoxColliderShape::SetOffset(const glm::vec3& offset)
	{
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTransform(offset, glm::vec3(0.0f)));
		m_Component.Offset = offset;
	}

	void BoxColliderShape::SetTrigger(bool isTrigger)
	{
		m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !isTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, isTrigger);
		m_Component.IsTrigger = isTrigger;
	}

	void BoxColliderShape::DetachFromActor(Ref<PhysicsActor> actor)
	{
		actor.As<PhysXActor>()->GetPhysXActor()->detachShape(*m_Shape);
	}




	SphereColliderShape::SphereColliderShape(SphereColliderComponent& component,
		const PhysXActor& actor,
		Entity entity,
		const glm::vec3& offset)
		: ColliderShape(ColliderType::Sphere), m_Component(component)
	{
		m_Material = CreatePhysicsMaterial();

		auto& actorScale = entity.Transform().Scale;
		float largestComponent = glm::max(actorScale.x, glm::max(actorScale.y, actorScale.z));

		physx::PxSphereGeometry geometry = physx::PxSphereGeometry(largestComponent * m_Component.Radius);
		m_Shape = physx::PxRigidActorExt::createExclusiveShape(*actor.GetPhysXActor(), geometry, *m_Material);
		m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !m_Component.IsTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, m_Component.IsTrigger);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTransform(offset + m_Component.Offset, glm::vec3(0.0f)));
	}

	SphereColliderShape::~SphereColliderShape()
	{
		m_Material->release();
		m_Shape->release();
	}

	void SphereColliderShape::SetOffset(const glm::vec3& offset)
	{
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTransform(offset, glm::vec3(0.0f)));
		m_Component.Offset = offset;
	}

	void SphereColliderShape::SetTrigger(bool isTrigger)
	{
		m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !isTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, isTrigger);
		m_Component.IsTrigger = isTrigger;
	}

	void SphereColliderShape::DetachFromActor(Ref<PhysicsActor> actor)
	{
		actor.As<PhysXActor>()->GetPhysXActor()->detachShape(*m_Shape);
	}

}