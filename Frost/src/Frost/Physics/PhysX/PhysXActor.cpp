#include "frostpch.h"
#include "PhysXActor.h"

#include "PhysXInternal.h"
#include "Frost/Physics/PhysicsEngine.h"
#include "PhysXShapes.h"

#include <glm/gtx/compatibility.hpp>

namespace Frost
{
	PhysXActor::PhysXActor(Entity entity)
		: m_Entity(entity)
	{
		m_RigidBodyData = m_Entity.GetComponent<RigidBodyComponent>();
		CreateRigidActor();
	}

	PhysXActor::~PhysXActor()
	{
		m_Colliders.clear();
		m_RigidActor->release();
	}

	void PhysXActor::SetTranslation(const glm::vec3& translation, bool autowake /*= true*/)
	{
		physx::PxTransform transform = m_RigidActor->getGlobalPose();
		transform.p = PhysXUtils::ToPhysXVector(translation);
		m_RigidActor->setGlobalPose(transform, autowake);

		if (m_RigidBodyData.BodyType == RigidBodyComponent::Type::Static)
			SynchronizeTransform();
	}

	void PhysXActor::SetRotation(const glm::vec3& rotation, bool autowake /*= true*/)
	{
		physx::PxTransform transform = m_RigidActor->getGlobalPose();
		transform.q = PhysXUtils::ToPhysXQuat(glm::quat(rotation));
		m_RigidActor->setGlobalPose(transform, autowake);

		if (m_RigidBodyData.BodyType == RigidBodyComponent::Type::Static)
			SynchronizeTransform();
	}

	void PhysXActor::Rotate(const glm::vec3& rotation, bool autowake /*= true*/)
	{
		physx::PxTransform transform = m_RigidActor->getGlobalPose();
		transform.q *= (physx::PxQuat(glm::radians(rotation.x), { 1.0f, 0.0f, 0.0f })
			* physx::PxQuat(glm::radians(rotation.y), { 0.0f, 1.0f, 0.0f })
			* physx::PxQuat(glm::radians(rotation.z), { 0.0f, 0.0f, 1.0f }));
		m_RigidActor->setGlobalPose(transform, autowake);

		if (m_RigidBodyData.BodyType == RigidBodyComponent::Type::Static)
			SynchronizeTransform();
	}

	void PhysXActor::WakeUp()
	{
		if (IsDynamic())
			m_RigidActor->is<physx::PxRigidDynamic>()->wakeUp();
	}

	void PhysXActor::PutToSleep()
	{
		if (IsDynamic())
			m_RigidActor->is<physx::PxRigidDynamic>()->putToSleep();
	}

	float PhysXActor::GetMass() const
	{
		return IsDynamic() ? m_RigidActor->is<physx::PxRigidDynamic>()->getMass() : m_RigidBodyData.Mass;
	}

	void PhysXActor::SetMass(float mass)
	{
		if (!IsDynamic())
			return;

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		FROST_ASSERT_INTERNAL(actor);
		physx::PxRigidBodyExt::setMassAndUpdateInertia(*actor, mass);
		m_RigidBodyData.Mass = mass;
	}

