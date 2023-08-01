#include "frostpch.h"
#include "PhysXScene.h"

#include "PhysXContactListener.h"
#include "PhysXInternal.h"
#include "PhysXActor.h"
#include "PhysXUtils.h"
#include "PhysXDebugger.h"
#include "Frost/Physics/PhysicsEngine.h"

namespace Frost
{
	static PhysXContactListener s_ContactListener;

	PhysXScene::PhysXScene(const PhysicsSettings& settings)
	{
		physx::PxSceneDesc sceneDesc(PhysXInternal::GetPhysXHandle().getTolerancesScale());
		sceneDesc.flags |= physx::PxSceneFlag::eENABLE_CCD | physx::PxSceneFlag::eENABLE_PCM;
		sceneDesc.flags |= physx::PxSceneFlag::eENABLE_ENHANCED_DETERMINISM;
		//sceneDesc.flags |= physx::PxSceneFlag::eENABLE_ACTIVE_ACTORS;
		sceneDesc.gravity = PhysXUtils::ToPhysXVector(settings.Gravity);
		sceneDesc.broadPhaseType = PhysXUtils::GetBroadphaseType(settings.BroadphaseAlgorithm);
		sceneDesc.cpuDispatcher = PhysXInternal::GetCPUDispatcher();
		sceneDesc.filterShader = (physx::PxSimulationFilterShader)PhysXInternal::FilterShader;
		sceneDesc.simulationEventCallback = &s_ContactListener;
		sceneDesc.frictionType = PhysXUtils::GetPhysXFrictionType(settings.FrictionModel);

		FROST_ASSERT_INTERNAL(sceneDesc.isValid());

		m_PhysXScene = PhysXInternal::GetPhysXHandle().createScene(sceneDesc);
		FROST_ASSERT_INTERNAL(m_PhysXScene);

		// TODO: Create Regions??

		if(PhysicsEngine::m_EnableDebugRecording)
			PhysXDebugger::StartDebugging("PhysXScene", true);
	}

	PhysXScene::~PhysXScene()
	{
		Destroy();
	}

	void PhysXScene::Simulate(float ts, bool callFixedUpdate)
	{
		if (callFixedUpdate)
		{
			for (auto& actor : m_Actors)
				actor->OnFixedUpdate(m_SubStepSize);
		}

		Advance(ts);

		for (auto& actor : m_Actors)
			actor->SynchronizeTransform();
	}

	Ref<PhysicsActor> PhysXScene::GetActor(Entity entity)
	{
		for (auto& actor : m_Actors)
		{
			if (actor->GetEntity() == entity)
				return actor;
		}

		return nullptr;
	}

	Ref<PhysicsActor> PhysXScene::CreateActor(Entity entity)
	{
		Ref<PhysicsActor> actor = PhysicsActor::Create(entity);
		actor->SetSimulationData(entity.GetComponent<RigidBodyComponent>().Layer);

		m_Actors.push_back(actor);
		m_PhysXScene->addActor(*actor.As<PhysXActor>()->m_RigidActor);

		return actor;
	}

	void PhysXScene::RemoveActor(Ref<PhysicsActor> actor)
	{
		if (!actor)
			return;

		Ref<PhysXActor> physxActor = actor.As<PhysXActor>();

		for (auto& collider : physxActor->m_Colliders)
		{
			collider->DetachFromActor(actor);
			//collider->Release();
		}

		m_PhysXScene->removeActor(*physxActor->m_RigidActor);
		//physxActor->m_RigidActor->release();
		//physxActor->m_RigidActor = nullptr;

		for (auto it = m_Actors.begin(); it != m_Actors.end(); it++)
		{
			if ((*it)->GetEntity() == actor->GetEntity())
			{
				m_Actors.erase(it);
				break;
			}
		}
	}

	void PhysXScene::SetGravity(const glm::vec3& gravity)
	{
		m_PhysXScene->setGravity(PhysXUtils::ToPhysXVector(gravity));
	}

	bool PhysXScene::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit* outHit)
	{
		physx::PxRaycastBuffer hitInfo;
		bool result = m_PhysXScene->raycast(PhysXUtils::ToPhysXVector(origin), PhysXUtils::ToPhysXVector(glm::normalize(direction)), maxDistance, hitInfo);

		if (result)
		{
			PhysicsActor* actor = (PhysicsActor*)hitInfo.block.actor->userData;
			outHit->HitEntity = actor->GetEntity().GetUUID();
			outHit->Position = PhysXUtils::FromPhysXVector(hitInfo.block.position);
			outHit->Normal = PhysXUtils::FromPhysXVector(hitInfo.block.normal);
			outHit->Distance = hitInfo.block.distance;
		}

		return result;
	}

	bool PhysXScene::OverlapBox(const glm::vec3& origin, const glm::vec3& halfSize, std::array<physx::PxOverlapHit, OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count)
	{
		return OverlapGeometry(origin, physx::PxBoxGeometry(halfSize.x, halfSize.y, halfSize.z), buffer, count);
	}

	bool PhysXScene::OverlapCapsule(const glm::vec3& origin, float radius, float halfHeight, std::array<physx::PxOverlapHit, OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count)
	{
		return OverlapGeometry(origin, physx::PxCapsuleGeometry(radius, halfHeight), buffer, count);
	}

	bool PhysXScene::OverlapSphere(const glm::vec3& origin, float radius, std::array<physx::PxOverlapHit, OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count)
	{
		return OverlapGeometry(origin, physx::PxSphereGeometry(radius), buffer, count);
	}

	bool PhysXScene::OverlapGeometry(const glm::vec3& origin, const physx::PxGeometry& geometry, std::array<physx::PxOverlapHit, OVERLAP_MAX_COLLIDERS>& buffer, uint32_t& count)
	{
		physx::PxOverlapBuffer buf(buffer.data(), OVERLAP_MAX_COLLIDERS);
		physx::PxTransform pose = PhysXUtils::ToPhysXTransform(glm::translate(glm::mat4(1.0f), origin));

		bool result = m_PhysXScene->overlap(geometry, pose, buf);
		if (result)
		{
			memcpy(buffer.data(), buf.touches, buf.nbTouches * sizeof(physx::PxOverlapHit));
			count = buf.nbTouches;
		}

		return result;
	}

	void PhysXScene::Destroy()
	{
		FROST_ASSERT_INTERNAL(m_PhysXScene);

		for (auto& actor : m_Actors)
			RemoveActor(actor);

		m_Actors.clear();
		m_PhysXScene->release();
		m_PhysXScene = nullptr;

		if (PhysicsEngine::m_EnableDebugRecording)
			PhysXDebugger::StopDebugging();
	}

	void PhysXScene::Advance(float ts)
	{
		m_PhysXScene->simulate(ts);
		m_PhysXScene->fetchResults(true);
	}
}