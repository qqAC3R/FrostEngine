#pragma once

#include "Frost/EntitySystem/Components.h"

#include "Frost/Physics/PhysicsShapes.h"
#include "Frost/Physics/PhysicsActor.h"
#include "PhysXUtils.h"

namespace Frost
{
	class PhysXActor;
}

namespace Frost::PhysX
{

	class BoxColliderShape : public ColliderShape
	{
	public:
		BoxColliderShape(BoxColliderComponent& component, const PhysXActor& actor, Entity entity, const glm::vec3& offset = glm::vec3(0.0f));
		~BoxColliderShape();

		// TODO: Physics Materials
		//void SetMaterial(AssetHandle material);

		// TODO: Add layers
		//virtual void SetFilterData(const physx::PxFilterData& filterData) override;

		virtual const glm::vec3& GetOffset() const override { return m_Component.Offset; }
		virtual void SetOffset(const glm::vec3& offset) override;

		virtual bool IsTrigger() const override { return m_Component.IsTrigger; }
		virtual void SetTrigger(bool isTrigger) override;

		//void DetachFromActor(physx::PxRigidActor* actor);
		virtual void DetachFromActor(Ref<PhysicsActor> actor) override;

	private:
		BoxColliderComponent& m_Component;
		physx::PxShape* m_Shape;

		physx::PxMaterial* m_Material;
	};


	class SphereColliderShape : public ColliderShape
	{
	public:
		SphereColliderShape(SphereColliderComponent& component, const PhysXActor& actor, Entity entity, const glm::vec3& offset = glm::vec3(0.0f));
		~SphereColliderShape();

		// TODO: Physics Materials
		//void SetMaterial(AssetHandle material);

		// TODO: Add layers
		//virtual void SetFilterData(const physx::PxFilterData& filterData) override;

		virtual const glm::vec3& GetOffset() const override { return m_Component.Offset; }
		virtual void SetOffset(const glm::vec3& offset) override;

		virtual bool IsTrigger() const override { return m_Component.IsTrigger; }
		virtual void SetTrigger(bool isTrigger) override;

		//void DetachFromActor(physx::PxRigidActor* actor);
		virtual void DetachFromActor(Ref<PhysicsActor> actor) override;

	private:
		SphereColliderComponent& m_Component;
		physx::PxShape* m_Shape;

		physx::PxMaterial* m_Material;
	};

}