#pragma once

#include "Frost/Physics/PhysicsShapes.h"
#include "Frost/Physics/PhysicsActor.h"
#include "PhysXUtils.h"

#include "Frost/EntitySystem/Components.h"

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

		void SetSize(const glm::vec3& transformScale, const glm::vec3& size);

		virtual void SetFilterData(const PhysicsLayer& layerInfo, CollisionDetectionType collisionType) override;

		virtual const glm::vec3& GetOffset() const override { return m_Component.Offset; }
		virtual void SetOffset(const glm::vec3& offset) override;

		virtual bool IsTrigger() const override { return m_Component.IsTrigger; }
		virtual void SetTrigger(bool isTrigger) override;

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

		void SetRadius(const glm::vec3& transformScale, float radius);

		virtual void SetFilterData(const PhysicsLayer& layerInfo, CollisionDetectionType collisionType) override;

		virtual const glm::vec3& GetOffset() const override { return m_Component.Offset; }
		virtual void SetOffset(const glm::vec3& offset) override;

		virtual bool IsTrigger() const override { return m_Component.IsTrigger; }
		virtual void SetTrigger(bool isTrigger) override;

		virtual void DetachFromActor(Ref<PhysicsActor> actor) override;

	private:
		SphereColliderComponent& m_Component;
		physx::PxShape* m_Shape;

		physx::PxMaterial* m_Material;
	};

	class CapsuleColliderShape : public ColliderShape
	{
	public:
		CapsuleColliderShape(CapsuleColliderComponent& component, const PhysXActor& actor, Entity entity, const glm::vec3& offset = glm::vec3(0.0f));
		~CapsuleColliderShape();

		// TODO: Physics Materials
		//void SetMaterial(AssetHandle material);

		void SetRadius(const glm::vec3& transformScale, float radius);
		void SetHeight(const glm::vec3& transformScale, float height);

		virtual void SetFilterData(const PhysicsLayer& layerInfo, CollisionDetectionType collisionType) override;

		virtual const glm::vec3& GetOffset() const override { return m_Component.Offset; }
		virtual void SetOffset(const glm::vec3& offset) override;

		virtual bool IsTrigger() const override { return m_Component.IsTrigger; }
		virtual void SetTrigger(bool isTrigger) override;

		virtual void DetachFromActor(Ref<PhysicsActor> actor) override;

	private:
		CapsuleColliderComponent& m_Component;
		physx::PxShape* m_Shape;

		physx::PxMaterial* m_Material;
	};



	class ConvexMeshShape : public ColliderShape
	{
	public:
		ConvexMeshShape(MeshColliderComponent& component, const PhysXActor& actor, Entity entity, const glm::vec3& offset = glm::vec3(0.0f));
		~ConvexMeshShape();

		// TODO: Physics Materials
		//void SetMaterial(AssetHandle material);

		virtual void SetFilterData(const PhysicsLayer& layerInfo, CollisionDetectionType collisionType) override;

		// TODO?
		virtual const glm::vec3& GetOffset() const override { return glm::vec3(0.0f); }
		virtual void SetOffset(const glm::vec3& offset) override {}

		virtual bool IsTrigger() const override { return m_Component.IsTrigger; }
		virtual void SetTrigger(bool isTrigger) override;

		virtual void DetachFromActor(Ref<PhysicsActor> actor) override;

	private:
		MeshColliderComponent& m_Component;
		Vector<physx::PxShape*> m_Shapes;

		physx::PxMaterial* m_Material;
	};


	class TriangleMeshShape : public ColliderShape
	{
	public:
		TriangleMeshShape(MeshColliderComponent& component, const PhysXActor& actor, Entity entity, const glm::vec3& offset = glm::vec3(0.0f));
		~TriangleMeshShape();

		// TODO: Physics Materials
		//void SetMaterial(AssetHandle material);

		virtual void SetFilterData(const PhysicsLayer& layerInfo, CollisionDetectionType collisionType) override;

		// TODO?
		virtual const glm::vec3& GetOffset() const override { return glm::vec3(0.0f); }
		virtual void SetOffset(const glm::vec3& offset) override {}

		virtual bool IsTrigger() const override { return m_Component.IsTrigger; }
		virtual void SetTrigger(bool isTrigger) override;

		virtual void DetachFromActor(Ref<PhysicsActor> actor) override;

	private:
		MeshColliderComponent& m_Component;
		Vector<physx::PxShape*> m_Shapes;

		physx::PxMaterial* m_Material;
	};
	


}