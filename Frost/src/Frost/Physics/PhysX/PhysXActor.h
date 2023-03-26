#pragma once

#include "Frost/Physics/PhysicsActor.h"
#include "Frost/Physics/PhysicsShapes.h"

#include "PhysXUtils.h"

namespace Frost
{

	class PhysXActor : public PhysicsActor
	{
	public:
		PhysXActor(Entity entity);
		~PhysXActor();

		virtual glm::vec3 GetTranslation() const override { return PhysXUtils::FromPhysXVector(m_RigidActor->getGlobalPose().p); }
		virtual void SetTranslation(const glm::vec3& translation, bool autowake = true) override;

		virtual glm::vec3 GetRotation() const override { return glm::eulerAngles(PhysXUtils::FromPhysXQuat(m_RigidActor->getGlobalPose().q)); }
		virtual void SetRotation(const glm::vec3& rotation, bool autowake = true) override;
		virtual void Rotate(const glm::vec3& rotation, bool autowake = true) override;

		virtual void WakeUp() override;
		virtual void PutToSleep() override;

		virtual float GetMass() const override;
		virtual void SetMass(float mass) override;

		virtual void AddForce(const glm::vec3& force, ForceMode forceMode) override;
		virtual void AddTorque(const glm::vec3& torque, ForceMode forceMode) override;

		virtual glm::vec3 GetLinearVelocity() const override;
		virtual void SetLinearVelocity(const glm::vec3& velocity) override;
		virtual glm::vec3 GetAngularVelocity() const override;
		virtual void SetAngularVelocity(const glm::vec3& velocity) override;

		virtual float GetMaxLinearVelocity() const override;
		virtual void SetMaxLinearVelocity(float maxVelocity) override;
		virtual float GetMaxAngularVelocity() const override;
		virtual void SetMaxAngularVelocity(float maxVelocity) override;

		virtual void SetLinearDrag(float drag) const override;
		virtual void SetAngularDrag(float drag) const override;

		virtual bool IsKinematic() const override { return IsDynamic() && m_RigidBodyData.IsKinematic; }
		virtual void SetKinematic(bool isKinematic) override;

		virtual bool IsDynamic() const override { return m_RigidBodyData.BodyType == RigidBodyComponent::Type::Dynamic; }

		virtual bool IsGravityDisabled() const override { return m_RigidActor->getActorFlags().isSet(physx::PxActorFlag::eDISABLE_GRAVITY); }
		virtual void SetGravityDisabled(bool disable) override;

		virtual bool IsLockFlagSet(ActorLockFlag flag) const override { return (uint32_t)flag & m_LockFlags; }
		virtual void SetLockFlag(ActorLockFlag flag, bool value) override;
		virtual uint32_t GetLockFlags() const override { return m_LockFlags; }

		virtual void OnFixedUpdate(float fixedDeltaTime) override;

		virtual Entity GetEntity() const override { return m_Entity; }
		virtual const TransformComponent& GetTransform() const override { return m_Entity.GetComponent<TransformComponent>(); }

		void AddCollider(BoxColliderComponent& collider, Entity entity, const glm::vec3& offset = glm::vec3(0.0f));
		void AddCollider(SphereColliderComponent& collider, Entity entity, const glm::vec3& offset = glm::vec3(0.0f));
		void AddCollider(CapsuleColliderComponent& collider, Entity entity, const glm::vec3& offset = glm::vec3(0.0f));
		void AddCollider(MeshColliderComponent& collider, Entity entity, const glm::vec3& offset = glm::vec3(0.0f));

		virtual void* GetInternalAPIActor() const override { return (void*)&m_RigidActor; }
		physx::PxRigidActor* GetPhysXActor() const { return m_RigidActor; }

	private:
		void CreateRigidActor();
		virtual void SynchronizeTransform() override;

	private:
		Entity m_Entity;
		RigidBodyComponent m_RigidBodyData;
		uint32_t m_LockFlags = 0;

		physx::PxRigidActor* m_RigidActor;
		Vector<Ref<ColliderShape>> m_Colliders;

		friend class PhysXScene;
	};

}