#pragma once

#include "Frost/EntitySystem/Entity.h"
#include "PhysicsSettings.h"

#include <glm/glm.hpp>

namespace Frost
{

	class PhysicsActor
	{
	public:
		static Ref<PhysicsActor> Create(Entity entity);

		virtual glm::vec3 GetTranslation() const = 0;
		virtual void SetTranslation(const glm::vec3& translation, bool autowake = true) = 0;

		virtual glm::vec3 GetRotation() const = 0;
		virtual void SetRotation(const glm::vec3& rotation, bool autowake = true) = 0;
		virtual void Rotate(const glm::vec3& rotation, bool autowake = true) = 0;

		virtual void WakeUp() = 0;
		virtual void PutToSleep() = 0;

		virtual float GetMass() const = 0;
		virtual void SetMass(float mass) = 0;


		virtual void AddForce(const glm::vec3& force, ForceMode forceMode) = 0;
		virtual void AddTorque(const glm::vec3& torque, ForceMode forceMode) = 0;

		virtual glm::vec3 GetLinearVelocity() const = 0;
		virtual void SetLinearVelocity(const glm::vec3& velocity) = 0;
		virtual glm::vec3 GetAngularVelocity() const = 0;
		virtual void SetAngularVelocity(const glm::vec3& velocity) = 0;
		
		virtual float GetMaxLinearVelocity() const = 0;
		virtual void SetMaxLinearVelocity(float maxVelocity) = 0;
		virtual float GetMaxAngularVelocity() const = 0;
		virtual void SetMaxAngularVelocity(float maxVelocity) = 0;

		virtual void SetLinearDrag(float drag) const = 0;
		virtual void SetAngularDrag(float drag) const = 0;

		virtual bool IsKinematic() const = 0;
		virtual void SetKinematic(bool isKinematic) = 0;

		virtual bool IsDynamic() const = 0;

		virtual bool IsGravityDisabled() const = 0;
		virtual void SetGravityDisabled(bool disable) = 0;

		virtual bool IsLockFlagSet(ActorLockFlag flag) const = 0;
		virtual void SetLockFlag(ActorLockFlag flag, bool value) = 0;
		virtual uint32_t GetLockFlags() const = 0;

		virtual void OnFixedUpdate(float fixedDeltaTime) = 0;

		virtual Entity GetEntity() const = 0;
		virtual const TransformComponent& GetTransform() const = 0;

		void AddCollider(BoxColliderComponent& collider, Entity entity, const glm::vec3& offset = glm::vec3(0.0f));
#if 0
		void AddCollider(SphereColliderComponent& collider, Entity entity, const glm::vec3& offset = glm::vec3(0.0f));
		void AddCollider(CapsuleColliderComponent& collider, Entity entity, const glm::vec3& offset = glm::vec3(0.0f));
		void AddCollider(MeshColliderComponent& collider, Entity entity, const glm::vec3& offset = glm::vec3(0.0f));
#endif

		virtual void* GetInternalAPIActor() const = 0;

	private:
		virtual void SynchronizeTransform() = 0;

		friend class PhysXScene;
	};

}