	void PhysXActor::AddForce(const glm::vec3& force, ForceMode forceMode)
	{
		if (!IsDynamic())
		{
			FROST_CORE_WARN("Trying to add force to non-dynamic PhysicsActor.");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		FROST_ASSERT_INTERNAL(actor);
		actor->addForce(PhysXUtils::ToPhysXVector(force), (physx::PxForceMode::Enum)forceMode);
	}

	void PhysXActor::AddTorque(const glm::vec3& torque, ForceMode forceMode)
	{
		if (!IsDynamic())
		{
			FROST_CORE_WARN("Trying to add torque to non-dynamic PhysicsActor.");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		FROST_ASSERT_INTERNAL(actor);
		actor->addTorque(PhysXUtils::ToPhysXVector(torque), (physx::PxForceMode::Enum)forceMode);
	}

	glm::vec3 PhysXActor::GetLinearVelocity() const
	{
		if (!IsDynamic())
		{
			FROST_CORE_WARN("Trying to get velocity of non-dynamic PhysicsActor.");
			return glm::vec3(0.0f);
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		FROST_ASSERT_INTERNAL(actor);
		return PhysXUtils::FromPhysXVector(actor->getLinearVelocity());
	}

	void PhysXActor::SetLinearVelocity(const glm::vec3& velocity)
	{
		if (!IsDynamic())
		{
			FROST_CORE_WARN("Trying to set velocity of non-dynamic PhysicsActor.");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		FROST_ASSERT_INTERNAL(actor);
		actor->setLinearVelocity(PhysXUtils::ToPhysXVector(velocity));
	}

	glm::vec3 PhysXActor::GetAngularVelocity() const
	{
		if (!IsDynamic())
		{
			FROST_CORE_WARN("Trying to get angular velocity of non-dynamic PhysicsActor.");
			return glm::vec3(0.0f);
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		FROST_ASSERT_INTERNAL(actor);
		return PhysXUtils::FromPhysXVector(actor->getAngularVelocity());
	}

	void PhysXActor::SetAngularVelocity(const glm::vec3& velocity)
	{
		if (!IsDynamic())
		{
			FROST_CORE_WARN("Trying to set angular velocity of non-dynamic PhysicsActor.");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		FROST_ASSERT_INTERNAL(actor);
		actor->setAngularVelocity(PhysXUtils::ToPhysXVector(velocity));
	}

	float PhysXActor::GetMaxLinearVelocity() const
	{
		if (!IsDynamic())
		{
			FROST_CORE_WARN("Trying to get max linear velocity of non-dynamic PhysicsActor.");
			return 0.0f;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		FROST_ASSERT_INTERNAL(actor);
		return actor->getMaxLinearVelocity();
	}

	void PhysXActor::SetMaxLinearVelocity(float maxVelocity)
	{
		if (!IsDynamic())
		{
			FROST_CORE_WARN("Trying to set max linear velocity of non-dynamic PhysicsActor.");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		FROST_ASSERT_INTERNAL(actor);
		actor->setMaxLinearVelocity(maxVelocity);
	}

	float PhysXActor::GetMaxAngularVelocity() const
	{
		if (!IsDynamic())
		{
			FROST_CORE_WARN("Trying to get max angular velocity of non-dynamic PhysicsActor.");
			return 0.0f;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		FROST_ASSERT_INTERNAL(actor);
		return actor->getMaxAngularVelocity();
	}

	void PhysXActor::SetMaxAngularVelocity(float maxVelocity)
	{
		if (!IsDynamic())
		{
			FROST_CORE_WARN("Trying to set max angular velocity of non-dynamic PhysicsActor.");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		FROST_ASSERT_INTERNAL(actor);
		actor->setMaxAngularVelocity(maxVelocity);
	}

	void PhysXActor::SetLinearDrag(float drag) const
	{
		if (!IsDynamic())
		{
			FROST_CORE_WARN("Trying to set linear drag of non-dynamic PhysicsActor.");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		FROST_ASSERT_INTERNAL(actor);
		actor->setLinearDamping(drag);
	}

	void PhysXActor::SetAngularDrag(float drag) const
	{
		if (!IsDynamic())
		{
			FROST_CORE_WARN("Trying to set angular drag of non-dynamic PhysicsActor.");
			return;
		}

		physx::PxRigidDynamic* actor = m_RigidActor->is<physx::PxRigidDynamic>();
		FROST_ASSERT_INTERNAL(actor);
		actor->setAngularDamping(drag);
	}

	void PhysXActor::SetKinematic(bool isKinematic)
	{
		if (!IsDynamic())
		{
			FROST_CORE_WARN("Static PhysicsActor can't be kinematic.");
			return;
		}

		m_RigidBodyData.IsKinematic = isKinematic;
		m_RigidActor->is<physx::PxRigidDynamic>()->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, isKinematic);
	}

	void PhysXActor::SetGravityDisabled(bool disable)
	{
		m_RigidActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, disable);
	}

	void PhysXActor::SetLockFlag(ActorLockFlag flag, bool value)
	{
		if (!IsDynamic())
			return;

		if (value)
			m_LockFlags |= (uint32_t)flag;
		else
			m_LockFlags &= ~(uint32_t)flag;

		m_RigidActor->is<physx::PxRigidDynamic>()->setRigidDynamicLockFlags((physx::PxRigidDynamicLockFlags)m_LockFlags);
	}

	void PhysXActor::OnFixedUpdate(float fixedDeltaTime)
	{
		// TODO: This is linked mostly with the Script Engine
	}

	void PhysXActor::AddCollider(BoxColliderComponent& collider, Entity entity, const glm::vec3& offset)
	{
		m_Colliders.push_back(Ref<PhysX::BoxColliderShape>::Create(collider, *this, entity, offset));
	}

	void PhysXActor::AddCollider(SphereColliderComponent& collider, Entity entity, const glm::vec3& offset)
	{
		m_Colliders.push_back(Ref<PhysX::SphereColliderShape>::Create(collider, *this, entity, offset));
	}

	void PhysXActor::AddCollider(CapsuleColliderComponent& collider, Entity entity, const glm::vec3& offset)
	{
		m_Colliders.push_back(Ref<PhysX::CapsuleColliderShape>::Create(collider, *this, entity, offset));
	}

	void PhysXActor::AddCollider(MeshColliderComponent& collider, Entity entity, const glm::vec3& offset)
	{
		if(collider.IsConvex)
			m_Colliders.push_back(Ref<PhysX::ConvexMeshShape>::Create(collider, *this, entity, offset));
		else
			m_Colliders.push_back(Ref<PhysX::TriangleMeshShape>::Create(collider, *this, entity, offset));
	}

	void PhysXActor::CreateRigidActor()
	{
		auto& physicsHandle = PhysXInternal::GetPhysXHandle();

		TransformComponent& transformComponent = m_Entity.GetComponent<TransformComponent>();
			
		if (m_RigidBodyData.BodyType == RigidBodyComponent::Type::Static)
		{
			m_RigidActor = physicsHandle.createRigidStatic(PhysXUtils::ToPhysXTransform(transformComponent));
		}
		else
		{
			const PhysicsSettings& settings = PhysicsEngine::GetSettings();

			physx::PxRigidDynamic* dynamicActor = physicsHandle.createRigidDynamic(PhysXUtils::ToPhysXTransform(transformComponent));
			m_RigidActor = dynamicActor;

			SetLinearDrag(m_RigidBodyData.LinearDrag);
			SetAngularDrag(m_RigidBodyData.AngularDrag);

			SetKinematic(m_RigidBodyData.IsKinematic);


			SetLockFlag(ActorLockFlag::TranslationX, m_RigidBodyData.LockPositionX);
			SetLockFlag(ActorLockFlag::TranslationY, m_RigidBodyData.LockPositionY);
			SetLockFlag(ActorLockFlag::TranslationZ, m_RigidBodyData.LockPositionZ);
			SetLockFlag(ActorLockFlag::RotationX, m_RigidBodyData.LockRotationX);
			SetLockFlag(ActorLockFlag::RotationY, m_RigidBodyData.LockRotationY);
			SetLockFlag(ActorLockFlag::RotationZ, m_RigidBodyData.LockRotationZ);
			
			SetGravityDisabled(m_RigidBodyData.DisableGravity);

			m_RigidActor->is<physx::PxRigidDynamic>()->setSolverIterationCounts(settings.SolverIterations, settings.SolverVelocityIterations);
			m_RigidActor->is<physx::PxRigidDynamic>()->setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_CCD, m_RigidBodyData.CollisionDetection == RigidBodyComponent::CollisionDetectionType::Continuous);
			m_RigidActor->is<physx::PxRigidDynamic>()->setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD, m_RigidBodyData.CollisionDetection == RigidBodyComponent::CollisionDetectionType::ContinuousSpeculative);
		}

		if (m_Entity.HasComponent<BoxColliderComponent>())
			AddCollider(m_Entity.GetComponent<BoxColliderComponent>(), m_Entity);
		
		if (m_Entity.HasComponent<SphereColliderComponent>())
			AddCollider(m_Entity.GetComponent<SphereColliderComponent>(), m_Entity);
		
		if (m_Entity.HasComponent<CapsuleColliderComponent>())
			AddCollider(m_Entity.GetComponent<CapsuleColliderComponent>(), m_Entity);

		if (m_Entity.HasComponent<MeshColliderComponent>())
		{
			auto& meshColliderComponent = m_Entity.GetComponent<MeshColliderComponent>();

			if (m_RigidBodyData.BodyType == RigidBodyComponent::Type::Dynamic && !meshColliderComponent.IsConvex)
			{
				FROST_CORE_WARN("Triangle meshes can't have a dynamic rigidbody!");
			}
			else
			{
				meshColliderComponent.CollisionMesh = m_Entity.GetComponent<MeshComponent>().Mesh;
				AddCollider(m_Entity.GetComponent<MeshColliderComponent>(), m_Entity);
			}
		}

		SetMass(m_RigidBodyData.Mass);

		m_RigidActor->userData = this;
	}

	void PhysXActor::SynchronizeTransform()
	{
		if (IsDynamic())
		{
			TransformComponent& transform = m_Entity.Transform();
			physx::PxTransform actorPose = m_RigidActor->getGlobalPose();
			transform.Translation = PhysXUtils::FromPhysXVector(actorPose.p);

			glm::quat q = PhysXUtils::FromPhysXQuat(actorPose.q);
			transform.Rotation = glm::degrees(glm::eulerAngles(q));
		}
		else
		{
			m_RigidActor->setGlobalPose(PhysXUtils::ToPhysXTransform(m_Entity.GetComponent<TransformComponent>()));
		}
	}

}